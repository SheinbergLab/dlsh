/*************************************************************************
 *
 *  NAME
 *    dlarith.c
 *
 *  DESCRIPTION
 *    Basic dl arithmetic / logical relation functions
 *
 *    DL_MATH_ADD
 *    DL_MATH_SUB
 *    DL_MATH_MUL
 *    DL_MATH_DIV
 *    DL_MATH_POW
 *    DL_MATH_MIN
 *    DL_MATH_MAX
 *
 *  AUTHOR
 *    DLS
 *
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "df.h"
#include "dfana.h"

#include <utilc.h>

DYN_LIST *dynListArithListList(DYN_LIST *l1, DYN_LIST *l2, int func)
{
  int i;
  int sametype = 1;		/* are args the same datatype?      */
  int copymode = 0;		/* N(l1)==N(l2), N(l1)==1, N(l2)==1 */
  int length, floatfirst = 1;
  DYN_LIST *list = NULL;
  DYN_LIST *flist = NULL, *nonflist = NULL;

  if (!l1 || !l2) return(NULL);

  /* If either operand is the empty list, just return it */
  if (DYN_LIST_DATATYPE(l1) != DF_LIST && DYN_LIST_N(l1) == 0) {
    if (DYN_LIST_DATATYPE(l1) == DF_FLOAT ||
	DYN_LIST_DATATYPE(l2) == DF_FLOAT) {
      list = dfuCreateDynList(DF_FLOAT, 1);
    }
    else {
      list = dfuCreateDynList(DYN_LIST_DATATYPE(l1), 1);
    }
    return list;
  }
  else if (DYN_LIST_DATATYPE(l2) != DF_LIST && DYN_LIST_N(l2) == 0) {
    if (DYN_LIST_DATATYPE(l1) == DF_FLOAT ||
	DYN_LIST_DATATYPE(l2) == DF_FLOAT) {
      list = dfuCreateDynList(DF_FLOAT, 1);
    }
    else {
      list = dfuCreateDynList(DYN_LIST_DATATYPE(l2), 1);
    }
    return list;
  }

  /* Determine whether either list is a single element list */
  if (DYN_LIST_N(l1) == DYN_LIST_N(l2)) {
    copymode = 0;
    length = DYN_LIST_N(l1);
  }
  else if (DYN_LIST_N(l1) == 1) {
    copymode = 1;
    length = DYN_LIST_N(l2);
  }
  else if (DYN_LIST_N(l2) == 1) {
    copymode = 2;
    length = DYN_LIST_N(l1);
  }
  else return NULL;

  if (DYN_LIST_DATATYPE(l1) == DF_STRING || 
      DYN_LIST_DATATYPE(l2) == DF_STRING) return(NULL);



  if (DYN_LIST_DATATYPE(l1) == DF_LIST &&
      DYN_LIST_DATATYPE(l2) == DF_LIST) {
    DYN_LIST **vals1 = (DYN_LIST **) DYN_LIST_VALS(l1);
    DYN_LIST **vals2 = (DYN_LIST **) DYN_LIST_VALS(l2);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);

    switch (copymode) {
    case 0:
      for (i = 0; i < length; i++) {
	curlist = dynListArithListList(vals1[i], vals2[i], func);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = dynListArithListList(vals1[0], vals2[i], func);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = dynListArithListList(vals1[i], vals2[0], func);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    }
  }

  else if (DYN_LIST_DATATYPE(l1) == DF_LIST &&
	   DYN_LIST_DATATYPE(l2) != DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(l1);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);
    if (copymode > 0) {
      for (i = 0; i < DYN_LIST_N(l1); i++) {
	curlist = dynListArithListList(vals[i], l2, func);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
    }
    else {
      DYN_LIST *arg = dfuCreateDynList(DYN_LIST_DATATYPE(l2),1);
      for (i = 0; i < DYN_LIST_N(l1); i++) {
	dfuResetDynList(arg);
	dynListCopyElement(l2, i, arg);
	curlist = dynListArithListList(vals[i], arg, func);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      dfuFreeDynList(arg);
      return(list);
    }
  }
  
  else if (DYN_LIST_DATATYPE(l2) == DF_LIST &&
	   DYN_LIST_DATATYPE(l1) != DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(l2);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);
    if (copymode > 0) {
      for (i = 0; i < DYN_LIST_N(l2); i++) {
	curlist = dynListArithListList(l1, vals[i], func);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
    }
    else {
      DYN_LIST *arg = dfuCreateDynList(DYN_LIST_DATATYPE(l1),1);
      for (i = 0; i < DYN_LIST_N(l2); i++) {
	dfuResetDynList(arg);
	dynListCopyElement(l1, i, arg);
	curlist = dynListArithListList(arg, vals[i], func);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      dfuFreeDynList(arg);
      return(list);
    }
  }

  if (DYN_LIST_DATATYPE(l1) != DYN_LIST_DATATYPE(l2)) {
    sametype = 0;
    if (DYN_LIST_DATATYPE(l1) == DF_FLOAT) {
      floatfirst = 1;		/* float first */
      flist = l1;
      nonflist = l2;
    }
    else if (DYN_LIST_DATATYPE(l2) == DF_FLOAT) {
      floatfirst = 0;		/* float second */
      flist = l2;
      nonflist = l1;
    }
    else {
      return(NULL);
    }
  }

  if (sametype) {
    switch (DYN_LIST_DATATYPE(l1)) {
    case DF_LONG:
      {
	int *vals1 = (int *) DYN_LIST_VALS(l1);
	int *vals2 = (int *) DYN_LIST_VALS(l2);
	int *newvals;
	
	if (!length) return dfuCreateDynList(DYN_LIST_DATATYPE(l1), length);
	else newvals = (int *) calloc(length, sizeof(int));

	switch (copymode) {
	case 0:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[i]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) pow(vals1[i],vals2[i]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) atan2(vals1[i],vals2[i]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) fmod(vals1[i],vals2[i]);
	    break;
	  }
	  break;
	case 1:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[0]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] < vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] > vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) pow(vals1[0],vals2[i]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) atan2(vals1[0],vals2[i]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) fmod(vals1[0],vals2[i]);
	    break;
	  }
	  break;
	case 2:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+vals2[0];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-vals2[0];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*vals2[0];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[0]) newvals[i] = vals1[i]/vals2[0];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) pow(vals1[i],vals2[0]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) atan2(vals1[i],vals2[0]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) fmod(vals1[i],vals2[0]);
	    break;
	  }
	  break;
	}
	list = dfuCreateDynListWithVals(DF_LONG, length, newvals);
      }
      break;
    case DF_SHORT:
      {
	short *vals1 = (short *) DYN_LIST_VALS(l1);
	short *vals2 = (short *) DYN_LIST_VALS(l2);
	short *newvals;
	
	if (!length) return dfuCreateDynList(DYN_LIST_DATATYPE(l1), length);
	else newvals = (short *) calloc(length, sizeof(short));

	switch (copymode) {
	case 0:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[i]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[i] ? vals1[i] : vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[0]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] < vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] > vals2[i] ? vals1[0] : vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+vals2[0];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-vals2[0];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*vals2[0];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[0]) newvals[i] = vals1[i]/vals2[0];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[0] ? vals1[i] : vals2[0];
	    break;
	  }
	  break;
	}
	list = dfuCreateDynListWithVals(DF_SHORT, length, newvals);
      }
    break;
    case DF_FLOAT:
      {
	float *vals1 = (float *) DYN_LIST_VALS(l1);
	float *vals2 = (float *) DYN_LIST_VALS(l2);
	float *newvals;
	
	if (!length) return dfuCreateDynList(DYN_LIST_DATATYPE(l1), length);
	else newvals = (float *) calloc(length, sizeof(float));

	switch (copymode) {
	case 0:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[i]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) pow(vals1[i],vals2[i]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) atan2(vals1[i],vals2[i]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) fmod(vals1[i],vals2[i]);
	    break;
	  }
	  break;
	case 1:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[0]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] < vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] > vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) pow(vals1[0],vals2[i]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) atan2(vals1[0],vals2[i]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) fmod(vals1[0],vals2[i]);
	    break;
	  }
	  break;
	case 2:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+vals2[0];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-vals2[0];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*vals2[0];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[0]) newvals[i] = vals1[i]/vals2[0];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) pow(vals1[i],vals2[0]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) atan2(vals1[i],vals2[0]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) fmod(vals1[i],vals2[0]);
	    break;
	  }
	  break;
	}
	list = dfuCreateDynListWithVals(DF_FLOAT, length, newvals);
      }
    break;
    case DF_CHAR:
      {
	char *vals1 = (char *) DYN_LIST_VALS(l1);
	char *vals2 = (char *) DYN_LIST_VALS(l2);
	char *newvals;

	if (!length) return dfuCreateDynList(DYN_LIST_DATATYPE(l1), length);
	else newvals = (char *) calloc(length, sizeof(char));

	switch (copymode) {
	case 0:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[i]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[i] ? vals1[i] : vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[0]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] < vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] > vals2[i] ? vals1[0] : vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+vals2[0];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-vals2[0];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*vals2[0];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[0]) newvals[i] = vals1[i]/vals2[0];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[0] ? vals1[i] : vals2[0];
	    break;
	  }
	  break;
	}
	list = dfuCreateDynListWithVals(DF_CHAR, length, newvals);
      }
    break;
    default:
      return(NULL);
      break;
    }
  }
  else if (floatfirst) {	/* float list is arg1, nonfloat arg2 */
    switch (DYN_LIST_DATATYPE(nonflist)) {
    case DF_LONG:
      {
	float *vals1 = (float *) DYN_LIST_VALS(flist);
	int *vals2 = (int *) DYN_LIST_VALS(nonflist);
	float *newvals;
	
	if (!length) return dfuCreateDynList(DF_FLOAT, length);
	else newvals = (float *) calloc(length, sizeof(float));

	switch (copymode) {
	case 0:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+(float)vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-(float)vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*(float)vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[i]/(float)vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) pow(vals1[i],(double) vals2[i]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) atan2(vals1[i],(double) vals2[i]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) fmod(vals1[i],(double) vals2[i]);
	    break;
	  }
	  break;
	case 1:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0]+(float) vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0]-(float) vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0]*(float) vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[0]/(float)vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] < vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] > vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) pow(vals1[0],(double) vals2[i]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) atan2(vals1[0],(double) vals2[i]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) fmod(vals1[0],(double) vals2[i]);
	    break;
	  }
	  break;
	case 2:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+(float)vals2[0];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-(float)vals2[0];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*(float)vals2[0];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[0]) newvals[i] = vals1[i]/(float)vals2[0];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) pow(vals1[i],(float) vals2[0]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) atan2(vals1[i],(float) vals2[0]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) fmod(vals1[i],(float) vals2[0]);
	    break;
	  }
	  break;
	}
	list = dfuCreateDynListWithVals(DF_FLOAT, length, newvals);
      }
      break;
    case DF_SHORT:
      {
	float *vals1 = (float *) DYN_LIST_VALS(flist);
	short *vals2 = (short *) DYN_LIST_VALS(nonflist);
	float *newvals;
	
	if (!length) return dfuCreateDynList(DF_FLOAT, length);
	else newvals = (float *) calloc(length, sizeof(float));
	
	switch (copymode) {
	case 0:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+(float)vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-(float)vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*(float)vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[i]/(float)vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[i] ? vals1[i] : vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]+(float)vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]-(float)vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]*(float)vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[0]/(float)vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] < vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] > vals2[i] ? vals1[0] : vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+(float)vals2[0];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-(float)vals2[0];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*(float)vals2[0];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[0]) newvals[i] = vals1[i]/(float)vals2[0];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[0] ? vals1[i] : vals2[0];
	    break;
	  }
	  break;
	}
	list = dfuCreateDynListWithVals(DF_FLOAT, length, newvals);
      }
    break;
    case DF_CHAR:
      {
	float *vals1 = (float *) DYN_LIST_VALS(flist);
	char *vals2 = (char *) DYN_LIST_VALS(nonflist);
	float *newvals;
	
	if (!length) return dfuCreateDynList(DF_FLOAT, length);
	else newvals = (float *) calloc(length, sizeof(float));

	switch (copymode) {
	case 0:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+(float)vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-(float)vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*(float)vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[i]/(float)vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[i] ? vals1[i] : vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]+(float)vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]-(float)vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0]*(float)vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = vals1[0]/(float)vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] < vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] > vals2[i] ? vals1[0] : vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]+(float)vals2[0];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]-(float)vals2[0];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i]*(float)vals2[0];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[0]) newvals[i] = vals1[i]/(float)vals2[0];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[0] ? vals1[i] : vals2[0];
	    break;
	  }
	  break;
	}
	list = dfuCreateDynListWithVals(DF_FLOAT, length, newvals);
      }
    break;
    default:
      return(NULL);
      break;
    }
  }
  else  {			/* float list is arg2, nonfloat arg1 */
    switch (DYN_LIST_DATATYPE(nonflist)) {
    case DF_LONG:
      {
	int *vals1 = (int *) DYN_LIST_VALS(nonflist);
	float *vals2 = (float *) DYN_LIST_VALS(flist);
	float *newvals;
	
	if (!length) return dfuCreateDynList(DF_FLOAT, length);
	else newvals = (float *) calloc(length, sizeof(float));

	switch (copymode) {
	case 0:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = (float)vals1[i]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) pow((float) vals1[i], vals2[i]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) atan2((float) vals1[i], vals2[i]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) fmod((float) vals1[i], vals2[i]);
	    break;
	  }
	  break;
	case 1:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[0]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[0]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[0]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = (float)vals1[0]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] < vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] > vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) pow((float) vals1[0], vals2[i]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) atan2((float) vals1[0], vals2[i]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) fmod((float) vals1[0], vals2[i]);
	    break;
	  }
	  break;
	case 2:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]+vals2[0];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]-vals2[0];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]*vals2[0];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[0]) newvals[i] = (float)vals1[i]/vals2[0];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_POW:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) pow((float) vals1[i], vals2[0]);
	    break;
	  case DL_MATH_ATAN2:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) atan2((float) vals1[i], vals2[0]);
	    break;
	  case DL_MATH_FMOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (float) fmod((float) vals1[i], vals2[0]);
	    break;
	  }
	  break;
	}
	list = dfuCreateDynListWithVals(DF_FLOAT, length, newvals);
      }
      break;
    case DF_SHORT:
      {
	short *vals1 = (short *) DYN_LIST_VALS(nonflist);
	float *vals2 = (float *) DYN_LIST_VALS(flist);
	float *newvals;
	
	if (!length) return dfuCreateDynList(DF_FLOAT, length);
	else newvals = (float *) calloc(length, sizeof(float));

	switch (copymode) {
	case 0:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = (float)vals1[i]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[i] ? vals1[i] : vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[0]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[0]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[0]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = (float)vals1[0]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] < vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] > vals2[i] ? vals1[0] : vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]+vals2[0];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]-vals2[0];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]*vals2[0];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[0]) newvals[i] = (float)vals1[i]/vals2[0];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[0] ? vals1[i] : vals2[0];
	    break;
	  }
	  break;
	}
	list = dfuCreateDynListWithVals(DF_FLOAT, length, newvals);
      }
      break;
    case DF_CHAR:
      {
	char *vals1 = (char *) DYN_LIST_VALS(nonflist);
	float *vals2 = (float *) DYN_LIST_VALS(flist);
	float *newvals;
	
	if (!length) return dfuCreateDynList(DF_FLOAT, length);
	else newvals = (float *) calloc(length, sizeof(float));

	switch (copymode) {
	case 0:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = (float)vals1[i]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[i] ? vals1[i] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[i] ? vals1[i] : vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[0]+vals2[i];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[0]-vals2[i];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[0]*vals2[i];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[i]) newvals[i] = (float)vals1[0]/vals2[i];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] < vals2[i] ? vals1[0] : vals2[i];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[0] > vals2[i] ? vals1[0] : vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (func) {
	  case DL_MATH_ADD:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]+vals2[0];
	    break;
	  case DL_MATH_SUB:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]-vals2[0];
	    break;
	  case DL_MATH_MUL:
	    for (i = 0; i < length; i++) newvals[i] = (float)vals1[i]*vals2[0];
	    break;
	  case DL_MATH_DIV:
	    for (i = 0; i < length; i++) {
	      if (vals2[0]) newvals[i] = (float)vals1[i]/vals2[0];
	      else newvals[i] = 0;
	    }
	    break;
	  case DL_MATH_MIN:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] < vals2[0] ? vals1[i] : vals2[0];
	    break;
	  case DL_MATH_MAX:
	    for (i = 0; i < length; i++) 
	      newvals[i] = vals1[i] > vals2[0] ? vals1[i] : vals2[0];
	    break;
	  }
	  break;
	}
	list = dfuCreateDynListWithVals(DF_FLOAT, length, newvals);
      }
      break;
    default:
      return(NULL);
      break;
    }
  }
  return(list);
}

