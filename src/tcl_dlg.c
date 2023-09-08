/*****************************************************************************
 *
 * NAME
 *      tcl_dlg.c
 *
 * PURPOSE
 *      Provide code for using dynLists for graphics
 *
 * AUTHOR
 *      DLS, JUL-95...DEC-16
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef WIN32
#include <unistd.h>
//#include <unixvid.h>
#endif
#include <cgraph.h>

#include <tcl.h>
#include <df.h>
#include "dfana.h"
#include "tcl_dl.h"

#include <rawapi.h>

typedef int (*TCL_FUNCTION)(ClientData, Tcl_Interp *, int, char **);
typedef struct {
  char *name;
  TCL_FUNCTION func;
  ClientData cd;
  char *desc;
} TCL_COMMANDS;

enum DLG_RETVALS    { DLG_OK, DLG_NOWINDOW, DLG_BADARGS, DLG_ARGMISMATCH,
		      DLG_NOMEMORY, DLG_ZEROLIST, DLG_BADCOLORSPEC };
enum DLG_MARKERS    { DLG_SQUARE, DLG_FSQUARE, DLG_CIRC, DLG_FCIRC,
			DLG_HTICK, DLG_VTICK, DLG_PLUS, DLG_HTICK_L,
		        DLG_HTICK_R, DLG_VTICK_U, DLG_VTICK_D, DLG_TRIANGLE,
                        DLG_DIAMOND };
enum DLG_SCALETYPES { DLG_UNIT_SCALE, DLG_X_SCALE, DLG_WINDOW_SCALE,
                      DLG_WINDOW_XSCALE, DLG_WINDOW_YSCALE};
enum DLG_RGBFUNCS { DLG_SINGLE_COLOR, DLG_MULTI_COLORS };

/*
 * Data structures used for drawing markers and lines
 */

typedef void (*MARKER_FUNC)(float, float, float);
     
typedef struct _mfuncinfo {
  char *name;
  MARKER_FUNC mfunc;
  int id;
  int color;
  int lwidth;
  float msize;
  int clip;
} MARKER_INFO;

static const char  *defaultMarkerType = "SQUARE";

struct _lfuncinfo;
typedef void (*LINE_FUNC)(int n, float *x, float *y, struct _lfuncinfo *);
     
typedef struct _lfuncinfo {
  char *name;
  LINE_FUNC lfunc;
  int id;
  int lstyle;
  int lwidth;
  int filled;
  int linecolor;
  int fillcolor;
  int sideways;
  int skip;
  int boxfilter;
  int clip;
  int closed;
  float fparams[4];		/* place for function specific params */
  DYN_LIST *linecolors;
  DYN_LIST *fillcolors;
} LINE_INFO;

struct _tfuncinfo;
typedef void (*TEXT_FUNC)(int n, float *x, float *y, void *s, 
			  struct _tfuncinfo *);
     
typedef struct _tfuncinfo {
  char *name;
  TEXT_FUNC tfunc;
  int id;
  char *font;
  float size;
  int just;
  int ori;
  int color;
  char *format;
  float spacing;		/* for multiline text strings */
  DYN_LIST *colors;		/* must be ints representing colors */
} TEXT_INFO;



static float heatmap_jet_r[] = { 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 15.937500, 31.875000, 47.812500, 63.750000, 79.687500, 95.625000, 111.562500, 127.500000, 143.437500, 159.375000, 175.312500, 191.250000, 207.187500, 223.125000, 239.062500, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 239.062500, 223.125000, 207.187500, 191.250000, 175.312500, 159.375000, 143.437500, 127.500000};

static float heatmap_jet_g[] = { 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 15.937500, 31.875000, 47.812500, 63.750000, 79.687500, 95.625000, 111.562500, 127.500000, 143.437500, 159.375000, 175.312500, 191.250000, 207.187500, 223.125000, 239.062500, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 239.062500, 223.125000, 207.187500, 191.250000, 175.312500, 159.375000, 143.437500, 127.500000, 111.562500, 95.625000, 79.687500, 63.750000, 47.812500, 31.875000, 15.937500, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000};
    
static float heatmap_jet_b[] = { 143.437500, 159.375000, 175.312500, 191.250000, 207.187500, 223.125000, 239.062500, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 255.000000, 239.062500, 223.125000, 207.187500, 191.250000, 175.312500, 159.375000, 143.437500, 127.500000, 111.562500, 95.625000, 79.687500, 63.750000, 47.812500, 31.875000, 15.937500, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000 };

static float heatmap_hot_r[] = { 10.6250, 21.2500, 31.8750, 42.5000, 53.1250, 63.7500, 74.3750, 85.0000, 95.6250, 106.2500, 116.8750, 127.5000, 138.1250, 148.7500, 159.3750, 170.0000, 180.6250, 191.2500, 201.8750, 212.5000, 223.1250, 233.7500, 244.3750, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000 }; 
	    
static float heatmap_hot_g[] = { 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 10.6250, 21.2500, 31.8750, 42.5000, 53.1250, 63.7500, 74.3750, 85.0000, 95.6250, 106.2500, 116.8750, 127.5000, 138.1250, 148.7500, 159.3750, 170.0000, 180.6250, 191.2500, 201.8750, 212.5000, 223.1250, 233.7500, 244.3750, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000, 255.0000 };
	    
static float heatmap_hot_b[] = { 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 15.9375, 31.8750, 47.8125, 63.7500, 79.6875, 95.6250, 111.5625, 127.5000, 143.4375, 159.3750, 175.3125, 191.2500, 207.1875, 223.1250, 239.0625, 255.0000 };


/*****************************************************************************
 *                             Local Functions 
 *****************************************************************************/

int dlgDrawMarkers(DYN_LIST *dlx, DYN_LIST *dly, MARKER_INFO *minfo);
int dlgDrawMultiMarkers(DYN_LIST *dlx, DYN_LIST *dly, 
			DYN_LIST *sizes, float scale, 
			DYN_LIST *colors, MARKER_INFO *minfo);

static int dlgGetMarkerInfo(char *tagname, MARKER_INFO *minfo);

int dlgDrawLines(DYN_LIST *dlx, DYN_LIST *dly, LINE_INFO *linfo, int idx);
static int dlgLine(int n, float *x, float *y, LINE_INFO *linfo);
static int dlgBar(int n, float *x, float *y, LINE_INFO *linfo);
static int dlgStep(int n, float *x, float *y, LINE_INFO *linfo);
static int dlgDisjointLines(int n, float *x, float *y, LINE_INFO *linfo);
static int dlgBezier(int n, float *x, float *y, LINE_INFO *linfo);
static int dlgGetLineInfo(int i, LINE_INFO *linfo);

int dlgDrawText(DYN_LIST *dlx, DYN_LIST *dly, DYN_LIST *data,
		TEXT_INFO *tinfo);
static int dlgTextInt(int n, float *x, float *y, int *ints, 
		      TEXT_INFO *tinfo);
static int dlgTextFloat(int n, float *x, float *y, float *floats, 
		      TEXT_INFO *tinfo);
static int dlgTextString(int n, float *x, float *y, char **strings, 
			 TEXT_INFO *tinfo);
static int dlgGetTextInfo(int i, TEXT_INFO *tinfo);

static int getPSBB(Tcl_Interp *interp, char *filename, 
		   double *bblx, double *bbly, double *bbux, double *bbuy);

static float nicenum(float x, int round);


/*****************************************************************************
 *                             Local Tables
 *****************************************************************************/


static MARKER_INFO MarkerTable[] = {
  { "SQUARE",    (MARKER_FUNC) square ,   DLG_SQUARE,    -1, -1, 5.0, -1 },
  { "FSQUARE",   (MARKER_FUNC) fsquare,   DLG_FSQUARE,   -1, -1, 5.0, -1 },
  { "CIRCLE",    (MARKER_FUNC) circle,    DLG_CIRC,      -1, -1, 5.0, -1 },
  { "FCIRCLE",   (MARKER_FUNC) fcircle,   DLG_FCIRC,     -1, -1, 5.0, -1 },
  { "HTICK",     (MARKER_FUNC) htick,     DLG_HTICK,     -1, -1, 5.0, -1 },
  { "HTICK_L",   (MARKER_FUNC) htick_left,DLG_HTICK_L,   -1, -1, 5.0, -1 },
  { "HTICK_R",   (MARKER_FUNC) htick_right,DLG_HTICK_R,  -1, -1, 5.0, -1 },
  { "VTICK",     (MARKER_FUNC) vtick,     DLG_VTICK,     -1, -1, 5.0, -1 },
  { "VTICK_U",   (MARKER_FUNC) vtick_up,  DLG_VTICK_U,   -1, -1, 5.0, -1 },
  { "VTICK_D",   (MARKER_FUNC) vtick_down,DLG_VTICK_D,   -1, -1, 5.0, -1 },
  { "PLUS",      (MARKER_FUNC) plus,      DLG_PLUS,      -1, -1, 5.0, -1 },
  { "TRIANGLE",  (MARKER_FUNC) triangle,  DLG_TRIANGLE,  -1, -1, 5.0, -1 },
  { "DIAMOND",   (MARKER_FUNC) diamond,   DLG_DIAMOND,  -1, -1, 5.0, -1 }
};


enum DLG_LINEFUNCS  { DLG_LINE, DLG_BAR, DLG_STEP, DLG_DISJOINT, DLG_BEZIER };
static LINE_INFO LineTable[] = {                             /* yoff */
  { "LINE", (LINE_FUNC) dlgLine , DLG_LINE, 0, 50, 0, -1, -1, 0, 1, -1, -1, 0,
    { 0.0, 0.0, 0.0, 0.0 }, NULL, NULL },
  { "BAR",  (LINE_FUNC) dlgBar,   DLG_BAR,  0, 50, 0, -1, -1, 0, 1, -1, -1, 0,
    { 0.0, 0.7, 0.0, 0.0 }, NULL, NULL },
  { "STEP", (LINE_FUNC) dlgStep,  DLG_STEP, 0, 50, 0, -1, -1, 0, 1, -1, -1, 0,
    { 0.0, 0.0, 0.0, 0.0 }, NULL, NULL },
  { "DISJOINT", (LINE_FUNC) dlgDisjointLines,
    DLG_DISJOINT,                         0, 50, 0, -1, -1, 0, 1, -1, -1, 0,
    { 0.0, 0.0, 0.0, 0.0 }, NULL, NULL },
  { "BEZIER", (LINE_FUNC) dlgBezier,DLG_BEZIER, 0, 50, 0, -1, -1, 0, 1, -1, -1, 0,
    { 0.0, 0.0, 0.0, 0.0 }, NULL, NULL }
};

enum DLG_TEXTFUNCS  { DLG_INTS, DLG_STRINGS, DLG_FLOATS };
static TEXT_INFO TextTable[] = {
  { "INTS",    (TEXT_FUNC) dlgTextInt    , DLG_INTS, 
      "Helvetica", 10.0, 0, 0, -1, "%d", 1.0, NULL },
  { "STRINGS", (TEXT_FUNC) dlgTextString , DLG_STRINGS, 
      "Helvetica", 10.0, 0, 0, -1, "%s", 1.0, NULL },
  { "FLOATS",  (TEXT_FUNC) dlgTextFloat ,  DLG_FLOATS, 
      "Helvetica", 10.0, 0, 0, -1, "%6.2f", 1.0, NULL },
};

/*****************************************************************************
 *                           TCL Bound Functions 
 *****************************************************************************/

static int tclMarkerDynList           (ClientData, Tcl_Interp *, int, char **);
static int tclLineDynList             (ClientData, Tcl_Interp *, int, char **);
static int tclTextDynList             (ClientData, Tcl_Interp *, int, char **);
static int tclPostscriptDynList       (ClientData, Tcl_Interp *, int, char **);
static int tclImagePlace              (ClientData, Tcl_Interp *, int, char **);
static int tclImageReplace            (ClientData, Tcl_Interp *, int, char **);
static int tclLabels                  (ClientData, Tcl_Interp *, int, char **);
static int tclNiceDpoints             (ClientData, Tcl_Interp *, int, char **);
static int tclNiceNum                 (ClientData, Tcl_Interp *, int, char **);
static int tclRGBColors               (ClientData, Tcl_Interp *, int, char **);
static int tclRGB2Hex                 (ClientData, Tcl_Interp *, int, char **);
static int tclPolarLAB2RGBColors      (ClientData, Tcl_Interp *, int, char **);
static int tclHeatmap                 (ClientData, Tcl_Interp *, int, char **);
static int tclDLGHelp                 (ClientData, Tcl_Interp *, int, char **);
				       
static TCL_COMMANDS DLGcommands[] = {
  { "dlg_markers",          tclMarkerDynList,     NULL, 
      "draw markers at positions designated by dynList" },

  { "dlg_lines",            tclLineDynList,       (void *) DLG_LINE,
      "draw lines" },
  { "dlg_disjointLines",    tclLineDynList,       (void *) DLG_DISJOINT,
      "draw disjoint lines" },
  { "dlg_bars",             tclLineDynList,       (void *) DLG_BAR,
      "draw bars" },
  { "dlg_steps",            tclLineDynList,       (void *) DLG_STEP,
      "draw steps" },
  { "dlg_bezier",           tclLineDynList,       (void *) DLG_BEZIER,
      "draw bezier curves" },

  { "dlg_text",             tclTextDynList,       NULL,
      "draw text at x, y positions" },

  { "dlg_postscript",       tclPostscriptDynList, NULL,
      "place ps file" },
  { "dlg_image",            tclImagePlace, NULL,
    "place image" },
  { "dlg_replaceImage",     tclImageReplace, NULL,
    "replace image data" },
  
  { "dlg_loose_label",      tclLabels, (void *) 1,
      "pick label positions \"loosely\"" },
  { "dlg_tight_label",      tclLabels, (void *) 0,
      "pick label positions without going over/under" },
  { "dlg_nicenum",          tclNiceNum, NULL,
      "choose a \"nice\" number for graph purposes" },
  { "dlg_nice_dpoints",     tclNiceDpoints, NULL,
      "choose appropriate number of decimal points" },

  { "dlg_rgbcolors",        tclRGBColors, (void *) DLG_MULTI_COLORS,
      "generate color indices from RGB color specs" },
  { "dlg_rgbcolor",         tclRGBColors, (void *) DLG_SINGLE_COLOR ,
      "generate single color from r g b vals" },

  { "dlg_rgb2hex",   tclRGB2Hex, NULL,
      "turn list of R G B values to Tcl #RRGGBB hex format" },

  { "dlg_polarlabcolors",   tclPolarLAB2RGBColors, (void *) DLG_MULTI_COLORS,
      "generate color indices from polar LAB (LCH) color specs" },
  { "dlg_polarlabcolor",    tclPolarLAB2RGBColors, (void *) DLG_SINGLE_COLOR ,
      "generate single color from L C H (deg) vals" },

  { "dlg_heatmap",           tclHeatmap, NULL, 
      "return heatmap by name (jet, hot)" },
  
  { "dlg_help",             tclDLGHelp,          (void *) DLGcommands, 
      "display help" },
  { NULL, NULL, NULL, NULL }
};




/*****************************************************************************
 *
 * FUNCTION
 *    Dlg_Init
 *
 * ARGS
 *    Tcl_Inter *interp
 *
 * DESCRIPTION
 *    Creates tcl commands for drawing with dynLists & dynGroups.
 *
 *****************************************************************************/

