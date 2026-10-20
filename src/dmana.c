/*************************************************************************
 *
 *  NAME
 *    dmana.c
 *
 *  DESCRIPTION
 *    Matrix functions which operate on dynLists which are float matrices
 *
 *  AUTHOR
 *    DLS
 *
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "df.h"
#include "dfana.h"

#include <utilc.h>

/*
 * Numerical Recipes helper functions and matrix funcs
 */

static double **dynMatrixToNRCMatrix(DYN_LIST *m);
static void dynMatrixFreeNRCMatrix(double **m, int nrows, int ncols);
static DYN_LIST *dynMatrixFromNRCMatrix(double **m, int nrows, int ncols);
static int dm_gaussj(double **a, int n, double **b, int m);
static int dm_ludcmp(double **a, int n, int *indx, float *d);
static void dm_lubksb(double **a, int n, int *indx, double b[]);


DYN_LIST *dynMatrixIdentity(int nrows)
{
  int i, j;
  DYN_LIST *newmat, *row;

  if (nrows <= 0) return(NULL);

  newmat = dfuCreateDynList(DF_LIST, nrows);
  row = dfuCreateDynList(DF_FLOAT, nrows);
  
  for (i = 0; i < nrows; i++) {
    dfuResetDynList(row);
    for (j = 0; j < nrows; j++) {
      if (i == j) dfuAddDynListFloat(row, 1.0);
      else dfuAddDynListFloat(row, 0.0);
    }
    dfuAddDynListList(newmat, row);
  }

  dfuFreeDynList(row);

  return(newmat);
}

DYN_LIST *dynMatrixZeros(int nrows, int ncols)
{
  int i, j;
  DYN_LIST *newmat, *row;

  if (nrows <= 0 || ncols <= 0) return(NULL);

  newmat = dfuCreateDynList(DF_LIST, nrows);
  row = dfuCreateDynList(DF_FLOAT, ncols);
  
  for (i = 0; i < nrows; i++) {
    dfuResetDynList(row);
    for (j = 0; j < ncols; j++) {
      dfuAddDynListFloat(row, 0.0);
    }
    dfuAddDynListList(newmat, row);
  }
  
  dfuFreeDynList(row);

  return(newmat);
}

DYN_LIST *dynMatrixUrands(int nrows, int ncols)
{
  int i, j;
  DYN_LIST *newmat, *row;

  if (nrows <= 0 || ncols <= 0) return(NULL);

  newmat = dfuCreateDynList(DF_LIST, nrows);
  row = dfuCreateDynList(DF_FLOAT, ncols);
  
  for (i = 0; i < nrows; i++) {
    dfuResetDynList(row);
    for (j = 0; j < ncols; j++) {
      dfuAddDynListFloat(row, frand());
    }
    dfuAddDynListList(newmat, row);
  }
  
  dfuFreeDynList(row);

  return(newmat);
}

DYN_LIST *dynMatrixZrands(int nrows, int ncols)
{
  int i, j;
  DYN_LIST *newmat, *row;

  if (nrows <= 0 || ncols <= 0) return(NULL);

  newmat = dfuCreateDynList(DF_LIST, nrows);
  row = dfuCreateDynList(DF_FLOAT, ncols);
  
  for (i = 0; i < nrows; i++) {
    dfuResetDynList(row);
    for (j = 0; j < ncols; j++) {
      dfuAddDynListFloat(row, zrand());
    }
    dfuAddDynListList(newmat, row);
  }
  
  dfuFreeDynList(row);

  return(newmat);
}


DYN_LIST *dynMatrixRowMeans(DYN_LIST *m)
{
  int i, j;
  int nrows, ncols;
  DYN_LIST *means = NULL, **rows;
  float *rowvals;
  double sum;

  if (!dynListIsMatrix(m)) return NULL;
  if (!dynMatrixDims(m, &nrows, &ncols)) return NULL;

  rows = (DYN_LIST **) DYN_LIST_VALS(m);
  means = dfuCreateDynList(DF_FLOAT, nrows);

  for (i = 0; i < nrows; i++) {
    rowvals = (float *) DYN_LIST_VALS(rows[i]);
    sum = 0.0;
    for (j = 0; j < ncols; j++) {
      sum += rowvals[j];
    }
    dfuAddDynListFloat(means, sum/ncols);
  }
  return(means);
}


