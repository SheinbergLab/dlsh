#include <stdio.h>
#include <stdlib.h>
#include <tcl.h>
#include <df.h>
#include <tcl_dl.h>
#include "clipper.hpp"
#include <string>
#include <string.h>

using namespace ClipperLib;
using namespace std;

extern "C" int add_clipper_commands(Tcl_Interp *interp);
extern int save_clipper_svg(Paths &ppg, const string &filename, float, bool);

extern "C" int clipperCmd (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[]);
extern "C" int clipperSVGCmd (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[]);

/*****************************************************************************
 *
 * polygon clipping code
 *
 *****************************************************************************/

static int dynListToPath(DYN_LIST *dl, Path& p)
{
  DYN_LIST **sublists;
  
  if (DYN_LIST_DATATYPE(dl) == DF_LONG) {
    if (DYN_LIST_N(dl) % 2) {
      return -1; 
    }
    int *vals = (int *) DYN_LIST_VALS(dl);

    for (auto i = 0; i < DYN_LIST_N(dl); i+=2) {
      p << IntPoint(vals[i],vals[i+1]);
    }
    return 0;
  }

  if (DYN_LIST_DATATYPE(dl) == DF_FLOAT) {
    if (DYN_LIST_N(dl) % 2) {
      return -1; 
    }
    float *vals = (float *) DYN_LIST_VALS(dl);
    for (auto i = 0; i < DYN_LIST_N(dl); i+=2) {
      p << IntPoint((int) vals[i], (int) vals[i+1]);
    }
    return 0;
  }

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return -2;
  if (DYN_LIST_N(dl) != 2) {
    return -1;
  }

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
  if (DYN_LIST_N(sublists[0]) != DYN_LIST_N(sublists[1])) return -1;

  if ((DYN_LIST_DATATYPE(sublists[0]) != DF_FLOAT &&
       DYN_LIST_DATATYPE(sublists[0]) != DF_LONG) ||
      (DYN_LIST_DATATYPE(sublists[1]) != DF_FLOAT &&
       (DYN_LIST_DATATYPE(sublists[1]) != DF_LONG))) return -2;

  if (DYN_LIST_DATATYPE(sublists[0]) == DF_FLOAT) {
    float *vx = (float *) DYN_LIST_VALS(sublists[0]);
    if (DYN_LIST_DATATYPE(sublists[1]) == DF_FLOAT) {
      float *vy = (float *) DYN_LIST_VALS(sublists[1]);
      for (auto i = 0; i < DYN_LIST_N(sublists[0]); i++) {
	p << IntPoint((int) vx[i], (int) vy[i]);
      }
      return 0;
    }
    if (DYN_LIST_DATATYPE(sublists[1]) == DF_LONG) {
      int *vy = (int *) DYN_LIST_VALS(sublists[1]);
      for (auto i = 0; i < DYN_LIST_N(sublists[0]); i++) {
	p << IntPoint((int) vx[i], (int) vy[i]);
      }
      return 0;
    }
  }

  if (DYN_LIST_DATATYPE(sublists[0]) == DF_LONG) {
    int *vx = (int *) DYN_LIST_VALS(sublists[0]);
    if (DYN_LIST_DATATYPE(sublists[1]) == DF_FLOAT) {
      float *vy = (float *) DYN_LIST_VALS(sublists[1]);
      for (auto i = 0; i < DYN_LIST_N(sublists[0]); i++) {
	p << IntPoint((int) vx[i], (int) vy[i]);
      }
      return 0;
    }
    if (DYN_LIST_DATATYPE(sublists[1]) == DF_LONG) {
      int *vy = (int *) DYN_LIST_VALS(sublists[1]);
      for (auto i = 0; i < DYN_LIST_N(sublists[0]); i++) {
	p << IntPoint((int) vx[i], (int) vy[i]);
      }
      return 0;
    }
  }
  return -1;
}

static DYN_LIST *pathToDynList(const Path p, bool close)
{
  DYN_LIST *xs, *ys, *verts;
  
  verts = dfuCreateDynList(DF_LIST, 2);
  xs = dfuCreateDynList(DF_LONG, p.size());
  ys = dfuCreateDynList(DF_LONG, p.size());
  for (auto const &point: p) {
    dfuAddDynListLong(xs, point.X);
    dfuAddDynListLong(ys, point.Y);
  }
  if (close && (p.back().X != p.front().X || p.back().Y != p.front().Y)) {
    dfuAddDynListLong(xs, p[0].X);
    dfuAddDynListLong(ys, p[0].Y);
  }
  
  dfuMoveDynListList(verts, xs);
  dfuMoveDynListList(verts, ys);

  return verts;
}