int Dlg_Init(Tcl_Interp *interp)
{
  int i = 0;

  while (DLGcommands[i].name) {
    Tcl_CreateCommand(interp, DLGcommands[i].name, 
		      (Tcl_CmdProc *) DLGcommands[i].func, 
		      (ClientData) DLGcommands[i].cd, 
		      (Tcl_CmdDeleteProc *) NULL);
    i++;
  }

  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclDLGHelp
 *
 * ARGS
 *    Tcl Args
 *
 * DESCRIPTION
 *    Loop through DLGCommands and print out help messages
 *
 *****************************************************************************/

static int tclDLGHelp (ClientData cd, Tcl_Interp *interp, 
		       int argc, char *argv[])
{
  TCL_COMMANDS *commands = (TCL_COMMANDS *) cd;
  int i = 0;

  while (commands[i].name) {
    printf("%-22s - %s\n", commands[i].name, commands[i].desc);
    i++;
  }
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclMarkerDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_marker
 *
 * DESCRIPTION
 *    Draws markers at positions specified by list arguments.
 *
 *****************************************************************************/

static int tclMarkerDynList (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  DYN_LIST *dlx, *dly, *dltx = NULL, *dlty = NULL;
  DYN_LIST *sizes = NULL, *colors = NULL;	/* for multi markers */
  int scaletype = DLG_UNIT_SCALE;
  MARKER_INFO minfo;
  char markerName[64] = "";
  double msize = -1, val;
  int color = -1;
  int lwidth = -1;
  int clip = -1;
  int status, i, j;

  /* This is a nasty command parsing loop, looking for option pairs */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-' &&
	argv[i][1] != '.' &&
	(argv[i][1] < '0' || argv[i][1] > '9')) {
      if (!strcmp(argv[i],"-size")) {
	int l;
	float scale = 1.0;
	char numeric[32];
	memset(numeric, 0, 32);

	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no size arg", (char *) NULL);
	  goto error;
	}
	l = strlen(argv[i+1])-1;

	if (argv[i+1][l] == 's' || argv[i+1][l] == 'S') {
	  scale = getxscale();
	}
	else if (argv[i+1][l] == 'w' || argv[i+1][l] == 'W' ||
		 argv[i+1][l] == 'x' || argv[i+1][l] == 'X') {
	  float xres, yres, xul, yub, xur, yut;
	  getresol(&xres, &yres);
	  getwindow(&xul, &yub, &xur, &yut);
	  /* Make the marker dims with respect to x axis */
	  scale = xres/fabs(xur-xul) * getxscale();
	}
	else if (argv[i+1][l] == 'y' || argv[i+1][l] == 'Y') {
	  float xres, yres, xul, yub, xur, yut;
	  getresol(&xres, &yres);
	  getwindow(&xul, &yub, &xur, &yut);
	  /* Make the marker dims with respect to y axis */
	  scale = yres/fabs(yut-yub) * getyscale();
	}
	else {
	  l = strlen(argv[i+1]);	/* no modifier */
	}
	strncpy(numeric, argv[i+1], l < 31 ? l : 31);
	
	if (Tcl_GetDouble(interp, numeric, &msize) != TCL_OK)
	  goto error;
	
	msize *= scale;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-sizes")) {
	if (tclFindDynList(interp, argv[i+1], &sizes) != TCL_OK)
	  goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-colors")) {
	if (tclFindDynList(interp, argv[i+1], &colors) != TCL_OK)
	  goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strncmp(argv[i], "-scale", 6)) {
	if (argv[i+1][0] == 's' || argv[i+1][0] == 'S') {
	  scaletype = DLG_X_SCALE;
	}
	else if (argv[i+1][0] == 'w' || argv[i+1][0] == 'W') {
	  scaletype = DLG_WINDOW_SCALE;
	}
	else if (argv[i+1][0] == 'u' || argv[i+1][0] == 'U') {
	  scaletype = DLG_UNIT_SCALE;
	}
	else if (argv[i+1][0] == 'x' || argv[i+1][0] == 'X') {
	  scaletype = DLG_WINDOW_XSCALE;
	}
	else if (argv[i+1][0] == 'y' || argv[i+1][0] == 'Y') {
	  scaletype = DLG_WINDOW_YSCALE;
	}
	else {
	  Tcl_AppendResult(interp, argv[0], 
		   ": scaletype must be either 'x', 'y', 'u', 's' or 'w'", 
			   (char *) NULL);
	  goto error;
	}
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-color")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no color specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &color) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-lwidth")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no lwidth specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &lwidth) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-clip")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": clipping not specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &clip) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-marker")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no marker specified", (char *) NULL);
	  goto error;
	}
	strncpy(markerName, argv[i+1], 63);
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else {
	/* Could be a negative number, so try to parse as double */
	double val;
	if (Tcl_GetDouble(interp, argv[i], &val) != TCL_OK) {
	  Tcl_ResetResult(interp);
	  Tcl_AppendResult(interp, argv[0], 
			   ": bad option ", argv[i], (char *) NULL);
	  goto error;
	}
      }
    }
  }


  if (argc < 3) {
    Tcl_AppendResult(interp, "usage:\t", argv[0], 
		     " xlist ylist [marker [size]]\n",
		     "options:   -marker, -size/-sizes, -color, -lwidth, -clip, -scaletype (x,y,s,u,w)",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  tclFindDynList(interp, argv[1], &dlx);
  tclFindDynList(interp, argv[2], &dly);
  
  if (!dlx && !dly) {		/* both scalars? */
    if (Tcl_GetDouble(interp, argv[1], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": xvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dlx = dfuCreateDynList(DF_FLOAT, 1);
    dfuAddDynListFloat(dlx, val);
    dltx = dlx;

    if (Tcl_GetDouble(interp, argv[2], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": yvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dly = dfuCreateDynList(DF_FLOAT, 1);
    dfuAddDynListFloat(dly, val);
    dlty = dly;
    Tcl_ResetResult(interp);
  }
  else if (!dlx) {
    if (Tcl_GetDouble(interp, argv[1], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": xvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dlx = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dly));
    dfuAddDynListFloat(dlx, val);
    dltx = dlx;
    Tcl_ResetResult(interp);
  }
  else if (!dly) {
    if (Tcl_GetDouble(interp, argv[2], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": yvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dly = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dlx));
    dfuAddDynListFloat(dly, val);
    dlty = dly;
    Tcl_ResetResult(interp);
  }

  if (argc < 4 && !strcmp(markerName,"")) {
    strcpy(markerName, defaultMarkerType);
  }
  else if (argc >= 4) {
    strcpy(markerName, argv[3]);
  }

  if (!dlgGetMarkerInfo(markerName, &minfo)) {
    Tcl_DString markernames;
    int i;
    int nmarkers = sizeof(MarkerTable)/sizeof(MARKER_INFO);
    Tcl_DStringInit(&markernames);
    
    Tcl_DStringAppend(&markernames, "Markers must be one of:\n\t", -1);
    for (i = 0; i < nmarkers; i++) {
      if (i > 0 && i % 5 == 0) Tcl_DStringAppend(&markernames, "\n\t", -1);
      else if (i) Tcl_DStringAppend(&markernames, " ", -1);
      Tcl_DStringAppend(&markernames, MarkerTable[i].name, -1);
    }
    Tcl_DStringResult(interp, &markernames);
    goto error;
  }

  /* These may have been specified on the command line */
  if (lwidth != -1) minfo.lwidth = lwidth;
  if (color != -1) minfo.color = color;
  if (clip != -1) minfo.clip = clip;
  if (msize != -1.0) minfo.msize = msize;
  
  if (argc > 4) {
    int l = strlen(argv[4])-1;
    float scale = 1.0;
    if (argv[4][l] == 's' || argv[4][l] == 'S') {
      scale = getxscale();
      argv[4][l] = 0;
    }
    if (Tcl_GetDouble(interp, argv[4], &msize) != TCL_OK) return TCL_ERROR;
    minfo.msize = msize*scale;
  }

  /* 
   * If marker info was supplied in vector format, then prepare minfos
   *  and called dlgDrawMultiMarkers
   */
  if (sizes) {
    float scale = 1.0;
    if (scaletype == DLG_X_SCALE) scale = getxscale();
    else if (scaletype == DLG_WINDOW_SCALE || scaletype == DLG_WINDOW_XSCALE) {
      float xres, yres, xul, yub, xur, yut;
      getresol(&xres, &yres);
      getwindow(&xul, &yub, &xur, &yut);
      scale = xres/fabs(xur-xul) * getxscale();
    }
    else if (scaletype == DLG_WINDOW_YSCALE) {
      float xres, yres, xul, yub, xur, yut;
      getresol(&xres, &yres);
      getwindow(&xul, &yub, &xur, &yut);
      scale = yres/fabs(yut-yub) * getyscale();
    }
    status = dlgDrawMultiMarkers(dlx, dly, sizes, scale, colors, &minfo);
  }
  else {
    float scale = 1.0;
    if (scaletype == DLG_X_SCALE) scale = getxscale();
    else if (scaletype == DLG_WINDOW_SCALE || scaletype == DLG_WINDOW_XSCALE) {
      float xres, yres, xul, yub, xur, yut;
      getresol(&xres, &yres);
      getwindow(&xul, &yub, &xur, &yut);
      scale = xres/fabs(xur-xul) * getxscale();
    }
    else if (scaletype == DLG_WINDOW_YSCALE) {
      float xres, yres, xul, yub, xur, yut;
      getresol(&xres, &yres);
      getwindow(&xul, &yub, &xur, &yut);
      scale = yres/fabs(yut-yub) * getyscale();
    }
    minfo.msize *= scale;

    if (colors) {
      /* create sizes list so MultiMarkers can be called */
      int length;
      length = DYN_LIST_N(dlx);
      if (DYN_LIST_N(dly) > length) length = DYN_LIST_N(dly);
      sizes = dynListOnesFloat(length);
      status = dlgDrawMultiMarkers(dlx, dly, sizes, minfo.msize, 
				   colors, &minfo);
      dfuFreeDynList(sizes);
    }
    else status = dlgDrawMarkers(dlx, dly, &minfo);
  }

  switch (status) {
  case DLG_NOWINDOW:
    Tcl_AppendResult(interp, argv[0], 
		     ": no graphics window open", (char *) NULL);
    goto error;
    break;
  case DLG_BADARGS:
    Tcl_AppendResult(interp, argv[0], 
		     ": bad arguments", (char *) NULL);
    goto error;
    break;
  case DLG_ARGMISMATCH:
    Tcl_AppendResult(interp, argv[0], 
		     ": lists must be same length", (char *) NULL);
    goto error;
    break;
  default:
    if (dltx) dfuFreeDynList(dltx);
    if (dlty) dfuFreeDynList(dlty);
    return TCL_OK;
  }

 error:
  /* Free temporary lists if they were created above */
  if (dltx) dfuFreeDynList(dltx);
  if (dlty) dfuFreeDynList(dlty);
  return TCL_ERROR;
}

int dlgDrawMarkers(DYN_LIST *dlx, DYN_LIST *dly, MARKER_INFO *minfo)
{
  int mode = 0;
  int oldcolor = 0, oldwidth = 0, oldclip = 0;
  int length;
  
  switch (DYN_LIST_DATATYPE(dlx)) {
  case DF_STRING:
    return DLG_BADARGS;
    break;
  case DF_LIST:
    if (DYN_LIST_DATATYPE(dly) != DF_LIST) {
      int i, status;
      DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dlx);
      for (i = 0; i < DYN_LIST_N(dlx); i++) {
	status = dlgDrawMarkers(sublists[i], dly, minfo);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else if (DYN_LIST_N(dly) == DYN_LIST_N(dlx)) {
	int i, status;
	DYN_LIST **sx = (DYN_LIST **) DYN_LIST_VALS(dlx);
	DYN_LIST **sy = (DYN_LIST **) DYN_LIST_VALS(dly);
	for (i = 0; i < DYN_LIST_N(dlx); i++) {
	  status = dlgDrawMarkers(sx[i], sy[i], minfo);
	  if (status != DLG_OK) return status;
	}
	return DLG_OK;
    }
    else if (DYN_LIST_N(dlx) == 1) {
      int i, status;
      DYN_LIST **sx = (DYN_LIST **) DYN_LIST_VALS(dlx);
      DYN_LIST **sy = (DYN_LIST **) DYN_LIST_VALS(dly);
      for (i = 0; i < DYN_LIST_N(dly); i++) {
	status = dlgDrawMarkers(sx[0], sy[i], minfo);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else if (DYN_LIST_N(dly) == 1) {
      int i, status;
      DYN_LIST **sx = (DYN_LIST **) DYN_LIST_VALS(dlx);
      DYN_LIST **sy = (DYN_LIST **) DYN_LIST_VALS(dly);
      for (i = 0; i < DYN_LIST_N(dlx); i++) {
	status = dlgDrawMarkers(sx[i], sy[0], minfo);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else return DLG_ARGMISMATCH;
    break;
  }
  
  switch (DYN_LIST_DATATYPE(dly)) {
  case DF_STRING:
    return DLG_BADARGS;
    break;
  case DF_LIST:
    if (DYN_LIST_DATATYPE(dlx) != DF_LIST) {
      int i, status;
      DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dly);
      for (i = 0; i < DYN_LIST_N(dly); i++) {
	status = dlgDrawMarkers(dlx, sublists[i], minfo);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else return DLG_BADARGS;
    break;
  }

  if (!DYN_LIST_N(dlx) || !DYN_LIST_N(dly)) return DLG_ZEROLIST;
  if (DYN_LIST_N(dlx) == DYN_LIST_N(dly)) {
    mode = 0;
    length = DYN_LIST_N(dlx);
  }
  else if (DYN_LIST_N(dlx) == 1) {
    mode = 1;
    length = DYN_LIST_N(dly);
  }
  else if (DYN_LIST_N(dly) == 1) {
    mode = 2;
    length = DYN_LIST_N(dlx);
  }
  else return DLG_ARGMISMATCH;


  if (DYN_LIST_DATATYPE(dlx) == DF_FLOAT &&
      DYN_LIST_DATATYPE(dly) == DF_FLOAT) {
    int i;
    float *x = (float *) DYN_LIST_VALS(dlx);
    float *y = (float *) DYN_LIST_VALS(dly);

    if (minfo->clip >= 0) oldclip = setclip(minfo->clip);
    if (minfo->color >= 0) oldcolor = setcolor(minfo->color);
    if (minfo->lwidth >= 0) oldwidth = setlwidth(minfo->lwidth);
    switch (mode) {
    case 0:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[i], y[i], minfo->msize);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[0], y[i], minfo->msize);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[i], y[0], minfo->msize);
      }
      break;
    }
    if (minfo->lwidth >= 0) setlwidth(oldwidth);
    if (minfo->color >= 0) setcolor(oldcolor);
    if (minfo->clip >= 0) setclip(oldclip);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_LONG &&
	   DYN_LIST_DATATYPE(dly) == DF_FLOAT) {
    int i;
    int *x = (int *) DYN_LIST_VALS(dlx);
    float *y = (float *) DYN_LIST_VALS(dly);
    
    if (minfo->clip >= 0) oldclip = setclip(minfo->clip);
    if (minfo->color >= 0) oldcolor = setcolor(minfo->color);
    if (minfo->lwidth >= 0) oldwidth = setlwidth(minfo->lwidth);
    switch (mode) {
    case 0:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[i], y[i], minfo->msize);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[0], y[i], minfo->msize);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[i], y[0], minfo->msize);
      }
      break;
    }
    if (minfo->lwidth >= 0) setlwidth(oldwidth);
    if (minfo->color >= 0) setcolor(oldcolor);
    if (minfo->clip >= 0) setclip(oldclip);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_FLOAT &&
	   DYN_LIST_DATATYPE(dly) == DF_LONG) {
    int i;
    float *x = (float *) DYN_LIST_VALS(dlx);
    int *y = (int *) DYN_LIST_VALS(dly);
    
    if (minfo->clip >= 0) oldclip = setclip(minfo->clip);
    if (minfo->color >= 0) oldcolor = setcolor(minfo->color);
    if (minfo->lwidth >= 0) oldwidth = setlwidth(minfo->lwidth);
    switch (mode) {
    case 0:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[i], y[i], minfo->msize);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[0], y[i], minfo->msize);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[i], y[0], minfo->msize);
      }
      break;
    }
    if (minfo->lwidth >= 0) setlwidth(oldwidth);
    if (minfo->color >= 0) setcolor(oldcolor);
    if (minfo->clip >= 0) setclip(oldclip);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_LONG &&
	   DYN_LIST_DATATYPE(dly) == DF_LONG) {
    int i;
    int *x = (int *) DYN_LIST_VALS(dlx);
    int *y = (int *) DYN_LIST_VALS(dly);
    
    if (minfo->clip >= 0) oldclip = setclip(minfo->clip);
    if (minfo->color >= 0) oldcolor = setcolor(minfo->color);
    if (minfo->lwidth >= 0) oldwidth = setlwidth(minfo->lwidth);
    switch (mode) {
    case 0:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[i], y[i], minfo->msize);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[0], y[i], minfo->msize);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	(*(minfo->mfunc))(x[i], y[0], minfo->msize);
      }
      break;
    }
    if (minfo->lwidth >= 0) setlwidth(oldwidth);
    if (minfo->color >= 0) setcolor(oldcolor);
    if (minfo->clip >= 0) setclip(oldclip);
    return DLG_OK;
  }
  else return DLG_BADARGS;
}

/*
 * Drawing different marker types (sizes, colors) at each x-y postition
 */

