/*
 * NAME
 *   tcl_curve.c
 *
 * DESCRIPTION
 *
 * AUTHOR
 *   DLS, 06/11
 */

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h" 
#undef WIN32_LEAN_AND_MEAN
#pragma warning (disable:4244)
#pragma warning (disable:4305)
#define DllEntryPoint DllMain
#define EXPORT(a,b) __declspec(dllexport) a b
#else
#define DllEntryPoint
#define EXPORT a b
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <tcl.h>
#include <df.h>
#include <tcl_dl.h>
#include <math.h>

#include "curve.h"

/* From dl_clipper.cpp */
extern int add_clipper_commands(Tcl_Interp *interp);


/*
 * Extract a float array from a dynlist that may hold DF_FLOAT or DF_LONG.
 * If a conversion was needed, *tofree receives the allocated buffer and the
 * caller must free it.  Without this, cubic/bezier accepted only DF_FLOAT and
 * an integer x/y list failed with the same opaque "unable to interpolate data"
 * message as a genuine numerical failure -- curve::clipper has always taken
 * both types, so the inconsistency was a trap.
 */
static float *as_floats(DYN_LIST *l, float **tofree)
{
  int i, n = DYN_LIST_N(l);
  int *iv;
  float *fv;

  *tofree = NULL;
  if (DYN_LIST_DATATYPE(l) == DF_FLOAT) return (float *) DYN_LIST_VALS(l);
  if (DYN_LIST_DATATYPE(l) != DF_LONG) return NULL;

  fv = (float *) calloc(n, sizeof(float));
  if (!fv) return NULL;
  iv = (int *) DYN_LIST_VALS(l);
  for (i = 0; i < n; i++) fv[i] = (float) iv[i];
  *tofree = fv;
  return fv;
}

static int numeric_list(DYN_LIST *l)
{
  return (DYN_LIST_DATATYPE(l) == DF_FLOAT || DYN_LIST_DATATYPE(l) == DF_LONG);
}

/*
 * NAME
 *    cubic
 * CALLS
 *    third_order_poly (curve.c)
 */
static DYN_LIST *cubic(DYN_LIST *x, DYN_LIST *y, int res)
{
  DYN_LIST *newlist = NULL, *tmplist;
  int nout, ret;
  float *interpx, *interpy;

  if (DYN_LIST_N(x) != DYN_LIST_N(y)) return NULL;
  if (res <= 0) return NULL;

  if (numeric_list(x) && numeric_list(y)) {
    float *fx = NULL, *fy = NULL, *xv, *yv;
    xv = as_floats(x, &fx);
    yv = as_floats(y, &fy);
    if (!xv || !yv) { if (fx) free(fx); if (fy) free(fy); return NULL; }

    nout = (res+1)*DYN_LIST_N(x);
    interpx = (float *) calloc(nout, sizeof(float));
    interpy = (float *) calloc(nout, sizeof(float));

    ret = third_order_poly(DYN_LIST_N(x), xv, yv, res, interpx, interpy);

    if (fx) free(fx);
    if (fy) free(fy);

    if (ret != nout) { free(interpx); free(interpy); return NULL; }
    
    newlist = dfuCreateDynList(DF_LIST, 2);
    
    tmplist = dfuCreateDynListWithVals(DF_FLOAT, ret, interpx);
    dfuMoveDynListList(newlist, tmplist);
    
    tmplist = dfuCreateDynListWithVals(DF_FLOAT, ret, interpy);
    dfuMoveDynListList(newlist, tmplist);
  } 
  else if (DYN_LIST_DATATYPE(x) == DF_LIST &&
	   DYN_LIST_DATATYPE(y) == DF_LIST) {
    int i;
    DYN_LIST **xvals = (DYN_LIST **) DYN_LIST_VALS(x);
    DYN_LIST **yvals = (DYN_LIST **) DYN_LIST_VALS(y);
    newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(x));

    for (i = 0; i < DYN_LIST_N(x); i++) {
      tmplist = cubic(xvals[i], yvals[i], res);
      if (tmplist) dfuMoveDynListList(newlist, tmplist);
      else {
	if (newlist) dfuFreeDynList(newlist);
	return NULL;
      }
    }
  }
  return newlist;
}


/*****************************************************************************
 *
 * FUNCTION
 *    cubicCmd
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    curve::cubic
 *
 * DESCRIPTION
 *    Perform third order polynomial fit
 *
 ****************************************************************************/

