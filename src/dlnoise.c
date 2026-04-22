/*************************************************************************
 *
 *  NAME
 *    dlnoise.c
 *
 *  DESCRIPTION
 *    Vectorized simplex-noise sampling for dlsh DYN_LISTs.
 *
 *    dl_simplexNoise2D <seed> <x_list> <y_list>
 *    dl_simplexNoise3D <seed> <x_list> <y_list> <z_list>
 *    dl_simplexNoise4D <seed> <x_list> <y_list> <z_list> <w_list>
 *
 *    Returns a DF_FLOAT DYN_LIST of noise samples, one per broadcast
 *    point. Any input list of length 1 is broadcast over the longest
 *    input. All inputs must share a common broadcast length otherwise.
 *
 *  AUTHOR
 *    DLS
 *
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <tcl.h>

#include "df.h"
#include "tcl_dl.h"
#include "open-simplex-noise.h"

static double dlnoise_get_double(DYN_LIST *dl, int i)
{
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_FLOAT:  return (double) ((float  *) DYN_LIST_VALS(dl))[i];
  case DF_LONG:   return (double) ((int    *) DYN_LIST_VALS(dl))[i];
  case DF_SHORT:  return (double) ((short  *) DYN_LIST_VALS(dl))[i];
  case DF_CHAR:   return (double) ((unsigned char *) DYN_LIST_VALS(dl))[i];
  default:        return 0.0;
  }
}

static int dlnoise_is_numeric(DYN_LIST *dl)
{
  int t = DYN_LIST_DATATYPE(dl);
  return (t == DF_FLOAT || t == DF_LONG || t == DF_SHORT || t == DF_CHAR);
}

/* Broadcast length across dim input lists.
 * Rules: every input must be length 1 or length L (the broadcast len).
 * Returns L, or -1 if inputs are incompatible / empty.
 */
static int dlnoise_broadcast_len(int dim, DYN_LIST **lists)
{
  int i, L = 1;
  for (i = 0; i < dim; i++) {
    int n = DYN_LIST_N(lists[i]);
    if (n <= 0) return -1;
    if (n != 1) {
      if (L == 1) L = n;
      else if (n != L) return -1;
    }
  }
  return L;
}

/* Core vectorized sampler. Caller supplies 2/3/4 coordinate lists. */
static DYN_LIST *dlnoise_sample(int dim, int64_t seed, DYN_LIST **lists)
{
  int i, d, L;
  float *out;
  struct osn_context *ctx = NULL;
  double coords[4] = { 0.0, 0.0, 0.0, 0.0 };
  int idx_stride[4];		/* 0 if input is scalar, 1 if vector */

  if (dim < 2 || dim > 4) return NULL;
  L = dlnoise_broadcast_len(dim, lists);
  if (L < 0) return NULL;
  for (i = 0; i < dim; i++) {
    if (!dlnoise_is_numeric(lists[i])) return NULL;
    idx_stride[i] = (DYN_LIST_N(lists[i]) == 1) ? 0 : 1;
  }

  if (open_simplex_noise(seed, &ctx) != 0) return NULL;

  out = (float *) calloc(L, sizeof(float));
  if (!out) { open_simplex_noise_free(ctx); return NULL; }

  for (i = 0; i < L; i++) {
    for (d = 0; d < dim; d++) {
      coords[d] = dlnoise_get_double(lists[d], i * idx_stride[d]);
    }
    switch (dim) {
    case 2:
      out[i] = (float) open_simplex_noise2(ctx, coords[0], coords[1]);
      break;
    case 3:
      out[i] = (float) open_simplex_noise3(ctx, coords[0], coords[1], coords[2]);
      break;
    case 4:
      out[i] = (float) open_simplex_noise4(ctx, coords[0], coords[1],
                                           coords[2], coords[3]);
      break;
    }
  }
  open_simplex_noise_free(ctx);

  return dfuCreateDynListWithVals(DF_FLOAT, L, out);
}

static int tclSimplexNoise(ClientData data, Tcl_Interp *interp,
                           int argc, char *argv[])
{
  int dim = (int)(intptr_t) data;
  int seed_int;
  DYN_LIST *lists[4];
  DYN_LIST *result;
  int i;

  if (argc != dim + 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " seed x y",
                     (dim >= 3) ? " z" : "",
                     (dim >= 4) ? " w" : "",
                     (char *) NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetInt(interp, argv[1], &seed_int) != TCL_OK) return TCL_ERROR;

  for (i = 0; i < dim; i++) {
    if (tclFindDynList(interp, argv[i + 2], &lists[i]) != TCL_OK)
      return TCL_ERROR;
  }

  result = dlnoise_sample(dim, (int64_t) seed_int, lists);
  if (!result) {
    Tcl_AppendResult(interp, argv[0],
                     ": incompatible list lengths or non-numeric type",
                     (char *) NULL);
    return TCL_ERROR;
  }
  return tclPutList(interp, result);
}

int DlNoise_Init(Tcl_Interp *interp)
{
  Tcl_CreateCommand(interp, "dl_simplexNoise2D",
                    (Tcl_CmdProc *) tclSimplexNoise,
                    (ClientData)(intptr_t) 2, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "dl_simplexNoise3D",
                    (Tcl_CmdProc *) tclSimplexNoise,
                    (ClientData)(intptr_t) 3, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand(interp, "dl_simplexNoise4D",
                    (Tcl_CmdProc *) tclSimplexNoise,
                    (ClientData)(intptr_t) 4, (Tcl_CmdDeleteProc *) NULL);
  return TCL_OK;
}