DYN_LIST *dynListOneOf(DYN_LIST *l1, DYN_LIST *l2)
{
  DYN_LIST *result, *list, *intermed, *compare, *orred;
  int i, copymode, length;
  
  /* When l1 is empty, the result is also empty */
  if (!DYN_LIST_N(l1)) {
    return dfuCopyDynList(l1);
  }

  /* Determine whether either list is a single element list */
  if (DYN_LIST_DATATYPE(l1) == DF_LIST ||
      DYN_LIST_DATATYPE(l2) == DF_LIST) {
    if (DYN_LIST_N(l1) == DYN_LIST_N(l2)) {
      copymode = 0;
      length = DYN_LIST_N(l1);
    }
    else if (DYN_LIST_N(l1) == 1) {
      copymode = 1;
      length = DYN_LIST_N(l2);
    }
    else if (DYN_LIST_N(l2) == 1) {
      copymode = 2;
      length = DYN_LIST_N(l1);
    }
  }


  /* Determine whether either list is a single element list */
  if (DYN_LIST_DATATYPE(l1) == DF_LIST ||
      DYN_LIST_DATATYPE(l2) != DF_LIST) {
    if (DYN_LIST_N(l1) == DYN_LIST_N(l2)) {
      copymode = 0;
      length = DYN_LIST_N(l1);
    }
    else if (DYN_LIST_N(l1) == 1) {
      copymode = 1;
      length = DYN_LIST_N(l2);
    }
    else if (DYN_LIST_N(l2) == 1) {
      copymode = 2;
      length = DYN_LIST_N(l1);
    }
  }

  if (DYN_LIST_DATATYPE(l2) == DF_LIST ||
      DYN_LIST_DATATYPE(l1) != DF_LIST) {
    length = DYN_LIST_N(l2);
  }
  
  if (DYN_LIST_DATATYPE(l2) != DF_LIST ||
      DYN_LIST_DATATYPE(l1) != DF_LIST) {
    length = DYN_LIST_N(l1);
  }
  
  if (DYN_LIST_DATATYPE(l1) == DF_LIST &&
      DYN_LIST_DATATYPE(l2) == DF_LIST) {
    DYN_LIST **vals1 = (DYN_LIST **) DYN_LIST_VALS(l1);
    DYN_LIST **vals2 = (DYN_LIST **) DYN_LIST_VALS(l2);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);
    
    switch (copymode) {
    case 0:
      for (i = 0; i < length; i++) {
	curlist = dynListOneOf(vals1[i], vals2[i]);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = dynListOneOf(vals1[0], vals2[i]);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = dynListOneOf(vals1[i], vals2[0]);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    }
  }
  
  else if (DYN_LIST_DATATYPE(l1) == DF_LIST &&
	   DYN_LIST_DATATYPE(l2) != DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(l1);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);
    
    for (i = 0; i < DYN_LIST_N(l1); i++) {
      curlist = dynListOneOf(vals[i], l2);
      if (curlist) dfuMoveDynListList(list, curlist);
      else {
	if (list) dfuFreeDynList(list);
	return NULL;
      }
    }
    return(list);
  }

  else if (DYN_LIST_DATATYPE(l2) == DF_LIST &&
	   DYN_LIST_DATATYPE(l1) != DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(l2);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);

    for (i = 0; i < DYN_LIST_N(l2); i++) {
      curlist = dynListOneOf(l1, vals[i]);
      if (curlist) dfuMoveDynListList(list, curlist);
      else {
	if (list) dfuFreeDynList(list);
	return NULL;
      }
    }
    return(list);
  }
  
  /* Special case -- empty l2 */
  if (!DYN_LIST_N(l2)) {
    DYN_LIST *lengths;
    if (DYN_LIST_DATATYPE(l1) == DF_LIST) {
      lengths = dynListListLengths(l1);
      result = dynListZeros(lengths);
      dfuFreeDynList(lengths);
    } else {
      result = dynListZerosInt(DYN_LIST_N(l1));
    }
    return result;
  }

  /* compare will hold the single element list for the successive dl_eq's */
  compare = dfuCreateDynList(DYN_LIST_DATATYPE(l2), 1);
  
  dynListCopyElement(l2, 0, compare);
  result = dynListRelationListList(l1, compare, DL_RELATION_EQ);
  if (!result) {
      dfuFreeDynList(compare);
      return NULL;
  }

  for (i = 1; i < DYN_LIST_N(l2); i++) {
    dfuResetDynList(compare);
    dynListCopyElement(l2, i, compare);
    intermed = dynListRelationListList(l1, compare, DL_RELATION_EQ);
    if (!intermed) {
      dfuFreeDynList(compare);
      dfuFreeDynList(result);
      return NULL;
    }
    orred = dynListRelationListList(result, intermed, DL_RELATION_OR);
    dfuFreeDynList(intermed);
    dfuFreeDynList(result);
    result = orred;
  }

  dfuFreeDynList(compare);
  return result;
}