DYN_LIST *dynMatrixColMeans(DYN_LIST *m)
{
  int i, j;
  int nrows, ncols;
  DYN_LIST *means = NULL, **rows;
  float *rowvals;
  double *sums;

  if (!dynListIsMatrix(m)) return NULL;
  if (!dynMatrixDims(m, &nrows, &ncols)) return NULL;

  rows = (DYN_LIST **) DYN_LIST_VALS(m);
  means = dfuCreateDynList(DF_FLOAT, ncols);
  sums = (double *) calloc(ncols, sizeof(double));

  for (i = 0; i < nrows; i++) {
    rowvals = (float *) DYN_LIST_VALS(rows[i]);
    for (j = 0; j < ncols; j++) {
      sums[j] += rowvals[j];
    }
  }
  
  for (i = 0; i < ncols; i++) {
    dfuAddDynListFloat(means, sums[i]/nrows);
  }

  free(sums);
  return(means);
}


DYN_LIST *dynMatrixRowSums(DYN_LIST *m)
{
  int i, j;
  int nrows, ncols;
  DYN_LIST *means = NULL, **rows;
  float *rowvals;
  double sum;

  if (!dynListIsMatrix(m)) return NULL;
  if (!dynMatrixDims(m, &nrows, &ncols)) return NULL;

  rows = (DYN_LIST **) DYN_LIST_VALS(m);
  means = dfuCreateDynList(DF_FLOAT, nrows);

  for (i = 0; i < nrows; i++) {
    rowvals = (float *) DYN_LIST_VALS(rows[i]);
    sum = 0.0;
    for (j = 0; j < ncols; j++) {
      sum += rowvals[j];
    }
    dfuAddDynListFloat(means, sum);
  }
  return(means);
}


DYN_LIST *dynMatrixColSums(DYN_LIST *m)
{
  int i, j;
  int nrows, ncols;
  DYN_LIST *means = NULL, **rows;
  float *rowvals;
  double *sums;

  if (!dynListIsMatrix(m)) return NULL;
  if (!dynMatrixDims(m, &nrows, &ncols)) return NULL;

  rows = (DYN_LIST **) DYN_LIST_VALS(m);
  means = dfuCreateDynList(DF_FLOAT, ncols);
  sums = (double *) calloc(ncols, sizeof(double));

  for (i = 0; i < nrows; i++) {
    rowvals = (float *) DYN_LIST_VALS(rows[i]);
    for (j = 0; j < ncols; j++) {
      sums[j] += rowvals[j];
    }
  }
  
  for (i = 0; i < ncols; i++) {
    dfuAddDynListFloat(means, sums[i]);
  }

  free(sums);
  return(means);
}



DYN_LIST *dynMatrixCenterRows(DYN_LIST *m, DYN_LIST *v)
{
  int i, j;
  int nrows, ncols;
  DYN_LIST *newmat = NULL, **rows, *newrow;
  float *rowvals, *centervals;

  if (!dynListIsMatrix(m)) return NULL;
  if (!dynMatrixDims(m, &nrows, &ncols)) return NULL;
  if (DYN_LIST_N(v) != nrows) return NULL;

  newmat = dfuCreateDynList(DF_LIST, nrows);
  rows = (DYN_LIST **) DYN_LIST_VALS(m);
  centervals = (float *) DYN_LIST_VALS(v);

  newrow = dfuCreateDynList(DF_FLOAT, ncols);
  for (i = 0; i < nrows; i++) {
    rowvals = (float *) DYN_LIST_VALS(rows[i]);
    dfuResetDynList(newrow);
    for (j = 0; j < ncols; j++) {
      dfuAddDynListFloat(newrow, rowvals[j]-centervals[i]);
    }
    dfuAddDynListList(newmat, newrow);
  }
  dfuFreeDynList(newrow);

  return(newmat);
}

DYN_LIST *dynMatrixCenterCols(DYN_LIST *m, DYN_LIST *v)
{
  int i, j;
  int nrows, ncols;
  DYN_LIST *newmat = NULL, **rows, *newrow;
  float *rowvals, *centervals;

  if (!dynListIsMatrix(m)) return NULL;
  if (!dynMatrixDims(m, &nrows, &ncols)) return NULL;
  if (DYN_LIST_N(v) != ncols) return NULL;

  newmat = dfuCreateDynList(DF_LIST, nrows);
  rows = (DYN_LIST **) DYN_LIST_VALS(m);
  centervals = (float *) DYN_LIST_VALS(v);

  newrow = dfuCreateDynList(DF_FLOAT, ncols);
  for (i = 0; i < nrows; i++) {
    rowvals = (float *) DYN_LIST_VALS(rows[i]);
    dfuResetDynList(newrow);
    for (j = 0; j < ncols; j++) {
      dfuAddDynListFloat(newrow, rowvals[j]-centervals[j]);
    }
    dfuAddDynListList(newmat, newrow);
  }
  dfuFreeDynList(newrow);

  return(newmat);
}


