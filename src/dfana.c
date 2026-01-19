/*************************************************************************
 *
 *  NAME
 *    dfana.c
 *
 *  DESCRIPTION
 *    Functions for manipulating dynlists
 *
 *  AUTHOR
 *    DLS
 *
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#if defined(SGI)
#include <ieeefp.h>
#endif

#include <df.h>
#include "dfana.h"

#include <utilc.h>

#include "uthash.h"


#ifdef WIN32
#pragma warning (disable:4244)
#pragma warning (disable:4305)
#pragma warning (disable:4761)
#define strcasecmp stricmp
#pragma function (floor)
#endif

/*
 *   DLCheckMatherr:       If set to 1, domain and sign errors are
 *                         are checked for basic math functions
 *                         (which, of course, takes time)
 */
static int DLCheckMatherr = 0;

static struct TableEntry DataTypeTable[] = {
  { "char",   DF_CHAR },
  { "short",  DF_SHORT },
  { "long",   DF_LONG },
  { "int",    DF_LONG },
  { "float",  DF_FLOAT },
  { "string", DF_STRING },
  { "list",   DF_LIST }
};

/*
 * How data are formatted when they get printed 
 */
char DLFormatTable[][128] = { "%d", "%d", "%f", "%s", "List(%d,%d)", "%d" };

#if defined(XXXWIN32)
double round(double x)
{
  //middle value point test
  if (ceil(x+0.5) == floor(x+0.5)) {
    int a = (int)ceil(x);
    if (a%2 == 0) return ceil(x);
    else return floor(x);
  }
  else return floor(x+0.5);
}
#endif

#if defined(SGI) || defined(__QNX__) || defined(XXXWIN32)
double lgamma(double xx)
{
  double x,y,tmp,ser;
  static double cof[6]={76.18009172947146,-86.50532032941677,
			  24.01409824083091,-1.231739572450155,
			  0.1208650973866179e-2,-0.5395239384953e-5};
  int j;
  
  y=x=xx;
  tmp=x+5.5;
  tmp -= (x+0.5)*log(tmp);
  ser=1.000000000190015;
  for (j=0;j<=5;j++) ser += cof[j]/++y;
  return -tmp+log(2.5066282746310005*ser/x);
}
#endif

#if defined(SGI)
#define isnan(x) isnanf(x)
#endif

#if defined(__QNX__) || defined(XXXWIN32)
#define isnan(x) (0)
#endif


/*
 * Structures used for sorting and keeping track of the permuted
 * indices 
 */
typedef struct _longi {
  int  i;
  int val;
} LONG_I;

typedef struct _shorti {
  int  i;
  short val;
} SHORT_I;

typedef struct _floati {
  int  i;
  float val;
} FLOAT_I;

typedef struct _chari {
  int  i;
  char val;
} CHAR_I;

typedef struct _stringi {
  int  i;
  char *val;
} STRING_I;


/************************************************************************/
/*                         Dyn* Functions                               */
/************************************************************************/

int dynGetDatatypeID(char *typename, int *typeid)
{
  int i, ntypes = sizeof(DataTypeTable)/sizeof(struct TableEntry);

  for (i = 0; i < ntypes; i++) {
    if (!strcasecmp(typename, DataTypeTable[i].name)) {
      *typeid = DataTypeTable[i].id;
      return(1);
    }
  }
  return(0);
}

char *dynGetDatatypeName(int typeid)
{
  int ntypes = sizeof(DataTypeTable)/sizeof(struct TableEntry);
  int i;
  
  for (i = 0; i < ntypes; i++) {
    if (DataTypeTable[i].id == typeid) {
      return(DataTypeTable[i].name);
    }
  }
  return(0);
}

/************************************************************************/
/*                         DynGroup Functions                           */
/************************************************************************/


int dynGroupMaxRows(DYN_GROUP *dg)
{
  int i, maxlistsize = 0;

  for (i = 0; i <  DYN_GROUP_NLISTS(dg); i++) {
    if (DYN_LIST_N(DYN_GROUP_LIST(dg,i)) > maxlistsize)
      maxlistsize = DYN_LIST_N(DYN_GROUP_LIST(dg,i));
  }

  return(maxlistsize);
}

int dynGroupMaxCols(DYN_GROUP *dg)
{
  return(DYN_GROUP_NLISTS(dg));
}


DYN_LIST *dynGroupFindList(DYN_GROUP *dg, char *name)
{
  int i;

  for (i = 0; i < DYN_GROUP_N(dg); i++) {
    if (!strcmp(DYN_LIST_NAME(DYN_GROUP_LIST(dg,i)), name)) {
      return(DYN_GROUP_LIST(dg,i));
    }
  }

  return(NULL);

}

int dynGroupFindListID(DYN_GROUP *dg, char *name)
{
  int i;

  for (i = 0; i < DYN_GROUP_N(dg); i++) {
    if (!strcmp(DYN_LIST_NAME(DYN_GROUP_LIST(dg,i)), name)) {
      return(i);
    }
  }
  return(-1);
}

int dynGroupRemoveList(DYN_GROUP *dg, char *name)
{
  DYN_LIST *dl;
  int i, id;
  
  id = dynGroupFindListID(dg, name);
  if (id < 0) return 0;
  
  dl = DYN_GROUP_LIST(dg,id);
  dfuFreeDynList(dl);
  
  for (i = id+1; i < DYN_GROUP_N(dg); i++) {
    DYN_GROUP_LIST(dg,i-1) = DYN_GROUP_LIST(dg,i);
  }
  DYN_GROUP_N(dg)--;
  return(1);
}

int dynGroupSetList(DYN_GROUP *dg, char *name, DYN_LIST *dl)
{
  DYN_LIST *old;
  int id;
  
  id = dynGroupFindListID(dg, name);
  if (id < 0) return 0;
  
  old = DYN_GROUP_LIST(dg,id);
  dfuFreeDynList(dl);
  DYN_GROUP_LIST(dg,id) = dfuCopyDynList(dl);
  
  return(1);
}

void dynGroupDumpListNames(DYN_GROUP *dg, FILE *stream)
{
  DYN_LIST *cols = dynListOnesInt(dynGroupMaxCols(dg));
  dynGroupDumpSelectedListNames(dg, cols, stream);
  dfuFreeDynList(cols);
}

int dynGroupDumpSelectedListNames(DYN_GROUP *dg, DYN_LIST *cols, FILE *stream)
{
  int i, n = 0;

  if (DYN_LIST_N(cols) < DYN_GROUP_NLISTS(dg)) {
    fprintf(stderr,"dynGroupDumpSelectedListNames(): invalid cols\n");
    return(0);
  }
  
  fprintf(stream, "%%%%%%\n%%%%%% Database:\t%s\n%%%%%%\n", 
	  DYN_GROUP_NAME(dg));
  
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    if (!dynListTestVal(cols,i)) continue;
    fprintf(stream, "%%%%%% Col %2d:\t", n++);
    fprintf(stream, "%s\n", DYN_LIST_NAME(DYN_GROUP_LIST(dg,i)));
  }
  return(1);
}



void dynGroupDump(DYN_GROUP *dg, char separator, FILE *stream)
{
  DYN_LIST *cols = dynListOnesInt(dynGroupMaxCols(dg));
  DYN_LIST *rows = dynListOnesInt(dynGroupMaxRows(dg));

  dynGroupDumpSelected(dg, rows, cols, separator, stream);

  dfuFreeDynList(rows);
  dfuFreeDynList(cols);
}


int dynGroupDumpSelected(DYN_GROUP *dg, DYN_LIST *rows, DYN_LIST *cols,
			  char separator, FILE *stream)
{
  int i, j, n = 0;
  int maxlistsize = dynGroupMaxRows(dg);

  if (DYN_LIST_N(rows) < maxlistsize || 
      DYN_LIST_N(cols) < DYN_GROUP_NLISTS(dg)) {
    fprintf(stderr,"dynGroupDumpSelected(): invalid rows/cols\n");
    return(0);
  }

  for (i = 0; i < maxlistsize; i++) {
    if (!dynListTestVal(rows,i)) continue;
    for (j = 0, n = 0; j < DYN_GROUP_NLISTS(dg); j++) {
      if (!dynListTestVal(cols,j)) continue;
      if (n++) fprintf(stream, "%c", separator);
      if (!dynListPrintVal(DYN_GROUP_LIST(dg,j), i, stream)) {
	if (DYN_LIST_DATATYPE(DYN_GROUP_LIST(dg,j)) == DF_FLOAT ||
	    DYN_LIST_DATATYPE(DYN_GROUP_LIST(dg,j)) == DF_LIST)
	  fprintf(stream, "   *   ");
	else fprintf(stream, ".");
      }
    }
    fprintf(stream, "\n");
  }
  return(1);
}

int dynGroupAppend(DYN_GROUP *dg1, DYN_GROUP *dg2)
{
  int i, match_id;

  /* Append all lists in dg2 that match dg1 */
  for (i = 0; i < DYN_GROUP_NLISTS(dg2); i++) {
      match_id = 
	dynGroupFindListID(dg1, DYN_GROUP_NAME(DYN_GROUP_LIST(dg2,i)));
      if (match_id < 0) return 0;
      if (DYN_LIST_DATATYPE(DYN_GROUP_LIST(dg1,match_id)) !=
	  DYN_LIST_DATATYPE(DYN_GROUP_LIST(dg2,i))) return 0;
  }

  /* Now call dynListConcat for each list */
  for (i = 0; i <  DYN_GROUP_NLISTS(dg2); i++) {
      match_id = 
	dynGroupFindListID(dg1, DYN_GROUP_NAME(DYN_GROUP_LIST(dg2,i)));
      dynListConcat(DYN_GROUP_LIST(dg1,match_id), DYN_GROUP_LIST(dg2,i));
  }
  return(1);
}

/************************************************************************/
/*                         DynList Functions                            */
/************************************************************************/


int dynListSetMatherrCheck(int new)
{
  int old = DLCheckMatherr;
  DLCheckMatherr = new;
  return(old);
}


int dynListIncrementCounter(DYN_LIST *dl)
{
  if (!dl) return(-1);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      if (DYN_LIST_N(dl)) dfuAddDynListLong(dl, vals[DYN_LIST_N(dl)-1]+1);
      else dfuAddDynListLong(dl, 0);
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      if (DYN_LIST_N(dl)) dfuAddDynListShort(dl, vals[DYN_LIST_N(dl)-1]+1);
      else dfuAddDynListShort(dl, 0);
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      if (DYN_LIST_N(dl)) dfuAddDynListFloat(dl, vals[DYN_LIST_N(dl)-1]+1.0);
      else dfuAddDynListFloat(dl, 0);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      if (DYN_LIST_N(dl)) dfuAddDynListChar(dl, vals[DYN_LIST_N(dl)-1]+1);
      else dfuAddDynListChar(dl, 0);
    }
    break;
  default:
    return(0);
    break;
  }
  return(1);
}

int dynListAddNullElement(DYN_LIST *dl)
{
  DYN_LIST *list;
  if (!dl) return(0);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    dfuAddDynListLong(dl, 0);
    break;
  case DF_SHORT:
    dfuAddDynListShort(dl, 0);
    break;
  case DF_FLOAT:
    dfuAddDynListFloat(dl, 0.0);
    break;
  case DF_CHAR:
    dfuAddDynListChar(dl, 0);
    break;
  case DF_STRING:
    dfuAddDynListString(dl, "");
    break;
  case DF_LIST:
    list = dfuCreateDynList(DF_LONG, 1);
    dfuMoveDynListList(dl, list);
    break;
  default:
    return(0);
    break;
  }
  return(1);
}


DYN_LIST *dynListPackList(DYN_LIST *dl)
{
  DYN_LIST *newlist, *d;
  int i;

  newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      d = dfuCreateDynList(DF_LONG, 1);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuResetDynList(d);
	dfuAddDynListLong(d, vals[i]);
	dfuAddDynListList(newlist, d);
      }
      dfuFreeDynList(d);
    }
  break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      d = dfuCreateDynList(DF_SHORT, 1);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuResetDynList(d);
	dfuAddDynListShort(d, vals[i]);
	dfuAddDynListList(newlist, d);
      }
      dfuFreeDynList(d);
    }
  break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      d = dfuCreateDynList(DF_FLOAT, 1);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuResetDynList(d);
	dfuAddDynListFloat(d, vals[i]);
	dfuAddDynListList(newlist, d);
      }
      dfuFreeDynList(d);
    }
  break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      d = dfuCreateDynList(DF_CHAR, 1);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuResetDynList(d);
	dfuAddDynListChar(d, vals[i]);
	dfuAddDynListList(newlist, d);
      }
      dfuFreeDynList(d);
    }
  break;
  case DF_STRING:
    {
      char **vals = (char **) DYN_LIST_VALS(dl);
      d = dfuCreateDynList(DF_STRING, 1);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuResetDynList(d);
	dfuAddDynListString(d, vals[i]);
	dfuAddDynListList(newlist, d);
      }
      dfuFreeDynList(d);
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      d = dfuCreateDynList(DF_LIST, 1);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuResetDynList(d);
	dfuAddDynListList(d, vals[i]);
	dfuAddDynListList(newlist, d);
      }
      dfuFreeDynList(d);
    }
  break;
  default:
    return(0);
    break;
  }
  return(newlist);
}

DYN_LIST *dynListDeepPackList(DYN_LIST *dl)
{
  DYN_LIST *newlist, *d;
  int i;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
  case DF_SHORT:
  case DF_FLOAT:
  case DF_CHAR:
  case DF_STRING:
    return dynListPackList(dl);
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	d = dynListDeepPackList(vals[i]);
	if (d) dfuMoveDynListList(newlist, d);
	else { 
	  dfuFreeDynList(newlist);
	  return NULL;
	}
      }
    }
    break;
  default:
    return(0);
    break;
  }
  return(newlist);
}


DYN_LIST *dynListUnpackList(DYN_LIST *dl)
{
  int i, j, n = 0;
  int default_datatype = DF_FLOAT;
  DYN_LIST **sublists, **subsublists, *newlist, *newsub;
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return NULL;
  if (DYN_LIST_N(dl) == 0) return NULL;

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  /* This is a list of lists (but no deeper) */
  if (dynListDepth(dl, 0) < 2) {
    /* check that all sublists are the same scalar type */
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      if (DYN_LIST_DATATYPE(sublists[i]) != 
	  DYN_LIST_DATATYPE(sublists[i])) {
	return NULL;
      }
    }
    newlist = dfuCreateDynList(DYN_LIST_DATATYPE(sublists[0]), n);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      for (j = 0; j < DYN_LIST_N(sublists[i]); j++) {
	dynListCopyElement(sublists[i], j, newlist);
      }
    }
    return(newlist);
  }

  /* Otherwise, things are a little more complicated... */
  /* First, check that all elements are the same type   */
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (DYN_LIST_DATATYPE(sublists[i]) != DF_LIST) {
      return NULL;
    }
    subsublists = (DYN_LIST **) DYN_LIST_VALS(sublists[i]);
    for (j = 0; j < DYN_LIST_N(sublists[i]); j++) {
      default_datatype = DYN_LIST_DATATYPE(subsublists[0]);
      if (DYN_LIST_DATATYPE(subsublists[j]) != 
	  DYN_LIST_DATATYPE(subsublists[0])) {
	return NULL;
      }
    }
  }

  newlist = dfuCreateDynList(DF_LIST, n);
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (!DYN_LIST_N(sublists[i])) {
      newsub = dfuCreateDynList(default_datatype, 2);
      dfuMoveDynListList(newlist, newsub);
    }
    else {
      subsublists = (DYN_LIST **) DYN_LIST_VALS(sublists[i]);
      newsub = dfuCreateDynList(DYN_LIST_DATATYPE(subsublists[0]), 10);
      for (j = 0; j < DYN_LIST_N(sublists[i]); j++) {
	dynListConcat(newsub, subsublists[j]);
      }
      dfuMoveDynListList(newlist, newsub);
    }
  }
  return(newlist);
}


DYN_LIST *dynListDeepUnpackList(DYN_LIST *dl)
{
  int i, j, n = 0;
  int default_datatype = DF_FLOAT;
  DYN_LIST **sublists, **subsublists, *newlist, *newsub;

  if (!dl) return NULL;

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return NULL;
  if (DYN_LIST_N(dl) == 0) return NULL;
  if (dynListDepth(dl, 0) < 2) return NULL;

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  /* check that all elements are the same type */
  /*
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (DYN_LIST_DATATYPE(sublists[i]) != DF_LIST) {
      return NULL;
    }
    subsublists = (DYN_LIST **) DYN_LIST_VALS(sublists[i]);
    for (j = 0; j < DYN_LIST_N(sublists[i]); j++) {
      default_datatype = DYN_LIST_DATATYPE(subsublists[0]);
      if (DYN_LIST_DATATYPE(subsublists[j]) != 
	  DYN_LIST_DATATYPE(subsublists[0])) {
	  return NULL;
	}
      }
  }
  */
  
  newlist = dfuCreateDynList(DF_LIST, n);
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (DYN_LIST_DATATYPE(sublists[i]) != DF_LIST) {
        // Not a list - can't unpack mixed depth, fail safely
        dfuFreeDynList(newlist);
        return NULL;
    }    
    if (!DYN_LIST_N(sublists[i])) {
      newsub = dfuCreateDynList(default_datatype, 2);
      dfuMoveDynListList(newlist, newsub);
    }
    else {
      subsublists = (DYN_LIST **) DYN_LIST_VALS(sublists[i]);
      if (DYN_LIST_DATATYPE(subsublists[0]) == DF_LIST) {
	newsub = dynListDeepUnpackList(sublists[i]);
	if (!newsub) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, newsub);
      }
      else {
	/* Make sure the datatype are consistent at the lowest level */
	for (j = 0; j < DYN_LIST_N(sublists[i]); j++) {
	  if (DYN_LIST_DATATYPE(subsublists[j]) !=
	      DYN_LIST_DATATYPE(subsublists[0])) return NULL;
	}
	newsub = dfuCreateDynList(DYN_LIST_DATATYPE(subsublists[0]), 10);
	for (j = 0; j < DYN_LIST_N(sublists[i]); j++) {
	  dynListConcat(newsub, subsublists[j]);
	}
	dfuMoveDynListList(newlist, newsub);
      }
    }
  }
  return(newlist);
}

int dynListTotalElements(DYN_LIST *dl)
{
  int i, sum = 0;
  DYN_LIST **sublists;

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return DYN_LIST_N(dl);
  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    sum += dynListTotalElements(sublists[i]);
  }
  return sum;
}

/* Make dl1 the same shape as dl2 */
DYN_LIST *dynListRestructureList(DYN_LIST *dl1, DYN_LIST *dl2, 
			     int check_length, int *curstart,
			     int *nused)
{
  DYN_LIST *retlist, *curlist, **sublists;
  int i, n = 0, sum;

  /* First make sure dl1 is a list of scalars */
  /* Removed this restriction 6/08 , DLS */
  /*   restructuring with lists it will make the resulting list "deeper" */
  /*  if (DYN_LIST_DATATYPE(dl1) == DF_LIST) return NULL; */
  
  /* Now make sure we have the correct number of elements */
  /* (Check length will only be true for the first call)  */
  if (check_length &&
      DYN_LIST_N(dl1) != dynListTotalElements(dl2)) return NULL;

  /* Easy case: scalar to scalar, just copy and set n to nelements */
  if (DYN_LIST_DATATYPE(dl2) != DF_LIST) {
    if (nused) *nused = DYN_LIST_N(dl2);
    curlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl1), DYN_LIST_N(dl2));
    for (i = 0; i < DYN_LIST_N(dl2); i++) {
      dynListCopyElement(dl1, i+(*curstart), curlist);
    }
    return curlist;
  }

  /* Otherwise, get recursive */
  sublists = (DYN_LIST **) DYN_LIST_VALS(dl2);
  retlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl2));
  for (i = 0, sum = 0; i < DYN_LIST_N(dl2); i++) {
    curlist = dynListRestructureList(dl1, sublists[i], 0, curstart, &n);
    if (!curlist) {
      dfuFreeDynList(retlist);
      return NULL;
    }
    dfuMoveDynListList(retlist, curlist);
    *curstart += n;
  }
  if (nused) *nused = 0;
  return retlist;
}

DYN_LIST *dynListReshapeList(DYN_LIST *dl, int nrows, int ncols)
{
  int i;
  DYN_LIST *newlist, **newrows, *row;
  if (DYN_LIST_N(dl) == 0) return NULL;
  
  if ((nrows * ncols) != DYN_LIST_N(dl)) return NULL;
  newlist = dfuCreateDynList(DF_LIST, nrows);
  row = dfuCreateDynList(DYN_LIST_DATATYPE(dl), ncols);
  for (i = 0; i < nrows; i++) 
    dfuAddDynListList(newlist, row);
  dfuFreeDynList(row);
  
  newrows = (DYN_LIST **) DYN_LIST_VALS(newlist);
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    dynListCopyElement(dl, i, newrows[i/ncols]);
  }
  return(newlist);
}

static int dynListCheckSublistType(DYN_LIST *dl, int type)
{
  int i;
  DYN_LIST **vals;
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    if (DYN_LIST_DATATYPE(dl) == type) return 1;
    else return 0;
  }

  /* It's a list of lists, so call the checker recursively */
  vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (!dynListCheckSublistType(vals[i], type)) return 0;
  }
  return 1;
}

/*
 * Splice elements of list 1 into list 2
 */

DYN_LIST *dynListSpliceLists(DYN_LIST *dl1, DYN_LIST *dl2, int pos)
{
  int i;
  DYN_LIST **vals;

  if (!dl1 || !dl2) return(NULL);
  if (DYN_LIST_DATATYPE(dl2) != DF_LIST) return NULL;

  /* In this condition, prepend or append the same element to all lists */
  if (DYN_LIST_DATATYPE(dl1) != DF_LIST && 
      DYN_LIST_N(dl1) == 1) {
    /*
     * Now we have to check all sublists to make sure they are either of
     * type DF_LIST or DYN_LIST_DATATYPE(dl1) or else lists can't be spliced 
     */
    if (!dynListCheckSublistType(dl2, DYN_LIST_DATATYPE(dl1))) return NULL;
    
    vals = (DYN_LIST **) DYN_LIST_VALS(dl2);

    switch (DYN_LIST_DATATYPE(dl1)) {
    case DF_LONG: 
      {
	int *inval = (int *) DYN_LIST_VALS(dl1);
	for (i = 0; i < DYN_LIST_N(dl2); i++) {
	  if (DYN_LIST_DATATYPE(vals[i]) == DF_LIST) {
	    dynListSpliceLists(dl1, vals[i], pos);
	  }
	  else {
	    if (pos == 0) 
	      dfuPrependDynListLong(vals[i], inval[0]);
	    else 
	      dfuAddDynListLong(vals[i], inval[0]);
	  }
	}
	break;
      }
    case DF_SHORT:
      {
	short *inval = (short *) DYN_LIST_VALS(dl1);
	for (i = 0; i < DYN_LIST_N(dl2); i++) {
	  if (DYN_LIST_DATATYPE(vals[i]) == DF_LIST) {
	    dynListSpliceLists(dl1, vals[i], pos);
	  }
	  else {
	    if (pos == 0) 
	      dfuPrependDynListShort(vals[i], inval[0]);
	    else 
	      dfuAddDynListShort(vals[i], inval[0]);
	  }
	}
	break;
      }
    case DF_CHAR:
      {
	char *inval = (char *) DYN_LIST_VALS(dl1);
	for (i = 0; i < DYN_LIST_N(dl2); i++) {
	  if (DYN_LIST_DATATYPE(vals[i]) == DF_LIST) {
	    dynListSpliceLists(dl1, vals[i], pos);
	  }
	  else {
	    if (pos == 0) 
	      dfuPrependDynListChar(vals[i], inval[0]);
	    else 
	      dfuAddDynListChar(vals[i], inval[0]);
	  }
	}
	break;
      }
    case DF_FLOAT:
      {
	float *inval = (float *) DYN_LIST_VALS(dl1);
	for (i = 0; i < DYN_LIST_N(dl2); i++) {
	  if (DYN_LIST_DATATYPE(vals[i]) == DF_LIST) {
	    dynListSpliceLists(dl1, vals[i], pos);
	  }
	  else {
	    if (pos == 0) 
	      dfuPrependDynListFloat(vals[i], inval[0]);
	    else 
	      dfuAddDynListFloat(vals[i], inval[0]);
	  }
	}
	break;
      }
    case DF_STRING:
      {
	char **inval = (char **) DYN_LIST_VALS(dl1);
	for (i = 0; i < DYN_LIST_N(dl2); i++) {
	  if (DYN_LIST_DATATYPE(vals[i]) == DF_LIST) {
	    dynListSpliceLists(dl1, vals[i], pos);
	  }
	  else {
	    if (pos == 0) 
	      dfuPrependDynListString(vals[i], inval[0]);
	    else 
	      dfuAddDynListString(vals[i], inval[0]);
	  }
	}
	break;
      }
    }
    return(dl2);
  }
  return NULL;
}

DYN_LIST *dynListUnpackLists(DYN_LIST *dl, DYN_LIST *out)
{
  int i;
  DYN_LIST **sublists;
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return NULL;

  if (!out) {
    out = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
  }

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (DYN_LIST_DATATYPE(sublists[i]) == DF_LIST) {
      dynListUnpackLists(sublists[i], out);
    }
    else {
      dynListCopyElement(dl, i, out);
    }
  }
  return(out);
}

DYN_LIST *dynListCollapseList(DYN_LIST *dl)
{
  int i, j, n = 0;
  DYN_LIST **sublists, *newlist;
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return NULL;
  if (DYN_LIST_N(dl) == 0) return NULL;

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  /* check that all elements are the same type */
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    n += DYN_LIST_N(sublists[i]);
    if (DYN_LIST_DATATYPE(sublists[i]) != DYN_LIST_DATATYPE(sublists[0])) {
      return NULL;
    }
  }
  newlist = dfuCreateDynList(DYN_LIST_DATATYPE(sublists[0]), n);

  for (i = 0; i < DYN_LIST_N(dl); i++) {
    for (j = 0; j < DYN_LIST_N(sublists[i]); j++) {
      dynListCopyElement(sublists[i], j, newlist);
    }
  }
  return(newlist);
}

int dynListConcat(DYN_LIST *dl1, DYN_LIST *dl2)
{
  int i, nelts;
  if (!dl1 || !dl2) return(0);
  if (DYN_LIST_DATATYPE(dl1) != DYN_LIST_DATATYPE(dl2)) return 0;

  nelts = DYN_LIST_N(dl2);	/* in case dl1 == dl2, this val won't change */
  for (i = 0; i < nelts; i++) {
    dynListCopyElement(dl2, i, dl1);
  }
  
  return(1);
}

DYN_LIST *dynListCombine(DYN_LIST *dl1, DYN_LIST *dl2)
{
  int i;
  DYN_LIST *newlist;

  if (!dl1 || !dl2) return(0);
  if (DYN_LIST_DATATYPE(dl1) != DYN_LIST_DATATYPE(dl2)) return 0;

  newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl1), 
			     DYN_LIST_N(dl1)+DYN_LIST_N(dl2));
  
  for (i = 0; i < DYN_LIST_N(dl1); i++) {
    dynListCopyElement(dl1, i, newlist);
  }
  for (i = 0; i < DYN_LIST_N(dl2); i++) {
    dynListCopyElement(dl2, i, newlist);
  }
  return(newlist);
}

DYN_LIST *dynListIndexList(DYN_LIST *dl)
{
  int i;
  DYN_LIST *newlist = NULL;
  if (!dl) return NULL;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curlist;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListIndexList(vals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
  break;
  default:
    newlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
    if (!newlist) return(NULL);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListLong(newlist, i);
    }
    break;
  }
  return(newlist);
}


DYN_LIST *dynListFirstIndexList(DYN_LIST *dl)
{
  int i;
  DYN_LIST *newlist = NULL;
  if (!dl) return NULL;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curlist;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListFirstIndexList(vals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
  break;
  default:
    newlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
    if (!newlist) return(NULL);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListLong(newlist, i == 0);
    }
    break;
  }
  return(newlist);
}


DYN_LIST *dynListLastIndexList(DYN_LIST *dl)
{
  int i, last;
  DYN_LIST *newlist = NULL;
  if (!dl) return NULL;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curlist;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListLastIndexList(vals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
  break;
  default:
    newlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
    if (!newlist) return(NULL);
    last = DYN_LIST_N(dl)-1;
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListLong(newlist, i == last);
    }
    break;
  }
  return(newlist);
}

DYN_LIST *dynListInterleave(DYN_LIST *dl1, DYN_LIST *dl2)
{
  int i;
  DYN_LIST *newlist = NULL;

  if (!dl1 || !dl2) return(NULL);

  if (DYN_LIST_DATATYPE(dl1) == DYN_LIST_DATATYPE(dl2)) {
    if (DYN_LIST_N(dl1) == DYN_LIST_N(dl2)) {
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl1), 
				 DYN_LIST_N(dl1)+DYN_LIST_N(dl2));
      for (i = 0; i < DYN_LIST_N(dl1); i++) {
	dynListCopyElement(dl1, i, newlist);
	dynListCopyElement(dl2, i, newlist);
      }
    }
    else if (DYN_LIST_N(dl1) == 1) {
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl1), 
				 2*DYN_LIST_N(dl2));
      for (i = 0; i < DYN_LIST_N(dl2); i++) {
	dynListCopyElement(dl1, 0, newlist);
	dynListCopyElement(dl2, i, newlist);
      }
    }
    else if (DYN_LIST_N(dl2) == 1) {
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl1), 
				 2*DYN_LIST_N(dl1));
      for (i = 0; i < DYN_LIST_N(dl1); i++) {
	dynListCopyElement(dl1, i, newlist);
	dynListCopyElement(dl2, 0, newlist);
      }
    }
    else return NULL;
  }
  else if (DYN_LIST_DATATYPE(dl1) == DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl1);
    DYN_LIST *curlist;
    newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl1), DYN_LIST_N(dl1));
    for (i = 0; i < DYN_LIST_N(dl1); i++) {
      curlist = dynListInterleave(vals[i], dl2);
      if (!curlist) {
	dfuFreeDynList(newlist);
	return NULL;
      }
      dfuMoveDynListList(newlist, curlist);
    }
  }
  else if (DYN_LIST_DATATYPE(dl2) == DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl2);
    DYN_LIST *curlist;
    newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl2), DYN_LIST_N(dl2));
    for (i = 0; i < DYN_LIST_N(dl2); i++) {
      curlist = dynListInterleave(dl1, vals[i]);
      if (!curlist) {
	dfuFreeDynList(newlist);
	return NULL;
      }
      dfuMoveDynListList(newlist, curlist);
    }
  }
  return(newlist);
}

DYN_LIST *dynListFillSparse(DYN_LIST *vals, DYN_LIST *times, DYN_LIST *range)
{
  int i, curindex = 0, index2;
  DYN_LIST *newlist = NULL;
  
  if (DYN_LIST_N(range) != 3)  return NULL;
  if (!vals || !times) return(NULL);
  if (DYN_LIST_N(vals) != DYN_LIST_N(times)) return NULL;

  switch (DYN_LIST_DATATYPE(range)) {
  case DF_LONG:
    {
      int *rvals = (int *) DYN_LIST_VALS(range);
      int nsteps;
      int time, start = rvals[0], stop = rvals[1], res = rvals[2];
      nsteps = (stop - start) / res + 1;
      
      if (nsteps <= 0) return NULL;
      index2 = DYN_LIST_N(times)-1;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(vals), nsteps);
      switch (DYN_LIST_DATATYPE(times)) {
      case DF_LONG:
	{
	  int *ts = (int *) DYN_LIST_VALS(times);
	  time = start;
	  while (time > ts[curindex] && curindex < DYN_LIST_N(times)) curindex++;
	  for (i = 0; i < nsteps; i++, time += res) {
	    while (curindex < index2 && time >= ts[curindex+1]) curindex++;
	    dynListCopyElement(vals, curindex, newlist);
	  }
	}
	break;
      case DF_FLOAT:
	{
	  float *ts = (float *) DYN_LIST_VALS(times);
	  time = start;
	  while (time > ts[curindex] && curindex < DYN_LIST_N(times)) curindex++;
	  for (i = 0; i < nsteps; i++, time += res) {
	    while (curindex < index2 && time >= ts[curindex+1]) curindex++;
	    dynListCopyElement(vals, curindex, newlist);
	  }
	}
	break;
      default:
	return NULL;
	break;
      }
    }
    break;
  case DF_FLOAT:
    {
      float *rvals = (float *) DYN_LIST_VALS(range);
      int nsteps;
      double time, start = rvals[0], stop = rvals[1], res = rvals[2];
      nsteps = (stop - start) / res + 1;
      
      if (nsteps <= 0) return NULL;
      index2 = DYN_LIST_N(times)-1;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(vals), nsteps);
      switch (DYN_LIST_DATATYPE(times)) {
      case DF_LONG:
	{
	  int *ts = (int *) DYN_LIST_VALS(times);
	  time = start;
	  while (time > ts[curindex] && curindex < DYN_LIST_N(times)) curindex++;
	  for (i = 0; i < nsteps; i++, time += res) {
	    while (curindex < index2 && time >= ts[curindex+1]) curindex++;
	    dynListCopyElement(vals, curindex, newlist);
	  }
	}
	break;
      case DF_FLOAT:
	{
	  float *ts = (float *) DYN_LIST_VALS(times);
	  time = start;
	  while (time > ts[curindex] && curindex < DYN_LIST_N(times)) curindex++;
	  for (i = 0; i < nsteps; i++, time += res) {
	    while (curindex < index2 && time >= ts[curindex+1]) curindex++;
	    dynListCopyElement(vals, curindex, newlist);
	  }
	}
	break;
      default:
	return NULL;
	break;
      }
    }
  }
  return(newlist);
}