DYN_LIST *dynListOneOfIndices(DYN_LIST *l1, DYN_LIST *l2)
{
  DYN_LIST *result, *list, *intermed, *compare, *orred, *retlist;
  int i, copymode, length;
  
  /* When l1 is empty, the result is also empty */
  if (!DYN_LIST_N(l1)) {
    return dfuCopyDynList(l1);
  }

  /* Determine whether either list is a single element list */
  if (DYN_LIST_DATATYPE(l1) == DF_LIST ||
      DYN_LIST_DATATYPE(l2) == DF_LIST) {
    if (DYN_LIST_N(l1) == DYN_LIST_N(l2)) {
      copymode = 0;
      length = DYN_LIST_N(l1);
    }
    else if (DYN_LIST_N(l1) == 1) {
      copymode = 1;
      length = DYN_LIST_N(l2);
    }
    else if (DYN_LIST_N(l2) == 1) {
      copymode = 2;
      length = DYN_LIST_N(l1);
    }
  }


  /* Determine whether either list is a single element list */
  if (DYN_LIST_DATATYPE(l1) == DF_LIST ||
      DYN_LIST_DATATYPE(l2) != DF_LIST) {
    if (DYN_LIST_N(l1) == DYN_LIST_N(l2)) {
      copymode = 0;
      length = DYN_LIST_N(l1);
    }
    else if (DYN_LIST_N(l1) == 1) {
      copymode = 1;
      length = DYN_LIST_N(l2);
    }
    else if (DYN_LIST_N(l2) == 1) {
      copymode = 2;
      length = DYN_LIST_N(l1);
    }
  }
  
  if (DYN_LIST_DATATYPE(l2) == DF_LIST ||
      DYN_LIST_DATATYPE(l1) != DF_LIST) {
    length = DYN_LIST_N(l2);
  }
  

  if (DYN_LIST_DATATYPE(l1) == DF_LIST &&
      DYN_LIST_DATATYPE(l2) == DF_LIST) {
    DYN_LIST **vals1 = (DYN_LIST **) DYN_LIST_VALS(l1);
    DYN_LIST **vals2 = (DYN_LIST **) DYN_LIST_VALS(l2);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);
    
    switch (copymode) {
    case 0:
      for (i = 0; i < length; i++) {
	curlist = dynListOneOfIndices(vals1[i], vals2[i]);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = dynListOneOfIndices(vals1[0], vals2[i]);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = dynListOneOfIndices(vals1[i], vals2[0]);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    }
  }
  
  else if (DYN_LIST_DATATYPE(l1) == DF_LIST &&
	   DYN_LIST_DATATYPE(l2) != DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(l1);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);
    
    for (i = 0; i < DYN_LIST_N(l1); i++) {
      curlist = dynListOneOfIndices(vals[i], l2);
      if (curlist) dfuMoveDynListList(list, curlist);
      else {
	if (list) dfuFreeDynList(list);
	return NULL;
      }
    }
    return(list);
  }

  else if (DYN_LIST_DATATYPE(l2) == DF_LIST &&
	   DYN_LIST_DATATYPE(l1) != DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(l2);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);

    for (i = 0; i < DYN_LIST_N(l2); i++) {
      curlist = dynListOneOfIndices(l1, vals[i]);
      if (curlist) dfuMoveDynListList(list, curlist);
      else {
	if (list) dfuFreeDynList(list);
	return NULL;
      }
    }
    return(list);
  }
  
  /* Special case -- empty l2 */
  if (!DYN_LIST_N(l2)) {
    result = dfuCreateDynList(DF_LONG, 10);
    return result;
  }

  /* compare will hold the single element list for the successive dl_eq's */
  compare = dfuCreateDynList(DYN_LIST_DATATYPE(l2), 1);
  
  dynListCopyElement(l2, 0, compare);
  result = dynListRelationListList(l1, compare, DL_RELATION_EQ);
  if (!result) {
      dfuFreeDynList(compare);
      return NULL;
  }

  for (i = 1; i < DYN_LIST_N(l2); i++) {
    dfuResetDynList(compare);
    dynListCopyElement(l2, i, compare);
    intermed = dynListRelationListList(l1, compare, DL_RELATION_EQ);
    if (!intermed) {
      dfuFreeDynList(compare);
      dfuFreeDynList(result);
      return NULL;
    }
    orred = dynListRelationListList(result, intermed, DL_RELATION_OR);
    dfuFreeDynList(intermed);
    dfuFreeDynList(result);
    result = orred;
  }

  dfuFreeDynList(compare);
  retlist = dynListIndices(result);
  dfuFreeDynList(result);

  return result;
}