DYN_LIST *dynMatrixAddFloat(DYN_LIST *m, float k)
{
  int i, j;
  int nrows, ncols;
  DYN_LIST *newmat = NULL, *vec, **m_vals;
  float *row;

  if (!dynListIsMatrix(m)) return (NULL);
  if (!dynMatrixDims(m, &nrows, &ncols)) return NULL;

  m_vals = (DYN_LIST **) DYN_LIST_VALS(m);

  newmat = dfuCreateDynList(DF_LIST, nrows);
  vec = dfuCreateDynList(DF_FLOAT, ncols);

  for (i = 0; i < nrows; i++) {
    row = (float *) DYN_LIST_VALS(m_vals[i]);
    dfuResetDynList(vec);
    for (j = 0; j < ncols; j++) {
      dfuAddDynListFloat(vec, row[j]+k);
    }
    dfuAddDynListList(newmat, vec);
  }
  dfuFreeDynList(vec);
  
  return(newmat);
}

DYN_LIST *dynMatrixSubFloat(DYN_LIST *m, float k)
{
  int i, j;
  int nrows, ncols;
  DYN_LIST *newmat = NULL, *vec, **m_vals;
  float *row;

  if (!dynListIsMatrix(m)) return (NULL);
  if (!dynMatrixDims(m, &nrows, &ncols)) return NULL;

  m_vals = (DYN_LIST **) DYN_LIST_VALS(m);

  newmat = dfuCreateDynList(DF_LIST, nrows);
  vec = dfuCreateDynList(DF_FLOAT, ncols);

  for (i = 0; i < nrows; i++) {
    row = (float *) DYN_LIST_VALS(m_vals[i]);
    dfuResetDynList(vec);
    for (j = 0; j < ncols; j++) {
      dfuAddDynListFloat(vec, row[j]-k);
    }
    dfuAddDynListList(newmat, vec);
  }
  dfuFreeDynList(vec);
  
  return(newmat);
}

DYN_LIST *dynMatrixMultFloat(DYN_LIST *m, float k)
{
  int i, j;
  int nrows, ncols;
  DYN_LIST *newmat = NULL, *vec, **m_vals;
  float *row;

  if (!dynListIsMatrix(m)) return (NULL);
  if (!dynMatrixDims(m, &nrows, &ncols)) return NULL;

  m_vals = (DYN_LIST **) DYN_LIST_VALS(m);

  newmat = dfuCreateDynList(DF_LIST, nrows);
  vec = dfuCreateDynList(DF_FLOAT, ncols);

  for (i = 0; i < nrows; i++) {
    row = (float *) DYN_LIST_VALS(m_vals[i]);
    dfuResetDynList(vec);
    for (j = 0; j < ncols; j++) {
      dfuAddDynListFloat(vec, row[j]*k);
    }
    dfuAddDynListList(newmat, vec);
  }
  dfuFreeDynList(vec);
  
  return(newmat);
}

DYN_LIST *dynMatrixDivFloat(DYN_LIST *m, float k)
{
  int i, j;
  int nrows, ncols;
  DYN_LIST *newmat = NULL, *vec, **m_vals;
  float *row;

  if (!dynListIsMatrix(m)) return (NULL);
  if (k == 0.0) return NULL;
  if (!dynMatrixDims(m, &nrows, &ncols)) return NULL;

  m_vals = (DYN_LIST **) DYN_LIST_VALS(m);

  newmat = dfuCreateDynList(DF_LIST, nrows);
  vec = dfuCreateDynList(DF_FLOAT, ncols);

  for (i = 0; i < nrows; i++) {
    row = (float *) DYN_LIST_VALS(m_vals[i]);
    dfuResetDynList(vec);
    for (j = 0; j < ncols; j++) {
      dfuAddDynListFloat(vec, row[j]/k);
    }
    dfuAddDynListList(newmat, vec);
  }
  dfuFreeDynList(vec);
  
  return(newmat);
}