DYN_LIST *dynListGenerate(DYN_LIST *size, int op)
{
  int i;
  DYN_LIST *list = NULL, *newlist;
  
  if (!size) return NULL;

  if (DYN_LIST_DATATYPE(size) == DF_LONG) {
    int *vals = (int *) DYN_LIST_VALS(size);

    /* Check for negative numbers */
    for (i = 0; i < DYN_LIST_N(size); i++) 
      if (vals[i] < 0) return NULL;

    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(size));
    switch (op) {
    case DL_GEN_ZEROS:
      for (i = 0; i < DYN_LIST_N(size); i++) {
	if (vals[i]) newlist = dynListZerosInt(vals[i]);
	else newlist = dfuCreateDynList(DF_LONG, 1);
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DL_GEN_ONES:
      for (i = 0; i < DYN_LIST_N(size); i++) {
	if (vals[i]) newlist = dynListOnesInt(vals[i]);
	else newlist = dfuCreateDynList(DF_LONG, 1);
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DL_GEN_ZRAND:
      for (i = 0; i < DYN_LIST_N(size); i++) {
	if (vals[i]) newlist = dynListNormalRandsInt(vals[i]);
	else newlist = dfuCreateDynList(DF_FLOAT, 1);
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DL_GEN_URAND:
      for (i = 0; i < DYN_LIST_N(size); i++) {
	if (vals[i]) newlist = dynListUniformRandsInt(vals[i]);
	else newlist = dfuCreateDynList(DF_FLOAT, 1);
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DL_GEN_RANDFILL:
      for (i = 0; i < DYN_LIST_N(size); i++) {
	if (vals[i]) newlist = dynListRandFillInt(vals[i]);
	else newlist = dfuCreateDynList(DF_LONG, 1);
	dfuMoveDynListList(list, newlist);
      }
      break;
    }
  }

  else if (DYN_LIST_DATATYPE(size) == DF_FLOAT) {
    float *vals = (float *) DYN_LIST_VALS(size);
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(size));

    /* Check for negative numbers */
    for (i = 0; i < DYN_LIST_N(size); i++) 
      if (vals[i] < 0) return NULL;

    switch (op) {
    case DL_GEN_ZEROS:
      for (i = 0; i < DYN_LIST_N(size); i++) {
	if (vals[i]) newlist = dynListZerosFloat(vals[i]);
	else newlist = dfuCreateDynList(DF_FLOAT, 1);
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DL_GEN_ONES:
      for (i = 0; i < DYN_LIST_N(size); i++) {
	if (vals[i]) newlist = dynListOnesFloat(vals[i]);
	else newlist = dfuCreateDynList(DF_FLOAT, 1);
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DL_GEN_ZRAND:
      for (i = 0; i < DYN_LIST_N(size); i++) {
	if (vals[i]) newlist = dynListNormalRandsInt(vals[i]);
	else newlist = dfuCreateDynList(DF_FLOAT, 1);
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DL_GEN_URAND:
      for (i = 0; i < DYN_LIST_N(size); i++) {
	if (vals[i]) newlist = dynListUniformRandsInt(vals[i]);
	else newlist = dfuCreateDynList(DF_FLOAT, 1);
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DL_GEN_RANDFILL:
      for (i = 0; i < DYN_LIST_N(size); i++) {
	if (vals[i]) newlist = dynListRandFillInt(vals[i]);
	else newlist = dfuCreateDynList(DF_FLOAT, 1);
	dfuMoveDynListList(list, newlist);
      }
      break;
    }
  }
  else if (DYN_LIST_DATATYPE(size) == DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(size);
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(size));
    for (i = 0; i < DYN_LIST_N(size); i++) {
      newlist = dynListGenerate(vals[i], op);
      dfuMoveDynListList(list, newlist);
    }
  }

  return(list);
}

DYN_LIST *dynListOnes(DYN_LIST *size)
{
  return(dynListGenerate(size, DL_GEN_ONES));
}

DYN_LIST *dynListZeros(DYN_LIST *size)
{
  return(dynListGenerate(size, DL_GEN_ZEROS));
}

DYN_LIST *dynListUniformRands(DYN_LIST *size)
{
  return(dynListGenerate(size, DL_GEN_URAND));
}

DYN_LIST *dynListNormalRands(DYN_LIST *size)
{
  return(dynListGenerate(size, DL_GEN_ZRAND));
}

DYN_LIST *dynListRandFill(DYN_LIST *size)
{
  return(dynListGenerate(size, DL_GEN_RANDFILL));
}

DYN_LIST *dynListUniformIRands(DYN_LIST *size, DYN_LIST *maxlist)
{
  int i;
  int max;
  DYN_LIST *list = NULL, *newlist;
  
  if (!size || !maxlist) return NULL;
  
  /* max list must be longs and either length 1 or same as sizelist */
  if (DYN_LIST_DATATYPE(size) == DF_LONG || 
      DYN_LIST_DATATYPE(size) == DF_FLOAT) {
    if ((DYN_LIST_DATATYPE(maxlist) != DF_LONG) ||
	((DYN_LIST_N(maxlist) != 1) &&
	 DYN_LIST_N(maxlist) != DYN_LIST_N(size)))
      return NULL;
  }
  else if (DYN_LIST_DATATYPE(size) == DF_LIST) {
    if ((DYN_LIST_DATATYPE(maxlist) != DF_LIST) ||
	((DYN_LIST_N(maxlist) != 1) &&
	 DYN_LIST_N(maxlist) != DYN_LIST_N(size)))
      return NULL;
  }

  else return NULL;
  
    if (DYN_LIST_DATATYPE(size) == DF_LONG) {
    int *maxvals;
    int *vals = (int *) DYN_LIST_VALS(size);
    maxvals = (int *) DYN_LIST_VALS(maxlist);
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(size));
    if (DYN_LIST_N(maxlist) == 1) {
      max = maxvals[0];
      for (i = 0; i < DYN_LIST_N(size); i++) {
	newlist = dynListUniformIRandsInt(vals[i], max);
	dfuMoveDynListList(list, newlist);
      }
    }
    else {
      for (i = 0; i < DYN_LIST_N(size); i++) {
	newlist = dynListUniformIRandsInt(vals[i], maxvals[i]);
	dfuMoveDynListList(list, newlist);
      }
    }
  }
  else if (DYN_LIST_DATATYPE(size) == DF_FLOAT) {
    int *maxvals;
    float *vals = (float *) DYN_LIST_VALS(size);
    maxvals = (int *) DYN_LIST_VALS(maxlist);
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(size));
    if (DYN_LIST_N(maxlist) == 1) {
      max = maxvals[0];
      for (i = 0; i < DYN_LIST_N(size); i++) {
	newlist = dynListUniformIRandsInt(vals[i], max);
	dfuMoveDynListList(list, newlist);
      }
    }
    else {
      for (i = 0; i < DYN_LIST_N(size); i++) {
	newlist = dynListUniformIRandsInt(vals[i], maxvals[i]);
	dfuMoveDynListList(list, newlist);
      }
    }
  }
  else if (DYN_LIST_DATATYPE(size) == DF_LIST) {
    DYN_LIST **maxvals;
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(size);
    maxvals = (DYN_LIST **) DYN_LIST_VALS(maxlist);
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(size));
    if (DYN_LIST_N(maxlist) == 1) {
      for (i = 0; i < DYN_LIST_N(size); i++) {
	newlist = dynListUniformIRands(vals[i], maxvals[0]);
	if (!newlist) {
	  dfuFreeDynList(list);
	  return NULL;
	}
	else {
	  dfuMoveDynListList(list, newlist);
	}
      }
    }
    else {
      for (i = 0; i < DYN_LIST_N(size); i++) {
	newlist = dynListUniformIRands(vals[i], maxvals[i]);
	if (!newlist) {
	  dfuFreeDynList(list);
	  return NULL;
	}
	else {
	  dfuMoveDynListList(list, newlist);
	}
      }
    }
  }
  return(list);
}


DYN_LIST *dynListZerosFloat(int size)
{
  DYN_LIST *list;
  float *newvals = (float *) calloc(size, sizeof(float));
  list = dfuCreateDynListWithVals(DF_FLOAT, size, newvals);
  return(list);
}

DYN_LIST *dynListOnesFloat(int size)
{
  int i;
  DYN_LIST *list;
  float *newvals = (float *) calloc(size, sizeof(float));
  for (i = 0; i < size; i++) newvals[i] = 1.0;
  list = dfuCreateDynListWithVals(DF_FLOAT, size, newvals);
  return(list);
}

DYN_LIST *dynListZerosInt(int size)
{
  DYN_LIST *list;
  int *newvals = (int *) calloc(size, sizeof(int));
  list = dfuCreateDynListWithVals(DF_LONG, size, newvals);
  return(list);
}

DYN_LIST *dynListOnesInt(int size)
{
  int i;
  DYN_LIST *list;
  int *newvals = (int *) calloc(size, sizeof(int));
  for (i = 0; i < size; i++) newvals[i] = 1;
  list = dfuCreateDynListWithVals(DF_LONG, size, newvals);
  return(list);
}

DYN_LIST *dynListUniformRandsInt(int size)
{
  int i;
  DYN_LIST *list;
  float *newvals = (float *) calloc(size, sizeof(float));
  for (i = 0; i < size; i++) newvals[i] = frand();
  list = dfuCreateDynListWithVals(DF_FLOAT, size, newvals);
  return(list);
}

DYN_LIST *dynListUniformIRandsInt(int size, int max)
{
  int i;
  DYN_LIST *list;
  int *newvals = (int *) calloc(size, sizeof(int));
  for (i = 0; i < size; i++) newvals[i] = ran(max);
  list = dfuCreateDynListWithVals(DF_LONG, size, newvals);
  return(list);
}

DYN_LIST *dynListNormalRandsInt(int size)
{
  int i;
  DYN_LIST *list;
  float *newvals = (float *) calloc(size, sizeof(float));
  for (i = 0; i < size; i++) newvals[i] = zrand();
  list = dfuCreateDynListWithVals(DF_FLOAT, size, newvals);
  return(list);
}

DYN_LIST *dynListShuffleList(DYN_LIST *dl)
{
  int i, *vals;
  DYN_LIST *order, *newlist;

  newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
  if (!DYN_LIST_N(dl)) return newlist;

  order = dynListRandFillInt(DYN_LIST_N(dl));
  vals = (int *) DYN_LIST_VALS(order);

  for (i = 0; i < DYN_LIST_N(dl); i++) {
    dynListCopyElement(dl, vals[i], newlist);
  }

  dfuFreeDynList(order);
  return newlist;
}

DYN_LIST *dynListRandFillInt(int size)
{
  int *rands;
  DYN_LIST *list;
  if (!(rands = (int *) calloc(size, sizeof(int)))) return NULL;
  rand_fill(size, rands);
  list = dfuCreateDynListWithVals(DF_LONG, size, rands);
  return(list);
}

DYN_LIST *dynListRandChoose(DYN_LIST *dl1, DYN_LIST *dl2)
{
  int i;
  DYN_LIST *newlist = NULL, *curlist;
  int *m_s, *n_s, m, n;
  if (DYN_LIST_DATATYPE(dl1) != DF_LONG ||
      DYN_LIST_DATATYPE(dl2) != DF_LONG) return NULL;
  if (DYN_LIST_N(dl1) == 1) {
    m_s = (int *) DYN_LIST_VALS(dl1);
    n_s = (int *) DYN_LIST_VALS(dl2);
    m = m_s[0];
    if (m < 0) return NULL;
    newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl2));
    for (i = 0; i < DYN_LIST_N(dl2); i++) {
      n = n_s[i];
      if (n > m || n < 0) goto error;
      if (n) curlist = dynListRandChooseInt(m, n);
      else curlist = dfuCreateDynList(DF_LONG, 1);
      if (!curlist) goto error;
      dfuMoveDynListList(newlist, curlist);
    }
  }
  else if (DYN_LIST_N(dl2) == 1) {
    m_s = (int *) DYN_LIST_VALS(dl1);
    n_s = (int *) DYN_LIST_VALS(dl2);
    n = n_s[0];
    if (n < 0) return NULL;
    newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl1));
    for (i = 0; i < DYN_LIST_N(dl1); i++) {
      m = m_s[i];
      if (n > m || m < 0) goto error;
      if (n) curlist = dynListRandChooseInt(m, n);
      else curlist = dfuCreateDynList(DF_LONG, 1);
      if (!curlist) goto error;
      dfuMoveDynListList(newlist, curlist);
    }
  }
  else if (DYN_LIST_N(dl1) == DYN_LIST_N(dl2)) {
    m_s = (int *) DYN_LIST_VALS(dl1);
    n_s = (int *) DYN_LIST_VALS(dl2);
    newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl1));
    for (i = 0; i < DYN_LIST_N(dl1); i++) {
      n = n_s[i];
      m = m_s[i];
      if (n > m || m < 0 || n < 0) goto error;
      if (n) curlist = dynListRandChooseInt(m, n);
      else curlist = dfuCreateDynList(DF_LONG, 1);

      if (!curlist) goto error;
      dfuMoveDynListList(newlist, curlist);
    }
  }
  return newlist;

 error:
  if (newlist) dfuFreeDynList(newlist);
  return NULL;
}

DYN_LIST *dynListRandChooseInt(int m, int n)
{
  int *rands;
  DYN_LIST *list;
  
  if (!(rands = (int *) calloc(n, sizeof(int))))
    return NULL;

  rand_choose(m, n, rands);
  list = dfuCreateDynListWithVals(DF_LONG, n, rands);
  return(list);
}

DYN_LIST *dynListBins(DYN_LIST *range, int nbins)
{
  int i;
  double bwidth, value, start, stop;
  DYN_LIST *list;
  
  if (!range || nbins <= 0) return NULL;
  switch (DYN_LIST_DATATYPE(range)) {
  case DF_LONG: 
    {
      int *vals = (int *) DYN_LIST_VALS(range);
      start = vals[0];
      stop = vals[1];
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(range);
      start = vals[0];
      stop = vals[1];
    }
    break;
  default:
    return NULL;
  }

  list = dfuCreateDynList(DF_FLOAT, nbins);
  
  bwidth = (stop-start)/nbins;
  value = start + bwidth/2.0;
  for (i = 0; i < nbins; i++) {
    dfuAddDynListFloat(list, value);
    value += bwidth;
  }
  
  return(list);
}

int dynListMatchLength3(DYN_LIST *l1, DYN_LIST *l2, DYN_LIST *l3, int *mode)
{
  int length = 1;
  int m = 0;

  /* Check that the lengths are equal or are 1 */
  if (DYN_LIST_N(l1) > 1) {
    length = DYN_LIST_N(l1);
    m = 4;
  }
  
  if (DYN_LIST_N(l2) > 1) {
    if (length != 1 && DYN_LIST_N(l2) != length) return 0;
    else {
      length = DYN_LIST_N(l2);
      m += 2;
    }
  }

  if (DYN_LIST_N(l3) > 1) {
    if (length != 1 && DYN_LIST_N(l3) != length) return 0;
    else {
      length = DYN_LIST_N(l3);
      m += 1;
    }
  }
  if (m == 0) m = 7;		/* These are functionally equivalent */
  *mode = m;

  return length;
}

/*
 * A terribly long function for generating vectorized series
 */

DYN_LIST *dynListSeries(DYN_LIST *starts, DYN_LIST *stops, DYN_LIST *steps, 
			int type)
{
  int length, mode, i;
  DYN_LIST *newlist = NULL, *curlist;

  if (DYN_LIST_DATATYPE(starts) == DF_LIST &&
      DYN_LIST_DATATYPE(stops) == DF_LIST) {
    DYN_LIST **vals1 = (DYN_LIST **) DYN_LIST_VALS(starts);
    DYN_LIST **vals2 = (DYN_LIST **) DYN_LIST_VALS(stops);
    if (DYN_LIST_N(starts) == DYN_LIST_N(stops)) {
      newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(starts));
      for (i = 0; i < DYN_LIST_N(starts); i++) {
	curlist = dynListSeries(vals1[i], vals2[i], steps, type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    else if (DYN_LIST_N(stops) == 1) {
      newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(starts));
      for (i = 0; i < DYN_LIST_N(starts); i++) {
	curlist = dynListSeries(vals1[i], vals2[0], steps, type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    else if (DYN_LIST_N(starts) == 1) {
      newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(starts));
      for (i = 0; i < DYN_LIST_N(stops); i++) {
	curlist = dynListSeries(vals1[0], vals2[i], steps, type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    return newlist;		/* NULL if bad list lengths */
  }

  /* The lists must either be ints or floats */
  if ((DYN_LIST_DATATYPE(starts) != DF_LONG && 
       DYN_LIST_DATATYPE(starts) != DF_FLOAT) ||
      (DYN_LIST_DATATYPE(starts) != DF_LONG && 
       DYN_LIST_DATATYPE(starts) != DF_FLOAT) ||
      (DYN_LIST_DATATYPE(starts) != DF_LONG && 
       DYN_LIST_DATATYPE(starts) != DF_FLOAT)) return NULL;
  
  if (!(length = dynListMatchLength3(starts, stops, steps, &mode))) 
    return NULL;

  /* There are lots of possibilities here (64 to be exact) so lets get to it */
  
  if (DYN_LIST_DATATYPE(starts) == DF_FLOAT &&
      DYN_LIST_DATATYPE(stops) == DF_FLOAT &&
      DYN_LIST_DATATYPE(steps) == DF_FLOAT) {
    float *start_vals, *stop_vals, *step_vals;
    
    start_vals = (float *) DYN_LIST_VALS(starts);
    stop_vals =  (float *) DYN_LIST_VALS(stops);
    step_vals =  (float *) DYN_LIST_VALS(steps);
    
    newlist = dfuCreateDynList(DF_LIST, length);
    
    switch (mode) {
    case 7:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 6:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 5:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 4:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 3:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
  }
  else if (DYN_LIST_DATATYPE(starts) == DF_LONG &&
      DYN_LIST_DATATYPE(stops) == DF_FLOAT &&
      DYN_LIST_DATATYPE(steps) == DF_FLOAT) {
    float *stop_vals, *step_vals;
    int *start_vals;
    
    start_vals = (int *) DYN_LIST_VALS(starts);
    stop_vals =  (float *) DYN_LIST_VALS(stops);
    step_vals =  (float *) DYN_LIST_VALS(steps);
    
    newlist = dfuCreateDynList(DF_LIST, length);
    
    switch (mode) {
    case 7:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 6:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 5:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 4:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 3:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
  }
  else if (DYN_LIST_DATATYPE(starts) == DF_FLOAT &&
      DYN_LIST_DATATYPE(stops) == DF_LONG &&
      DYN_LIST_DATATYPE(steps) == DF_FLOAT) {
    float *start_vals, *step_vals;
    int *stop_vals;
    
    start_vals = (float *) DYN_LIST_VALS(starts);
    stop_vals =  (int *) DYN_LIST_VALS(stops);
    step_vals =  (float *) DYN_LIST_VALS(steps);
    
    newlist = dfuCreateDynList(DF_LIST, length);
    
    switch (mode) {
    case 7:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 6:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 5:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 4:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 3:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
  }
  else if (DYN_LIST_DATATYPE(starts) == DF_FLOAT &&
      DYN_LIST_DATATYPE(stops) == DF_FLOAT &&
      DYN_LIST_DATATYPE(steps) == DF_LONG) {
    float *start_vals, *stop_vals;
    int *step_vals;
    
    start_vals = (float *) DYN_LIST_VALS(starts);
    stop_vals =  (float *) DYN_LIST_VALS(stops);
    step_vals =  (int *) DYN_LIST_VALS(steps);
    
    newlist = dfuCreateDynList(DF_LIST, length);
    
    switch (mode) {
    case 7:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 6:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 5:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 4:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 3:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
  }
  else if (DYN_LIST_DATATYPE(starts) == DF_LONG &&
      DYN_LIST_DATATYPE(stops) == DF_LONG &&
      DYN_LIST_DATATYPE(steps) == DF_FLOAT) {
    int *start_vals, *stop_vals;
    float *step_vals;
    
    start_vals = (int *) DYN_LIST_VALS(starts);
    stop_vals =  (int *) DYN_LIST_VALS(stops);
    step_vals =  (float *) DYN_LIST_VALS(steps);
    
    newlist = dfuCreateDynList(DF_LIST, length);
    
    switch (mode) {
    case 7:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 6:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 5:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 4:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 3:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
  }
  else if (DYN_LIST_DATATYPE(starts) == DF_LONG &&
      DYN_LIST_DATATYPE(stops) == DF_FLOAT &&
      DYN_LIST_DATATYPE(steps) == DF_LONG) {
    int *start_vals, *step_vals;
    float *stop_vals;
    
    start_vals = (int *) DYN_LIST_VALS(starts);
    stop_vals =  (float *) DYN_LIST_VALS(stops);
    step_vals =  (int *) DYN_LIST_VALS(steps);
    
    newlist = dfuCreateDynList(DF_LIST, length);
    
    switch (mode) {
    case 7:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 6:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 5:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 4:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 3:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
  }
  else if (DYN_LIST_DATATYPE(starts) == DF_FLOAT &&
      DYN_LIST_DATATYPE(stops) == DF_LONG &&
      DYN_LIST_DATATYPE(steps) == DF_LONG) {
    float *start_vals;
    int *stop_vals, *step_vals;
    
    start_vals = (float *) DYN_LIST_VALS(starts);
    stop_vals =  (int *) DYN_LIST_VALS(stops);
    step_vals =  (int *) DYN_LIST_VALS(steps);
    
    newlist = dfuCreateDynList(DF_LIST, length);
    
    switch (mode) {
    case 7:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 6:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 5:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 4:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[i], stop_vals[0], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 3:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesFloatT(start_vals[0], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
  }
  else if (DYN_LIST_DATATYPE(starts) == DF_LONG &&
	   DYN_LIST_DATATYPE(stops) == DF_LONG &&
	   DYN_LIST_DATATYPE(steps) == DF_LONG) {

    int *start_vals, *stop_vals, *step_vals;
    
    start_vals = (int *) DYN_LIST_VALS(starts);
    stop_vals =  (int *) DYN_LIST_VALS(stops);
    step_vals =  (int *) DYN_LIST_VALS(steps);
    
    newlist = dfuCreateDynList(DF_LIST, length);
    
    switch (mode) {
    case 7:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesLongT(start_vals[i], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 6:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesLongT(start_vals[i], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 5:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesLongT(start_vals[i], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 4:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesLongT(start_vals[i], stop_vals[0], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 3:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesLongT(start_vals[0], stop_vals[i], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesLongT(start_vals[0], stop_vals[i], step_vals[0], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = 
	  dynListSeriesLongT(start_vals[0], stop_vals[0], step_vals[i], type);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    }
  }
  return newlist;
}

DYN_LIST *dynListSeriesFloatT(float start, float stop, float step, int type)
{
  if (type == 0) 
    return dynListSeriesFloatExclude(start, stop, step);
  else return dynListSeriesFloat(start, stop, step);
}

DYN_LIST *dynListSeriesLongT(long start, long stop, long step, int type)
{
  if (type == 0) 
    return dynListSeriesLongExclude(start, stop, step);
  else return dynListSeriesLong(start, stop, step);
}

DYN_LIST *dynListSeriesFloat(float start, float stop, float step)
{
  double x;
  DYN_LIST *list;
  
  if (start == stop) {
    list = dfuCreateDynList(DF_FLOAT, 1);
    dfuAddDynListFloat(list, start);
  }
  
  else if (start < stop) {
    stop += 0.000001;
    if (step <= 0.0) return NULL;
    list = dfuCreateDynList(DF_FLOAT, (stop-start)/step+1);
    for (x = start; x <= stop; x+=step) {
      dfuAddDynListFloat(list, x);
    }
  }
  
  else {
    stop -= 0.000001;
    if (step >= 0.0) return NULL;
    list = dfuCreateDynList(DF_FLOAT, (stop-start)/step+1);
    for (x = start; x >= stop; x+=step) {
      dfuAddDynListFloat(list, x);
    }
  }
  
  return(list);
}

DYN_LIST *dynListSeriesFloatExclude(float start, float stop, float step)
{
  double x;
  DYN_LIST *list;
  
  if (start == stop) {
    list = dfuCreateDynList(DF_FLOAT, 1);
    //    dfuAddDynListFloat(list, start);
  }
  
  else if (start < stop) {
    if (step <= 0.0) return NULL;
    list = dfuCreateDynList(DF_FLOAT, (stop-start)/step+1);
    for (x = start; x < stop; x+=step) {
      dfuAddDynListFloat(list, x);
    }
  }
  
  else {
    if (step >= 0.0) return NULL;
    list = dfuCreateDynList(DF_FLOAT, (stop-start)/step+1);
    for (x = start; x > stop; x+=step) {
      dfuAddDynListFloat(list, x);
    }
  }
  
  return(list);
}

DYN_LIST *dynListSeriesLong(long start, long stop, long step)
{
  int x;
  DYN_LIST *list; 

  if (start == stop) {
    list = dfuCreateDynList(DF_LONG, 1);
    dfuAddDynListLong(list, start);
  }
  else if (start < stop) {
    if (step <= 0) return NULL;
    list = dfuCreateDynList(DF_LONG, (stop-start)/step+1);
    
    for (x = start; x <= stop; x+=step) {
      dfuAddDynListLong(list, x);
    }
  }
  else {
    if (step >= 0) return NULL;
    list = dfuCreateDynList(DF_LONG, (stop-start)/step+1);
    
    for (x = start; x >= stop; x+=step) {
      dfuAddDynListLong(list, x);
    }
  }
  return(list);
}

DYN_LIST *dynListSeriesLongExclude(long start, long stop, long step)
{
  int x;
  DYN_LIST *list; 

  if (start == stop) {
    list = dfuCreateDynList(DF_LONG, 1);
    //    dfuAddDynListLong(list, start);
  }
  else if (start < stop) {
    if (step <= 0) return NULL;
    list = dfuCreateDynList(DF_LONG, (stop-start)/step+1);
    
    for (x = start; x < stop; x+=step) {
      dfuAddDynListLong(list, x);
    }
  }
  else {
    if (step >= 0) return NULL;
    list = dfuCreateDynList(DF_LONG, (stop-start)/step+1);
    
    for (x = start; x > stop; x+=step) {
      dfuAddDynListLong(list, x);
    }
  }
  return(list);
}


DYN_LIST *dynListReplicate(DYN_LIST *dl, int n)
{
  int i, j;
  DYN_LIST *newlist;

  if (!dl) return NULL;

  newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), n*DYN_LIST_N(dl));
  if (!newlist) return NULL;
  
  for (i = 0; i < n; i++) {
    for (j = 0; j < DYN_LIST_N(dl); j++) {
      dynListCopyElement(dl, j, newlist);
    }
  }

  return(newlist);
}


DYN_LIST *dynListConvertList(DYN_LIST *dl, int type)
{
  int i;
  DYN_LIST *newlist = NULL;
  
  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    DYN_LIST *curlist;
    newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListConvertList(vals[i], type);
      if (curlist) dfuMoveDynListList(newlist, curlist);
      else {
	if (newlist) dfuFreeDynList(newlist);
	return NULL;
      }
    }
    return(newlist);
  }

  /* For empty input lists, just create an empty output list of desired type */
  if (!DYN_LIST_N(dl)) {
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_FLOAT:
    case DF_LONG:
    case DF_CHAR:
    case DF_SHORT:
      return dfuCreateDynList(type, 10);
      break;
    default:
      return NULL;
    }
  }

  switch (type) {
  case DF_FLOAT:
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	float *newvals = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
	memcpy(newvals, vals, DYN_LIST_N(dl)*sizeof(float));
	newlist = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_LONG:
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	float *newvals = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	float *newvals = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_CHAR:
      {
	char *vals = (char *) DYN_LIST_VALS(dl);
	float *newvals = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	float *newvals = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = (float) atof(vals[i]);
	}
	newlist = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
      }
      break;
    default:
      newlist = NULL;
      break;
    }
    break;
  case DF_LONG:
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_LONG:
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	memcpy(newvals, vals, DYN_LIST_N(dl)*sizeof(int));
	newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_CHAR:
      {
	char *vals = (char *) DYN_LIST_VALS(dl);
	int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = atoi(vals[i]);
	}
	newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
      }
      break;
    default:
      newlist = NULL;
      break;
    }
    break;
  case DF_CHAR:
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	char *newvals = (char *) calloc(DYN_LIST_N(dl), sizeof(char));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_CHAR, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_LONG:
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	char *newvals = (char *) calloc(DYN_LIST_N(dl), sizeof(char));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_CHAR, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	char *newvals = (char *) calloc(DYN_LIST_N(dl), sizeof(char));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_CHAR, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_CHAR:
      {
	char *vals = (char *) DYN_LIST_VALS(dl);
	char *newvals = (char *) calloc(DYN_LIST_N(dl), sizeof(char));
	memcpy(newvals, vals, DYN_LIST_N(dl)*sizeof(char));
	newlist = dfuCreateDynListWithVals(DF_CHAR, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	char *newvals = (char *) calloc(DYN_LIST_N(dl), sizeof(char));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = atoi(vals[i]);
	}
	newlist = dfuCreateDynListWithVals(DF_CHAR, DYN_LIST_N(dl), newvals);
      }
      break;
    default:
      newlist = NULL;
      break;
    }
    break;
  case DF_SHORT:
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	short *newvals = (short *) calloc(DYN_LIST_N(dl), sizeof(short));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_SHORT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_LONG:
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	short *newvals = (short *) calloc(DYN_LIST_N(dl), sizeof(short));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_SHORT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_CHAR:
      {
	char *vals = (char *) DYN_LIST_VALS(dl);
	short *newvals = (short *) calloc(DYN_LIST_N(dl), sizeof(short));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_SHORT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	short *newvals = (short *) calloc(DYN_LIST_N(dl), sizeof(short));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = atoi(vals[i]);
	}
	newlist = dfuCreateDynListWithVals(DF_SHORT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	short *newvals = (short *) calloc(DYN_LIST_N(dl), sizeof(short));
	memcpy(newvals, vals, DYN_LIST_N(dl)*sizeof(short));
	newlist = dfuCreateDynListWithVals(DF_SHORT, DYN_LIST_N(dl), newvals);
      }
      break;
    default:
      newlist = NULL;
      break;
    }
    break;
  }
  return (newlist);
}

DYN_LIST *dynListUnsignedConvertList(DYN_LIST *dl, int type)
{
  int i;
  DYN_LIST *newlist = NULL;
  
  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    DYN_LIST *curlist;
    newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListUnsignedConvertList(vals[i], type);
      if (curlist) dfuMoveDynListList(newlist, curlist);
      else {
	if (newlist) dfuFreeDynList(newlist);
	return NULL;
      }
    }
    return(newlist);
  }

  /* For empty input lists, just create an empty output list of desired type */
  if (!DYN_LIST_N(dl)) {
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_FLOAT:
    case DF_LONG:
    case DF_CHAR:
    case DF_SHORT:
      return dfuCreateDynList(type, 10);
      break;
    default:
      return NULL;
    }
  }

  switch (type) {
  case DF_FLOAT:
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	float *newvals = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
	memcpy(newvals, vals, DYN_LIST_N(dl)*sizeof(float));
	newlist = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_LONG:
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	float *newvals = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	float *newvals = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_CHAR:
      {
	unsigned char *vals = (unsigned char *) DYN_LIST_VALS(dl);
	float *newvals = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	float *newvals = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = (float) atof(vals[i]);
	}
	newlist = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
      }
      break;
    default:
      newlist = NULL;
      break;
    }
    break;
  case DF_LONG:
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_LONG:
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	memcpy(newvals, vals, DYN_LIST_N(dl)*sizeof(int));
	newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_CHAR:
      {
	unsigned char *vals = (unsigned char *) DYN_LIST_VALS(dl);
	int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = atoi(vals[i]);
	}
	newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
      }
      break;
    default:
      newlist = NULL;
      break;
    }
    break;
  case DF_CHAR:
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	unsigned char *newvals = (unsigned char *) calloc(DYN_LIST_N(dl), sizeof(char));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_CHAR, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_LONG:
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	unsigned char *newvals = (unsigned char *) calloc(DYN_LIST_N(dl), sizeof(char));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_CHAR, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	unsigned char *newvals = (unsigned char *) calloc(DYN_LIST_N(dl), sizeof(char));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_CHAR, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_CHAR:
      {
	char *vals = (char *) DYN_LIST_VALS(dl);
	char *newvals = (char *) calloc(DYN_LIST_N(dl), sizeof(char));
	memcpy(newvals, vals, DYN_LIST_N(dl)*sizeof(char));
	newlist = dfuCreateDynListWithVals(DF_CHAR, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	char *newvals = (char *) calloc(DYN_LIST_N(dl), sizeof(char));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = atoi(vals[i]);
	}
	newlist = dfuCreateDynListWithVals(DF_CHAR, DYN_LIST_N(dl), newvals);
      }
      break;
    default:
      newlist = NULL;
      break;
    }
    break;
  case DF_SHORT:
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	short *newvals = (short *) calloc(DYN_LIST_N(dl), sizeof(short));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_SHORT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_LONG:
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	short *newvals = (short *) calloc(DYN_LIST_N(dl), sizeof(short));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_SHORT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_CHAR:
      {
	unsigned char *vals = (unsigned char *) DYN_LIST_VALS(dl);
	short *newvals = (short *) calloc(DYN_LIST_N(dl), sizeof(short));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = vals[i];
	}
	newlist = dfuCreateDynListWithVals(DF_SHORT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	short *newvals = (short *) calloc(DYN_LIST_N(dl), sizeof(short));
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  newvals[i] = atoi(vals[i]);
	}
	newlist = dfuCreateDynListWithVals(DF_SHORT, DYN_LIST_N(dl), newvals);
      }
      break;
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	short *newvals = (short *) calloc(DYN_LIST_N(dl), sizeof(short));
	memcpy(newvals, vals, DYN_LIST_N(dl)*sizeof(short));
	newlist = dfuCreateDynListWithVals(DF_SHORT, DYN_LIST_N(dl), newvals);
      }
      break;
    default:
      newlist = NULL;
      break;
    }
    break;
  }
  return (newlist);
}

