#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(SUN4) || defined(LYNX)
#include <unistd.h>
#endif

#include <tcl.h>
#include "cgraph.h"
#include "gbuf.h"
#include "gbufutl.h"
#include "cg_base_wrapper.h"

/* Define wrapper functions for all commands */
CG_CMD_WRAPPER_DECL(cgClearWindow)
CG_CMD_WRAPPER_DECL(cgDumpWindow)
CG_CMD_WRAPPER_DECL(cgPlayback)
CG_CMD_WRAPPER_DECL(gbSizeCmd)
CG_CMD_WRAPPER_DECL(gbIsEmptyCmd)
CG_CMD_WRAPPER_DECL(gbResetCmd)
CG_CMD_WRAPPER_DECL(cgGetResol)
CG_CMD_WRAPPER_DECL(cgGetFrame)
CG_CMD_WRAPPER_DECL(cgGetXScale)
CG_CMD_WRAPPER_DECL(cgGetYScale)
CG_CMD_WRAPPER_DECL(cgWindowToScreen)
CG_CMD_WRAPPER_DECL(cgScreenToWindow)
CG_CMD_WRAPPER_DECL(cgPushViewport)
CG_CMD_WRAPPER_DECL(cgPopViewport)
CG_CMD_WRAPPER_DECL(cgSetViewport)
CG_CMD_WRAPPER_DECL(cgGetViewport)
CG_CMD_WRAPPER_DECL(cgGetFViewport)
CG_CMD_WRAPPER_DECL(cgGetWindow)
CG_CMD_WRAPPER_DECL(cgGetAspect)
CG_CMD_WRAPPER_DECL(cgGetUAspect)
CG_CMD_WRAPPER_DECL(cgSetFViewport)
CG_CMD_WRAPPER_DECL(cgSetPViewport)
CG_CMD_WRAPPER_DECL(cgSetWindow)
CG_CMD_WRAPPER_DECL(cgSetPSPageOri)
CG_CMD_WRAPPER_DECL(cgSetPSPageFill)
CG_CMD_WRAPPER_DECL(cgGsave)
CG_CMD_WRAPPER_DECL(cgGrestore)
CG_CMD_WRAPPER_DECL(cgGroup)
CG_CMD_WRAPPER_DECL(cgUngroup)
CG_CMD_WRAPPER_DECL(cgFrame)
CG_CMD_WRAPPER_DECL(cgMoveto)
CG_CMD_WRAPPER_DECL(cgLineto)
CG_CMD_WRAPPER_DECL(cgDotAt)
CG_CMD_WRAPPER_DECL(cgSquare)
CG_CMD_WRAPPER_DECL(cgFsquare)
CG_CMD_WRAPPER_DECL(cgPoly)
CG_CMD_WRAPPER_DECL(cgFpoly)
CG_CMD_WRAPPER_DECL(cgCircle)
CG_CMD_WRAPPER_DECL(cgFcircle)
CG_CMD_WRAPPER_DECL(cgFilledRect)
CG_CMD_WRAPPER_DECL(cgRect)
CG_CMD_WRAPPER_DECL(cgSetorientation)
CG_CMD_WRAPPER_DECL(cgSetjust)
CG_CMD_WRAPPER_DECL(cgSetlstyle)
CG_CMD_WRAPPER_DECL(cgSetlwidth)
CG_CMD_WRAPPER_DECL(cgRGBcolor)
CG_CMD_WRAPPER_DECL(cgGetcolor)
CG_CMD_WRAPPER_DECL(cgSetcolor)
CG_CMD_WRAPPER_DECL(cgSetfont)
CG_CMD_WRAPPER_DECL(cgSetsfont)
CG_CMD_WRAPPER_DECL(cgPostscript)
CG_CMD_WRAPPER_DECL(cgSetImagePreview)
CG_CMD_WRAPPER_DECL(cgDrawtext)
CG_CMD_WRAPPER_DECL(cgSetclip)
CG_CMD_WRAPPER_DECL(cgSetClipRegion)
CG_CMD_WRAPPER_DECL(cgLYaxis)
CG_CMD_WRAPPER_DECL(cgLXaxis)

