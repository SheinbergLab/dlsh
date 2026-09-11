/*
 * dlwide.h -- numeric kernels that work on ANY element type through a type
 * table, written for the 8-byte types (DF_INT64, DF_DOUBLE) and used for
 * every operation that involves one.
 *
 * Background.  dlarith.c and dfana.c dispatch on element type with one
 * hand-written arm per type; the binary operators expand to a type x
 * copy-mode x operator product that is 1300 lines for four types and grows
 * quadratically.  This file is the alternative: DLW_NUMERIC_TYPES lists the
 * element types once, per-type readers are instantiated from it, and each
 * kernel is written once in terms of "read element i as double/int64".  A
 * new element type is one line in the table.
 *
 * For now the old per-type code still handles the pre-2026 types (so its
 * behaviour is provably unchanged: the command-surface differential is
 * empty); the callers route to these kernels only when an operand is
 * wide.  Once the kernels have been exercised, the old arms can be
 * retired onto them.
 *
 * Promotion.  dlwPromote(a, b) gives the result type of a binary operation:
 * rank char < short < long < int64 < float < double, except that int64
 * with float promotes to double, because float cannot hold an int64.
 */

#ifndef DLWIDE_H
#define DLWIDE_H

#include <stdint.h>
#include <df.h>

#ifdef __cplusplus
extern "C" {
#endif

/* X(DFTYPE, CTYPE, SUFFIX) for every numeric element type, narrowest first */
#define DLW_NUMERIC_TYPES(X)			\
  X(DF_CHAR,   char,    Char)			\
  X(DF_SHORT,  short,   Short)			\
  X(DF_LONG,   int,     Long)			\
  X(DF_INT64,  int64_t, Int64)			\
  X(DF_FLOAT,  float,   Float)			\
  X(DF_DOUBLE, double,  Double)

int dlwIsNumeric(int datatype);	/* one of the six above */
int dlwIsInteger(int datatype);	/* char, short, long, int64 */
int dlwIsWide(int datatype);	/* int64 or double */
int dlwPromote(int a, int b);	/* common result type, or -1 */

/* Element readers: convert element i of a typed value array */
typedef double  (*dlw_getd_fn)(const void *vals, int i);
typedef int64_t (*dlw_geti_fn)(const void *vals, int i);
dlw_getd_fn dlwDoubleReader(int datatype);	/* NULL if not numeric */
dlw_geti_fn dlwInt64Reader(int datatype);

/* Append a value to a list of any numeric type, converting on the way */
void dlwAppendDouble(DYN_LIST *dl, double v);
void dlwAppendInt64(DYN_LIST *dl, int64_t v);

/*
 * Kernels.  All take FLAT numeric lists; the callers in dlarith.c/dfana.c
 * do the list-of-lists recursion and broadcasting decisions exactly as
 * they do for the old types, then hand the leaves here.
 *
 * copymode: 0 elementwise (equal lengths), 1 l1 is a scalar, 2 l2 is a
 * scalar; length is the output length.
 */
DYN_LIST *dlwArith(DYN_LIST *l1, DYN_LIST *l2, int func,
		   int copymode, int length);
DYN_LIST *dlwRelation(DYN_LIST *l1, DYN_LIST *l2, int op,
		      int copymode, int length, int want_indices);

/* A list of one-element results (as dynListSumProdLists and friends build)
   collapsed into one flat list of the narrowest type that holds them all;
   an empty entry contributes 0. */
DYN_LIST *dlwScalarsToList(DYN_LIST *working);

#ifdef __cplusplus
}
#endif
#endif /* DLWIDE_H */
