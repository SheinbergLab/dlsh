/*
 * dlsh_main.c
 *   Entry point for the standalone `dlsh` interpreter: a self-contained Tcl
 *   (+ Tk) runtime that pulls the dlsh PACKAGE from dlsh.zip at startup, the
 *   same way dserv and stim2 do. Two layers, deliberately separate:
 *
 *     - The Tcl/Tk RUNTIME (init.tcl/tk.tcl) is baked into this binary -- it is
 *       what makes dlsh a standalone interpreter, independent of dlsh.zip's
 *       contents. (Appended at //zipfs:/app in single-file builds, or mounted
 *       from a sidecar dlsh-runtime.zip in signable builds.)
 *     - The dlsh PACKAGE (libdlsh + plot/stats/... helpers) is NOT linked in;
 *       the bootstrap mounts dlsh.zip and does `package require dlsh`, so dlsh
 *       loads the EXACT same artifact dserv/stim2 load. dlsh.zip stays the one
 *       source of truth and can be updated without rebuilding this binary.
 *
 *   Run modes:
 *     - `dlsh -e <script>`: one-shot evaluation for scripting/CI/agents.
 *     - everything else: stock Tcl_Main (tclsh-compatible script file, piped
 *       stdin, and an event-loop-driven interactive prompt that keeps Tk live).
 *
 *   The interactive prompt is plain tclsh (no line editing) -- a deliberate
 *   simplicity tradeoff, since interactive use is light here.
 *
 *   With no discoverable dlsh.zip you get bare Tcl/Tk (no dl_* commands); that
 *   is an accepted tradeoff for keeping a single source of truth.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>

#if defined(DLSH_WITH_TK)
#include <tk.h>
#endif

/*
 * Find dlsh.zip, mount it, put its lib/ on auto_path, then `package require
 * dlsh` -- the SAME load path dserv and stim2 use, so all three load the one
 * libdlsh + Tcl helpers out of the shared zip (one source of truth, updatable
 * without rebuilding any of the binaries). This is the app's deployment policy,
 * deliberately kept out of the C runtime: with no zip you get bare Tcl/Tk.
 *
 * Discovery order: $DLSH_ZIP -> $DLSH_LIBRARY/dlsh.zip -> next to / near the
 * executable (incl. ../Resources for a .app bundle) -> /usr/local/dlsh, the
 * canonical install location shared with dserv/stim2.
 *
 * We mount at //zipfs:/dlsh (NOT the runtime's //zipfs:/app or
 * //zipfs:/dlsh_runtime), so the package and the baked-in Tcl/Tk runtime never
 * collide. The `package require dlsh` is best-effort (catch): a missing or
 * partial zip must not abort startup.
 *
 * We also drop /usr/local/lib from auto_path: Tcl's init adds it (the configured
 * prefix's lib dir), but it holds older copies of our packages that would shadow
 * the zip's. A user who wants it back can re-add it (e.g. from ~/.dlshrc).
 */
/* Mount dlsh.zip and put its lib/ on auto_path -- WITHOUT yet pulling in the
   dlsh package. Shared by the normal startup (which then does `package require
   dlsh` to load the zip's libdlsh) and by --libdlsh dev mode (which instead
   loads a working-tree libdlsh, but still wants the zip on auto_path so other
   packages and Tcl helpers resolve). Returns the mounted zip path (or {}). */
static const char *dlsh_vfs_mount =
    "apply {{} {\n"
    "  set cands {}\n"
    "  if {[info exists ::env(DLSH_ZIP)]} { lappend cands $::env(DLSH_ZIP) }\n"
    "  if {[info exists ::env(DLSH_LIBRARY)]} { lappend cands [file join $::env(DLSH_LIBRARY) dlsh.zip] }\n"
    "  set d [file dirname [info nameofexecutable]]\n"
    "  lappend cands [file join $d dlsh.zip] \\\n"
    "                [file normalize [file join $d .. Resources dlsh.zip]] \\\n"
    "                [file normalize [file join $d .. dlsh dlsh.zip]] \\\n"
    "                /usr/local/dlsh/dlsh.zip\n"
    "  foreach z $cands {\n"
    "    if {![file exists $z]} continue\n"
    "    set base [file join [zipfs root] dlsh]\n"
    "    if {$base ni [zipfs list]} { zipfs mount $z $base }\n"
    "    set libdir [file join $base lib]\n"
    "    if {[file isdirectory $libdir] && $libdir ni $::auto_path} {\n"
    "      set ::auto_path [linsert $::auto_path 0 $libdir]\n"
    "    }\n"
    "    return $z\n"
    "  }\n"
    "  return {}\n"
    "}}\n"
    "set ::auto_path [lsearch -all -inline -not -exact $::auto_path /usr/local/lib]\n";