int dlgDrawMultiMarkers(DYN_LIST *dlx, DYN_LIST *dly, 
			DYN_LIST *sizes, float scale, 
			DYN_LIST *colors, MARKER_INFO *minfo)
{
  int mode = 0;
  int *colorvals = NULL;
  int oldcolor = 0, oldwidth = 0, oldclip = 0;
  int length;


  switch (DYN_LIST_DATATYPE(dlx)) {
  case DF_STRING:
    return DLG_BADARGS;
    break;
  case DF_LIST:
    if (DYN_LIST_DATATYPE(dly) != DF_LIST) {
      int i, status;
      DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dlx);
      for (i = 0; i < DYN_LIST_N(dlx); i++) {
	status = dlgDrawMultiMarkers(sublists[i], dly, 
				     sizes, scale, colors, minfo);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else if (DYN_LIST_N(dly) == DYN_LIST_N(dlx)) {
	int i, status;
	DYN_LIST **sx = (DYN_LIST **) DYN_LIST_VALS(dlx);
	DYN_LIST **sy = (DYN_LIST **) DYN_LIST_VALS(dly);
	for (i = 0; i < DYN_LIST_N(dlx); i++) {
	  status = dlgDrawMultiMarkers(sx[i], sy[i],
				  sizes, scale, colors, minfo);
	  if (status != DLG_OK) return status;
	}
	return DLG_OK;
    }
    else if (DYN_LIST_N(dlx) == 1) {
      int i, status;
      DYN_LIST **sx = (DYN_LIST **) DYN_LIST_VALS(dlx);
      DYN_LIST **sy = (DYN_LIST **) DYN_LIST_VALS(dly);
      for (i = 0; i < DYN_LIST_N(dly); i++) {
	status = dlgDrawMultiMarkers(sx[0], sy[i],
				     sizes, scale, colors, minfo);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else if (DYN_LIST_N(dly) == 1) {
      int i, status;
      DYN_LIST **sx = (DYN_LIST **) DYN_LIST_VALS(dlx);
      DYN_LIST **sy = (DYN_LIST **) DYN_LIST_VALS(dly);
      for (i = 0; i < DYN_LIST_N(dlx); i++) {
	status = dlgDrawMultiMarkers(sx[i], sy[0],
				     sizes, scale, colors, minfo);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else return DLG_ARGMISMATCH;
    break;
  }
  
  switch (DYN_LIST_DATATYPE(dly)) {
  case DF_STRING:
    return DLG_BADARGS;
    break;
  case DF_LIST:
    if (DYN_LIST_DATATYPE(dlx) != DF_LIST) {
      int i, status;
      DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dly);
      for (i = 0; i < DYN_LIST_N(dly); i++) {
	status = dlgDrawMultiMarkers(dlx, sublists[i], 
				     sizes, scale, colors, minfo);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else return DLG_BADARGS;
    break;
  }
  

  if (!DYN_LIST_N(dlx) || !DYN_LIST_N(dly)) return DLG_ZEROLIST;
  if (DYN_LIST_N(dlx) == DYN_LIST_N(dly)) {
    mode = 0;
    length = DYN_LIST_N(dlx);
  }
  else if (DYN_LIST_N(dlx) == 1) {
    mode = 1;
    length = DYN_LIST_N(dly);
  }
  else if (DYN_LIST_N(dly) == 1) {
    mode = 2;
    length = DYN_LIST_N(dlx);
  }
  else return DLG_ARGMISMATCH;

  /* Check the size list */
  if (DYN_LIST_N(sizes) != length) return DLG_ARGMISMATCH;
  if (DYN_LIST_DATATYPE(sizes) != DF_FLOAT &&
      DYN_LIST_DATATYPE(sizes) != DF_LONG) return DLG_BADARGS;

  if (colors) {
    if (DYN_LIST_N(colors) != length) return DLG_ARGMISMATCH;
    if (DYN_LIST_DATATYPE(colors) != DF_LONG) return DLG_BADARGS;
    colorvals = (int *) DYN_LIST_VALS(colors);
  }

  if (DYN_LIST_DATATYPE(dlx) == DF_FLOAT &&
      DYN_LIST_DATATYPE(dly) == DF_FLOAT) {
    int i;
    float *x = (float *) DYN_LIST_VALS(dlx);
    float *y = (float *) DYN_LIST_VALS(dly);

    if (minfo->clip >= 0) oldclip = setclip(minfo->clip);
    if (minfo->color >= 0) oldcolor = setcolor(minfo->color);
    if (minfo->lwidth >= 0) oldwidth = setlwidth(minfo->lwidth);
    switch (mode) {
    case 0:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }
      break;
    case 1:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }

      break;
    case 2:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }
      break;
    }
    if (minfo->lwidth >= 0) setlwidth(oldwidth);
    if (minfo->color >= 0) setcolor(oldcolor);
    if (minfo->clip >= 0) setclip(oldclip);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_LONG &&
	   DYN_LIST_DATATYPE(dly) == DF_FLOAT) {
    int i;
    int *x = (int *) DYN_LIST_VALS(dlx);
    float *y = (float *) DYN_LIST_VALS(dly);
    
    if (minfo->clip >= 0) oldclip = setclip(minfo->clip);
    if (minfo->color >= 0) oldcolor = setcolor(minfo->color);
    if (minfo->lwidth >= 0) oldwidth = setlwidth(minfo->lwidth);
    switch (mode) {
    case 0:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }
      break;
    case 1:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }
      break;
    case 2:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }
      break;
    }
    if (minfo->lwidth >= 0) setlwidth(oldwidth);
    if (minfo->color >= 0) setcolor(oldcolor);
    if (minfo->clip >= 0) setclip(oldclip);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_FLOAT &&
	   DYN_LIST_DATATYPE(dly) == DF_LONG) {
    int i;
    float *x = (float *) DYN_LIST_VALS(dlx);
    int *y = (int *) DYN_LIST_VALS(dly);
    
    if (minfo->clip >= 0) oldclip = setclip(minfo->clip);
    if (minfo->color >= 0) oldcolor = setcolor(minfo->color);
    if (minfo->lwidth >= 0) oldwidth = setlwidth(minfo->lwidth);
    switch (mode) {
    case 0:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }
      break;
    case 1:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }
      break;
    case 2:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }
      break;
    }
    if (minfo->lwidth >= 0) setlwidth(oldwidth);
    if (minfo->color >= 0) setcolor(oldcolor);
    if (minfo->clip >= 0) setclip(oldclip);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_LONG &&
	   DYN_LIST_DATATYPE(dly) == DF_LONG) {
    int i;
    int *x = (int *) DYN_LIST_VALS(dlx);
    int *y = (int *) DYN_LIST_VALS(dly);
    
    if (minfo->clip >= 0) oldclip = setclip(minfo->clip);
    if (minfo->color >= 0) oldcolor = setcolor(minfo->color);
    if (minfo->lwidth >= 0) oldwidth = setlwidth(minfo->lwidth);
    switch (mode) {
    case 0:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[i], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }
      break;
    case 1:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[0], y[i], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }
      break;
    case 2:
      if (!colors) {
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
      }
      else {
	oldcolor = setcolor(0);
	if (DYN_LIST_DATATYPE(sizes) == DF_FLOAT) {
	  float *vals = (float *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	else if (DYN_LIST_DATATYPE(sizes) == DF_LONG) {
	  int *vals = (int *) DYN_LIST_VALS(sizes);
	  for (i = 0; i < length; i++) {
	    setcolor(colorvals[i]);
	    (*(minfo->mfunc))(x[i], y[0], vals[i]*scale);
	  }
	}
	setcolor(oldcolor);
      }
      break;
    }
    if (minfo->lwidth >= 0) setlwidth(oldwidth);
    if (minfo->color >= 0) setcolor(oldcolor);
    if (minfo->clip >= 0) setclip(oldclip);
    return DLG_OK;
  }
  else return DLG_BADARGS;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclLineDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_line
 *    dlg_bar
 *    dlg_step
 *
 * DESCRIPTION
 *    Draws lines at positions specified by list arguments.
 *
 *****************************************************************************/