static int cubicCmd (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DYN_LIST *x, *y, *newlist;	/* i/o dynlists                 */
  int res = 20;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " xvals yvals [res]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  if (argc > 3) {
    if (Tcl_GetInt(interp, argv[3], &res) != TCL_OK) return TCL_ERROR;
  }

  if (DYN_LIST_N(x) != DYN_LIST_N(y)) {
    Tcl_AppendResult(interp, argv[0], ": x/y lists must be of same length",
		     (char *) NULL);
    return TCL_ERROR;
  }

  newlist = cubic(x, y, res);

  if (!newlist) {
    Tcl_AppendResult(interp, argv[0], ": unable to interpolate data",
		     (char *) NULL);
    return TCL_ERROR;    
  }
  return(tclPutList(interp, newlist));
}



/*
 * NAME
 *    bezier
 * CALLS
 *    bezier_interpolate (curve.c)
 */
static DYN_LIST *bezier(DYN_LIST *x, DYN_LIST *y, int res)
{
  DYN_LIST *newlist = NULL, *tmplist;
  int nout, ret;
  float *interpx, *interpy;

  if (DYN_LIST_N(x) != DYN_LIST_N(y)) return NULL;
  if (DYN_LIST_N(x) % 4 != 0) return NULL;
  if (res <= 0) return NULL;

  if (numeric_list(x) && numeric_list(y)) {
    float *fx = NULL, *fy = NULL, *xv, *yv;
    xv = as_floats(x, &fx);
    yv = as_floats(y, &fy);
    if (!xv || !yv) { if (fx) free(fx); if (fy) free(fy); return NULL; }

    nout = (res+1)*(DYN_LIST_N(x)/4);
    interpx = (float *) calloc(nout, sizeof(float));
    interpy = (float *) calloc(nout, sizeof(float));

    ret = bezier_interpolate(DYN_LIST_N(x), xv, yv, res, interpx, interpy);

    if (fx) free(fx);
    if (fy) free(fy);

    if (ret != nout) { free(interpx); free(interpy); return NULL; }
    
    newlist = dfuCreateDynList(DF_LIST, 2);
    
    tmplist = dfuCreateDynListWithVals(DF_FLOAT, ret, interpx);
    dfuMoveDynListList(newlist, tmplist);
    
    tmplist = dfuCreateDynListWithVals(DF_FLOAT, ret, interpy);
    dfuMoveDynListList(newlist, tmplist);
  } 
  else if (DYN_LIST_DATATYPE(x) == DF_LIST &&
	   DYN_LIST_DATATYPE(y) == DF_LIST) {
    int i;
    DYN_LIST **xvals = (DYN_LIST **) DYN_LIST_VALS(x);
    DYN_LIST **yvals = (DYN_LIST **) DYN_LIST_VALS(y);
    newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(x));

    for (i = 0; i < DYN_LIST_N(x); i++) {
      tmplist = bezier(xvals[i], yvals[i], res);
      if (tmplist) dfuMoveDynListList(newlist, tmplist);
      else {
	if (newlist) dfuFreeDynList(newlist);
	return NULL;
      }
    }
  }
  return newlist;
}

