/*
 * dlsh_main.c
 *   Entry point for the standalone `dlsh` interpreter -- a tclsh that has the
 *   dlsh C packages compiled/linked in. Two modes:
 *
 *     - `dlsh -e <script>`: one-shot evaluation for scripting/CI/agents.
 *     - everything else: delegate to stock Tcl_Main, so behavior is
 *       byte-for-byte tclsh-compatible (script file, piped stdin, and the
 *       interactive prompt -- including event-loop pumping, which keeps a
 *       future Tk's windows live at the prompt with no extra plumbing).
 *
 *   The interactive prompt is plain tclsh: no arrow-key line editing. That is
 *   a deliberate simplicity tradeoff -- interactive use is light here (the
 *   primary interfaces are web front-ends), and leaning on Tcl_Main keeps the
 *   loop event-driven for Tk. (rc-file sourcing, result echoing, and
 *   multi-line input all come from Tcl_Main for free.)
 *
 *   The dlsh packages are registered statically so their commands AND
 *   `package require dlsh` work without a separate libdlsh load step -- the
 *   same scripts then run identically here, in stim2, and in dserv.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>

#if defined(DLSH_WITH_TK)
#include <tk.h>
#endif

/* libdlsh's package init: runs all sub-inits (Dl_Init/Df_Init/Dlg_Init/
   Cgbase_Init/DlNoise_Init), evaluates the embedded dl_comprehension prelude,
   and Tcl_PkgProvide("dlsh", ...). */
extern int Dlsh_Init(Tcl_Interp *interp);
extern int Dlsh_SafeInit(Tcl_Interp *interp);

/*
 * Mount the dlsh.zip VFS if we can find it, and add its lib/ to auto_path -- so
 * the standalone `dlsh` is self-sufficient (the pure-Tcl library + sub-packages
 * are available without sourcing dlsh_setup.tcl). This is the app's deployment
 * policy, kept OUT of libdlsh's Dlsh_Init. It is graceful: if no zip is found,
 * the C commands + embedded comprehension prelude still work.
 *
 * Discovery order: $DLSH_ZIP -> $DLSH_LIBRARY/dlsh.zip -> next to / near the
 * executable (incl. ../Resources for a future .app bundle) -> /usr/local.
 * dlsh is already Tcl_PkgProvide'd above, so the zip's own dlsh pkgIndex never
 * triggers a second load.
 *
 * We then drop /usr/local/lib from auto_path: Tcl's init adds it (it is the
 * configured prefix's lib dir), but it holds older copies of our packages that
 * would shadow the ones in dlsh.zip. dlsh now ships its libraries via the zip,
 * so the system lib dir is off by default. A user who wants it back can re-add
 * it explicitly (e.g. from a future ~/.dlshrc).
 */
static const char *dlsh_vfs_bootstrap =
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
 * Make a Tcl (and Tk) script library reachable BEFORE Tcl_Init -- the one thing
 * the post-init dlsh.zip bootstrap below cannot do, since Tcl_Init itself needs
 * init.tcl.
 *
 *   - Single-file build: TclZipfs_AppHook already mounted an archive appended
 *     to the executable at //zipfs:/app (tcl_library/ at its root). Nothing to
 *     do. This is the portable-but-unsignable form (trailing data breaks macOS
 *     codesign), good for Linux / dev / CI artifacts.
 *   - Signable build: the executable has no trailing data; the runtime ships in
 *     a sidecar dlsh.zip (next to the exe, or ../Resources in a .app) carrying
 *     lib/tcl9.0 + lib/tk9.0. Mount it and point TCL_LIBRARY/TK_LIBRARY there.
 *
 * Best-effort: if nothing is found, Tcl falls back to its own search (e.g. an
 * installed prefix). The post-init bootstrap then sees //zipfs:/dlsh already
 * mounted and only has to extend auto_path.
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

    char cands[4][2048];
    int n = 0;
    const char *env_zip = getenv("DLSH_ZIP");
    if (env_zip && *env_zip) snprintf(cands[n++], sizeof(cands[0]), "%s", env_zip);
    snprintf(cands[n++], sizeof(cands[0]), "%s/dlsh.zip", dir);
    snprintf(cands[n++], sizeof(cands[0]), "%s/../Resources/dlsh.zip", dir);
    snprintf(cands[n++], sizeof(cands[0]), "/usr/local/dlsh/dlsh.zip");

    for (int i = 0; i < n; i++) {
        if (!dlsh_path_exists(cands[i])) continue;
        if (TclZipfs_Mount(NULL, cands[i], "//zipfs:/dlsh", NULL) != TCL_OK
                && !dlsh_path_exists("//zipfs:/dlsh/lib")) {
            continue;
        }
        if (dlsh_path_exists("//zipfs:/dlsh/lib/tcl9.0/init.tcl")) {
            setenv("TCL_LIBRARY", "//zipfs:/dlsh/lib/tcl9.0", 1);
            if (dlsh_path_exists("//zipfs:/dlsh/lib/tk9.0/tk.tcl")) {
                setenv("TK_LIBRARY", "//zipfs:/dlsh/lib/tk9.0", 1);
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
    if (Dlsh_Init(interp) == TCL_ERROR) {
        return TCL_ERROR;
    }
    /* So `package require dlsh` resolves (incl. in child interps) without a
       separate .so load step. */
    Tcl_StaticLibrary(interp, "dlsh", Dlsh_Init, Dlsh_SafeInit);

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
    (void) Tcl_Eval(interp, dlsh_vfs_bootstrap);
    Tcl_ResetResult(interp);

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

int main(int argc, char **argv)
{
    /* Tcl 9 single-file deployment: if a Tcl script-library zip is appended to
       this executable, mount it (//zipfs:/app) and prime tcl_library before any
       interp is created -- so Tcl_Init finds init.tcl on EVERY path (incl. -e).
       A no-op when nothing is appended (e.g. a shared-Tcl build whose library
       is embedded in the dylib), so it is always safe to call. */
    TclZipfs_AppHook(&argc, &argv);

    /* If not a single-file build, find a sidecar dlsh.zip and point Tcl/Tk at
       the script libraries it carries -- before any interp is created. */
    Dlsh_BootstrapRuntime();

    /* `dlsh -e <script> [args...]`: evaluate and exit (scriptable/CI use). */
    if (argc >= 3 && (strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "--eval") == 0)) {
        return Dlsh_EvalOnce(argc, argv, argv[2]);
    }

    /* Everything else -- script file, piped stdin, interactive prompt -- is
       stock Tcl_Main: tclsh-identical, event-loop-driven, and Tk-ready. */
    Tcl_Main(argc, argv, Dlsh_AppInit);
    return 0;   /* Tcl_Main does not return */
}