DYN_LIST *dynListRelationListList(DYN_LIST *l1, DYN_LIST *l2, int op)
{
  int i;
  int copymode = 0, sametype = 1, length;
  int *newvals = NULL;
  DYN_LIST *list = NULL;
  DYN_LIST *flist = NULL, *nonflist = NULL;
  int floatfirst = 1;
  if (!l1 || !l2) return NULL;

  /* If either operand is the empty list, just return it */
  if (DYN_LIST_DATATYPE(l1) != DF_LIST && DYN_LIST_N(l1) == 0) {
    list = dfuCreateDynList(DF_LONG, 1);
    return list;
  }
  else if (DYN_LIST_DATATYPE(l2) != DF_LIST && DYN_LIST_N(l2) == 0) {
    list = dfuCreateDynList(DF_LONG, 1);
    return list;
  }

  if (DYN_LIST_N(l1) == DYN_LIST_N(l2)) {
    copymode = 0;
    length = DYN_LIST_N(l1);
  }
  else if (DYN_LIST_N(l1) == 1) {
    copymode = 1;
    length = DYN_LIST_N(l2);
  }
  else if (DYN_LIST_N(l2) == 1) {
    copymode = 2;
    length = DYN_LIST_N(l1);
  }
  else return NULL;

  /* If either argument is a list of lists ... */

  if (DYN_LIST_DATATYPE(l1) == DF_LIST &&
      DYN_LIST_DATATYPE(l2) == DF_LIST) {
    DYN_LIST **vals1 = (DYN_LIST **) DYN_LIST_VALS(l1);
    DYN_LIST **vals2 = (DYN_LIST **) DYN_LIST_VALS(l2);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);
    
    switch (copymode) {
    case 0:
      for (i = 0; i < length; i++) {
	curlist = dynListRelationListList(vals1[i], vals2[i], op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = dynListRelationListList(vals1[0], vals2[i], op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = dynListRelationListList(vals1[i], vals2[0], op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    }
  }

  else if (DYN_LIST_DATATYPE(l1) == DF_LIST &&
	   DYN_LIST_DATATYPE(l2) != DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(l1);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);

    if (copymode > 0) {
      for (i = 0; i < DYN_LIST_N(l1); i++) {
	curlist = dynListRelationListList(vals[i], l2, op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
    }
    else {
      DYN_LIST *arg = dfuCreateDynList(DYN_LIST_DATATYPE(l2),1);
      for (i = 0; i < DYN_LIST_N(l1); i++) {
	dfuResetDynList(arg);
	dynListCopyElement(l2, i, arg);
	curlist = dynListRelationListList(vals[i], arg, op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      dfuFreeDynList(arg);
      return(list);
    }
  }
  else if (DYN_LIST_DATATYPE(l2) == DF_LIST &&
	   DYN_LIST_DATATYPE(l1) != DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(l2);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);
    if (copymode > 0) {
      for (i = 0; i < DYN_LIST_N(l2); i++) {
	curlist = dynListRelationListList(l1, vals[i], op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
    }
    else {
      DYN_LIST *arg = dfuCreateDynList(DYN_LIST_DATATYPE(l1),1);
      for (i = 0; i < DYN_LIST_N(l2); i++) {
	dfuResetDynList(arg);
	dynListCopyElement(l1, i, arg);
	curlist = dynListRelationListList(arg, vals[i], op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      dfuFreeDynList(arg);
      return(list);
    }
  }


  if (DYN_LIST_DATATYPE(l1) != DYN_LIST_DATATYPE(l2)) {
    sametype = 0;
    if (DYN_LIST_DATATYPE(l1) == DF_FLOAT) {
      floatfirst = 1;		/* float first */
      flist = l1;
      nonflist = l2;
    }
    else if (DYN_LIST_DATATYPE(l2) == DF_FLOAT) {
      floatfirst = 0;		/* float second */
      flist = l2;
      nonflist = l1;
    }
    else {
      return(NULL);
    }
  }

  if (!length) 
    return(dfuCreateDynList(DF_LONG, length));
  else newvals = (int *) calloc(length, sizeof(int));

  if (DYN_LIST_DATATYPE(l1) == DF_STRING ||
      DYN_LIST_DATATYPE(l2) == DF_STRING) {
    char **vals1, **vals2;
    if (!sametype) return NULL;
    vals1 = (char **) DYN_LIST_VALS(l1);
    vals2 = (char **) DYN_LIST_VALS(l2);

    switch (copymode) {
    case 0:
      switch (op) {
      case DL_RELATION_AND:
      case DL_RELATION_OR:
	return NULL;
	break;
      case DL_RELATION_EQ:
	for (i = 0; i < length; i++) 
	  newvals[i] = !strcmp(vals1[i], vals2[i]);
	break;
      case DL_RELATION_NE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[i]) != 0;
	break;
      case DL_RELATION_LT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[i]) < 0;
	break;
      case DL_RELATION_LTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[i]) <= 0;
	break;
      case DL_RELATION_GT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[i]) > 0;
	break;
      case DL_RELATION_GTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[i]) <= 0;
	break;
      }
      break;
    case 1:
      switch (op) {
      case DL_RELATION_AND:
      case DL_RELATION_OR:
	return NULL;
	break;
      case DL_RELATION_EQ:
	for (i = 0; i < length; i++) 
	  newvals[i] = !strcmp(vals1[0], vals2[i]);
	break;
      case DL_RELATION_NE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[0], vals2[i]) != 0;
	break;
      case DL_RELATION_LT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[0], vals2[i]) < 0;
	break;
      case DL_RELATION_LTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[0], vals2[i]) <= 0;
	break;
      case DL_RELATION_GT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[0], vals2[i]) > 0;
	break;
      case DL_RELATION_GTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[0], vals2[i]) <= 0;
	break;
      }
      break;
    case 2:
      switch (op) {
      case DL_RELATION_AND:
      case DL_RELATION_OR:
	return NULL;
	break;
      case DL_RELATION_EQ:
	for (i = 0; i < length; i++) 
	  newvals[i] = !strcmp(vals1[i], vals2[0]);
	break;
      case DL_RELATION_NE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[0]) != 0;
	break;
      case DL_RELATION_LT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[0]) < 0;
	break;
      case DL_RELATION_LTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[0]) <= 0;
	break;
      case DL_RELATION_GT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[0]) > 0;
	break;
      case DL_RELATION_GTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[0]) <= 0;
	break;
      }
      break;
    }
    list = dfuCreateDynListWithVals(DF_LONG, length, newvals);
    return(list);
  }

  if (sametype) {
    switch (DYN_LIST_DATATYPE(l1)) {
    case DF_LONG:
      {
	int *vals1 = (int *) DYN_LIST_VALS(l1);
	int *vals2 = (int *) DYN_LIST_VALS(l2);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_SHORT:
      {
	short *vals1 = (short *) DYN_LIST_VALS(l1);
	short *vals2 = (short *) DYN_LIST_VALS(l2);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	break;
	}
      }
      break;
    case DF_FLOAT:
      {
	float *vals1 = (float *) DYN_LIST_VALS(l1);
	float *vals2 = (float *) DYN_LIST_VALS(l2);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_CHAR:
      {
	char *vals1 = (char *) DYN_LIST_VALS(l1);
	char *vals2 = (char *) DYN_LIST_VALS(l2);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
	break;
      }
    default:
      return(NULL);
      break;
    }
  }
  else if (floatfirst) {	/* float list is arg1, nonfloat arg2 */
    switch (DYN_LIST_DATATYPE(nonflist)) {
    case DF_LONG:
      {
	float *vals1 = (float *) DYN_LIST_VALS(flist);
	int *vals2 = (int *) DYN_LIST_VALS(nonflist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_SHORT:
      {
	float *vals1 = (float *) DYN_LIST_VALS(flist);
	short *vals2 = (short *) DYN_LIST_VALS(nonflist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_CHAR:
      {
	float *vals1 = (float *) DYN_LIST_VALS(flist);
	char *vals2 = (char *) DYN_LIST_VALS(nonflist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    default:
      return(NULL);
      break;
    }
  }
  else  {			/* float list is arg2, nonfloat arg1 */
    switch (DYN_LIST_DATATYPE(nonflist)) {
    case DF_LONG:
      {
	int *vals1 = (int *) DYN_LIST_VALS(nonflist);
	float *vals2 = (float *) DYN_LIST_VALS(flist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_SHORT:
      {
	short *vals1 = (short *) DYN_LIST_VALS(nonflist);
	float *vals2 = (float *) DYN_LIST_VALS(flist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_CHAR:
      {
	char *vals1 = (char *) DYN_LIST_VALS(nonflist);
	float *vals2 = (float *) DYN_LIST_VALS(flist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    default:
      return(NULL);
      break;
    }
  }
  list = dfuCreateDynListWithVals(DF_LONG, length, newvals);
  return(list);
}


DYN_LIST *dynListRelationListListIndices(DYN_LIST *l1, DYN_LIST *l2, int op)
{
  int i;
  int copymode = 0, sametype = 1, length;
  int *newvals = NULL;
  DYN_LIST *list = NULL;
  DYN_LIST *flist = NULL, *nonflist = NULL;
  int floatfirst = 1;
  int match_count = 0;
  if (!l1 || !l2) return NULL;

  /* If either operand is the empty list, just return it */
  if (DYN_LIST_DATATYPE(l1) != DF_LIST && DYN_LIST_N(l1) == 0) {
    list = dfuCreateDynList(DF_LONG, 1);
    return list;
  }
  else if (DYN_LIST_DATATYPE(l2) != DF_LIST && DYN_LIST_N(l2) == 0) {
    list = dfuCreateDynList(DF_LONG, 1);
    return list;
  }

  if (DYN_LIST_N(l1) == DYN_LIST_N(l2)) {
    copymode = 0;
    length = DYN_LIST_N(l1);
  }
  else if (DYN_LIST_N(l1) == 1) {
    copymode = 1;
    length = DYN_LIST_N(l2);
  }
  else if (DYN_LIST_N(l2) == 1) {
    copymode = 2;
    length = DYN_LIST_N(l1);
  }
  else return NULL;

  /* If either argument is a list of lists ... */

  if (DYN_LIST_DATATYPE(l1) == DF_LIST &&
      DYN_LIST_DATATYPE(l2) == DF_LIST) {
    DYN_LIST **vals1 = (DYN_LIST **) DYN_LIST_VALS(l1);
    DYN_LIST **vals2 = (DYN_LIST **) DYN_LIST_VALS(l2);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);
    
    switch (copymode) {
    case 0:
      for (i = 0; i < length; i++) {
	curlist = dynListRelationListListIndices(vals1[i], vals2[i], op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    case 1:
      for (i = 0; i < length; i++) {
	curlist = dynListRelationListListIndices(vals1[0], vals2[i], op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    case 2:
      for (i = 0; i < length; i++) {
	curlist = dynListRelationListListIndices(vals1[i], vals2[0], op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
      break;
    }
  }

  else if (DYN_LIST_DATATYPE(l1) == DF_LIST &&
	   DYN_LIST_DATATYPE(l2) != DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(l1);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);

    if (copymode > 0) {
      for (i = 0; i < DYN_LIST_N(l1); i++) {
	curlist = dynListRelationListListIndices(vals[i], l2, op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
    }
    else {
      DYN_LIST *arg = dfuCreateDynList(DYN_LIST_DATATYPE(l2),1);
      for (i = 0; i < DYN_LIST_N(l1); i++) {
	dfuResetDynList(arg);
	dynListCopyElement(l2, i, arg);
	curlist = dynListRelationListListIndices(vals[i], arg, op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      dfuFreeDynList(arg);
      return(list);
    }
  }
  else if (DYN_LIST_DATATYPE(l2) == DF_LIST &&
	   DYN_LIST_DATATYPE(l1) != DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(l2);
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, length);
    if (copymode > 0) {
      for (i = 0; i < DYN_LIST_N(l2); i++) {
	curlist = dynListRelationListListIndices(l1, vals[i], op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      return(list);
    }
    else {
      DYN_LIST *arg = dfuCreateDynList(DYN_LIST_DATATYPE(l1),1);
      for (i = 0; i < DYN_LIST_N(l2); i++) {
	dfuResetDynList(arg);
	dynListCopyElement(l1, i, arg);
	curlist = dynListRelationListListIndices(arg, vals[i], op);
	if (curlist) dfuMoveDynListList(list, curlist);
	else {
	  if (list) dfuFreeDynList(list);
	  return NULL;
	}
      }
      dfuFreeDynList(arg);
      return(list);
    }
  }


  if (DYN_LIST_DATATYPE(l1) != DYN_LIST_DATATYPE(l2)) {
    sametype = 0;
    if (DYN_LIST_DATATYPE(l1) == DF_FLOAT) {
      floatfirst = 1;		/* float first */
      flist = l1;
      nonflist = l2;
    }
    else if (DYN_LIST_DATATYPE(l2) == DF_FLOAT) {
      floatfirst = 0;		/* float second */
      flist = l2;
      nonflist = l1;
    }
    else {
      return(NULL);
    }
  }

  if (!length) 
    return(dfuCreateDynList(DF_LONG, length));
  else newvals = (int *) calloc(length, sizeof(int));

  if (DYN_LIST_DATATYPE(l1) == DF_STRING ||
      DYN_LIST_DATATYPE(l2) == DF_STRING) {
    char **vals1, **vals2;
    if (!sametype) return NULL;
    vals1 = (char **) DYN_LIST_VALS(l1);
    vals2 = (char **) DYN_LIST_VALS(l2);

    switch (copymode) {
    case 0:
      switch (op) {
      case DL_RELATION_AND:
      case DL_RELATION_OR:
	return NULL;
	break;
      case DL_RELATION_EQ:
	for (i = 0; i < length; i++) 
	  newvals[i] = !strcmp(vals1[i], vals2[i]);
	break;
      case DL_RELATION_NE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[i]) != 0;
	break;
      case DL_RELATION_LT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[i]) < 0;
	break;
      case DL_RELATION_LTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[i]) <= 0;
	break;
      case DL_RELATION_GT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[i]) > 0;
	break;
      case DL_RELATION_GTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[i]) <= 0;
	break;
      }
      break;
    case 1:
      switch (op) {
      case DL_RELATION_AND:
      case DL_RELATION_OR:
	return NULL;
	break;
      case DL_RELATION_EQ:
	for (i = 0; i < length; i++) 
	  newvals[i] = !strcmp(vals1[0], vals2[i]);
	break;
      case DL_RELATION_NE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[0], vals2[i]) != 0;
	break;
      case DL_RELATION_LT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[0], vals2[i]) < 0;
	break;
      case DL_RELATION_LTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[0], vals2[i]) <= 0;
	break;
      case DL_RELATION_GT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[0], vals2[i]) > 0;
	break;
      case DL_RELATION_GTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[0], vals2[i]) <= 0;
	break;
      }
      break;
    case 2:
      switch (op) {
      case DL_RELATION_AND:
      case DL_RELATION_OR:
	return NULL;
	break;
      case DL_RELATION_EQ:
	for (i = 0; i < length; i++) 
	  newvals[i] = !strcmp(vals1[i], vals2[0]);
	break;
      case DL_RELATION_NE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[0]) != 0;
	break;
      case DL_RELATION_LT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[0]) < 0;
	break;
      case DL_RELATION_LTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[0]) <= 0;
	break;
      case DL_RELATION_GT:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[0]) > 0;
	break;
      case DL_RELATION_GTE:
	for (i = 0; i < length; i++) 
	  newvals[i] = strcmp(vals1[i], vals2[0]) <= 0;
	break;
      }
      break;
    }

    for (i = 0; i < length; i++)
      if (newvals[i]) match_count++;
    list = dfuCreateDynList(DF_LONG, match_count);
    for (i = 0; i < length; i++)
      if (newvals[i]) dfuAddDynListLong(list, i);
    
    return(list);
  }

  if (sametype) {
    switch (DYN_LIST_DATATYPE(l1)) {
    case DF_LONG:
      {
	int *vals1 = (int *) DYN_LIST_VALS(l1);
	int *vals2 = (int *) DYN_LIST_VALS(l2);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_SHORT:
      {
	short *vals1 = (short *) DYN_LIST_VALS(l1);
	short *vals2 = (short *) DYN_LIST_VALS(l2);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	break;
	}
      }
      break;
    case DF_FLOAT:
      {
	float *vals1 = (float *) DYN_LIST_VALS(l1);
	float *vals2 = (float *) DYN_LIST_VALS(l2);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_CHAR:
      {
	char *vals1 = (char *) DYN_LIST_VALS(l1);
	char *vals2 = (char *) DYN_LIST_VALS(l2);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
	break;
      }
    default:
      return(NULL);
      break;
    }
  }
  else if (floatfirst) {	/* float list is arg1, nonfloat arg2 */
    switch (DYN_LIST_DATATYPE(nonflist)) {
    case DF_LONG:
      {
	float *vals1 = (float *) DYN_LIST_VALS(flist);
	int *vals2 = (int *) DYN_LIST_VALS(nonflist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_SHORT:
      {
	float *vals1 = (float *) DYN_LIST_VALS(flist);
	short *vals2 = (short *) DYN_LIST_VALS(nonflist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_CHAR:
      {
	float *vals1 = (float *) DYN_LIST_VALS(flist);
	char *vals2 = (char *) DYN_LIST_VALS(nonflist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    default:
      return(NULL);
      break;
    }
  }
  else  {			/* float list is arg2, nonfloat arg1 */
    switch (DYN_LIST_DATATYPE(nonflist)) {
    case DF_LONG:
      {
	int *vals1 = (int *) DYN_LIST_VALS(nonflist);
	float *vals2 = (float *) DYN_LIST_VALS(flist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_SHORT:
      {
	short *vals1 = (short *) DYN_LIST_VALS(nonflist);
	float *vals2 = (float *) DYN_LIST_VALS(flist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    case DF_CHAR:
      {
	char *vals1 = (char *) DYN_LIST_VALS(nonflist);
	float *vals2 = (float *) DYN_LIST_VALS(flist);
	switch (copymode) {
	case 0:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[i];
	    break;
	  }
	  break;
	case 1:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] && vals2[i];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] || vals2[i];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] == vals2[i];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] != vals2[i];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] < vals2[i];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] <= vals2[i];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] > vals2[i];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[0] >= vals2[i];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[0] % (int) vals2[i];
	    break;
	  }
	  break;
	case 2:
	  switch (op) {
	  case DL_RELATION_AND:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] && vals2[0];
	    break;
	  case DL_RELATION_OR:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] || vals2[0];
	    break;
	  case DL_RELATION_EQ:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] == vals2[0];
	    break;
	  case DL_RELATION_NE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] != vals2[0];
	    break;
	  case DL_RELATION_LT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] < vals2[0];
	    break;
	  case DL_RELATION_LTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] <= vals2[0];
	    break;
	  case DL_RELATION_GT:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] > vals2[0];
	    break;
	  case DL_RELATION_GTE:
	    for (i = 0; i < length; i++) newvals[i] = vals1[i] >= vals2[0];
	    break;
	  case DL_RELATION_MOD:
	    for (i = 0; i < length; i++) 
	      newvals[i] = (int) vals1[i] % (int) vals2[0];
	    break;
	  }
	  break;
	}
      }
      break;
    default:
      return(NULL);
      break;
    }
  }

  for (i = 0; i < length; i++)
    if (newvals[i]) match_count++;
  list = dfuCreateDynList(DF_LONG, match_count);
  for (i = 0; i < length; i++)
    if (newvals[i]) dfuAddDynListLong(list, i);

  return(list);
}

DYN_LIST *dynListNotList(DYN_LIST *dl)
{
  int i;
  int *newvals;
  DYN_LIST *newlist = NULL;
  if (!dl) return NULL;

  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    DYN_LIST *curlist;
    newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListNotList(vals[i]);
      if (!curlist) {
	dfuFreeDynList(newlist);
	return NULL;
      }
      dfuMoveDynListList(newlist, curlist);
    }
    return newlist;
  }

  else {

    /* If the list is empty, return another empty list */
    if (!DYN_LIST_N(dl)) {
      return dfuCreateDynList(DYN_LIST_DATATYPE(dl), 1);
    }

    newvals = (int *) calloc(DYN_LIST_N(dl), sizeof(int));
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG:
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	for (i = 0; i < DYN_LIST_N(dl); i++) newvals[i] = !vals[i];
	break;
      }
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	for (i = 0; i < DYN_LIST_N(dl); i++) newvals[i] = !vals[i];
	break;
      }
    case DF_CHAR:
      {
	char *vals = (char *) DYN_LIST_VALS(dl);
	for (i = 0; i < DYN_LIST_N(dl); i++) newvals[i] = !vals[i];
	break;
      }
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	for (i = 0; i < DYN_LIST_N(dl); i++) newvals[i] = !vals[i];
	break;
      }
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	for (i = 0; i < DYN_LIST_N(dl); i++) 
	  newvals[i] = !(vals[0] && vals[i][0]);
	break;
      }
    }
    
    newlist = dfuCreateDynListWithVals(DF_LONG, DYN_LIST_N(dl), newvals);
    return(newlist);
  }
}