static DYN_LIST *pathsToDynList(const Paths p)
{
  DYN_LIST *paths = dfuCreateDynList(DF_LIST, p.size());
  for (int i = 0; i < (int) p.size(); i++) {
    dfuMoveDynListList(paths, pathToDynList(p[i], true));
  }
  return paths;
}


static bool ASCII_icompare(const char* str1, const char* str2)
{
  //case insensitive compare for ASCII chars only
  while (*str1) 
  {
    if (toupper(*str1) != toupper(*str2)) return false;
    str1++;
    str2++;
  }
  return (!*str2);
}

int clipperCmd (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DYN_LIST *newlist;
  DYN_LIST *subj, *clip;
  DYN_LIST **sublists;
  ClipType ct = ctIntersection;
  PolyFillType pft = pftNonZero;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " subj ?clip? ?cliptype? ?filltype?", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (tclFindDynList(interp, argv[1], &subj) != TCL_OK) return TCL_ERROR;  

  /* An empty clip argument means "no clip" -- without it the later optional
     args (cliptype, filltype) could not be reached for a plain union. */
  if (argc > 2 && argv[2][0] != '\0' && strcmp(argv[2], "-") != 0) {
    if (tclFindDynList(interp, argv[2], &clip) != TCL_OK) return TCL_ERROR;
  }
  else clip = NULL;
  
  if (argc > 4) {
    /* Fill rule. NonZero (the previous hard-coded behavior) fills the whole
       envelope of an overlapping path; EvenOdd punches out the overlaps, which
       is how you get a shape with a hole. */
    if (ASCII_icompare(argv[4], "EVENODD")) pft = pftEvenOdd;
    else if (ASCII_icompare(argv[4], "NONZERO")) pft = pftNonZero;
    else if (ASCII_icompare(argv[4], "POSITIVE")) pft = pftPositive;
    else if (ASCII_icompare(argv[4], "NEGATIVE")) pft = pftNegative;
    else {
      Tcl_AppendResult(interp, argv[0], ": invalid fill type\
 (nonzero|evenodd|positive|negative)", (char *) NULL);
      return TCL_ERROR;
    }
  }

  if (argc > 3) {
    if (ASCII_icompare(argv[3], "XOR")) ct = ctXor;
    else if (ASCII_icompare(argv[3], "UNION")) ct = ctUnion;
    else if (ASCII_icompare(argv[3], "DIFFERENCE")) ct = ctDifference;
    else if (ASCII_icompare(argv[3], "INTERSECTION")) ct = ctIntersection;
    else {
      Tcl_AppendResult(interp, argv[0], ": invalid clip function", 
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  /* Create clipper object with subj/clip paths */
  Clipper clpr;
  
  /* Build subject paths */
  if (DYN_LIST_DATATYPE(subj) != DF_LIST) {
    Tcl_AppendResult(interp, "usage: ", "invalid subj paths specified ", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  /* reserve, don't size: sizing here left N empty paths in front of the real
     ones, doubling the vector every call */
  Paths sub;
  sub.reserve(DYN_LIST_N(subj));
  sublists = (DYN_LIST **) DYN_LIST_VALS(subj);
  for (int i = 0; i < DYN_LIST_N(subj); i++) {
    Path p;
    if (dynListToPath(sublists[i], p) < 0) {
      Tcl_AppendResult(interp, argv[0], ": error creating paths from sub",
		       (char *) NULL);
      return TCL_ERROR;
    }
    sub.push_back(p);
  }
  clpr.AddPaths(sub, ptSubject, true);

  if (clip) {
    /* Build clipping paths */
    if (DYN_LIST_DATATYPE(clip) != DF_LIST) {
      Tcl_AppendResult(interp, "usage: ", "invalid clip paths specified ",
		       (char *) NULL);
      return TCL_ERROR;
    }
    Paths clp;
    clp.reserve(DYN_LIST_N(clip));
    sublists = (DYN_LIST **) DYN_LIST_VALS(clip);
    for (int i = 0; i < DYN_LIST_N(clip); i++) {
      Path p;
      if (dynListToPath(sublists[i], p) < 0) {
	Tcl_AppendResult(interp, argv[0], ": error creating paths from clp",
			 (char *) NULL);
	return TCL_ERROR;    
      }
      clp.push_back(p);
    }
    clpr.AddPaths(clp, ptClip, true);
  }
  else {
    ct = ctUnion;
  }

  Paths solution;
  clpr.Execute(ct, solution, pft, pft);
  /* SimplifyPolygons defaults to pftEvenOdd, which would undo the requested
     fill rule; pass it through. */
  SimplifyPolygons(solution, pft);
  newlist = pathsToDynList(solution);

  if (!newlist) {
    Tcl_AppendResult(interp, argv[0], ": unable to interpolate data",
		     (char *) NULL);
    return TCL_ERROR;    
  }
  return(tclPutList(interp, newlist));
}

/*****************************************************************************
 *
 * curve::polygonOffset paths delta ?jointype? ?arctolerance?
 *
 * Inflate (delta > 0) or deflate (delta < 0) closed polygons.  Coordinates are
 * integers, so delta is in the same units the paths were scaled into.
 *
 * Offsetting by -d then +d is a morphological open: it removes filaments and
 * necks thinner than 2d while leaving everything wider than that essentially
 * unchanged.  That is what gives a generated shape a guaranteed minimum
 * feature width.  The reverse order (+d then -d) closes pinholes and notches.
 *
 *****************************************************************************/

extern "C" int polygonOffsetCmd (ClientData data, Tcl_Interp *interp,
				 int argc, char *argv[]);

int polygonOffsetCmd (ClientData data, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  DYN_LIST *dl, **sublists;
  double delta, arctol = 0.25;
  JoinType jt = jtRound;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " paths delta ?jointype? ?arctolerance?", (char *) NULL);
    return TCL_ERROR;
  }

  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &delta) != TCL_OK) return TCL_ERROR;

  if (argc > 3) {
    if (ASCII_icompare(argv[3], "ROUND")) jt = jtRound;
    else if (ASCII_icompare(argv[3], "SQUARE")) jt = jtSquare;
    else if (ASCII_icompare(argv[3], "MITER")) jt = jtMiter;
    else {
      Tcl_AppendResult(interp, argv[0],
		       ": invalid join type (round|square|miter)",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  if (argc > 4) {
    if (Tcl_GetDouble(interp, argv[4], &arctol) != TCL_OK) return TCL_ERROR;
  }

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    Tcl_AppendResult(interp, argv[0], ": invalid paths specified",
		     (char *) NULL);
    return TCL_ERROR;
  }

  Paths sub;
  sub.reserve(DYN_LIST_N(dl));
  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
  for (int i = 0; i < DYN_LIST_N(dl); i++) {
    Path p;
    if (dynListToPath(sublists[i], p) < 0) {
      Tcl_AppendResult(interp, argv[0], ": error creating paths",
		       (char *) NULL);
      return TCL_ERROR;
    }
    sub.push_back(p);
  }

  ClipperOffset co;
  co.ArcTolerance = arctol;
  co.AddPaths(sub, jt, etClosedPolygon);

  Paths solution;
  co.Execute(solution, delta);

  return(tclPutList(interp, pathsToDynList(solution)));
}

int clipperSVGCmd (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  DYN_LIST *dl, **sublists;
  double scale = 1.0;
  bool flipy = false;
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " paths filename ?scale? ?flipy?", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  
  /* Build subject paths */
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    Tcl_AppendResult(interp, "usage: ", "invalid subj paths specified ", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (argc > 3) {
    if (Tcl_GetDouble(interp, argv[3], &scale) != TCL_OK) return TCL_ERROR;
  }

  if (argc > 4) {		/* was argc > 3: read argv[4] past the end */
    int dummy;
    if (Tcl_GetInt(interp, argv[4], &dummy) != TCL_OK) return TCL_ERROR;
    if (dummy == 0) flipy = false;
    else flipy = true;
  }

  Paths sub;
  sub.reserve(DYN_LIST_N(dl));
  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
  for (int i = 0; i < DYN_LIST_N(dl); i++) {
    Path p;
    if (dynListToPath(sublists[i], p) < 0) {
      Tcl_AppendResult(interp, argv[0], ": error creating paths for SVG",
		       (char *) NULL);
      return TCL_ERROR;    
    }
    sub.push_back(p);
  }
  
  save_clipper_svg(sub, argv[2], scale, flipy);

  return TCL_OK;
}

int add_clipper_commands(Tcl_Interp *interp)
{
  Tcl_CreateCommand(interp, "curve::clipper", 
		    (Tcl_CmdProc *) clipperCmd, 
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "curve::polygonOffset",
		    (Tcl_CmdProc *) polygonOffsetCmd,
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "curve::saveSVG",
		    (Tcl_CmdProc *) clipperSVGCmd,
		    (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  return TCL_OK;
}

  



