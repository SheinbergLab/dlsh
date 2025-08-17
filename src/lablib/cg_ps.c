/***********************************************\
* File                                          *
*   cg_ps.c                                     *
*                                               *
* Description                                   *
*   Functions for use of off-screen graphics    *
* with dlsh/dlwish                              *
*                                               *
* Functions                                     *
\***********************************************/

#include <stdlib.h>
#include <math.h>
#include <tcl.h>

#include "cgraph.h"
#include <gbuf.h>


struct dl_ps_ctx {
  FRAME fr ;
  GBUF_DATA gb ;
  struct dl_ps_ctx * next ;
} ;


typedef struct dl_ps_ctx dpc ;
dpc * first_dpc ;
dpc * default_dpc ;
dpc * current_dpc ;
FRAME * default_frame ;
GBUF_DATA * default_gb ;


float PSColorTableVals[][4] = {
/* R    G    B   Grey -- currently we use the grey approx. */
  { 0.0, 0.0, 0.0, 0.0 },       /*   0 -> BLACK     1 -> WHITE     */
  { 0.1, 0.1, 0.4, 0.4 },
  { 0.0, 0.35, 0.0, 0.1 },
  { 0.0, 0.7, 0.7, 0.7 },
  { 0.8, 0.05, 0.0, 0.3 },
  { 0.8, 0.0, 0.8, 0.0 },
  { 0.0, 0.0, 0.0, 0.0 },
  { 0.0, 0.0, 0.0, 0.0 },
  { 0.7, 0.7, 0.7, 0.7 },
  { 0.3, 0.45, 0.9, 0.0 },
  { 0.05, 0.95, 0.1, 0.0 },
  { 0.0, 0.9, 0.9, 0.9 },
  { 0.0, 0.0, 0.0, 0.0 },
  { 0.0, 0.0, 0.0, 0.0 },
  { 0.94, 0.94, 0.05, 0.8 },
  { 0.0, 0.0, 0.0, 0.2 },
  { 1.0, 1.0, 1.0, 1.0 },
  { 0.96, 0.96, 0.96, 0.96 },
};
int NColorVals = 18;

/* dummy handlers */

static void GS_Point(float x, float y) {}

static void GS_Line(float x1, float y1, float x2, float y2) {}

static void GS_Poly(float * verts, int nverts) {}

static void GS_Circle(float x, float y, float width, int filled) {}

void dl_ps_init_frame(float w, float h, int inverse_video)
{
	setresol(w, h) ;
	setpoint((PHANDLER) GS_Point) ;
	setline((LHANDLER) GS_Line) ;
	setfilledpoly((FHANDLER) GS_Poly) ;
	setcircfunc((CHANDLER) GS_Circle) ;
	setjust(CENTER_JUST) ;
	setfont("Helvetica", 10) ;
	setcolor((inverse_video ? 7 : 0)) ;
	setlstyle(1) ;
	setlwidth(100) ;
	setwindow(0, 0, 1, 1) ;
	setfviewport(0, 0, 1, 1) ;
}

void dl_ps_init_dpc(dpc * inst, float w, float h, int inverse_video)
{
  GBUF_DATA * gb = gbGetGeventBuffer() ;
  
  gbDisableCurrentBuffer() ;				  /* valid data */
  setframe(&inst->fr);
  dl_ps_init_frame(w, h, inverse_video) ;
  gbInitGeventBuffer(&inst->gb) ;
  gbSetGeventBuffer(gb) ;
  gbEnableCurrentBuffer() ;
  inst->next = first_dpc;
  first_dpc = inst ;
}


int dl_ps_install_dpc(dpc * inst)
{
  dpc * d ;
  
  if (inst == NULL) d = default_dpc ;
  else for (d = first_dpc ; d != inst && d != NULL ; d = d->next) ;
  if (d == NULL) return(0) ;
  current_dpc = d;
  setframe(&d->fr);
  gbSetGeventBuffer(& d->gb);
  gbEnableCurrentBuffer();
  user();
  return(1);
}


dpc *dl_ps_create(void)
{
  dpc *new_dpc ;
  
  if (!(new_dpc = calloc(1, sizeof(dpc)))) return(NULL) ;
  dl_ps_init_dpc(new_dpc, 640, 480, 0) ;
  current_dpc = new_dpc;
  setframe(&new_dpc->fr);
  gbSetGeventBuffer(&new_dpc->gb);
  gbEnableCurrentBuffer();
  user();
  return(new_dpc);
}