DYN_LIST *dynListRepeat(DYN_LIST *dl, DYN_LIST *rep)
{
  int i, j;
  DYN_LIST *newlist, *curlist;
  DYN_LIST **reps, **sublists;
  int *vals;
  int repmode;

  if (!dl || !rep) 
    return NULL;

  if (DYN_LIST_DATATYPE(rep) != DF_LIST && DYN_LIST_DATATYPE(rep) != DF_LONG) {
    return NULL;
  }

  if (DYN_LIST_DATATYPE(rep) == DF_LIST &&
      DYN_LIST_DATATYPE(dl) == DF_LIST) {

    if (DYN_LIST_N(rep) == 1) repmode = 0;
    else if (DYN_LIST_N(dl) == DYN_LIST_N(rep)) repmode = 1;
    else if (DYN_LIST_N(dl) == 1) repmode = 2;
    else if (DYN_LIST_N(dl) == 0) {
      return dfuCreateDynList(DF_LIST, 1);
    }
    else return NULL;

    newlist = dfuCreateDynList(DF_LIST, 2*DYN_LIST_N(dl));
    if (!newlist) return NULL;

    switch (repmode) {
    case 0:
      reps = (DYN_LIST **) DYN_LIST_VALS(rep);
      sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListRepeat(sublists[i], reps[0]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 1:
      reps = (DYN_LIST **) DYN_LIST_VALS(rep);
      sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListRepeat(sublists[i], reps[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    case 2:
      reps = (DYN_LIST **) DYN_LIST_VALS(rep);
      sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

      for (i = 0; i < DYN_LIST_N(rep); i++) {
	curlist = dynListRepeat(sublists[0], reps[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
      break;
    }
  }
  else if (DYN_LIST_DATATYPE(rep) == DF_LIST) {
    newlist = dfuCreateDynList(DF_LIST, 2*DYN_LIST_N(dl));
    if (!newlist) return NULL;
    
    reps = (DYN_LIST **) DYN_LIST_VALS(rep);
    
    for (i = 0; i < DYN_LIST_N(rep); i++) {
      curlist = dynListRepeat(dl, reps[i]);
      if (!curlist) {
	dfuFreeDynList(newlist);
	return NULL;
      }
      dfuMoveDynListList(newlist, curlist);
    }
  }
  else {
    if (!rep || DYN_LIST_DATATYPE(rep) != DF_LONG) return NULL;
    if (DYN_LIST_N(rep) == 1) repmode = 0;
    else if (DYN_LIST_N(rep) == DYN_LIST_N(dl)) repmode = 1;
    /* Kind of degenerate, but for compatibiity just return an empty list */
    else if (DYN_LIST_N(dl) == 0 || DYN_LIST_N(rep) == 0) {
      return dfuCreateDynList(DYN_LIST_DATATYPE(dl), 10);
    }
    else 
      return NULL;

    newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), 2*DYN_LIST_N(dl));
    if (!newlist) return NULL;
    
    vals = (int *) DYN_LIST_VALS(rep);
    
    if (repmode == 0) {
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	for (j = 0; j < vals[0]; j++) {
	  dynListCopyElement(dl, i, newlist);
	}
      }
    }
    else {
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	for (j = 0; j < vals[i]; j++) {
	  dynListCopyElement(dl, i, newlist);
	}
      }
    }
  }
  return(newlist);
}

DYN_LIST *dynListRepeatElements(DYN_LIST *dl, DYN_LIST *rep)
{
  int i;
  DYN_LIST *curlist, *newlist, **sublists;
  int *vals;

  if (!dl) 
    return NULL;
  if (!rep || DYN_LIST_DATATYPE(rep) != DF_LONG) return NULL;

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) 
    return dynListRepeat(dl, rep);

  newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
  if (!newlist) return NULL;
  
  vals = (int *) DYN_LIST_VALS(rep);
  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  for (i = 0; i < DYN_LIST_N(dl); i++) {
    curlist = dynListRepeatElements(sublists[i], rep);
    if (!curlist) {
      dfuFreeDynList(newlist);
      return NULL;
    }
    dfuMoveDynListList(newlist, curlist);
  }
  
  return(newlist);
}

int dynListDepth(DYN_LIST *dl, int level)
{
  DYN_LIST **vals;
  int i, d, curmax = level;
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return level;
  vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    d = dynListDepth(vals[i], level+1);
    if (d > curmax) curmax = d;
  }
  return curmax;
}

int dynListMaxListLength(DYN_LIST *dl)
{
  int max;
  DYN_LIST *lengths;

  if (!dl) return(0);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(0);
  
  lengths = dynListListLengths(dl);
  max = dynListMaxList(lengths);
  
  dfuFreeDynList(lengths);
  return(max);
}

int dynListMinListLength(DYN_LIST *dl)
{
  int min;
  DYN_LIST *lengths;

  if (!dl) return(0);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(0);
  
  lengths = dynListListLengths(dl);
  min = dynListMinList(lengths);
  
  dfuFreeDynList(lengths);
  return(min);
}



DYN_LIST *dynListMathOneArg(DYN_LIST *dl, int func_id)
{
  int i;
  DYN_LIST *list;
  MATH_FUNC1 func;


  struct MathFunc1 Math1Table[] = {
    { "ABS",    (MATH_FUNC1) fabs },
    { "ACOS",   (MATH_FUNC1) acos },
    { "ASIN",   (MATH_FUNC1) asin },
    { "ATAN",   (MATH_FUNC1) atan },
    { "CEIL",   (MATH_FUNC1) ceil },
    { "COS",    (MATH_FUNC1) cos },
    { "COSH",   (MATH_FUNC1) cosh },
    { "EXP",    (MATH_FUNC1) exp },
    { "FLOOR",  (MATH_FUNC1) floor },
    { "LOG",    (MATH_FUNC1) log },
    { "LOG10",  (MATH_FUNC1) log10 },
    { "ROUND",  (MATH_FUNC1) round },
    { "SIN",    (MATH_FUNC1) sin },
    { "SINH",   (MATH_FUNC1) sinh },
    { "SQRT",   (MATH_FUNC1) sqrt },
    { "TAN",    (MATH_FUNC1) tan },
    { "TANH",   (MATH_FUNC1) tanh },
    { "LGAMMA", (MATH_FUNC1) lgamma },
  };
  
  
  if (func_id >= DL_N_MATHFUNC1) {
    return NULL;
  }
  func = Math1Table[func_id].mfunc;

  if (DYN_LIST_DATATYPE(dl) == DF_STRING) return NULL;

  if (!DYN_LIST_N(dl)) {
    return dfuCreateDynList(DYN_LIST_DATATYPE(dl), 1);
  }

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      if (DLCheckMatherr) {
	switch (func_id) {
	case DL_SQRT:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] < 0.0) return NULL;
	  break;
	case DL_LOG:
	case DL_LOG10:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] <= 0.0) return NULL;
	  break;
	case DL_ACOS:
	case DL_ASIN:
	case DL_ATAN:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] < -1.0 || vals[i] > 1.0) return NULL;
	  break;
	}
      }
      switch (func_id) {
      case DL_ABS:
	{
	  int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	  for (i = 0; i < DYN_LIST_N(dl); i++) {
	    if (vals[i] >= 0) newvals[i] = vals[i];
	    else newvals[i] = -vals[i];
	  }
	  list = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
	  break;
	}
      case DL_CEIL:
      case DL_FLOOR:
      case DL_ROUND:
	{
	  int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	  for (i = 0; i < DYN_LIST_N(dl); i++) {
	    newvals[i] = (int) (*func)((double)vals[i]);
	  }
	  list = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
	  break;
	}
      default:
	{
	  size_t num_elements = DYN_LIST_N(dl);
	  if (num_elements <= SIZE_MAX/sizeof(long)) {
	    float *newvals = (float *) calloc(num_elements, sizeof(float));
	    for (i = 0; i < DYN_LIST_N(dl); i++) {
	      newvals[i] = (*func)((double)vals[i]);
	    }
	    list = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
	  }
	  else list = NULL;
	  break;
	}
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      if (DLCheckMatherr) {
	switch (func_id) {
	case DL_SQRT:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] < 0.0) return NULL;
	  break;
	case DL_LOG:
	case DL_LOG10:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] <= 0.0) return NULL;
	  break;
	case DL_ACOS:
	case DL_ASIN:
	case DL_ATAN:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] < -1.0 || vals[i] > 1.0) return NULL;
	  break;
	}
      }
      switch (func_id) {
      case DL_ABS:
	{
	  short *newvals = (short *) calloc(DYN_LIST_N(dl), sizeof(short));
	  for (i = 0; i < DYN_LIST_N(dl); i++) {
	    if (vals[i] >= 0) newvals[i] = vals[i];
	    else newvals[i] = -vals[i];
	  }
	  list = dfuCreateDynListWithVals(DF_SHORT, DYN_LIST_N(dl), newvals);
	  break;
	}
      case DL_CEIL:
      case DL_FLOOR:
      case DL_ROUND:
	{
	  int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	  for (i = 0; i < DYN_LIST_N(dl); i++) {
	    newvals[i] = (int) (*func)((double)vals[i]);
	  }
	  list = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
	  break;
	}
      default:
	{
	  size_t num_elements = DYN_LIST_N(dl);
	  if (num_elements <= SIZE_MAX/sizeof(long)) {
	    float *newvals = (float *) calloc(num_elements, sizeof(float));
	    for (i = 0; i < DYN_LIST_N(dl); i++) {
	      newvals[i] = (*func)((double)vals[i]);
	    }
	    list = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
	  }
	  else
	    list = NULL;
	  break;
	}
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      if (DLCheckMatherr) {
	switch (func_id) {
	case DL_SQRT:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] < 0.0) return NULL;
	  break;
	case DL_LOG:
	case DL_LOG10:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] <= 0.0) return NULL;
	  break;
	case DL_ACOS:
	case DL_ASIN:
	case DL_ATAN:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] < -1.0 || vals[i] > 1.0) return NULL;
	  break;
	}
      }
      switch (func_id) {
      case DL_CEIL:
      case DL_FLOOR:
      case DL_ROUND:
	{
	  int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	  for (i = 0; i < DYN_LIST_N(dl); i++) {
	    newvals[i] = (int) (*func)((double)vals[i]);
	  }
	  list = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
	  break;
	}
      default:
	{
	  size_t num_elements = DYN_LIST_N(dl);
	  if (num_elements <= SIZE_MAX/sizeof(long)) {
	    float *newvals = (float *) calloc(num_elements, sizeof(float));
	    if (newvals) {
	      for (i = 0; i < DYN_LIST_N(dl); i++) {
		newvals[i] = (*func)((double)vals[i]);
	      }
	      list = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
	    }
	    else list = NULL;
	  }
	  else
	    list = NULL;
	  break;
	}
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      if (DLCheckMatherr) {
	switch (func_id) {
	case DL_SQRT:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] < 0.0) return NULL;
	  break;
	case DL_LOG:
	case DL_LOG10:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] <= 0.0) return NULL;
	  break;
	case DL_ACOS:
	case DL_ASIN:
	case DL_ATAN:
	  for (i = 0;  i < DYN_LIST_N(dl); i++) 
	    if (vals[i] < -1.0 || vals[i] > 1.0) return NULL;
	  break;
	}
      }
      switch (func_id) {
      case DL_CEIL:
      case DL_FLOOR:
      case DL_ROUND:
	{
	  int *newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	  for (i = 0; i < DYN_LIST_N(dl); i++) {
	    newvals[i] = (int) (*func)((double)vals[i]);
	  }
	  list = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
	  break;
	}
      case DL_ABS:
	{
	  char *newvals = (char *) calloc(DYN_LIST_N(dl), sizeof(char));
	  memcpy(newvals, vals, DYN_LIST_N(dl));
	  list = dfuCreateDynListWithVals(DF_CHAR, DYN_LIST_N(dl), newvals);
	  break;
	}
      default:
	{
	  size_t num_elements = DYN_LIST_N(dl);
	  if (num_elements <= SIZE_MAX/sizeof(long)) {
	    float *newvals = (float *) calloc(DYN_LIST_N(dl),
					      sizeof(float));
	    for (i = 0; i < DYN_LIST_N(dl); i++) {
	      newvals[i] = (*func)((double)vals[i]);
	    }
	    list = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), newvals);
	  }
	  else list = NULL;
	  break;
	}
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curlist;
      list = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListMathOneArg(vals[i], func_id);
	if (!curlist) {
	  dfuFreeDynList(list);
	  return NULL;
	}
	dfuMoveDynListList(list, curlist);
      }
    }
    break;
  default:
    return(NULL);
  }
  return(list);
}

float dynListMaxList(DYN_LIST *dl)
{
  int i;
  float max = 0.0;

  if (!dl) return(0.0);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      max = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(max < vals[i])  max = vals[i];
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      max = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(max < vals[i])  max = vals[i];
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      max = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(max < vals[i])  max = vals[i];
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      max = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(max < vals[i])  max = vals[i];
    }
    break;
  case DF_LIST:
    {
      float val;
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      for (i = 1; i < DYN_LIST_N(dl); i++) {
	val = dynListMaxList(vals[i]);
	if(max < val)  max = val;
      }
    }
    break;
  default:
    return(0.0);
  }
  return(max);
}



int dynListMaxListIndex(DYN_LIST *dl)
{
  int i;
  float max;
  int maxi = 0;

  if (!dl) return(0.0);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      max = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(max < vals[i])  { max = vals[i]; maxi = i; }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      max = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(max < vals[i])  { max = vals[i]; maxi = i; }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      max = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(max < vals[i])  { max = vals[i]; maxi = i; }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      max = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(max < vals[i])  { max = vals[i]; maxi = i; }
    }
    break;
  default:
    return(-1);
  }
  return(maxi);
}


int dynListMinListIndex(DYN_LIST *dl)
{
  int i;
  float min;
  int mini = 0;

  if (!dl) return(0.0);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      min = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(min > vals[i])  { min = vals[i]; mini = i; }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      min = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(min > vals[i])  { min = vals[i]; mini = i; }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      min = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(min > vals[i])  { min = vals[i]; mini = i; }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      min = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(min > vals[i])  { min = vals[i]; mini = i; }
    }
    break;
  default:
    return(-1);
  }
  return(mini);
}

DYN_LIST *dynListMinMaxIndices(DYN_LIST *dl, int op)
{
  DYN_LIST **vals;
  DYN_LIST *list = NULL;
  int i;
  if (!dl || DYN_LIST_N(dl) == 0) return(NULL);
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);
  
  vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  list = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
  if (op == DL_MINIMUM) {
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListLong(list, dynListMinListIndex(vals[i]));
    }
  }
  else {
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListLong(list, dynListMaxListIndex(vals[i]));
    }
  }
  return list;
}

DYN_LIST *dynListMinIndices(DYN_LIST *dl)
{
  return dynListMinMaxIndices(dl, DL_MINIMUM);
}

DYN_LIST *dynListMaxIndices(DYN_LIST *dl)
{
  return dynListMinMaxIndices(dl, DL_MAXIMUM);
}

DYN_LIST *dynListMinMaxPositions(DYN_LIST *dl, int op)
{
  DYN_LIST **vals, *curlist;
  DYN_LIST *list = NULL;
  int i;
  if (!dl || DYN_LIST_DATATYPE(dl) == DF_STRING) return NULL;
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    list = dfuCreateDynList(DF_LONG, 1);
    if (DYN_LIST_N(dl) == 0) return(list);
    if (op == DL_MINIMUM) dfuAddDynListLong(list, dynListMinListIndex(dl));    
    else dfuAddDynListLong(list, dynListMaxListIndex(dl));    
    return list;
  }
  vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
  for (i = 0; i < DYN_LIST_N(dl); i++) {  
    curlist = dynListMinMaxPositions(vals[i], op);
    if (!curlist) {
      dfuFreeDynList(list);
      return NULL;
    }
    else dfuMoveDynListList(list, curlist);
  }
  return list;
}

/*
 * Regular recursive min/max finding
 */

DYN_LIST *dynListBMinMaxList(DYN_LIST *dl, int op)
{
  int i;
  DYN_LIST *retlist = NULL;
  if(!dl || DYN_LIST_N(dl) == 0) return NULL;
  if (op != DL_MINIMUM && op != DL_MAXIMUM) return NULL;
  
  retlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), 1);
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      int retval = vals[0];
      if (op == DL_MINIMUM) {
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval > vals[i])  retval = vals[i];
      }
      else {			/* MAX */
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval < vals[i])  retval = vals[i];
      }
      dfuAddDynListLong(retlist, retval);
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      short retval = vals[0];
      if (op == DL_MINIMUM) {
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval > vals[i])  retval = vals[i];
      }
      else {			/* MAX */
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval < vals[i])  retval = vals[i];
      }
      dfuAddDynListLong(retlist, retval);
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      float retval = vals[0];
      if (op == DL_MINIMUM) {
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval > vals[i])  retval = vals[i];
      }
      else {			/* MAX */
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval < vals[i])  retval = vals[i];
      }
      dfuAddDynListFloat(retlist, retval);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      char retval = vals[0];
      if (op == DL_MINIMUM) {
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval > vals[i])  retval = vals[i];
      }
      else {			/* MAX */
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval < vals[i])  retval = vals[i];
      }
      dfuAddDynListLong(retlist, retval);
    }
    break;
  case DF_LIST:
    {
      DYN_LIST *curlist;
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListBMinMaxList(vals[i],op);
	dfuMoveDynListList(retlist, curlist);
      }
    }
    break;
  default:
    return NULL;
  }
  return(retlist);
}

DYN_LIST *dynListMinMaxLists(DYN_LIST *dl, int op)
{
  int i, need_float = 0;
  DYN_LIST *list, *working, *curlist, *emptylist;
  DYN_LIST **vals;

  if (!dl) return(NULL);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);
  working = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
  emptylist = dfuCreateDynList(DF_LONG, 1);

  vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    curlist = dynListMinMaxList(vals[i], op);
    if (curlist) {
      dfuMoveDynListList(working, curlist);
    }
    else dfuAddDynListList(working, emptylist);	/* placeholder */
  }
  dfuFreeDynList(emptylist);
  
  /* determine if we need a float list, or if ints will do */
  vals = (DYN_LIST **) DYN_LIST_VALS(working);
  for (i = 0; i < DYN_LIST_N(working); i++) {
    if (DYN_LIST_N(vals[i]) == 0) continue;
    if (DYN_LIST_DATATYPE(vals[i]) == DF_FLOAT) need_float = 1;
  }
  if (need_float) {
    list = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(working));
    for (i = 0; i < DYN_LIST_N(working); i++) {
      if (DYN_LIST_N(vals[i]) == 0) dfuAddDynListFloat(list, 0.0);
      else {
	switch (DYN_LIST_DATATYPE(vals[i])) {
	case DF_FLOAT:
	  {
	    float *val = (float *) DYN_LIST_VALS(vals[i]);
	    dfuAddDynListFloat(list, val[0]);
	  }
	  break;
	case DF_LONG:
	  {
	    int *val = (int *) DYN_LIST_VALS(vals[i]);
	    dfuAddDynListFloat(list, val[0]);
	  }
	  break;
	case DF_SHORT:
	  {
	    short *val = (short *) DYN_LIST_VALS(vals[i]);
	    dfuAddDynListFloat(list, val[0]);
	  }
	  break;
	case DF_CHAR:
	  {
	    char *val = (char *) DYN_LIST_VALS(vals[i]);
	    dfuAddDynListFloat(list, val[0]);
	  }
	  break;
	}
      }
    }
  }
  else {			/* make a list of longs */
    list = dfuCreateDynList(DF_LONG, DYN_LIST_N(working));
    for (i = 0; i < DYN_LIST_N(working); i++) {
      if (DYN_LIST_N(vals[i]) == 0) dfuAddDynListLong(list, 0);
      else {
	switch (DYN_LIST_DATATYPE(vals[i])) {
	case DF_FLOAT:
	  {
	    float *val = (float *) DYN_LIST_VALS(vals[i]);
	    dfuAddDynListLong(list, val[0]);
	  }
	  break;
	case DF_LONG:
	  {
	    int *val = (int *) DYN_LIST_VALS(vals[i]);
	    dfuAddDynListLong(list, val[0]);
	  }
	  break;
	case DF_SHORT:
	  {
	    short *val = (short *) DYN_LIST_VALS(vals[i]);
	    dfuAddDynListLong(list, val[0]);
	  }
	  break;
	case DF_CHAR:
	  {
	    char *val = (char *) DYN_LIST_VALS(vals[i]);
	    dfuAddDynListLong(list, val[0]);
	  }
	  break;
	}
      }
    }
  }
  dfuFreeDynList(working);
  return(list);
}

DYN_LIST *dynListMinLists(DYN_LIST *dl)
{
  return dynListMinMaxLists(dl, DL_MINIMUM);
}

DYN_LIST *dynListMaxLists(DYN_LIST *dl)
{
  return dynListMinMaxLists(dl, DL_MAXIMUM);
}

float dynListMinList(DYN_LIST *dl)
{
  int i;
  float min = 0.0;

  if(!dl) return(0.0);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      min = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(min > vals[i])  min = vals[i];
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      min = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(min > vals[i])  min = vals[i];
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      min = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(min > vals[i])  min = vals[i];
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      min = vals[0];
      for (i = 1; i < DYN_LIST_N(dl); i++) 
	if(min > vals[i])  min = vals[i];
    }
    break;
  case DF_LIST:
    {
      float val;
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      for (i = 1; i < DYN_LIST_N(dl); i++) {
	val = dynListMinList(vals[i]);
	if (min > val)  min = val;
      }
    }
    break;
  default:
    return(0.0);
  }
  return(min);
}


DYN_LIST *dynListMinMaxList(DYN_LIST *dl, int op)
{
  int i;
  DYN_LIST *retlist = NULL;
  if(!dl || DYN_LIST_N(dl) == 0) return NULL;
  if (op != DL_MINIMUM && op != DL_MAXIMUM) return NULL;
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      int retval = vals[0];
      retlist = dfuCreateDynList(DF_LONG, 1);
      if (op == DL_MINIMUM) {
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval > vals[i])  retval = vals[i];
      }
      else {			/* MAX */
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval < vals[i])  retval = vals[i];
      }
      dfuAddDynListLong(retlist, retval);
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      short retval = vals[0];
      retlist = dfuCreateDynList(DF_LONG, 1);
      if (op == DL_MINIMUM) {
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval > vals[i])  retval = vals[i];
      }
      else {			/* MAX */
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval < vals[i])  retval = vals[i];
      }
      dfuAddDynListLong(retlist, retval);
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      float retval = vals[0];
      retlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), 1);
      if (op == DL_MINIMUM) {
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval > vals[i])  retval = vals[i];
      }
      else {			/* MAX */
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval < vals[i])  retval = vals[i];
      }
      dfuAddDynListFloat(retlist, retval);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      char retval = vals[0];
      retlist = dfuCreateDynList(DF_LONG, 1);
      if (op == DL_MINIMUM) {
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval > vals[i])  retval = vals[i];
      }
      else {			/* MAX */
	for (i = 1; i < DYN_LIST_N(dl); i++) 
	  if(retval < vals[i])  retval = vals[i];
      }
      dfuAddDynListLong(retlist, retval);
    }
    break;
  case DF_LIST:
    {
      DYN_LIST *curlist, *curbest;
      DYN_LIST **vals;
      vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      curbest = dynListMinMaxList(vals[0], op);
      for (i = 1; i < DYN_LIST_N(dl); i++) {
	curlist = dynListMinMaxList(vals[i], op);
	/* 
	 * The curlist and curbest are either NULL, or 
	 * of type DF_LONG or DF_FLOAT
	 * so now we compare the two, keeping the min/max of them
	 */
	if (!curlist) continue;
	else if (!curbest) curbest = curlist;
	else if (DYN_LIST_DATATYPE(curbest) == DF_LONG &&
		 DYN_LIST_DATATYPE(curlist) == DF_LONG) {
	  int *v0 = (int *) DYN_LIST_VALS(curbest);
	  int *v1 = (int *) DYN_LIST_VALS(curlist);
	  if (op == DL_MINIMUM) {
	    if (v0[0] > v1[0]) { dfuFreeDynList(curbest); curbest = curlist; }
	    else dfuFreeDynList(curlist);
	  }
	  if (op == DL_MAXIMUM) {
	    if (v0[0] < v1[0]) { dfuFreeDynList(curbest); curbest = curlist; }
	    else dfuFreeDynList(curlist);
	  }
	}
	else if (DYN_LIST_DATATYPE(curbest) == DF_FLOAT &&
		 DYN_LIST_DATATYPE(curlist) == DF_LONG) {
	  float *v0 = (float *) DYN_LIST_VALS(curbest);
	  int *v1 = (int *) DYN_LIST_VALS(curlist);
	  if (op == DL_MINIMUM) {
	    if (v0[0] > v1[0]) { dfuFreeDynList(curbest); curbest = curlist; }
	    else dfuFreeDynList(curlist);
	  }
	  if (op == DL_MAXIMUM) {
	    if (v0[0] < v1[0]) { dfuFreeDynList(curbest); curbest = curlist; }
	    else dfuFreeDynList(curlist);
	  }
	}
	else if (DYN_LIST_DATATYPE(curbest) == DF_LONG &&
		 DYN_LIST_DATATYPE(curlist) == DF_FLOAT) {
	  int *v0 = (int *) DYN_LIST_VALS(curbest);
	  float *v1 = (float *) DYN_LIST_VALS(curlist);
	  if (op == DL_MINIMUM) {
	    if (v0[0] > v1[0]) { dfuFreeDynList(curbest); curbest = curlist; }
	    else dfuFreeDynList(curlist);
	  }
	  if (op == DL_MAXIMUM) {
	    if (v0[0] < v1[0]) { dfuFreeDynList(curbest); curbest = curlist; }
	    else dfuFreeDynList(curlist);
	  }
	}
	else if (DYN_LIST_DATATYPE(curbest) == DF_FLOAT &&
		 DYN_LIST_DATATYPE(curlist) == DF_FLOAT) {
	  float *v0 = (float *) DYN_LIST_VALS(curbest);
	  float *v1 = (float *) DYN_LIST_VALS(curlist);
	  if (op == DL_MINIMUM) {
	    if (v0[0] > v1[0]) { dfuFreeDynList(curbest); curbest = curlist; }
	    else dfuFreeDynList(curlist);
	  }
	  if (op == DL_MAXIMUM) {
	    if (v0[0] < v1[0]) { dfuFreeDynList(curbest); curbest = curlist; }
	    else dfuFreeDynList(curlist);
	  }
	}
      }
      retlist = curbest;
    }
    break;
  default:
    return NULL;
  }
  return(retlist);
}


DYN_LIST *dynListRecursiveSearch(DYN_LIST *source, DYN_LIST *pattern, 
				 DYN_LIST *result)
{
  int i, j, k;
  DYN_LIST **vals;
  if (!source || !pattern) return result;
  if (DYN_LIST_DATATYPE(source) == DYN_LIST_DATATYPE(pattern)) {
    if (DYN_LIST_N(pattern) > DYN_LIST_N(source)) return NULL;
    k = DYN_LIST_N(source)-DYN_LIST_N(pattern)+1;
    for (i = 0; i < k; i++) {
      for (j = 0; j < DYN_LIST_N(pattern); j++) {
	if (dynListCompareElements(source, i+j, pattern, j)) break;
      }
      if (j == DYN_LIST_N(pattern)) {
	dfuAddDynListLong(result, i);
	return result;
      }
    }
  }
  else if (DYN_LIST_DATATYPE(source) == DF_LIST) {
    vals = (DYN_LIST **) DYN_LIST_VALS(source);
    for (i = 0; i < DYN_LIST_N(source); i++) {
      if (dynListRecursiveSearch(vals[i], pattern, result)) {
	dfuAddDynListLong(result, i);
	return result;
      }
    }
  }
  return NULL;
}

DYN_LIST *dynListFindSublist(DYN_LIST *source, DYN_LIST *pattern)
{
  int i, j, k;
  DYN_LIST *result = NULL;
  if (!source || !pattern) return NULL;
  
  if (DYN_LIST_DATATYPE(source) == DYN_LIST_DATATYPE(pattern)) {
    if (DYN_LIST_DATATYPE(source) == DF_LIST) {

      /* A single pattern search */
      if (DYN_LIST_N(pattern) == 1) {
	for (i = 0; i < DYN_LIST_N(source); i++) {
	  if (!dynListCompareElements(source, i, pattern, 0)) {
	    result = dfuCreateDynList(DF_LONG, 1);
	    dfuAddDynListLong(result, i);
	    return result;
	  }
	}
      }
      /* Multiple list pattern search */
      else {
	for (i = 0; i < DYN_LIST_N(source); i++) {
	  for (j = 0; j < DYN_LIST_N(pattern); j++) {
	    if (!dynListCompareElements(source, i, pattern, j)) {
	      result = dfuCreateDynList(DF_LONG, 2);
	      dfuAddDynListLong(result, j);
	      dfuAddDynListLong(result, i);
	      return result;
	    }
	  }
	}
      }
      return result;
    }

    if (DYN_LIST_N(pattern) > DYN_LIST_N(source)) return NULL;
    k = DYN_LIST_N(source)-DYN_LIST_N(pattern)+1;
    for (i = 0; i < k; i++) {
      for (j = 0; j < DYN_LIST_N(pattern); j++) {
	if (dynListCompareElements(source, i+j, pattern, j)) break;
      }
      if (j == DYN_LIST_N(pattern)) {
	result = dfuCreateDynList(DF_LONG, 1);
	dfuAddDynListLong(result, i);
	return result;
      }
    }
  }
  else if (DYN_LIST_DATATYPE(pattern) == DF_LIST) {
    DYN_LIST **patterns = (DYN_LIST **) DYN_LIST_VALS(pattern);
    for (i = 0; i < DYN_LIST_N(source); i++) {
      for (j = 0; j < DYN_LIST_N(pattern); j++) {
	if (i+DYN_LIST_N(patterns[j]) > DYN_LIST_N(source)) continue;
	for (k = 0; k < DYN_LIST_N(patterns[j]); k++) {
	  if (dynListCompareElements(source, i+k, patterns[j], k)) break;
	}
	if (k == DYN_LIST_N(patterns[j])) {
	  result = dfuCreateDynList(DF_LONG, 2);
	  dfuAddDynListLong(result, j);
	  dfuAddDynListLong(result, i);
	  return result;
	}
      }
    }
  }
  /* 
   * Do a recursive (depth first) search and return indices 
   * leading to pattern (they are reversed before being returned)
   */
  else if (DYN_LIST_DATATYPE(source) == DF_LIST) {
    DYN_LIST *out, *reversed;
    result = dfuCreateDynList(DF_LONG, 2);
    out = dynListRecursiveSearch(source, pattern, result);
    if (out) {
      reversed = dynListReverseList(out);
      dfuFreeDynList(out);
      result = reversed;
    }
    else {
      dfuFreeDynList(result);
      return NULL;
    }
  }
  return result;
}

