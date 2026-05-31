/*
 * dgz_roundtrip.c -- self-contained regression test for the in-memory gzip
 * reader (dguGzipFileToStruct).  Builds a DYN_GROUP exercising every scalar
 * datatype plus a RAGGED nested list, writes it gzip-compressed via the
 * standard buffer path (the same one dg_write uses), reads it back with the
 * in-memory reader, and asserts the round-tripped group is identical.
 *
 * Exit 0 = identical; nonzero = mismatch/error (suitable for ctest).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <df.h>
#include <dynio.h>

extern DYN_GROUP *dfuCreateDynGroup(int);
extern void       dfuFreeDynGroup(DYN_GROUP *);
extern DYN_LIST  *dfuCreateDynListWithVals(int datatype, int n, void *vals);
extern int        dfuAddDynGroupExistingList(DYN_GROUP *, char *name, DYN_LIST *);
extern void       dgInitBuffer(void);
extern void       dgCloseBuffer(void);
extern void       dgRecordDynGroup(DYN_GROUP *);
extern int        dgWriteBufferCompressed(char *filename);

static int cmp_list(DYN_LIST *a, DYN_LIST *b)
{
  int i, n;
  if (strcmp(DYN_LIST_NAME(a), DYN_LIST_NAME(b))) {
    fprintf(stderr, "  name mismatch: '%s' vs '%s'\n",
	    DYN_LIST_NAME(a), DYN_LIST_NAME(b)); return 1;
  }
  if (DYN_LIST_DATATYPE(a) != DYN_LIST_DATATYPE(b)) {
    fprintf(stderr, "  '%s' datatype %d vs %d\n",
	    DYN_LIST_NAME(a), DYN_LIST_DATATYPE(a), DYN_LIST_DATATYPE(b)); return 1;
  }
  if (DYN_LIST_N(a) != DYN_LIST_N(b)) {
    fprintf(stderr, "  '%s' length %d vs %d\n",
	    DYN_LIST_NAME(a), DYN_LIST_N(a), DYN_LIST_N(b)); return 1;
  }
  n = DYN_LIST_N(a);
  switch (DYN_LIST_DATATYPE(a)) {
  case DF_LONG: { int   *x=(int*)DYN_LIST_VALS(a),   *y=(int*)DYN_LIST_VALS(b);
    for (i=0;i<n;i++) if (x[i]!=y[i]) return 1; break; }
  case DF_SHORT:{ short *x=(short*)DYN_LIST_VALS(a), *y=(short*)DYN_LIST_VALS(b);
    for (i=0;i<n;i++) if (x[i]!=y[i]) return 1; break; }
  case DF_CHAR: { char  *x=(char*)DYN_LIST_VALS(a),  *y=(char*)DYN_LIST_VALS(b);
    for (i=0;i<n;i++) if (x[i]!=y[i]) return 1; break; }
  case DF_FLOAT:{ float *x=(float*)DYN_LIST_VALS(a), *y=(float*)DYN_LIST_VALS(b);
    for (i=0;i<n;i++) if (memcmp(&x[i],&y[i],sizeof(float))) return 1; break; }
  case DF_STRING:{ char **x=(char**)DYN_LIST_VALS(a),**y=(char**)DYN_LIST_VALS(b);
    for (i=0;i<n;i++) if (strcmp(x[i]?x[i]:"", y[i]?y[i]:"")) return 1; break; }
  case DF_LIST: { DYN_LIST **x=(DYN_LIST**)DYN_LIST_VALS(a),
                          **y=(DYN_LIST**)DYN_LIST_VALS(b);
    for (i=0;i<n;i++) if (cmp_list(x[i],y[i])) return 1; break; }
  default: break;
  }
  return 0;
}

int main(int argc, char **argv)
{
  const char *tmp = (argc > 1) ? argv[1] : "dgz_roundtrip_test.dgz";
  DYN_GROUP *a, *b;
  int i, rc = 0;

  /* dfuCreateDynListWithVals() takes OWNERSHIP of the value buffer
     (DYN_LIST_VALS = vals, no copy; freed later by dfuFreeDynGroup), so
     every buffer here MUST be heap-allocated -- including the string array
     (and each string) and the sublist-pointer array for the nested list. */
  int   *longs = (int *)   malloc(4 * sizeof(int));
  short *shrts = (short *) malloc(3 * sizeof(short));
  float *flts  = (float *) malloc(3 * sizeof(float));
  char **strs  = (char **) malloc(3 * sizeof(char *));
  float *s0    = (float *) malloc(2 * sizeof(float));   /* ragged sublists */
  float *s1    = (float *) malloc(3 * sizeof(float));
  DYN_LIST **subs = (DYN_LIST **) malloc(2 * sizeof(DYN_LIST *));

  longs[0]=1; longs[1]=2; longs[2]=3; longs[3]=-7;
  shrts[0]=10; shrts[1]=-20; shrts[2]=30;
  flts[0]=1.5f; flts[1]=-2.25f; flts[2]=3.14159f;
  strs[0]=strdup("alpha"); strs[1]=strdup("beta"); strs[2]=strdup("");
  s0[0]=1.f; s0[1]=2.f;
  s1[0]=3.f; s1[1]=4.f; s1[2]=5.f;

  a = dfuCreateDynGroup(8);
  strncpy(DYN_GROUP_NAME(a), "rttest", DYN_GROUP_NAME_SIZE-1);
  dfuAddDynGroupExistingList(a, "longs",   dfuCreateDynListWithVals(DF_LONG,  4, longs));
  dfuAddDynGroupExistingList(a, "shorts",  dfuCreateDynListWithVals(DF_SHORT, 3, shrts));
  dfuAddDynGroupExistingList(a, "floats",  dfuCreateDynListWithVals(DF_FLOAT, 3, flts));
  dfuAddDynGroupExistingList(a, "strings", dfuCreateDynListWithVals(DF_STRING,3, strs));
  subs[0] = dfuCreateDynListWithVals(DF_FLOAT, 2, s0);
  subs[1] = dfuCreateDynListWithVals(DF_FLOAT, 3, s1);
  dfuAddDynGroupExistingList(a, "ragged",  dfuCreateDynListWithVals(DF_LIST, 2, subs));

  /* write gzip via the standard buffer path (identical to dg_write) */
  dgInitBuffer();
  dgRecordDynGroup(a);
  if (dgWriteBufferCompressed((char *) tmp) != DF_OK) {
    fprintf(stderr, "dgz_roundtrip: gzip write failed\n");
    dgCloseBuffer(); return 1;
  }
  dgCloseBuffer();

  /* read back via the in-memory gunzip reader under test */
  b = dfuCreateDynGroup(8);
  if (dguGzipFileToStruct((char *) tmp, b) != DF_OK) {
    fprintf(stderr, "dgz_roundtrip: in-memory gzip read failed\n");
    remove(tmp); return 1;
  }

  if (DYN_GROUP_NLISTS(a) != DYN_GROUP_NLISTS(b)) {
    fprintf(stderr, "dgz_roundtrip: nlists %d vs %d\n",
	    DYN_GROUP_NLISTS(a), DYN_GROUP_NLISTS(b)); rc = 1;
  }
  for (i = 0; i < DYN_GROUP_NLISTS(a) && i < DYN_GROUP_NLISTS(b); i++)
    if (cmp_list(DYN_GROUP_LIST(a,i), DYN_GROUP_LIST(b,i))) {
      fprintf(stderr, "dgz_roundtrip: list %d mismatch\n", i); rc = 1;
    }

  {
    int nlists = DYN_GROUP_NLISTS(b);    /* capture before freeing */
    remove(tmp);
    dfuFreeDynGroup(a);
    dfuFreeDynGroup(b);
    if (rc) { fprintf(stderr, "dgz_roundtrip: FAILED\n"); return 1; }
    printf("dgz_roundtrip: OK (%d lists incl. ragged nested)\n", nlists);
  }
  return 0;
}