static int cgClearWindow(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  setfviewport(0,0,1,1);
  clearscreen();
  return TCL_OK;
}

static int cgDumpWindow(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  char *outfile = NULL;
  static char *usage = "usage: dumpwin {printer|ascii|raw|postscript|pdf|string}";
  if (argc > 2) outfile = argv[2];
  if (argc < 2) {
    Tcl_SetResult(interp, usage, TCL_STATIC);
    return TCL_ERROR;
  }
  if (!strcmp(argv[1],"printer")) {
    gbPrintGevents();
    return TCL_OK;
  }
  else if (!strcmp(argv[1],"raw")) {
    if (argc < 3) {
      Tcl_SetResult(interp, "usage: dumpwin raw filename", TCL_STATIC);
      return TCL_ERROR;
    }
    gbWriteGevents(outfile,GBUF_RAW);
    return(TCL_OK);
  }
  else if (!strcmp(argv[1],"ascii")) {
    gbWriteGevents(outfile,GBUF_ASCII);
    return(TCL_OK);
  }
  else if (!strcmp(argv[1],"postscript")) {
    gbWriteGevents(outfile,GBUF_PS);
    return(TCL_OK);
  }
  else if (!strcmp(argv[1],"pdf")) {
    gbWriteGevents(outfile,GBUF_PDF);
    return(TCL_OK);
  }
  else if (!strcmp(argv[1],"string")) {
    char *result_string = gbuf_dump_ascii_to_string(GB_GBUF(gbGetGeventBuffer()), 
                                                   GB_GBUFINDEX(gbGetGeventBuffer()));
    if (!result_string) {
      Tcl_SetResult(interp, "Error: Unable to convert graphics buffer to string", TCL_STATIC);
      return TCL_ERROR;
    }
    
    if (argc >= 3) {
      /* Store result in variable and return byte count */
      if (Tcl_SetVar(interp, argv[2], result_string, TCL_LEAVE_ERR_MSG) == NULL) {
        free(result_string);
        return TCL_ERROR;
      }
      /* Return the length of the string */
      char length_str[32];
      sprintf(length_str, "%d", (int)strlen(result_string));
      Tcl_SetResult(interp, length_str, TCL_VOLATILE);
      free(result_string);
    } else {
      /* Copy the string to Tcl-managed memory */
      Tcl_SetResult(interp, result_string, TCL_VOLATILE);
      free(result_string);
    }
    return TCL_OK;
  }
  else {
    Tcl_SetResult(interp, usage, TCL_STATIC);
    return TCL_ERROR;
  }
}

static int cgPlayback(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  FILE *fp;
  if (argc < 2) {
    Tcl_SetResult(interp, "usage: gbufplay filename", TCL_STATIC);
    return TCL_ERROR;
  }
  
  if (!(fp = fopen(argv[1],"rb"))) {
    Tcl_AppendResult(interp, argv[0], ": unable to open file ", argv[1], NULL);
    return TCL_ERROR;
  }
  playback_gfile(fp);
  
  fclose(fp);
  
  return(TCL_OK);
}


static int gbSizeCmd(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  extern int gbSize(void);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(gbSize()));
  return TCL_OK;
}

static int gbIsEmptyCmd(ClientData clientData, Tcl_Interp *interp,
		     int argc, char *argv[])
{
  Tcl_SetObjResult(interp, Tcl_NewIntObj(gbIsEmpty()));
  return TCL_OK;
}

static int gbResetCmd(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  gbResetCurrentBuffer();
  return TCL_OK;
}

static int cgGetResol(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  float x, y;
  getresol(&x, &y);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}

