/**************************************************************************
 *
 * NAME:        
 *  labtcl.h
 *
 * PURPOSE:     
 *  Header file for lab programs using tcl/tk.
 *
 *************************************************************************/

/*
 * An alternate main routine which uses the readline library
 */
extern void Tk_ReadlineMain(int argc, char **argv, Tcl_AppInitProc *);
extern void Tcl_ReadlineMain(int argc, char **argv, Tcl_AppInitProc *);

extern int Cg_Init(Tcl_Interp *);
extern int CgPS_Init(Tcl_Interp *);
extern int CgBase_Init(Tcl_Interp *);
extern int Df_Init(Tcl_Interp *);
extern int Dl_Init(Tcl_Interp *);
extern int Dlg_Init(Tcl_Interp *);
extern int Raster_Init(Tcl_Interp *);
extern int Tix_Init(Tcl_Interp *);

extern void Sig_Setup(Tcl_Interp *interp);

typedef int (*TCL_FUNCTION)(ClientData, Tcl_Interp *, int, char **);
typedef int (*TCL_OBJ_FUNCTION)(ClientData, Tcl_Interp *, int,
	Tcl_Obj * CONST objv[]);

typedef struct {
  char *name;
  TCL_FUNCTION func;
  ClientData cd;
  char *desc;
} TCL_COMMANDS;

typedef struct {
  char *name;
  TCL_OBJ_FUNCTION func;
  ClientData cd;
  char *desc;
} TCL_OBJ_COMMANDS;

