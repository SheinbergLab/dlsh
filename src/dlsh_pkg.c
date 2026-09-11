/* 
 * dlsh_pkg.c
 *
 */

#include "tcl.h"
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#if defined(_MSC_VER)
#define EXPORT(a,b) __declspec(dllexport) a b
#define DllEntryPoint DllMain
#endif
#else
#define EXPORT(a,b) a b
#endif

#include <tcl.h>

extern int Dl_Init(Tcl_Interp * interp) ;
extern int Df_Init(Tcl_Interp * interp) ;
extern int Dlg_Init(Tcl_Interp * interp) ;
extern int Cgps_Init(Tcl_Interp * interp) ;
extern int Cgbase_Init(Tcl_Interp * interp) ;
extern int DlNoise_Init(Tcl_Interp * interp) ;

EXPORT(int,Dlsh_Init) (Tcl_Interp *interp)
{
  if (Dl_Init(interp) == TCL_ERROR) return(TCL_ERROR);
  if (Df_Init(interp) == TCL_ERROR) return(TCL_ERROR);
  if (Dlg_Init(interp) == TCL_ERROR) return(TCL_ERROR);
  if (Cgbase_Init(interp) == TCL_ERROR) return(TCL_ERROR) ;
  if (DlNoise_Init(interp) == TCL_ERROR) return(TCL_ERROR);

  Tcl_PkgProvide(interp, "dlsh", "1.2");

  return TCL_OK;
}

EXPORT(int,Dlsh_SafeInit) (Tcl_Interp *interp)
{
  return Dlsh_Init(interp);
}

/*
 * Deliberately NO Dlsh_Unload / Dlsh_SafeUnload.
 *
 * Exporting an unload proc tells Tcl the library may be dlclose'd, and Tcl
 * does exactly that when the last interpreter that loaded it is deleted
 * (tclLoad.c LoadCleanupProc -> UnloadLibrary -> Tcl_FSUnloadFile, under
 * TCL_UNLOAD_DLLS which is on for every Unix build).  That happens from the
 * assoc-data callbacks in DeleteInterpProc, i.e. BEFORE the interpreter frees
 * its result, errorInfo/errorStack and literals -- and any of those may hold a
 * Tcl_Obj of the "dynlist" type (dlref.c), whose Tcl_ObjType struct and
 * freeIntRepProc live in this library.  TclFreeObj then calls through an
 * unmapped pointer: `dlsh -e 'dl_ilist 1 2 3'` segfaulted at exit, as did any
 * `-e` script in which a command failed with a list handle among its
 * arguments.  The library also registers the obj type process-wide and
 * installs dfuDynListFreeHook, neither of which can be taken back.
 *
 * With no unload proc, UnloadLibrary just detaches the interp and leaves the
 * code mapped, which is the only safe thing for this library.
 */

#ifdef WIN32
BOOL APIENTRY
DllEntryPoint(hInst, reason, reserved)
    HINSTANCE hInst;
    DWORD reason;
    LPVOID reserved;
{
	return TRUE;
}
#endif