static int cgGetFrame(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  char resultstr[32];
  snprintf(resultstr, sizeof(resultstr), "%p", getframe());
  Tcl_AppendResult(interp, resultstr, NULL);
  return TCL_OK;
}

static int cgGetXScale(ClientData clientData, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj(getxscale()));
  return TCL_OK;
}

static int cgGetYScale(ClientData clientData, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj(getyscale()));
  return TCL_OK;
}

static int cgWindowToScreen(ClientData clientData, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  double x0, y0;
  int x, y;
  if (argc != 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " x y", (char *) NULL);
      return TCL_ERROR;
  }
  if (Tcl_GetDouble(interp, argv[1], &x0) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y0) != TCL_OK) return TCL_ERROR;
  window_to_screen(x0, y0, &x, &y);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(x));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(y));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}

static int cgScreenToWindow(ClientData clientData, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  int x0, y0;
  float x, y;
  if (argc != 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " x y", (char *) NULL);
      return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &x0) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &y0) != TCL_OK) return TCL_ERROR;
  screen_to_window(x0, y0, &x, &y);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}


static int cgPushViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  if (!strcmp(argv[0],"pushpviewport")) {
    double x0, y0, x1, y1;
    if (argc != 5 && argc != 1) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " [x0 y0 x1 y1]", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (argc == 5) {
      if (Tcl_GetDouble(interp, argv[1], &x0) != TCL_OK) return TCL_ERROR;
      if (Tcl_GetDouble(interp, argv[2], &y0) != TCL_OK) return TCL_ERROR;
      if (Tcl_GetDouble(interp, argv[3], &x1) != TCL_OK) return TCL_ERROR;
      if (Tcl_GetDouble(interp, argv[4], &y1) != TCL_OK) return TCL_ERROR;
    }
    pushviewport();
    if (argc == 5) setpviewport(x0, y0, x1, y1);
    return TCL_OK;
  }
  pushviewport();
  return TCL_OK;
}

static int cgPopViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  if (argc > 1) {
    while (popviewport());
    return TCL_OK;
  }
  if (popviewport()) return TCL_OK;
  else {
    Tcl_AppendResult(interp, argv[0], ": popped empty stack", NULL);
    return TCL_ERROR;
  }
}

static int cgSetViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " lx by rx ty", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  setviewport(x1, y1, x2, y2);
  return TCL_OK;
}


static int cgGetViewport(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  float x1, y1, x2, y2;

  if (argc != 1) {
    Tcl_AppendResult(interp, "usage: ", argv[0], NULL);
    return TCL_ERROR;
  }

  getviewport(&x1, &y1, &x2, &y2);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x1));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y1));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x2));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y2));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}

static int cgGetFViewport(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  float x1, y1, x2, y2;
  float w, h;

  if (argc != 1) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " getfviewport", NULL);
    return TCL_ERROR;
  }

  getviewport(&x1, &y1, &x2, &y2);
  getresol(&w, &h);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x1/w));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y1/h));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x2/w));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y2/h));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;  
}


static int cgGetWindow(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  float x1, y1, x2, y2;

  if (argc != 1) {
    Tcl_AppendResult(interp, "usage: ", argv[0], NULL);
    return TCL_ERROR;
  }

  getwindow(&x1, &y1, &x2, &y2);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x1));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y1));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x2));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y2));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}

static int cgGetAspect(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  float x1, y1, x2, y2;

  if (argc != 1) {
    Tcl_AppendResult(interp, "usage: ", argv[0], NULL);
    return TCL_ERROR;
  }

  getviewport(&x1, &y1, &x2, &y2);
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj((x2-x1)/(y2-y1)));
  return TCL_OK;
}

static int cgGetUAspect(ClientData clientData, Tcl_Interp *interp,
			int argc, char *argv[])
{
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj(getuaspect()));
  return TCL_OK;
}


static int cgSetFViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " lx by rx ty", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  setfviewport(x1, y1, x2, y2);
  return TCL_OK;
}

