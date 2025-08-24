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
  static char *usage = "usage: dumpwin {printer|ascii|raw|postscript|pdf|string|json}";
  if (argc > 2) outfile = argv[2];

  if (argc < 2) {
    interp->result = usage;
    return TCL_ERROR;
  }
  if (!strcmp(argv[1],"printer")) {
    gbPrintGevents();
    return TCL_OK;
  }  else if (!strcmp(argv[1],"raw")) {
    if (argc < 3) {
      interp->result = "usage: dumpwin raw filename";
      return TCL_ERROR;
    }
    gbWriteGevents(outfile,GBUF_RAW);
    return(TCL_OK);
  }
  else if (!strcmp(argv[1], "images")) {
    FILE *fp;
    if (argc < 3) {
      interp->result = "usage: dumpwin images filename";
      return TCL_ERROR;
    }
    fp = fopen(argv[2], "wb");
    if (!fp) {
      interp->result = "dumpwin: error opening gbimage file";
      return TCL_ERROR;
    }
    gbWriteImageFile(fp);
    fclose(fp);
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
  else {
    interp->result = usage;
    return TCL_ERROR;
  }
}

static int cgPlayback(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  FILE *fp;
  if (argc < 2) {
    interp->result = "usage: gbufplay filename [images]";
    return TCL_ERROR;
  }
  
  if (argc > 2) {
    if (!(fp = fopen(argv[2],"rb"))) {
      sprintf(interp->result,"unable to open file %s", argv[2]);
      return TCL_ERROR;
    }
    if (!gbReadImageFile(fp)) {
      sprintf(interp->result,"error opening image file %s", argv[2]);
      return TCL_ERROR;
    }
    fclose(fp);
  }
  if (!(fp = fopen(argv[1], "rb"))) {
    sprintf(interp->result,"unable to open file %s", argv[1]);
    return TCL_ERROR;
  }
  playback_gfile(fp);
  
  fclose(fp);
  
  return(TCL_OK);
}


static int gbSizeCmd(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  sprintf(interp->result,"%d", gbSize());
  return TCL_OK;
}

static int gbIsEmptyCmd(ClientData clientData, Tcl_Interp *interp,
		     int argc, char *argv[])
{
  sprintf(interp->result,"%d", gbIsEmpty());
  return TCL_OK;
}


static int cgGetResol(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  float x, y;
  getresol(&x, &y);
  sprintf(interp->result,"%4.0f %4.0f", x, y);
  return TCL_OK;
}

static int cgGetXScale(ClientData clientData, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  sprintf(interp->result,"%6.4f", getxscale());
  return TCL_OK;
}

static int cgGetYScale(ClientData clientData, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  sprintf(interp->result,"%6.4f", getyscale());
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
  sprintf(interp->result,"%d %d", x, y);
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
  sprintf(interp->result,"%g %g", x, y);
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
    interp->result = "popviewport: popped empty stack";
    return TCL_ERROR;
  }
}

static int cgSetViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  double x1, y1, x2, y2;

  if (argc != 5) {
    interp->result = "usage: setviewport lx by rx ty";
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
    interp->result = "usage: getviewport";
    return TCL_ERROR;
  }

  getviewport(&x1, &y1, &x2, &y2);
  sprintf(interp->result,"%f %f %f %f", x1, y1, x2, y2);
  return TCL_OK;
}

static int cgGetFViewport(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  float x1, y1, x2, y2;
  float w, h;

  if (argc != 1) {
    interp->result = "usage: getfviewport";
    return TCL_ERROR;
  }

  getviewport(&x1, &y1, &x2, &y2);
  getresol(&w, &h);
  sprintf(interp->result,"%f %f %f %f", x1/w, y1/h, x2/w, y2/h);
  return TCL_OK;
}


static int cgGetWindow(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  float x1, y1, x2, y2;

  if (argc != 1) {
    interp->result = "usage: getwindow";
    return TCL_ERROR;
  }

  getwindow(&x1, &y1, &x2, &y2);
  sprintf(interp->result,"%f %f %f %f", x1, y1, x2, y2);
  return TCL_OK;
}

static int cgGetFrame(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  sprintf(interp->result,"%x", getframe());
  return TCL_OK;
}

static int cgGetAspect(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  float x1, y1, x2, y2;

  if (argc != 1) {
    interp->result = "usage: getfviewport";
    return TCL_ERROR;
  }

  getviewport(&x1, &y1, &x2, &y2);
  sprintf(interp->result,"%f", (x2-x1)/(y2-y1));
  return TCL_OK;
}