int dynListTestVal(DYN_LIST *dl, int i)
{
  if (DYN_LIST_N(dl) <= i) return(-1);
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      return(vals[i] != 0);
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      return(vals[i] != 0);
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      return(vals[i] != 0);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      return(vals[i] != 0);
    }
    break;
  default:
    return(-1);
  }
}



DYN_LIST *dynListSumProdLists(DYN_LIST *dl, int op)
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
    curlist = dynListSumProdList(vals[i], op);
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
	    dfuAddDynListFloat(list, (float) val[0]);
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
	    dfuAddDynListLong(list, (int) val[0]);
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

DYN_LIST *dynListSumLists(DYN_LIST *dl)
{
  return dynListSumProdLists(dl, DL_SUM_LIST);
}

DYN_LIST *dynListProdLists(DYN_LIST *dl)
{
  return dynListSumProdLists(dl, DL_PROD_LIST);
}

/*
 * dynListSumInt64 -- exact sum of an integer-typed flat list.
 *
 * dynListSumProdList has to return a DYN_LIST, and the widest numeric list
 * type is float, so a large integer sum comes back as a float32
 * approximation. For the scalar dl_sum case the Tcl layer can do better:
 * Tcl carries 64-bit integers natively, so hand it the exact value.
 *
 * Returns 1 and fills *out when dl is a flat integer-typed list; 0 otherwise,
 * in which case the caller should fall back to the DYN_LIST path.
 */