static int cgSetPViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " lx by rx ty", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  setpviewport(x1, y1, x2, y2);
  return TCL_OK;
}


static int cgSetWindow(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " top left bottom right",
    	(char *) NULL) ;
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  setwindow(x1, y1, x2, y2);
  return TCL_OK;
}

static int cgSetResol(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  double w, h;

  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " width height",
    	(char *) NULL) ;
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &w) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &h) != TCL_OK) return TCL_ERROR;
  
  setresol(w, h);
  return TCL_OK;
}

static int cgSetPSPageOri(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  static char *usage_message = "usage: setpageori {landscape|portrait}";

  if (argc != 2) {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }
  if (!strcmp(argv[1],"landscape") || !strcmp(argv[1],"LANDSCAPE"))
    gbSetPageOrientation(PS_LANDSCAPE);
  else if (!strcmp(argv[1],"portrait") || !strcmp(argv[1],"PORTRAIT"))
    gbSetPageOrientation(PS_PORTRAIT);
  else {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }
  return TCL_OK;
}

static int cgSetPSPageFill(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  static char *usage_message = "usage: setpagefill {0|1}";
  int status = 0;

  if (argc != 2) {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }
  
  if (Tcl_GetInt(interp, argv[1], &status) != TCL_OK) return TCL_ERROR;
  gbSetPageFill(status);

  return TCL_OK;
}

static int cgGsave(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[])
{
  gsave();
  return TCL_OK;
}

static int cgGrestore(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[])
{
  if (grestore()) return TCL_OK;
  else {
    Tcl_SetResult(interp, "grestore: popped empty stack", TCL_STATIC);
    return TCL_ERROR;
  }
}

static int cgGroup(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[])
{
  group();
  return TCL_OK;
}

static int cgUngroup(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[])
{
  ungroup();
  return TCL_OK;
}

static int cgFrame(ClientData clientData, Tcl_Interp *interp,
	    int argc, char *argv[])
{
  frame();
  return TCL_OK;
}

static int cgMoveto(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  double x, y;

  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  moveto(x, y);
  return TCL_OK;
}

static int cgLineto(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  double x, y;

  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  lineto(x, y);
  return TCL_OK;
}

static int cgDotAt(ClientData clientData, Tcl_Interp *interp,
	     int argc, char *argv[])
{
  double x, y;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  dotat(x, y);
  return TCL_OK;
}


static int cgSquare(ClientData clientData, Tcl_Interp *interp,
	     int argc, char *argv[])
{
  double x, y, scale = 3.0;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y {scale}", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  if (argc > 3) {
    if (Tcl_GetDouble(interp, argv[3], &scale) != TCL_OK) return TCL_ERROR;
  }

  square(x, y, scale);
  return TCL_OK;
}


static int cgFsquare(ClientData clientData, Tcl_Interp *interp,
	     int argc, char *argv[])
{
  double x, y, scale = 3.0;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y {scale}", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  if (argc > 3) {
    if (Tcl_GetDouble(interp, argv[3], &scale) != TCL_OK) return TCL_ERROR;
  }

  fsquare(x, y, scale);
  return TCL_OK;
}

static int cgPoly(ClientData clientData, Tcl_Interp *interp,
	    int argc, char *argv[])
{
  int i, n = argc-1;
  float *verts;
  double vert;
  
  if (n < 6 || n%2) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " x0 y0 x1 y1 x2 y2 [x3 y3 ... xn yn]", NULL);
    return TCL_ERROR;
  }
  
  verts = (float *) calloc(n, sizeof(float));
  
  for (i = 0; i < n; i++) {
    if (Tcl_GetDouble(interp, argv[i+1], &vert) != TCL_OK) return TCL_ERROR;
    verts[i] = vert;
  }
  
  polyline(n/2, verts);
  
  free(verts);
  return TCL_OK;
}