DYN_LIST *dynMatrixAdd(DYN_LIST *m, DYN_LIST *l)
{
  int i, j;
  int nrows1, nrows2, ncols1, ncols2;
  DYN_LIST *newmat = NULL, *vec, **m1_vals, **m2_vals;
  float *r1, *r2;

  if (dynListIsMatrix(m) && dynListIsMatrix(l)) {
    if (!dynMatrixDims(m, &nrows1, &ncols1) ||
	!dynMatrixDims(l, &nrows2, &ncols2)) {
      return NULL;
    }
    if (ncols1 != ncols2 || nrows1 != nrows2) return(NULL);

    m1_vals = (DYN_LIST **) DYN_LIST_VALS(m);
    m2_vals = (DYN_LIST **) DYN_LIST_VALS(l);
    newmat = dfuCreateDynList(DF_LIST, nrows1);
    vec = dfuCreateDynList(DF_FLOAT, ncols1);

    for (i = 0; i < nrows1; i++) {
      r1 = (float *) DYN_LIST_VALS(m1_vals[i]);
      r2 = (float *) DYN_LIST_VALS(m2_vals[i]);
      dfuResetDynList(vec);
      for (j = 0; j < ncols1; j++) {
	dfuAddDynListFloat(vec, r1[j]+r2[j]);
      }
      dfuAddDynListList(newmat, vec);
    }
    dfuFreeDynList(vec);
  }


  else if (dynListIsMatrix(m) && DYN_LIST_DATATYPE(l) == DF_FLOAT) {
    if (!dynMatrixDims(m, &nrows1, &ncols1)) return NULL;
    if (DYN_LIST_N(l) != ncols1) return NULL;

    m1_vals = (DYN_LIST **) DYN_LIST_VALS(m);
    newmat = dfuCreateDynList(DF_LIST, nrows1);
    vec = dfuCreateDynList(DF_FLOAT, ncols1);
    r2 = (float *) DYN_LIST_VALS(l);
    
    for (i = 0; i < nrows1; i++) {
      r1 = (float *) DYN_LIST_VALS(m1_vals[i]);
      dfuResetDynList(vec);
      for (j = 0; j < ncols1; j++) {
	dfuAddDynListFloat(vec, r1[j]+r2[j]);
      }
      dfuAddDynListList(newmat, vec);
    }
    dfuFreeDynList(vec);
  }
  
  return(newmat);
}

DYN_LIST *dynMatrixSubtract(DYN_LIST *m, DYN_LIST *l)
{
  int i, j;
  int nrows1, nrows2, ncols1, ncols2;
  DYN_LIST *newmat = NULL, *vec, **m1_vals, **m2_vals;
  float *r1, *r2;

  if (dynListIsMatrix(m) && dynListIsMatrix(l)) {
    if (!dynMatrixDims(m, &nrows1, &ncols1) ||
	!dynMatrixDims(l, &nrows2, &ncols2)) {
      return NULL;
    }
    if (ncols1 != ncols2 || nrows1 != nrows2) return(NULL);

    m1_vals = (DYN_LIST **) DYN_LIST_VALS(m);
    m2_vals = (DYN_LIST **) DYN_LIST_VALS(l);
    newmat = dfuCreateDynList(DF_LIST, nrows1);
    vec = dfuCreateDynList(DF_FLOAT, ncols1);

    for (i = 0; i < nrows1; i++) {
      r1 = (float *) DYN_LIST_VALS(m1_vals[i]);
      r2 = (float *) DYN_LIST_VALS(m2_vals[i]);
      dfuResetDynList(vec);
      for (j = 0; j < ncols1; j++) {
	dfuAddDynListFloat(vec, r1[j]-r2[j]);
      }
      dfuAddDynListList(newmat, vec);
    }
    dfuFreeDynList(vec);
  }


  else if (dynListIsMatrix(m) && DYN_LIST_DATATYPE(l) == DF_FLOAT) {
    if (!dynMatrixDims(m, &nrows1, &ncols1)) return NULL;
    if (DYN_LIST_N(l) != ncols1) return NULL;

    m1_vals = (DYN_LIST **) DYN_LIST_VALS(m);
    newmat = dfuCreateDynList(DF_LIST, nrows1);
    vec = dfuCreateDynList(DF_FLOAT, ncols1);
    r2 = (float *) DYN_LIST_VALS(l);
    
    for (i = 0; i < nrows1; i++) {
      r1 = (float *) DYN_LIST_VALS(m1_vals[i]);
      dfuResetDynList(vec);
      for (j = 0; j < ncols1; j++) {
	dfuAddDynListFloat(vec, r1[j]-r2[j]);
      }
      dfuAddDynListList(newmat, vec);
    }
    dfuFreeDynList(vec);
  }
  
  return(newmat);
}




