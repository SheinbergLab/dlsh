/*
 * dlwide.c -- see dlwide.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <df.h>
#include "dlwide.h"
#include "dfana.h"

/*********************************************************************/
/*                          Type table                               */
/*********************************************************************/

int dlwIsNumeric(int t)
{
  switch (t) {
#define X(DF, CT, SUF) case DF:
    DLW_NUMERIC_TYPES(X)
#undef X
    return 1;
  default:
    return 0;
  }
}

int dlwIsInteger(int t)
{
  return t == DF_CHAR || t == DF_SHORT || t == DF_LONG || t == DF_INT64;
}

int dlwIsWide(int t)
{
  return t == DF_INT64 || t == DF_DOUBLE;
}

int dlwHasWideLeaf(DYN_LIST *dl)
{
  int i;
  if (!dl) return 0;
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_INT64:
  case DF_DOUBLE:
    return 1;
  case DF_LIST: {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++)
      if (dlwHasWideLeaf(vals[i])) return 1;
    return 0;
  }
  default:
    return 0;
  }
}

static int rank(int t)
{
  switch (t) {
  case DF_CHAR:   return 0;
  case DF_SHORT:  return 1;
  case DF_LONG:   return 2;
  case DF_INT64:  return 3;
  case DF_FLOAT:  return 4;
  case DF_DOUBLE: return 5;
  default:        return -1;
  }
}

int dlwPromote(int a, int b)
{
  if (rank(a) < 0 || rank(b) < 0) return -1;
  /* float cannot represent an int64: widen to double instead */
  if ((a == DF_INT64 && b == DF_FLOAT) || (a == DF_FLOAT && b == DF_INT64))
    return DF_DOUBLE;
  return rank(a) >= rank(b) ? a : b;
}

/*********************************************************************/
/*                          Readers                                  */
/*********************************************************************/

#define X(DF, CT, SUF)							\
  static double  getd_##SUF(const void *v, int i) { return (double)  ((const CT *) v)[i]; } \
  static int64_t geti_##SUF(const void *v, int i) { return (int64_t) ((const CT *) v)[i]; }
DLW_NUMERIC_TYPES(X)
#undef X

dlw_getd_fn dlwDoubleReader(int t)
{
  switch (t) {
#define X(DF, CT, SUF) case DF: return getd_##SUF;
    DLW_NUMERIC_TYPES(X)
#undef X
  default: return NULL;
  }
}

dlw_geti_fn dlwInt64Reader(int t)
{
  switch (t) {
#define X(DF, CT, SUF) case DF: return geti_##SUF;
    DLW_NUMERIC_TYPES(X)
#undef X
  default: return NULL;
  }
}

void dlwAppendDouble(DYN_LIST *dl, double v)
{
  switch (DYN_LIST_DATATYPE(dl)) {
#define X(DF, CT, SUF) case DF: dfuAddDynList##SUF(dl, (CT) v); break;
    DLW_NUMERIC_TYPES(X)
#undef X
  default: break;
  }
}

void dlwAppendInt64(DYN_LIST *dl, int64_t v)
{
  switch (DYN_LIST_DATATYPE(dl)) {
#define X(DF, CT, SUF) case DF: dfuAddDynList##SUF(dl, (CT) v); break;
    DLW_NUMERIC_TYPES(X)
#undef X
  default: break;
  }
}

/*********************************************************************/
/*                          Arithmetic                               */
/*********************************************************************/

/* Division by zero yields 0, as the int and float arms always have. */

