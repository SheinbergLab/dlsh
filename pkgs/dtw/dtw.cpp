#include <cstdio>
#include <iostream>
#include <vector>
#include <DTW.hpp>

#include <tcl.h>
#include <df.h>
#include <dfana.h>
#include <tcl_dl.h>


static int dl_to_2d_vector(DYN_LIST *dl, std::vector<std::vector<double> > &v)
{
  DYN_LIST **sublists;
  float *xs, *ys;
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return 0;
  if (DYN_LIST_N(dl) != 2) return 0;
  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
  if (DYN_LIST_DATATYPE(sublists[0]) != DF_FLOAT ||
      DYN_LIST_DATATYPE(sublists[1]) != DF_FLOAT ||
      DYN_LIST_N(sublists[0]) != DYN_LIST_N(sublists[1])) {
    return 0;
  }
  xs = (float *) DYN_LIST_VALS(sublists[0]);
  ys = (float *) DYN_LIST_VALS(sublists[1]);

  for (int i = 0; i < DYN_LIST_N(sublists[0]); i++) {
    v.push_back(std::vector<double>{xs[i], ys[i]});
  }
  return 1;
}

/*
 * Explain why dl is not a usable xy path, or return NULL if it is one.
 *
 * Every structural test has to happen before the DYN_LIST ** cast below:
 * a list that is not DF_LIST holds floats/ints, and one that is DF_LIST but
 * does not hold exactly two elements has no sublists[1] to read.  Casting
 * either one to DYN_LIST ** and dereferencing it is a segfault, not a
 * failed test.
 */
static const char *path_problem(DYN_LIST *dl)
{
  DYN_LIST **sublists, *dlx, *dly;

  if (!dl) return "path is null";
  if (DYN_LIST_DATATYPE(dl) != DF_LIST)
    return "path must be a list of lists";
  if (DYN_LIST_N(dl) != 2)
    return "path must hold exactly two sublists (x and y)";

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
  dlx = sublists[0];
  dly = sublists[1];
  if (!dlx || !dly) return "path sublists must not be null";
  if (DYN_LIST_DATATYPE(dlx) != DF_FLOAT ||
      DYN_LIST_DATATYPE(dly) != DF_FLOAT)
    return "path x and y sublists must both be float lists";
  if (DYN_LIST_N(dlx) != DYN_LIST_N(dly))
    return "path x and y sublists must be the same length";
  return NULL;
}

static int is_single_path(DYN_LIST *dl)
{
  return path_problem(dl) == NULL;
}

/*
 * Check that dl is a non-empty list whose every element is a valid path,
 * leaving a specific error in interp if it is not.
 */
static int check_path_list(Tcl_Interp *interp, const char *cmd,
			   const char *argname, DYN_LIST *dl)
{
  DYN_LIST **sublists;
  const char *why;

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    Tcl_AppendResult(interp, cmd, ": ", argname,
		     " must be a list of paths, each path a list of two"
		     " equal length float lists", NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(dl) < 1) {
    Tcl_AppendResult(interp, cmd, ": ", argname,
		     " must hold at least one path", NULL);
    return TCL_ERROR;
  }

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
  for (int i = 0; i < DYN_LIST_N(dl); i++) {
    if ((why = path_problem(sublists[i]))) {
      char idx[32];
      snprintf(idx, sizeof(idx), "%d", i);
      Tcl_AppendResult(interp, cmd, ": ", argname, " element ", idx,
		       ": ", why, NULL);
      return TCL_ERROR;
    }
  }
  return TCL_OK;
}

// the p-norm to use; 2.0 = euclidean, 1.0 = manhattan
static DYN_LIST *distance_only(DYN_LIST *dl1, DYN_LIST *dl2, double pnorm)
{
  DYN_LIST *result = NULL;

  if (DYN_LIST_DATATYPE(dl1) != DF_LIST ||
      DYN_LIST_DATATYPE(dl2) != DF_LIST) {
    return NULL;
  }

  // Both lists are single xy paths
  if (is_single_path(dl1) && is_single_path(dl2)) {
    std::vector<std::vector<double> > v1, v2;
    if (!dl_to_2d_vector(dl1, v1) ||
	!dl_to_2d_vector(dl2, v2)) {
      return NULL;
    }
    result = dfuCreateDynList(DF_FLOAT, 1);
    dfuAddDynListFloat(result, DTW::dtw_distance_only(v1, v2, pnorm));
  }

  // dl1 is single xy path and dl2 list of paths
  else if (is_single_path(dl1)) {
    DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dl2);

    // check that all sublists are paths
    for (int i = 0; i < DYN_LIST_N(dl2); i++) {
      if (!is_single_path(sublists[i])) return NULL;
    }
    result = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dl2));
    for (int i = 0; i < DYN_LIST_N(dl2); i++) {
      std::vector<std::vector<double> > v1, v2;
      if (!dl_to_2d_vector(dl1, v1) ||
	  !dl_to_2d_vector(sublists[i], v2)) {
	dfuFreeDynList(result);
	return NULL;
      }
      dfuAddDynListFloat(result, DTW::dtw_distance_only(v1, v2, pnorm));
    }
  }

  // dl1 is multi xy path and dl2 single xy path
  else if (is_single_path(dl2)) {
    DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dl1);

    // check that all sublists are paths
    for (int i = 0; i < DYN_LIST_N(dl1); i++) {
      if (!is_single_path(sublists[i])) return NULL;
    }
    result = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dl1));
    for (int i = 0; i < DYN_LIST_N(dl1); i++) {
      std::vector<std::vector<double> > v1, v2;
      if (!dl_to_2d_vector(sublists[i], v1) ||
	  !dl_to_2d_vector(dl2, v2)) {
	dfuFreeDynList(result);
	return NULL;
      }
      dfuAddDynListFloat(result, DTW::dtw_distance_only(v1, v2, pnorm));
    }
  }

  return result;
}