int dynListSumInt64(DYN_LIST *dl, long long *out)
{
  int i;
  long long acc = 0;

  if (!dl || DYN_LIST_N(dl) == 0) return 0;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) acc += vals[i];
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) acc += vals[i];
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) acc += vals[i];
    }
    break;
  default:
    return 0;
  }
  *out = acc;
  return 1;
}

/*
 * dynListSumDouble -- sum of a flat float list, accumulated and returned in
 * double. Same reasoning as dynListSumInt64: the widest list type is float,
 * so returning through a DYN_LIST costs precision the caller does not have
 * to accept when it is handing the value to Tcl.
 */
int dynListSumDouble(DYN_LIST *dl, double *out)
{
  int i;
  double acc = 0.0;
  float *vals;

  if (!dl || DYN_LIST_N(dl) == 0) return 0;
  if (DYN_LIST_DATATYPE(dl) != DF_FLOAT) return 0;

  vals = (float *) DYN_LIST_VALS(dl);
  for (i = 0; i < DYN_LIST_N(dl); i++)
    if (!isnan(vals[i])) acc += vals[i];
  *out = acc;
  return 1;
}

DYN_LIST *dynListSumProdList(DYN_LIST *dl, int op)
{
  int i;
  DYN_LIST *retlist = NULL;
  if(!dl || DYN_LIST_N(dl) == 0) return NULL;
  if (op != DL_SUM_LIST && op != DL_PROD_LIST) return NULL;
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      long long retval;
      /* Accumulate wide: an int accumulator silently wrapped, so summing a
	 long list of ints returned a negative number rather than a large
	 one. */
      if (op == DL_SUM_LIST) {
	retval = 0;
	for (i = 0; i < DYN_LIST_N(dl); i++) retval += vals[i];
      }
      else {			/* prod */
	retval = 1;
	for (i = 0; i < DYN_LIST_N(dl); i++) retval *= vals[i];
      }
      if (retval >= INT_MIN && retval <= INT_MAX) {
	retlist = dfuCreateDynList(DF_LONG, 1);
	dfuAddDynListLong(retlist, (int) retval);
      }
      else {
	/* Too big for an int list. A float is approximate but is vastly
	   closer than a wrapped int, and there is no wider list type. */
	retlist = dfuCreateDynList(DF_FLOAT, 1);
	dfuAddDynListFloat(retlist, (float) retval);
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      long long retval;
      /* Accumulate wide: an int accumulator silently wrapped, so summing a
	 long list of ints returned a negative number rather than a large
	 one. */
      if (op == DL_SUM_LIST) {
	retval = 0;
	for (i = 0; i < DYN_LIST_N(dl); i++) retval += vals[i];
      }
      else {			/* prod */
	retval = 1;
	for (i = 0; i < DYN_LIST_N(dl); i++) retval *= vals[i];
      }
      if (retval >= INT_MIN && retval <= INT_MAX) {
	retlist = dfuCreateDynList(DF_LONG, 1);
	dfuAddDynListLong(retlist, (int) retval);
      }
      else {
	/* Too big for an int list. A float is approximate but is vastly
	   closer than a wrapped int, and there is no wider list type. */
	retlist = dfuCreateDynList(DF_FLOAT, 1);
	dfuAddDynListFloat(retlist, (float) retval);
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      double retval;		/* accumulate in double: a float accumulator
				   loses whole addends once the running sum
				   passes ~2^24 */
      retlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), 1);
      if (op == DL_SUM_LIST) {
	retval = 0.0;
	for (i = 0; i < DYN_LIST_N(dl); i++) retval += vals[i];
      }
      else {			/* prod */
	retval = 1.0;
	for (i = 0; i < DYN_LIST_N(dl); i++) retval *= vals[i];
      }
      dfuAddDynListFloat(retlist, (float) retval);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      long long retval;
      /* Accumulate wide: an int accumulator silently wrapped, so summing a
	 long list of ints returned a negative number rather than a large
	 one. */
      if (op == DL_SUM_LIST) {
	retval = 0;
	for (i = 0; i < DYN_LIST_N(dl); i++) retval += vals[i];
      }
      else {			/* prod */
	retval = 1;
	for (i = 0; i < DYN_LIST_N(dl); i++) retval *= vals[i];
      }
      if (retval >= INT_MIN && retval <= INT_MAX) {
	retlist = dfuCreateDynList(DF_LONG, 1);
	dfuAddDynListLong(retlist, (int) retval);
      }
      else {
	/* Too big for an int list. A float is approximate but is vastly
	   closer than a wrapped int, and there is no wider list type. */
	retlist = dfuCreateDynList(DF_FLOAT, 1);
	dfuAddDynListFloat(retlist, (float) retval);
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST *curlist, *curresult;
      DYN_LIST **vals;
      vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      
      /*
       * Initialize the curresult to either 0 (sum) or 1 (prod) and
       * start off as int.  Only promote to float if necessary
       */
      curresult = dfuCreateDynList(DF_LONG, 1);
      if (op == DL_SUM_LIST) {
	dfuAddDynListLong(curresult, 0);
      }
      else {
	dfuAddDynListLong(curresult, 1);
      }

      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListSumProdList(vals[i], op);
	/* 
	 * The curlist and curresult are either NULL, or 
	 * of type DF_LONG or DF_FLOAT
	 * so now we combine the two, promoting to float if necessary
	 */
	if (!curlist) continue;
	else if (DYN_LIST_DATATYPE(curresult) == DF_LONG &&
		 DYN_LIST_DATATYPE(curlist) == DF_LONG) {
	  int *v0 = (int *) DYN_LIST_VALS(curresult);
	  int *v1 = (int *) DYN_LIST_VALS(curlist);
	  if (op == DL_SUM_LIST) {
	    v0[0] += v1[0];	/* add to curresult */
	    dfuFreeDynList(curlist);
	  }
	  if (op == DL_PROD_LIST) {
	    v0[0] *= v1[0];	/* multiply curresult by curlist */
	    dfuFreeDynList(curlist);
	  }
	}
	else if (DYN_LIST_DATATYPE(curresult) == DF_FLOAT &&
		 DYN_LIST_DATATYPE(curlist) == DF_LONG) {
	  float *v0 = (float *) DYN_LIST_VALS(curresult);
	  int *v1 = (int *) DYN_LIST_VALS(curlist);
	  if (op == DL_SUM_LIST) {
	    v0[0] += v1[0];	/* add to curresult */
	    dfuFreeDynList(curlist);
	  }
	  if (op == DL_PROD_LIST) {
	    v0[0] *= v1[0];	/* multiply curresult by curlist */
	    dfuFreeDynList(curlist);
	  }
	}

	/* 
	 * This one's a little trickier, because we interchange
	 * the current result with the current list in order to promote
	 * to float
	 */
	else if (DYN_LIST_DATATYPE(curresult) == DF_LONG &&
		 DYN_LIST_DATATYPE(curlist) == DF_FLOAT) {
	  DYN_LIST *tmp = curresult;
	  float *v0;
	  int *v1;
	  curresult = curlist;	/* swap them (these ops are commutative) */
	  curlist = tmp;
	  v0 = (float *) DYN_LIST_VALS(curresult);
	  v1 = (int *) DYN_LIST_VALS(curlist);
	  if (op == DL_SUM_LIST) {
	    v0[0] += v1[0];	/* add to curresult */
	    dfuFreeDynList(curlist);
	  }
	  if (op == DL_PROD_LIST) {
	    v0[0] *= v1[0];	/* multiply curresult by curlist */
	    dfuFreeDynList(curlist);
	  }
	}
	else if (DYN_LIST_DATATYPE(curresult) == DF_FLOAT &&
		 DYN_LIST_DATATYPE(curlist) == DF_FLOAT) {
	  float *v0 = (float *) DYN_LIST_VALS(curresult);
	  float *v1 = (float *) DYN_LIST_VALS(curlist);
	  if (op == DL_SUM_LIST) {
	    v0[0] += v1[0];	/* add to curresult */
	    dfuFreeDynList(curlist);
	  }
	  if (op == DL_PROD_LIST) {
	    v0[0] *= v1[0];	/* multiply curresult by curlist */
	    dfuFreeDynList(curlist);
	  }
	}
      }
      retlist = curresult;
    }
    break;
  default:
    return NULL;
  }
  return(retlist);
}