static int cgFpoly(ClientData clientData, Tcl_Interp *interp,
	    int argc, char *argv[])
{
  int i, n = argc-1;
  float *verts;
  double vert;
  
  if (n < 6 || n%2) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " x0 y0 x1 y1 x2 y2 [x3 y3 ... xn yn]", NULL);
    return TCL_ERROR;
  }
  
  verts = (float *) calloc(n, sizeof(float));
  
  for (i = 0; i < n; i++) {
    if (Tcl_GetDouble(interp, argv[i+1], &vert) != TCL_OK) return TCL_ERROR;
    verts[i] = vert;
  }
  
  filledpoly(n/2, verts);
  
  free(verts);
  return TCL_OK;
}


static int cgCircle(ClientData clientData, Tcl_Interp *interp,
	     int argc, char *argv[])
{
  double x, y, scale = 3.0;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y {scale}", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  if (argc > 3) {
    if (Tcl_GetDouble(interp, argv[3], &scale) != TCL_OK) return TCL_ERROR;
  }
  
  circle(x, y, scale);
  return TCL_OK;
}


static int cgFcircle(ClientData clientData, Tcl_Interp *interp,
	      int argc, char *argv[])
{
  double x, y, scale = 3.0;
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y {scale}", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  if (argc > 3) {
    if (Tcl_GetDouble(interp, argv[3], &scale) != TCL_OK) return TCL_ERROR;
  }

  fcircle(x, y, scale);
  return TCL_OK;
}


static int cgFilledRect(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " lx by rx ty", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  filledrect(x1, y1, x2, y2);
  return TCL_OK;
}

static int cgRect(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " lx by rx ty", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  rect(x1, y1, x2, y2);
  return TCL_OK;
}

static int cgSetorientation(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  int orient, oldori;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " {0|1}", NULL); 
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &orient) != TCL_OK) return TCL_ERROR;
  oldori = setorientation(orient);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldori));
  return TCL_OK;
}

static int cgSetjust(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  int just, oldjust;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " {-1|0|1}", NULL); 
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &just) != TCL_OK) return TCL_ERROR;
  oldjust = setjust(just);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldjust));
  return TCL_OK;
}

static int cgSetlstyle(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  int lstyle, oldlstyle;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " {0-8}", NULL); 
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &lstyle) != TCL_OK) return TCL_ERROR;
  oldlstyle = setlstyle(lstyle);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(lstyle));
  return TCL_OK;
}

static int cgSetlwidth(ClientData clientData, Tcl_Interp *interp,
	    int argc, char *argv[])
{
  int lwidth, oldlwidth;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " points*100", NULL); 
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &lwidth) != TCL_OK) return TCL_ERROR;
  oldlwidth = setlwidth(lwidth);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldlwidth));
  return TCL_OK;
}

static int cgRGBcolor(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  int colorval, r, g, b;
  if (argc != 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " r g b", NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &r) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &g) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &b) != TCL_OK) return TCL_ERROR;
  
  if (r > 255 || g > 255 || b > 255 || r < 0 || g < 0 || b < 0) {
    Tcl_AppendResult(interp, argv[0], ": color out of range", NULL);
    return TCL_ERROR;
  }
  
  /* 
   * This is a crazy color scheme, which allows 5 bits of color index
   * and another 24 bits above that for direct color specification
   */
  colorval = (r << 21) + (g << 13) + (b << 5);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(colorval));
  return TCL_OK;
}

static int cgGetcolor(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  int oldcolor = getcolor();
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldcolor));
  return TCL_OK;
}

static int cgSetcolor(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  int color, oldcolor;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " color", NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &color) != TCL_OK) return TCL_ERROR;
  oldcolor = setcolor(color);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldcolor));
  return TCL_OK;
}


