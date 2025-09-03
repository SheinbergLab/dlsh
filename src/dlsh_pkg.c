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

EXPORT(int,Dlsh_Init) (Tcl_Interp *interp)
{
  if (Dl_Init(interp) == TCL_ERROR) return(TCL_ERROR);
  if (Df_Init(interp) == TCL_ERROR) return(TCL_ERROR);
  if (Dlg_Init(interp) == TCL_ERROR) return(TCL_ERROR);
  if (Cgbase_Init(interp) == TCL_ERROR) return(TCL_ERROR) ;

  Tcl_PkgProvide(interp, "dlsh", "1.2");

  return TCL_OK;
}

EXPORT(int,Dlsh_SafeInit) (Tcl_Interp *interp)
{
  return Dlsh_Init(interp);
}

EXPORT(int,Dlsh_Unload) (Tcl_Interp *interp)
{
  return TCL_OK;
}

EXPORT(int,Dlsh_SafeUnload) (Tcl_Interp *interp)
{
  return TCL_OK;
}

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
