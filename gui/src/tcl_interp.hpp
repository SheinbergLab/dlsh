#ifndef __TCLINTERP_H
#define __TCLINTERP_H

#include <tcl.h>
#include <cassert>

class TclInterp {
private:
  Tcl_Interp *_interp;

public:
  int DlshAppInit(Tcl_Interp *interp)
  {
#ifndef _WIN32
    setenv("TCLLIBPATH", "", 1);
#endif
    
    if (Tcl_Init(interp) == TCL_ERROR) return TCL_ERROR;

    Tcl_VarEval(interp, 
		"proc load_local_packages {} {\n"
		" global auto_path\n"
		" set f [file dirname [info nameofexecutable]]\n"
		" if [file exists [file join $f dlsh.zip]] { set dlshzip [file join $f dlsh.zip] } {"
#ifdef _WIN32
		"   set dlshzip c:/usr/local/dlsh/dlsh.zip }\n"
#else
		"   set dlshzip /usr/local/dlsh/dlsh.zip }\n"
#endif
		" set dlshroot [file join [zipfs root] dlsh]\n"
		" zipfs unmount $dlshroot\n"
		" zipfs mount $dlshzip $dlshroot\n"
		" set auto_path [linsert $auto_path [set auto_path 0] $dlshroot/lib]\n"
		" package require dlsh; package require flcgwin\n"
		"}\n"
		"load_local_packages\n",
		NULL);
    
    return TCL_OK;
  }


  TclInterp(int argc, char *argv[])
  {
    _interp = Tcl_CreateInterp();
    assert(_interp != NULL);
#if 0
    if (TclZipfs_Mount(_interp, "dlsh.zip", "app", NULL) != TCL_OK) {
      printf("error mounting zip from \"%s\"\n", argv[1]); 
    }
#endif
#ifdef _WIN32
    TclZipfs_AppHook(&argc, (wchar_t ***) &argv);
#else
    TclZipfs_AppHook(&argc, &argv);
#endif
    /*
     * Invoke application-specific initialization.
     */
    
    if (DlshAppInit(_interp) != TCL_OK) {
      std::cerr << "application-specific initialization failed: ";
      std::cerr << Tcl_GetStringResult(_interp) << std::endl;
    }
    else {
      Tcl_SourceRCFile(_interp);
    }
  }

  ~TclInterp(void)
  {
    /* Free the interpreter */
    Tcl_DeleteInterp(_interp);
  }

  Tcl_Interp *interp(void) { return _interp; }
  
  int eval(const char *command, std::string &resultstr)
  {
    int len;
    int result = Tcl_Eval(_interp, command);
    auto rstr = Tcl_GetStringResult(_interp);
    resultstr = std::string(rstr);
    return result;
  }

  std::string eval(const char *command)
  {
    int len;
    int result = Tcl_Eval(_interp, command);
    auto rstr = Tcl_GetStringResult(_interp);
    return std::string(rstr);
  }
};


#endif /* __TCLINTERP_H */