static int cgSetfont(ClientData clientData, Tcl_Interp *interp,
		     int argc, char *argv[])
{
  double size;
  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " fontname pointsize", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[2], &size) != TCL_OK) return TCL_ERROR;
  setfont(argv[1], size);
  return TCL_OK;
}

static int cgSetsfont(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  double size, newsize;
  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " fontname pointsize", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[2], &size) != TCL_OK) return TCL_ERROR;
    
  newsize = setsfont(argv[1], size);
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj(newsize));
  return TCL_OK;
}

static int cgPostscript(ClientData clientData, Tcl_Interp *interp,
			int argc, char *argv[])
{
  double x, y;
  if (argc != 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " filename xscale yscale", NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetDouble(interp, argv[2], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &y) != TCL_OK) return TCL_ERROR;
  postscript(argv[1], x, y);
  return TCL_OK;
}


static int cgSetImagePreview(ClientData clientData, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  int val;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " 0|1", NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &val) != TCL_OK) return TCL_ERROR;
  Tcl_SetObjResult(interp, Tcl_NewIntObj(val));
  return TCL_OK;
}


static int cgDrawtext(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " text", NULL);
    return TCL_ERROR;
  }

  drawtext(argv[1]);
  return TCL_OK;
}

static int cgSetclip(ClientData clientData, Tcl_Interp *interp,
		     int argc, char *argv[])
{
  int clip;
  if (argc > 1) {
    if (Tcl_GetInt(interp, argv[1], &clip) != TCL_OK) return TCL_ERROR;
    setclip(clip);
  }
  Tcl_SetObjResult(interp, Tcl_NewIntObj(getclip()));
  return TCL_OK;
}


static int cgSetClipRegion(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " top left bottom right",
    	(char *) NULL) ;
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  setclipregion(x1, y1, x2, y2);
  return TCL_OK;
}


static int cgLYaxis(ClientData clientData, Tcl_Interp *interp,
		    int argc, char *argv[])
{
  double pos, tic;
  int interval;
  char *title = NULL;
  
  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     "xpos tick_interval label_interval [title]", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[1], &pos) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad xposition specified", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[2], &tic) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad tick interval specified", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetInt(interp, argv[3], &interval)) {
    Tcl_AppendResult(interp, argv[0], ": bad label interval specified", NULL);
    return TCL_ERROR;
  }
  
  if (argc > 4) title = argv[4];
  lyaxis(pos, tic, interval, title);
  return TCL_OK;
}

static int cgLXaxis(ClientData clientData, Tcl_Interp *interp,
		    int argc, char *argv[])
{
  double pos, tic;
  int interval;
  char *title = NULL;
  
  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     "xpos tick_interval label_interval [title]", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[1], &pos) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad yposition specified", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[2], &tic) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad tick interval specified", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetInt(interp, argv[3], &interval) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad label interval specified", NULL);
    return TCL_ERROR;
  }
  
  if (argc > 4) title = argv[4];
  lxaxis(pos, tic, interval, title);
  return TCL_OK;
}