DYN_LIST *dynListFindSublistAll(DYN_LIST *source, DYN_LIST *pattern)
{
  int i, j, k;
  DYN_LIST *result = NULL, *newlist;
  if (!source || !pattern) return NULL;

  if (DYN_LIST_DATATYPE(source) == DYN_LIST_DATATYPE(pattern)) {
    if (DYN_LIST_DATATYPE(source) == DF_LIST) {
      if (DYN_LIST_N(pattern) == 1) {
	result = dfuCreateDynList(DF_LONG, 4);
	for (i = 0; i < DYN_LIST_N(source); i++) {
	  if (!dynListCompareElements(source, i, pattern, 0))
	    dfuAddDynListLong(result, i);
	}
      }
      else {			/* multiple element pattern list */
	result = dfuCreateDynList(DF_LIST, 4);
	newlist = dfuCreateDynList(DF_LONG, 2);
	for (i = 0; i < DYN_LIST_N(source); i++) {
	  for (j = 0; j < DYN_LIST_N(pattern); j++) {
	    if (!dynListCompareElements(source, i, pattern, j)) {
	      dfuAddDynListLong(newlist, j);
	      dfuAddDynListLong(newlist, i);
	      dfuAddDynListList(result, newlist);
	      dfuResetDynList(newlist);
	    }
	  }
	}
	dfuFreeDynList(newlist);
      }
      return result;
    }

    else if (DYN_LIST_N(pattern) > DYN_LIST_N(source)) {
      return dfuCreateDynList(DF_LONG, 1); /* empty list */
    }

    k = DYN_LIST_N(source)-DYN_LIST_N(pattern)+1;
    result = dfuCreateDynList(DF_LONG, 4);
    for (i = 0; i < k; i++) {
      for (j = 0; j < DYN_LIST_N(pattern); j++) {
	if (dynListCompareElements(source, i+j, pattern, j)) break;
      }
      if (j == DYN_LIST_N(pattern)) {
	dfuAddDynListLong(result, i);
      }
    }
  }
  else if (DYN_LIST_DATATYPE(pattern) == DF_LIST) {
    DYN_LIST **patterns = (DYN_LIST **) DYN_LIST_VALS(pattern);
    result = dfuCreateDynList(DF_LIST, 4);
    for (i = 0; i < DYN_LIST_N(source); i++) {
      for (j = 0; j < DYN_LIST_N(pattern); j++) {
	if (i+DYN_LIST_N(patterns[j]) > DYN_LIST_N(source)) continue;
	for (k = 0; k < DYN_LIST_N(patterns[j]); k++) {
	  if (dynListCompareElements(source, i+k, patterns[j], k)) break;
	}
	if (k == DYN_LIST_N(patterns[j])) {
	  newlist = dfuCreateDynList(DF_LONG, 2);
	  dfuAddDynListLong(newlist, j);
	  dfuAddDynListLong(newlist, i);
	  dfuMoveDynListList(result, newlist);
	}
      }
    }
  }

  /* This does a findAll applied to each sublist in source using pattern */
  else if (DYN_LIST_DATATYPE(source) == DF_LIST) {
    DYN_LIST *curfind;
    DYN_LIST **sources = (DYN_LIST **) DYN_LIST_VALS(source);
    result = dfuCreateDynList(DF_LIST, DYN_LIST_N(source));
    for (i = 0; i < DYN_LIST_N(source); i++) {
      curfind = dynListFindSublistAll(sources[i], pattern);
      if (!curfind) {
	dfuFreeDynList(result);
	return NULL;
      }
      dfuMoveDynListList(result, curfind);
    }
  }

  return result;
}

/*
 * dlFindIndices
 *
 *  Use hash tables (UT_Hash) https://troydhanson.github.io/uthash/
 * to search a source list of ints or strings and return indices corresponding
 * to locations of search items (which can be sublists)
 */
typedef struct {
  int key;
  int index;
  UT_hash_handle hh;
} HashEntryInt;

typedef struct {
  char *key;
  int index;
  UT_hash_handle hh;
} HashEntryStr;

void build_int_hash_table(DYN_LIST *table, HashEntryInt **hash_table)
{
  int table_size = DYN_LIST_N(table);

  switch (DYN_LIST_DATATYPE(table)) {
  case DF_LONG:
    {
      int *table_vals = (int *) DYN_LIST_VALS(table);
      for (int i = 0; i < table_size; i++) {
	HashEntryInt *entry = malloc(sizeof(HashEntryInt));
	entry->key = table_vals[i];
	entry->index = i;
	HASH_ADD_INT(*hash_table, key, entry); // Add entry to the hash table
      }
    }
    break;
  case DF_SHORT:
    {
      short *table_vals = (short *) DYN_LIST_VALS(table);
      for (int i = 0; i < table_size; i++) {
	HashEntryInt *entry = malloc(sizeof(HashEntryInt));
	entry->key = table_vals[i];
	entry->index = i;
	HASH_ADD_INT(*hash_table, key, entry); // Add entry to the hash table
      }
    }
    break;
  case DF_CHAR:
    {
      char *table_vals = (char *) DYN_LIST_VALS(table);
      for (int i = 0; i < table_size; i++) {
	HashEntryInt *entry = malloc(sizeof(HashEntryInt));
	entry->key = table_vals[i];
	entry->index = i;
	HASH_ADD_INT(*hash_table, key, entry); // Add entry to the hash table
      }
    }
    break;
  }    
}

void build_str_hash_table(DYN_LIST *table, HashEntryStr **hash_table)
{
  int table_size = DYN_LIST_N(table);
  char **table_vals = (char **) DYN_LIST_VALS(table);
  for (int i = 0; i < table_size; i++) {
    HashEntryStr *entry = malloc(sizeof(HashEntryStr));
    entry->key = table_vals[i];
    entry->index = i;
    HASH_ADD_STR(*hash_table, key, entry); // Add entry to the hash table
  }
}

void free_int_hash_table(HashEntryInt *hash_table) {
  HashEntryInt *current_entry, *tmp;
  HASH_ITER(hh, hash_table, current_entry, tmp) {
    HASH_DEL(hash_table, current_entry); // Delete the entry from the hash table
    free(current_entry);                // Free the memory
  }
}

void free_str_hash_table(HashEntryStr *hash_table) {
  HashEntryStr *current_entry, *tmp;
  HASH_ITER(hh, hash_table, current_entry, tmp) {
    HASH_DEL(hash_table, current_entry); // Delete the entry from the hash table
    free(current_entry);                // Free the memory
  }
}

DYN_LIST *doFindIndicesInt(HashEntryInt *hash_table, DYN_LIST *search_list)
{
  if (DYN_LIST_DATATYPE(search_list) != DF_LONG &&
      DYN_LIST_DATATYPE(search_list) != DF_SHORT &&
      DYN_LIST_DATATYPE(search_list) != DF_CHAR &&
      DYN_LIST_DATATYPE(search_list) != DF_LIST) return NULL;

  switch (DYN_LIST_DATATYPE(search_list)) {
  case DF_LONG:
    {
      DYN_LIST *result = dfuCreateDynList(DF_LONG, DYN_LIST_N(search_list));
      int search_size = DYN_LIST_N(search_list);
      int *search_values = (int *) DYN_LIST_VALS(search_list);
      for (int i = 0; i < search_size; i++) {
	HashEntryInt *entry;
	HASH_FIND_INT(hash_table, &search_values[i], entry);
	if (entry) {
	  dfuAddDynListLong(result, entry->index);
	} else {
	  dfuAddDynListLong(result, -1);
	}
      }
      return result;
      break;
    }
  case DF_SHORT:
    {
      DYN_LIST *result = dfuCreateDynList(DF_LONG, DYN_LIST_N(search_list));
      int search_size = DYN_LIST_N(search_list);
      short *search_values = (short *) DYN_LIST_VALS(search_list);
      for (int i = 0; i < search_size; i++) {
	HashEntryInt *entry;
	int v = search_values[i];
	HASH_FIND_INT(hash_table, &v, entry);
	if (entry) {
	  dfuAddDynListLong(result, entry->index);
	} else {
	  dfuAddDynListLong(result, -1);
	}
      }
      return result;
      break;
    }
  case DF_CHAR:
    {
      DYN_LIST *result = dfuCreateDynList(DF_LONG, DYN_LIST_N(search_list));
      int search_size = DYN_LIST_N(search_list);
      unsigned char *search_values = (unsigned char *) DYN_LIST_VALS(search_list);
      for (int i = 0; i < search_size; i++) {
	HashEntryInt *entry;
	int v = search_values[i];
	HASH_FIND_INT(hash_table, &v, entry);
	if (entry) {
	  dfuAddDynListLong(result, entry->index);
	} else {
	  dfuAddDynListLong(result, -1);
	}
      }
      return result;
      break;
    }
  case DF_LIST:
    {
      DYN_LIST *newlist;
      DYN_LIST *result = dfuCreateDynList(DF_LIST, DYN_LIST_N(search_list));
      DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(search_list);
      for (int i = 0; i < DYN_LIST_N(search_list); i++) {
	newlist = doFindIndicesInt(hash_table, sublists[i]);
	if (!newlist) {
	  dfuFreeDynList(result);
	  return NULL;
	}
	dfuMoveDynListList(result, newlist);
      }
      return result;
      break;
    }
  default:
    return NULL;
  }
}

DYN_LIST *doFindIndicesStr(HashEntryStr *hash_table, DYN_LIST *search_list)
{
  if (DYN_LIST_DATATYPE(search_list) != DF_STRING &&
      DYN_LIST_DATATYPE(search_list) != DF_LIST) return NULL;

  switch (DYN_LIST_DATATYPE(search_list)) {
  case DF_STRING:
    {
      DYN_LIST *result = dfuCreateDynList(DF_LONG, DYN_LIST_N(search_list));
      int search_size = DYN_LIST_N(search_list);
      char **search_values = (char **) DYN_LIST_VALS(search_list);
      for (int i = 0; i < search_size; i++) {
	HashEntryStr *entry;
	HASH_FIND_STR(hash_table, search_values[i], entry);
	if (entry) {
	  dfuAddDynListLong(result, entry->index);
	} else {
	  dfuAddDynListLong(result, -1);
	}
      }
      return result;
      break;
    }
  case DF_LIST:
    {
      DYN_LIST *newlist;
      DYN_LIST *result = dfuCreateDynList(DF_LIST, DYN_LIST_N(search_list));
      DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(search_list);
      for (int i = 0; i < DYN_LIST_N(search_list); i++) {
	newlist = doFindIndicesStr(hash_table, sublists[i]);
	if (!newlist) {
	  dfuFreeDynList(result);
	  return NULL;
	}
	dfuMoveDynListList(result, newlist);
      }
      return result;
      break;
    }
  default:
    return NULL;
  }
}

DYN_LIST *dynListFindIndices(DYN_LIST *source_values, DYN_LIST *search_values)
{
  /* these must be initialized to zero for proper initialization! */
  HashEntryInt *hash_table_int = NULL;
  HashEntryStr *hash_table_str = NULL;
  
  DYN_LIST *result = NULL;

  switch (DYN_LIST_DATATYPE(source_values)) {
  case DF_CHAR:
  case DF_SHORT:
  case DF_LONG:
    build_int_hash_table(source_values, &hash_table_int);
    break;
  case DF_STRING:
    build_str_hash_table(source_values, &hash_table_str);
    break;
  default:
    return NULL;
  }

  switch (DYN_LIST_DATATYPE(search_values)) {
  case DF_CHAR:
  case DF_SHORT:
  case DF_LONG:
    result = doFindIndicesInt(hash_table_int, search_values);
    break;
  case DF_STRING:
    result = doFindIndicesStr(hash_table_str, search_values);
    break;
  case DF_LIST:
    switch (DYN_LIST_DATATYPE(source_values)) {
    case DF_CHAR:
    case DF_SHORT:
    case DF_LONG:
      result = doFindIndicesInt(hash_table_int, search_values);
      break;
    case DF_STRING:
      result = doFindIndicesStr(hash_table_str, search_values);
      break;
    default:
      goto done;
    }
  default:
    goto done;
  }

 done:
  if (hash_table_int) free_int_hash_table(hash_table_int);
  if (hash_table_str) free_str_hash_table(hash_table_str);
  return result;
  
}


DYN_LIST *dynListCountOccurences(DYN_LIST *source, DYN_LIST *pattern)
{
  DYN_LIST *find_result, *trans_result, *hist_result, *range, **vals;
  DYN_LIST *pattern_list = NULL;
  int i, copymode;
  
  if (!source || !pattern) return(NULL);

  /*
   * If the source is a list of lists, then apply recursively
   */
  
  if (DYN_LIST_DATATYPE(source) == DF_LIST) {
    DYN_LIST *newsub;
    DYN_LIST *newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(source));
    DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(source);
    DYN_LIST **subpatterns;
    
    if (DYN_LIST_DATATYPE(pattern) != DF_LIST) return NULL;
    if (DYN_LIST_N(pattern) == 1) copymode = 0;
    else if (DYN_LIST_N(pattern) == DYN_LIST_N(source)) copymode = 1;
    else return NULL;

    subpatterns = (DYN_LIST **) DYN_LIST_VALS(pattern);
    for (i = 0; i < DYN_LIST_N(source); i++) {
      if (copymode == 0) {
	newsub = dynListCountOccurences(sublists[i], subpatterns[0]);
      }
      else {
	newsub = dynListCountOccurences(sublists[i], subpatterns[i]);
      }
      if (!newsub) {
	dfuFreeDynList(newlist);
	return NULL;
      }
      dfuMoveDynListList(newlist, newsub);
    }
    return(newlist);
  }
  
  
  /* 
   * Here's the approach:
   *    Do a findAll, followed by a transpose and then grab the list of
   *    indices that are found (vals[0]).  Then HistLists is
   *    applied to that list giving a list of counts -- our result.
   */

  /* 
   * If the pattern is a list of scalars, make it a list of single
   * element lists so that dynListFindSublistAll operates correctly 
   * (target/index pair mode)
   */
  if (DYN_LIST_DATATYPE(pattern) != DF_LIST) {
    int i;
    DYN_LIST *newlist;
    pattern_list = dfuCreateDynList(DF_LIST, DYN_LIST_N(pattern));
    newlist = dfuCreateDynList(DYN_LIST_DATATYPE(pattern), 1);
    for (i = 0; i < DYN_LIST_N(pattern); i++) {
      dynListCopyElement(pattern, i, newlist);
      dfuAddDynListList(pattern_list, newlist);
      dfuResetDynList(newlist);
    }
    find_result = dynListFindSublistAll(source, pattern_list);
    dfuFreeDynList(newlist);
    dfuFreeDynList(pattern_list);
  }
  else {
    find_result = dynListFindSublistAll(source, pattern);
  }

  if (!find_result) return NULL;

  if (DYN_LIST_DATATYPE(find_result) != DF_LIST) {
    hist_result = dfuCreateDynList(DF_LONG, 1);
    dfuAddDynListLong(hist_result, DYN_LIST_N(find_result));
    dfuFreeDynList(find_result);
    return(hist_result);
  }

  /* If nothing was found just return a bunch of zeros */
  if (DYN_LIST_N(find_result) == 0) {
    hist_result = dynListZerosInt(DYN_LIST_N(pattern));
    dfuFreeDynList(find_result);
    return hist_result;
  }

  trans_result = dynListTransposeList(find_result);
  dfuFreeDynList(find_result);
  if (!trans_result) return NULL;

  vals = (DYN_LIST **) DYN_LIST_VALS(trans_result);

  /* Now do the histogram */
  range = dfuCreateDynList(DF_LONG, 2);
  dfuAddDynListLong(range, 0);
  dfuAddDynListLong(range, DYN_LIST_N(pattern));
  hist_result = dynListHistList(vals[0], range, DYN_LIST_N(pattern));

  /* Clean up and return */
  dfuFreeDynList(range);
  dfuFreeDynList(trans_result);
  return(hist_result);
}


DYN_LIST *dynListListLengths(DYN_LIST *dl)
{
  int i;
  DYN_LIST *list;
  DYN_LIST **vals;

  if (!dl) return(NULL);

  vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);

  list = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (vals[i]) dfuAddDynListLong(list, DYN_LIST_N(vals[i]));
    else  dfuAddDynListLong(list, 0);
  }
  
  return(list);
}

DYN_LIST *dynListLLength(DYN_LIST *dl)
{
  int i;
  DYN_LIST *list;
  DYN_LIST **vals;

  if (!dl) return(NULL);

  vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    list = dfuCreateDynList(DF_LONG, 1);
    dfuAddDynListLong(list, DYN_LIST_N(dl));    
  }
  else {
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListLLength(vals[i]);
      dfuMoveDynListList(list, curlist);
    }
  }
  return(list);
}

DYN_LIST *dynListLengthLists(DYN_LIST *dl)
{
  int i;
  DYN_LIST *list;
  DYN_LIST **vals;

  if (!dl) return(NULL);

  vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);

  list = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    dfuAddDynListLong(list, dynListLengthList(vals[i]));
  }
  
  return(list);
}

long dynListLengthList(DYN_LIST *dl)
{
  int i;
  int length = 0;

  if (!dl) return(0);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
  case DF_SHORT:
  case DF_FLOAT:
  case DF_CHAR:
  case DF_STRING:
    length = DYN_LIST_N(dl);
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) length+=dynListLengthList(vals[i]);
    }
    break;
  default:
    return(0);
  }
  return(length);
}


static int string_match(char *a, char *b)
{
  int position = 0;
  char *x, *y;
 
  x = a;
  y = b;
 
  while(*a)
    {
      while(*x==*y)
	{
	  x++;
	  y++;
	  if(*x=='\0'||*y=='\0')
            break;         
	}   
      if(*y=='\0')
	break;
 
      a++;
      position++;
      x = a;
      y = b;
    }
  if(*a)
    return 1;
  else   
    return 0;   
}

DYN_LIST *dynListStringMatch(DYN_LIST *dl, DYN_LIST *plist, int options)
{
  int i;
  int *newvals = NULL;
  DYN_LIST *newlist = NULL;
  if (!dl) return NULL;

  /* This should change in the future */
  if (DYN_LIST_DATATYPE(plist) != DF_STRING) return NULL;
  if (DYN_LIST_N(plist) != 1) return NULL;

  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    DYN_LIST *curlist;
    newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));

    /* Doesn't do anything fancy with the plist for now */
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListStringMatch(vals[i], plist, options);
      if (!curlist) {
	dfuFreeDynList(newlist);
	return NULL;
      }
      dfuMoveDynListList(newlist, curlist);
    }
    return newlist;
  }

  else {
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG:
    case DF_SHORT:
    case DF_CHAR:
    case DF_FLOAT:
      return NULL;
      break;
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	char **patterns = (char **) DYN_LIST_VALS(plist);

	/* Need to figure out the best way to handle this...*/
	//	int Tcl_StringMatch(char *, char *);
	
	newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
	for (i = 0; i < DYN_LIST_N(dl); i++) 
	  //	  newvals[i] = Tcl_StringMatch(vals[i], patterns[0]);
	  string_match(vals[i], patterns[0]);
	break;
      }
    }
    if (newvals)
      newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
    return(newlist);
  }
}


DYN_LIST *dynListMeanLists(DYN_LIST *dl)
{
  int i, type = DF_FLOAT;
  DYN_LIST *list, *trans, *newlist;
  DYN_LIST **sublists;

  if (!dl) return(NULL);

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);

  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (DYN_LIST_DATATYPE(sublists[i]) != 
	DYN_LIST_DATATYPE(sublists[0])) return(NULL);
    if (DYN_LIST_N(dl)) type = DYN_LIST_DATATYPE(sublists[i]);
  }

  switch (type) {
  case DF_LIST:
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      if (DYN_LIST_N(sublists[i])) {
	trans = dynListTransposeList(sublists[i]);
	if (!trans) {
	  dfuFreeDynList(list);
	  return NULL;
	}
	newlist = dynListMeanLists(trans);
	dfuFreeDynList(trans);
	if (!newlist) {
	  dfuFreeDynList(list);
	  return NULL;
	}
      }
      else {
	newlist = dfuCreateDynList(type, 10);
      }
      dfuMoveDynListList(list, newlist);
    }
    break;
  default:
    list = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListFloat(list, dynListMeanList(sublists[i]));
    }
    break;
  }

  return(list);
}


DYN_LIST *dynListBMeanLists(DYN_LIST *dl)
{
  int i;
  DYN_LIST *list;
  DYN_LIST **vals;

  if (!dl) return(NULL);

  vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    list = dfuCreateDynList(DF_FLOAT, 1);
    dfuAddDynListFloat(list, dynListMeanList(dl));
  }
  else {
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListBMeanLists(vals[i]);
      dfuMoveDynListList(list, curlist);
    }
  }
  return(list);
}

DYN_LIST *dynListBStdLists(DYN_LIST *dl)
{
  int i;
  DYN_LIST *list;
  DYN_LIST **vals;

  if (!dl) return(NULL);

  vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    list = dfuCreateDynList(DF_FLOAT, 1);
    dfuAddDynListFloat(list, dynListStdList(dl));
  }
  else {
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListBStdLists(vals[i]);
      dfuMoveDynListList(list, curlist);
    }
  }
  return(list);
}


DYN_LIST *dynListHMeanLists(DYN_LIST *dl)
{
  int i, type = DF_FLOAT;
  DYN_LIST *list, *newlist;
  DYN_LIST **sublists;

  if (!dl) return(NULL);

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);

  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (DYN_LIST_DATATYPE(sublists[i]) != 
	DYN_LIST_DATATYPE(sublists[0])) return(NULL);
    if (DYN_LIST_N(dl)) type = DYN_LIST_DATATYPE(sublists[i]);
  }

  switch (type) {
  case DF_LIST:
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      if (DYN_LIST_N(sublists[i])) {
	newlist = dynListMeanLists(sublists[i]);
      }
      else {
	newlist = dfuCreateDynList(type, 10);
      }
      dfuMoveDynListList(list, newlist);
    }
    break;
  default:
    list = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListFloat(list, dynListMeanList(sublists[i]));
    }
    break;
  }

  return(list);
}

float dynListMeanList(DYN_LIST *dl)
{
  int i;
  float sum = 0.0;
  
  if (!dl || !DYN_LIST_N(dl)) return(0.0);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) sum+=vals[i];
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) sum+=vals[i];
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) if (!isnan(vals[i])) sum+=vals[i];
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) sum+=vals[i];
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) sum+=dynListMeanList(vals[i]);
    }
    break;
  default:
    return(0.0);
  }
  return(sum/DYN_LIST_N(dl));
}



DYN_LIST* dynListAverageList(DYN_LIST *dl)
{
  int i, j, length;
  float sum = 0.0;
  DYN_LIST **sublists, *newlist = NULL;

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return NULL;
  if (!DYN_LIST_N(dl)) {	/* a bit degenerate, but whatever */
    return dfuCreateDynList(DF_FLOAT, 1);
  }
  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if ((DYN_LIST_DATATYPE(sublists[i]) !=
	 DYN_LIST_DATATYPE(sublists[0])) ||
	(DYN_LIST_N(sublists[i]) != 
	 DYN_LIST_N(sublists[0]))) return NULL;
  }
  length = DYN_LIST_N(sublists[0]);
  newlist = dfuCreateDynList(DF_FLOAT, length);

  switch (DYN_LIST_DATATYPE(sublists[0])) {
  case DF_LONG:
    {
      int *vals;
      for (i = 0; i < length; i++) {
	for (sum = 0, j = 0; j < DYN_LIST_N(dl); j++) {
	  vals = (int *) DYN_LIST_VALS(sublists[j]);
	  sum+=vals[i];
	}
	dfuAddDynListFloat(newlist, sum/j);
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals;
      for (i = 0; i < length; i++) {
	for (sum = 0, j = 0; j < DYN_LIST_N(dl); j++) {
	  vals = (short *) DYN_LIST_VALS(sublists[j]);
	  sum+=vals[i];
	}
	dfuAddDynListFloat(newlist, sum/j);
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals;
      for (i = 0; i < length; i++) {
	for (sum = 0, j = 0; j < DYN_LIST_N(dl); j++) {
	  vals = (float *) DYN_LIST_VALS(sublists[j]);
	  sum+=vals[i];
	}
	dfuAddDynListFloat(newlist, sum/j);
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals;
      for (i = 0; i < length; i++) {
	for (sum = 0, j = 0; j < DYN_LIST_N(dl); j++) {
	  vals = (char *) DYN_LIST_VALS(sublists[j]);
	  sum+=vals[i];
	}
	dfuAddDynListFloat(newlist, sum/j);
      }
    }
    break;
  default:
    break;
  }
  return(newlist);
}

DYN_LIST* dynListSumColsList(DYN_LIST *dl)
{
  int i, j, length;
  DYN_LIST **sublists, *newlist = NULL;

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return NULL;
  if (!DYN_LIST_N(dl)) {	/* a bit degenerate, but whatever */
    return dfuCreateDynList(DF_FLOAT, 1);
  }
  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if ((DYN_LIST_DATATYPE(sublists[i]) !=
	 DYN_LIST_DATATYPE(sublists[0])) ||
	(DYN_LIST_N(sublists[i]) != 
	 DYN_LIST_N(sublists[0]))) return NULL;
  }
  length = DYN_LIST_N(sublists[0]);
 
  switch (DYN_LIST_DATATYPE(sublists[0])) {
  case DF_LONG:
    {
      int *vals;
      int sum = 0;
      newlist = dfuCreateDynList(DF_LONG, length);
      for (i = 0; i < length; i++) {
	for (sum = 0, j = 0; j < DYN_LIST_N(dl); j++) {
	  vals = (int *) DYN_LIST_VALS(sublists[j]);
	  sum+=vals[i];
	}
	dfuAddDynListLong(newlist, sum);
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals;
      int sum = 0;
      newlist = dfuCreateDynList(DF_LONG, length);
      for (i = 0; i < length; i++) {
	for (sum = 0, j = 0; j < DYN_LIST_N(dl); j++) {
	  vals = (short *) DYN_LIST_VALS(sublists[j]);
	  sum+=vals[i];
	}
	dfuAddDynListLong(newlist, sum);
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals;
      float sum = 0.;
      newlist = dfuCreateDynList(DF_FLOAT, length);
      for (i = 0; i < length; i++) {
	for (sum = 0, j = 0; j < DYN_LIST_N(dl); j++) {
	  vals = (float *) DYN_LIST_VALS(sublists[j]);
	  sum+=vals[i];
	}
	dfuAddDynListFloat(newlist, sum);
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals;
      int sum = 0;
      newlist = dfuCreateDynList(DF_LONG, length);
      for (i = 0; i < length; i++) {
	for (sum = 0, j = 0; j < DYN_LIST_N(dl); j++) {
	  vals = (char *) DYN_LIST_VALS(sublists[j]);
	  sum+=vals[i];
	}
	dfuAddDynListLong(newlist, sum);
      }
    }
    break;
  default:
    break;
  }
  return(newlist);
}


DYN_LIST *dynListHVarLists(DYN_LIST *dl)
{
  int i, type = DF_FLOAT;
  DYN_LIST *list, *newlist;
  DYN_LIST **sublists;

  if (!dl) return(NULL);

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);

  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (DYN_LIST_DATATYPE(sublists[i]) != 
	DYN_LIST_DATATYPE(sublists[0])) return(NULL);
    if (DYN_LIST_N(dl)) type = DYN_LIST_DATATYPE(sublists[i]);
  }

  switch (type) {
  case DF_LIST:
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      if (DYN_LIST_N(sublists[i])) {
	newlist = dynListVarLists(sublists[i]);
      }
      else {
	newlist = dfuCreateDynList(type, 10);
      }
      dfuMoveDynListList(list, newlist);
    }
    break;
  default:
    list = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListFloat(list, dynListVarList(sublists[i]));
    }
    break;
  }
  return(list);
}


DYN_LIST *dynListVarLists(DYN_LIST *dl)
{
  int i, type = DF_FLOAT;
  DYN_LIST *list, *trans, *newlist;
  DYN_LIST **sublists;

  if (!dl) return(NULL);

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);

  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (DYN_LIST_DATATYPE(sublists[i]) != 
	DYN_LIST_DATATYPE(sublists[0])) return(NULL);
    if (DYN_LIST_N(dl)) type = DYN_LIST_DATATYPE(sublists[i]);
  }

  switch (type) {
  case DF_LIST:
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      if (DYN_LIST_N(sublists[i])) {
	trans = dynListTransposeList(sublists[i]);
	newlist = dynListVarLists(trans);
	dfuFreeDynList(trans);
      }
      else {
	newlist = dfuCreateDynList(type, 10);
      }
      dfuMoveDynListList(list, newlist);
    }
    break;
  default:
    list = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListFloat(list, dynListVarList(sublists[i]));
    }
    break;
  }
  return(list);
}

float dynListVarList(DYN_LIST *dl)
{
  int i;
  double var = 0.0;
  
  if (!dl) return(0.0);

  if (DYN_LIST_N(dl) < 2) return(0.0);

  /* From Numerical Recipes 2nd Ed., p. 613 */

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      float mean = dynListMeanList(dl);
      int n = DYN_LIST_N(dl);
      double sdiff  = 0.0;
      double ssdiff = 0.0;
      for (i = 0; i < n; i++) {
	ssdiff += (vals[i]-mean)*(vals[i]-mean);
      }
      for (i = 0; i < n; i++) {
	sdiff += vals[i]-mean;
      }
      var = 1.0/(n-1.0) * (ssdiff - (1.0/n)*(sdiff*sdiff));
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      float mean = dynListMeanList(dl);
      int n = DYN_LIST_N(dl);
      double sdiff  = 0.0;
      double ssdiff = 0.0;
      for (i = 0; i < n; i++) {
	ssdiff += (vals[i]-mean)*(vals[i]-mean);
      }
      for (i = 0; i < n; i++) {
	sdiff += vals[i]-mean;
      }
      var = 1.0/(n-1.0) * (ssdiff - (1.0/n)*(sdiff*sdiff));
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      float mean = dynListMeanList(dl);
      int n = DYN_LIST_N(dl);
      double sdiff  = 0.0;
      double ssdiff = 0.0;
      for (i = 0; i < n; i++) {
	if (!isnan(vals[i])) ssdiff += (vals[i]-mean)*(vals[i]-mean);
      }
      for (i = 0; i < n; i++) {
	if (!isnan(vals[i])) sdiff += vals[i]-mean;
      }
      var = 1.0/(n-1.0) * (ssdiff - (1.0/n)*(sdiff*sdiff));
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      float mean = dynListMeanList(dl);
      int n = DYN_LIST_N(dl);
      double sdiff  = 0.0;
      double ssdiff = 0.0;
      for (i = 0; i < n; i++) {
	ssdiff += (vals[i]-mean)*(vals[i]-mean);
      }
      for (i = 0; i < n; i++) {
	sdiff += vals[i]-mean;
      }
      var = 1.0/(n-1.0) * (ssdiff - (1.0/n)*(sdiff*sdiff));
    }
    break;
  case DF_LIST:
    {
      /* NOT YET IMPLEMENTED */
      /*      
      ** DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      */
    }
    break;
  default:
    return(0.0);
  }
  return(var);
}



float dynListStdList(DYN_LIST *dl)
{
  return(sqrt((double) dynListVarList(dl)));
}


DYN_LIST *dynListHStdLists(DYN_LIST *dl)
{
  int i, type = DF_FLOAT;
  DYN_LIST *list, *newlist;
  DYN_LIST **sublists;

  if (!dl) return(NULL);

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);

  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (DYN_LIST_DATATYPE(sublists[i]) != 
	DYN_LIST_DATATYPE(sublists[0])) return(NULL);
    if (DYN_LIST_N(dl)) type = DYN_LIST_DATATYPE(sublists[i]);
  }

  switch (type) {
  case DF_LIST:
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      if (DYN_LIST_N(sublists[i])) {
	newlist = dynListStdLists(sublists[i]);
      }
      else {
	newlist = dfuCreateDynList(type, 10);
      }
      dfuMoveDynListList(list, newlist);
    }
    break;
  default:
    list = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListFloat(list, dynListStdList(sublists[i]));
    }
    break;
  }
  return(list);
}


DYN_LIST *dynListStdLists(DYN_LIST *dl)
{
  int i, type = DF_FLOAT;
  DYN_LIST *list, *trans, *newlist;
  DYN_LIST **sublists;

  if (!dl) return(NULL);

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);

  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (DYN_LIST_DATATYPE(sublists[i]) != 
	DYN_LIST_DATATYPE(sublists[0])) return(NULL);
    if (DYN_LIST_N(dl)) type = DYN_LIST_DATATYPE(sublists[i]);
  }

  switch (type) {
  case DF_LIST:
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      if (DYN_LIST_N(sublists[i])) {
	trans = dynListTransposeList(sublists[i]);
	newlist = dynListStdLists(trans);
	dfuFreeDynList(trans);
      }
      else {
	newlist = dfuCreateDynList(type, 10);
      }
      dfuMoveDynListList(list, newlist);
    }
    break;
  default:
    list = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListFloat(list, dynListStdList(sublists[i]));
    }
    break;
  }
  return(list);
}