DYN_LIST *dynMatrixMultiply(DYN_LIST *m1, DYN_LIST *m2)
{
  int i, j, k;
  int rows1, rows2, cols1, cols2;
  DYN_LIST *newmat = NULL, *vec, *t, **m1_vals, **m2_vals;
  float *row, *col, *r2;
  double sum;

  if (dynListIsMatrix(m1) && dynListIsMatrix(m2)) {
    if (!dynMatrixDims(m1, &rows1, &cols1) ||
	!dynMatrixDims(m2, &rows2, &cols2)) {
      return NULL;
    }
    if (cols1 != rows2) return(NULL);
    
    /*
     * Create the transpose so we have pointers to each column
     */
    t = dynListTransposeList(m2);
    
    m1_vals = (DYN_LIST **) DYN_LIST_VALS(m1);
    m2_vals = (DYN_LIST **) DYN_LIST_VALS(t);
    
    newmat = dfuCreateDynList(DF_LIST, rows1);
    vec = dfuCreateDynList(DF_FLOAT, cols2);
    
    for (i = 0; i < rows1; i++) {
      row = (float *) DYN_LIST_VALS(m1_vals[i]);
      dfuResetDynList(vec);
      for (j = 0; j < cols2; j++) {
	col = (float *) DYN_LIST_VALS(m2_vals[j]);
	for (k = 0, sum = 0.0; k < rows2; k++) {
	  sum += row[k]*col[k];
	}
	dfuAddDynListFloat(vec, sum);
      }
      dfuAddDynListList(newmat, vec);
    }
    
    dfuFreeDynList(t);
    dfuFreeDynList(vec);
  }
  
  else if (dynListIsMatrix(m1) && DYN_LIST_DATATYPE(m2) == DF_FLOAT) {
    if (!dynMatrixDims(m1, &rows1, &cols1)) return NULL;
    if (DYN_LIST_N(m2) != cols1) return NULL;
    
    m1_vals = (DYN_LIST **) DYN_LIST_VALS(m1);
    newmat = dfuCreateDynList(DF_LIST, rows1);
    vec = dfuCreateDynList(DF_FLOAT, cols1);
    r2 = (float *) DYN_LIST_VALS(m2);
    
    for (i = 0; i < rows1; i++) {
      row = (float *) DYN_LIST_VALS(m1_vals[i]);
      dfuResetDynList(vec);
      for (j = 0; j < cols1; j++) {
	dfuAddDynListFloat(vec, row[j]*r2[j]);
      }
      dfuAddDynListList(newmat, vec);
    }
    dfuFreeDynList(vec);
  }
  
  return(newmat);
}


DYN_LIST *dynMatrixInverse(DYN_LIST *m)
{
  int retval;
  int nrows, ncols;
  DYN_LIST *newmat = NULL;
  double **nrmatrix, **b;


  if (!dynListIsMatrix(m)) return 0;
  if (!dynMatrixDims(m, &nrows, &ncols)) return 0;
  
  /* prepare data for numerical recipes func */
  
  nrmatrix = dynMatrixToNRCMatrix(m);  
  b = dynMatrixToNRCMatrix(m);
  if (!b) return (NULL);

  retval = dm_gaussj(nrmatrix, nrows, b, ncols);

  if (retval) {
    newmat = dynMatrixFromNRCMatrix(nrmatrix, nrows, ncols);
  }
  dynMatrixFreeNRCMatrix(nrmatrix, nrows, ncols);
  dynMatrixFreeNRCMatrix(b, nrows, ncols);

  return(newmat);
}


DYN_LIST *dynMatrixLUInverse(DYN_LIST *m)
{
  int i, j;
  int nrows, ncols;
  DYN_LIST *newmat = NULL;
  double **a, **b;
  double *col;
  int *indx;
  float d;

  if (!dynListIsMatrix(m)) return 0;
  if (!dynMatrixDims(m, &nrows, &ncols)) return 0;
  
  /* prepare data for numerical recipes func */
  
  a = dynMatrixToNRCMatrix(m);  
  indx = (int *) calloc(nrows, sizeof(int));
  dm_ludcmp(a, nrows, indx-1, &d);

  b = dynMatrixToNRCMatrix(m);  

  if (!a) {
    dynMatrixFreeNRCMatrix(a, nrows, ncols);
    free((void *) indx);
    return NULL;
  }

  col = (double *) calloc(nrows+1, sizeof(float));

  for (j = 1; j <= nrows; j++) {
    for (i = 1; i <= nrows; i++) col[i] = 0.0;
    col[j] = 1.0;
    dm_lubksb(a, nrows, indx-1, col);
    for (i = 0; i <= nrows; i++) b[i][j] = col[i];
  }
  
  newmat = dynMatrixFromNRCMatrix(b, nrows, ncols);

  free((void *) indx);
  free((void *) col);
  dynMatrixFreeNRCMatrix(a, nrows, ncols);
  dynMatrixFreeNRCMatrix(b, nrows, ncols);

  return(newmat);
}