CG_CMD_WRAPPER_IMPL(cgClearWindow)
CG_CMD_WRAPPER_IMPL(cgDumpWindow)
CG_CMD_WRAPPER_IMPL(cgPlayback)
CG_CMD_WRAPPER_IMPL(gbSizeCmd)
CG_CMD_WRAPPER_IMPL(gbIsEmptyCmd)
CG_CMD_WRAPPER_IMPL(cgGetResol)
CG_CMD_WRAPPER_IMPL(cgGetFrame)
CG_CMD_WRAPPER_IMPL(cgGetXScale)
CG_CMD_WRAPPER_IMPL(cgGetYScale)
CG_CMD_WRAPPER_IMPL(cgWindowToScreen)
CG_CMD_WRAPPER_IMPL(cgScreenToWindow)
CG_CMD_WRAPPER_IMPL(cgPushViewport)
CG_CMD_WRAPPER_IMPL(cgPopViewport)
CG_CMD_WRAPPER_IMPL(cgSetViewport)
CG_CMD_WRAPPER_IMPL(cgGetViewport)
CG_CMD_WRAPPER_IMPL(cgGetFViewport)
CG_CMD_WRAPPER_IMPL(cgGetWindow)
CG_CMD_WRAPPER_IMPL(cgGetAspect)
CG_CMD_WRAPPER_IMPL(cgGetUAspect)
CG_CMD_WRAPPER_IMPL(cgSetFViewport)
CG_CMD_WRAPPER_IMPL(cgSetPViewport)
CG_CMD_WRAPPER_IMPL(cgSetWindow)
CG_CMD_WRAPPER_IMPL(cgSetResol)
CG_CMD_WRAPPER_IMPL(cgSetPSPageOri)
CG_CMD_WRAPPER_IMPL(cgSetPSPageFill)
CG_CMD_WRAPPER_IMPL(cgGsave)
CG_CMD_WRAPPER_IMPL(cgGrestore)
CG_CMD_WRAPPER_IMPL(cgGroup)
CG_CMD_WRAPPER_IMPL(cgUngroup)
CG_CMD_WRAPPER_IMPL(cgFrame)
CG_CMD_WRAPPER_IMPL(cgMoveto)
CG_CMD_WRAPPER_IMPL(cgLineto)
CG_CMD_WRAPPER_IMPL(cgDotAt)
CG_CMD_WRAPPER_IMPL(cgSquare)
CG_CMD_WRAPPER_IMPL(cgFsquare)
CG_CMD_WRAPPER_IMPL(cgPoly)
CG_CMD_WRAPPER_IMPL(cgFpoly)
CG_CMD_WRAPPER_IMPL(cgCircle)
CG_CMD_WRAPPER_IMPL(cgFcircle)
CG_CMD_WRAPPER_IMPL(cgFilledRect)
CG_CMD_WRAPPER_IMPL(cgRect)
CG_CMD_WRAPPER_IMPL(cgSetorientation)
CG_CMD_WRAPPER_IMPL(cgSetjust)
CG_CMD_WRAPPER_IMPL(cgSetlstyle)
CG_CMD_WRAPPER_IMPL(cgSetlwidth)
CG_CMD_WRAPPER_IMPL(cgRGBcolor)
CG_CMD_WRAPPER_IMPL(cgGetcolor)
CG_CMD_WRAPPER_IMPL(cgSetcolor)
CG_CMD_WRAPPER_IMPL(cgSetfont)
CG_CMD_WRAPPER_IMPL(cgSetsfont)
CG_CMD_WRAPPER_IMPL(cgPostscript)
CG_CMD_WRAPPER_IMPL(cgSetImagePreview)
CG_CMD_WRAPPER_IMPL(cgDrawtext)
CG_CMD_WRAPPER_IMPL(cgSetclip)
CG_CMD_WRAPPER_IMPL(cgSetClipRegion)
CG_CMD_WRAPPER_IMPL(cgLYaxis)
CG_CMD_WRAPPER_IMPL(cgLXaxis)