static int tclLineDynList (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  DYN_LIST *dlx, *dly, *dltx = NULL, *dlty = NULL;
  DYN_LIST *fillcolors, *linecolors;
  LINE_INFO linfo;
  int lstyle, lwidth, filled, linecolor, fillcolor, sideways, clip;
  int status;
  int mode = (int) data;
  int i, j, skip = 1, boxfilter = -1, closed = 0;
  double val;
  
  if (!dlgGetLineInfo(mode, &linfo)) {
    Tcl_AppendResult(interp, argv[0], ": unknown line function", 
		     (char *) NULL);
      goto error;
  }

  /* This is a nasty command parsing loop, looking for option pairs */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-' &&
	argv[i][1] != '.' &&
	(argv[i][1] < '0' || argv[i][1] > '9')) {
      if (!strcmp(argv[i],"-filled")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no arg to filled", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &filled) != TCL_OK) goto error;
	linfo.filled = filled;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-lstyle")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no arg to lstyle", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &lstyle) != TCL_OK) goto error;
	linfo.lstyle = lstyle;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-lwidth")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no arg to lwidth", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &lwidth) != TCL_OK) goto error;
	linfo.lwidth = lwidth;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-sideways")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no arg to sideways", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &sideways) != TCL_OK) goto error;
	linfo.sideways = sideways;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-clip")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no arg to clip", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &clip) != TCL_OK) goto error;
	linfo.clip = clip;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-closed")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no arg to closed", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &closed) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-linecolor")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no arg to linecolor", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &linecolor) != TCL_OK) goto error;
	linfo.linecolor = linecolor;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-start")) {
	double start;
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no start specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetDouble(interp, argv[i+1], &start) != TCL_OK) goto error;
	linfo.fparams[0] = start;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-width")) {
	double width;
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no width specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetDouble(interp, argv[i+1], &width) != TCL_OK) goto error;
	linfo.fparams[1] = width;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }

      else if (!strcmp(argv[i],"-interbar")) {
	double width;
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no interbar spacing specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetDouble(interp, argv[i+1], &width) != TCL_OK) goto error;
	linfo.fparams[3] = width;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }

      else if (!strcmp(argv[i],"-offset") || !strcmp(argv[i],"-xoffset")) {
	double offset;
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no offset specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetDouble(interp, argv[i+1], &offset) != TCL_OK) goto error;
	linfo.fparams[2] = offset;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-skip")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no skip specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &skip) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strncmp(argv[i],"-box", 4)) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no boxfilter value specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &boxfilter) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-fillcolor")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no arg to fillcolor", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &fillcolor) != TCL_OK) goto error;
	linfo.fillcolor = fillcolor;
	linfo.filled = 1;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-linecolors")) {
	if (tclFindDynList(interp, argv[i+1], &linecolors) != TCL_OK)
	  goto error;
	linfo.linecolors = linecolors;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-fillcolors")) {
	if (tclFindDynList(interp, argv[i+1], &fillcolors) != TCL_OK)
	  goto error;
	linfo.fillcolors = fillcolors;
	linfo.filled = 1;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      
      else {
	/* Could be a negative number, so try to parse as double */
	double val;
	if (Tcl_GetDouble(interp, argv[i], &val) != TCL_OK) {
	  Tcl_ResetResult(interp);
	  Tcl_AppendResult(interp, argv[0], 
			   ": bad option ", argv[i], (char *) NULL);
	  goto error;
	}
      }
    }
  }
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage:\t", argv[0], 
		     " xlist ylist [lstyle lwidth filled]",
		     "\noptions:   -lstyle, -lwidth, -filled, -linecolor,", "-linecolors",
		     " -fillcolor, -fillcolors, -start, -width, -interbar, -sideways",
		     "-skip n, -boxfilter n, -closed", (char *) NULL);
    return TCL_ERROR;
  }

  tclFindDynList(interp, argv[1], &dlx);
  tclFindDynList(interp, argv[2], &dly);

  /* 
   * If both are lists and either is a list of length one, while the other is 
   * not, replicate the length one list n times...
   */
  if (dlx && dly && DYN_LIST_N(dlx) && DYN_LIST_N(dly)) {
    DYN_LIST *temp;
    if (DYN_LIST_N(dlx) != DYN_LIST_N(dly)) {
      if (DYN_LIST_N(dlx) == 1) {
	temp = dfuCreateDynList(DYN_LIST_DATATYPE(dlx), DYN_LIST_N(dly));
	for (i = 0; i < DYN_LIST_N(dly); i++) 
	  dynListCopyElement(dlx, 0, temp);
	dlx = temp;
	dltx = dlx;
      }
      else if (DYN_LIST_N(dly) == 1) {
	DYN_LIST *temp;
	temp = dfuCreateDynList(DYN_LIST_DATATYPE(dly), DYN_LIST_N(dlx));
	for (i = 0; i < DYN_LIST_N(dlx); i++) 
	  dynListCopyElement(dly, 0, temp);
	dly = temp;
	dlty = dly;
      }
    }
  }
  else if (!dlx && !dly) {		/* both scalars? */
    if (Tcl_GetDouble(interp, argv[1], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": xvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dlx = dfuCreateDynList(DF_FLOAT, 1);
    dfuAddDynListFloat(dlx, val);
    dltx = dlx;

    if (Tcl_GetDouble(interp, argv[2], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": yvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dly = dfuCreateDynList(DF_FLOAT, 1);
    dfuAddDynListFloat(dly, val);
    dlty = dly;
    Tcl_ResetResult(interp);
  }
  else if (!dlx) {
    if (Tcl_GetDouble(interp, argv[1], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": xvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dlx = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dly));
    for (i = 0; i < DYN_LIST_N(dly); i++) 
      dfuAddDynListFloat(dlx, val);
    dltx = dlx;
    Tcl_ResetResult(interp);
  }
  else if (!dly) {
    if (Tcl_GetDouble(interp, argv[2], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": yvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dly = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dlx));
    for (i = 0; i < DYN_LIST_N(dlx); i++) 
      dfuAddDynListFloat(dly, val);
    dlty = dly;
    Tcl_ResetResult(interp);
  }

  if (argc > 3) {
    if (Tcl_GetInt(interp, argv[3], &lstyle) != TCL_OK) 
      goto error;
    linfo.lstyle = lstyle;
  }

  if (argc > 4) {
    if (Tcl_GetInt(interp, argv[4], &lwidth) != TCL_OK) 
      goto error;
    linfo.lwidth = lwidth;
  }

  if (argc > 5) {
    if (Tcl_GetInt(interp, argv[5], &filled) != TCL_OK) 
      goto error;
    linfo.filled = filled;
  }

  linfo.skip = skip;
  linfo.boxfilter = boxfilter;
  linfo.closed = closed;

  status = dlgDrawLines(dlx, dly, &linfo, 0);

  switch (status) {
  case DLG_NOWINDOW:
    Tcl_AppendResult(interp, argv[0], 
		     ": no graphics window open", (char *) NULL);
    goto error;
    break;
  case DLG_BADARGS:
    Tcl_AppendResult(interp, argv[0], 
		     ": bad arguments", (char *) NULL);
    goto error;
    break;
  case DLG_ARGMISMATCH:
    Tcl_AppendResult(interp, argv[0], 
		     ": lists must be same length", (char *) NULL);
    goto error;
    break;
  case DLG_NOMEMORY:
    Tcl_AppendResult(interp, argv[0], 
		     ": unable to allocate memory", (char *) NULL);
    goto error;
    break;
  default:
    if (dltx) dfuFreeDynList(dltx);
    if (dlty) dfuFreeDynList(dlty);
    return TCL_OK;
  }

 error:
  if (dltx) dfuFreeDynList(dltx);
  if (dlty) dfuFreeDynList(dlty);
  return TCL_ERROR;
}

int dlgDrawLines(DYN_LIST *dlx, DYN_LIST *dly, LINE_INFO *linfo, int idx)
{
  switch (DYN_LIST_DATATYPE(dlx)) {
  case DF_STRING:
    return DLG_BADARGS;
    break;
  case DF_LIST:
    if (DYN_LIST_DATATYPE(dly) != DF_LIST) {
      int i, status;
      DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dlx);
      for (i = 0; i < DYN_LIST_N(dlx); i++) {
	status = dlgDrawLines(sublists[i], dly, linfo, i);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else if (DYN_LIST_N(dly) == DYN_LIST_N(dlx)) {
	int i, status;
	DYN_LIST **sx = (DYN_LIST **) DYN_LIST_VALS(dlx);
	DYN_LIST **sy = (DYN_LIST **) DYN_LIST_VALS(dly);
	for (i = 0; i < DYN_LIST_N(dlx); i++) {
	  status = dlgDrawLines(sx[i], sy[i], linfo, i);
	  if (status != DLG_OK) return status;
	}
	return DLG_OK;
    }
    else if (DYN_LIST_N(dlx) == 1) {
      int i, status;
      DYN_LIST **sx = (DYN_LIST **) DYN_LIST_VALS(dlx);
      DYN_LIST **sy = (DYN_LIST **) DYN_LIST_VALS(dly);
      for (i = 0; i < DYN_LIST_N(dly); i++) {
	status = dlgDrawLines(sx[0], sy[i], linfo, i);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else if (DYN_LIST_N(dly) == 1) {
      int i, status;
      DYN_LIST **sx = (DYN_LIST **) DYN_LIST_VALS(dlx);
      DYN_LIST **sy = (DYN_LIST **) DYN_LIST_VALS(dly);
      for (i = 0; i < DYN_LIST_N(dlx); i++) {
	status = dlgDrawLines(sx[i], sy[0], linfo, i);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else return DLG_ARGMISMATCH;
    break;
  }
  
  switch (DYN_LIST_DATATYPE(dly)) {
  case DF_STRING:
    return DLG_BADARGS;
    break;
  case DF_LIST:
    if (DYN_LIST_DATATYPE(dlx) != DF_LIST) {
      int i, status;
      DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dly);
      for (i = 0; i < DYN_LIST_N(dly); i++) {
	status = dlgDrawLines(dlx, sublists[i], linfo, i);
	if (status != DLG_OK) return status;
      }
      return DLG_OK;
    }
    else return DLG_BADARGS;
    break;
  }

  if (DYN_LIST_N(dlx) != DYN_LIST_N(dly)) {
    return DLG_ARGMISMATCH;
  }

  if (!DYN_LIST_N(dlx)) {
    return DLG_ZEROLIST;
  }

  if (linfo->id == DLG_BEZIER) {
    if ((DYN_LIST_N(dlx) < 4) || (DYN_LIST_N(dlx) % 4 != 0))
      return DLG_BADARGS;
  }

  /* set current color using dynlist of linecolors if possible */
  if (linfo->linecolors) {
    if (idx < DYN_LIST_N(linfo->linecolors) &&
	DYN_LIST_DATATYPE(linfo->linecolors) == DF_LONG) {
      linfo->linecolor = ((int *) DYN_LIST_VALS(linfo->linecolors))[idx];
    }
  }
  
  /* set current color using dynlist of fillcolors if possible */
  if (linfo->fillcolors) {
    if (idx < DYN_LIST_N(linfo->fillcolors) &&
	DYN_LIST_DATATYPE(linfo->fillcolors) == DF_LONG) {
      linfo->fillcolor = ((int *) DYN_LIST_VALS(linfo->fillcolors))[idx];
    }
  }
  
/* 
   * If lists are both floats, then no float conversion is
   * necessary
   */
  if (DYN_LIST_DATATYPE(dlx) == DF_FLOAT &&
      DYN_LIST_DATATYPE(dly) == DF_FLOAT) {
    float *x = (float *) DYN_LIST_VALS(dlx);
    float *y = (float *) DYN_LIST_VALS(dly);
    (*(linfo->lfunc))(DYN_LIST_N(dlx), x, y, linfo);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_LONG &&
	   DYN_LIST_DATATYPE(dly) == DF_FLOAT) {
    int i;
    float *x = (float *) calloc(DYN_LIST_N(dlx), sizeof(float));
    int   *xv = (int *) DYN_LIST_VALS(dlx);
    float *y = (float *) DYN_LIST_VALS(dly);

    if (!x) return (DLG_NOMEMORY);
    
    for (i = 0; i < DYN_LIST_N(dlx); i++) {
      x[i] = (float) xv[i];
    }
    
    (*(linfo->lfunc))(DYN_LIST_N(dlx), x, y, linfo);

    free ((void *) x);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_FLOAT &&
	   DYN_LIST_DATATYPE(dly) == DF_LONG) {
    int i;
    float *x = (float *) DYN_LIST_VALS(dlx);
    float *y = (float *) calloc(DYN_LIST_N(dly), sizeof(float));
    int   *yv = (int *) DYN_LIST_VALS(dly);

    if (!y) return (DLG_NOMEMORY);
    
    for (i = 0; i < DYN_LIST_N(dly); i++) {
      y[i] = (float) yv[i];
    }
    
    (*(linfo->lfunc))(DYN_LIST_N(dlx), x, y, linfo);

    free ((void *) y);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_LONG &&
	   DYN_LIST_DATATYPE(dly) == DF_LONG) {
    int i;
    float *x = (float *) calloc(DYN_LIST_N(dlx), sizeof(float));
    int   *xv = (int *) DYN_LIST_VALS(dlx);
    float *y = (float *) calloc(DYN_LIST_N(dly), sizeof(float));
    int   *yv = (int *) DYN_LIST_VALS(dly);

    if (!x || !y) return (DLG_NOMEMORY);
    
    for (i = 0; i < DYN_LIST_N(dly); i++) {
      x[i] = (float) xv[i];
      y[i] = (float) yv[i];
    }
    
    (*(linfo->lfunc))(DYN_LIST_N(dlx), x, y, linfo);

    free ((void *) x);
    free ((void *) y);
    return DLG_OK;
  }
  else return DLG_BADARGS;
}



int dlgLine(int n, float *x, float *y, LINE_INFO *linfo)
{
  int i, j, k, total;
  int oldstyle = 0, oldwidth = 0, oldcolor = 0, oldclip;
  float *verts = NULL, *v;
  
  if (n < 2) return 0;
  oldstyle = setlstyle(linfo->lstyle);
  oldwidth = setlwidth(linfo->lwidth);
  if (linfo->clip >= 0) oldclip = setclip(linfo->clip);

  if (linfo->filled) {
    /* If the polygon is closed, no need to start from the base */
    if (linfo->closed || (x[0] == x[n-1] && y[0] == y[n-1])) {
      if (linfo->boxfilter < 0) {
	verts = (float *) calloc((2*n)/linfo->skip, sizeof(float));
	total = n/linfo->skip;
      }
      else {
	verts = (float *) calloc((2*n)/linfo->boxfilter, sizeof(float));
	total = n/linfo->boxfilter;
      }
      if (!verts) return (0);
      if (linfo->fillcolor >= 0) oldcolor = setcolor(linfo->fillcolor);
      if (linfo->boxfilter < 0) {
	for (i = 0, v = verts; i < n; i+=linfo->skip) {
	  *v++ = x[i];
	  *v++ = y[i];
	}
      }
      else {
	int max, stop;
	float sumx, sumy;
	max = n - linfo->boxfilter + 1;
	for (i = 0, k = 0, v = verts; i < max; i+=linfo->boxfilter, k++) {
	  stop = i+linfo->boxfilter;
	  sumx = 0; sumy = 0;
	  for (j = i+1; j < stop; j++) {
	    sumx += x[j];
	    sumy += y[j];
	  }
	  *v++ = sumx/linfo->boxfilter; 
	  *v++ = sumy/linfo->boxfilter;
	}
      }
      filledpoly(total, verts);
      if (linfo->fillcolor >= 0) setcolor(oldcolor);
      free((void *) verts);
    }
    else {			/* use linfo->fparams[0] as start y */
      if (linfo->boxfilter < 0) {
	verts = (float *) calloc((2*n)/linfo->skip+4, sizeof(float));
	total = n/linfo->skip+2;
      }
      else {
	verts = (float *) calloc((2*n)/linfo->boxfilter+4, sizeof(float));
	total = n/linfo->boxfilter+2;
      }
      if (!verts) return (0);
      if (linfo->fillcolor >= 0) oldcolor = setcolor(linfo->fillcolor);
      verts[0] = x[0];
      verts[1] = linfo->fparams[0];

      if (linfo->boxfilter < 0) {
	for (i = 0, v = verts+2; i < n; i+=linfo->skip) {
	  *v++ = x[i];
	  *v++ = y[i];
	}
      }
      else {
	int max, stop;
	float sumx, sumy;
	max = n - linfo->boxfilter + 1;
	for (i = 0, k = 0, v = verts; i < max; i+=linfo->boxfilter, k++) {
	  stop = i+linfo->boxfilter;
	  sumx = 0; sumy = 0;
	  for (j = i+1; j < stop; j++) {
	    sumx += x[j];
	    sumy += y[j];
	  }
	  *v++ = sumx/linfo->boxfilter; 
	  *v++ = sumy/linfo->boxfilter;
	}
      }
      *v++ = x[i-1];
      *v++ = linfo->fparams[0];

      filledpoly(total, verts);
      if (linfo->fillcolor >= 0) setcolor(oldcolor);
      free((void *) verts);
    }
  }

  if (linfo->boxfilter < 0) {
    verts = (float *) calloc((2*n)/linfo->skip, sizeof(float));
    total = n/linfo->skip;
  }
  else {
    verts = (float *) calloc((2*n)/linfo->boxfilter, sizeof(float));
    total = n/linfo->boxfilter;
  }
  if (!verts) return (0);
  if (linfo->boxfilter < 0) {
    for (i = 0, v = verts; i < n; i+=linfo->skip) {
      *v++ = x[i];
      *v++ = y[i];
    }
  }
  else {
    int max, stop;
    float sumx, sumy;
    max = n - linfo->boxfilter + 1;
    for (i = 0, k = 0, v = verts; i < max; i+=linfo->boxfilter, k++) {
      stop = i+linfo->boxfilter;
      sumx = 0; sumy = 0;
      for (j = i+1; j < stop; j++) {
	sumx += x[j];
	sumy += y[j];
      }
      *v++ = sumx/linfo->boxfilter; 
      *v++ = sumy/linfo->boxfilter;
    }
  }
  
  /* Now draw lines on top */
  if (linfo->linecolor >= 0) oldcolor = setcolor(linfo->linecolor);
  polyline(total, verts);
  if (linfo->linecolor >= 0) setcolor(oldcolor);

  if (linfo->clip >= 0) setclip(oldclip);
  setlwidth(oldwidth);
  setlstyle(oldstyle);

  free(verts);

  return 1;
}


int dlgBar(int n, float *x, float *y, LINE_INFO *linfo)
{
  int i;
  int oldstyle = 0, oldwidth = 0, oldcolor = 0, oldclip;
  float width, off = 0, start;
  
  if (n < 1) return 0;

  oldstyle = setlstyle(linfo->lstyle);
  oldwidth = setlwidth(linfo->lwidth);
  if (linfo->clip >= 0) oldclip = setclip(linfo->clip);

  if (!linfo->sideways) {
    if (linfo->filled) {
      if (linfo->fillcolor >= 0) oldcolor = setcolor(linfo->fillcolor);

      /* fparams[3] = width override */
      if (linfo->fparams[3] == 0.0) width = x[1]-x[0];
      else width = linfo->fparams[3];
	
      off = 0.5*(width*linfo->fparams[1]);
      for (i = 0; i < n; i++) {
	start = x[i]+linfo->fparams[2];
	filledrect(start-off, linfo->fparams[0],
		   start+off, y[i]);
      }
      if (linfo->fillcolor >= 0) setcolor(oldcolor);
    }
    
    if (linfo->linecolor >= 0) oldcolor = setcolor(linfo->linecolor);

    /* fparams[3] = width override */
    if (linfo->fparams[3] == 0.0) width = x[1]-x[0];
    else width = linfo->fparams[3];

    off = 0.5*(width*linfo->fparams[1]);
    for (i = 0; i < n; i++) {
      start = x[i]+linfo->fparams[2];
      rect(start-off, linfo->fparams[0], start+off, y[i]);
    }
    if (linfo->linecolor >= 0) setcolor(oldcolor);
  }
  else {			/* start from yaxis, not xaxis */
    if (linfo->filled) {
      if (linfo->fillcolor >= 0) oldcolor = setcolor(linfo->fillcolor);

      /* fparams[3] = width override */
      if (linfo->fparams[3] == 0.0) width = x[1]-x[0];
      else width = linfo->fparams[3];

      off = 0.5*(width*linfo->fparams[1]);
      for (i = 0; i < n; i++) {
	start = x[i]+linfo->fparams[2];
	filledrect(linfo->fparams[0], start-off, y[i], start+off);
      }
      if (linfo->fillcolor >= 0) setcolor(oldcolor);
    }
    
    if (linfo->linecolor >= 0) oldcolor = setcolor(linfo->linecolor);

    /* fparams[3] = width override */
    if (linfo->fparams[3] == 0.0) width = x[1]-x[0];
    else width = linfo->fparams[3];
    
    off = 0.5*(width*linfo->fparams[1]);
    for (i = 0; i < n; i++) {
      start = x[i]+linfo->fparams[2];
      rect(linfo->fparams[0], start-off, y[i], start+off);
    }
    if (linfo->linecolor >= 0) setcolor(oldcolor);
  }
  
  if (linfo->clip >= 0) setclip(oldclip);
  setlwidth(oldwidth);
  setlstyle(oldstyle);
  return 1;
}

int dlgStep(int n, float *x, float *y, LINE_INFO *linfo)
{
  int i, stop = n-1;
  int oldstyle = 0, oldwidth = 0, oldcolor = 0, oldclip;

  if (n < 2) return 0;

  oldstyle = setlstyle(linfo->lstyle);
  oldwidth = setlwidth(linfo->lwidth);
  if (linfo->clip >= 0) oldclip = setclip(linfo->clip);

  if (linfo->filled) {
    int i;
    float *verts = (float *) calloc(4*n+2, sizeof(float));
    float *v = verts;
    if (!verts) return (0);
    *v++ = x[0];
    *v++ = linfo->fparams[0];
    for (i = 0; i < stop; i++) {
      *v++ = x[i];
      *v++ = y[i];
      *v++ = x[i+1];
      *v++ = y[i];
    }
    *v++ = x[i];
    *v++ = y[i];
    *v++ = x[i];
    *v++ = linfo->fparams[0];

    if (linfo->fillcolor >= 0) oldcolor = setcolor(linfo->fillcolor);
    filledpoly(2*n+1, verts);
    if (linfo->fillcolor >= 0) setcolor(oldcolor);
    free((void *) verts);
  }

  if (linfo->linecolor >= 0) oldcolor = setcolor(linfo->linecolor);
  moveto(x[0], linfo->fparams[0]);
  for (i = 0; i < stop; i++) {
    lineto(x[i], y[i]);
    lineto(x[i+1], y[i]);
  }
  
  lineto(x[i],y[i]);
  lineto(x[i],linfo->fparams[0]);
  lineto(x[0],linfo->fparams[0]);
  if (linfo->linecolor >= 0) setcolor(oldcolor);
  
  if (linfo->clip >= 0) setclip(oldclip);
  setlwidth(oldwidth);
  setlstyle(oldstyle);
  return 1;
}


int dlgDisjointLines(int n, float *x, float *y, LINE_INFO *linfo)
{
  int i, j, stop = n/2;
  int oldstyle = 0, oldwidth = 0, oldcolor = 0, oldclip;
  
  if (n < 2) return 0;
  oldstyle = setlstyle(linfo->lstyle);
  oldwidth = setlwidth(linfo->lwidth);
  if (linfo->clip >= 0) oldclip = setclip(linfo->clip);

  if (linfo->linecolor >= 0) oldcolor = setcolor(linfo->linecolor);
  for (i = 0, j = 0; i < stop; i++, j+=2) {
    moveto(x[j]+linfo->fparams[2], y[j]);
    lineto(x[j+1]+linfo->fparams[2], y[j+1]);
  }
  if (linfo->linecolor >= 0) setcolor(oldcolor);

  if (linfo->clip >= 0) setclip(oldclip);
  setlwidth(oldwidth);
  setlstyle(oldstyle);

  return 1;
}



/*
 * CREDIT: The bezier functions were taken from the Graphics Gems V
 *         chapter on quick and simple bezier curves
 *   DLS / MAR-98
 */

static void bezierForm(int nctrlpnts, float *x, float *y, float *bx, float *by)
/*   Setup Bezier coefficient array once for each control polygon. */
{
  int i, n, choose;
  n = nctrlpnts-1;
  for(i = 0; i <= n; i++) {
    if (i == 0) choose = 1;
    else if (i == 1) choose = n;
    else choose = choose *(n-i+1)/i;
    bx[i] = x[i]*choose;
    by[i] = y[i]*choose;
  }
}

static void bezierCurve(int nctrlpnts, float *bx, float *by, 
			float *wbx, float *wby,
			float *x, float *y, float t)
/*  Return point (x,y), t <= 0 <= 1 from C, given the number
    of Points in control polygon. BezierForm must be called
    once for any given control polygon. */
{   
  int k, n;
  float t1, tt, u;

  n = nctrlpnts - 1;  
  u = t;
  wbx[0] = bx[0];
  wby[0] = by[0];
  for(k =1; k <=n; k++) {
    wbx[k] = bx[k]*u;
    wby[k] = by[k]*u;
    u =u*t;
  }
  
  *x = wbx[n];  
  *y = wby[n];
  t1 = 1-t;
  tt = t1;
  for(k = n-1; k >=0; k--) {
    *x += wbx[k]*tt;
    *y += wby[k]*tt;
    tt =tt*t1;
  }
}


int dlgBezier(int n, float *x, float *y, LINE_INFO *linfo)
{
  int j, k, m;
  int resolution = linfo->skip?linfo->skip:30;
  float tstep = 1./resolution;
  int nquads = n/4, nverts = (n/2)*(resolution+1);
  int oldstyle = 0, oldwidth = 0, oldcolor = 0, oldclip;
  float bx[4], by[4];		/* For holding coefficients  */
  float wbx[4], wby[4];		/* Working buffers for above */
  float xpos, ypos, t;

  if (n < 2) return 0;

  if (linfo->filled) {
    float *verts = (float *) calloc(nverts, sizeof(float));
    float *v = verts;
    if (!verts) return (0);

    for (k = 0, m = 0; k < nquads; k++, m+=4) {
      bezierForm(4, &x[m], &y[m], bx, by);
      *v++ = x[m];
      *v++ = y[m];
      for (j = 1, t = tstep; j <= resolution; t+=tstep, j++) {
	bezierCurve(4, bx, by, wbx, wby, &xpos, &ypos, t);
	*v++ = xpos;
	*v++ = ypos;
      }
    }

    if (linfo->fillcolor >= 0) oldcolor = setcolor(linfo->fillcolor);
    filledpoly(nverts/2, verts);
    if (linfo->fillcolor >= 0) setcolor(oldcolor);
    free((void *) verts);
  }

  else {
    oldstyle = setlstyle(linfo->lstyle);
    oldwidth = setlwidth(linfo->lwidth);
    if (linfo->clip >= 0) oldclip = setclip(linfo->clip);
    
    if (linfo->linecolor >= 0) oldcolor = setcolor(linfo->linecolor);
    for (k = 0, m = 0; k < nquads; k++, m+=4) {
      bezierForm(4, &x[m], &y[m], bx, by);
      moveto(x[m], y[m]);
      for (j = 1, t = tstep; j <= resolution; t+=tstep, j++) {
	bezierCurve(4, bx, by, wbx, wby, &xpos, &ypos, t);
	lineto(xpos, ypos);
      }
    }
    if (linfo->linecolor >= 0) setcolor(oldcolor);
    
    if (linfo->clip >= 0) setclip(oldclip);
    setlwidth(oldwidth);
    setlstyle(oldstyle);
  }

  return 1;
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclTextDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_text
 *    dlg_intText
 *
 * DESCRIPTION
 *    Draws text at positions specified by list arguments.
 *
 *****************************************************************************/

static int tclTextDynList (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DYN_LIST *dlx, *dly, *dldata, *dltx = NULL, *dlty = NULL, *dltt = NULL;
  DYN_LIST *colors;
  int i, j;
  double size, val;
  int status, dpoints;
  static TEXT_INFO tinfo;
  static char format[48], fontname[64];

  /* 
   * Initialize here, but will change below depending on the actual
   * data to be plotted
   */
  dlgGetTextInfo(DLG_STRINGS, &tinfo);
  format[0] = 0;
  tinfo.format = NULL;

  /* This is a nasty command parsing loop, looking for option pairs */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-' &&
	argv[i][1] != '.' &&
	(argv[i][1] < '0' || argv[i][1] > '9')) {
      if (!strcmp(argv[i],"-font")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no font specified", (char *) NULL);
	  goto error;
	}
	strncpy(fontname, argv[i+1], 63);
	tinfo.font = fontname;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-just")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no justification specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &tinfo.just) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-spacing")) {
	double spacing;
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no line spacing specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetDouble(interp, argv[i+1], &spacing) != TCL_OK) goto error;
	tinfo.spacing = spacing;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-size")) {
	int l;
	float scale = 1.0;
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no size arg", (char *) NULL);
	  goto error;
	}
	l = strlen(argv[i+1])-1;
	if (argv[i+1][l] == 's' || argv[i+1][l] == 'S') {
	  scale = getxscale();
	  argv[i+1][l] = 0;
	}
	if (Tcl_GetDouble(interp, argv[i+1], &size) != TCL_OK)
	  goto error;
	tinfo.size = size*scale;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-color")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no color specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &tinfo.color) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-colors")) {
	if (tclFindDynList(interp, argv[i+1], &colors) != TCL_OK)
	  goto error;
	if (DYN_LIST_DATATYPE(colors) != DF_LONG) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": color list must be ints", (char *) NULL);
	  goto error;
	}
	tinfo.colors = colors;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-ori")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no orientation", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &tinfo.ori) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-dpoints")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": missing decimal points", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &dpoints) != TCL_OK) goto error;
	if (dpoints < 0) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": number of decimal points must be nonnegative", 
			   (char *) NULL);
	  goto error;
	}
	else {
	  sprintf(format, "%%.%df", dpoints);
	}
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else {
	/* Could be a negative number, so try to parse as double */
	double val;
	if (Tcl_GetDouble(interp, argv[i], &val) != TCL_OK) {
	  Tcl_ResetResult(interp);
	  Tcl_AppendResult(interp, argv[0], 
			   ": bad option ", argv[i], (char *) NULL);
	  goto error;
	}
      }
    }
  }

  if (argc < 4) {
    Tcl_AppendResult(interp, "usage:\t", argv[0], 
		     " xlist ylist data [font size just ori dpoints]",
		     "\noptions: -font, -size, -just, -ori,",
		     " -dpoints, -color(s), -spacing",

		     (char *) NULL);
    return TCL_ERROR;
  }
  
  tclFindDynList(interp, argv[1], &dlx);
  tclFindDynList(interp, argv[2], &dly);

  /* 
   * If both are lists and either is a list of length one, while the other is 
   * not, replicate the length one list n times...
   */
  if (dlx && dly && DYN_LIST_N(dlx) && DYN_LIST_N(dly)) {
    DYN_LIST *temp;
    if (DYN_LIST_N(dlx) != DYN_LIST_N(dly)) {
      if (DYN_LIST_N(dlx) == 1) {
	temp = dfuCreateDynList(DYN_LIST_DATATYPE(dlx), DYN_LIST_N(dly));
	for (i = 0; i < DYN_LIST_N(dly); i++) 
	  dynListCopyElement(dlx, 0, temp);
	dlx = temp;
	dltx = dlx;
      }
      else if (DYN_LIST_N(dly) == 1) {
	DYN_LIST *temp;
	temp = dfuCreateDynList(DYN_LIST_DATATYPE(dly), DYN_LIST_N(dlx));
	for (i = 0; i < DYN_LIST_N(dlx); i++) 
	  dynListCopyElement(dly, 0, temp);
	dly = temp;
	dlty = dly;
      }
    }
  }
  
  else if (!dlx && !dly) {		/* both scalars? */
    if (Tcl_GetDouble(interp, argv[1], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": xvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dlx = dfuCreateDynList(DF_FLOAT, 1);
    dfuAddDynListFloat(dlx, val);
    dltx = dlx;

    if (Tcl_GetDouble(interp, argv[2], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": yvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dly = dfuCreateDynList(DF_FLOAT, 1);
    dfuAddDynListFloat(dly, val);
    dlty = dly;
    Tcl_ResetResult(interp);
  }
  else if (!dlx) {
    if (Tcl_GetDouble(interp, argv[1], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": xvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dlx = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dly));
    for (i = 0; i < DYN_LIST_N(dly); i++) 
      dfuAddDynListFloat(dlx, val);
    dltx = dlx;
    Tcl_ResetResult(interp);
  }
  else if (!dly) {
    if (Tcl_GetDouble(interp, argv[2], &val) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": yvalue must be a scalar or a list",
		       (char *) NULL);
      goto error;
    }
    dly = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dlx));
    for (i = 0; i < DYN_LIST_N(dlx); i++) 
      dfuAddDynListFloat(dly, val);
    dlty = dly;
    Tcl_ResetResult(interp);
  }

  if (tclFindDynList(interp, argv[3], &dldata) != TCL_OK) {
    int i;
    int length = DYN_LIST_N(dlx);
    if (DYN_LIST_N(dly) > length) length = DYN_LIST_N(dly);

    Tcl_ResetResult(interp);
    dldata = dfuCreateDynList(DF_STRING, length);
    for (i = 0; i < length; i++) {
      dfuAddDynListString(dldata, argv[3]);
    }
    dltt = dldata;
  }

  /*
   * Now change the tinfo structure to reflect actual data specified
   */
  switch (DYN_LIST_DATATYPE(dldata)) {
  case DF_STRING:
    tinfo.id = DLG_STRINGS;
    tinfo.tfunc = (TEXT_FUNC) dlgTextString;
    tinfo.format = format;
    strcpy(tinfo.format,TextTable[DLG_STRINGS].format);
    break;
  case DF_LONG:
    tinfo.id = DLG_INTS;
    tinfo.tfunc = (TEXT_FUNC) dlgTextInt;
    tinfo.format = format;
    strcpy(tinfo.format,TextTable[DLG_INTS].format);
    break;
  case DF_FLOAT:
    tinfo.id = DLG_FLOATS;
    tinfo.format = format;
    tinfo.tfunc = (TEXT_FUNC) dlgTextFloat;
    if (strcmp(format,"")) {
      if (tinfo.format != format)
	strcpy(tinfo.format,format);
    }
    else {
      strcpy(tinfo.format,TextTable[DLG_FLOATS].format);
    }
    break;
  default:
    Tcl_AppendResult(interp, argv[0], ": cannot draw text for elements of \"",
		     argv[3], "\"", (char *) NULL);
    goto error;
  }

  if (argc > 4) {
    strncpy(fontname, argv[4], 63);
    tinfo.font = fontname;
  }
  
  if (argc > 5) {
    int l = strlen(argv[5])-1;
    float scale = 1.0;
    if (argv[5][l] == 's' || argv[5][l] == 'S') {
      scale = getxscale();
      argv[5][l] = 0;
    }
    if (Tcl_GetDouble(interp, argv[5], &size) != TCL_OK)
      goto error;
    tinfo.size = size*scale;
  }
  
  if (argc > 6) {
    if (Tcl_GetInt(interp, argv[6], &tinfo.just) != TCL_OK)
      goto error;
  }
  
  if (argc > 7) {
    if (Tcl_GetInt(interp, argv[7], &tinfo.ori) != TCL_OK)
      goto error;
  }

  if (argc > 8) {
    if (Tcl_GetInt(interp, argv[8], &dpoints) != TCL_OK)
      goto error;
    if (dpoints < 0) {
      Tcl_AppendResult(interp, argv[0], 
		       ": number of decimal points must be nonnegative", 
		       (char *) NULL);
      goto error;
    }
    else {
      switch (DYN_LIST_DATATYPE(dldata)) {
      case DF_FLOAT:
	tinfo.format = format;
	sprintf(format, "%%.%df", dpoints);
	break;
      }
    }
  }
  
  status = dlgDrawText(dlx, dly, dldata, &tinfo);

  switch (status) {
  case DLG_NOWINDOW:
    Tcl_AppendResult(interp, argv[0], 
		     ": no graphics window open", (char *) NULL);
    goto error;
    break;
  case DLG_BADARGS:
    Tcl_AppendResult(interp, argv[0], 
		     ": bad arguments", (char *) NULL);
    goto error;
    break;
  case DLG_ARGMISMATCH:
    Tcl_AppendResult(interp, argv[0], 
		     ": lists must be same length", (char *) NULL);
    goto error;
    break;
  case DLG_NOMEMORY:
    Tcl_AppendResult(interp, argv[0], 
		     ": unable to allocate memory", (char *) NULL);
    goto error;
    break;
  case DLG_BADCOLORSPEC:
    Tcl_AppendResult(interp, argv[0], 
		     ": color list of incorrect length specified", (char *) NULL);
    goto error;
    break;
  default:
    if (dltx) dfuFreeDynList(dltx);
    if (dlty) dfuFreeDynList(dlty);
    if (dltt) dfuFreeDynList(dltt);
    return TCL_OK;
  }

 error:
  if (dltx) dfuFreeDynList(dltx);
  if (dlty) dfuFreeDynList(dlty);
  if (dltt) dfuFreeDynList(dltt);
  return TCL_ERROR;
}

int dlgDrawText(DYN_LIST *dlx, DYN_LIST *dly, DYN_LIST *data,
		TEXT_INFO *tinfo)
{
/*  if (!getpoint()) return DLG_NOWINDOW; */
  
  switch (DYN_LIST_DATATYPE(dlx)) {
  case DF_STRING:
  case DF_LIST:
    return DLG_BADARGS;
  }
  
  switch (DYN_LIST_DATATYPE(dly)) {
  case DF_STRING:
  case DF_LIST:
    return DLG_BADARGS;
  }

  switch (DYN_LIST_DATATYPE(data)) {
  case DF_STRING:
  case DF_LONG:
  case DF_FLOAT:
    break;
  default:
    return DLG_BADARGS;
  }

  if (DYN_LIST_N(dlx) != DYN_LIST_N(dly) || 
      DYN_LIST_N(dlx) != DYN_LIST_N(data)) {
    return DLG_ARGMISMATCH;
  }

  if (tinfo->colors && (DYN_LIST_N(tinfo->colors) != DYN_LIST_N(dlx))) {
    return DLG_BADCOLORSPEC;
  }

  if (!DYN_LIST_N(dlx)) {
    return DLG_ZEROLIST;
  }

  /* 
   * If lists are both floats, then no float conversion is
   * necessary
   */
  if (DYN_LIST_DATATYPE(dlx) == DF_FLOAT &&
      DYN_LIST_DATATYPE(dly) == DF_FLOAT) {
    float *x = (float *) DYN_LIST_VALS(dlx);
    float *y = (float *) DYN_LIST_VALS(dly);
    (*(tinfo->tfunc))(DYN_LIST_N(dlx), x, y, DYN_LIST_VALS(data), tinfo);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_LONG &&
	   DYN_LIST_DATATYPE(dly) == DF_FLOAT) {
    int i;
    float *x = (float *) calloc(DYN_LIST_N(dlx), sizeof(float));
    int   *xv = (int *) DYN_LIST_VALS(dlx);
    float *y = (float *) DYN_LIST_VALS(dly);

    if (!x) return (DLG_NOMEMORY);
    
    for (i = 0; i < DYN_LIST_N(dlx); i++) {
      x[i] = (float) xv[i];
    }
    
    (*(tinfo->tfunc))(DYN_LIST_N(dlx), x, y, DYN_LIST_VALS(data), tinfo);

    free ((void *) x);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_FLOAT &&
	   DYN_LIST_DATATYPE(dly) == DF_LONG) {
    int i;
    float *x = (float *) DYN_LIST_VALS(dlx);
    float *y = (float *) calloc(DYN_LIST_N(dly), sizeof(float));
    int   *yv = (int *) DYN_LIST_VALS(dly);

    if (!y) return (DLG_NOMEMORY);
    
    for (i = 0; i < DYN_LIST_N(dly); i++) {
      y[i] = (float) yv[i];
    }
    
    (*(tinfo->tfunc))(DYN_LIST_N(dlx), x, y, DYN_LIST_VALS(data), tinfo);

    free ((void *) y);
    return DLG_OK;
  }

  else if (DYN_LIST_DATATYPE(dlx) == DF_LONG &&
	   DYN_LIST_DATATYPE(dly) == DF_LONG) {
    int i;
    float *x = (float *) calloc(DYN_LIST_N(dlx), sizeof(float));
    int   *xv = (int *) DYN_LIST_VALS(dlx);
    float *y = (float *) calloc(DYN_LIST_N(dly), sizeof(float));
    int   *yv = (int *) DYN_LIST_VALS(dly);

    if (!x || !y) return (DLG_NOMEMORY);
    
    for (i = 0; i < DYN_LIST_N(dly); i++) {
      x[i] = (float) xv[i];
      y[i] = (float) yv[i];
    }
    
    (*(tinfo->tfunc))(DYN_LIST_N(dlx), x, y, DYN_LIST_VALS(data), tinfo);

    free ((void *) x);
    free ((void *) y);
    return DLG_OK;
  }
  else return DLG_BADARGS;
}


int dlgTextString(int n, float *x, float *y, char **strings, TEXT_INFO *tinfo)
{
  int i;
  float oldsize;
  int oldjust, oldcolor = 0;
  int *colors, multicolors = 0;
  int oldori, oldclip;
  char oldfont[64], *fontname;
  char msg[128];		/* drawtextf has an arbitrary limit of 128 */

  oldclip = setclip(0);
  oldsize = getfontsize();
  fontname =  getfontname();
  if (fontname) strcpy(oldfont, fontname);
  setfont(tinfo->font, tinfo->size);
  oldjust = setjust(tinfo->just);
  oldori = setorientation(tinfo->ori);
  
  if (!tinfo->colors) {
    if (tinfo->color >= 0) oldcolor = setcolor(tinfo->color);
  } else {
    colors = (int *) DYN_LIST_VALS(tinfo->colors);
    oldcolor = setcolor(0);
    multicolors = 1;
  }
  for (i = 0; i < n; i++) {
    moveto(x[i], y[i]);

    if (multicolors) setcolor(colors[i]);

    /* If no newlines, just print */
    if (strchr(strings[i],'\n') == NULL) {
      strncpy(msg, strings[i], 127);
      drawtextf(tinfo->format, msg);
    }
    
    else {
      char *curstring, *strptr, *curptr, *lastchar;

      curstring = (char *) malloc(strlen(strings[i])+1);
      strcpy(curstring, strings[i]);
      strptr = curstring;
      lastchar = strptr+strlen(curstring);

      while (strptr < lastchar && 
	     (curptr = strchr(strptr, '\n')) != NULL) {
	*curptr = '\0';
	strncpy(msg, strptr, 127);
	drawtextf(tinfo->format, msg);
	strptr = curptr+1;
	switch (tinfo->ori) {
	case 0:	  moverel(0.0, -tinfo->spacing); break;
	case 1:	  moverel(tinfo->spacing, 0.0); break;
	case 2:	  moverel(0.0, tinfo->spacing); break;
	case 3:	  moverel(-tinfo->spacing, 0.0); break;
	}
      }
      if (strptr < lastchar) {
	strncpy(msg, strptr, 127);
	drawtextf(tinfo->format, msg);
      }
      free((void *) curstring);
    }
  }
  if (tinfo->colors || tinfo->color >= 0) setcolor(oldcolor);

  setorientation(oldori);
  setjust(oldjust);
  if (fontname) setfont(oldfont, oldsize);
  setclip(oldclip);

  return 1;
}

int dlgTextInt(int n, float *x, float *y, int *ints, TEXT_INFO *tinfo)
{
  int i;
  float oldsize;
  int oldjust, oldclip;
  int oldori, oldcolor = 0;
  int *colors, multicolors = 0;
  char oldfont[64], *fontname;
  
  oldclip = setclip(0);
  oldsize = getfontsize();
  fontname = getfontname();
  if (fontname) strcpy(oldfont, fontname);

  setfont(tinfo->font, tinfo->size);
  oldjust = setjust(tinfo->just);
  oldori = setorientation(tinfo->ori);

  if (!tinfo->colors) {
    if (tinfo->color >= 0) oldcolor = setcolor(tinfo->color);
  } else {
    colors = (int *) DYN_LIST_VALS(tinfo->colors);
    oldcolor = setcolor(0);
    multicolors = 1;
  }

  for (i = 0; i < n; i++) {
    moveto(x[i], y[i]);
    if (multicolors) setcolor(colors[i]);
    drawtextf(tinfo->format, ints[i]);
  }
  
  if (tinfo->colors || tinfo->color >= 0) setcolor(oldcolor);
  setorientation(oldori);
  setjust(oldjust);
  if (fontname) setfont(oldfont, oldsize);
  setclip(oldclip);
  
  return 1;
}

int dlgTextFloat(int n, float *x, float *y, float *floats, TEXT_INFO *tinfo)
{
  int i;
  float oldsize;
  int oldjust, oldclip;
  int *colors, multicolors = 0;
  int oldori, oldcolor = 0;
  char oldfont[64], *fontname;
  
  oldclip = setclip(0);
  oldsize = getfontsize();
  fontname = getfontname();
  if (fontname) strcpy(oldfont, fontname);
    
  setfont(tinfo->font, tinfo->size);
  oldjust = setjust(tinfo->just);
  oldori = setorientation(tinfo->ori);
  
  if (!tinfo->colors) {
    if (tinfo->color >= 0) oldcolor = setcolor(tinfo->color);
  } else {
    colors = (int *) DYN_LIST_VALS(tinfo->colors);
    oldcolor = setcolor(0);
    multicolors = 1;
  }

  for (i = 0; i < n; i++) {
    moveto(x[i], y[i]);
    if (multicolors) setcolor(colors[i]);
    drawtextf(tinfo->format, floats[i]);
  }
  
  if (tinfo->colors || tinfo->color >= 0) setcolor(oldcolor);
  setorientation(oldori);
  setjust(oldjust);
  if (fontname) setfont(oldfont, oldsize);
  setclip(oldclip);

  return 1;
}



/************************************************************************/
/*                           Helper Functions                           */
/************************************************************************/

static int dlgGetMarkerInfo(char *tagname, MARKER_INFO *minfo)
{
  int i, ntags = sizeof(MarkerTable)/sizeof(MARKER_INFO);
  
  for (i = 0; i < ntags; i++) {
    if (!strcasecmp(tagname, MarkerTable[i].name)) {
      minfo->mfunc = MarkerTable[i].mfunc;
      minfo->name = MarkerTable[i].name;
      minfo->id = MarkerTable[i].id;
      minfo->msize = MarkerTable[i].msize;
      minfo->color = MarkerTable[i].color;
      minfo->lwidth = MarkerTable[i].lwidth;
      minfo->clip = MarkerTable[i].clip;
      return(1);
    }
  }
  return(0);
}

static int dlgGetLineInfo(int i, LINE_INFO *linfo)
{
  int ntags = sizeof(LineTable)/sizeof(LINE_INFO);
  
  if (i >= ntags) return 0;
  memcpy(linfo, (char *) &LineTable[i], sizeof(LINE_INFO));
  return(1);
}

static int dlgGetTextInfo(int i, TEXT_INFO *tinfo)
{
  int ntags = sizeof(TextTable)/sizeof(TEXT_INFO);
  
  if (i >= ntags) return 0;
  memcpy(tinfo, (char *) &TextTable[i], sizeof(TEXT_INFO));
  return(1);
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclPostscriptDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_postscript
 *
 * DESCRIPTION
 *    Place an ps/eps file at x,y scaled by sx,sy
 *
 *****************************************************************************/

static int tclPostscriptDynList (ClientData data, Tcl_Interp *interp,
				 int argc, char *argv[])
{
  int i, j;
  char *psfile;
  double x, y, width, height = 0.0; /* height is optional            */
  int lwidth = 0;		/* in 100's of a point (like above)  */
  int lcolor = -1;
  int nops = 0;
  int center = 0;
  int w, h, d;
  int do_place_image = 0;
  char *filename;
  int header_bytes = 0;

  /* This is a command parsing loop, looking for option pairs */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i],"-lwidth")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no frame width specified", (char *) NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[i+1], &lwidth) != TCL_OK) return TCL_ERROR;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      if (!strcmp(argv[i],"-lcolor")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no frame color specified", (char *) NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[i+1], &lcolor) != TCL_OK) return TCL_ERROR;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      if (!strcmp(argv[i],"-center")) {
	center = 1;
	for (j = i+1; j < argc; j++) argv[j-1] = argv[j];
	argc-=1;
      }
      if (!strcmp(argv[i],"-nops")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": invalid nops arg", (char *) NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[i+1], &nops) != TCL_OK) return TCL_ERROR;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
    }
  }

  if (argc < 5) {
    Tcl_AppendResult(interp, "usage:\t", argv[0], " x y psfile width [height]",
		     "[-lwidth (0)]", (char *) NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  psfile = argv[3];
  if (Tcl_GetDouble(interp, argv[4], &width) != TCL_OK) return TCL_ERROR;

  if (argc > 5) {
    if (Tcl_GetDouble(interp, argv[5], &height) != TCL_OK) return TCL_ERROR;
  }

  if (height == 0.0) {		/* set automatically */
    int status;
    float bbaspect = 1.0, aspect = getuaspect();
    double bblx, bbly, bbux, bbuy;

    if (strstr(psfile, ".raw")) {
      filename = psfile;
      do_place_image = 1;
      w = h = d = 0;
      if (!raw_getImageDims(psfile, &w, &h, &d, &header_bytes)) bbaspect = 1.;
      else bbaspect = w/(float) h;

      height = width*aspect/(bbaspect);
    }
    else {
      /* Get aspect ratio from ps file if available */
      status = getPSBB(interp, psfile, &bblx, &bbly, &bbux, &bbuy);
      if (status) bbaspect = (bbux-bblx)/(bbuy-bbly);
      
      /* Now compute appropriate height to maintain aspect ratios */
      height = width*aspect/(bbaspect);
    }
  }

  if (center) {
    x-=width/2.0;
    y-=height/2.0;
  }

  if (do_place_image) {
    FILE *infp;
    unsigned char *imgdata;
    unsigned int nbytes = w*h*d;
    infp = fopen(filename, "rb");
    if (!infp) {
      Tcl_AppendResult(interp, "error opening file \"", filename, "\"", NULL);
      return TCL_ERROR;
    }
    if (header_bytes) fseek(infp, header_bytes, SEEK_SET);
    imgdata = (unsigned char *) calloc(nbytes, 1);
    if (!imgdata) {
      Tcl_AppendResult(interp, "error allocating space for image \"", 
		       filename, "\"", NULL);
      fclose(infp);
      return TCL_ERROR;
    }
    if (fread(imgdata, 1, nbytes, infp) != nbytes) {
      Tcl_AppendResult(interp, "error reading data from file \"", 
		       filename, "\"", NULL);
      free(imgdata);
      fclose(infp);
      return TCL_ERROR;
    }
    fclose(infp);
    moveto(x, y);
    place_image(w, h, d, imgdata, width, height);
    free(imgdata);
  }
  else if (!nops) {
    moveto(x, y);
    postscript(psfile, width, height);
  }

  /* add border if specified lwidth */
  if (lwidth) {
    int oldclip;
    int oldcolor = -1, oldwidth;
    if (lcolor >= 0) oldcolor = setcolor(lcolor);
    oldwidth = setlwidth(lwidth);

    oldclip = setclip(0);
    moveto(x, y);
    lineto(x+width, y);
    lineto(x+width, y+height);
    lineto(x, y+height);
    lineto(x, y);
    setclip(oldclip);
    
    if (oldcolor >= 0) setcolor(oldcolor);
    setlwidth(oldwidth);
  }

  sprintf(interp->result, "%.3f", height); /* return the height */

  return TCL_OK;
}

static int tclFindImageData(Tcl_Interp *interp, char *name, 
			    int *w, int *h, int *d, unsigned char **data)
{
  DYN_LIST *dl;
  DYN_LIST **sublists;
  int temp;
  if (tclFindDynList(interp, name, &dl) != TCL_OK) return TCL_ERROR;

  if (*w && *h) {		/* user specified the dims */
    int size = (*w) * (*h);
    if (DYN_LIST_DATATYPE(dl) != DF_CHAR) {
      Tcl_AppendResult(interp, "dynlist images must be chars", NULL);
      return TCL_ERROR;
    }
    if (DYN_LIST_N(dl) % size) {
      Tcl_AppendResult(interp, "find image: invalid size specified", NULL);
      return TCL_ERROR;
    }
    else *d = DYN_LIST_N(dl)/size;
    *data = (unsigned char *) DYN_LIST_VALS(dl);
    return TCL_OK;
  }
  

  if (DYN_LIST_DATATYPE(dl) != DF_LIST || DYN_LIST_N(dl) != 2) {
    Tcl_AppendResult(interp, "dynlist images must contain header/data", NULL);
    return TCL_ERROR;
  }

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(sublists[0]) != DF_LONG ||
      DYN_LIST_N(sublists[0]) != 2) {
    Tcl_AppendResult(interp, "image header must include w & h (ints)", NULL);
    return TCL_ERROR;
  }
  else {
    int *header = (int *) DYN_LIST_VALS(sublists[0]);
    *w = header[0];
    *h = header[1];
  }

  if (DYN_LIST_DATATYPE(sublists[1]) != DF_CHAR) {
    Tcl_AppendResult(interp, "image data must be chars", NULL);
    return TCL_ERROR;
  }
  
  temp = DYN_LIST_N(sublists[1])/((*w)*(*h));

  switch (temp) {
  case 1:
  case 3:
  case 4:
    if ((temp*(*w)*(*h)) != DYN_LIST_N(sublists[1])) {
      char sizes[64];
      sprintf(sizes, "[%dx%dx%d != %d]", *w, *h, temp,DYN_LIST_N(sublists[1]));
      Tcl_AppendResult(interp, "image data size mismatch ", sizes, NULL);
      return TCL_ERROR;
    }
    *d = temp;
    *data = (unsigned char *) DYN_LIST_VALS(sublists[1]);
    return TCL_OK;
    break;
  default:
    {
      char sizes[64];
      sprintf(sizes, "[w=%d, h=%d, n=%d]", *w, *h, DYN_LIST_N(sublists[1]));
      Tcl_AppendResult(interp, "image data size mismatch ", sizes, NULL);
      return TCL_ERROR;
    }
  }
}
/*****************************************************************************
 *
 * FUNCTION
 *    tclImagePlace
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_image
 *
 * DESCRIPTION
 *    Place an image (raw file or dynlist+h,w,d)
 *
 *****************************************************************************/