DYN_LIST *dynListCumSumProdList(DYN_LIST *dl, int op)
{
  int i;
  DYN_LIST *retlist = NULL;

  if(!dl) return NULL;
  if (DYN_LIST_N(dl) == 0) 
    return dfuCreateDynList(DYN_LIST_DATATYPE(dl), 1);

  if (op != DL_SUM_LIST && op != DL_PROD_LIST) return NULL;
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      int retval;
      retlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
      if (op == DL_SUM_LIST) {
	retval = 0;
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  retval += vals[i];
	  dfuAddDynListLong(retlist, retval);
	}
      }
      else {			/* prod */
	retval = 1;
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  retval *= vals[i];
	  dfuAddDynListLong(retlist, retval);
	}
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      int retval;
      retlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
      if (op == DL_SUM_LIST) {
	retval = 0;
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  retval += vals[i];
	  dfuAddDynListLong(retlist, retval);
	}
      }
      else {			/* prod */
	retval = 1;
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  retval *= vals[i];
	  dfuAddDynListLong(retlist, retval);
	}
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      float retval;
      retlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      if (op == DL_SUM_LIST) {
	retval = 0.0;
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  retval += vals[i];
	  dfuAddDynListFloat(retlist, retval);
	}
      }
      else {			/* prod */
	retval = 1.0;
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  retval *= vals[i];
	  dfuAddDynListFloat(retlist, retval);
	}
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      int retval;
      retlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
      if (op == DL_SUM_LIST) {
	retval = 0;
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  retval += vals[i];
	  dfuAddDynListLong(retlist, retval);
	}
      }
      else {			/* prod */
	retval = 1;
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  retval *= vals[i];
	  dfuAddDynListLong(retlist, retval);
	}
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curcum;
      retlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curcum = dynListCumSumProdList(vals[i], op);
	if (!curcum) {
	  dfuFreeDynList(retlist);
	  return NULL;
	}
	dfuMoveDynListList(retlist, curcum);
      }
    }
    break;
  default:
    return NULL;
  }
  return(retlist);
}