static int cgGetUAspect(ClientData clientData, Tcl_Interp *interp,
			int argc, char *argv[])
{
  sprintf(interp->result,"%f", getuaspect());
  return TCL_OK;
}


static int cgSetFViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  double x1, y1, x2, y2;

  if (argc != 5) {
    interp->result = "usage: setfviewport lx by rx ty";
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
    interp->result = "usage: setpviewport lx by rx ty";
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

static int cgSetPSPageOri(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  static char *usage_message = "usage: setpageori {landscape|portrait}";

  if (argc != 2) {
    interp->result = usage_message;
    return TCL_ERROR;
  }
  if (!strcmp(argv[1],"landscape") || !strcmp(argv[1],"LANDSCAPE"))
    gbSetPageOrientation(PS_LANDSCAPE);
  else if (!strcmp(argv[1],"portrait") || !strcmp(argv[1],"PORTRAIT"))
    gbSetPageOrientation(PS_PORTRAIT);
  else {
    interp->result = usage_message;
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
    interp->result = usage_message;
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
    interp->result = "grestore: popped empty stack";
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
    interp->result = "usage: moveto x y";
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
    interp->result = "usage: lineto x y";
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
    interp->result = "usage: point x y";
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
    interp->result = "usage: square x y {scale}";
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
    interp->result = "usage: fsquare x y {scale}";
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


static int cgFpoly(ClientData clientData, Tcl_Interp *interp,
	    int argc, char *argv[])
{
  int i, n = argc-1;
  float *verts;
  double vert;
  
  if (n < 6 || n%2) {
    interp->result = "usage: fpoly x0 y0 x1 y1 x2 y2 [x3 y3 ... xn yn]";
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

static int cgPoly(ClientData clientData, Tcl_Interp *interp,
	    int argc, char *argv[])
{
  int i, n = argc-1;
  float *verts;
  double vert;
  
  if (n < 4 || n%2) {
    interp->result = "usage: poly x0 y0 x1 y1 [x2 y2 x3 y3 ... xn yn]";
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


static int cgCircle(ClientData clientData, Tcl_Interp *interp,
	     int argc, char *argv[])
{
  double x, y, scale = 3.0;

  if (argc < 3) {
    interp->result = "usage: circle x y {scale}";
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
    interp->result = "usage: fcircle x y {scale}";
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
    interp->result = "usage: filledrect lx by rx ty";
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
    interp->result = "usage: rect lx by rx ty";
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
    interp->result = "usage: setorientation {0|1}";
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &orient) != TCL_OK) return TCL_ERROR;
  oldori = setorientation(orient);
  sprintf(interp->result,"%d", oldori);
  return TCL_OK;
}

static int cgSetjust(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  int just, oldjust;
  if (argc != 2) {
    interp->result = "usage: setjust {-1|0|1}";
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &just) != TCL_OK) return TCL_ERROR;
  oldjust = setjust(just);
  sprintf(interp->result,"%d", oldjust);
  return TCL_OK;
}

static int cgSetlstyle(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  int lstyle, oldlstyle;
  if (argc != 2) {
    interp->result = "usage: setlstyle {0-8}";
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &lstyle) != TCL_OK) return TCL_ERROR;
  oldlstyle = setlstyle(lstyle);
  sprintf(interp->result,"%d", oldlstyle);
  return TCL_OK;
}

static int cgSetlwidth(ClientData clientData, Tcl_Interp *interp,
	    int argc, char *argv[])
{
  int lwidth, oldlwidth;
  if (argc != 2) {
    interp->result = "usage: setlwidth points*100";
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &lwidth) != TCL_OK) return TCL_ERROR;
  oldlwidth = setlwidth(lwidth);
  sprintf(interp->result,"%d", oldlwidth);
  return TCL_OK;
}

static int cgRGBcolor(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  int colorval, r, g, b;
  if (argc != 4) {
    interp->result = "usage: rgbcolor r g b";
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &r) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &g) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &b) != TCL_OK) return TCL_ERROR;
  
  if (r > 255 || g > 255 || b > 255 || r < 0 || g < 0 || b < 0) {
    interp->result = "rgbcolor: color out of range";
    return TCL_ERROR;
  }
  
  /* 
   * This is a crazy color scheme, which allows 5 bits of color index
   * and another 24 bits above that for direct color specification
   */
  colorval = (r << 21) + (g << 13) + (b << 5);
  sprintf(interp->result,"%d", colorval);  
  return TCL_OK;
}

static int cgSetcolor(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  int color, oldcolor;
  if (argc != 2) {
    interp->result = "usage: setcolor color";
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &color) != TCL_OK) return TCL_ERROR;
  oldcolor = setcolor(color);
  sprintf(interp->result,"%d", oldcolor);
  return TCL_OK;
}