static int tclImagePlace (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  int i, j;
  double x, y, width, height = 0.0; /* height is optional            */
  int lwidth = 0;		/* in 100's of a point (like above)  */
  int lcolor = -1;
  int oldclip;
  int center = 0;
  int w = 0;
  int h = 0;
  int d = 0;
  int ref;
  int noclip = 0;
  unsigned char *imagedata;

  /* Call postscript function for raw files */
  if (argc > 3 && strstr(argv[3], ".raw"))
    return tclPostscriptDynList(data, interp, argc, argv);

  /* This is a command parsing loop, looking for option pairs */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i],"-lwidth")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no frame width specified", (char *) NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[i+1], &lwidth) != TCL_OK) return TCL_ERROR;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      if (!strcmp(argv[i],"-width")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no image width specified", (char *) NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[i+1], &w) != TCL_OK) return TCL_ERROR;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      if (!strcmp(argv[i],"-height")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no image height specified", (char *) NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[i+1], &h) != TCL_OK) return TCL_ERROR;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      if (!strcmp(argv[i],"-lcolor")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no frame color specified", (char *) NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[i+1], &lcolor) != TCL_OK) return TCL_ERROR;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      if (!strcmp(argv[i],"-center")) {
	center = 1;
	for (j = i+1; j < argc; j++) argv[j-1] = argv[j];
	argc-=1;
      }
      if (!strcmp(argv[i],"-noclip")) {
	noclip = 1;
	for (j = i+1; j < argc; j++) argv[j-1] = argv[j];
	argc-=1;
      }
    }
  }

  if (argc < 5) {
    Tcl_AppendResult(interp, "usage:\t", argv[0], " x y image width [height]",
		     "[-lwidth (0)]", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;

  if (tclFindImageData(interp, argv[3], &w, &h, &d, &imagedata) != TCL_OK) 
    return TCL_ERROR;

  if (Tcl_GetDouble(interp, argv[4], &width) != TCL_OK) return TCL_ERROR;

  if (argc > 5) {
    if (Tcl_GetDouble(interp, argv[5], &height) != TCL_OK) return TCL_ERROR;
  }

  if (height == 0.0) {		/* set automatically */
    float bbaspect, aspect = getuaspect();
    bbaspect = w/(float) h;
    height = width*aspect/(bbaspect);
  }

  if (center) {
    x-=width/2.0;
    y-=height/2.0;
  }
  
  moveto(x, y);
  if (noclip) oldclip = setclip(0);
  ref = place_image(w, h, d, imagedata, width, height);
  if (noclip) setclip(oldclip);

  /* add border if specified lwidth */
  if (lwidth) {
    int oldcolor = -1, oldwidth;
    if (lcolor >= 0) oldcolor = setcolor(lcolor);
    oldwidth = setlwidth(lwidth);

    oldclip = setclip(0);
    moveto(x, y);
    lineto(x+width, y);
    lineto(x+width, y+height);
    lineto(x, y+height);
    lineto(x, y);
    setclip(oldclip);
    
    if (oldcolor >= 0) setcolor(oldcolor);
    setlwidth(oldwidth);
  }

  sprintf(interp->result, "%d %.4f %.4f", ref, width, height);

  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclImageReplace
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_replaceImage
 *
 * DESCRIPTION
 *    Replace image data
 *
 *****************************************************************************/

static int tclImageReplace (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  int i, j, ref;
  int w = 0;
  int h = 0;
  int d = 0;
  unsigned char *imagedata;

  /* Parse options */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i],"-width")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no image width specified", (char *) NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[i+1], &w) != TCL_OK) return TCL_ERROR;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      if (!strcmp(argv[i],"-height")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no image height specified", (char *) NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[i+1], &h) != TCL_OK) return TCL_ERROR;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
    }
  }

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage:\t", argv[0], " imgref image", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetInt(interp, argv[1], &ref) != TCL_OK) return TCL_ERROR;

  if (tclFindImageData(interp, argv[2], &w, &h, &d, &imagedata) != TCL_OK) 
    return TCL_ERROR;

  replace_image(ref, w, h, d, imagedata);
  return TCL_OK;
}