DYN_LIST *dynListListCounts(DYN_LIST *dl, DYN_LIST *range)
{
  int i;
  DYN_LIST *list, *clist;
  DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);

  if (DYN_LIST_DATATYPE(range) != DF_LIST) {
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      /* 
       * Collapse the lowest level so that the counts make up a list of ints
       * and not a list of lists of single counts
       */
      if (dynListDepth(vals[i], 0) == 1) {
	clist = dynListCountLists(vals[i], range);
      }
      else {
	clist = dynListListCount(vals[i], range);
      }
      if (!clist) {
	dfuFreeDynList(list);
	return NULL;
      }
      dfuMoveDynListList(list, clist);
    }
    return(list);
  }
  else {
    int j;
    DYN_LIST *all = dfuCreateDynList(DF_LIST, DYN_LIST_N(range));
    DYN_LIST **ranges = (DYN_LIST **) DYN_LIST_VALS(range);

    for (j = 0; j < DYN_LIST_N(range); j++) {
      list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	/* 
	 * Collapse the lowest level so that the counts make up a list of ints
	 * and not a list of lists of single counts
	 */
	if (dynListDepth(vals[i], 0) == 1) {
	  clist = dynListCountLists(vals[i], ranges[j]);
	}
	else {
	  clist = dynListListCount(vals[i], ranges[j]);
	}
	if (!clist) {
	  dfuFreeDynList(list);
	  return NULL;
	}
	dfuMoveDynListList(list, clist);
      }
      dfuMoveDynListList(all, list);
    }
    return all;
  }
}