static int cgSetfont(ClientData clientData, Tcl_Interp *interp,
		     int argc, char *argv[])
{
  double size;
  if (argc != 3) {
    interp->result = "usage: setfont fontname pointsize";
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
    interp->result = "usage: setsfont fontname pointsize";
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[2], &size) != TCL_OK) return TCL_ERROR;
    
  newsize = setsfont(argv[1], size);
  sprintf(interp->result,"%f",newsize);
  return TCL_OK;
}

static int cgPostscript(ClientData clientData, Tcl_Interp *interp,
			int argc, char *argv[])
{
  double x, y;
  if (argc != 4) {
    interp->result = "usage: postcript filename xscale yscale";
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
    interp->result = "usage: setimgpreview 0|1";
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &val) != TCL_OK) return TCL_ERROR;
  sprintf(interp->result, "%d", setimgpreview(val));
  return TCL_OK;
}

static int cgDrawtext(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  if (argc != 2) {
    interp->result = "usage: drawtextf text";
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
  sprintf(interp->result, "%d", getclip());
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
    interp->result = "usage: lyaxis xpos tick_interval label_interval [title]";
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[1], &pos) != TCL_OK) {
    interp->result = "lyaxis: bad xposition specified";
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[2], &tic) != TCL_OK) {
    interp->result = "lyaxis: bad tick interval specified";
    return TCL_ERROR;
  }
  
  if (Tcl_GetInt(interp, argv[3], &interval)) {
    interp->result = "lyaxis: bad label interval specified";
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
    interp->result = "usage: lxaxis ypos tick_interval label_interval [title]";
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[1], &pos) != TCL_OK) {
    interp->result = "lxaxis: bad yposition specified";
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[2], &tic) != TCL_OK) {
    interp->result = "lxaxis: bad tick interval specified";
    return TCL_ERROR;
  }
  
  if (Tcl_GetInt(interp, argv[3], &interval) != TCL_OK) {
    interp->result = "lxaxis: bad label interval specified";
    return TCL_ERROR;
  }
  
  if (argc > 4) title = argv[4];
  lxaxis(pos, tic, interval, title);
  return TCL_OK;
}


int Cgbase_Init(Tcl_Interp *interp)
{
  Tcl_CreateCommand(interp, "clearwin", cgClearWindow, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getresol", cgGetResol,  (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getxscale", cgGetXScale,  (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getyscale", cgGetYScale,  (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "wintoscreen", cgWindowToScreen, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "screentowin", cgScreenToWindow, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "dumpwin", cgDumpWindow, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "gbufplay", cgPlayback, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "pushviewport", cgPushViewport, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "pushpviewport", cgPushViewport, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "popviewport", cgPopViewport, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "poppviewport", cgPopViewport, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setviewport", cgSetViewport, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getviewport", cgGetViewport, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getwindow", cgGetWindow, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getframe", cgGetFrame, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getfviewport", cgGetFViewport, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getaspect", cgGetAspect, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "getuaspect", cgGetUAspect, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setfviewport", cgSetFViewport, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setpviewport", cgSetPViewport, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setwindow", cgSetWindow, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setpageori", cgSetPSPageOri, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setpagefill", cgSetPSPageFill, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "postscript",  cgPostscript, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setimgpreview",  cgSetImagePreview, 
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "group",  cgGroup, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "ungroup", cgUngroup, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "gsave",  cgGsave, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "grestore", cgGrestore, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "cgframe", cgFrame, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "moveto", cgMoveto, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "lineto", cgLineto, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "poly", cgPoly, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "fpoly", cgFpoly, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "fsquare", cgFsquare, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "square", cgSquare, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "fcircle", cgFcircle, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "circle", cgCircle, (ClientData) NULL,
 		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "point", cgDotAt, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "rect", cgRect, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "filledrect", cgFilledRect, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setfont", cgSetfont, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setsfont", cgSetsfont, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "drawtext", cgDrawtext, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setjust", cgSetjust, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setclip", cgSetclip, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setclipregion", cgSetClipRegion, 
		    (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setorientation", cgSetorientation, 
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setlstyle", cgSetlstyle, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setlwidth", cgSetlwidth, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "setcolor", cgSetcolor, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "rgbcolor", cgRGBcolor, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "lxaxis", cgLXaxis, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "lyaxis", cgLYaxis, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "gbufsize", gbSizeCmd, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "gbufisempty", gbIsEmptyCmd, (ClientData) NULL,
		    (Tcl_CmdDeleteProc *) NULL);


  return TCL_OK;
}