static int getPSBB(Tcl_Interp *interp, char *filename, 
		   double *bblx, double *bbly, double *bbux, double *bbuy) 
{
  FILE *ifp;
  char inbuff[256], *s;

  if (!(ifp = fopen(filename, "r"))) return 0;
  while (!feof(ifp)) {
    if (fgets(inbuff, 250, ifp) != NULL) {
      if (strncmp(inbuff,"%%BoundingBox:", 14) == 0) {
	s = strtok(inbuff, " :\t");
	if (Tcl_GetDouble(interp, strtok(0, " :\t"), bblx) != TCL_OK) 
	  goto error;
	if (Tcl_GetDouble(interp, strtok(0, " :\t"), bbly) != TCL_OK) 
	  goto error;
	if (Tcl_GetDouble(interp, strtok(0, " :\t"), bbux) != TCL_OK) 
	  goto error;
	if (Tcl_GetDouble(interp, strtok(0, " :\t"), bbuy) != TCL_OK) 
	  goto error;
	break;
      }
    }
  }
  fclose(ifp);
  return 1;

 error:
  Tcl_ResetResult(interp);
  fclose(ifp);
  return 0;
}


/*****************************************************************************
 *
 * FUNCTIONS
 *    tclLabel
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_loose_label
 *    dlg_tight_label
 *
 * DESCRIPTION
 *    Find good labels for graphs
 *
 *****************************************************************************/