DYN_LIST *dlwArith(DYN_LIST *l1, DYN_LIST *l2, int func, int copymode, int length)
{
  int i, rtype = dlwPromote(DYN_LIST_DATATYPE(l1), DYN_LIST_DATATYPE(l2));
  const void *v1 = DYN_LIST_VALS(l1), *v2 = DYN_LIST_VALS(l2);
  int s1 = (copymode == 1) ? 0 : 1;	/* index stride: 0 broadcasts */
  int s2 = (copymode == 2) ? 0 : 1;

  if (rtype < 0) return NULL;
  if (!length) return dfuCreateDynList(rtype, 1);

  if (dlwIsInteger(rtype)) {
    /* Both operands integer, at least one int64 (a narrower pair never
       gets here): compute in int64, store as int64. */
    dlw_geti_fn g1 = dlwInt64Reader(DYN_LIST_DATATYPE(l1));
    dlw_geti_fn g2 = dlwInt64Reader(DYN_LIST_DATATYPE(l2));
    int64_t *out = (int64_t *) calloc(length, sizeof(int64_t));
    int64_t a, b;
    if (!out) return NULL;
#define LOOP(EXPR) for (i = 0; i < length; i++) { a = g1(v1, i*s1); b = g2(v2, i*s2); out[i] = (EXPR); }
    switch (func) {
    case DL_MATH_ADD:   LOOP(a + b); break;
    case DL_MATH_SUB:   LOOP(a - b); break;
    case DL_MATH_MUL:   LOOP(a * b); break;
    case DL_MATH_DIV:   LOOP(b ? a / b : 0); break;
    case DL_MATH_MIN:   LOOP(a < b ? a : b); break;
    case DL_MATH_MAX:   LOOP(a > b ? a : b); break;
    case DL_MATH_POW:   LOOP((int64_t) pow((double) a, (double) b)); break;
    case DL_MATH_ATAN2: LOOP((int64_t) atan2((double) a, (double) b)); break;
    case DL_MATH_FMOD:  LOOP((int64_t) fmod((double) a, (double) b)); break;
    default: free(out); return NULL;
    }
#undef LOOP
    return dfuCreateDynListWithVals(DF_INT64, length, out);
  }
  else {
    dlw_getd_fn g1 = dlwDoubleReader(DYN_LIST_DATATYPE(l1));
    dlw_getd_fn g2 = dlwDoubleReader(DYN_LIST_DATATYPE(l2));
    double *out = (double *) calloc(length, sizeof(double));
    double a, b;
    if (!out) return NULL;
#define LOOP(EXPR) for (i = 0; i < length; i++) { a = g1(v1, i*s1); b = g2(v2, i*s2); out[i] = (EXPR); }
    switch (func) {
    case DL_MATH_ADD:   LOOP(a + b); break;
    case DL_MATH_SUB:   LOOP(a - b); break;
    case DL_MATH_MUL:   LOOP(a * b); break;
    case DL_MATH_DIV:   LOOP(b != 0.0 ? a / b : 0.0); break;
    case DL_MATH_MIN:   LOOP(a < b ? a : b); break;
    case DL_MATH_MAX:   LOOP(a > b ? a : b); break;
    case DL_MATH_POW:   LOOP(pow(a, b)); break;
    case DL_MATH_ATAN2: LOOP(atan2(a, b)); break;
    case DL_MATH_FMOD:  LOOP(fmod(a, b)); break;
    default: free(out); return NULL;
    }
#undef LOOP
    /* rtype is DF_DOUBLE here: the only floating result a wide operand
       can produce.  (A float/float pair never reaches this file.) */
    return dfuCreateDynListWithVals(DF_DOUBLE, length, out);
  }
}

/*********************************************************************/
/*                          Relations                                */
/*********************************************************************/

/*
 * Comparisons answer 0/1 in a DF_LONG list (or the matching indices), as
 * the old arms do.  Integer pairs compare exactly in int64; anything with a
 * floating operand compares in double.  DL_RELATION_MOD is the odd one out:
 * it is arithmetic, and returns int64 for integer pairs and double (fmod)
 * otherwise -- the old arms truncated both operands to int first, which
 * is exactly what a wide operand cannot afford.
 */