/* Working-tree libdlsh to `load` instead of `package require dlsh`, set by the
   `--libdlsh <path>` dev flag (NULL in normal use). A development affordance for
   exercising an unreleased libdlsh build before it ships inside dlsh.zip. */
static const char *dlsh_dev_lib = NULL;

/* True if a path (native OR //zipfs:) is accessible. Works before Tcl_Init:
   Tcl_FindExecutable (called via TclZipfs_AppHook in main) sets up the FS. */
static int dlsh_path_exists(const char *path)
{
    Tcl_Obj *o = Tcl_NewStringObj(path, -1);
    Tcl_IncrRefCount(o);
    int ok = (Tcl_FSAccess(o, 0 /*F_OK*/) == 0);
    Tcl_DecrRefCount(o);
    return ok;
}

/*
 * Make the baked-in Tcl (and Tk) script RUNTIME reachable BEFORE Tcl_Init --
 * the one thing the post-init dlsh.zip bootstrap cannot do, since Tcl_Init
 * itself needs init.tcl. This is the interpreter's own runtime, distinct from
 * the dlsh PACKAGE (dlsh.zip), and uses its own filenames / mount points so the
 * two never collide.
 *
 *   - Single-file build: TclZipfs_AppHook already mounted an archive appended
 *     to the executable at //zipfs:/app (tcl_library/ at its root). Nothing to
 *     do. Portable-but-unsignable form (trailing data breaks macOS codesign);
 *     good for Linux / dev / CI artifacts.
 *   - Signable build: the executable has no trailing data; the runtime ships in
 *     a sidecar "dlsh-runtime.zip" (next to the exe, or ../Resources in a .app)
 *     carrying lib/tcl9.0 + lib/tk9.0. Mount it at //zipfs:/dlsh_runtime and
 *     point TCL_LIBRARY/TK_LIBRARY there.
 *
 * Best-effort: if nothing is found, Tcl falls back to its own search.
 */