static int bezierCmd (ClientData data, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  DYN_LIST *x, *y, *newlist;	/* i/o dynlists                 */
  int res = 20;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " xvals yvals [res]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  if (argc > 3) {
    if (Tcl_GetInt(interp, argv[3], &res) != TCL_OK) return TCL_ERROR;
  }

  if (DYN_LIST_N(x) != DYN_LIST_N(y)) {
    Tcl_AppendResult(interp, argv[0], ": x/y lists must be of same length",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (DYN_LIST_N(x) % 4 != 0) {
    Tcl_AppendResult(interp, argv[0], ": x & y lengths must be multiples of 4",
		     (char *) NULL);
    return TCL_ERROR;
  }
  newlist = bezier(x, y, res);

  if (!newlist) {
    Tcl_AppendResult(interp, argv[0], ": unable to interpolate data",
		     (char *) NULL);
    return TCL_ERROR;    
  }
  return(tclPutList(interp, newlist));
}




/*****************************************************************************
 *
 * FUNCTION
 *    closestPointCmd
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    curves::closestPoint
 *
 * DESCRIPTION
 *    Find point index nearest to new point (for ordered insertion)
 *
 ****************************************************************************/

static int closestPointCmd (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  DYN_LIST *xp, *yp;
  double x, y;
  int result;

  if (argc < 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " xvals yvals x y", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &xp) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &yp) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y) != TCL_OK) return TCL_ERROR;

  if (DYN_LIST_N(xp) != DYN_LIST_N(yp)) {
    Tcl_AppendResult(interp, argv[0], ": x/y lists must be of same length",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  result = closest_point(DYN_LIST_N(xp), 
			 (float *) DYN_LIST_VALS(xp), 
			 (float *) DYN_LIST_VALS(yp),
			 x, y);

  Tcl_SetObjResult(interp, Tcl_NewIntObj(result));
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    polygonCentroid
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    curves::polygonCentroid
 *
 * DESCRIPTION
 *    Find area and centroid of a polygon
 *
 ****************************************************************************/

static int polygonCentroidCmd (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  DYN_LIST *xp, *yp;
  int result;
  float area, xc, yc;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " xvals yvals", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &xp) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &yp) != TCL_OK) return TCL_ERROR;

  if (DYN_LIST_N(xp) != DYN_LIST_N(yp)) {
    Tcl_AppendResult(interp, argv[0], ": x/y lists must be of same length",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (DYN_LIST_DATATYPE(xp) != DF_FLOAT &&
      DYN_LIST_DATATYPE(yp) != DF_FLOAT) {
    Tcl_AppendResult(interp, argv[0], ": x/y lists must be floats",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  result = polygon_centroid((float *) DYN_LIST_VALS(xp), 
			    (float *) DYN_LIST_VALS(yp),
			    DYN_LIST_N(xp), 
			    &xc, &yc, &area);
  if (result) {
    Tcl_AppendResult(interp, argv[0], ": error computing area/centroid",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (area < 0) {
    area = -area;
  }

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(area));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(xc));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(yc));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    polygonClose
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    curves::polygonClose
 *
 * DESCRIPTION
 *    close a polygon (if not already closed)
 *
 ****************************************************************************/

static int polygonCloseCmd (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  DYN_LIST *xp, *yp;
  float *xvals, *yvals;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " xvals yvals", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &xp) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &yp) != TCL_OK) return TCL_ERROR;

  if (DYN_LIST_N(xp) != DYN_LIST_N(yp)) {
    Tcl_AppendResult(interp, argv[0], ": x/y lists must be of same length",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (DYN_LIST_DATATYPE(xp) != DF_FLOAT &&
      DYN_LIST_DATATYPE(yp) != DF_FLOAT) {
    Tcl_AppendResult(interp, argv[0], ": x/y lists must be floats",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  xvals = (float *) DYN_LIST_VALS(xp);
  yvals = (float *) DYN_LIST_VALS(yp);
  if (xvals[0] == xvals[DYN_LIST_N(xp)-1] &&
      xvals[0] == xvals[DYN_LIST_N(xp)-1]) {
    /* already closed */
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    return TCL_OK;
  }
  else {
    dfuAddDynListFloat(xp, xvals[0]);
    dfuAddDynListFloat(yp, yvals[0]);
    return TCL_OK;
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    polygonSelfIntersectsCmd
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    curves::polygonSelfIntersects
 *
 * DESCRIPTION
 *    Test for self intersection
 *
 ****************************************************************************/


/*****************************************************************************
 *
 * curve::resample xvals yvals n ?closed?
 *
 * Resample a polyline/polygon so the returned n points are spaced uniformly in
 * ARCLENGTH.  Returns {xlist ylist}.
 *
 * Clipper and the spline routines both emit vertices that are dense on tight
 * curves and sparse on straight runs, so anything that measures a shape --
 * curvature, Fourier descriptors, shape distance -- has to re-space the
 * contour first.  There is no dl_interp in dlsh, so without this it cannot be
 * done in Tcl at all.
 *
 *****************************************************************************/

static int resampleCmd (ClientData data, Tcl_Interp *interp,
			int argc, char *argv[])
{
  DYN_LIST *x, *y, *newlist, *tmplist;
  float *xv, *yv, *fx = NULL, *fy = NULL;
  float *outx, *outy, *cum;
  int n, npts, i, seg, closed = 1;
  double total, step, target, t;

  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " xvals yvals n ?closed?",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (tclFindDynList(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &n) != TCL_OK) return TCL_ERROR;
  if (argc > 4) {
    if (Tcl_GetInt(interp, argv[4], &closed) != TCL_OK) return TCL_ERROR;
  }

  if (DYN_LIST_N(x) != DYN_LIST_N(y)) {
    Tcl_AppendResult(interp, argv[0], ": x/y lists must be of same length",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (!numeric_list(x) || !numeric_list(y)) {
    Tcl_AppendResult(interp, argv[0], ": x/y must be float or int lists",
		     (char *) NULL);
    return TCL_ERROR;
  }
  npts = DYN_LIST_N(x);
  if (npts < 2 || n < 2) {
    Tcl_AppendResult(interp, argv[0], ": need at least 2 input and 2 output points",
		     (char *) NULL);
    return TCL_ERROR;
  }

  xv = as_floats(x, &fx);
  yv = as_floats(y, &fy);
  if (!xv || !yv) {
    if (fx) free(fx);
    if (fy) free(fy);
    Tcl_AppendResult(interp, argv[0], ": out of memory", (char *) NULL);
    return TCL_ERROR;
  }

  /* cumulative arclength; nseg = npts for a closed contour (the wrap segment
     counts), npts-1 for an open one */
  {
    int nseg = closed ? npts : npts-1;
    cum = (float *) calloc(nseg+1, sizeof(float));
    cum[0] = 0.0f;
    for (i = 0; i < nseg; i++) {
      int k = (i+1) % npts;
      double dx = xv[k]-xv[i], dy = yv[k]-yv[i];
      cum[i+1] = cum[i] + (float) sqrt(dx*dx + dy*dy);
    }
    total = cum[nseg];
    if (total <= 0.0) {
      free(cum);
      if (fx) free(fx);
      if (fy) free(fy);
      Tcl_AppendResult(interp, argv[0], ": zero-length contour", (char *) NULL);
      return TCL_ERROR;
    }

    outx = (float *) calloc(n, sizeof(float));
    outy = (float *) calloc(n, sizeof(float));

    /* a closed contour is sampled over [0,total), so the last sample does not
       land back on the first; an open one includes both endpoints */
    step = closed ? total/n : total/(n-1);

    seg = 0;
    for (i = 0; i < n; i++) {
      target = i*step;
      if (target > total) target = total;
      while (seg < nseg-1 && cum[seg+1] < target) seg++;
      {
	double len = cum[seg+1]-cum[seg];
	int k = (seg+1) % npts;
	t = (len > 0.0) ? (target-cum[seg])/len : 0.0;
	outx[i] = (float) (xv[seg] + t*(xv[k]-xv[seg]));
	outy[i] = (float) (yv[seg] + t*(yv[k]-yv[seg]));
      }
    }
    free(cum);
  }

  if (fx) free(fx);
  if (fy) free(fy);

  newlist = dfuCreateDynList(DF_LIST, 2);
  tmplist = dfuCreateDynListWithVals(DF_FLOAT, n, outx);
  dfuMoveDynListList(newlist, tmplist);
  tmplist = dfuCreateDynListWithVals(DF_FLOAT, n, outy);
  dfuMoveDynListList(newlist, tmplist);

  return(tclPutList(interp, newlist));
}

static int polygonSelfIntersectsCmd (ClientData data, Tcl_Interp *interp,
				     int argc, char *argv[])
{
  DYN_LIST *xp, *yp;
  int result;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " xvals yvals", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &xp) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &yp) != TCL_OK) return TCL_ERROR;

  if (DYN_LIST_N(xp) != DYN_LIST_N(yp)) {
    Tcl_AppendResult(interp, argv[0], ": x/y lists must be of same length",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  result = polygon_self_intersects(DYN_LIST_N(xp), 
				   (float *) DYN_LIST_VALS(xp), 
				   (float *) DYN_LIST_VALS(yp));
  Tcl_SetObjResult(interp, Tcl_NewIntObj(result));

  return(TCL_OK);
}


/*****************************************************************************
 *
 * EXPORT
 *
 *****************************************************************************/

#ifdef WIN32
EXPORT(int,Curve_Init) (Tcl_Interp *interp)
#else
int Curve_Init(Tcl_Interp *interp)
#endif
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
  if (Tcl_PkgRequire(interp, "dlsh", "1.2", 0) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_PkgProvide(interp, "curve", "1.1") != TCL_OK) {
    return TCL_ERROR;
  }

  Tcl_Eval(interp, "namespace eval curve {}");
  Tcl_CreateCommand(interp, "curve::cubic", 
		    (Tcl_CmdProc *) cubicCmd, 
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "curve::bezier", 
		    (Tcl_CmdProc *) bezierCmd, 
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "curve::closestPoint", 
		    (Tcl_CmdProc *) closestPointCmd, 
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "curve::resample",
		    (Tcl_CmdProc *) resampleCmd,
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "curve::polygonSelfIntersects", 
		    (Tcl_CmdProc *) polygonSelfIntersectsCmd, 
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "curve::polygonCentroid", 
		    (Tcl_CmdProc *) polygonCentroidCmd, 
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "curve::polygonClose", 
		    (Tcl_CmdProc *) polygonCloseCmd, 
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

  add_clipper_commands(interp);

  return TCL_OK;
}