DYN_LIST *dlwRelation(DYN_LIST *l1, DYN_LIST *l2, int op,
		      int copymode, int length, int want_indices)
{
  int i, n = 0;
  int t1 = DYN_LIST_DATATYPE(l1), t2 = DYN_LIST_DATATYPE(l2);
  const void *v1 = DYN_LIST_VALS(l1), *v2 = DYN_LIST_VALS(l2);
  int s1 = (copymode == 1) ? 0 : 1, s2 = (copymode == 2) ? 0 : 1;
  int both_int = dlwIsInteger(t1) && dlwIsInteger(t2);
  int *out;

  if (!dlwIsNumeric(t1) || !dlwIsNumeric(t2)) return NULL;
  if (!length) return dfuCreateDynList(DF_LONG, 1);

  if (op == DL_RELATION_MOD) {
    if (both_int) {
      dlw_geti_fn g1 = dlwInt64Reader(t1), g2 = dlwInt64Reader(t2);
      int64_t *m = (int64_t *) calloc(length, sizeof(int64_t));
      if (!m) return NULL;
      for (i = 0; i < length; i++) {
	int64_t a = g1(v1, i*s1), b = g2(v2, i*s2);
	m[i] = b ? a % b : 0;
      }
      return dfuCreateDynListWithVals(DF_INT64, length, m);
    }
    else {
      dlw_getd_fn g1 = dlwDoubleReader(t1), g2 = dlwDoubleReader(t2);
      double *m = (double *) calloc(length, sizeof(double));
      if (!m) return NULL;
      for (i = 0; i < length; i++) m[i] = fmod(g1(v1, i*s1), g2(v2, i*s2));
      return dfuCreateDynListWithVals(DF_DOUBLE, length, m);
    }
  }

  out = (int *) calloc(length, sizeof(int));
  if (!out) return NULL;

#define CMP_LOOP(TYPE, READER)						\
  {									\
    TYPE a, b;								\
    switch (op) {							\
    case DL_RELATION_OR:  for (i = 0; i < length; i++) { a = READER(t1)(v1, i*s1); b = READER(t2)(v2, i*s2); out[i] = (a != 0) || (b != 0); } break; \
    case DL_RELATION_AND: for (i = 0; i < length; i++) { a = READER(t1)(v1, i*s1); b = READER(t2)(v2, i*s2); out[i] = (a != 0) && (b != 0); } break; \
    case DL_RELATION_EQ:  for (i = 0; i < length; i++) { a = READER(t1)(v1, i*s1); b = READER(t2)(v2, i*s2); out[i] = a == b; } break; \
    case DL_RELATION_NE:  for (i = 0; i < length; i++) { a = READER(t1)(v1, i*s1); b = READER(t2)(v2, i*s2); out[i] = a != b; } break; \
    case DL_RELATION_LT:  for (i = 0; i < length; i++) { a = READER(t1)(v1, i*s1); b = READER(t2)(v2, i*s2); out[i] = a <  b; } break; \
    case DL_RELATION_LTE: for (i = 0; i < length; i++) { a = READER(t1)(v1, i*s1); b = READER(t2)(v2, i*s2); out[i] = a <= b; } break; \
    case DL_RELATION_GT:  for (i = 0; i < length; i++) { a = READER(t1)(v1, i*s1); b = READER(t2)(v2, i*s2); out[i] = a >  b; } break; \
    case DL_RELATION_GTE: for (i = 0; i < length; i++) { a = READER(t1)(v1, i*s1); b = READER(t2)(v2, i*s2); out[i] = a >= b; } break; \
    default: free(out); return NULL;					\
    }									\
  }
  if (both_int) CMP_LOOP(int64_t, dlwInt64Reader)
  else          CMP_LOOP(double,  dlwDoubleReader)
#undef CMP_LOOP

  if (!want_indices) return dfuCreateDynListWithVals(DF_LONG, length, out);

  /* compact to the indices of the true elements */
  for (i = 0; i < length; i++) if (out[i]) out[n++] = i;
  if (!n) { free(out); return dfuCreateDynList(DF_LONG, 1); }
  return dfuCreateDynListWithVals(DF_LONG, n, out);
}

/*********************************************************************/
/*                    Collecting per-row scalars                     */
/*********************************************************************/

DYN_LIST *dlwScalarsToList(DYN_LIST *working)
{
  int i, n = DYN_LIST_N(working), rtype = DF_LONG;
  int any_float = 0, any_int64 = 0, any_double = 0;
  DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(working);
  DYN_LIST *list;

  for (i = 0; i < n; i++) {
    if (!vals[i] || DYN_LIST_N(vals[i]) == 0) continue;
    switch (DYN_LIST_DATATYPE(vals[i])) {
    case DF_FLOAT:  any_float = 1;  break;
    case DF_INT64:  any_int64 = 1;  break;
    case DF_DOUBLE: any_double = 1; break;
    default: break;
    }
  }
  if (any_double || (any_float && any_int64)) rtype = DF_DOUBLE;
  else if (any_float) rtype = DF_FLOAT;
  else if (any_int64) rtype = DF_INT64;

  list = dfuCreateDynList(rtype, n ? n : 1);
  if (!list) return NULL;
  for (i = 0; i < n; i++) {
    if (!vals[i] || DYN_LIST_N(vals[i]) == 0) { dlwAppendInt64(list, 0); continue; }
    if (dlwIsInteger(rtype)) {
      dlw_geti_fn g = dlwInt64Reader(DYN_LIST_DATATYPE(vals[i]));
      dlwAppendInt64(list, g ? g(DYN_LIST_VALS(vals[i]), 0) : 0);
    }
    else {
      dlw_getd_fn g = dlwDoubleReader(DYN_LIST_DATATYPE(vals[i]));
      dlwAppendDouble(list, g ? g(DYN_LIST_VALS(vals[i]), 0) : 0.0);
    }
  }
  return list;
}