int dl_ps_destroy(dpc * inst)
{
  GBUF_DATA * gb;
  dpc * d;
  
  if (first_dpc == NULL) return(0) ;
  if (inst == first_dpc) first_dpc = inst->next ;
  else {
    for (d = first_dpc ; d->next != inst && d->next != NULL ;
	 d = d->next) ;
    if (d->next == NULL) return(0) ;
    d->next = d->next->next ;
  }
  gbDisableCurrentBuffer() ;
  if (inst == current_dpc)
    dl_ps_install_dpc(NULL) ;
  gb = gbGetGeventBuffer() ;
  gbSetGeventBuffer(& inst->gb) ;
  gbCloseGevents() ;
  gbSetGeventBuffer(gb) ;
  free(d) ;
  return(1) ;
}


void dl_ps_resize(int w, int h)
{
  dl_ps_init_dpc(current_dpc, w, h, 0) ;
}


int tclPSCreate(ClientData clientData, Tcl_Interp * interp, int argc,
		char * argv[])
{
  Tcl_SetObjResult(interp, Tcl_NewIntObj(dl_ps_create()));
  return(TCL_OK) ;
}


int tclPSDelete(ClientData clientData, Tcl_Interp * interp, int argc,
		char * argv[])
{
  dpc * inst ;
  
  if (argc < 1) {
    Tcl_AppendResult(interp, "Usage: ", argv[0],
		     " id", (char *) NULL) ;
    return(TCL_ERROR) ;
  }
  if (Tcl_GetInt(interp, argv[1], (int *) &inst) != TCL_OK) {
    return(TCL_ERROR) ;
  }
  if (!dl_ps_destroy(inst))
    {
      Tcl_AppendResult(interp, argv[0], " : id is not valid",
		       (char *) NULL) ;
      return(TCL_ERROR) ;
    }
  return(TCL_OK) ;
}


int tclPSSelect(ClientData clientData, Tcl_Interp * interp, int argc,
		char * argv[])
{
  dpc * inst ;
  
  if (argc < 1) {
    Tcl_AppendResult(interp, "Usage: ", argv[0],
		     " id", (char *) NULL) ;
    return(TCL_ERROR) ;
  }
  if (Tcl_GetInt(interp, argv[1], (int *) &inst) != TCL_OK) {
    return(TCL_ERROR) ;
  }
  if (!dl_ps_install_dpc(inst)) {
    Tcl_AppendResult(interp, argv[0], " : id is not valid",
		     (char *) NULL) ;
    return(TCL_ERROR) ;
  }
  return(TCL_OK) ;
}


int tclPSResize(ClientData clientData, Tcl_Interp * interp, int argc,
		char * argv[])
{
  int w, h ;
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "Usage: ", argv[0],
		     " width height", (char *) NULL) ;
    return(TCL_ERROR) ;
  }
  if (Tcl_GetInt(interp, argv[1], &w) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &h) != TCL_OK) return TCL_ERROR;

  dl_ps_resize(w, h) ;
  return(TCL_OK) ;
}

int tclFlushWin(ClientData clientData, Tcl_Interp * interp, int argc,
		char * argv[])
{
  return(TCL_OK) ;
}

int Cgps_Init(Tcl_Interp * interp)
{
  if (
#ifdef USE_TCL_STUBS
      Tcl_InitStubs(interp, "8.5-", 0)
#else
      Tcl_PkgRequire(interp, "Tcl", "8.5-", 0)
#endif
      == NULL) {
    return TCL_ERROR;
  }

  Tcl_CreateCommand(interp, "flushwin",
		    (Tcl_CmdProc *) tclFlushWin, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL) ;
  Tcl_CreateCommand(interp, "createps", (Tcl_CmdProc *) tclPSCreate,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL) ;
  Tcl_CreateCommand(interp, "deleteps", (Tcl_CmdProc *) tclPSDelete,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL) ;
  Tcl_CreateCommand(interp, "selectps", (Tcl_CmdProc *) tclPSSelect,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL) ;
  Tcl_CreateCommand(interp, "resizeps", (Tcl_CmdProc *) tclPSResize,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL) ;

  first_dpc = default_dpc = dl_ps_create() ;

  return(TCL_OK) ;
}