static void Dlsh_BootstrapRuntime(void)
{
    if (dlsh_path_exists("//zipfs:/app/tcl_library/init.tcl")) {
        return;                                   /* appended single-file */
    }

    char dir[2048] = ".";
    const char *exe = Tcl_GetNameOfExecutable();
    if (exe && *exe) {
        strncpy(dir, exe, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        char *slash = strrchr(dir, '/');
        if (slash) { *slash = '\0'; } else { strcpy(dir, "."); }
    }

    char cands[3][2048];
    int n = 0;
    snprintf(cands[n++], sizeof(cands[0]), "%s/dlsh-runtime.zip", dir);
    snprintf(cands[n++], sizeof(cands[0]), "%s/../Resources/dlsh-runtime.zip", dir);
    /* Installed CLI layout: exe in <prefix>/bin, runtime beside the package zip
       in <prefix>/dlsh -- so /usr/local/bin/dlsh finds /usr/local/dlsh/. */
    snprintf(cands[n++], sizeof(cands[0]), "%s/../dlsh/dlsh-runtime.zip", dir);

    for (int i = 0; i < n; i++) {
        if (!dlsh_path_exists(cands[i])) continue;
        if (TclZipfs_Mount(NULL, cands[i], "//zipfs:/dlsh_runtime", NULL) != TCL_OK
                && !dlsh_path_exists("//zipfs:/dlsh_runtime/lib")) {
            continue;
        }
        if (dlsh_path_exists("//zipfs:/dlsh_runtime/lib/tcl9.0/init.tcl")) {
            setenv("TCL_LIBRARY", "//zipfs:/dlsh_runtime/lib/tcl9.0", 1);
            if (dlsh_path_exists("//zipfs:/dlsh_runtime/lib/tk9.0/tk.tcl")) {
                setenv("TK_LIBRARY", "//zipfs:/dlsh_runtime/lib/tk9.0", 1);
            }
            return;
        }
    }
}

static int Dlsh_AppInit(Tcl_Interp *interp)
{
    if (Tcl_Init(interp) == TCL_ERROR) {
        return TCL_ERROR;
    }
    /* The dlsh PACKAGE is NOT linked in -- the bootstrap below mounts dlsh.zip
       and `package require dlsh` loads libdlsh + helpers from it, the same as
       dserv/stim2. (Only the Tcl/Tk runtime is baked into this binary.) */

#if defined(DLSH_WITH_TK)
    /* Make the statically-linked Aqua Tk load on demand: register it globally
       (interp=NULL) so `load {} Tk` invokes Tk_Init, then wire a `package
       ifneeded` that does exactly that. So `package require Tk` works, while
       the -e/headless path never inits Tk -- never touching the window server.
       Tk's script library is embedded (zipfs) in the linked .a, so nothing
       extra has to ship. TK_PATCH_LEVEL keeps the version lockstep with the
       Tk we actually linked. */
    Tcl_StaticLibrary(NULL, "Tk", Tk_Init, Tk_SafeInit);
    (void) Tcl_Eval(interp,
        "package ifneeded Tk " TK_PATCH_LEVEL " {load {} Tk}");
    Tcl_ResetResult(interp);
#endif

    /* Best-effort: mount the VFS if present. Never fatal. */
    (void) Tcl_Eval(interp, dlsh_vfs_mount);
    Tcl_ResetResult(interp);

    if (dlsh_dev_lib) {
        /* Dev mode (--libdlsh): load a working-tree libdlsh directly. Its
           Dlsh_Init calls Tcl_PkgProvide(dlsh,...), so this both registers the
           C commands and satisfies any later `package require dlsh` -- the zip's
           copy is never loaded. The zip's Tcl helper scripts (plot/stats/...)
           are therefore NOT sourced; this flag is for exercising the C lib. */
        Tcl_SetVar(interp, "dlsh_dev_lib", dlsh_dev_lib, TCL_GLOBAL_ONLY);
        /* No package prefix: let Tcl derive Dlsh_Init from the libdlsh
           filename, exactly as dlsh.zip's pkgIndex.tcl does. */
        if (Tcl_Eval(interp, "load $dlsh_dev_lib") != TCL_OK) {
            fprintf(stderr, "dlsh: --libdlsh: failed to load %s: %s\n",
                    dlsh_dev_lib, Tcl_GetStringResult(interp));
        } else {
            fprintf(stderr,
                    "dlsh: --libdlsh: loaded dev libdlsh from %s "
                    "(dlsh.zip Tcl helpers not sourced)\n", dlsh_dev_lib);
        }
        Tcl_ResetResult(interp);
    }
    else {
        /* Pull in the dlsh package (C lib + Tcl helpers) from the mounted zip,
           just as dserv/stim2 do. Best-effort: no zip -> bare Tcl/Tk. */
        (void) Tcl_Eval(interp, "catch { package require dlsh }");
        Tcl_ResetResult(interp);
    }

    /* Name the user startup file. Tcl_Main calls Tcl_SourceRCFile on its
       interactive path, which reads this variable and sources it iff it
       exists -- so it is a no-op for the -e one-shot path (never reaches
       Tcl_Main) and for non-interactive script/stdin use.
       NB: Tcl 9 dropped automatic "~" expansion in filename translation, so
       we resolve the home dir up front via `file home` (its cross-platform
       replacement) rather than handing Tcl_SourceRCFile a literal "~". */
    (void) Tcl_Eval(interp, "set ::tcl_rcFileName [file join [file home] .dlshrc]");
    Tcl_ResetResult(interp);

    return TCL_OK;
}

/*
 * One-shot evaluation: `dlsh -e <script> [args...]`. Evaluates <script>, prints
 * a non-empty result to stdout, and exits 0 on success / 1 on error (full
 * errorInfo to stderr) -- the scriptable/CI/agent entry point. Trailing args
 * are exposed as argv/argc/argv0 just as a script file would see them.
 */
static int Dlsh_EvalOnce(int argc, char **argv, const char *script)
{
    Tcl_Interp *interp;
    Tcl_Obj *av;
    int i, code, rc;

    Tcl_FindExecutable(argv[0]);
    interp = Tcl_CreateInterp();
    if (Dlsh_AppInit(interp) != TCL_OK) {
        fprintf(stderr, "dlsh: initialization failed: %s\n",
                Tcl_GetStringResult(interp));
        Tcl_DeleteInterp(interp);
        return 1;
    }

    av = Tcl_NewListObj(0, NULL);
    for (i = 3; i < argc; i++) {
        Tcl_ListObjAppendElement(interp, av, Tcl_NewStringObj(argv[i], -1));
    }
    Tcl_SetVar2Ex(interp, "argv", NULL, av, TCL_GLOBAL_ONLY);
    Tcl_SetVar2Ex(interp, "argc", NULL, Tcl_NewIntObj(argc - 3), TCL_GLOBAL_ONLY);
    Tcl_SetVar(interp, "argv0", argv[0], TCL_GLOBAL_ONLY);

    code = Tcl_EvalEx(interp, script, -1, TCL_EVAL_GLOBAL);
    if (code == TCL_OK) {
        const char *res = Tcl_GetStringResult(interp);
        if (res && *res) printf("%s\n", res);
        rc = 0;
    } else {
        const char *ei = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
        const char *res = Tcl_GetStringResult(interp);
        fprintf(stderr, "%s\n", (ei && *ei) ? ei : (res ? res : "error"));
        rc = 1;
    }
    Tcl_DeleteInterp(interp);
    return rc;
}

#if defined(DLSH_WITH_TK)
/*
 * `dlsh --gui [args]`: bring up the bundled GUI (dlshell: a Tk console with
 * cgraph/gbuf visualization). The script is NOT compiled in -- it is sourced
 * from the dlsh.zip the bootstrap already mounted (//zipfs:/dlsh/lib/dlsh/bin),
 * so it tracks the same source of truth as everything else and updates with the
 * zip. Then hand off to the Tk event loop. Requires a discoverable dlsh.zip.
 */
static int Dlsh_Gui(int argc, char **argv)
{
    Tcl_Interp *interp;
    Tcl_Obj *av;
    int i;

    Tcl_FindExecutable(argv[0]);
    interp = Tcl_CreateInterp();
    if (Dlsh_AppInit(interp) != TCL_OK) {
        fprintf(stderr, "dlsh --gui: init failed: %s\n", Tcl_GetStringResult(interp));
        Tcl_DeleteInterp(interp);
        return 1;
    }
    Tcl_SetVar(interp, "tcl_interactive", "1", TCL_GLOBAL_ONLY);
    /* Args after --gui are exposed as argv/argc, as a wish script would see. */
    av = Tcl_NewListObj(0, NULL);
    for (i = 2; i < argc; i++) {
        Tcl_ListObjAppendElement(interp, av, Tcl_NewStringObj(argv[i], -1));
    }
    Tcl_SetVar2Ex(interp, "argv", NULL, av, TCL_GLOBAL_ONLY);
    Tcl_SetVar2Ex(interp, "argc", NULL, Tcl_NewIntObj(argc - 2), TCL_GLOBAL_ONLY);
    Tcl_SetVar(interp, "argv0", argv[0], TCL_GLOBAL_ONLY);

    static const char *gui_launch =
        "package require Tk\n"
        "set g [file join [zipfs root] dlsh lib dlsh bin dlshell.tcl]\n"
        "if {![file exists $g]} {\n"
        "  error \"dlsh --gui: GUI script not found ($g); need a dlsh.zip with lib/dlsh/bin/dlshell.tcl\"\n"
        "}\n"
        "source $g\n";
    if (Tcl_Eval(interp, gui_launch) != TCL_OK) {
        const char *ei = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
        fprintf(stderr, "%s\n", ei ? ei : Tcl_GetStringResult(interp));
        Tcl_DeleteInterp(interp);
        return 1;
    }
    Tk_MainLoop();                 /* runs until the last window closes */
    Tcl_DeleteInterp(interp);
    return 0;
}
#endif /* DLSH_WITH_TK */

int main(int argc, char **argv)
{
    /* Tcl 9 single-file deployment: if a Tcl script-library zip is appended to
       this executable, mount it (//zipfs:/app) and prime tcl_library before any
       interp is created -- so Tcl_Init finds init.tcl on EVERY path (incl. -e).
       A no-op when nothing is appended (e.g. a shared-Tcl build whose library
       is embedded in the dylib), so it is always safe to call. */
    TclZipfs_AppHook(&argc, &argv);

    /* If not a single-file build, find the sidecar dlsh-runtime.zip and point
       Tcl/Tk at the script libraries it carries -- before any interp exists. */
    Dlsh_BootstrapRuntime();

    /* `dlsh --libdlsh <path> [...]`: dev flag -- load a working-tree libdlsh
       instead of the one inside dlsh.zip, for testing an unreleased build.
       Consume the two args here so the remaining dispatch (-e / --gui /
       Tcl_Main) sees argv exactly as if the flag were absent. */
    if (argc >= 2 && strcmp(argv[1], "--libdlsh") == 0) {
        if (argc < 3) {
            fprintf(stderr, "dlsh: --libdlsh requires a path argument\n");
            return 2;
        }
        dlsh_dev_lib = argv[2];
        argv[2] = argv[0];   /* keep program name at the new argv[0] */
        argv += 2;
        argc -= 2;
    }

    /* `dlsh -e <script> [args...]`: evaluate and exit (scriptable/CI use). */
    if (argc >= 3 && (strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "--eval") == 0)) {
        return Dlsh_EvalOnce(argc, argv, argv[2]);
    }

#if defined(DLSH_WITH_TK)
    /* `dlsh --gui [args]`: launch the bundled Tk GUI from the zip. */
    if (argc >= 2 && strcmp(argv[1], "--gui") == 0) {
        return Dlsh_Gui(argc, argv);
    }
#endif

    /* Everything else -- script file, piped stdin, interactive prompt -- is
       stock Tcl_Main: tclsh-identical, event-loop-driven, and Tk-ready. */
    Tcl_Main(argc, argv, Dlsh_AppInit);
    return 0;   /* Tcl_Main does not return */
}