DYN_LIST *dynMatrixLudcmp(DYN_LIST *m)
{
  int i;
  int nrows, ncols, retval;
  DYN_LIST *result = NULL, *newmat, *dlist, *ilist;
  int *indx;
  double **nrmatrix;
  float d;
  
  if (!dynListIsMatrix(m)) return 0;
  if (!dynMatrixDims(m, &nrows, &ncols)) return 0;
  
  /* prepare data for numerical recipes func */
  
  nrmatrix = dynMatrixToNRCMatrix(m);  
  if (!nrmatrix) return (NULL);
  
  indx = (int *) calloc(nrows, sizeof(int));
  retval = dm_ludcmp(nrmatrix, nrows, indx-1, &d);
  if (!retval) {
    dynMatrixFreeNRCMatrix(nrmatrix, nrows, ncols);
    free((void *) indx);
    return NULL;
  }

  newmat = dynMatrixFromNRCMatrix(nrmatrix, nrows, ncols);
  dynMatrixFreeNRCMatrix(nrmatrix, nrows, ncols);

  ilist = dfuCreateDynList(DF_LONG, nrows);
  for (i = 0; i < nrows; i++) dfuAddDynListLong(ilist, indx[i]-1);
  free((void *) indx);

  dlist = dfuCreateDynList(DF_LONG, 1);
  dfuAddDynListLong(dlist, (long) d);
  
  result = dfuCreateDynList(DF_LIST, 3);
  dfuAddDynListList(result, newmat);
  dfuAddDynListList(result, ilist);
  dfuAddDynListList(result, dlist);

  dfuFreeDynList(newmat);
  dfuFreeDynList(ilist);
  dfuFreeDynList(dlist);

  return(result);
}

DYN_LIST *dynMatrixDiag(DYN_LIST *m)
{
  int i;
  int nrows, ncols;
  DYN_LIST *result = NULL;
  DYN_LIST **rows;
  float *row;
  
  if (!dynListIsMatrix(m)) return 0;
  if (!dynMatrixDims(m, &nrows, &ncols)) return 0;
  
  result = dfuCreateDynList(DF_FLOAT, nrows);
  rows = (DYN_LIST **) DYN_LIST_VALS(m);
  for (i = 0; i < nrows; i++) {
    row = (float *) DYN_LIST_VALS(rows[i]);
    dfuAddDynListFloat(result, row[i]);
  }
  return(result);
}

int dynMatrixDims(DYN_LIST *m, int *nrows, int *ncols)
{
  DYN_LIST **rows;
  if (!dynListIsMatrix(m)) return 0;

  *nrows = DYN_LIST_N(m);
  rows = (DYN_LIST **) DYN_LIST_VALS(m);
  *ncols = DYN_LIST_N(rows[0]);

  return 1;
}