int Cgbase_Init(Tcl_Interp *interp)
{
  if (
#ifdef USE_TCL_STUBS
      Tcl_InitStubs(interp, "8.6-", 0)
#else
      Tcl_PkgRequire(interp, "Tcl", "8.6-", 0)
#endif
      == NULL) {
    return TCL_ERROR;
  }
  
    /* Initialize the interpreter context for cgraph */
  Cgraph_InitInterp(interp);
  
  Tcl_CreateCommand(interp, "clearwin", (Tcl_CmdProc *) cgClearWindow,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getresol", (Tcl_CmdProc *) cgGetResol,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getxscale", (Tcl_CmdProc *) cgGetXScale,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getyscale", (Tcl_CmdProc *) cgGetYScale,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "getframe", (Tcl_CmdProc *) cgGetFrame,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "wintoscreen", (Tcl_CmdProc *) cgWindowToScreen,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "screentowin", (Tcl_CmdProc *) cgScreenToWindow,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "dumpwin", (Tcl_CmdProc *) cgDumpWindow,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "gbufplay", (Tcl_CmdProc *) cgPlayback,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "pushviewport", (Tcl_CmdProc *) cgPushViewport,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "pushpviewport", (Tcl_CmdProc *) cgPushViewport,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "popviewport", (Tcl_CmdProc *) cgPopViewport,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "poppviewport", (Tcl_CmdProc *) cgPopViewport,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setviewport", (Tcl_CmdProc *) cgSetViewport,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getviewport", (Tcl_CmdProc *) cgGetViewport,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getwindow", (Tcl_CmdProc *) cgGetWindow,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getfviewport", (Tcl_CmdProc *) cgGetFViewport,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getaspect", (Tcl_CmdProc *) cgGetAspect,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getuaspect", (Tcl_CmdProc *) cgGetUAspect,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setfviewport", (Tcl_CmdProc *) cgSetFViewport,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setpviewport", (Tcl_CmdProc *) cgSetPViewport,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setresol", (Tcl_CmdProc *) cgSetResol,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setwindow", (Tcl_CmdProc *) cgSetWindow,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setpageori", (Tcl_CmdProc *) cgSetPSPageOri,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setpagefill", (Tcl_CmdProc *) cgSetPSPageFill,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "postscript", (Tcl_CmdProc *)  cgPostscript,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setimgpreview", (Tcl_CmdProc *)  cgSetImagePreview,
		    
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "group", (Tcl_CmdProc *)  cgGroup,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "ungroup", (Tcl_CmdProc *) cgUngroup,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "gsave", (Tcl_CmdProc *)  cgGsave,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "grestore", (Tcl_CmdProc *) cgGrestore,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "cgframe", (Tcl_CmdProc *) cgFrame,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "moveto", (Tcl_CmdProc *) cgMoveto,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "lineto", (Tcl_CmdProc *) cgLineto,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "poly", (Tcl_CmdProc *) cgPoly,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "fpoly", (Tcl_CmdProc *) cgFpoly,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "fsquare", (Tcl_CmdProc *) cgFsquare,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "square", (Tcl_CmdProc *) cgSquare,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "fcircle", (Tcl_CmdProc *) cgFcircle,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "circle", (Tcl_CmdProc *) cgCircle,
		    (ClientData) NULL,
 		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "point", (Tcl_CmdProc *) cgDotAt,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "rect", (Tcl_CmdProc *) cgRect,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "filledrect", (Tcl_CmdProc *) cgFilledRect,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setfont", (Tcl_CmdProc *) cgSetfont,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setsfont", (Tcl_CmdProc *) cgSetsfont,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "drawtext", (Tcl_CmdProc *) cgDrawtext,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setjust", (Tcl_CmdProc *) cgSetjust,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setclip", (Tcl_CmdProc *) cgSetclip,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setclipregion", (Tcl_CmdProc *) cgSetClipRegion,
		    
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setorientation", (Tcl_CmdProc *) cgSetorientation,
		    
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setlstyle", (Tcl_CmdProc *) cgSetlstyle,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setlwidth", (Tcl_CmdProc *) cgSetlwidth,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setcolor", (Tcl_CmdProc *) cgSetcolor,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getcolor", (Tcl_CmdProc *) cgGetcolor,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "rgbcolor", (Tcl_CmdProc *) cgRGBcolor,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "lxaxis", (Tcl_CmdProc *) cgLXaxis,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "lyaxis", (Tcl_CmdProc *) cgLYaxis,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "gbufsize", (Tcl_CmdProc *) gbSizeCmd,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "gbufisempty", (Tcl_CmdProc *) gbIsEmptyCmd,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "gbufreset", (Tcl_CmdProc *) gbResetCmd,
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  


  return TCL_OK;
}