static int tclLabels (ClientData data, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  int loose = (int) data;
  double min, max, t;
  int nticks = 5;
  int reverse = 0;
  float range, d, graphmin, graphmax, nfrac, x;
  float upper, lower;
  char fmt[16], numstr[32];
  Tcl_DString retList;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " min max [nticks]", (char *) NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetDouble(interp, argv[1], &min) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &max) != TCL_OK) return TCL_ERROR;
  if (argc > 3) {
    if (Tcl_GetInt(interp, argv[3], &nticks) != TCL_OK) return TCL_ERROR;
    if (nticks < 1) {
      Tcl_AppendResult(interp, argv[0], 
		       ": invalid number of ticks specified", (char *) NULL);
      return TCL_ERROR;
    }
  }

  /* Set reverse flag if backwards */
  if (min > max) {
    reverse = 1;
    t = min;
    min = max;
    max = t;
  }

  else if (min == max) {	/*  very boring  */
    Tcl_SetResult(interp, argv[1], TCL_STATIC);
    return TCL_OK;
  }

  range = nicenum(max-min, 0);
  d = nicenum(range/(nticks-1.0), 1);
  graphmin = floor(min/d)*d;
  graphmax = ceil(max/d)*d;
  nfrac = -floor(log10(d));
  if (nfrac < 0) nfrac = 0.0;
  sprintf(fmt, "%%.%df", (int) nfrac);
  
  upper = graphmax+0.5*d;
  lower = graphmin-0.5*d;

  Tcl_DStringInit(&retList);

  if (loose) {
    if (!reverse) {
      for (x = graphmin; x < upper; x += d) {
	sprintf(numstr, fmt, x);
	Tcl_DStringAppendElement(&retList, numstr);
      }
    }
    else {
      for (x = graphmax; x > lower; x -= d) {
	sprintf(numstr, fmt, x);
	Tcl_DStringAppendElement(&retList, numstr);
      }
    }
  }
  else {			/* tight - don't go over/under */
    if (!reverse) {
      for (x = graphmin; x < upper; x += d) {
	if (x >= min && x <= max) {
	  sprintf(numstr, fmt, x);
	  Tcl_DStringAppendElement(&retList, numstr);
	}
      }
    }
    else {
      for (x = graphmax; x > lower; x -= d) {
	if (x >= min && x <= max) {
	  sprintf(numstr, fmt, x);
	  Tcl_DStringAppendElement(&retList, numstr);
	}
      }
    }
  }
  Tcl_DStringResult(interp, &retList);
  return TCL_OK;
}



/*****************************************************************************
 *
 * FUNCTIONS
 *    tclNiceNum
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_nicenum
 *
 * DESCRIPTION
 *    Return a "nice" number for graphs
 *
 *****************************************************************************/

static int tclNiceNum (ClientData data, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  double x;
  float retval;
  int round;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " x round", (char *) NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &round) != TCL_OK) return TCL_ERROR;

  retval = nicenum(x, round);
  sprintf(interp->result, "%f", retval);
  return TCL_OK;
}


static float nicenum(float x, int round) {
  double e, f, nf;
  float neg_mult = 1;

  if (x == 0) return 0;
  
  if (x < 0.0) {
    neg_mult = -1;
    x = -x;
  }

  e = floor(log10(x));
  f = x/pow(10, e);
  if (round) {
    if (f < 1.5) nf = 1.0;
    else if (f < 3.) nf = 2.0;
    else if (f < 7.) nf = 5.0;
    else nf = 10.0;
  }
  else {
    if (f <= 1.) nf = 1.0;
    else if (f <= 2.) nf = 2.0;
    else if (f <= 5.) nf = 5.0;
    else nf = 10.0;
  }
  return (neg_mult*nf*pow(10.0, e));
}


/*****************************************************************************
 *
 * FUNCTIONS
 *    tclNiceDpoints
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_nice_dpoints
 *
 * DESCRIPTION
 *    Return a reasonable number of decimal points to print out the numbers
 * in the specified list
 *
 *****************************************************************************/

static int tclNiceDpoints (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DYN_LIST *dl;
  int dpoints, i;
  float range, *vals;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " dynlist", (char *) NULL);
    return TCL_ERROR;
  }
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;

  if (DYN_LIST_N(dl) < 2 || DYN_LIST_DATATYPE(dl) != DF_FLOAT) dpoints = 0;
  else {
    vals = (float *) DYN_LIST_VALS(dl);
    range = vals[1]-vals[0];
    if (range < 0) range = -range;
    dpoints = -floor(log10(range));
    if (dpoints < 0) dpoints = 0;

    /* If they are small, but not all ints, add one decimal point */
    if (dpoints == 0) {
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (vals[i] != 0. && (int) vals[i] != vals[i]) {
	  dpoints = 1;
	  break;
	}
      }
    }
  }
  sprintf(interp->result, "%d", dpoints);
  return TCL_OK;
}


/* Heatmaps */
static DYN_LIST *create_colormap(float *r, float *g, float *b, int n)
{
  DYN_LIST *dl = NULL, *c;
  float *cvals;
  
  dl = dfuCreateDynList(DF_LIST, 3);

  cvals = (float *) calloc(n, sizeof(float));
  memcpy(cvals, r, sizeof(float)*n);
  c = dfuCreateDynListWithVals(DF_FLOAT, n, cvals);
  dfuMoveDynListList(dl, c);

  cvals = (float *) calloc(n, sizeof(float));
  memcpy(cvals, g, sizeof(float)*n);
  c = dfuCreateDynListWithVals(DF_FLOAT, n, cvals);
  dfuMoveDynListList(dl, c);

  cvals = (float *) calloc(n, sizeof(float));
  memcpy(cvals, b, sizeof(float)*n);
  c = dfuCreateDynListWithVals(DF_FLOAT, n, cvals);
  dfuMoveDynListList(dl, c);

  return dl;
}
static int tclHeatmap (ClientData data, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  DYN_LIST *newlist = NULL;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " mapname", (char *) NULL);
    return TCL_ERROR;
  }
  if (!strcmp(argv[1], "jet") || !strcmp(argv[1], "JET")) {
    newlist = create_colormap(heatmap_jet_r, heatmap_jet_g, heatmap_jet_b,
			      sizeof(heatmap_jet_r)/sizeof(float));
  }
  else if (!strcmp(argv[1], "hot") || !strcmp(argv[1], "HOT")) {
    newlist = create_colormap(heatmap_hot_r, heatmap_hot_g, heatmap_hot_b,
			      sizeof(heatmap_hot_r)/sizeof(float));
  }
  if (newlist) 
    return(tclPutList(interp, newlist));

  
  else {
    Tcl_AppendResult(interp, argv[0], " mapname \"", argv[1],
		     "\" not found", (char *) NULL);
    return TCL_ERROR;
  }
}


/***** COLORSPACE Support ******/

#define DEG2RAD(x) (.01745329251994329576*(x))
#define RAD2DEG(x) (57.29577951308232087721*(x))


/* Copyright 2005, Ross Ihaka. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 *    3. The name of the Ross Ihaka may not be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ROSS IHAKA BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 */

/* ----- CIE-XYZ <-> Device dependent RGB -----
 *
 *  Gamma Correction
 *
 *  The following two functions provide gamma correction which
 *  can be used to switch between sRGB and linearized sRGB (RGB).
 *
 *  The standard value of gamma for sRGB displays is approximately 2.2,
 *  but more accurately is a combination of a linear transform and
 *  a power transform with exponent 2.4
 *
 *  gtrans maps linearized sRGB to sRGB.
 *  ftrans provides the inverse map.
 *
 */

static double gtrans(double u, double gamma)
{
    if (u > 0.00304)
	return 1.055 * pow(u, (1 / gamma)) - 0.055;
    else
	return 12.92 * u;
}

static double ftrans(double u, double gamma)
{
    if (u > 0.03928)
	return pow((u + 0.055) / 1.055, gamma);
    else
	return u / 12.92;
}

static void DEVRGB_to_RGB(double R, double G, double B, double gamma,
			 double *r, double *g, double *b)
{
    *r = ftrans(R, gamma);
    *g = ftrans(G, gamma);
    *b = ftrans(B, gamma);
}

static void RGB_to_DEVRGB(double R, double G, double B, double gamma,
			 double *r, double *g, double *b)
{
    *r = gtrans(R, gamma);
    *g = gtrans(G, gamma);
    *b = gtrans(B, gamma);
}

/* ----- CIE-XYZ <-> Device independent RGB -----
 *
 *  R, G, and B give the levels of red, green and blue as values
 *  in the interval [0,1].  X, Y and Z give the CIE chromaticies.
 *  XN, YN, ZN gives the chromaticity of the white point.
 *
 */

static void RGB_to_XYZ(double R, double G, double B,
		      double XN, double YN, double ZN,
		      double *X, double *Y, double *Z)
{
    *X = YN * (0.412453 * R + 0.357580 * G + 0.180423 * B);
    *Y = YN * (0.212671 * R + 0.715160 * G + 0.072169 * B);
    *Z = YN * (0.019334 * R + 0.119193 * G + 0.950227 * B);
}

static void XYZ_to_RGB(double X, double Y, double Z,
		      double XN, double YN, double ZN,
		      double *R, double *G, double *B)
{
    *R = ( 3.240479 * X - 1.537150 * Y - 0.498535 * Z) / YN;
    *G = (-0.969256 * X + 1.875992 * Y + 0.041556 * Z) / YN;
    *B = ( 0.055648 * X - 0.204043 * Y + 1.057311 * Z) / YN;
}

/* ----- CIE-XYZ <-> sRGB -----
 *
 *  R, G, and B give the levels of red, green and blue as values
 *  in the interval [0,1].  X, Y and Z give the CIE chromaticies.
 *  XN, YN, ZN gives the chromaticity of the white point.
 *
 */

static void sRGB_to_XYZ(double R, double G, double B,
                        double XN, double YN, double ZN,
                        double *X, double *Y, double *Z)
{
    double r, g, b;
    r = ftrans(R, 2.4);
    g = ftrans(G, 2.4);
    b = ftrans(B, 2.4);
    *X = YN * (0.412453 * r + 0.357580 * g + 0.180423 * b);
    *Y = YN * (0.212671 * r + 0.715160 * g + 0.072169 * b);
    *Z = YN * (0.019334 * r + 0.119193 * g + 0.950227 * b);
}

static void XYZ_to_sRGB(double X, double Y, double Z,
                        double XN, double YN, double ZN,
                        double *R, double *G, double *B)
{
    *R = gtrans(( 3.240479 * X - 1.537150 * Y - 0.498535 * Z) / YN, 2.4);
    *G = gtrans((-0.969256 * X + 1.875992 * Y + 0.041556 * Z) / YN, 2.4);
    *B = gtrans(( 0.055648 * X - 0.204043 * Y + 1.057311 * Z) / YN, 2.4);
}


/* ----- CIE-XYZ <-> CIE-LAB ----- */

/* UNUSED ?
static double g(double t)
{
    return (t > 7.999625) ? pow(t, 3) : (t - 16.0 / 116.0) / 7.787;
}
*/

static void LAB_to_XYZ(double L, double A, double B,
		      double XN, double YN, double ZN,
		      double *X, double *Y, double *Z)
{
    double fx, fy, fz;

    if (L <= 0)
	*Y = 0.0;
    else if (L <= 8.0)
	*Y = L * YN / 903.3;
    else if (L <= 100) 
	*Y = YN * pow((L + 16) / 116, 3);
    else
	*Y = YN;
    
    /* CHECKME - IS 0.00856 CORRECT???!!! */
    if (*Y <= 0.00856 * YN)
	fy = 7.787 * *Y / YN + 16.0 / 116.0;
    else
	fy = pow(*Y / YN, 1.0/3.0);
    
    fx = fy + (A / 500.0);
    if (pow(fx, 3) <= 0.008856)
	*X = XN * (fx - 16.0 / 116.0) / 7.787;
    else
	*X = XN * pow(fx, 3);
    
    fz = fy - (B / 200.0);
    if (pow(fz, 3) <= 0.008856)
	*Z = ZN * (fz - 16.0 / 116.0) / 7.787;
    else
	*Z = ZN * pow(fz, 3);
}

static double f(double t)
{
    return (t > 0.008856) ? pow(t, 1.0/3.0) : 7.787 * t + 16.0/116.0;
} 

static void XYZ_to_LAB(double X, double Y, double Z,
		      double XN, double YN, double ZN,
		      double *L, double *A, double *B)
{
    double xr, yr, zr, xt, yt ,zt;
    xr = X / XN;
    yr = Y / YN;
    zr = Z / ZN;
    if (yr > 0.008856)
	*L = 116.0 * pow(yr, 1.0/3.0) - 16.0;
    else
	*L = 903.3 * yr;
    xt = f(xr);
    yt = f(yr);
    zt = f(zr);
    *A = 500.0 * (xt - yt);
    *B = 200.0 * (yt - zt);
}


/* ----- CIE-XYZ <-> Hunter LAB -----
 *
 *  Hunter LAB is no longer part of the public API, but the code
 *  is still here in case it is needed.

static void XYZ_to_HLAB(double X, double Y, double Z,
                        double XN, double YN, double ZN,
                        double *L, double *A, double *B)
{
    X = X / XN;
    Y = Y / YN;
    Z = Z / ZN;
    *L = sqrt(Y);
    *A = 17.5 * (((1.02 * X) - Y) / *L);
    *B = 7 * ((Y - (0.847 * Z)) / *L);
    *L = 10 * *L;
}

static void HLAB_to_XYZ(double L, double A, double B,
                        double XN, double YN, double ZN,
                        double *X, double *Y, double *Z)
{
    double vX, vY, vZ;
    vY = L / 10;

    vX = (A / 17.5) * (L / 10);
    vZ = (B / 7) * (L / 10);
    vY = vY * vY;

    *Y = vY;
    *X = (vX + vY) / 1.02;
    *Z = -(vZ - vY) / 0.847;

    *X = *X * XN;
    *Y = *Y * YN;
    *Z = *Z * ZN;
}

 *
 */

/* ----- LAB <-> polarLAB ----- */

static void LAB_to_polarLAB(double L, double A, double B,
                            double *l, double *c, double *h)
{
    double vH;
    vH = RAD2DEG(atan2(B, A));
    while (vH > 360) vH -= 360;
    while (vH < 0) vH += 360;
    *l = L;
    *c = sqrt(A * A + B * B);
    *h = vH;
}

static void polarLAB_to_LAB(double L, double C, double H,
			   double *l, double *a, double *b)
{
    *l = L;
    *a = cos(DEG2RAD(H)) * C;
    *b = sin(DEG2RAD(H)) * C;
}

/* ----- RGB <-> HSV ----- */

static double max3(double a, double b, double c)
{
    if (a < b) a = b;
    if (a < c) a = c;
    return a;
}

static double min3(double a, double b, double c)
{
    if (a > b) a = b;
    if (a > c) a = c;
    return a;
}

static void RGB_to_HSV(double r, double g, double b,
		      double *h, double *s, double *v)
{
    double y, x, f;
    int i;
    x = min3(r, g, b);
    y = max3(r, g, b);
    if (y != x) {
	f = (r == x) ? g - b : ((g == x) ? b - r : r - g);
	i = (r == x) ? 3 : ((g == x) ? 5 : 1);
	*h = 60 * (i - f /(y - x));
	*s = (y - x)/y;
	*v = y;
    }
    else {
#ifdef MONO
	*h = NAN; *s = 0; *v = y;
#else
	*h = 0; *s = 0; *v = y;
#endif
    }
}

#define RETURN_RGB(red,green,blue) *r=red;*g=green;*b=blue;break;

static void HSV_to_RGB(double h, double s, double v,
                       double *r, double *g, double *b)
{
    double m, n, f;
    int i;
#ifndef WIN32
    if (h == NAN) {
	*r = v; *g = v; *b = v;
    }
    else {
#endif
	h = h /60;		/* convert to [0, 6] */
	i = floor(h);
	f = h - i;
	if(!(i & 1))	/* if i is even */
	    f = 1 - f;
	m = v * (1 - s);
	n = v * (1 - s * f);
	switch (i) {
	case 6:
	case 0: RETURN_RGB(v, n, m);
	case 1: RETURN_RGB(n, v, m);
	case 2: RETURN_RGB(m, v, n);
	case 3: RETURN_RGB(m, n, v);
	case 4: RETURN_RGB(n, m, v);
	case 5: RETURN_RGB(v, m, n);
	}
#ifndef WIN32
    }
#endif
}