DYN_LIST *dynMatrixReshape(DYN_LIST *l, int nrows, int ncols)
{
  DYN_LIST *newmat, **newrows, *row;
  int oldnrows = 1, oldncols, i, j, k, r;

  switch (DYN_LIST_DATATYPE(l)) {
  case DF_CHAR:
  case DF_LONG:
  case DF_SHORT:
  case DF_FLOAT:
    if ((nrows * ncols) != (oldncols = DYN_LIST_N(l))) return NULL;
    break;
  case DF_LIST:
    if (!dynListIsMatrix(l)) return (NULL);
    if (!dynMatrixDims(l, &oldnrows, &oldncols)) return NULL;
    if ((nrows * ncols) != (oldnrows * oldncols)) return NULL;
    break;
  default:
    return NULL;
  }

  newmat = dfuCreateDynList(DF_LIST, nrows);
  row = dfuCreateDynList(DF_FLOAT, ncols);
  for (i = 0; i < nrows; i++) 
    dfuAddDynListList(newmat, row);
  dfuFreeDynList(row);
  
  newrows = (DYN_LIST **) DYN_LIST_VALS(newmat);
  switch (DYN_LIST_DATATYPE(l)) {
  case DF_LONG:
    {
      long *oldrow = (long *) DYN_LIST_VALS(l);
      
      for (i = 0, k = 0; i < oldnrows; i++) {
	for (j = 0; j < oldncols; j++, k++) {
	  r = k / ncols;
	  dfuAddDynListFloat(newrows[r], oldrow[j]);
	}
      }
    }
    break;
  case DF_SHORT:
    {
      short *oldrow = (short *) DYN_LIST_VALS(l);
      
      for (i = 0, k = 0; i < oldnrows; i++) {
	for (j = 0; j < oldncols; j++, k++) {
	  r = k / ncols;
	  dfuAddDynListFloat(newrows[r], oldrow[j]);
	}
      }
    }
    break;
  case DF_CHAR:
    {
      unsigned char *oldrow = (unsigned char *) DYN_LIST_VALS(l);
      
      for (i = 0, k = 0; i < oldnrows; i++) {
	for (j = 0; j < oldncols; j++, k++) {
	  r = k / ncols;
	  dfuAddDynListFloat(newrows[r], oldrow[j]);
	}
      }
    }
    break;
  case DF_FLOAT:
    {
      float *oldrow = (float *) DYN_LIST_VALS(l);
    
      for (i = 0, k = 0; i < oldnrows; i++) {
	for (j = 0; j < oldncols; j++, k++) {
	  r = k / ncols;
	  dfuAddDynListFloat(newrows[r], oldrow[j]);
	}
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **oldrows = (DYN_LIST **) DYN_LIST_VALS(l);
      float *oldrow;
      
      for (i = 0, k = 0; i < oldnrows; i++) {
	oldrow = (float *) DYN_LIST_VALS(oldrows[i]);
	for (j = 0; j < oldncols; j++, k++) {
	  r = k / ncols;
	  dfuAddDynListFloat(newrows[r], oldrow[j]);
	}
      }
    }
    break;
  }
  return(newmat);
}


static double **dynMatrixToNRCMatrix(DYN_LIST *m)
{
  int nrows, ncols, i, j;
  double **nrc_matrix;
  float *dyn_row;
  DYN_LIST **dyn_rows;

  if (!dynMatrixDims(m, &nrows, &ncols)) return 0;
  
  nrc_matrix = (double **) calloc(nrows+1, sizeof(double *));
  if (!nrc_matrix) return NULL;

  for (i = 1; i <= nrows; i++) {
    nrc_matrix[i] = (double *) calloc(ncols+1, sizeof(double));
    if (!nrc_matrix[i]) return NULL;
  }

  dyn_rows = (DYN_LIST **) DYN_LIST_VALS(m);

  for (i = 1; i <= nrows; i++) {
    dyn_row = (float *) DYN_LIST_VALS(dyn_rows[i-1]);
    for (j = 1; j <= ncols; j++) {
      nrc_matrix[i][j] = dyn_row[j-1];
    }
  }
  return (nrc_matrix);
}

static void dynMatrixFreeNRCMatrix(double **m, int nrows, int ncols)
{
  int i;
  if (!m) return;
  for (i = 1; i <= nrows; i++) {
    if (m[i]) free((void *) m[i]);
  }
  free(m);
}

static DYN_LIST *dynMatrixFromNRCMatrix(double **m, int nrows, int ncols)
{
  int i, j;
  DYN_LIST *newmat, *row;

  if (nrows <= 0 || ncols <= 0) return(NULL);

  newmat = dfuCreateDynList(DF_LIST, nrows);
  row = dfuCreateDynList(DF_FLOAT, ncols);
  
  for (i = 0; i < nrows; i++) {
    dfuResetDynList(row);
    for (j = 0; j < ncols; j++) {
      dfuAddDynListFloat(row, m[i+1][j+1]);
    }
    dfuAddDynListList(newmat, row);
  }
  
  dfuFreeDynList(row);

  return(newmat);
}




/*
 * Taken from Numerical Recipes
 */

#define SWAP(a,b) {temp=(a);(a)=(b);(b)=temp;}

static int dm_gaussj(double **a, int n, double **b, int m)
{
  int *indxc,*indxr,*ipiv;
  int i,j,k,l,ll;
  float big,dum,pivinv,temp;
  int icol = 0, irow = 0;
  
  indxc=(int *) calloc(n+1, sizeof(int));
  indxr=(int *) calloc(n+1, sizeof(int));
  ipiv=(int *) calloc(n+1, sizeof(int));

  if (!indxc || !indxr || !ipiv) return 0;

  for (j=1;j<=n;j++) ipiv[j]=0;
  for (i=1;i<=n;i++) {
    big=0.0;
    for (j=1;j<=n;j++)
      if (ipiv[j] != 1)
	for (k=1;k<=n;k++) {
	  if (ipiv[k] == 0) {
	    if (fabs(a[j][k]) >= big) {
	      big=fabs(a[j][k]);
	      irow=j;
	      icol=k;
	    }
	  } else if (ipiv[k] > 1) goto error;
	}
    ++(ipiv[icol]);
    if (irow != icol) {
      for (l=1;l<=n;l++) SWAP(a[irow][l],a[icol][l])
	for (l=1;l<=m;l++) SWAP(b[irow][l],b[icol][l])
    }
    indxr[i]=irow;
    indxc[i]=icol;
    if (a[icol][icol] == 0.0) goto error;
    pivinv=1.0/a[icol][icol];
    a[icol][icol]=1.0;
    for (l=1;l<=n;l++) a[icol][l] *= pivinv;
    for (l=1;l<=m;l++) b[icol][l] *= pivinv;
    for (ll=1;ll<=n;ll++)
      if (ll != icol) {
	dum=a[ll][icol];
	a[ll][icol]=0.0;
	for (l=1;l<=n;l++) a[ll][l] -= a[icol][l]*dum;
	for (l=1;l<=m;l++) b[ll][l] -= b[icol][l]*dum;
      }
  }
  for (l=n;l>=1;l--) {
    if (indxr[l] != indxc[l])
      for (k=1;k<=n;k++)
	SWAP(a[k][indxr[l]],a[k][indxc[l]]);
  }
  free((void *) ipiv);
  free((void *) indxr);
  free((void *) indxc);
  return 1;

 error:
  free((void *) ipiv);
  free((void *) indxr);
  free((void *) indxc);
  return 0;
}
#undef SWAP

#define TINY 1.0e-20;
static int dm_ludcmp(double **a, int n, int *indx, float *d)
{
  int i,imax = 0,j,k;
  float big,dum,sum,temp;
  float *vv;
  
  vv=(float *) calloc(n+1, sizeof(float));
  *d=1.0;
  for (i=1;i<=n;i++) {
    big=0.0;
    for (j=1;j<=n;j++)
      if ((temp=fabs(a[i][j])) > big) big=temp;
    if (big == 0.0) {
      free((void *) vv);
      return 0;
    }
    vv[i]=1.0/big;
  }
  for (j=1;j<=n;j++) {
    for (i=1;i<j;i++) {
      sum=a[i][j];
      for (k=1;k<i;k++) sum -= a[i][k]*a[k][j];
      a[i][j]=sum;
    }
    big=0.0;
    for (i=j;i<=n;i++) {
      sum=a[i][j];
      for (k=1;k<j;k++)
	sum -= a[i][k]*a[k][j];
      a[i][j]=sum;
      if ( (dum=vv[i]*fabs(sum)) >= big) {
	big=dum;
	imax=i;
      }
    }
    if (j != imax) {
      for (k=1;k<=n;k++) {
	dum=a[imax][k];
	a[imax][k]=a[j][k];
	a[j][k]=dum;
      }
      *d = -(*d);
      vv[imax]=vv[j];
    }
    indx[j]=imax;
    if (a[j][j] == 0.0) a[j][j]=TINY;
    if (j != n) {
      dum=1.0/(a[j][j]);
      for (i=j+1;i<=n;i++) a[i][j] *= dum;
    }
  }
  free ((void *) vv);
  return 1;
}
#undef TINY


void dm_lubksb(double **a, int n, int *indx, double b[])
{
  int i,ii=0,ip,j;
  double sum;
  
  for (i=1;i<=n;i++) {
    ip=indx[i];
    sum=b[ip];
    b[ip]=b[i];
    if (ii)
      for (j=ii;j<=i-1;j++) sum -= a[i][j]*b[j];
    else if (sum) ii=i;
    b[i]=sum;
  }
  for (i=n;i>=1;i--) {
    sum=b[i];
    for (j=i+1;j<=n;j++) sum -= a[i][j]*b[j];
    b[i]=sum/a[i][i];
  }
}