// the p-norm to use; 2.0 = euclidean, 1.0 = manhattan
static DYN_LIST *distance_only(DYN_LIST *dl, double pnorm)
{
  DYN_LIST *result = NULL;

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    return NULL;
  }

  DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  // check that all sublists are paths
  for (int i = 0; i < DYN_LIST_N(dl); i++) {
    if (!is_single_path(sublists[i])) return NULL;
  }

  // as the output is symmetric, we need random access to all cells
  float **rows = (float **) calloc(DYN_LIST_N(dl), sizeof(float *));
  for (int i = 0; i < DYN_LIST_N(dl); i++) {
    rows[i] = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
  }

  // fill diagonal with 0s
  for (int i = 0; i < DYN_LIST_N(dl); i++) rows[i][i] = 0.0;

  // now fill rest of matrix
  for (int i = 0; i < DYN_LIST_N(dl); i++) {
    std::vector<std::vector<double> > v1;
    if (!dl_to_2d_vector(sublists[i], v1)) {
      for (int k = 0; k < DYN_LIST_N(dl); k++) free(rows[k]);
      free(rows);
      return NULL;
    }
    for (int j = 0; j < i; j++) {
      std::vector<std::vector<double> > v2;
      if (!dl_to_2d_vector(sublists[j], v2)) {
	for (int k = 0; k < DYN_LIST_N(dl); k++) free(rows[k]);
	free(rows);
	return NULL;
      }
      rows[i][j] = rows[j][i] = DTW::dtw_distance_only(v1, v2, pnorm);
    }
  }

  // final result is be list of lists
  result = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));

  // reuse allocated rows as sublists in result
  for (int i = 0; i < DYN_LIST_N(dl); i++) {
    dfuAddDynListList(result,
		      dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), rows[i]));
  }

  // free original float ** array holding rows
  free(rows);
  
  return result;
}

static int
dtw_distance_only_func(ClientData data, Tcl_Interp *interp,
		       int objc, Tcl_Obj *const objv[])
{
  DYN_LIST *dl1, *dl2;
  DYN_LIST *retlist = NULL;
  const char *cmd;

  if (objc < 2 || objc > 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "dl1 [dl2]");
    return (TCL_ERROR);
  }
  cmd = Tcl_GetString(objv[0]);

  if (tclFindDynList(interp, Tcl_GetString(objv[1]), &dl1) != TCL_OK)
    return TCL_ERROR;

  double pnorm = 2;
  if (objc == 2) {
    /* single argument: a list of paths, compared pairwise */
    if (check_path_list(interp, cmd, "argument", dl1) != TCL_OK)
      return TCL_ERROR;
    retlist = distance_only(dl1, pnorm);
  }

  else {
    if (tclFindDynList(interp, Tcl_GetString(objv[2]), &dl2) != TCL_OK)
      return TCL_ERROR;

    /* each argument is either one path or a list of paths, and at least
       one of the two has to be a single path */
    if (!is_single_path(dl1) &&
	check_path_list(interp, cmd, "first argument", dl1) != TCL_OK)
      return TCL_ERROR;
    if (!is_single_path(dl2) &&
	check_path_list(interp, cmd, "second argument", dl2) != TCL_OK)
      return TCL_ERROR;
    if (!is_single_path(dl1) && !is_single_path(dl2)) {
      Tcl_AppendResult(interp, cmd, ": comparing two lists of paths is not"
		       " supported; at least one argument must be a single"
		       " path", NULL);
      return TCL_ERROR;
    }

    retlist = distance_only(dl1, dl2, pnorm);
  }

  if (retlist)
    return(tclPutList(interp, retlist));
  else {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
		     ": error computing distance(s)", NULL);
    return TCL_ERROR;
  }
}

// Extension initialization function
extern "C" int Dtw_Init(Tcl_Interp *interp)
{
  if (Tcl_InitStubs(interp, "9.0", 0) == nullptr) {
    return TCL_ERROR;
  }

  if (Tcl_PkgProvide(interp, "dtw", "1.0") != TCL_OK) {
    return TCL_ERROR;
  }
  

  Tcl_CreateObjCommand(interp,
		       "dtwDistanceOnly",
		       (Tcl_ObjCmdProc *) dtw_distance_only_func, 
		       (ClientData) NULL, NULL);
  return TCL_OK;
}