DYN_LIST *dynListListCount(DYN_LIST *dl, DYN_LIST *range)
{
  int i;
  DYN_LIST *list = NULL;
  int count = 0;
  
  if (DYN_LIST_N(range) != 2) return 0;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      switch (DYN_LIST_DATATYPE(range)) {
      case DF_LONG:
	{
	  int *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      case DF_FLOAT:
	{
	  float *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      switch (DYN_LIST_DATATYPE(range)) {
      case DF_LONG:
	{
	  int *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      case DF_FLOAT:
	{
	  float *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      switch (DYN_LIST_DATATYPE(range)) {
      case DF_LONG:
	{
	  int *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      case DF_FLOAT:
	{
	  float *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      switch (DYN_LIST_DATATYPE(range)) {
      case DF_LONG:
	{
	  int *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      case DF_FLOAT:
	{
	  float *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST *curlist;
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListListCount(vals[i], range);
	if (!curlist) {
	  dfuFreeDynList(list);
	  return NULL;
	}
	dfuMoveDynListList(list, curlist);
      }
    }
    break;
  default:
    return(NULL);
  }
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LIST:
    break;
  default:
    list = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
    dfuAddDynListLong(list, count);
  }

  return(list);
}


DYN_LIST *dynListCountLists(DYN_LIST *dl, DYN_LIST *range)
{
  int i;
  DYN_LIST *list;
  DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);
  if (DYN_LIST_DATATYPE(range) == DF_LIST) {
    DYN_LIST **ranges = (DYN_LIST **) DYN_LIST_VALS(range);
    if (DYN_LIST_N(range) != DYN_LIST_N(dl)) return NULL;
    list = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListLong(list, dynListCountList(vals[i], ranges[i]));
    }
  }
  else {
    list = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      dfuAddDynListLong(list, dynListCountList(vals[i], range));
    }
  }
  
  return(list);
}


long dynListCountList(DYN_LIST *dl, DYN_LIST *range)
{
  int i;
  int count = 0;
  
  if (DYN_LIST_N(range) != 2) return 0;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      switch (DYN_LIST_DATATYPE(range)) {
      case DF_LONG:
	{
	  int *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      case DF_FLOAT:
	{
	  float *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      switch (DYN_LIST_DATATYPE(range)) {
      case DF_LONG:
	{
	  int *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      case DF_FLOAT:
	{
	  float *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      switch (DYN_LIST_DATATYPE(range)) {
      case DF_LONG:
	{
	  int *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      case DF_FLOAT:
	{
	  float *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      switch (DYN_LIST_DATATYPE(range)) {
      case DF_LONG:
	{
	  int *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      case DF_FLOAT:
	{
	  float *rvals = DYN_LIST_VALS(range);
	  for (i = 0; i < DYN_LIST_N(dl); i++) 
	    count += (vals[i] >= rvals[0] && vals[i] < rvals[1]);
	}
	break;
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) 
	count+=dynListCountList(vals[i], range);
    }
    break;
  default:
    return(0);
  }
  return(count);
}


DYN_LIST *dynListHistLists(DYN_LIST *dl, DYN_LIST *range, int nbins)
{
  int i;
  DYN_LIST *list, *newlist;
  DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);
  if (nbins <= 0) return(NULL);

  list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    newlist = dynListHistList(vals[i], range, nbins);
    dfuMoveDynListList(list, newlist);
  }
  
  return(list);
}

DYN_LIST *dynListHistList(DYN_LIST *dl, DYN_LIST *range, int nbins)
{
  int i,index;
  double bwidth, start, stop;
  DYN_LIST *hist;

  if (nbins <= 0 || !dl || !range) return(NULL);
  
  switch (DYN_LIST_DATATYPE(range)) {
  case DF_LONG: 
    {
      int *vals = (int *) DYN_LIST_VALS(range);
      start = vals[0];
      stop = vals[1];
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(range);
      start = vals[0];
      stop = vals[1];
    }
    break;
  default:
    return NULL;
  }

  hist = dfuCreateDynList(DF_LONG, nbins);

  bwidth = (stop-start)/nbins;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      int *counts = (int *) calloc(nbins, sizeof(int));
      
      for (i = 0; i < DYN_LIST_N(dl); i++)
	{
	  if(vals[i]<start || vals[i]>=stop) continue;
	  index = (vals[i]-start) / bwidth;
	  counts[index]++;
	}      
      for (i = 0; i < nbins; i++) 
	dfuAddDynListLong(hist, counts[i]);
      free(counts);
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      int *counts = (int *) calloc(nbins, sizeof(int));
      
      for (i = 0; i < DYN_LIST_N(dl); i++)
	{
	  if(vals[i]<start || vals[i]>=stop) continue;
	  index = (vals[i]-start) / bwidth;
	  counts[index]++;
	}      

      for (i = 0; i < nbins; i++) 
	dfuAddDynListLong(hist, counts[i]);
      free(counts);
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      int *counts = (int *) calloc(nbins, sizeof(int));
      
      for (i = 0; i < DYN_LIST_N(dl); i++)
	{
	  if(vals[i]<start || vals[i]>=stop) continue;
	  index = (vals[i]-start) / bwidth;
	  counts[index]++;
	}      

      for (i = 0; i < nbins; i++) 
	dfuAddDynListLong(hist, counts[i]);
      free(counts);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      int *counts = (int *) calloc(nbins, sizeof(int));
      
      for (i = 0; i < DYN_LIST_N(dl); i++)
	{
	  if(vals[i]<start || vals[i]>=stop) continue;
	  index = (vals[i]-start) / bwidth;
	  counts[index]++;
	}      
      for (i = 0; i < nbins; i++) 
	dfuAddDynListLong(hist, counts[i]);
      free(counts);
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curhist = NULL;
      DYN_LIST *counts, *newcounts;

      if (!DYN_LIST_N(dl)) {
	return dynListZerosInt(nbins);
      }

      for (i = 0; i < DYN_LIST_N(dl); i++) {
	counts = dynListHistList(vals[i], range, nbins);
	if (i) {
	  newcounts = dynListArithListList(counts, curhist, DL_MATH_ADD);
	  dfuFreeDynList(counts);
	  dfuFreeDynList(curhist);
	  curhist = newcounts;
	}
	else {
	  curhist = counts;
	}
      }
      hist = curhist;
    }
    break;
  default:
    return(NULL);
  }

  return(hist);
}

DYN_LIST *dynListGaussian2D(int w, int h, float sd)
{
  float *vals;
  int length = h*w;
  int i, j;
  float x0, y0, x, y, d;
  float sd_2 = sd*sd;
  vals = calloc(length, sizeof(float));
  if (!vals) return NULL;
  
  if (w%2) x0 = -w/2;
  else x0  = -w/2+0.5;

  if (h%2) y0 = -h/2;
  else y0  = -h/2+0.5;

  for (i = 0, x = x0; i < h; i++, x+=1.) {
    for (j = 0, y = y0; j < w; j++, y+=1.) {
      d = x*x+y*y;
      vals[i*w+j] = exp(-(d/sd_2));
    }
  }
  return dfuCreateDynListWithVals(DF_FLOAT, length, vals);
}

DYN_LIST *dynListSdfFullLists( DYN_LIST *dl, float ksd, float knsd, int resol)
{
  DYN_LIST *newlist, *start, *stop;
  if (!dl) return(NULL);
  
  start = dfuCreateDynList(DF_FLOAT, 1);
  dfuAddDynListFloat(start, dynListMinList(dl));
  stop = dfuCreateDynList(DF_FLOAT, 1);
  dfuAddDynListFloat(stop, dynListMaxList(dl));

  newlist = dynListSdfLists(dl, start, stop, ksd, knsd, resol);
  dfuFreeDynList(start);
  dfuFreeDynList(stop);

  return newlist;
}

DYN_LIST *dynListSdfFullList( DYN_LIST *dl, float ksd, float knsd, int resol)
{
  float start, stop;
  if (!dl) return(NULL);

  start = dynListMinList(dl);
  stop = dynListMaxList(dl);
  return(dynListSdfList(dl, start, stop, ksd, knsd, resol));
}

#ifdef ORIGINAL_SDF_LISTS
DYN_LIST *dynListSdfLists(DYN_LIST *dl, float start, float stop, float ksd,
			  float knsd, int resolution)
{
  int i;
  DYN_LIST *list, *newlist;
  DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);

  list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    newlist = dynListSdfList(vals[i], start, stop, ksd, knsd, resolution);
    if (!newlist) {
      int i;
      int length = ( stop - start) / resolution + 1;
      newlist = dfuCreateDynList(DF_FLOAT, length);
      for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
    }
    dfuMoveDynListList(list, newlist);
  }
  
  return(list);
}
#endif

DYN_LIST *dynListSdfLists(DYN_LIST *dl, DYN_LIST *starts, 
			  DYN_LIST *stops, float ksd,
			  float nsd, int resolution)
{
  int i, i1, i2;
  DYN_LIST *list, *newlist;
  DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  float *fstarts, *fstops;
  int *lstarts, *lstops;
  int repstarts = 0, repstops = 0;

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);
  if ((DYN_LIST_DATATYPE(starts) != DF_LONG &&
       DYN_LIST_DATATYPE(starts) != DF_FLOAT) ||
      (DYN_LIST_DATATYPE(stops) != DF_LONG &&
       DYN_LIST_DATATYPE(stops) != DF_FLOAT)) {
    return(NULL);
  }
  if ((DYN_LIST_N(starts) != 1 && DYN_LIST_N(starts) != DYN_LIST_N(dl)) ||
      (DYN_LIST_N(stops) != 1 && DYN_LIST_N(stops) != DYN_LIST_N(dl))) {
    return(NULL);
  }
  if (DYN_LIST_N(starts) == 1) repstarts = 1;
  if (DYN_LIST_N(stops) == 1) repstops = 1;

  list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
  
  switch(DYN_LIST_DATATYPE(starts)) {
  case DF_FLOAT:
    fstarts = (float *) DYN_LIST_VALS(starts);
    switch(DYN_LIST_DATATYPE(stops)) {
    case DF_FLOAT:
      fstops = (float *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0;  else i1 = i;
	if (repstops) i2 = 0;  else i2 = i;
	newlist = 
	  dynListSdfList(vals[i], fstarts[i1], fstops[i2], 
			 ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (fstops[i2] - fstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DF_LONG:
      lstops = (int *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0; else i1 = i;
	if (repstops) i2 = 0; else i2 = i;
	newlist = 
	  dynListSdfList(vals[i], fstarts[i1], lstops[i2], 
			 ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (lstops[i2] - fstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    }
    break;
  case DF_LONG:
    lstarts = (int *) DYN_LIST_VALS(starts);
    switch(DYN_LIST_DATATYPE(stops)) {
    case DF_FLOAT:
      fstops = (float *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0;  else i1 = i;
	if (repstops) i2 = 0;  else i2 = i;
	newlist = 
	  dynListSdfList(vals[i], lstarts[i1], fstops[i2], 
			 ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (fstops[i2] - lstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DF_LONG:
      lstops = (int *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0;  else i1 = i;
	if (repstops) i2 = 0;  else i2 = i;
	newlist = 
	  dynListSdfList(vals[i], lstarts[i1], lstops[i2], 
			 ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (lstops[i2] - lstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    }
  }
  return(list);
}

DYN_LIST *dynListSdfList(DYN_LIST *dl, float start, float stop, float ksd, 
			 float knsd, int resol)
{
  int i,length;
  DYN_LIST *sdf;
  float *kvals;
  int ksize;

  kvals = sdfMakeKernel (ksd, knsd, &ksize);

  length = ( stop - start) / resol + 1;
  sdf = dfuCreateDynList(DF_FLOAT, length);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int duration = stop-start+1;
      int *vals = (int *) DYN_LIST_VALS(dl);
      float *data = (float *) calloc(DYN_LIST_N(dl),sizeof(float));
      float *counts;      

      for (i = 0; i < DYN_LIST_N(dl); i++) 
	data[i] = vals[i];

      counts = sdfMakeSdf(start, duration, data, DYN_LIST_N(dl), kvals, ksize);
      if (counts) {
	for (i = 0; i < duration; i+= resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(data);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+= resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_SHORT:
    {
      int duration = stop-start+1;
      short *vals = (short *) DYN_LIST_VALS(dl);
      float *data = (float *) calloc(DYN_LIST_N(dl),sizeof(float));
      float *counts;      

      for (i = 0; i < DYN_LIST_N(dl); i++) 
	data[i] = vals[i];

      counts = sdfMakeSdf(start, duration, data, DYN_LIST_N(dl), kvals, ksize);
      if (counts) {
	for (i = 0; i < duration; i+=resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(data);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+=resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_FLOAT:
    {
      int duration = stop-start+1;
      float *vals = (float *) DYN_LIST_VALS(dl);
      float *counts;      

      counts = sdfMakeSdf (start, stop-start, 
			   vals,  DYN_LIST_N(dl), kvals, ksize);
      
      if (counts) {
	for (i = 0; i < duration; i+=resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+=resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_CHAR:
    {
      int duration = stop-start+1;
      char *vals = (char *) DYN_LIST_VALS(dl);
      float *data = (float *) calloc(DYN_LIST_N(dl),sizeof(float));
      float *counts;      

      for (i = 0; i < DYN_LIST_N(dl); i++) 
	data[i] = vals[i];

      counts = sdfMakeSdf(start, duration, data, DYN_LIST_N(dl), kvals, ksize);
      if (counts) {
	for (i = 0; i < duration; i+=resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(data);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+=resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *cursdf = NULL;
      DYN_LIST *counts, *newcounts, *div;

      for (i = 0; i < DYN_LIST_N(dl); i++) {
	counts = dynListSdfList(vals[i], start, stop,  ksd, knsd, resol);
	if (i) {
	  newcounts = dynListArithListList(counts, cursdf, DL_MATH_ADD);
	  dfuFreeDynList(cursdf);
	  dfuFreeDynList(counts);
	  cursdf = newcounts;
	}
	else {
	  cursdf = counts;
	}
      }
      div = dfuCreateDynList(DF_FLOAT, 1);
      dfuAddDynListFloat(div, (float) DYN_LIST_N(dl));
      sdf = dynListArithListList(cursdf, div, DL_MATH_DIV);
      dfuFreeDynList(div);      
      dfuFreeDynList(cursdf);
    }
    break;
  default:
    free(kvals);
    return(NULL);
  }

  free(kvals);
  return(sdf);
}

/* Recurse all the way down and don't do any averaging */
DYN_LIST *dynListSdfListsR(DYN_LIST *dl, DYN_LIST *starts, 
			   DYN_LIST *stops, float ksd,
			   float nsd, int resolution)
{
  int i, i1, i2;
  DYN_LIST *list, *newlist;
  DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  float *fstarts, *fstops;
  int *lstarts, *lstops;
  int repstarts = 0, repstops = 0;

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);
  if ((DYN_LIST_DATATYPE(starts) != DF_LONG &&
       DYN_LIST_DATATYPE(starts) != DF_FLOAT) ||
      (DYN_LIST_DATATYPE(stops) != DF_LONG &&
       DYN_LIST_DATATYPE(stops) != DF_FLOAT)) {
    return(NULL);
  }
  if ((DYN_LIST_N(starts) != 1 && DYN_LIST_N(starts) != DYN_LIST_N(dl)) ||
      (DYN_LIST_N(stops) != 1 && DYN_LIST_N(stops) != DYN_LIST_N(dl))) {
    return(NULL);
  }
  if (DYN_LIST_N(starts) == 1) repstarts = 1;
  if (DYN_LIST_N(stops) == 1) repstops = 1;

  list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
  
  switch(DYN_LIST_DATATYPE(starts)) {
  case DF_FLOAT:
    fstarts = (float *) DYN_LIST_VALS(starts);
    switch(DYN_LIST_DATATYPE(stops)) {
    case DF_FLOAT:
      fstops = (float *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0;  else i1 = i;
	if (repstops) i2 = 0;  else i2 = i;
	newlist = 
	  dynListSdfListR(vals[i], fstarts[i1], fstops[i2], 
			 ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (fstops[i2] - fstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DF_LONG:
      lstops = (int *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0; else i1 = i;
	if (repstops) i2 = 0; else i2 = i;
	newlist = 
	  dynListSdfListR(vals[i], fstarts[i1], lstops[i2], 
			 ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (lstops[i2] - fstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    }
    break;
  case DF_LONG:
    lstarts = (int *) DYN_LIST_VALS(starts);
    switch(DYN_LIST_DATATYPE(stops)) {
    case DF_FLOAT:
      fstops = (float *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0;  else i1 = i;
	if (repstops) i2 = 0;  else i2 = i;
	newlist = 
	  dynListSdfListR(vals[i], lstarts[i1], fstops[i2], 
			 ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (fstops[i2] - lstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DF_LONG:
      lstops = (int *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0;  else i1 = i;
	if (repstops) i2 = 0;  else i2 = i;
	newlist = 
	  dynListSdfListR(vals[i], lstarts[i1], lstops[i2], 
			 ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (lstops[i2] - lstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    }
  }
  return(list);
}

DYN_LIST *dynListSdfListR(DYN_LIST *dl, float start, float stop, float ksd, 
			  float knsd, int resol)
{
  int i,length;
  DYN_LIST *sdf;
  float *kvals;
  int ksize;

  kvals = sdfMakeKernel (ksd, knsd, &ksize);

  length = ( stop - start) / resol + 1;
  sdf = dfuCreateDynList(DF_FLOAT, length);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int duration = stop-start+1;
      int *vals = (int *) DYN_LIST_VALS(dl);
      float *data = (float *) calloc(DYN_LIST_N(dl),sizeof(float));
      float *counts;      

      for (i = 0; i < DYN_LIST_N(dl); i++) 
	data[i] = vals[i];

      counts = sdfMakeSdf(start, duration, data, DYN_LIST_N(dl), kvals, ksize);
      if (counts) {
	for (i = 0; i < duration; i+= resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(data);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+= resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_SHORT:
    {
      int duration = stop-start+1;
      short *vals = (short *) DYN_LIST_VALS(dl);
      float *data = (float *) calloc(DYN_LIST_N(dl),sizeof(float));
      float *counts;      

      for (i = 0; i < DYN_LIST_N(dl); i++) 
	data[i] = vals[i];

      counts = sdfMakeSdf(start, duration, data, DYN_LIST_N(dl), kvals, ksize);
      if (counts) {
	for (i = 0; i < duration; i+=resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(data);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+=resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_FLOAT:
    {
      int duration = stop-start+1;
      float *vals = (float *) DYN_LIST_VALS(dl);
      float *counts;      

      counts = sdfMakeSdf (start, stop-start, 
			   vals,  DYN_LIST_N(dl), kvals, ksize);
      
      if (counts) {
	for (i = 0; i < duration; i+=resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+=resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_CHAR:
    {
      int duration = stop-start+1;
      char *vals = (char *) DYN_LIST_VALS(dl);
      float *data = (float *) calloc(DYN_LIST_N(dl),sizeof(float));
      float *counts;      

      for (i = 0; i < DYN_LIST_N(dl); i++) 
	data[i] = vals[i];

      counts = sdfMakeSdf(start, duration, data, DYN_LIST_N(dl), kvals, ksize);
      if (counts) {
	for (i = 0; i < duration; i+=resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(data);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+=resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *cursdf = NULL;

      sdf = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	cursdf = dynListSdfListR(vals[i], start, stop,  ksd, knsd, resol);
	if (!cursdf) {
	  dfuFreeDynList(sdf);
	  free(kvals);
	  return NULL;
	}
	dfuMoveDynListList(sdf, cursdf);
      }
    }
    break;
  default:
    free(kvals);
    return(NULL);
  }

  free(kvals);
  return(sdf);
}


DYN_LIST *dynListParzenLists(DYN_LIST *dl, DYN_LIST *starts, 
			     DYN_LIST *stops, float ksd,
			     float nsd, int resolution)
{
  int i, i1, i2;
  DYN_LIST *list, *newlist;
  DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  float *fstarts, *fstops;
  int *lstarts, *lstops;
  int repstarts = 0, repstops = 0;

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);
  if ((DYN_LIST_DATATYPE(starts) != DF_LONG &&
       DYN_LIST_DATATYPE(starts) != DF_FLOAT) ||
      (DYN_LIST_DATATYPE(stops) != DF_LONG &&
       DYN_LIST_DATATYPE(stops) != DF_FLOAT)) {
    return(NULL);
  }
  if ((DYN_LIST_N(starts) != 1 && DYN_LIST_N(starts) != DYN_LIST_N(dl)) ||
      (DYN_LIST_N(stops) != 1 && DYN_LIST_N(stops) != DYN_LIST_N(dl))) {
    return(NULL);
  }
  if (DYN_LIST_N(starts) == 1) repstarts = 1;
  if (DYN_LIST_N(stops) == 1) repstops = 1;

  list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
  
  switch(DYN_LIST_DATATYPE(starts)) {
  case DF_FLOAT:
    fstarts = (float *) DYN_LIST_VALS(starts);
    switch(DYN_LIST_DATATYPE(stops)) {
    case DF_FLOAT:
      fstops = (float *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0;  else i1 = i;
	if (repstops) i2 = 0;  else i2 = i;
	newlist = 
	  dynListParzenList(vals[i], fstarts[i1], fstops[i2], 
			    ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (fstops[i2] - fstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DF_LONG:
      lstops = (int *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0; else i1 = i;
	if (repstops) i2 = 0; else i2 = i;
	newlist = 
	  dynListParzenList(vals[i], fstarts[i1], lstops[i2], 
			    ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (lstops[i2] - fstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    }
    break;
  case DF_LONG:
    lstarts = (int *) DYN_LIST_VALS(starts);
    switch(DYN_LIST_DATATYPE(stops)) {
    case DF_FLOAT:
      fstops = (float *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0;  else i1 = i;
	if (repstops) i2 = 0;  else i2 = i;
	newlist = 
	  dynListParzenList(vals[i], lstarts[i1], fstops[i2], 
			    ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (fstops[i2] - lstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    case DF_LONG:
      lstops = (int *) DYN_LIST_VALS(stops);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (repstarts) i1 = 0;  else i1 = i;
	if (repstops) i2 = 0;  else i2 = i;
	newlist = 
	  dynListParzenList(vals[i], lstarts[i1], lstops[i2], 
			    ksd, nsd, resolution);
	if (!newlist) {
	  int i;
	  int length = (lstops[i2] - lstarts[i1]) / resolution + 1;
	  newlist = dfuCreateDynList(DF_FLOAT, length);
	  for (i = 0; i < length; i++) dfuAddDynListFloat(newlist, 0.0);
	}
	dfuMoveDynListList(list, newlist);
      }
      break;
    }
  }
  return(list);
}


DYN_LIST *dynListParzenList(DYN_LIST *dl, float start, float stop, float ksd, 
			    float nsd, int resol)
{
  int i,length;
  DYN_LIST *sdf;

  length = ( stop - start) / resol + 1;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int duration = stop-start+1;
      int *vals = (int *) DYN_LIST_VALS(dl);
      float *data = (float *) calloc(DYN_LIST_N(dl),sizeof(float));
      float *counts;      

      sdf = dfuCreateDynList(DF_FLOAT, length);
      for (i = 0; i < DYN_LIST_N(dl); i++) 
	data[i] = vals[i];

      counts = sdfMakeAdaptiveSdf(start, duration, data, DYN_LIST_N(dl), 
				  ksd, nsd);
      if (counts) {
	for (i = 0; i < duration; i+= resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(data);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+= resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_SHORT:
    {
      int duration = stop-start+1;
      short *vals = (short *) DYN_LIST_VALS(dl);
      float *data = (float *) calloc(DYN_LIST_N(dl),sizeof(float));
      float *counts;      
      sdf = dfuCreateDynList(DF_FLOAT, length);

      for (i = 0; i < DYN_LIST_N(dl); i++) 
	data[i] = vals[i];

      counts = sdfMakeAdaptiveSdf(start, duration, data, DYN_LIST_N(dl), 
				  ksd, nsd);
      if (counts) {
	for (i = 0; i < duration; i+=resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(data);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+=resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_FLOAT:
    {
      int duration = stop-start+1;
      float *vals = (float *) DYN_LIST_VALS(dl);
      float *counts;      
      sdf = dfuCreateDynList(DF_FLOAT, length);

      counts = sdfMakeAdaptiveSdf(start, duration, vals, DYN_LIST_N(dl), 
				  ksd, nsd);
      if (counts) {
	for (i = 0; i < duration; i+=resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+=resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_CHAR:
    {
      int duration = stop-start+1;
      char *vals = (char *) DYN_LIST_VALS(dl);
      float *data = (float *) calloc(DYN_LIST_N(dl),sizeof(float));
      float *counts;      
      sdf = dfuCreateDynList(DF_FLOAT, length);

      for (i = 0; i < DYN_LIST_N(dl); i++) 
	data[i] = vals[i];

      counts = sdfMakeAdaptiveSdf(start, duration, data, DYN_LIST_N(dl), 
				  ksd, nsd);
      if (counts) {
	for (i = 0; i < duration; i+=resol) 
	  dfuAddDynListFloat(sdf, counts[i]);
	free(data);
	free(counts);
      }
      else {
	for (i = 0; i < duration; i+=resol) dfuAddDynListFloat(sdf, 0.0);
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *cursdf = NULL;
      DYN_LIST *counts, *newcounts, *div;

      for (i = 0; i < DYN_LIST_N(dl); i++) {
	counts = dynListParzenList(vals[i], start, stop, ksd, nsd, resol);
	if (i) {
	  newcounts = dynListArithListList(counts, cursdf, DL_MATH_ADD);
	  dfuFreeDynList(cursdf);
	  dfuFreeDynList(counts);
	  cursdf = newcounts;
	}
	else {
	  cursdf = counts;
	}
      }
      div = dfuCreateDynList(DF_FLOAT, 1);
      dfuAddDynListFloat(div, (float) DYN_LIST_N(dl));
      sdf = dynListArithListList(cursdf, div, DL_MATH_DIV);
      dfuFreeDynList(div);      
      dfuFreeDynList(cursdf);
    }
    break;
  default:
    return(NULL);
  }

  return(sdf);
}

DYN_LIST *dynListDiffList(DYN_LIST *dl, int lag)
{
  int i, j, length;
  DYN_LIST *diffs;
  
  if (DYN_LIST_DATATYPE(dl) != DF_LIST)
    length = DYN_LIST_N(dl)-lag;
  else 
    length = DYN_LIST_N(dl);

  if (length <= 0) {
    diffs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), 4);
    return(diffs);
  }
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      int *ds = (int *) calloc(length, sizeof(int));
      int *d = ds;
      for (i = lag, j = 0; i < DYN_LIST_N(dl); i++, j++) {
	*ds++ = vals[i]-vals[j];
      }
      diffs = dfuCreateDynListWithVals(DYN_LIST_DATATYPE(dl), length, d);
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      short *ds = (short *) calloc(length, sizeof(short));
      short *d = ds;
      for (i = lag, j = 0; i < DYN_LIST_N(dl); i++, j++) {
	*ds++ = vals[i]-vals[j];
      }
      diffs = dfuCreateDynListWithVals(DYN_LIST_DATATYPE(dl), length, d);
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      float *ds = (float *) calloc(length, sizeof(float));
      float *d = ds;
      for (i = lag, j = 0; i < DYN_LIST_N(dl); i++, j++) {
	*ds++ = vals[i]-vals[j];
      }
      diffs = dfuCreateDynListWithVals(DYN_LIST_DATATYPE(dl), length, d);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      char *ds = (char *) calloc(length, sizeof(char));
      char *d = ds;
      diffs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), length);
      for (i = lag, j = 0; i < DYN_LIST_N(dl); i++, j++) {
	*ds++ = vals[i]-vals[j];
      }
      diffs = dfuCreateDynListWithVals(DYN_LIST_DATATYPE(dl), length, d);
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curdiff;
      diffs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), length);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curdiff = dynListDiffList(vals[i], lag);
	if (!curdiff) {
	  dfuFreeDynList(diffs);
	  return NULL;
	}
	dfuMoveDynListList(diffs, curdiff);
      }
    }
    break;
  default:
    return(NULL);
  }
  return(diffs);
}


DYN_LIST *dynListIdiffLists(DYN_LIST *dl1, DYN_LIST *dl2, int autocorr)
{
  int i, j, length;
  DYN_LIST *corrs;
  
  if (DYN_LIST_DATATYPE(dl1) != DF_LIST &&
      DYN_LIST_DATATYPE(dl2) != DF_LIST) {
    if (autocorr) length = (DYN_LIST_N(dl1)*DYN_LIST_N(dl1))-DYN_LIST_N(dl1);
    else length = DYN_LIST_N(dl1)*DYN_LIST_N(dl2);
  }
  else length = DYN_LIST_N(dl1);

  /* expand this in the future */
  if (DYN_LIST_DATATYPE(dl1) != DYN_LIST_DATATYPE(dl2)) {
    return NULL;
  }

  switch (DYN_LIST_DATATYPE(dl1)) {
  case DF_LONG:
    {
      int *v1 = (int *) DYN_LIST_VALS(dl1);
      int *v2 = (int *) DYN_LIST_VALS(dl2);
      int *ds = (int *) calloc(length, sizeof(int));
      int *d = ds;

      if (!length) 
	return dfuCreateDynList(DYN_LIST_DATATYPE(dl1), 1);

      if (!autocorr) {
	for (i = 0; i < DYN_LIST_N(dl1); i++) {
	  for (j = 0; j < DYN_LIST_N(dl2); j++) {
	    *ds++ = v1[i]-v2[j];
	  }
	}
      }
      else {
	for (i = 0; i < DYN_LIST_N(dl1); i++) {
	  for (j = 0; j < DYN_LIST_N(dl2); j++) {
	    if (i != j) *ds++ = v1[i]-v2[j];
	  }
	}
      }
      corrs = dfuCreateDynListWithVals(DYN_LIST_DATATYPE(dl1), length, d);
    }
    break;
  case DF_FLOAT:
    {
      float *v1 = (float *) DYN_LIST_VALS(dl1);
      float *v2 = (float *) DYN_LIST_VALS(dl2);
      float *ds = (float *) calloc(length, sizeof(float));
      float *d = ds;

      if (!length) 
	return dfuCreateDynList(DYN_LIST_DATATYPE(dl1), 1);

      if (!autocorr) {
	for (i = 0; i < DYN_LIST_N(dl1); i++) {
	  for (j = 0; j < DYN_LIST_N(dl2); j++) {
	    *ds++ = v1[i]-v2[j];
	  }
	}
      }
      else {
	for (i = 0; i < DYN_LIST_N(dl1); i++) {
	  for (j = 0; j < DYN_LIST_N(dl2); j++) {
	    if (i != j) *ds++ = v1[i]-v2[j];
	  }
	}
      }
      corrs = dfuCreateDynListWithVals(DYN_LIST_DATATYPE(dl1), length, d);
    }
    break;
  case DF_LIST:
    {
      /* Fix this to allow single ref lists */
      DYN_LIST **vals1 = (DYN_LIST **) DYN_LIST_VALS(dl1);
      DYN_LIST **vals2 = (DYN_LIST **) DYN_LIST_VALS(dl2);
      DYN_LIST *curcorr;
      corrs = dfuCreateDynList(DYN_LIST_DATATYPE(dl1), length);
      for (i = 0; i < DYN_LIST_N(dl1); i++) {
	curcorr = dynListIdiffLists(vals1[i], vals2[i], autocorr);
	if (!curcorr) {
	  dfuFreeDynList(corrs);
	  return NULL;
	}
	dfuMoveDynListList(corrs, curcorr);
      }
    }
    break;
  default:
    return(NULL);
  }
  return(corrs);
}

DYN_LIST *dynListGradientList(DYN_LIST *dl)
{
  int i, length;
  DYN_LIST *gradients;
  
  length = DYN_LIST_N(dl);
  if (length <= 0) {
    gradients = dfuCreateDynList(DYN_LIST_DATATYPE(dl), 4);
    return(gradients);
  }
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      int *gs = (int *) calloc(length, sizeof(int));
      int *g = gs;
      
      if (length == 1) {
        *gs = 0;
      } else {
        // First point: forward difference
        *gs++ = vals[1] - vals[0];
        
        // Middle points: central difference
        for (i = 1; i < length - 1; i++) {
          *gs++ = (vals[i+1] - vals[i-1]) / 2;
        }
        
        // Last point: backward difference
        *gs = vals[length-1] - vals[length-2];
      }
      gradients = dfuCreateDynListWithVals(DYN_LIST_DATATYPE(dl), length, g);
    }
    break;
    
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      short *gs = (short *) calloc(length, sizeof(short));
      short *g = gs;
      
      if (length == 1) {
        *gs = 0;
      } else {
        // First point: forward difference
        *gs++ = vals[1] - vals[0];
        
        // Middle points: central difference
        for (i = 1; i < length - 1; i++) {
          *gs++ = (vals[i+1] - vals[i-1]) / 2;
        }
        
        // Last point: backward difference
        *gs = vals[length-1] - vals[length-2];
      }
      gradients = dfuCreateDynListWithVals(DYN_LIST_DATATYPE(dl), length, g);
    }
    break;
    
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      float *gs = (float *) calloc(length, sizeof(float));
      float *g = gs;
      
      if (length == 1) {
        *gs = 0.0f;
      } else {
        // First point: forward difference
        *gs++ = vals[1] - vals[0];
        
        // Middle points: central difference
        for (i = 1; i < length - 1; i++) {
          *gs++ = (vals[i+1] - vals[i-1]) / 2.0f;
        }
        
        // Last point: backward difference
        *gs = vals[length-1] - vals[length-2];
      }
      gradients = dfuCreateDynListWithVals(DYN_LIST_DATATYPE(dl), length, g);
    }
    break;
    
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      char *gs = (char *) calloc(length, sizeof(char));
      char *g = gs;
      
      if (length == 1) {
        *gs = 0;
      } else {
        // First point: forward difference
        *gs++ = vals[1] - vals[0];
        
        // Middle points: central difference
        for (i = 1; i < length - 1; i++) {
          *gs++ = (vals[i+1] - vals[i-1]) / 2;
        }
        
        // Last point: backward difference
        *gs = vals[length-1] - vals[length-2];
      }
      gradients = dfuCreateDynListWithVals(DYN_LIST_DATATYPE(dl), length, g);
    }
    break;
    
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curgrad;
      gradients = dfuCreateDynList(DYN_LIST_DATATYPE(dl), length);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        curgrad = dynListGradientList(vals[i]);
        if (!curgrad) {
          dfuFreeDynList(gradients);
          return NULL;
        }
        dfuMoveDynListList(gradients, curgrad);
      }
    }
    break;
    
  default:
    return(NULL);
  }
  return(gradients);
}

DYN_LIST *dynListZeroCrossingList(DYN_LIST *dl)
{
  int i, length;
  DYN_LIST *zxings;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      length = DYN_LIST_N(dl)-1;
      zxings = dfuCreateDynList(DF_LONG, length);

      dfuAddDynListLong(zxings, 0);

      /* 
       * Test the first (and last positions) by hand, so the
       * main test loop can check the position before and after
       * without checking bounds (this is in case vals[i] == 0)
       */

      if (length > 1) {
	if (vals[0] < 0 && vals[1] > 0) 
	  dfuAddDynListLong(zxings, 1);
	else if (vals[0] > 0 && vals[1] < 0) 
	  dfuAddDynListLong(zxings, -1);
	else
	  dfuAddDynListLong(zxings, 0);
      }
      for (i = 1; i < length; i++) {
	if (vals[i] < 0 && vals[i+1] > 0) 
	  dfuAddDynListLong(zxings, 1);
	else if (vals[i] > 0 && vals[i+1] < 0) 
	  dfuAddDynListLong(zxings, -1);
	else if (vals[i] == 0) {
	  if (vals[i-1] < 0 && vals[i+1] > 0)
	    dfuAddDynListLong(zxings, 1);
	  else if (vals[i-1] > 0 && vals[i+1] < 0)
	    dfuAddDynListLong(zxings, -1);
	  else 
	    dfuAddDynListLong(zxings, 0);
	}
	else 
	  dfuAddDynListLong(zxings, 0);
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      length = DYN_LIST_N(dl)-1;
      zxings = dfuCreateDynList(DF_LONG, length);

      dfuAddDynListLong(zxings, 0);

      if (length > 1) {
	if (vals[0] < 0 && vals[1] > 0) 
	  dfuAddDynListLong(zxings, 1);
	else if (vals[0] > 0 && vals[1] < 0) 
	  dfuAddDynListLong(zxings, -1);
	else
	  dfuAddDynListLong(zxings, 0);
      }
      for (i = 1; i < length; i++) {
	if (vals[i] < 0 && vals[i+1] > 0) 
	  dfuAddDynListLong(zxings, 1);
	else if (vals[i] > 0 && vals[i+1] < 0) 
	  dfuAddDynListLong(zxings, -1);
	else if (vals[i] == 0) {
	  if (vals[i-1] < 0 && vals[i+1] > 0)
	    dfuAddDynListLong(zxings, 1);
	  else if (vals[i-1] > 0 && vals[i+1] < 0)
	    dfuAddDynListLong(zxings, -1);
	  else 
	    dfuAddDynListLong(zxings, 0);
	}
	else 
	  dfuAddDynListLong(zxings, 0);
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      length = DYN_LIST_N(dl)-1;
      zxings = dfuCreateDynList(DF_LONG, length);

      dfuAddDynListLong(zxings, 0);

      if (length > 1) {
	if (vals[0] < 0 && vals[1] > 0) 
	  dfuAddDynListLong(zxings, 1);
	else if (vals[0] > 0 && vals[1] < 0) 
	  dfuAddDynListLong(zxings, -1);
	else
	  dfuAddDynListLong(zxings, 0);
      }
      for (i = 1; i < length; i++) {
	if (vals[i] < 0 && vals[i+1] > 0) 
	  dfuAddDynListLong(zxings, 1);
	else if (vals[i] > 0 && vals[i+1] < 0) 
	  dfuAddDynListLong(zxings, -1);
	else if (vals[i] == 0) {
	  if (vals[i-1] < 0 && vals[i+1] > 0)
	    dfuAddDynListLong(zxings, 1);
	  else if (vals[i-1] > 0 && vals[i+1] < 0)
	    dfuAddDynListLong(zxings, -1);
	  else 
	    dfuAddDynListLong(zxings, 0);
	}
	else 
	  dfuAddDynListLong(zxings, 0);
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curzxing;
      length = DYN_LIST_N(dl);
      zxings = dfuCreateDynList(DF_LIST, length);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curzxing = dynListZeroCrossingList(vals[i]);
	if (!curzxing) {
	  dfuFreeDynList(zxings);
	  return NULL;
	}
	dfuMoveDynListList(zxings, curzxing);
      }
    }
    break;
  default:
    return(NULL);
  }
  return(zxings);
}

DYN_LIST *dynListSelect(DYN_LIST *dl, DYN_LIST *selections)
{
  int i, binary = 1, range_error = 0;
  DYN_LIST *newlist = NULL;

  if (!dl || !selections) return NULL;

  /* This takes care of the list of selections lists possibility */
  if (DYN_LIST_DATATYPE(selections) == DF_LIST) {
    DYN_LIST *curlist;
    DYN_LIST **lvals, **svals;
    int repeat_selections = 0;
    int repeat_data = 0;
    if (DYN_LIST_DATATYPE(dl) != DF_LIST) repeat_data = 2;
    else if (DYN_LIST_N(selections) == 1) repeat_selections = 1;
    else if (DYN_LIST_N(dl) == 1) repeat_data = 1;
    else if (DYN_LIST_N(dl) != DYN_LIST_N(selections)) return NULL;
    newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    lvals = (DYN_LIST **) DYN_LIST_VALS(dl);
    svals = (DYN_LIST **) DYN_LIST_VALS(selections);
    if (repeat_selections) {
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListSelect(lvals[i], svals[0]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    else if (repeat_data == 1) {
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	curlist = dynListSelect(lvals[0], svals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    else if (repeat_data == 2) {
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	curlist = dynListSelect(dl, svals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    else {
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListSelect(lvals[i], svals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    return(newlist);
  }

  switch (DYN_LIST_DATATYPE(selections)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(selections);
      
      /* check out the selections list and make sure it's all nonneg */
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	if (vals[i] < 0) return NULL;
	if (vals[i] >= DYN_LIST_N(dl)) range_error = 1;
	if (vals[i] > 1) binary = 0;
      }
      
      /* 0 / 1 Selections */
      if (binary) {
	newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl)/2);
	if (!newlist) return(NULL);
	if (!DYN_LIST_N(selections)) return newlist;
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  if (vals[(i%DYN_LIST_N(selections))]) {
	    dynListCopyElement(dl, i, newlist);
	  }
	}
	return(newlist);
      }
      
      if (range_error) return NULL;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(selections));
      if (!newlist) return(NULL);
      if (!DYN_LIST_N(selections)) return newlist;
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	dynListCopyElement(dl, vals[i], newlist);
      }
      return(newlist);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(selections);
      
      /* check out the selections list and make sure it's all nonneg */
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	if (vals[i] >= DYN_LIST_N(dl)) range_error = 1;
	if (vals[i] > 1) binary = 0;
      }
      
      if (binary) {
	newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl)/2);
	if (!newlist) return(NULL);
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  if (vals[(i%DYN_LIST_N(selections))]) {
	    dynListCopyElement(dl, i, newlist);
	  }
	}
	return(newlist);
      }
      if (range_error) return NULL;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(selections));
      if (!newlist) return(NULL);
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	dynListCopyElement(dl, vals[i], newlist);
      }
      return(newlist);
    }
    break;
  default:
    /* Allow abnormal selector lists if they are zero length */
    if (!DYN_LIST_N(selections)) {
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), 1);
      return newlist;
    }
    else return NULL;
    break;
  }
}

/*
 * Similar to select, except don't allow binary selections
 */

DYN_LIST *dynListChoose(DYN_LIST *dl, DYN_LIST *selections)
{
  int i, range_error = 0;
  DYN_LIST *newlist = NULL;

  if (!dl || !selections) return NULL;

  /* This takes care of the list of selections lists possibility */
  if (DYN_LIST_DATATYPE(selections) == DF_LIST) {
    DYN_LIST *curlist;
    DYN_LIST **lvals = NULL, **svals;
    int repeat_selections = 0;
    int repeat_data = 0;
    if (DYN_LIST_DATATYPE(dl) != DF_LIST) repeat_data = 2;
    else if (DYN_LIST_N(selections) == 1) repeat_selections = 1;
    else if (DYN_LIST_N(dl) == 1) repeat_data = 1;
    else if (DYN_LIST_N(dl) != DYN_LIST_N(selections)) return NULL;
    newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    if (repeat_data != 2) lvals = (DYN_LIST **) DYN_LIST_VALS(dl);
    svals = (DYN_LIST **) DYN_LIST_VALS(selections);
    if (repeat_selections) {
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListChoose(lvals[i], svals[0]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    else if (repeat_data == 1) {
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	curlist = dynListChoose(lvals[0], svals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    else if (repeat_data == 2) {
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	curlist = dynListChoose(dl, svals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    else {
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListChoose(lvals[i], svals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    return(newlist);
  }

  switch (DYN_LIST_DATATYPE(selections)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(selections);
      
      /* check out the selections list and make sure it's all nonneg */
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	if (vals[i] < 0) return NULL;
	if (vals[i] >= DYN_LIST_N(dl)) range_error = 1;
      }
      
      if (range_error) return NULL;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(selections));
      if (!newlist) return(NULL);
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	dynListCopyElement(dl, vals[i], newlist);
      }
      return(newlist);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(selections);
      
      /* check out the selections list and make sure it's all nonneg */
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	if (vals[i] >= DYN_LIST_N(dl)) range_error = 1;
      }
      
      if (range_error) return NULL;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(selections));
      if (!newlist) return(NULL);
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	dynListCopyElement(dl, vals[i], newlist);
      }
      return(newlist);
    }
    break;
  default:
    return NULL;
    break;
  }
}


DYN_LIST *dynListReplace(DYN_LIST *dl, DYN_LIST *selections, DYN_LIST *r)
{
  int copymode;
  int i;
  DYN_LIST *newlist = NULL;

  if (!dl || !selections || !r) return NULL;

  /* This takes care of the list of selections lists possibility */
  if (DYN_LIST_DATATYPE(selections) == DF_LIST) {
    DYN_LIST *curlist;
    DYN_LIST **lvals, **svals, **rvals, *rlist[1];
    int repeat_selections = 0, repeat_replace = 0;
    if (DYN_LIST_DATATYPE(dl) != DF_LIST) return NULL;

    if (DYN_LIST_N(selections) == 1) repeat_selections = 1;
    else if (DYN_LIST_N(dl) != DYN_LIST_N(selections)) return NULL;

    newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    lvals = (DYN_LIST **) DYN_LIST_VALS(dl);
    svals = (DYN_LIST **) DYN_LIST_VALS(selections);

    if (DYN_LIST_DATATYPE(r) == DF_LIST) {
      rvals = (DYN_LIST **) DYN_LIST_VALS(r);
      if (DYN_LIST_N(r) == 1) repeat_replace = 1;
      else if (DYN_LIST_N(dl) != DYN_LIST_N(r)) return NULL;
    }
    else {
      /* This sets up a  replace_repeat list of repeats to use below */
      rlist[0] = r;
      rvals = rlist;
      repeat_replace = 1;
    }
    if (repeat_replace) {
      if (repeat_selections) {
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  curlist = dynListReplace(lvals[i], svals[0], rvals[0]);
	  if (!curlist) {
	    dfuFreeDynList(newlist);
	    return NULL;
	  }
	  dfuMoveDynListList(newlist, curlist);
	}
      }
      else {
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  curlist = dynListReplace(lvals[i], svals[i], rvals[0]);
	  if (!curlist) {
	    dfuFreeDynList(newlist);
	    return NULL;
	  }
	  dfuMoveDynListList(newlist, curlist);
	}
      }
      return(newlist);
    }
    else {
      if (repeat_selections) {
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  curlist = dynListReplace(lvals[i], svals[0], rvals[i]);
	  if (!curlist) {
	    dfuFreeDynList(newlist);
	    return NULL;
	  }
	  dfuMoveDynListList(newlist, curlist);
	}
      }
      else {
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  curlist = dynListReplace(lvals[i], svals[i], rvals[i]);
	  if (!curlist) {
	    dfuFreeDynList(newlist);
	    return NULL;
	  }
	  dfuMoveDynListList(newlist, curlist);
	}
      }
      return(newlist);
    }
  }

  switch (DYN_LIST_DATATYPE(selections)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(selections);
      
      /* check out the selections list and make sure it's all nonneg */
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	if (vals[i] < 0 || vals[i] > 1) return NULL;
      }
      
      /* Need a 1:1 mapping of input to selections */
      if (DYN_LIST_N(dl) != DYN_LIST_N(selections)) return NULL;
      
      /* Replacements can be singular or 1:1 */
      if (DYN_LIST_N(r) == 1) copymode = 0;
      else if (DYN_LIST_N(r) == DYN_LIST_N(selections)) copymode = 1;
      else return NULL;
      
      /* Easy if replacement datatype is the same as source */
      if (DYN_LIST_DATATYPE(r) == DYN_LIST_DATATYPE(dl)) {
	newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
	if (!newlist) return(NULL);
	
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  if (vals[i]) {
	    if (copymode) dynListCopyElement(r, i, newlist);
	    else dynListCopyElement(r, 0, newlist);
	  }
	  else dynListCopyElement(dl, i, newlist);
	}
	return(newlist);
      }
      else if ((DYN_LIST_DATATYPE(r) == DF_LONG &&
	       DYN_LIST_DATATYPE(dl) == DF_FLOAT) ||
	       (DYN_LIST_DATATYPE(r) == DF_FLOAT &&
	       DYN_LIST_DATATYPE(dl) == DF_LONG)) {
	DYN_LIST *templist;
	
	newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
	if (!newlist) return(NULL);
	
	/* Created a temporary list of converted values */
	templist = dynListConvertList(r, DYN_LIST_DATATYPE(dl));

	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  if (vals[i]) {
	    if (copymode) dynListCopyElement(templist, i, newlist);
	    else dynListCopyElement(templist, 0, newlist);
	  }
	  else dynListCopyElement(dl, i, newlist);
	}
	
	/* Now free the temporary list */
	dfuFreeDynList(templist);
	return(newlist);
      }
      else return NULL;
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(selections);

      /* check out the selections list and make sure it's all nonneg */
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	if (vals[i] > 1) return NULL;
      }
      
      /* Need a 1:1 mapping of input to selections */
      if (DYN_LIST_N(dl) != DYN_LIST_N(selections)) return NULL;

      /* Replacements can be singular or 1:1 */
      if (DYN_LIST_N(r) == 1) copymode = 0;
      else if (DYN_LIST_N(r) == DYN_LIST_N(selections)) copymode = 1;
      else return NULL;

      /* Replacement datatype must be same as source */
      if (DYN_LIST_DATATYPE(r) != DYN_LIST_DATATYPE(dl)) return NULL;

      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      if (!newlist) return(NULL);

      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (vals[i]) {
	  if (copymode) dynListCopyElement(r, i, newlist);
	  else dynListCopyElement(r, 0, newlist);
	}
	else dynListCopyElement(dl, i, newlist);
      }
      return(newlist);
    }
    break;
  default:
    return NULL;
    break;
  }
}

/*
 * This is an indexed based dynListReplace
 */
DYN_LIST *dynListReplaceByIndex(DYN_LIST *dl, DYN_LIST *selections, 
				DYN_LIST *orig_r)
{
  int i, j;
  int copymode = 0;
  int *vals;
  int converted_r = 0;		/* Did we have to convert r? */
  DYN_LIST *newlist = NULL;
  DYN_LIST *r, *sorted_s = NULL, *sorted_s_inds = NULL, *sorted_r = NULL;

  if (!dl || !selections || !orig_r) return NULL;

  if (!DYN_LIST_N(selections))
    return dfuCopyDynList(dl);

  /* Recursively apply function... */
  if ((DYN_LIST_DATATYPE(dl) == DF_LIST &&
       DYN_LIST_DATATYPE(selections) == DF_LIST &&
       DYN_LIST_DATATYPE(orig_r) == DF_LIST)) {
    if (DYN_LIST_N(dl) == DYN_LIST_N(selections) &&
	DYN_LIST_N(dl) == DYN_LIST_N(orig_r)) {
      DYN_LIST *curlist;
      DYN_LIST **lvals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST **svals = (DYN_LIST **) DYN_LIST_VALS(selections);
      DYN_LIST **rvals = (DYN_LIST **) DYN_LIST_VALS(orig_r);

      newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListReplaceByIndex(lvals[i], svals[i], rvals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    else if (DYN_LIST_N(dl) == DYN_LIST_N(selections) &&
	     DYN_LIST_N(orig_r) == 1) {
      DYN_LIST *curlist;
      DYN_LIST **lvals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST **svals = (DYN_LIST **) DYN_LIST_VALS(selections);
      DYN_LIST **rvals = (DYN_LIST **) DYN_LIST_VALS(orig_r);

      newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListReplaceByIndex(lvals[i], svals[i], rvals[0]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    else if (DYN_LIST_N(selections) == 1 &&
	     DYN_LIST_N(orig_r) == DYN_LIST_N(dl)) {
      DYN_LIST *curlist;
      DYN_LIST **lvals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST **svals = (DYN_LIST **) DYN_LIST_VALS(selections);
      DYN_LIST **rvals = (DYN_LIST **) DYN_LIST_VALS(orig_r);

      newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListReplaceByIndex(lvals[i], svals[0], rvals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    else if (DYN_LIST_N(selections) == 1 &&
	     DYN_LIST_N(orig_r) == 1) {
      DYN_LIST *curlist;
      DYN_LIST **lvals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST **svals = (DYN_LIST **) DYN_LIST_VALS(selections);
      DYN_LIST **rvals = (DYN_LIST **) DYN_LIST_VALS(orig_r);

      newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListReplaceByIndex(lvals[i], svals[0], rvals[0]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    return newlist;
  }      


  if (DYN_LIST_DATATYPE(selections) != DF_LONG) return NULL;

  vals = (int *) DYN_LIST_VALS(selections);

  /* check out the selections list and make sure it's all nonneg */
  for (i = 0; i < DYN_LIST_N(selections); i++) {
    if (vals[i] < 0) return NULL;
    if (vals[i] >= DYN_LIST_N(dl)) return NULL;
  }

  if (DYN_LIST_N(orig_r) == 1) copymode = 1;
  else if (DYN_LIST_N(orig_r) == DYN_LIST_N(selections)) copymode = 0;
  else return NULL;

  /* Now ensure the replacements will fit into the original list */
  /* We are willing to put floats into int lists and vice versa  */
  if (DYN_LIST_DATATYPE(dl) == DYN_LIST_DATATYPE(orig_r)) r = orig_r;
  else if ((DYN_LIST_DATATYPE(orig_r) == DF_LONG &&
	    DYN_LIST_DATATYPE(dl) == DF_FLOAT) ||
	   (DYN_LIST_DATATYPE(orig_r) == DF_FLOAT &&
	    DYN_LIST_DATATYPE(dl) == DF_LONG)) {
    r = dynListConvertList(orig_r, DYN_LIST_DATATYPE(dl));
    converted_r = 1;
  }
  else return NULL;


  /* Handle the special conditions of long replacements */
  if (DYN_LIST_DATATYPE(dl) == DF_LONG) {
    int *ivals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
    int *rvals = (int *) DYN_LIST_VALS(r);
    memcpy(ivals, DYN_LIST_VALS(dl), sizeof(int)*DYN_LIST_N(dl));
    if (copymode == 1) {
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	ivals[vals[i]] = rvals[0];
      }
    }
    else {
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	ivals[vals[i]] = rvals[i];
      }
    }
    newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), ivals);
    if (converted_r) dfuFreeDynList(r);
    return(newlist);
  }

  /* Handle the special conditions of int replacements */
  if (DYN_LIST_DATATYPE(dl) == DF_FLOAT) {
    float *fvals = (float *) calloc(DYN_LIST_N(dl), sizeof(float));
    float *rvals = (float *) DYN_LIST_VALS(r);
    memcpy(fvals, DYN_LIST_VALS(dl), sizeof(float)*DYN_LIST_N(dl));
    if (copymode == 1) {
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	fvals[vals[i]] = rvals[0];
      }
    }
    else {
      for (i = 0; i < DYN_LIST_N(selections); i++) {
	fvals[vals[i]] = rvals[i];
      }
    }
    newlist = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), fvals);
    if (converted_r) dfuFreeDynList(r);
    return(newlist);
  }

  /* Now the general case */
  newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
  if (!newlist) {
    if (converted_r) dfuFreeDynList(r);
    return(NULL);
  }
  /*
   *  We first sort the selections list so that it can be scanned
   *  monotonically.  While it's scanned, only the selection for the
   *  last element that matches is used as a replacement (since previous
   *  ones would have be replaced by subsequent replacements anyway).
   */

  if (DYN_LIST_N(selections) > 1) {
    sorted_s = dynListSortList(selections);
    vals = (int *) DYN_LIST_VALS(sorted_s);
  }

  if (copymode == 0) {
    sorted_s_inds = dynListSortListIndices(selections);
    sorted_r = dynListChoose(r, sorted_s_inds);
  }
  for (i = 0, j = 0; i < DYN_LIST_N(dl); i++) {
    if (j < DYN_LIST_N(selections) && i == vals[j]) {
      do {
	j++;
      } while (j < DYN_LIST_N(selections) && i == vals[j]);
      switch (copymode) {
	case 0:   dynListCopyElement(sorted_r, j-1, newlist);    break;
       	case 1:	  dynListCopyElement(r, 0, newlist);      break;
      }
    }
    else {
      dynListCopyElement(dl, i, newlist);
    }
  }

  if (DYN_LIST_N(selections)) dfuFreeDynList(sorted_s);
  if (copymode == 0) {
    dfuFreeDynList(sorted_s_inds);
    dfuFreeDynList(sorted_r);
  }
  if (converted_r) dfuFreeDynList(r);
  return(newlist);
}


/*
 * dynListAnyList  / dynListAllList  - Reduce to single scalar (dl_any / dl_all)
 * dynListAnyLists / dynListAllLists - Reduce per sublist (dl_anys / dl_alls)
 *
 * For nested lists:
 *   - List version: collapses entire structure to single 0/1
 *   - Lists version: returns one 0/1 per sublist
 *
 * Short-circuit evaluation for efficiency:
 *   - any: return 1 as soon as any nonzero found
 *   - all: return 0 as soon as any zero found
 */

/*
 * Helper: test any/all on a flat (non-nested) list
 * Returns: 0 or 1
 */
static int dynListAnyFlat(DYN_LIST *dl)
{
  int i;
  
  if (!dl || DYN_LIST_N(dl) == 0) return 0;
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        if (vals[i]) return 1;
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        if (vals[i]) return 1;
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        if (vals[i] != 0.0f) return 1;
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        if (vals[i]) return 1;
      }
    }
    break;
  default:
    return 0;
  }
  return 0;
}

static int dynListAllFlat(DYN_LIST *dl)
{
  int i;
  
  if (!dl || DYN_LIST_N(dl) == 0) return 1;  /* vacuous truth */
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        if (!vals[i]) return 0;
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        if (!vals[i]) return 0;
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        if (vals[i] == 0.0f) return 0;
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        if (!vals[i]) return 0;
      }
    }
    break;
  default:
    return 0;
  }
  return 1;
}

/*****************************************************************************
 * Scalar reduction versions (dl_any / dl_all)
 * Collapse entire structure to single 0/1 value
 *****************************************************************************/

/*
 * dynListAnyList - returns 1 if any element nonzero, 0 otherwise
 * For nested lists, collapses and tests entire structure
 */
int dynListAnyList(DYN_LIST *dl)
{
  int i;
  
  if (!dl || DYN_LIST_N(dl) == 0) return 0;
  
  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      if (dynListAnyList(vals[i])) return 1;  /* short-circuit */
    }
    return 0;
  }
  
  return dynListAnyFlat(dl);
}

/*
 * dynListAllList - returns 1 if all elements nonzero, 0 otherwise
 * For nested lists, collapses and tests entire structure
 */
int dynListAllList(DYN_LIST *dl)
{
  int i;
  
  if (!dl || DYN_LIST_N(dl) == 0) return 1;  /* vacuous truth */
  
  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      if (!dynListAllList(vals[i])) return 0;  /* short-circuit */
    }
    return 1;
  }
  
  return dynListAllFlat(dl);
}

/*****************************************************************************
 * List reduction versions (dl_anys / dl_alls)
 * Return one 0/1 per sublist, flattened (matching dl_sums behavior)
 *****************************************************************************/

/*
 * dynListAnyLists - returns list with 1 per sublist if any element nonzero
 * For nested lists: returns one result per sublist, flattening single values
 * For flat lists: returns a single-element list
 *
 * Example: {{0 1} {0 0} {1 1}} -> {1 0 1}  (flat, not {{1} {0} {1}})
 */
DYN_LIST *dynListAnyLists(DYN_LIST *dl)
{
  int i;
  DYN_LIST *retlist = NULL;
  
  if (!dl) return NULL;
  
  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    DYN_LIST *curlist;
    int all_scalars = 1;
    
    /* First pass: check if all children are flat lists (will produce scalars) */
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      if (DYN_LIST_DATATYPE(vals[i]) == DF_LIST) {
        all_scalars = 0;
        break;
      }
    }
    
    if (all_scalars) {
      /* All children are flat - return flat DF_LONG list */
      retlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
      if (!retlist) return NULL;
      
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        dfuAddDynListLong(retlist, dynListAnyFlat(vals[i]));
      }
      return retlist;
    }
    else {
      /* Mixed or nested - recurse and build list of lists */
      retlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      if (!retlist) return NULL;
      
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        curlist = dynListAnyLists(vals[i]);
        if (!curlist) {
          dfuFreeDynList(retlist);
          return NULL;
        }
        dfuMoveDynListList(retlist, curlist);
      }
      return retlist;
    }
  }
  
  /* Flat list - return single element result */
  retlist = dfuCreateDynList(DF_LONG, 1);
  if (!retlist) return NULL;
  
  dfuAddDynListLong(retlist, dynListAnyFlat(dl));
  return retlist;
}

/*
 * dynListAllLists - returns list with 1 per sublist if all elements nonzero
 * For nested lists: returns one result per sublist, flattening single values
 * For flat lists: returns a single-element list
 *
 * Example: {{1 1} {1 0} {1 1}} -> {1 0 1}  (flat, not {{1} {0} {1}})
 */
DYN_LIST *dynListAllLists(DYN_LIST *dl)
{
  int i;
  DYN_LIST *retlist = NULL;
  
  if (!dl) return NULL;
  
  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    DYN_LIST *curlist;
    int all_scalars = 1;
    
    /* First pass: check if all children are flat lists (will produce scalars) */
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      if (DYN_LIST_DATATYPE(vals[i]) == DF_LIST) {
        all_scalars = 0;
        break;
      }
    }
    
    if (all_scalars) {
      /* All children are flat - return flat DF_LONG list */
      retlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
      if (!retlist) return NULL;
      
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        dfuAddDynListLong(retlist, dynListAllFlat(vals[i]));
      }
      return retlist;
    }
    else {
      /* Mixed or nested - recurse and build list of lists */
      retlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      if (!retlist) return NULL;
      
      for (i = 0; i < DYN_LIST_N(dl); i++) {
        curlist = dynListAllLists(vals[i]);
        if (!curlist) {
          dfuFreeDynList(retlist);
          return NULL;
        }
        dfuMoveDynListList(retlist, curlist);
      }
      return retlist;
    }
  }
  
  /* Flat list - return single element result */
  retlist = dfuCreateDynList(DF_LONG, 1);
  if (!retlist) return NULL;
  
  dfuAddDynListLong(retlist, dynListAllFlat(dl));
  return retlist;
}

/*
 * dynListWhere - Conditional selection: where(mask, if_true, if_false)
 *
 * For each element, returns if_true[i] when mask[i] is true (nonzero),
 * otherwise returns if_false[i].
 *
 * Like a vectorized ternary operator: mask ? if_true : if_false
 *
 * Arguments:
 *   mask     - boolean/integer list (0 = false, nonzero = true)
 *   if_true  - values to use when mask is true
 *   if_false - values to use when mask is false
 *
 * Broadcasting rules (copymode):
 *   - All three lists same length: element-wise selection
 *   - if_true length 1: broadcast scalar to all true positions
 *   - if_false length 1: broadcast scalar to all false positions
 *   - Both can be length 1 simultaneously
 *
 * Type promotion:
 *   - If if_true and if_false have different numeric types, result is DF_FLOAT
 *   - mask must be DF_LONG or DF_CHAR (integer-like)
 *
 * Recursion:
 *   - If mask is DF_LIST, recurses into sublists
 *   - Supports nested structures with matching shapes
 */

DYN_LIST *dynListWhere(DYN_LIST *mask, DYN_LIST *if_true, DYN_LIST *if_false)
{
  int i;
  int copymode_true = 0;   /* 0=element-wise, 1=broadcast if_true */
  int copymode_false = 0;  /* 0=element-wise, 1=broadcast if_false */
  int length;
  int result_type;
  DYN_LIST *newlist = NULL;

  if (!mask || !if_true || !if_false) return NULL;

  /* Handle empty mask */
  if (DYN_LIST_DATATYPE(mask) != DF_LIST && DYN_LIST_N(mask) == 0) {
    /* Determine result type from if_true/if_false */
    if (DYN_LIST_DATATYPE(if_true) == DF_FLOAT ||
        DYN_LIST_DATATYPE(if_false) == DF_FLOAT) {
      return dfuCreateDynList(DF_FLOAT, 0);
    }
    return dfuCreateDynList(DYN_LIST_DATATYPE(if_true), 0);
  }

  /*
   * Recursive case: mask is a list of lists
   */
  if (DYN_LIST_DATATYPE(mask) == DF_LIST) {
    DYN_LIST **mvals = (DYN_LIST **) DYN_LIST_VALS(mask);
    DYN_LIST **tvals = NULL;
    DYN_LIST **fvals = NULL;
    DYN_LIST *curlist;
    int repeat_true = 0, repeat_false = 0;

    length = DYN_LIST_N(mask);
    newlist = dfuCreateDynList(DF_LIST, length);
    if (!newlist) return NULL;

    /* Determine broadcasting for if_true */
    if (DYN_LIST_DATATYPE(if_true) == DF_LIST) {
      tvals = (DYN_LIST **) DYN_LIST_VALS(if_true);
      if (DYN_LIST_N(if_true) == 1) repeat_true = 1;
      else if (DYN_LIST_N(if_true) != length) {
        dfuFreeDynList(newlist);
        return NULL;
      }
    } else {
      /* if_true is a scalar list - broadcast to all sublists */
      repeat_true = 2;
    }

    /* Determine broadcasting for if_false */
    if (DYN_LIST_DATATYPE(if_false) == DF_LIST) {
      fvals = (DYN_LIST **) DYN_LIST_VALS(if_false);
      if (DYN_LIST_N(if_false) == 1) repeat_false = 1;
      else if (DYN_LIST_N(if_false) != length) {
        dfuFreeDynList(newlist);
        return NULL;
      }
    } else {
      /* if_false is a scalar list - broadcast to all sublists */
      repeat_false = 2;
    }

    /* Recurse into each sublist */
    for (i = 0; i < length; i++) {
      DYN_LIST *t_arg = (repeat_true == 2) ? if_true :
                        (repeat_true == 1) ? tvals[0] : tvals[i];
      DYN_LIST *f_arg = (repeat_false == 2) ? if_false :
                        (repeat_false == 1) ? fvals[0] : fvals[i];

      curlist = dynListWhere(mvals[i], t_arg, f_arg);
      if (!curlist) {
        dfuFreeDynList(newlist);
        return NULL;
      }
      dfuMoveDynListList(newlist, curlist);
    }
    return newlist;
  }

  /*
   * Base case: mask is a flat list (DF_LONG or DF_CHAR)
   */

  /* Validate mask type - must be integer-like */
  if (DYN_LIST_DATATYPE(mask) != DF_LONG && 
      DYN_LIST_DATATYPE(mask) != DF_CHAR) {
    return NULL;
  }

  /* Cannot operate on string data */
  if (DYN_LIST_DATATYPE(if_true) == DF_STRING ||
      DYN_LIST_DATATYPE(if_false) == DF_STRING) {
    return NULL;
  }

  /* if_true/if_false cannot be DF_LIST at this point (mask is flat) */
  if (DYN_LIST_DATATYPE(if_true) == DF_LIST ||
      DYN_LIST_DATATYPE(if_false) == DF_LIST) {
    return NULL;
  }

  length = DYN_LIST_N(mask);

  /* Determine broadcasting for if_true */
  if (DYN_LIST_N(if_true) == 1) {
    copymode_true = 1;
  } else if (DYN_LIST_N(if_true) != length) {
    return NULL;
  }

  /* Determine broadcasting for if_false */
  if (DYN_LIST_N(if_false) == 1) {
    copymode_false = 1;
  } else if (DYN_LIST_N(if_false) != length) {
    return NULL;
  }

  /* Determine result type - promote to float if mixed */
  if (DYN_LIST_DATATYPE(if_true) == DYN_LIST_DATATYPE(if_false)) {
    result_type = DYN_LIST_DATATYPE(if_true);
  } else if (DYN_LIST_DATATYPE(if_true) == DF_FLOAT ||
             DYN_LIST_DATATYPE(if_false) == DF_FLOAT) {
    result_type = DF_FLOAT;
  } else {
    /* Both are integer types (DF_LONG, DF_CHAR) - use DF_LONG */
    result_type = DF_LONG;
  }

  /* Create result list */
  newlist = dfuCreateDynList(result_type, length);
  if (!newlist) return NULL;

  /*
   * Perform element-wise selection based on mask type
   */
  if (DYN_LIST_DATATYPE(mask) == DF_LONG) {
    int *mvals = (int *) DYN_LIST_VALS(mask);

    if (result_type == DYN_LIST_DATATYPE(if_true) &&
        result_type == DYN_LIST_DATATYPE(if_false)) {
      /* Same types - use direct copy */
      for (i = 0; i < length; i++) {
        if (mvals[i]) {
          dynListCopyElement(if_true, copymode_true ? 0 : i, newlist);
        } else {
          dynListCopyElement(if_false, copymode_false ? 0 : i, newlist);
        }
      }
    } else {
      /* Need type conversion - convert both to result_type */
      DYN_LIST *t_conv = (DYN_LIST_DATATYPE(if_true) == result_type) ?
                          if_true : dynListConvertList(if_true, result_type);
      DYN_LIST *f_conv = (DYN_LIST_DATATYPE(if_false) == result_type) ?
                          if_false : dynListConvertList(if_false, result_type);

      if (!t_conv || !f_conv) {
        if (t_conv && t_conv != if_true) dfuFreeDynList(t_conv);
        if (f_conv && f_conv != if_false) dfuFreeDynList(f_conv);
        dfuFreeDynList(newlist);
        return NULL;
      }

      for (i = 0; i < length; i++) {
        if (mvals[i]) {
          dynListCopyElement(t_conv, copymode_true ? 0 : i, newlist);
        } else {
          dynListCopyElement(f_conv, copymode_false ? 0 : i, newlist);
        }
      }

      /* Free converted lists if we created them */
      if (t_conv != if_true) dfuFreeDynList(t_conv);
      if (f_conv != if_false) dfuFreeDynList(f_conv);
    }
  }
  else if (DYN_LIST_DATATYPE(mask) == DF_CHAR) {
    char *mvals = (char *) DYN_LIST_VALS(mask);

    if (result_type == DYN_LIST_DATATYPE(if_true) &&
        result_type == DYN_LIST_DATATYPE(if_false)) {
      for (i = 0; i < length; i++) {
        if (mvals[i]) {
          dynListCopyElement(if_true, copymode_true ? 0 : i, newlist);
        } else {
          dynListCopyElement(if_false, copymode_false ? 0 : i, newlist);
        }
      }
    } else {
      DYN_LIST *t_conv = (DYN_LIST_DATATYPE(if_true) == result_type) ?
                          if_true : dynListConvertList(if_true, result_type);
      DYN_LIST *f_conv = (DYN_LIST_DATATYPE(if_false) == result_type) ?
                          if_false : dynListConvertList(if_false, result_type);

      if (!t_conv || !f_conv) {
        if (t_conv && t_conv != if_true) dfuFreeDynList(t_conv);
        if (f_conv && f_conv != if_false) dfuFreeDynList(f_conv);
        dfuFreeDynList(newlist);
        return NULL;
      }

      for (i = 0; i < length; i++) {
        if (mvals[i]) {
          dynListCopyElement(t_conv, copymode_true ? 0 : i, newlist);
        } else {
          dynListCopyElement(f_conv, copymode_false ? 0 : i, newlist);
        }
      }

      if (t_conv != if_true) dfuFreeDynList(t_conv);
      if (f_conv != if_false) dfuFreeDynList(f_conv);
    }
  }

  return newlist;
}



DYN_LIST *dynListShift(DYN_LIST *dl, int shifter, int mode)
{
  DYN_LIST *newlist = NULL;
  DYN_LIST *curlist;
  DYN_LIST **sublists;
  int i, j, k;

  if (!dl) return NULL;

  if (mode == DL_SUBSHIFT_LEFT || mode == DL_SUBSHIFT_RIGHT) {
    if (DYN_LIST_DATATYPE(dl) != DF_LIST) return NULL;
  }

  newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
  if (!newlist) return(NULL);
  
  if (shifter < 0) {
    switch (mode) {
    case DL_SHIFT_CYCLE:
      do {
	shifter += DYN_LIST_N(dl);
      } while (shifter < 0);
      break;
    case DL_SHIFT_LEFT:       mode = DL_SHIFT_RIGHT;          break;
    case DL_SHIFT_RIGHT:      mode = DL_SHIFT_LEFT;           break;
    case DL_SUBSHIFT_LEFT:    mode = DL_SUBSHIFT_RIGHT;       break;
    case DL_SUBSHIFT_RIGHT:   mode = DL_SUBSHIFT_LEFT;        break;
    }
  }
  if (shifter >= DYN_LIST_N(dl)) {
    switch (mode) {
    case DL_SHIFT_CYCLE:
      do {
	shifter -= DYN_LIST_N(dl);
      } while (shifter >= DYN_LIST_N(dl));
      break;
    case DL_SHIFT_LEFT:
    case DL_SHIFT_RIGHT:
      shifter = DYN_LIST_N(dl);
      break;

    }
  }
  
  switch (mode) {
  case DL_SHIFT_CYCLE:
    for (i = 0, j = DYN_LIST_N(dl)-shifter; i < DYN_LIST_N(dl); i++, j++) {
      dynListCopyElement(dl, j%DYN_LIST_N(dl), newlist);
    }
    break;
  case DL_SHIFT_LEFT:
    for (i = shifter; i < DYN_LIST_N(dl); i++) {
      dynListCopyElement(dl, i, newlist);
    }
    for (i = 0; i < shifter; i++) dynListAddNullElement(newlist);
    break;
  case DL_SHIFT_RIGHT:
    for (i = 0; i < shifter; i++) dynListAddNullElement(newlist);
    for (i = 0, j = shifter; j < DYN_LIST_N(dl); i++, j++) {
      dynListCopyElement(dl, i, newlist);
    }
    break;
  case DL_SUBSHIFT_LEFT:
    sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (k = 0; k < DYN_LIST_N(dl); k++) {
      curlist = dfuCreateDynList(DYN_LIST_DATATYPE(sublists[k]), 
				 DYN_LIST_N(sublists[k]));
      for (i = shifter; i < DYN_LIST_N(sublists[k]); i++) {
	dynListCopyElement(sublists[k], i, curlist);
      }
      for (i = 0; i < shifter && i < DYN_LIST_N(sublists[k]); i++) {
	dynListAddNullElement(curlist);
      }
      dfuMoveDynListList(newlist, curlist);
    }
    break;
  case DL_SUBSHIFT_RIGHT:
    sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (k = 0; k < DYN_LIST_N(dl); k++) {
      curlist = dfuCreateDynList(DYN_LIST_DATATYPE(sublists[k]), 
				 DYN_LIST_N(sublists[k]));
      for (i = 0; i < shifter && i < DYN_LIST_N(sublists[k]); i++) {
	dynListAddNullElement(curlist);
      }
      for (i = 0, j = shifter; j < DYN_LIST_N(sublists[k]); i++, j++) {
	dynListCopyElement(sublists[k], i, curlist);
      }
      dfuMoveDynListList(newlist, curlist);
    }
    break;
  }
  return(newlist);
}


DYN_LIST *dynListBShift(DYN_LIST *dl, int shifter, int mode)
{
  int i;
  DYN_LIST *list;
  DYN_LIST **vals;

  if (!dl) return(NULL);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    return dynListShift(dl, shifter, mode);
  }
  else {
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListBShift(vals[i], shifter, mode);
      dfuMoveDynListList(list, curlist);
    }
  }
  return(list);
}


DYN_LIST *dynListIndices(DYN_LIST *dl)
{
  DYN_LIST *newlist = NULL;
  int i;

  if (!dl) return NULL;
 
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
     int *vals = (int *) DYN_LIST_VALS(dl);
     newlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
     if (!newlist) return(NULL);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (vals[i]) dfuAddDynListLong(newlist, i);
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      newlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
      if (!newlist) return(NULL);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (vals[i]) dfuAddDynListLong(newlist, i);
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      newlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
      if (!newlist) return(NULL);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (vals[i]) dfuAddDynListLong(newlist, i);
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      newlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
      if (!newlist) return(NULL);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (vals[i]) dfuAddDynListLong(newlist, i);
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curlist;
      newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      if (!newlist) return(NULL);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListIndices(vals[i]);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return(NULL);
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
  break;
  default:
    return(NULL);
  }
  
  return(newlist);
}

int dynListDump(DYN_LIST *dl, FILE *stream)
{
  int i;

  if (!dl) return(0);
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (!dynListPrintVal(dl, i, stream)) {
      if (DYN_LIST_DATATYPE(dl) == DF_FLOAT ||
	  DYN_LIST_DATATYPE(dl) == DF_LIST)
	fprintf(stream, "   *   ");
      else fprintf(stream, ".");
    }
    fprintf(stream, "\n");
  }
  return(1);
}

int dynListIsMatrix(DYN_LIST *dl)
{
  int i, size = 0;

  DYN_LIST **vals;

  if (!dl) return(0);
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(0);
  if (DYN_LIST_N(dl) == 0) return(0);

  vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (!i) {
      size = DYN_LIST_N(vals[i]);
      if (!size) return 0;
    }
    if (size != DYN_LIST_N(vals[i])) return 0;
    if (DYN_LIST_DATATYPE(vals[i]) != DF_FLOAT) return 0;
  }
  return(1);
}


int dynListDumpMatrix(DYN_LIST *dl, FILE *stream, char separator)
{
  int i;

  DYN_LIST **vals;

  if (!dl) return(0);
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(0);
  vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    dynListDumpAsRow(vals[i], stream, separator);
  }
  return(1);
}


int dynListDumpMatrixInCols(DYN_LIST *dl, FILE *stream, char separator)
{
  int i, j, maxlength;
  DYN_LIST **vals;

  if (!dl) return(0);
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(0);
  vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  maxlength =  dynListMaxListLength(dl);

  for (i = 0; i < maxlength; i++) {
    for (j = 0; j < DYN_LIST_N(dl); j++) {
      if (j) fprintf(stream, "%c", separator);
      if (!dynListPrintVal(vals[j], i, stream)) {
	if (DYN_LIST_DATATYPE(dl) == DF_FLOAT ||
	    DYN_LIST_DATATYPE(dl) == DF_LIST)
	  fprintf(stream, "    *   ");
	else fprintf(stream, ".");
      }
    }
    fprintf(stream, "\n");
  }
  return(1);
}



int dynListDumpAsRow(DYN_LIST *dl, FILE *stream, char separator)
{
  int i;
  
  if (!dl) return(0);

  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (i) fprintf(stream, "%c", separator);
    if (!dynListPrintVal(dl, i, stream)) {
      if (DYN_LIST_DATATYPE(dl) == DF_FLOAT ||
	  DYN_LIST_DATATYPE(dl) == DF_LIST)
	fprintf(stream, "     *   ");
      else fprintf(stream, ".");
    }
  }
  fprintf(stream, "\n");
  return(1);
}



int dynListSetValLong(DYN_LIST *dl, int i, int val)
{
  if (!dl) return(0);
  if (i < 0 || DYN_LIST_N(dl) <= i) return(0);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      vals[i] = val;
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      vals[i] = val;
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      vals[i] = val;
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      vals[i] = val;
    }
    break;
  case DF_LIST:
  case DF_STRING:
    return(0);
    break;
  }
  return(1);
}



int dynListSetIntVals(DYN_LIST *dl, int val, int n)
{
  int i;

  if (!dl) return(0);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      for (i = 0; i < n; i++) dfuAddDynListLong(dl, val);
    }
    break;
  case DF_SHORT:
    {
      for (i = 0; i < n; i++) dfuAddDynListShort(dl, val);
    }
    break;
  case DF_FLOAT:
    {
      for (i = 0; i < n; i++) dfuAddDynListFloat(dl, val);
    }
    break;
  case DF_CHAR:
    {
      for (i = 0; i < n; i++) dfuAddDynListChar(dl, val);
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      for (i = 0; i < n; i++) dynListSetIntVals(vals[i], val, n);
    }
    break;
  }
  return(n);
}


char *dynListSetFormat(int datatype, char *format)
{
  int type;
  static char old[MAX_FORMAT_STRING];

  /*
   * Here we should test for the validity of the format string!
   */
  switch (datatype) {
  case DF_LONG:    type = FMT_LONG;   break;
  case DF_SHORT:   type = FMT_SHORT;  break;
  case DF_CHAR:    type = FMT_CHAR;   break;
  case DF_LIST:    type = FMT_LIST;   break;
  case DF_FLOAT:   type = FMT_FLOAT;  break;
  case DF_STRING:  type = FMT_STRING;  break;
  default:
    return NULL;
  }
  strcpy(old, DLFormatTable[type]);
  strncpy(DLFormatTable[type], format, MAX_FORMAT_STRING-1);
  return(old);
}

int dynListPrintVal(DYN_LIST *dl, int i, FILE *stream)
{
  if (!dl) return(0);
  if (i < 0 || DYN_LIST_N(dl) <= i) return(0);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      fprintf(stream, DLFormatTable[FMT_LONG], (int) vals[i]);
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      fprintf(stream, DLFormatTable[FMT_SHORT], vals[i]);
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      fprintf(stream, DLFormatTable[FMT_FLOAT], vals[i]);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      fprintf(stream, DLFormatTable[FMT_CHAR], vals[i]);
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      fprintf(stream,DLFormatTable[FMT_LIST], DYN_LIST_DATATYPE(vals[i]), 
	      DYN_LIST_N(vals[i]));
    }
    break;
  case DF_STRING:
    {
      char **vals = (char **) DYN_LIST_VALS(dl);
      fprintf(stream,DLFormatTable[FMT_STRING], vals[i]);
    }
    break;
  }
  return(1);
}