/* 
 * rgb all in [0,1] 
 * h in [0, 360], ls in [0,1]
 *
 * From:
 * http://wiki.beyondunreal.com/wiki/RGB_To_HLS_Conversion
 */
static void RGB_to_HLS(double r, double g, double b,
                       double *h, double *l, double *s)
{
    double min, max;

    min = min3(r, g, b);
    max = max3(r, g, b);

    *l = (max + min)/2;

    if (max != min) {
        if (*l <  0.5)  
            *s = (max - min)/(max + min);
        if (*l >= 0.5)  
            *s = (max - min)/(2.0 - max - min);

        if (r == max) 
            *h = (g - b)/(max - min);
        if (g == max) 
            *h = 2.0 + (b - r)/(max - min);
        if (b == max) 
            *h = 4.0 + (r - g)/(max - min);

        *h = *h * 60;
        if (*h < 0) 
            *h = *h + 360;
        if (*h > 360) 
            *h = *h - 360;
    } else {
        *s = 0;
#ifdef MONO
	*h = NAN; 
#else
	*h = 0;
#endif
    }
}

static double qtrans(double q1, double q2, double hue) {
#ifndef WIN32
  double result = NAN;
#else
  double result = 0;
#endif

    if (hue > 360) 
        hue = hue - 360;
    if (hue < 0) 
        hue = hue + 360;

    if (hue < 60) 
        result = q1 + (q2 - q1) * hue / 60;
    else if (hue < 180)
        result = q2;
    else if (hue < 240) 
        result = q1 + (q2 - q1) * (240 - hue) / 60;
    else 
        result = q1;
    return result;
}

static void HLS_to_RGB(double h, double l, double s,
                       double *r, double *g, double *b)
{  
#ifndef WIN32
    double p1 = NAN;
    double p2 = NAN;
#else
    double p1 = 0;
    double p2 = 0;
#endif
    if (l <= 0.5) 
        p2 = l * (1 + s); 
    else 
        p2 = l + s - (l * s);
    p1 = 2 * l - p2;

    if (s == 0) {
        *r = l;
        *g = l;
        *b = l;
    } else {
        *r = qtrans(p1, p2, h + 120);
        *g = qtrans(p1, p2, h);
        *b = qtrans(p1, p2, h - 120);
    }
}

/* ----- CIE-XYZ <-> CIE-LUV ----- */

static void XYZ_to_uv(double X, double Y, double Z, double *u, double *v)
{
    double t, x, y;
    t = X + Y + Z;
    x = X / t;
    y = Y / t;
    *u = 2 * x / (6 * y - x + 1.5);
    *v = 4.5 * y / (6 * y - x + 1.5);
}

static void XYZ_to_LUV(double X, double Y, double Z,
                       double XN, double YN, double ZN,
                       double *L, double *U, double *V)
{
    double u, v, uN, vN, y;
    XYZ_to_uv(X, Y, Z, &u, &v);
    XYZ_to_uv(XN, YN, ZN, &uN, &vN);
    y = Y / YN;
    *L = (y > 0.008856) ? 116 * pow(y, 1.0/3.0) - 16 : 903.3 * y;
    *U = 13 * *L * (u - uN);
    *V = 13 * *L * (v - vN);
}

static void LUV_to_XYZ(double L, double U, double V,
                       double XN, double YN, double ZN,
                       double *X, double *Y, double *Z)
{
    double u, v, uN, vN;
    if (L <= 0 && U == 0 && V == 0) {
	*X = 0; *Y = 0; *Z = 0;
    }
    else {
	*Y = YN * ((L > 7.999592) ? pow((L + 16)/116, 3) : L / 903.3);
	XYZ_to_uv(XN, YN, ZN, &uN, &vN);
	u = U / (13 * L) + uN;
	v = V / (13 * L) + vN;
	*X =  9.0 * *Y * u / (4 * v);
	*Z =  - *X / 3 - 5 * *Y + 3 * *Y / v;
    }
}


/* ----- LUV <-> polarLUV ----- */

static void LUV_to_polarLUV(double L, double U, double V,
                            double *l, double *c, double *h)
{
    *l = L;
    *c = sqrt(U * U + V * V);
    *h = RAD2DEG(atan2(V, U));
    while (*h > 360) *h -= 360;
    while (*h < 0) *h += 360;
}

static void polarLUV_to_LUV(double l, double c, double h,
                            double *L, double *U, double *V)
{
    h = DEG2RAD(h);
    *L = l;
    *U = c * cos(h);
    *V = c * sin(h);
}



/*****************************************************************************
 *
 * FUNCTIONS
 *    tclRGBColors
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_rgbcolors
 *
 * DESCRIPTION
 *    Returns a list of RGB color indices from either 
 *
 *****************************************************************************/

static int tclRGBColors (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  int mode = (int) data;
  int copy_r = 0, copy_g = 0, copy_b = 0;
  DYN_LIST *r_list, *g_list, *b_list;
  int *r_vals, *g_vals, *b_vals;
  int r, g, b;
  int i, length;
  DYN_LIST *newlist;
  
  if (mode == DLG_SINGLE_COLOR) {
    if (argc < 4) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " r g b", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[1], &r) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[2], &g) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[3], &b) != TCL_OK) return TCL_ERROR;

    sprintf(interp->result, "%d", (r << 21) + (g << 13) + (b << 5));
    return TCL_OK;
  }

  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " r_list g_list b_list", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &r_list) != TCL_OK) {
  usage:
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "usage: ", argv[0], " r_list g_list b_list", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  /* Insist that it's a float list */
  else {
    if (DYN_LIST_DATATYPE(r_list) != DF_LONG) {
    color_int_error:
      Tcl_AppendResult(interp, argv[0], ": color vals must be ints", 
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  length = DYN_LIST_N(r_list);

  if (tclFindDynList(interp, argv[2], &g_list) != TCL_OK) goto usage;

  /* Insist that it's a int list */
  else {
    if (DYN_LIST_DATATYPE(g_list) != DF_LONG) goto color_int_error;
  }
  /* Make sure the length is OK */
  if (DYN_LIST_N(g_list) != 1) {
    length = DYN_LIST_N(g_list);
    if (DYN_LIST_N(r_list) != 1 &&
	(DYN_LIST_N(g_list) != DYN_LIST_N(r_list))) {
     bad_lengths:
      Tcl_AppendResult(interp, argv[0], ": args of incompatible lengths", 
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  if (tclFindDynList(interp, argv[3], &b_list) != TCL_OK) goto usage;
  else if (DYN_LIST_DATATYPE(b_list) != DF_LONG) goto color_int_error;
  /* Make sure the length is OK */
  if (DYN_LIST_N(b_list) != 1) {
    length = DYN_LIST_N(b_list);
    if (DYN_LIST_N(r_list) != 1 &&
	DYN_LIST_N(b_list) != DYN_LIST_N(r_list)) goto bad_lengths;
    if (DYN_LIST_N(g_list) != 1 &&
	DYN_LIST_N(b_list) != DYN_LIST_N(g_list)) goto bad_lengths;
  }

  if (DYN_LIST_N(r_list) == 1) copy_r = 1;
  if (DYN_LIST_N(g_list) == 1) copy_g = 1;
  if (DYN_LIST_N(b_list) == 1) copy_b = 1;
  
  r_vals = (int *) DYN_LIST_VALS(r_list);
  g_vals = (int *) DYN_LIST_VALS(g_list);
  b_vals = (int *) DYN_LIST_VALS(b_list);

  newlist = dfuCreateDynList(DF_LONG, length);
  for (i = 0; i < length; i++) {
    r = copy_r ? r_vals[0] : r_vals[i];
    g = copy_g ? g_vals[0] : g_vals[i];
    b = copy_b ? b_vals[0] : b_vals[i];
    dfuAddDynListLong(newlist, (r << 21) + (g << 13) + (b << 5));
  }
  return(tclPutList(interp, newlist));
}



/*****************************************************************************
 *
 * FUNCTIONS
 *    tclRGB2Hex
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_rgb2hex
 *
 * DESCRIPTION
 *    Returns a list of RGB colors in hex format
 *
 *****************************************************************************/

static int tclRGB2Hex (ClientData data, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  int mode = (int) data;
  int copy_r = 0, copy_g = 0, copy_b = 0;
  DYN_LIST *r_list, *g_list, *b_list;
  int *r_vals, *g_vals, *b_vals;
  char *newcolor;
  int r, g, b;
  int i, length;
  DYN_LIST *newlist;

  /* Handle special case where 3 numbers passed in */
  if (argc == 4) {
    if (Tcl_GetInt(interp, argv[1], &r) == TCL_OK &&
	Tcl_GetInt(interp, argv[2], &g) == TCL_OK &&
	Tcl_GetInt(interp, argv[3], &b) == TCL_OK) {
      sprintf(interp->result, "#%02x%02x%02x", r, g, b);
      return TCL_OK;
    }
    else {
      Tcl_ResetResult(interp);
    }
  }

  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " r_list g_list b_list", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &r_list) != TCL_OK) {
  usage:
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "usage: ", argv[0], " r_list g_list b_list", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  /* Insist that it's a float list */
  else {
    if (DYN_LIST_DATATYPE(r_list) != DF_LONG) {
    color_int_error:
      Tcl_AppendResult(interp, argv[0], ": color vals must be ints", 
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  length = DYN_LIST_N(r_list);

  if (tclFindDynList(interp, argv[2], &g_list) != TCL_OK) goto usage;

  /* Insist that it's a int list */
  else {
    if (DYN_LIST_DATATYPE(g_list) != DF_LONG) goto color_int_error;
  }
  /* Make sure the length is OK */
  if (DYN_LIST_N(g_list) != 1) {
    length = DYN_LIST_N(g_list);
    if (DYN_LIST_N(r_list) != 1 &&
	(DYN_LIST_N(g_list) != DYN_LIST_N(r_list))) {
     bad_lengths:
      Tcl_AppendResult(interp, argv[0], ": args of incompatible lengths", 
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  if (tclFindDynList(interp, argv[3], &b_list) != TCL_OK) goto usage;
  else if (DYN_LIST_DATATYPE(b_list) != DF_LONG) goto color_int_error;
  /* Make sure the length is OK */
  if (DYN_LIST_N(b_list) != 1) {
    length = DYN_LIST_N(b_list);
    if (DYN_LIST_N(r_list) != 1 &&
	DYN_LIST_N(b_list) != DYN_LIST_N(r_list)) goto bad_lengths;
    if (DYN_LIST_N(g_list) != 1 &&
	DYN_LIST_N(b_list) != DYN_LIST_N(g_list)) goto bad_lengths;
  }

  if (DYN_LIST_N(r_list) == 1) copy_r = 1;
  if (DYN_LIST_N(g_list) == 1) copy_g = 1;
  if (DYN_LIST_N(b_list) == 1) copy_b = 1;
  
  r_vals = (int *) DYN_LIST_VALS(r_list);
  g_vals = (int *) DYN_LIST_VALS(g_list);
  b_vals = (int *) DYN_LIST_VALS(b_list);

  newlist = dfuCreateDynList(DF_STRING, length);
  for (i = 0; i < length; i++) {
    r = copy_r ? r_vals[0] : r_vals[i];
    g = copy_g ? g_vals[0] : g_vals[i];
    b = copy_b ? b_vals[0] : b_vals[i];
    newcolor = (char *) malloc(8); /* to hold #RRGGBB\0 */
    sprintf(newcolor, "#%02x%02x%02x", r, g, b);
    dfuAddDynListString(newlist, newcolor);
  }
  return(tclPutList(interp, newlist));
}


static void LCH_to_RGB(double L, double C, double H,
		      int *rr, int *gg, int *bb)
{
  /* Use d65 whitepoint */
  double XN = 0.96;
  double YN = 1.00;
  double ZN = 0.92;

  double X, Y, Z;
  double l, a, b;
  double R, G, B;
  
  polarLAB_to_LAB(L, C, H, &l, &a, &b);
  LAB_to_XYZ(l, a, b, XN, YN, ZN, &X, &Y, &Z);
  XYZ_to_RGB(X, Y, Z, XN, YN, ZN, &R, &G, &B);

  *rr = (int) (R*256);
  if (*rr > 255) *rr = 255;
  if (*rr < 0) *rr = 0;
  *gg = (int) (G*256);
  if (*gg > 255) *gg = 255;
  if (*gg < 0) *gg = 0;
  *bb = (int) (B*256);
  if (*bb > 255) *bb = 255;
  if (*bb < 0) *bb = 0;

  return;
}
/*****************************************************************************
 *
 * FUNCTIONS
 *    tclRGBColors
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dlg_rgbcolors
 *
 * DESCRIPTION
 *    Returns a list of RGB color indices from Polar LUV spec
 *
 *****************************************************************************/

static int tclPolarLAB2RGBColors (ClientData data, Tcl_Interp *interp,
				  int argc, char *argv[])
{
  int mode = (int) data;
  int copy_L = 0, copy_C = 0, copy_H = 0;
  DYN_LIST *L_list, *C_list, *H_list;
  float *L_vals, *C_vals, *H_vals;
  int r, g, b;
  int i, length;
  DYN_LIST *newlist, *rlist, *glist, *blist;
  double L, C, H;

  if (mode == DLG_SINGLE_COLOR) {
    if (argc < 4) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " l c h", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (Tcl_GetDouble(interp, argv[1], &L) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetDouble(interp, argv[2], &C) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetDouble(interp, argv[3], &H) != TCL_OK) return TCL_ERROR;

    LCH_to_RGB(L, C, H, &r, &g, &b);
    sprintf(interp->result, "%d %d %d", r, g, b);
    return TCL_OK;
  }

  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " L_list C_list H_list", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &L_list) != TCL_OK) {
  usage:
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "usage: ", argv[0], " L_list C_list H_list", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  /* Insist that it's a float list */
  else {
    if (DYN_LIST_DATATYPE(L_list) != DF_FLOAT) {
    color_int_error:
      Tcl_AppendResult(interp, argv[0], ": vals must be floats", 
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  length = DYN_LIST_N(L_list);

  if (tclFindDynList(interp, argv[2], &C_list) != TCL_OK) goto usage;

  /* Insist that it's a float list */
  else {
    if (DYN_LIST_DATATYPE(C_list) != DF_FLOAT) goto color_int_error;
  }
  /* Make sure the length is OK */
  if (DYN_LIST_N(C_list) != 1) {
    length = DYN_LIST_N(C_list);
    if (DYN_LIST_N(L_list) != 1 &&
	(DYN_LIST_N(C_list) != DYN_LIST_N(L_list))) {
     bad_lengths:
      Tcl_AppendResult(interp, argv[0], ": args of incompatible lengths", 
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  if (tclFindDynList(interp, argv[3], &H_list) != TCL_OK) goto usage;
  else if (DYN_LIST_DATATYPE(H_list) != DF_FLOAT) goto color_int_error;
  /* Make sure the length is OK */
  if (DYN_LIST_N(H_list) != 1) {
    length = DYN_LIST_N(H_list);
    if (DYN_LIST_N(L_list) != 1 &&
	DYN_LIST_N(H_list) != DYN_LIST_N(L_list)) goto bad_lengths;
    if (DYN_LIST_N(C_list) != 1 &&
	DYN_LIST_N(H_list) != DYN_LIST_N(C_list)) goto bad_lengths;
  }

  if (DYN_LIST_N(L_list) == 1) copy_L = 1;
  if (DYN_LIST_N(C_list) == 1) copy_C = 1;
  if (DYN_LIST_N(H_list) == 1) copy_H = 1;
  
  L_vals = (float *) DYN_LIST_VALS(L_list);
  C_vals = (float *) DYN_LIST_VALS(C_list);
  H_vals = (float *) DYN_LIST_VALS(H_list);

  newlist = dfuCreateDynList(DF_LIST, 3);
  rlist = dfuCreateDynList(DF_LONG, length);
  glist = dfuCreateDynList(DF_LONG, length);
  blist = dfuCreateDynList(DF_LONG, length);

  for (i = 0; i < length; i++) {
    L = copy_L ? L_vals[0] : L_vals[i];
    C = copy_C ? C_vals[0] : C_vals[i];
    H = copy_H ? H_vals[0] : H_vals[i];
    LCH_to_RGB(L, C, H, &r, &g, &b);
    dfuAddDynListLong(rlist, r);
    dfuAddDynListLong(glist, g);
    dfuAddDynListLong(blist, b);
  }
  dfuMoveDynListList(newlist, rlist);
  dfuMoveDynListList(newlist, glist);
  dfuMoveDynListList(newlist, blist);
  return(tclPutList(interp, newlist));
}