DYN_LIST *dynListBSumLists(DYN_LIST *dl)
{
  int i;
  DYN_LIST *list;
  DYN_LIST **vals;
  
  if (!dl) return(NULL);
  
  vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
    return(dynListSumProdList(dl, DL_SUM_LIST));
  }
  else {
    DYN_LIST *curlist;
    list = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
    vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = dynListBSumLists(vals[i]);
      if (!curlist) {
	dfuFreeDynList(list);
	return NULL;
      }
      dfuMoveDynListList(list, curlist);
    }
  }
  return(list);
}


static float fsign(float x)
{
  if (x < 0.) return -1.;
  else if (x > 0.) return 1.;
  else return 0.;
}

static int sign(int x) 
{
  if (x < 0) return -1;
  else if (x > 0) return 1;
  else return 0;

}

DYN_LIST *dynListSignList(DYN_LIST *dl)
{
  int i;
  DYN_LIST *signs;
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      signs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuAddDynListLong(signs, sign(vals[i]));
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      signs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuAddDynListShort(signs, (short) sign(vals[i]));
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      signs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuAddDynListFloat(signs, fsign(vals[i]));
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      signs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuAddDynListChar(signs, (char) sign(vals[i]));
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *cursign;
      signs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	cursign = dynListSignList(vals[i]);
	if (!cursign) {
	  dfuFreeDynList(signs);
	  return NULL;
	}
	dfuMoveDynListList(signs, cursign);
      }
    }
    break;
  default:
    return(NULL);
  }
  return(signs);
}

DYN_LIST *dynListNegateList(DYN_LIST *dl)
{
  int i;
  DYN_LIST *negs;
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      negs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuAddDynListLong(negs, -vals[i]);
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      negs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuAddDynListShort(negs, (short) -vals[i]);
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      negs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuAddDynListFloat(negs, -vals[i]);
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      negs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	dfuAddDynListChar(negs, (char) -vals[i]);
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curneg;
      negs = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curneg = dynListNegateList(vals[i]);
	if (!curneg) {
	  dfuFreeDynList(negs);
	  return NULL;
	}
	dfuMoveDynListList(negs, curneg);
      }
    }
    break;
  default:
    return(NULL);
  }
  return(negs);
}

static DYN_LIST *dynListConvModeList(DYN_LIST *dl, DYN_LIST *kernel, int mode)
{
  int i, j, kwidth, hwidth, length, stop;
  DYN_LIST *result, *dlf = NULL, *kernelf = NULL, **subkernels;
  double sum;
  float *v, *vcur, *k, *kcenter;
  

  /* The original convolve func is mode 0, and doesn't recurse deep kernels */
  if (mode == 0) { 
    if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curconv;
      result = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (DYN_LIST_DATATYPE(vals[i]) != DF_LIST &&
	    DYN_LIST_DATATYPE(kernel) == DF_LIST) {
	  subkernels = DYN_LIST_VALS(kernel);
	  if (DYN_LIST_N(kernel) == 1) 
	    curconv = dynListConvList(vals[i], subkernels[0]);
	  else if (DYN_LIST_N(kernel) == DYN_LIST_N(dl))
	    curconv = dynListConvList(vals[i], subkernels[i]);
	  else { 
	    dfuFreeDynList(result);
	    return NULL;
	  }
	}
	else {
	  curconv = dynListConvList(vals[i], kernel);
	}
	if (!curconv) {
	  dfuFreeDynList(result);
	  return NULL;
	}
	dfuMoveDynListList(result, curconv);
      }
      return(result);
    }
  }
  /* New, deeper recursion */
  else {
    if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curconv;
      result = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (DYN_LIST_DATATYPE(vals[i]) != DF_LIST &&
	    DYN_LIST_DATATYPE(kernel) == DF_LIST) {
	  subkernels = DYN_LIST_VALS(kernel);
	  if (DYN_LIST_N(kernel) == 1) 
	    curconv = dynListConvList(vals[i], subkernels[0]);
	  else if (DYN_LIST_N(kernel) == DYN_LIST_N(dl))
	    curconv = dynListConvList(vals[i], subkernels[i]);
	  else { 
	    dfuFreeDynList(result);
	    return NULL;
	  }
	}
	else if (DYN_LIST_DATATYPE(vals[i]) == DF_LIST &&
		 DYN_LIST_DATATYPE(kernel) == DF_LIST) {
	  subkernels = DYN_LIST_VALS(kernel);
	  if (DYN_LIST_N(kernel) == 1) 
	    curconv = dynListConvList(vals[i], subkernels[0]);
	  else if (DYN_LIST_N(kernel) == DYN_LIST_N(dl))
	    curconv = dynListConvList(vals[i], subkernels[i]);
	  else { 
	    dfuFreeDynList(result);
	    return NULL;
	  }
	}
	else {
	  curconv = dynListConvList(vals[i], kernel);
	}
	if (!curconv) {
	  dfuFreeDynList(result);
	  return NULL;
	}
	dfuMoveDynListList(result, curconv);
      }
      return(result);
    }
  }

  /* Return original list, if kernel is empty list */
  if (DYN_LIST_N(kernel) == 0) {
    return dfuCopyDynList(dl);
  }

  if (DYN_LIST_DATATYPE(dl) != DF_FLOAT) {
    dlf = dynListConvertList(dl, DF_FLOAT);
  }
  if (DYN_LIST_DATATYPE(kernel) != DF_FLOAT) {
    kernelf = dynListConvertList(kernel, DF_FLOAT);
  }
  
  /* Make sure the data is as long or longer than the kernel */
  if (DYN_LIST_N(dl) <= DYN_LIST_N(kernel)) return NULL;
  
  /* For now, insist that kernel length is odd */
  if (!(DYN_LIST_N(kernel) % 2)) return NULL;

  result = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dl));
  length = DYN_LIST_N(dl);

  if (dlf)
    v = (float *) DYN_LIST_VALS(dlf);
  else
    v = (float *) DYN_LIST_VALS(dl);

  if (kernelf) 
    k = (float *) DYN_LIST_VALS(kernelf);
  else 
    k = (float *) DYN_LIST_VALS(kernel);

  kwidth = DYN_LIST_N(kernel);
  hwidth = kwidth/2;
  kcenter = k + hwidth;		/* so kernel can be indexed with neg numbers */

  /* The beginning of the loop, which will check for lower bound conditions */
  for (i = 0; i < hwidth; i++) {
    sum = 0.0;
    vcur = v+i;			/* current data center */
    for (j = -hwidth; j <= hwidth; j++) {
      if ((j+i) >= 0) sum += kcenter[j]*vcur[j];
    }
    dfuAddDynListFloat(result, (float) sum);
  }

  /* The middle of the loop which doesn't need to check */
  stop = length-hwidth;
  for (i = hwidth; i < stop; i++) {
    sum = 0.0;
    vcur = v+i;			/* current data center */
    for (j = -hwidth; j <= hwidth; j++) 
      sum += kcenter[j]*vcur[j];
    dfuAddDynListFloat(result, (float) sum);
  }

  /* The end of the loop which checks upper boundary conditions */
  for (i = stop; i < length; i++) {
    sum = 0.0;
    vcur = v+i;			/* current data center */
    for (j = -hwidth; j <= hwidth; j++) {
      if ((j+i) < length) sum += kcenter[j]*vcur[j];
    }
    dfuAddDynListFloat(result, (float) sum);
  }

  if (kernelf) dfuFreeDynList(kernelf);
  if (dlf) dfuFreeDynList(dlf);

  return(result);
}

DYN_LIST *dynListConvList(DYN_LIST *dl, DYN_LIST *kernel)
{
  return dynListConvModeList(dl, kernel, 0);
}

DYN_LIST *dynListConvList2(DYN_LIST *dl, DYN_LIST *kernel)
{
  return dynListConvModeList(dl, kernel, 1);
}