static int dynListLessThanFloat(const void *a, const void *b)
{ 
  const float *x = a;
  const float *y = b;
  if (*x > *y) return 1;
  if (*x == *y) return 0;
  return -1;
}

static int dynListLessThanChar(const void *a, const void *b)
{ 
  const char *x = a;
  const char *y = b;
  return(*x - *y);
}

static int dynListLessThanLong(const void *a, const void *b)
{ 
  const int *x = a;
  const int *y = b;
  return(*x - *y);
}

static int dynListLessThanShort(const void *a, const void *b)
{ 
  const short *x = a;
  const short *y = b;
  return(*x - *y);
}

static int dynListLessThanString(const void *a, const void *b)
{ 
  const char **x = (const char **) a;
  const char **y = (const char **) b;
  return(strcmp(*x,*y));
}


static int dynListLessThanFloatI(const void *a, const void *b)
{ 
  const FLOAT_I *x = a;
  const FLOAT_I *y = b;
  if (x->val > y->val) return 1;
  if (x->val == y->val) return 0;
  return -1;
}

static int dynListLessThanCharI(const void *a, const void *b)
{ 
  const CHAR_I *x = a;
  const CHAR_I *y = b;
  return(x->val - y->val);
}

static int dynListLessThanLongI(const void *a, const void *b)
{ 
  const LONG_I *x = a;
  const LONG_I *y = b;
  return(x->val - y->val);
}

static int dynListLessThanShortI(const void *a, const void *b)
{ 
  const SHORT_I *x = a;
  const SHORT_I *y = b;
  return(x->val - y->val);
}

static int dynListLessThanStringI(const void *a, const void *b)
{ 
  const STRING_I *x = a;
  const STRING_I *y = b;
  return(strcmp(x->val,y->val));
}


DYN_LIST *dynListSortList(DYN_LIST *d1)
{
  DYN_LIST *dl;
  if (!d1) return(NULL);
  
  if (DYN_LIST_DATATYPE(d1) != DF_LIST)
    dl = dfuCopyDynList(d1);	/* Will sort in place */
  else
    dl = dfuCreateDynList(DF_LIST, DYN_LIST_N(d1));

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      qsort(vals, DYN_LIST_N(dl), sizeof(int), dynListLessThanLong);
    }
  break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      qsort(vals, DYN_LIST_N(dl), sizeof(short), dynListLessThanShort);
    }
  break;
  case DF_FLOAT:
    { 
      float *vals = (float *) DYN_LIST_VALS(dl);
      qsort(vals, DYN_LIST_N(dl), sizeof(float), dynListLessThanFloat);
    }
  break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      qsort(vals, DYN_LIST_N(dl), sizeof(char), dynListLessThanChar);
    }
  break;
  case DF_STRING:
    {
      char **vals = (char **) DYN_LIST_VALS(dl);
      qsort(vals, DYN_LIST_N(dl), sizeof(char *), dynListLessThanString);
    }
  break;
  case DF_LIST:
    {
      DYN_LIST *curlist, **vals;
      int i;
      vals = (DYN_LIST **) DYN_LIST_VALS(d1);
      for (i = 0; i < DYN_LIST_N(d1); i++) {
	curlist = dynListSortList(vals[i]);
	if (!curlist) {
	  dfuFreeDynList(dl);
	  return NULL;
	}
	dfuMoveDynListList(dl, curlist);
      }
    }
  break;
  }
  return(dl);
}

DYN_LIST *dynListSortListIndices(DYN_LIST *dl)
{
  DYN_LIST *indices;
  if (!dl) return(NULL);

  if (DYN_LIST_DATATYPE(dl) == DF_LIST) 
    indices = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
  else 
    indices = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
  
  if (!indices) return (NULL);
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int i;
      int *vals = (int *) DYN_LIST_VALS(dl);
      LONG_I *li = (LONG_I *) calloc(DYN_LIST_N(dl), sizeof(LONG_I));
      for (i = 0; i < DYN_LIST_N(dl); i++) { 
	li[i].i = i; 
	li[i].val = vals[i];
      }
      qsort(li, DYN_LIST_N(dl), sizeof(LONG_I), dynListLessThanLongI);
      for (i = 0; i < DYN_LIST_N(dl); i++) { 
	dfuAddDynListLong(indices, li[i].i);
      }
      free((void *) li);
    }
  break;
  case DF_SHORT:
    {
      int i;
      short *vals = (short *) DYN_LIST_VALS(dl);
      SHORT_I *li = (SHORT_I *) calloc(DYN_LIST_N(dl), sizeof(SHORT_I));
      for (i = 0; i < DYN_LIST_N(dl); i++) { 
	li[i].i = i; 
	li[i].val = vals[i];
      }
      qsort(li, DYN_LIST_N(dl), sizeof(SHORT_I), dynListLessThanShortI);
      for (i = 0; i < DYN_LIST_N(dl); i++) { 
	dfuAddDynListLong(indices, li[i].i);
      }
      free((void *) li);
    }
  break;
  case DF_FLOAT:
    {
      int i;
      float *vals = (float *) DYN_LIST_VALS(dl);
      FLOAT_I *li = (FLOAT_I *) calloc(DYN_LIST_N(dl), sizeof(FLOAT_I));
      for (i = 0; i < DYN_LIST_N(dl); i++) { 
	li[i].i = i; 
	li[i].val = vals[i];
      }
      qsort(li, DYN_LIST_N(dl), sizeof(FLOAT_I), dynListLessThanFloatI);
      for (i = 0; i < DYN_LIST_N(dl); i++) { 
	dfuAddDynListLong(indices, li[i].i);
      }
      free((void *) li);
    }
  break;
  case DF_CHAR:
    {
      int i;
      char *vals = (char *) DYN_LIST_VALS(dl);
      CHAR_I *li = (CHAR_I *) calloc(DYN_LIST_N(dl), sizeof(CHAR_I));
      for (i = 0; i < DYN_LIST_N(dl); i++) { 
	li[i].i = i; 
	li[i].val = vals[i];
      }
      qsort(li, DYN_LIST_N(dl), sizeof(CHAR_I), dynListLessThanCharI);
      for (i = 0; i < DYN_LIST_N(dl); i++) { 
	dfuAddDynListLong(indices, li[i].i);
      }
      free((void *) li);
    }
  break;
  case DF_STRING:
    {
      int i;
      char **vals = (char **) DYN_LIST_VALS(dl);
      STRING_I *li = (STRING_I *) calloc(DYN_LIST_N(dl), sizeof(STRING_I));
      for (i = 0; i < DYN_LIST_N(dl); i++) { 
	li[i].i = i; 
	li[i].val = vals[i];
      }
      qsort(li, DYN_LIST_N(dl), sizeof(STRING_I), dynListLessThanStringI);
      for (i = 0; i < DYN_LIST_N(dl); i++) { 
	dfuAddDynListLong(indices, li[i].i);
      }
      free((void *) li);
    }
  break;
  case DF_LIST:
    {
      DYN_LIST *curlist, **vals;
      int i;
      vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListSortListIndices(vals[i]);
	if (!curlist) {
	  dfuFreeDynList(indices);
	  return NULL;
	}
	dfuMoveDynListList(indices, curlist);
      }
    }
  break;
  }
  return(indices);
}


DYN_LIST *dynListBSortList(DYN_LIST *dl)
{
  int i;
  DYN_LIST *list;
  DYN_LIST **vals;

  if (!dl) return(NULL);

  vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    return dynListSortList(dl);
  }
  else {
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListBSortList(vals[i]);
      dfuMoveDynListList(list, curlist);
    }
  }
  return(list);
}

DYN_LIST *dynListBSortListIndices(DYN_LIST *dl)
{
  int i;
  DYN_LIST *list;
  DYN_LIST **vals;

  if (!dl) return(NULL);

  vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    return dynListSortListIndices(dl);
  }
  else {
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListBSortListIndices(vals[i]);
      dfuMoveDynListList(list, curlist);
    }
  }
  return(list);
}


DYN_LIST *dynListReverseList(DYN_LIST *dl)
{
  int i, j;
  DYN_LIST *l;
  if (!dl) return NULL;
  l = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
  if (!l) return NULL;
  for (i = 0, j = DYN_LIST_N(dl)-1; i < DYN_LIST_N(dl); i++,j--) {
    dynListCopyElement(dl, j, l);
  }
  return l;
}

DYN_LIST *dynListReverseAll(DYN_LIST *dl)
{
  int i, j;
  DYN_LIST *l, *r;
  DYN_LIST **vals;
  if (!dl) return NULL;
  l = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
  if (!l) return NULL;
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LIST:
    vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0, j = DYN_LIST_N(dl)-1; i < DYN_LIST_N(dl); i++,j--) {
      r = dynListReverseAll(vals[j]);
      if (!r) {
	dfuFreeDynList(l);
	return NULL;
      }
      dfuMoveDynListList(l, r);
    }
    break;
  default:
    for (i = 0, j = DYN_LIST_N(dl)-1; i < DYN_LIST_N(dl); i++,j--) {
      dynListCopyElement(dl, j, l);
    }
    break;
  }
  return l;
}

DYN_LIST *dynListBReverseList(DYN_LIST *dl)
{
  int i;
  DYN_LIST *list;
  DYN_LIST **vals;

  if (!dl) return(NULL);

  vals = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    return dynListReverseList(dl);
  }
  else {
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListBReverseList(vals[i]);
      dfuMoveDynListList(list, curlist);
    }
  }
  return(list);
}


DYN_LIST *dynListPermuteList(DYN_LIST *l1, DYN_LIST *l2)
{
  int i, *order;
  DYN_LIST *l;
  if (!l1 || !l2) return NULL;
  
  if (DYN_LIST_DATATYPE(l2) != DF_LONG) return NULL;

  l = dfuCreateDynList(DYN_LIST_DATATYPE(l1), DYN_LIST_N(l1));
  
  order = (int *) DYN_LIST_VALS(l2);
  for (i = 0; i < DYN_LIST_N(l2); i++) {
    dynListCopyElement(l1, order[i], l);
  }
  return l;
}


int dynListCompareElements(DYN_LIST *l1, int i1, DYN_LIST *l2, int i2)
{
  if (!l1 || !l2) return(1);
  if (DYN_LIST_DATATYPE(l1) != DYN_LIST_DATATYPE(l2)) return(1);
  if (i1 >= DYN_LIST_N(l1) || i2 >= DYN_LIST_N(l2)) return(1);
  
  switch (DYN_LIST_DATATYPE(l1)) {
  case DF_LONG:
    {
      int *v1 = (int *) DYN_LIST_VALS(l1);
      int *v2 = (int *) DYN_LIST_VALS(l2);
      return(dynListLessThanLong(&v1[i1], &v2[i2]));
    }
    break;
  case DF_SHORT:
    {
      short *v1 = (short *) DYN_LIST_VALS(l1);
      short *v2 = (short *) DYN_LIST_VALS(l2);
      return(dynListLessThanShort(&v1[i1], &v2[i2]));
    }
    break;
  case DF_FLOAT:
    { 
      float *v1 = (float *) DYN_LIST_VALS(l1);
      float *v2 = (float *) DYN_LIST_VALS(l2);
      return(dynListLessThanFloat(&v1[i1], &v2[i2]));
    }
    break;
  case DF_CHAR:
    {
      char *v1 = (char *) DYN_LIST_VALS(l1);
      char *v2 = (char *) DYN_LIST_VALS(l2);
      return(dynListLessThanChar(&v1[i1], &v2[i2]));
    }
    break;
  case DF_STRING:
    {
      char **v1 = (char **) DYN_LIST_VALS(l1);
      char **v2 = (char **) DYN_LIST_VALS(l2);
      return(dynListLessThanString(&v1[i1], &v2[i2]));
    }
    break;
  case DF_LIST:
    {
      int i, status;
      DYN_LIST **v1 = (DYN_LIST **) DYN_LIST_VALS(l1);
      DYN_LIST **v2 = (DYN_LIST **) DYN_LIST_VALS(l2);
      if (DYN_LIST_DATATYPE(v1[i1]) != DYN_LIST_DATATYPE(v2[i2])) return(1);
      if ((status = DYN_LIST_N(v1[i1])-DYN_LIST_N(v2[i2]))) return(status);
      for (i = 0; i < DYN_LIST_N(v1[i1]); i++) {
	if ((status = dynListCompareElements(v1[i1], i, v2[i2], i)))
	  return(status);
      }
      return(0);
    }
    break;
  }
  return(0);
}


/*
 * This function should be done inside uniquelist, it seems?
 */

DYN_LIST *dynListUniqueLists(DYN_LIST *dl)
{
  int i;
  DYN_LIST *list, *newlist;
  DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(NULL);
  
  if (!DYN_LIST_N(dl)) {
    return dfuCreateDynList(DF_LIST, 1);
  }

  list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    newlist = dynListUniqueList(vals[i]);
    if (!newlist) {
      dfuFreeDynList(list);
      return(NULL);
    }
    dfuMoveDynListList(list, newlist);
  }
  
  return(list);
}


static DYN_LIST *dynListDoUniqueNoSortList(DYN_LIST *sortlist)
{
  DYN_LIST *uniques;
  int i;

  if (!sortlist) return(NULL);
  if (DYN_LIST_N(sortlist) == 0)
    return dfuCreateDynList(DYN_LIST_DATATYPE(sortlist), 1);

  if (DYN_LIST_DATATYPE(sortlist) == DF_LIST) 
    return(NULL);
  
  uniques = dfuCreateDynList(DYN_LIST_DATATYPE(sortlist), 
			     DYN_LIST_N(sortlist)/2+1);

  switch (DYN_LIST_DATATYPE(sortlist)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(sortlist);
      dfuAddDynListLong(uniques, vals[0]);
      for (i = 1; i < DYN_LIST_N(sortlist); i++) {
	if (vals[i] != vals[i-1]) dfuAddDynListLong(uniques, vals[i]);
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(sortlist);
      dfuAddDynListShort(uniques, vals[0]);
      for (i = 1; i < DYN_LIST_N(sortlist); i++) {
	if (vals[i] != vals[i-1]) dfuAddDynListShort(uniques, vals[i]);
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(sortlist);
      dfuAddDynListFloat(uniques, vals[0]);
      for (i = 1; i < DYN_LIST_N(sortlist); i++) {
	if (vals[i] != vals[i-1]) dfuAddDynListFloat(uniques, vals[i]);
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(sortlist);
      dfuAddDynListChar(uniques, vals[0]);
      for (i = 1; i < DYN_LIST_N(sortlist); i++) {
	if (vals[i] != vals[i-1]) dfuAddDynListChar(uniques, vals[i]);
      }
    }
    break;
  case DF_STRING:
    {
      char **vals = (char **) DYN_LIST_VALS(sortlist);
      dfuAddDynListString(uniques, vals[0]);
      for (i = 1; i < DYN_LIST_N(sortlist); i++) {
	if (strcmp(vals[i],vals[i-1])) dfuAddDynListString(uniques, vals[i]);
      }
    }
    break;
  }
  return(uniques);
}

DYN_LIST *dynListUniqueList(DYN_LIST *dl)
{
  DYN_LIST *newlist, *d;
  int i;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
  case DF_SHORT:
  case DF_FLOAT:
  case DF_CHAR:
  case DF_STRING:
    {
      DYN_LIST *sortlist, *uniques, *l;
      
      if (!dl) return(NULL);
      if (DYN_LIST_N(dl) == 0) 
	return dfuCreateDynList(DYN_LIST_DATATYPE(dl), 1);
      
      l = dfuCopyDynList(dl);
      sortlist = dynListSortList(l);
      uniques = dynListDoUniqueNoSortList(sortlist);
      
      dfuFreeDynList(sortlist);
      dfuFreeDynList(l);
      
      return(uniques);
    }    
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	d = dynListUniqueList(vals[i]);
	if (d) dfuMoveDynListList(newlist, d);
	else { 
	  dfuFreeDynList(newlist);
	  return NULL;
	}
      }
    }
    break;
  default:
    return(0);
    break;
  }
  return(newlist);
}


