/*
 * dlsh_main.c
 *   Entry point for the standalone `dlsh` interpreter -- a tclsh that has the
 *   dlsh C packages compiled/linked in. Two modes:
 *
 *     - Non-interactive (a script arg, or piped stdin, or a non-linenoise
 *       build): delegate to stock Tcl_Main, so behavior is byte-for-byte
 *       tclsh-compatible.
 *     - Interactive TTY (and built with linenoise): a REPL with arrow-key
 *       history, emacs editing, persistent history, and multi-line input --
 *       core tclsh has none of this.
 *
 *   Uses plain Tcl_Main / Tcl_CreateInterp (NOT dlAppInit.c's Tcl_ReadlineMain,
 *   which references a readline shim not built here).
 *
 *   The dlsh packages are registered statically so their commands AND
 *   `package require dlsh` work without a separate libdlsh load step -- the
 *   same scripts then run identically here, in stim2, and in dserv.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <tcl.h>

#if defined(DLSH_HAVE_LINENOISE)
#include "linenoise.h"
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
    "    if {[file isdirectory $base/lib] && $base/lib ni $::auto_path} {\n"
    "      set ::auto_path [linsert $::auto_path 0 $base/lib]\n"
    "    }\n"
    "    return $z\n"
    "  }\n"
    "  return {}\n"
    "}}\n";

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

    /* Best-effort: mount the VFS if present. Never fatal. */
    (void) Tcl_Eval(interp, dlsh_vfs_bootstrap);
    Tcl_ResetResult(interp);

    return TCL_OK;
}

#if defined(DLSH_HAVE_LINENOISE)
/*
 * Interactive REPL backed by linenoise. Accumulates lines until the buffered
 * text is a complete Tcl command (Tcl_CommandComplete), then evaluates it.
 */
static int Dlsh_Repl(int argc, char **argv)
{
    Tcl_Interp *interp;
    Tcl_DString cmd;
    char *line;
    char history[1024];
    const char *home;
    int code;

    Tcl_FindExecutable(argv[0]);
    interp = Tcl_CreateInterp();
    if (Dlsh_AppInit(interp) != TCL_OK) {
        const char *msg = Tcl_GetStringResult(interp);
        fprintf(stderr, "dlsh: initialization failed: %s\n", msg ? msg : "");
        return 1;
    }
    Tcl_SetVar(interp, "tcl_interactive", "1", TCL_GLOBAL_ONLY);

    history[0] = '\0';
    if ((home = getenv("HOME")) != NULL) {
        snprintf(history, sizeof(history), "%s/.dlsh_history", home);
        linenoiseHistoryLoad(history);
    }
    linenoiseHistorySetMaxLen(1000);
    linenoiseSetMultiLine(1);

    Tcl_DStringInit(&cmd);
    while ((line = linenoise(Tcl_DStringLength(&cmd) ? "    > " : "dlsh% ")) != NULL) {
        Tcl_DStringAppend(&cmd, line, -1);
        Tcl_DStringAppend(&cmd, "\n", 1);
        linenoiseFree(line);

        if (!Tcl_CommandComplete(Tcl_DStringValue(&cmd))) {
            continue;   /* keep reading: open brace/bracket/quote */
        }

        /* a lone blank line resets the buffer without error noise */
        if (Tcl_DStringLength(&cmd) > 1) {
            linenoiseHistoryAdd(Tcl_DStringValue(&cmd));
            if (history[0]) linenoiseHistorySave(history);

            code = Tcl_EvalEx(interp, Tcl_DStringValue(&cmd), -1, TCL_EVAL_GLOBAL);
            const char *res = Tcl_GetStringResult(interp);
            if (code == TCL_OK) {
                if (res && *res) printf("%s\n", res);
            } else {
                fprintf(stderr, "%s\n", res ? res : "");
            }
        }
        Tcl_DStringFree(&cmd);
        Tcl_DStringInit(&cmd);
    }
    Tcl_DStringFree(&cmd);
    printf("\n");
    Tcl_DeleteInterp(interp);
    return 0;
}
#endif /* DLSH_HAVE_LINENOISE */

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
    /* `dlsh -e <script> [args...]`: evaluate and exit (scriptable/CI use). */
    if (argc >= 3 && (strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "--eval") == 0)) {
        return Dlsh_EvalOnce(argc, argv, argv[2]);
    }

#if defined(DLSH_HAVE_LINENOISE)
    /* Custom REPL only for an interactive terminal with no script argument;
       everything else (script file, piped stdin) goes to stock Tcl_Main. */
    if (argc == 1 && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
        return Dlsh_Repl(argc, argv);
    }
#endif
    Tcl_Main(argc, argv, Dlsh_AppInit);
    return 0;   /* Tcl_Main does not return */
}