DYN_LIST *dynListUniqueNoSortList(DYN_LIST *dl)
{
  DYN_LIST *newlist, *d;
  int i;

  if (DYN_LIST_N(dl) == 0) 
    return dfuCreateDynList(DYN_LIST_DATATYPE(dl), 1);
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
  case DF_SHORT:
  case DF_FLOAT:
  case DF_CHAR:
  case DF_STRING:
    {
      if (!dl) return(NULL);
      return(dynListDoUniqueNoSortList(dl));
    }    
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	d = dynListUniqueNoSortList(vals[i]);
	if (d) dfuMoveDynListList(newlist, d);
	else { 
	  dfuFreeDynList(newlist);
	  return NULL;
	}
      }
    }
    break;
  default:
    return(0);
    break;
  }
  return(newlist);
}




DYN_LIST *dynListRankOrderedList(DYN_LIST *dl)
{
  int i, code = 0;
  DYN_LIST *sortindices, *ranked, *l;
  int *vals, *counts, *ranks, *r;


  if (!dl) return(NULL);
  if (DYN_LIST_N(dl) == 0) {
    l = dfuCreateDynList(DF_LONG, 1);
    return l;
  }
  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST *newsub;
    DYN_LIST *newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      newsub = dynListRankOrderedList(sublists[i]);
      if (!newsub) {
	dfuFreeDynList(newlist);
	return NULL;
      }
      dfuMoveDynListList(newlist, newsub);
    }
    return(newlist);
  }

  l = dfuCopyDynList(dl);
  sortindices = dynListSortListIndices(l);
  dfuFreeDynList(l);
  if (!sortindices) return NULL;
  vals = DYN_LIST_VALS(sortindices);
  
  /* make a list of zeros */
  ranks = (int *) calloc(DYN_LIST_N(dl), sizeof(int));

  /* now recode all data with ints between 0 and n-uniques */
  for (i = 1; i < DYN_LIST_N(dl); i++) {
    if (dynListCompareElements(dl, vals[i], dl, vals[i-1])) code++;
    ranks[vals[i]] = code;
  }

  /* loop through recoded list and replace elements with counts of elts */
  r = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
  counts = (int *) calloc(code+1, sizeof(int));
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    r[i] = (counts[ranks[i]])++;
  }
  free(counts);
  free(ranks);

  dfuFreeDynList(sortindices);
  ranked = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), r);
  return(ranked);
}

DYN_LIST *dynListRecodeListFromList(DYN_LIST *dl, DYN_LIST *dl2)
{
  int i, j, code, comp, firstcode = -1;
  DYN_LIST *sortindices, *recoded, *l, *uniques1, *uniques2;
  int *vals, *recodes, *newcodes;

  if (!dl || !dl2) return(NULL);
  if (DYN_LIST_N(dl) == 0 || DYN_LIST_DATATYPE(dl) == DF_LIST) return(NULL);
  if (DYN_LIST_N(dl2) == 0 || DYN_LIST_DATATYPE(dl2) == DF_LIST) return(NULL);

  /* create newcodes by comparing sorted uniques lists */
  uniques1 = dynListUniqueList(dl);
  uniques2 = dynListUniqueList(dl2);

  if (DYN_LIST_N(uniques2) < DYN_LIST_N(uniques1)) {
  bad_lists:
    dfuFreeDynList(uniques1);
    dfuFreeDynList(uniques2);
    return NULL;
  }

  newcodes = calloc(DYN_LIST_N(uniques1), sizeof(int));
  for (i = 0, j = 0; i < DYN_LIST_N(uniques1); i++ ) {
    comp = dynListCompareElements(uniques1, i, uniques2, j);
    if (!comp) {		/* match */
      if (firstcode < 0) firstcode = j;
      newcodes[i] = j++;
    }
    else if (comp > 0) {
      if (j == DYN_LIST_N(uniques2)) {
	free(newcodes);
	goto bad_lists;
      }
      j++; i--;			/* try again */
    }
    else if (comp < 0) {	/* no match found in list 2 */
	free(newcodes);
	goto bad_lists;
    }
  }

  dfuFreeDynList(uniques1);
  dfuFreeDynList(uniques2);
  
  l = dfuCopyDynList(dl);
  sortindices = dynListSortListIndices(l);
  dfuFreeDynList(l);
  if (!sortindices) return NULL;
  vals = DYN_LIST_VALS(sortindices);
  
  recodes = (int *) calloc(DYN_LIST_N(dl), sizeof(int));

  code = 0;
  recodes[vals[0]] = firstcode;
  for (i = 1; i < DYN_LIST_N(dl); i++) {
    if (dynListCompareElements(dl, vals[i], dl, vals[i-1])) code++;
    recodes[vals[i]] = newcodes[code];
  }
  dfuFreeDynList(sortindices);
  recoded = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), recodes);

  free(newcodes);
  return(recoded);
}


DYN_LIST *dynListRecodeList(DYN_LIST *dl)
{
  int i, code = 0;
  DYN_LIST *sortindices, *ranked, *l;
  int *vals, *ranks;

  if (!dl) return(NULL);
  if (DYN_LIST_N(dl) == 0) {
    return dfuCreateDynList(DYN_LIST_DATATYPE(dl), 2);
  }
  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST *newsub;
    DYN_LIST *newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      newsub = dynListRecodeList(sublists[i]);
      if (!newsub) {
	dfuFreeDynList(newlist);
	return NULL;
      }
      dfuMoveDynListList(newlist, newsub);
    }
    return(newlist);
  }
    

  l = dfuCopyDynList(dl);
  sortindices = dynListSortListIndices(l);
  dfuFreeDynList(l);
  if (!sortindices) return NULL;
  vals = DYN_LIST_VALS(sortindices);
  
  /* make a list of zeros */
  ranks = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
  for (i = 1; i < DYN_LIST_N(dl); i++) {
    if (dynListCompareElements(dl, vals[i], dl, vals[i-1])) code++;
    ranks[vals[i]] = code;
  }
  dfuFreeDynList(sortindices);
  ranked = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), ranks);
  return(ranked);
}

DYN_LIST *dynListRecodeWithTiesList(DYN_LIST *dl) {
  int i, k, j, nties;
  DYN_LIST *sortindices, *ranked, *l;
  int *vals;
  float *ranks, code;

  if (!dl) return(NULL);
  if (DYN_LIST_N(dl) == 0) {
    return dfuCreateDynList(DYN_LIST_DATATYPE(dl), 2);
  }
  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST *newsub;
    DYN_LIST *newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      newsub = dynListRecodeWithTiesList(sublists[i]);
      if (!newsub) {
	dfuFreeDynList(newlist);
	return NULL;
      }
      dfuMoveDynListList(newlist, newsub);
    }
    return(newlist);
  }

  l = dfuCopyDynList(dl);
  sortindices = dynListSortListIndices(l);
  dfuFreeDynList(l);
  if (!sortindices) return NULL;
  vals = DYN_LIST_VALS(sortindices);

  /* make a list of zeros.  the first element must be zero anyway */
  ranks = (float *) calloc(DYN_LIST_N(dl), sizeof(float));

  k = 0; /* we need to start by dealing with first element */
  for (i = 1; i < DYN_LIST_N(dl); i++) {
    /* check to see if we are at the end of the list, then if previous element is different */
    while (i < DYN_LIST_N(dl) && !dynListCompareElements(dl, vals[i], dl, vals[i-1])) {
      /* current 'i'th is the same as last element */
      i++;
    }

    /* now we know that elements 'k' (anchor) through 'i'-1 are the same */
    nties = i - k;
    code = k + (nties-1)/2.;

    for (j=k; j<i; j++) {
      ranks[vals[j]] = code;
    }

    k = i; /* setup for next time through */
  }

  if (i == DYN_LIST_N(dl) && k == i-1) {
    ranks[vals[DYN_LIST_N(dl)-1]] = DYN_LIST_N(dl)-1;
  }

  dfuFreeDynList(sortindices);
  ranked = dfuCreateDynListWithVals(DF_FLOAT, DYN_LIST_N(dl), ranks);
  return(ranked);
}


DYN_LIST *dynListCutList(DYN_LIST *dl, DYN_LIST *breaks)
{
  int i, code = 0;
  DYN_LIST *sortindices, *binned, *l;
  int *sortvals, *bins, curcut = 0;
  float *f_breaks, *f_vals, f_curval;
  int *i_breaks, *i_vals, i_curval;
  
  if (!dl) return(NULL);
  if (DYN_LIST_N(dl) == 0) {
    return dfuCreateDynList(DYN_LIST_DATATYPE(dl), 2);
  }
  if (!DYN_LIST_N(breaks)) return NULL;
  
  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST *newsub;
    DYN_LIST *newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    DYN_LIST **sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      newsub = dynListCutList(sublists[i], breaks);
      if (!newsub) {
	dfuFreeDynList(newlist);
	return NULL;
      }
      dfuMoveDynListList(newlist, newsub);
    }
    return(newlist);
  }
    
  if (DYN_LIST_DATATYPE(dl) == DF_FLOAT &&
      DYN_LIST_DATATYPE(breaks) == DF_FLOAT) {
    f_vals = (float *) DYN_LIST_VALS(dl);
    f_breaks = (float *) DYN_LIST_VALS(breaks);
    
    l = dfuCopyDynList(dl);
    sortindices = dynListSortListIndices(l);
    dfuFreeDynList(l);
    if (!sortindices) return NULL;
    sortvals = (int *) DYN_LIST_VALS(sortindices);
    
    /* make a list of zeros */
    bins = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      /* if we're out of break intervals, set to -1 */
      if (curcut == DYN_LIST_N(breaks)) {
	bins[sortvals[i]] = -1;
      }
      else {
	f_curval = f_vals[sortvals[i]];
	/* if we're on the last break point */
	if (curcut == DYN_LIST_N(breaks)-1) {
	  if (curcut == 0) {
	    bins[sortvals[i]] = -1;
	  }
	  else if (f_curval > f_breaks[curcut-1] &&
		   f_curval <= f_breaks[curcut]) {
	    bins[sortvals[i]] = curcut;
	  }
	  else {
	    bins[sortvals[i]] = -1;
	    curcut++;
	  }
	}
	else {
	  if (f_curval > f_breaks[curcut] &&
	      f_curval <= f_breaks[curcut+1]) {
	    bins[sortvals[i]] = curcut;
	  }
	  else if (f_curval > f_breaks[curcut]) {
	    /* bump the current cut and try again */
	    curcut++;
	    i--;
	  }
	  else {
	    bins[sortvals[i]] = -1;
	  }
	}
      }
    }
  }
  else if (DYN_LIST_DATATYPE(dl) == DF_LONG &&
	   DYN_LIST_DATATYPE(breaks) == DF_LONG) {
    i_vals = (int *) DYN_LIST_VALS(dl);
    i_breaks = (int *) DYN_LIST_VALS(breaks);
    
    l = dfuCopyDynList(dl);
    sortindices = dynListSortListIndices(l);
    dfuFreeDynList(l);
    if (!sortindices) return NULL;
    sortvals = (int *) DYN_LIST_VALS(sortindices);
    
    /* make a list of zeros */
    bins = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      /* if we're out of break intervals, set to -1 */
      if (curcut == DYN_LIST_N(breaks)) {
	bins[sortvals[i]] = -1;
      }
      else {
	i_curval = i_vals[sortvals[i]];
	/* if we're on the last break point */
	if (curcut == DYN_LIST_N(breaks)-1) {
	  if (curcut == 0) {
	    bins[sortvals[i]] = -1;
	  }
	  else if (i_curval > i_breaks[curcut-1] &&
		   i_curval <= i_breaks[curcut]) {
	    bins[sortvals[i]] = curcut;
	  }
	  else {
	    bins[sortvals[i]] = -1;
	    curcut++;
	  }
	}
	else {
	  if (i_curval > i_breaks[curcut] &&
	      i_curval <= i_breaks[curcut+1]) {
	    bins[sortvals[i]] = curcut;
	  }
	  else if (i_curval > i_breaks[curcut]) {
	    /* bump the current cut and try again */
	    curcut++;
	    i--;
	  }
	  else {
	    bins[sortvals[i]] = -1;
	  }
	}
      }
    }
  }
  else if (DYN_LIST_DATATYPE(dl) == DF_FLOAT &&
	   DYN_LIST_DATATYPE(breaks) == DF_LONG) {
    f_vals = (float *) DYN_LIST_VALS(dl);
    i_breaks = (int *) DYN_LIST_VALS(breaks);
    
    l = dfuCopyDynList(dl);
    sortindices = dynListSortListIndices(l);
    dfuFreeDynList(l);
    if (!sortindices) return NULL;
    sortvals = (int *) DYN_LIST_VALS(sortindices);
    
    /* make a list of zeros */
    bins = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      /* if we're out of break intervals, set to -1 */
      if (curcut == DYN_LIST_N(breaks)) {
	bins[sortvals[i]] = -1;
      }
      else {
	f_curval = f_vals[sortvals[i]];
	/* if we're on the last break point */
	if (curcut == DYN_LIST_N(breaks)-1) {
	  if (curcut == 0) {
	    bins[sortvals[i]] = -1;
	  }
	  else if (f_curval > i_breaks[curcut-1] &&
		   f_curval <= i_breaks[curcut]) {
	    bins[sortvals[i]] = curcut;
	  }
	  else {
	    bins[sortvals[i]] = -1;
	    curcut++;
	  }
	}
	else {
	  if (f_curval > i_breaks[curcut] &&
	      f_curval <= i_breaks[curcut+1]) {
	    bins[sortvals[i]] = curcut;
	  }
	  else if (f_curval > i_breaks[curcut]) {
	    /* bump the current cut and try again */
	    curcut++;
	    i--;
	  }
	  else {
	    bins[sortvals[i]] = -1;
	  }
	}
      }
    }
  }  
  else if (DYN_LIST_DATATYPE(dl) == DF_LONG &&
	   DYN_LIST_DATATYPE(breaks) == DF_FLOAT) {
    i_vals = (int *) DYN_LIST_VALS(dl);
    f_breaks = (float *) DYN_LIST_VALS(breaks);
    
    l = dfuCopyDynList(dl);
    sortindices = dynListSortListIndices(l);
    dfuFreeDynList(l);
    if (!sortindices) return NULL;
    sortvals = (int *) DYN_LIST_VALS(sortindices);
    
    /* make a list of zeros */
    bins = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      /* if we're out of break intervals, set to -1 */
      if (curcut == DYN_LIST_N(breaks)) {
	bins[sortvals[i]] = -1;
      }
      else {
	i_curval = i_vals[sortvals[i]];
	/* if we're on the last break point */
	if (curcut == DYN_LIST_N(breaks)-1) {
	  if (curcut == 0) {
	    bins[sortvals[i]] = -1;
	  }
	  else if (i_curval > f_breaks[curcut-1] &&
		   i_curval <= f_breaks[curcut]) {
	    bins[sortvals[i]] = curcut;
	  }
	  else {
	    bins[sortvals[i]] = -1;
	    curcut++;
	  }
	}
	else {
	  if (i_curval > f_breaks[curcut] &&
	      i_curval <= f_breaks[curcut+1]) {
	    bins[sortvals[i]] = curcut;
	  }
	  else if (i_curval > f_breaks[curcut]) {
	    /* bump the current cut and try again */
	    curcut++;
	    i--;
	  }
	  else {
	    bins[sortvals[i]] = -1;
	  }
	}
      }
    }
  }  
  else {
    return NULL;
  }
  
  dfuFreeDynList(sortindices);
  binned = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), bins);
  return(binned);
}


DYN_LIST *dynListSortListByList(DYN_LIST *data, DYN_LIST *categories)
{
  int i, j;
  DYN_LIST *uniques, *sorted, *newlist;
  
  if (!data || !categories) return(NULL);
  if (!DYN_LIST_N(data) || !DYN_LIST_N(categories)) return(NULL);
  if (DYN_LIST_DATATYPE(categories) == DF_LIST) return(NULL);
  
  uniques = dynListUniqueList(categories);
  if (!uniques) return NULL;
  sorted = dfuCreateDynList(DF_LIST, DYN_LIST_N(uniques));
  newlist = dfuCreateDynList(DYN_LIST_DATATYPE(data), 10);

  for (i = 0; i < DYN_LIST_N(uniques); i++) {
    dfuResetDynList(newlist);
    for (j = 0; j < DYN_LIST_N(data); j++) {
      if (!dynListCompareElements(categories, j, uniques, i)) {
	dynListCopyElement(data, j, newlist);
      }
    }
    dfuAddDynListList(sorted, newlist);
  }
  
  dfuFreeDynList(newlist);
  dfuFreeDynList(uniques);
  
  return(sorted);
}


DYN_LIST *dynListSortListByLists(DYN_LIST *data, DYN_LIST *categories, 
				 DYN_LIST *selections)
{
  int i, j, k, ntypes, match;
  DYN_LIST *uniques, *sorted, **newlists, *list;
  DYN_LIST **catlists, **uniqlists, **selectlists;
  
  if (!data || !categories || !selections) return(NULL);

  if (DYN_LIST_DATATYPE(categories) != DF_LIST) return(NULL);
  catlists = (DYN_LIST **) DYN_LIST_VALS(categories);
  for (i = 0; i < DYN_LIST_N(categories); i++) {
    if (!catlists[i] || DYN_LIST_N(catlists[i]) != DYN_LIST_N(data)) 
      return(NULL);
  }
  if (DYN_LIST_DATATYPE(selections) != DF_LIST) return(NULL);
  selectlists = (DYN_LIST **) DYN_LIST_VALS(selections);
  for (i = 0; i < DYN_LIST_N(selections); i++) {
    if (!selectlists[i] || 
	DYN_LIST_DATATYPE(selectlists[i]) != DYN_LIST_DATATYPE(catlists[i]))
      return(NULL);
  }

  uniques  = dynListUniqueCrossLists(selections);
  if (!uniques) return NULL;
  uniqlists = (DYN_LIST **) DYN_LIST_VALS(uniques);

  if (!uniqlists || !DYN_LIST_N(uniqlists[0])) {
    dfuFreeDynList(uniques);
    return NULL;
  }

  ntypes   = DYN_LIST_N(uniqlists[0]);
  sorted   = dfuCreateDynList(DF_LIST, DYN_LIST_N(uniques));
  
  for (i = 0; i < ntypes; i++) {
    list = dfuCreateDynList(DYN_LIST_DATATYPE(data), 10);
    dfuMoveDynListList(sorted, list);
  }
  newlists = (DYN_LIST **) DYN_LIST_VALS(sorted);

  for (i = 0; i < DYN_LIST_N(data); i++) {
    for (j = 0, match = 0; j < ntypes && !match; j++) {
      for (k = 0, match = 1; k < DYN_LIST_N(categories) && match; k++) {
	if (dynListCompareElements(catlists[k], i, uniqlists[k], j)) {
	  match = 0;
	}
      }
      if (match) dynListCopyElement(data, i, newlists[j]);
    }
  }
  
  dfuFreeDynList(uniques);
  
  return(sorted);
}

DYN_LIST *dynListGroupListByLists(DYN_LIST *data, DYN_LIST *categories)
{
  int i, j, k, ntypes, match;
  DYN_LIST *uniques, *sorted, **newlists, *list;
  DYN_LIST **catlists, **uniqlists;
  
  if (!data || !categories) return(NULL);

  if (DYN_LIST_DATATYPE(categories) != DF_LIST) return(NULL);
  catlists = (DYN_LIST **) DYN_LIST_VALS(categories);
  for (i = 0; i < DYN_LIST_N(categories); i++) {
    if (DYN_LIST_N(catlists[i]) != DYN_LIST_N(data)) return(NULL);
  }
  
  uniques  = dynListUniqueCrossLists(categories);
  uniqlists = (DYN_LIST **) DYN_LIST_VALS(uniques);

  if (!uniqlists || !DYN_LIST_N(uniqlists[0])) {
    dfuFreeDynList(uniques);
    return NULL;
  }

  ntypes   = DYN_LIST_N(uniqlists[0]);
  sorted   = dfuCreateDynList(DF_LIST, DYN_LIST_N(uniques));
  
  for (i = 0; i < ntypes; i++) {
    list = dfuCreateDynList(DYN_LIST_DATATYPE(data), 10);
    dfuMoveDynListList(sorted, list);
  }
  newlists = (DYN_LIST **) DYN_LIST_VALS(sorted);

  for (i = 0; i < DYN_LIST_N(data); i++) {
    for (j = 0, match = 0; j < ntypes && !match; j++) {
      for (k = 0, match = 1; k < DYN_LIST_N(categories) && match; k++) {
	if (dynListCompareElements(catlists[k], i, uniqlists[k], j)) {
	  match = 0;
	}
      }
      if (match) dynListCopyElement(data, i, newlists[j]);
    }
  }
  
  dfuFreeDynList(uniques);
  
  return(sorted);
}



DYN_LIST *dynListUniqueCrossLists(DYN_LIST *categories)
{
  DYN_LIST *uniques, *uniqs;

  if (!categories || DYN_LIST_DATATYPE(categories) != DF_LIST) return(NULL);

  uniqs   = dynListUniqueLists(categories);
  if (!uniqs) return(NULL);
  uniques = dynListCrossLists(uniqs);
  dfuFreeDynList(uniqs);

  return(uniques);
}


DYN_LIST *dynListCrossLists(DYN_LIST *lists)
{
  int i, j, k, l, prod = 1, total;
  int *vals, *ls;
  DYN_LIST *combined, *newlist, **sublists, *lengths, *products;
  
  if (!lists) return(NULL);
  if (!DYN_LIST_N(lists)) return(NULL);
  if (DYN_LIST_DATATYPE(lists) != DF_LIST) return(NULL);
  sublists = (DYN_LIST **) DYN_LIST_VALS(lists);

  combined = dfuCreateDynList(DF_LIST, 20);

  lengths = dynListListLengths(lists);
  products = dfuCreateDynList(DF_LONG, 20);

  ls = (int *) DYN_LIST_VALS(lengths);
  for (i = 1, prod = 1; i <= DYN_LIST_N(lengths); i++) {
    dfuAddDynListLong(products, prod);
    prod *= ls[i-1];
  }
  dfuAddDynListLong(products, prod);
  total = prod;

  vals = (int *) DYN_LIST_VALS(products);

  for (i = 0; i < DYN_LIST_N(lists); i++) {
    newlist = dfuCreateDynList(DYN_LIST_DATATYPE(sublists[i]), 20);

    /* number of times to repeat the series */
    for (j = 0; j < total / (ls[i]*vals[i]); j++) {

      /* number of elements to repeat */
      for (k = 0; k < DYN_LIST_N(sublists[i]); k++) {

	/* number of times to repeat each element */
	for (l = 0; l < vals[i]; l++) {
	  dynListCopyElement(sublists[i], k, newlist);
	}
      }
    }
    dfuMoveDynListList(combined, newlist);
  }
  
  dfuFreeDynList(lengths);
  dfuFreeDynList(products);
  return(combined);
}


DYN_LIST *dynListTransposeList(DYN_LIST *dl)
{
  int i, j, maxlength, added_null_list, datatype;
  DYN_LIST *t, *newlist, **sublists, **tsublists;

  if (!dl || DYN_LIST_DATATYPE(dl) != DF_LIST || !DYN_LIST_N(dl)) return(NULL);
  maxlength = dynListMaxListLength(dl);
  t = dfuCreateDynList(DF_LIST, maxlength);
  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  /*
   * Transpose requires that sublists are all of the same type
   * so we just use sublists[0]
   */
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (!sublists[i] ||
	(DYN_LIST_DATATYPE(sublists[i]) != 
	 DYN_LIST_DATATYPE(sublists[0]))) return(NULL);
  }

  newlist = dfuCreateDynList(DYN_LIST_DATATYPE(sublists[0]), DYN_LIST_N(dl));
  for (i = 0; i < maxlength; i++) {
    dfuResetDynList(newlist);
    added_null_list = 0;
    datatype = DF_LONG;
    for (j = 0; j < DYN_LIST_N(dl); j++) {
      if (!dynListCopyElement(sublists[j], i, newlist)) {
	dynListAddNullElement(newlist);
	if (DYN_LIST_DATATYPE(newlist) == DF_LIST)
	  added_null_list = 1;
      }
    }
    /* try to keep any null lists of same type */
    if (DYN_LIST_DATATYPE(newlist) == DF_LIST &&
	added_null_list) {
      tsublists = (DYN_LIST **) DYN_LIST_VALS(newlist);
      for (j = 0; j < DYN_LIST_N(newlist); j++) {
	if (DYN_LIST_N(tsublists[j]) > 0) {
	  datatype = DYN_LIST_DATATYPE(tsublists[j]);
	  break;
	}
      }
      for (j = 0; j < DYN_LIST_N(newlist); j++) {
	if (DYN_LIST_N(tsublists[j]) == 0) {
	  dfuFreeDynList(tsublists[j]);
	  tsublists[j] = dfuCreateDynList(datatype, 1);
	}
      }
    }
    dfuAddDynListList(t, newlist);
  }
  dfuFreeDynList(newlist);

  return(t);
}

DYN_LIST *dynListTransposeListAt(DYN_LIST *dl, int level)
{
  int i;
  DYN_LIST **sublists;
  DYN_LIST *curlist, *retlist;
  
  if (level < 0) return NULL;
  
  if (level == 0) return dynListTransposeList(dl);

  if (dynListDepth(dl, 0) < (level-1)) return NULL;
  
  retlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    curlist = dynListTransposeListAt(sublists[i], level-1);
    if (!curlist) {
      dfuFreeDynList(retlist);
      return NULL;
    }
    dfuMoveDynListList(retlist, curlist);
  }
  return retlist;
}

int dynListCopyElement(DYN_LIST *source, int i, DYN_LIST *dest)
{
  if (DYN_LIST_DATATYPE(source) != DYN_LIST_DATATYPE(dest)) return(0);
  if (i >= DYN_LIST_N(source)) return(0);
  
  switch (DYN_LIST_DATATYPE(source)) {
  case DF_LONG:
    {
      int *v1 = (int *) DYN_LIST_VALS(source);
      dfuAddDynListLong(dest, v1[i]);
    }
    break;
  case DF_SHORT:
    {
      short *v1 = (short *) DYN_LIST_VALS(source);
      dfuAddDynListShort(dest, v1[i]);
    }
    break;
  case DF_FLOAT:
    { 
      float *v1 = (float *) DYN_LIST_VALS(source);
      dfuAddDynListFloat(dest, v1[i]);
    }
    break;
  case DF_CHAR:
    {
      char *v1 = (char *) DYN_LIST_VALS(source);
      dfuAddDynListChar(dest, v1[i]);
    }
    break;
  case DF_STRING:
    {
      char **v1 = (char **) DYN_LIST_VALS(source);
      dfuAddDynListString(dest, v1[i]);
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **v1 = (DYN_LIST **) DYN_LIST_VALS(source);
      dfuAddDynListList(dest, v1[i]);
    }
    break;
  }
  return(1);
}

DYN_LIST *dynListElementList(DYN_LIST *source, int i)
{
  DYN_LIST **v;
  if (DYN_LIST_DATATYPE(source) != DF_LIST) return(NULL);
  if (i >= DYN_LIST_N(source)) return(0);
  v = (DYN_LIST **) DYN_LIST_VALS(source);
  return(v[i]);
}

DYN_LIST *dynListCopyElementList(DYN_LIST *source, int i)
{
  DYN_LIST *element;

  element = dynListElementList(source, i);
  if (!element) return(NULL);

  return(dfuCopyDynList(element));
}

float *sdfMakeSdf (float start, int duration, float *spikes, int nspikes,
		   float *kernel, int ksize)
{
  int i, j, s, t;
  float *fp;
  int low=0, high=0, limit;

  if (!spikes || !duration || !nspikes) {
    return(NULL);
  }
  duration++;			/* make it inclusive */

  fp = (float *) calloc(duration, sizeof(float));

  low = nspikes;
  limit = start-ksize;
  for (j = 0; j < nspikes; j++) {
    if (spikes[j] > limit) {
      low = j;
      break;
    }
  }

  high = low-1;
  limit = start+duration+ksize;
  for (j = nspikes-1; j >= low-1 && j >= 0; j--) {
    if (spikes[j] < limit) {
      high = j;
      break;
    }
  }
  for (j = low; j <= high; j++) {
    s = spikes[j] - start;
    if (s >= 0 && s < duration) fp[s] += kernel[0];
    for (i = 1; i < ksize; i++) {
      t = s+i; 
      if (t < duration && t >= 0) fp[t] += kernel[i];
      t = s-i;
      if (t < duration && t >= 0) fp[t] += kernel[i];
    }
  }
  return fp;
}


float *sdfMakeKernel (float ksd, float nsd, int *ksize)
{
  int i, ks;
  float sum, denom, width;
  float *kernel;
  
  width = nsd * ksd;
  ks = width + 0.5;
  if (!(ks & 1)) ++ks;
  sum = 0.0;
  denom = - ksd * ksd;
  
  if (!(kernel = (float *)calloc(ks, sizeof(float)))) return (NULL);
  
  for (i = 0; i < ks; i++) {
    kernel[i] = (float) exp((i*i) / denom);
    sum += kernel[i];
  }
  
  for (i = 0; i < ks; i++)
    kernel[i] /= (2.0 * sum);
  
  *ksize = ks;
  return (kernel);
}


float *sdfMakeAdaptiveSdf (int start, int duration, float *spikes, 
			   int nspikes, float pilot_sd, float nsd)
{
  int i, j, s, t, kindex;
  float *fp;
  int low=0, high=0, limit;
  
  float *fixed_kernel;		/* These are for the pilot estimate    */
  float *pilot_estimate;	
  float *adapt_sd;
  float maxsd = duration / 4.0;	/* No kernel sd's greater than .25 the */
  float minsd = 2.0;		/*  total time or less than 2 ms       */

  float sumln, mu, lambda_i;
  int ksize, ksize_max;

  float *kernel = NULL;		/* These are for managing the variable */
#ifdef KERNEL_TABLE
  float **kernels;		/* Kernel sizes                        */
  int   *ksizes;
#endif

  if (!spikes || !duration || !nspikes) {
    return(NULL);
  }
  duration++;			/* make it inclusive */

  fixed_kernel = sdfMakeKernel (pilot_sd, nsd, &ksize);
  pilot_estimate = sdfMakeSdf (start, duration, spikes, nspikes,
			       fixed_kernel, ksize);
  free((void *) fixed_kernel);
  
  /* Calculate mu - the geometric mean */
  for (i = 0, sumln = 0.0; i < duration; i++) 
    if (pilot_estimate[i] > 0.0) sumln += log(pilot_estimate[i]);
  mu = exp(sumln/duration);

  /* Now create the adaptive sds, by dividing pilot sd by lambda */
  adapt_sd = (float *) calloc(duration, sizeof(float));

  ksize_max = 0;
  for (i = 0; i < duration; i++) {
    lambda_i = sqrt(pilot_estimate[i]/mu);
    if (lambda_i > 0.0) {
      adapt_sd[i] = pilot_sd/lambda_i;
      if (adapt_sd[i] > maxsd) adapt_sd[i] = maxsd;
      if (adapt_sd[i] < minsd) adapt_sd[i] = minsd;
      if (adapt_sd[i]*nsd > ksize_max) ksize_max = adapt_sd[i]*nsd;
    }
    else adapt_sd[i] = minsd;
  }
  ksize_max = ksize;

  free((void *) pilot_estimate);

  /* Allocate the output buffer */
  fp = (float *) calloc(duration, sizeof(float));

  /* 
   * Now figure out start and stop indices for spikes, using maximum ksize
   * in the set 
   */
  low = nspikes;
  limit = start-ksize_max;
  for (j = 0; j < nspikes; j++) {
    if (spikes[j] > limit) {
      low = j;
      break;
    }
  }

  high = low-1;
  limit = start+duration+ksize_max;
  for (j = nspikes-1; j >= low-1 && j >= 0; j--) {
    if (spikes[j] < limit) {
      high = j;
      break;
    }
  }
  for (j = low; j <= high; j++) {
    kindex = s = spikes[j] - start;
    if (kindex < 0) kindex = 0;
    if (kindex >= duration) kindex = duration-1;
    if (kernel) free((void *) kernel);
    
    /* This should probably be done using a table... */
    kernel = sdfMakeKernel (adapt_sd[kindex], nsd, &ksize);

    if (s >= 0 && s < duration) fp[s] += kernel[0];
    for (i = 1; i < ksize; i++) {
      t = s+i; 
      if (t < duration && t >= 0) fp[t] += kernel[i];
      t = s-i;
      if (t < duration && t >= 0) fp[t] += kernel[i];
    }
  }
  
  if (kernel) free((void *) kernel);
  free((void *) adapt_sd);
  return fp;
}

