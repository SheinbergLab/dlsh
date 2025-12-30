/*****************************************************************************
 *
 * NAME
 *      tcl_dl.c
 *
 * PURPOSE
 *      Provide code for creating/manipulating DynGroups and DynLists 
 *
 * AUTHOR
 *      DLS, JUL95 - ???!
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <string.h>
#include <math.h>

#ifdef WIN32
#pragma warning (disable:4996)
#endif

#if defined(SUN4) || defined(LYNX)
#include <unistd.h>
#endif

#ifdef __QNX__
  #include <unix.h>
#endif

#include <stdint.h>
#include <tcl.h>
#include <df.h>
#include <dynio.h>
#include "dfana.h"
#include <labtcl.h>
#include "tcl_dl.h"
#include "dgmsgpack.h"
#include "dgarrow.h"
#include <jansson.h>

#include <zlib.h>

#include <utilc.h>

static const char *DLSH_ASSOC_DATA_KEY = "dlsh";

/* to protect non thread safe dynio operations */
static Tcl_Mutex dgBufferMutex;

/* to save as JSON */
extern json_t *dg_to_json(DYN_GROUP *dg);
extern json_t *dg_element_to_json(DYN_GROUP *dg, int element);
extern json_t *dl_to_json(DYN_LIST *dl);
extern json_t *dl_element_to_json(DYN_LIST *dl, int element);
extern json_t *dg_to_hybrid_json(DYN_GROUP *dg);

/*
 * Callback function for deleted temporary lists 
 */
static char *tclDeleteLocalDynList(ClientData clientData, Tcl_Interp *interp,
				   char *name1, char *name2, int flags);


static int tclFindDynListParent(Tcl_Interp *interp, char *name, DYN_LIST **dl,
				DYN_LIST ***parent, int *index);

static int tclFindSubDynListParent(Tcl_Interp *interp, char *dlname, 
				   DYN_LIST *dll, 
				   char *sel, DYN_LIST **dl, 
				   DYN_LIST ***parent, int *index);

static DYN_LIST *dynListFileString(Tcl_Interp *interp, DYN_LIST *dl, int op);


/*
 * some regexp code adapted from tclCmdMZ.c --
 *
 *	This file contains the top-level command routines for most of the Tcl
 *	built-in commands whose names begin with the letters M to Z. It
 *	contains only commands in the generic core (i.e. those that don't
 *	depend much upon UNIX facilities).
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 Scriptics Corporation.
 * Copyright (c) 2002 ActiveState Corporation.
 * Copyright (c) 2003-2009 Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


static DYN_LIST *dynListRegexpString(Tcl_Interp *interp, DYN_LIST *dl, 
				     Tcl_RegExp regexp, int matchonly);
static DYN_LIST *dynListScanString(Tcl_Interp *interp, DYN_LIST *dl, 
				   int base, int scan_int);


/*
 * Defined enums used as clientData for tcl functions
 */
enum DL_APPEND_MODE  { DL_APPEND, DL_PREPEND, DL_INSERT, DL_SPLICE_BEFORE,
                       DL_SPLICE_AFTER };
enum DL_GETPUT_MODE  { DL_GET, DL_PUT, DL_FIRST, DL_LAST, DL_PICKONE };
enum DL_CHOOSE_TYPES { DL_SELECT, DL_CHOOSE };
enum DL_CLEAN_TYPES  { DL_CLEAN_TEMPS, DL_CLEAN_RETS };
enum DG_DUMP_TYPES   { DG_DUMP, DG_DUMP_LIST_NAMES, DG_LISTNAMES, 
		       DG_TCL_LISTNAMES };
enum DG_COMPRESSED_TYPES   { DG_UNCOMPRESSED, DG_COMPRESSED };
enum DG_APPEND_TYPES { DG_MOVE, DG_COPY };
enum DL_TYPE_INFO    { DL_DATATYPE, DL_IS_MATRIX };
enum DL_DUMP_TYPES   { 
  DL_DUMP, DL_DUMP_AS_ROW, DL_DUMP_MATRIX, DL_DUMP_MATRIX_IN_COLS, 
  DL_TO_TCL_LIST, DL_DUMPBYTES, DL_DUMPBYTES_AS };
enum DL_DUMP_FORMATS {
  DL_DUMP_USHORT
};
enum DL_CONSERS      { DL_COMBINE, DL_CONCAT, DL_INCREMENT, DL_INTERLEAVE };
enum DL_ARITH_TYPES  { DL_ADD, DL_SUBTRACT, DL_MULTIPLY, DL_DIVIDE,
		      DL_DIVIDE_ROWS, DL_MULTIPLY_ROWS, DL_POW, DL_CONV,
		       DL_CONV2, DL_ATAN2, DL_FMOD};
enum DL_LISTLIST_TYPES { DL_RECIPROCAL, DL_TRANSPOSE, DL_SORT, DL_UNIQUE,
			   DL_LENGTHS, DL_UNIQUE_NO_SORT, DL_SORT_INDICES, 
			   DL_RANK, DL_COLLAPSE, DL_DIFF, DL_UNPACK, DL_NEGATE,
			   DL_CUMSUM, DL_CUMPROD, DL_INDICES,
			   DL_SHIFTCYCLE, DL_SHIFT, DL_LLENGTH, DL_RECODE,
                           DL_DEEP_UNPACK, DL_BSORT, DL_BSORT_INDICES,
                           DL_SIGN, DL_ZERO_CROSSING, DL_INDEX_LIST,
                           DL_FILE_ROOT, DL_FILE_TAIL, DL_FILE_DIR,
                           DL_FILE_EXT, DL_FILE_BASE, DL_FIRST_INDEX_LIST,
			   DL_LAST_INDEX_LIST, DL_PACK, DL_DEEP_PACK,
			 DL_IDIFF, DL_SUBSHIFT, DL_UNPACK_LISTS,
			 DL_RECODE_WITH_TIES, DL_BSHIFT, DL_BSHIFTCYCLE,
                         DL_TRANSPOSE_AT, DL_CUT, DL_GRADIENT};
enum DL_REPLACERS    { DL_REPLACE, DL_REPLACE_BY_INDEX };
enum DL_REPEATERS    { DL_REPEAT, DL_REPLICATE, DL_REPEAT_ELTS };
enum DL_PERMUTERS    { DL_PERMUTE, DL_REVERSE, DL_REVERSE_ALL, DL_BREVERSE };
enum DL_RECURSION    { DL_SINGLE, DL_RECURSIVE, DL_REC2 };
enum DL_LIST_SORTERS { DL_BYLIST, DL_BYLISTS,  DL_CROSS, DL_UNIQUE_CROSS,
		       DL_BYSELECTED };
enum DL_REDUCERS     { DL_LENGTH, DL_MIN, DL_MAX, DL_SUM, DL_MEAN, DL_STD,
		       DL_MAX_INDEX, DL_MIN_INDEX, DL_HMEAN, DL_HSTD,
		       DL_VAR, DL_HVAR, DL_PROD, DL_DEPTH, DL_BMEAN, 
		       DL_BSUMS, DL_BSTD, DL_MIN_POSITIONS, DL_MAX_POSITIONS, 
		       DL_MEAN_LIST, DL_SUMCOLS_LIST, 
		       DL_BMIN, DL_BMAX };
enum DL_GENERATOR    { DL_ZEROS, DL_ONES, DL_URAND, DL_ZRAND, DL_RANDFILL,
     		       DL_RANDCHOOSE, DL_IRAND, DL_GAUSSIAN_2D,
                       DL_SHUFFLE};
enum DL_SERIES_MAKERS { DL_SERIES, DL_FROMTO };
enum DL_CONVERT      { DL_FLOAT, DL_INT, DL_CHAR, DL_SHORT };
enum DL_FINDERS      { DL_FIND, DL_FIND_ALL, DL_COUNT_OCCURENCES, 
		       DL_FIND_PATTERNS, DL_FIND_INDICES };
enum DL_COMPARATORS  { DL_EQUAL_TO, DL_LESS_THAN, DL_GREATER_THAN, 
		       DL_LTE, DL_GTE, DL_AND, DL_OR, DL_MOD, 
		       DL_NOT_EQUAL_TO, DL_STRINGMATCH, DL_ONEOF };

enum DL_REGEXPS       { DL_REGEXP, DL_REGMATCH };
enum DL_SCANNERS      { DL_SCAN_INT, DL_SCAN_FLOAT, DL_SCAN_BINARY,
			DL_SCAN_OCTAL, DL_SCAN_HEX };

enum DG_DELETERS     { DG_DELETE_NORMAL, DG_DELETE_TEMPS };
enum DG_TOFROMSTRING { DG_TOFROM_BINARY, DG_TOFROM_BASE64, DG_TOFROM_JSON, DG_TOFROM_JSON_HYBRID,
		       DG_TOFROM_MSGPACK_FILE, DG_TOFROM_MSGPACK_DATA,
		       DG_TOFROM_MSGPACK_HYBRID_FILE, DG_TOFROM_MSGPACK_HYBRID_DATA,
   	           DG_TOFROM_ARROW_FILE, DG_TOFROM_ARROW_DATA  };
enum DL_TOFROMSTRING { DL_TOFROM_BINARY, DL_TOFROM_BASE64, DL_TOFROM_JSON };
/*****************************************************************************
 *                           TCL Bound Functions 
 *****************************************************************************/

static int tclDynGroupDir             (ClientData, Tcl_Interp *, int, char **);
static int tclDynGroupExists          (ClientData, Tcl_Interp *, int, char **);
static int tclCreateDynGroup          (ClientData, Tcl_Interp *, int, char **);
static int tclCopyDynGroup            (ClientData, Tcl_Interp *, int, char **);
static int tclRenameDynGroup          (ClientData, Tcl_Interp *, int, char **);
static int tclAppendDynGroup          (ClientData, Tcl_Interp *, int, char **);
static int tclWriteDynGroup           (ClientData, Tcl_Interp *, int, char **);
static int tclReadDynGroup            (ClientData, Tcl_Interp *, int, char **);
static int tclDeleteDynGroup          (ClientData, Tcl_Interp *, int, char **);
static int tclRemoveDynGroupList      (ClientData, Tcl_Interp *, int, char **);
static int tclAddNewListDynGroup      (ClientData, Tcl_Interp *, int, char **);
static int tclAddExistingListDynGroup (ClientData, Tcl_Interp *, int, char **);
static int tclResetDynGroup           (ClientData, Tcl_Interp *, int, char **);
static int tclDumpDynGroup            (ClientData, Tcl_Interp *, int, char **);
static int tclSetFormat               (ClientData, Tcl_Interp *, int, char **);

static int tclDynListDir              (ClientData, Tcl_Interp *, int, char **);
static int tclDynListExists           (ClientData, Tcl_Interp *, int, char **);
static int tclDynListSublist          (ClientData, Tcl_Interp *, int, char **);
static int tclDynListDatatype         (ClientData, Tcl_Interp *, int, char **);
static int tclCreateDynList           (ClientData, Tcl_Interp *, int, char **);
static int tclSetDynList              (ClientData, Tcl_Interp *, int, char **);
static int tclLocalDynList            (ClientData, Tcl_Interp *, int, char **);
static int tclReturnDynList           (ClientData, Tcl_Interp *, int, char **);
static int tclGetPutDynList           (ClientData, Tcl_Interp *, int, char **);
static int tclDeleteDynList           (ClientData, Tcl_Interp *, int, char **);
static int tclDeleteTraceDynList      (ClientData, Tcl_Interp *, int, char **);
static int tclRenameDynList           (ClientData, Tcl_Interp *, int, char **);
static int tclResetDynList            (ClientData, Tcl_Interp *, int, char **);
static int tclCleanDynList            (ClientData, Tcl_Interp *, int, char **);
static int tclPushTmpList             (ClientData, Tcl_Interp *, int, char **);
static int tclPopTmpList              (ClientData, Tcl_Interp *, int, char **);
static int tclReplaceDynList          (ClientData, Tcl_Interp *, int, char **);
static int tclSelectDynList           (ClientData, Tcl_Interp *, int, char **);
static int tclAppendDynList           (ClientData, Tcl_Interp *, int, char **);
static int tclCompareDynList          (ClientData, Tcl_Interp *, int, char **);
static int tclCompareDynListIndices   (ClientData, Tcl_Interp *, int, char **);
static int tclFindSublist             (ClientData, Tcl_Interp *, int, char **);
static int tclDynListNot              (ClientData, Tcl_Interp *, int, char **);
static int tclConsDynList             (ClientData, Tcl_Interp *, int, char **);
static int tclDumpDynList             (ClientData, Tcl_Interp *, int, char **);
static int tclConvertDynList          (ClientData, Tcl_Interp *, int, char **);
static int tclUnsignedConvertDynList  (ClientData, Tcl_Interp *, int, char **);
static int tclArithDynList            (ClientData, Tcl_Interp *, int, char **);
static int tclListFromList            (ClientData, Tcl_Interp *, int, char **);
static int tclReshapeList             (ClientData, Tcl_Interp *, int, char **);
static int tclRestructureList         (ClientData, Tcl_Interp *, int, char **);
static int tclPermuteDynList          (ClientData, Tcl_Interp *, int, char **);
static int tclRepList                 (ClientData, Tcl_Interp *, int, char **);
static int tclCountLists              (ClientData, Tcl_Interp *, int, char **);
static int tclSortByLists             (ClientData, Tcl_Interp *, int, char **);
static int tclReduceList              (ClientData, Tcl_Interp *, int, char **);
static int tclReduceLists             (ClientData, Tcl_Interp *, int, char **);
static int tclHistLists               (ClientData, Tcl_Interp *, int, char **);
static int tclHistBins                (ClientData, Tcl_Interp *, int, char **);
static int tclGenerateDynList         (ClientData, Tcl_Interp *, int, char **);
static int tclSeries                  (ClientData, Tcl_Interp *, int, char **);
static int tclSdfLists                (ClientData, Tcl_Interp *, int, char **);
static int tclSdfListsRecursive       (ClientData, Tcl_Interp *, int, char **);
static int tclParzenLists             (ClientData, Tcl_Interp *, int, char **);
static int tclDLHelp                  (ClientData, Tcl_Interp *, int, char **);
static int tclMathFuncOneArg          (ClientData, Tcl_Interp *, int, char **);
static int tclFillList                (ClientData, Tcl_Interp *, int, char **);
static int tclTempName                (ClientData, Tcl_Interp *, int, char **);
static int tclDgTempName              (ClientData, Tcl_Interp *, int, char **);
static int tclSetMatherr              (ClientData, Tcl_Interp *, int, char **);
static int tclLoadPackage             (ClientData, Tcl_Interp *, int, char **);
static int tclDateToDays              (ClientData, Tcl_Interp *, int, char **); 
static int tclDaysToDate              (ClientData, Tcl_Interp *, int, char **); 
static int tclNoOp                    (ClientData, Tcl_Interp *, int, char **);
static int tclSetRandomSeed           (ClientData, Tcl_Interp *, int, char **);

static TCL_COMMANDS DLcommands[] = {
  { "dg_exists",           tclDynGroupExists,     NULL, 
      "returns 1 if dynGroup exists" },
  { "dg_dir",              tclDynGroupDir,        NULL, 
      "returns list of current dynGroups" },
  { "dg_create",           tclCreateDynGroup,     NULL, 
      "create a dynGroup" },
  { "dg_copy",             tclCopyDynGroup,       NULL, 
      "copy a dynGroup" },
  { "dg_rename",           tclRenameDynGroup,     NULL, 
      "rename a dynGroup or dynGroup:list" },
  { "dg_append",           tclAppendDynGroup,     NULL, 
      "append dg2 onto dg1" },
  { "dg_write",            tclWriteDynGroup,      NULL, 
      "write a dynGroup" },
  { "dg_read",             tclReadDynGroup,       NULL, 
      "read a dynGroup" },
  { "dg_delete",           tclDeleteDynGroup,     (void *) DG_DELETE_NORMAL, 
      "delete a dynGroup" },
  { "dg_clean",            tclDeleteDynGroup,     (void *) DG_DELETE_TEMPS, 
      "delete temporary dyngroups" },
  { "dg_reset",            tclResetDynGroup,      NULL, 
      "reset a dynGroup" },
  { "dg_remove",           tclRemoveDynGroupList, NULL, 
      "remove a list from a dynGroup" },
  { "dg_addNewList",       tclAddNewListDynGroup, NULL, 
      "add a new list to a dynGroup" },
  { "dg_addExistingList",  tclAddExistingListDynGroup, (void *) DG_MOVE, 
      "add an existing list to a dynGroup" },
  { "dg_copyExistingList", tclAddExistingListDynGroup, (void *) DG_COPY, 
      "add an existing list to a dynGroup" },
  { "dg_dump",             tclDumpDynGroup,       (void *) DG_DUMP, 
      "dump a dynGroup" },
  { "dg_dumpListNames",    tclDumpDynGroup,       (void *) DG_DUMP_LIST_NAMES, 
      "dump the names of the lists in a dynGroup" },
  { "dg_listnames",        tclDumpDynGroup,       (void *) DG_LISTNAMES, 
    "make a dynlist of lists in a dynGroup" },
  { "dg_tclListnames",     tclDumpDynGroup,       (void *) DG_TCL_LISTNAMES, 
    "make a tcllist of lists in a dynGroup" },

  { "dg_tmpname",          tclDgTempName,           NULL,
      "return an unused group name" },
  { "dg_tempname",         tclDgTempName,           NULL,
      "return an unused group name" },



  { "dl_exists",           tclDynListExists,      NULL, 
      "returns 1 if dynList exists" },
  { "dl_sublist",          tclDynListSublist,     NULL, 
      "returns 1 if dynList is a sublist" },
  { "dl_dir",              tclDynListDir,         NULL, 
      "returns list of existing dynLists" },
  { "dl_create",           tclCreateDynList,      NULL, 
      "create a dynList" },
  { "dl_clist",           tclCreateDynList,       (void *) DF_CHAR, 
      "create a char dynList" },
  { "dl_ilist",           tclCreateDynList,       (void *) DF_LONG, 
      "create an int dynList" },
  { "dl_flist",           tclCreateDynList,       (void *) DF_FLOAT, 
      "create an int dynList" },
  { "dl_slist",           tclCreateDynList,       (void *) DF_STRING, 
      "create a string dynList" },
  { "dl_llist",           tclCreateDynList,       (void *) DF_LIST, 
      "create a dynList of lists" },
  { "dl_datatype",         tclDynListDatatype,    (void *) DL_DATATYPE, 
      "returns datatype of dynList" },
  { "dl_isMatrix",         tclDynListDatatype,    (void *) DL_IS_MATRIX, 
      "returns 1 if is matrix" },
  { "dl_set",              tclSetDynList,         NULL, 
      "set one dynList to another" },
  { "dl_rename",           tclRenameDynList,      NULL, 
      "rename existing dynList to new name" },
  { "dl_local",            tclLocalDynList,       NULL, 
      "create a locally scoped dynList" },
  { "dl_return",           tclReturnDynList,      NULL, 
      "make a dynlist the current return value" },
  { "dl_delete",           tclDeleteDynList,      NULL, 
      "delete a dynList" },
  { "dl_deleteTrace",      tclDeleteTraceDynList,    NULL, 
      "delete a dynList from a trace cmd" },
  { "dl_reset",            tclResetDynList,       NULL, 
      "reset a dynList" },
  { "dl_pushTemps" ,       tclPushTmpList,        NULL,
      "save names of subsequently created temp lists" },
  { "dl_popTemps" ,        tclPopTmpList,         NULL,
      "remove lists created since last dl_pushTemps" },
  { "dl_clean" ,           tclCleanDynList,       (void *) DL_CLEAN_TEMPS, 
      "delete temporary dynLists" },
  { "dl_clean" ,           tclCleanDynList,       (void *) DL_CLEAN_TEMPS, 
      "delete temporary dynLists" },
  { "dl_cleanReturns" ,    tclCleanDynList,       (void *) DL_CLEAN_RETS, 
      "delete temporary dynLists" },
  { "dl_get",              tclGetPutDynList,      (void *) DL_GET, 
      "get an element from a dynList" },
  { "dl_cycle",            tclListFromList,       (void *) DL_SHIFTCYCLE,
      "cycle elements of a list" },
  { "dl_shift",            tclListFromList,       (void *) DL_SHIFT,
      "shift elements of a list" },
  { "dl_subshift",         tclListFromList,       (void *) DL_SUBSHIFT,
      "shift elements of sublists" },
  { "dl_bcycle",            tclListFromList,      (void *) DL_BSHIFTCYCLE,
      "cycle elements of bottommost sublists" },
  { "dl_bshift",           tclListFromList,       (void *) DL_BSHIFT,
      "shift bottommost sublists" },
  { "dl_index",            tclListFromList,      (void *) DL_INDEX_LIST,
      "return index of each element of list" },
  { "dl_firstPos",         tclListFromList,      (void *) DL_FIRST_INDEX_LIST,
      "return logical list with ones for first elements" },
  { "dl_lastPos",          tclListFromList,      (void *) DL_LAST_INDEX_LIST,
      "return logical list with ones for last elements" },
  { "dl_select",           tclSelectDynList,      (void *) DL_SELECT,
      "select elements from a list" },
  { "dl_choose",           tclSelectDynList,      (void *) DL_CHOOSE,
      "select elements from a list" },
  { "dl_replace",          tclReplaceDynList,     (void *) DL_REPLACE,
      "replace selected elements from a list" },
  { "dl_replaceByIndex",   tclReplaceDynList,     (void *) DL_REPLACE_BY_INDEX,
      "replace selected elements from a list (using indices)" },
  { "dl_put",              tclGetPutDynList,      (void *) DL_PUT, 
      "put an element in a dynList" },
  { "dl_first",            tclGetPutDynList,      (void *) DL_FIRST, 
      "get first element from a dynList" },
  { "dl_last",             tclGetPutDynList,      (void *) DL_LAST, 
      "get last element from a dynList" },
  { "dl_pickone",          tclGetPutDynList,      (void *) DL_PICKONE, 
      "get random element from a dynList" },
  { "dl_append",           tclAppendDynList,      (void *) DL_APPEND, 
      "append an element to a dynList" },
  { "dl_prepend",          tclAppendDynList,      (void *) DL_PREPEND, 
      "prepend an element to a dynList" },
  { "dl_insert",           tclAppendDynList,      (void *) DL_INSERT, 
      "insert an element into a dynList" },
  { "dl_spliceBefore",     tclAppendDynList,      (void *) DL_SPLICE_BEFORE, 
      "splice list2 onto front of list1 (recursively)" },
  { "dl_spliceAfter",      tclAppendDynList,      (void *) DL_SPLICE_AFTER, 
      "splice list2 onto end of list1 (recursively)" },
  { "dl_concat",           tclConsDynList,        (void *) DL_CONCAT, 
      "concatenate list2 onto list1" },
  { "dl_combine",          tclConsDynList,        (void *) DL_COMBINE, 
      "combine list1 and list2 into new list" },
  { "dl_interleave",       tclConsDynList,        (void *) DL_INTERLEAVE, 
      "interleave lists into new list" },
  { "dl_increment",        tclConsDynList,        (void *) DL_INCREMENT, 
      "append element 1 greater than current last" },
  { "dl_incr",             tclConsDynList,        (void *) DL_INCREMENT, 
      "append element 1 greater than current last" },

  { "dl_setFormat",        tclSetFormat,          (void *) NULL, 
      "set the format string for data types" },
  { "dl_dump",             tclDumpDynList,        (void *) DL_DUMP, 
      "dump a dynList in a column" },
  { "dl_dumpAsRow",        tclDumpDynList,        (void *) DL_DUMP_AS_ROW, 
      "dump a dynList in a row" },
  { "dl_dumpMatrix",       tclDumpDynList,        (void *) DL_DUMP_MATRIX, 
      "dump a dynList as a matrix rowwise" },
  { "dl_dumpMatrixInCols", tclDumpDynList, (void *) DL_DUMP_MATRIX_IN_COLS, 
      "dump a dynList as a matrix columnwise" },
  { "dl_write",            tclDumpDynList,        (void *) DL_DUMPBYTES, 
      "dump binary representation of dynlist" },
  { "dl_writeAs",          tclDumpDynList,        (void *) DL_DUMPBYTES_AS, 
      "dump binary representation of dynlist in alternate formats" },
  { "dl_tcllist",          tclDumpDynList,        (void *) DL_TO_TCL_LIST, 
      "dump a dynList in a column" },
  { "dl_zeros",            tclGenerateDynList,    (void *) DL_ZEROS,
      "generate list of ones" },
  { "dl_ones",             tclGenerateDynList,    (void *) DL_ONES,
      "generate list of ones" },
  { "dl_srand",            tclSetRandomSeed,      (void *) NULL,
      "initialize random number generators"},      
  { "dl_irand",            tclGenerateDynList,    (void *) DL_IRAND,
      "generate uniformly distributed random integers" },
  { "dl_urand",            tclGenerateDynList,    (void *) DL_URAND,
      "generate uniformly distributed random numbers" },
  { "dl_zrand",            tclGenerateDynList,    (void *) DL_ZRAND,
      "generate normally distributed random numbers" },
  { "dl_gaussian2D",       tclGenerateDynList,    (void *) DL_GAUSSIAN_2D,
      "generate a gaussian map" },
  { "dl_randfill",         tclGenerateDynList,    (void *) DL_RANDFILL,
      "randomly permute numbers between 0 and n" },
  { "dl_shuffle",          tclGenerateDynList,    (void *) DL_SHUFFLE,
      "randomly shuffle elements of a list" },
  { "dl_randchoose",       tclGenerateDynList,    (void *) DL_RANDCHOOSE,
      "select m random numbers between 0 and n" },
  { "dl_series",           tclSeries,             (void *) DL_SERIES,
      "generate an arithmetic series" },
  { "dl_fromto",           tclSeries,             (void *) DL_FROMTO,
      "generate a series excluding final value" },
  { "dl_repeat",           tclRepList,            (void *) DL_REPEAT,
      "repeat each element of a list n times" },
  { "dl_repeatElements",   tclRepList,            (void *) DL_REPEAT_ELTS,
      "repeat each element of a list of lists n times" },
  { "dl_replicate",        tclRepList,            (void *) DL_REPLICATE,
      "replicate entire list n times" },
  { "dl_float",            tclConvertDynList,     (void *) DL_FLOAT,
      "convert list to floats" },
  { "dl_int",              tclConvertDynList,     (void *) DL_INT,
      "convert list to ints" },
  { "dl_char",             tclConvertDynList,     (void *) DL_CHAR,
      "convert list to bytes" },
  { "dl_short",            tclConvertDynList,     (void *) DL_SHORT,
      "convert list to bytes" },
  { "dl_ufloat",           tclUnsignedConvertDynList,     (void *) DL_FLOAT,
      "convert list to floats" },
  { "dl_uint",             tclUnsignedConvertDynList,     (void *) DL_INT,
      "convert list to ints" },
  { "dl_uchar",            tclUnsignedConvertDynList,     (void *) DL_CHAR,
      "convert list to bytes" },
  { "dl_ushort",           tclUnsignedConvertDynList,     (void *) DL_SHORT,
      "convert list to bytes" },
  { "dl_not",              tclDynListNot,         NULL,
      "elementwise not operation" },
  { "dl_and",              tclCompareDynList,     (void *) DL_AND,
      "elementwise and operation" },
  { "dl_or",               tclCompareDynList,     (void *) DL_OR,
      "elementwise or operation" },
  { "dl_eq",               tclCompareDynList,     (void *) DL_EQUAL_TO,
      "elementwise equal compare" },
  { "dl_noteq",            tclCompareDynList,     (void *) DL_NOT_EQUAL_TO,
      "elementwise not equal compare" },
  { "dl_lt",               tclCompareDynList,     (void *) DL_LESS_THAN,
      "elementwise less than compare" },
  { "dl_gt",               tclCompareDynList,     (void *) DL_GREATER_THAN,
      "elementwise greater than compare" },
  { "dl_lte",              tclCompareDynList,     (void *) DL_LTE,
      "elementwise less than or equal to compare" },
  { "dl_gte",              tclCompareDynList,     (void *) DL_GTE,
      "elementwise greater than or equal to compare" },

  { "dl_andIndex",         tclCompareDynListIndices, (void *) DL_AND,
      "elementwise and operation" },
  { "dl_orIndex",          tclCompareDynListIndices, (void *) DL_OR,
      "elementwise or operation" },
  { "dl_eqIndex",          tclCompareDynListIndices, (void *) DL_EQUAL_TO,
      "elementwise equal compare" },
  { "dl_noteqIndex",       tclCompareDynListIndices, (void *) DL_NOT_EQUAL_TO,
      "elementwise not equal compare" },
  { "dl_ltIndex",          tclCompareDynListIndices, (void *) DL_LESS_THAN,
      "elementwise less than compare" },
  { "dl_gtIndex",          tclCompareDynListIndices, (void *) DL_GREATER_THAN,
      "elementwise greater than compare" },
  { "dl_lteIndex",         tclCompareDynListIndices, (void *) DL_LTE,
      "elementwise less than or equal to compare" },
  { "dl_gteIndex",         tclCompareDynListIndices, (void *) DL_GTE,
      "elementwise greater than or equal to compare" },

  { "dl_oneof",            tclCompareDynList,     (void *) DL_ONEOF,
      "compare source list to a set of possible matches" },
  { "dl_stringmatch",      tclCompareDynList,     (void *) DL_STRINGMATCH,
      "compare elements in source with pattern using glob style matching" },

  { "dl_find",             tclFindSublist,        (void *) DL_FIND,
      "find first occurence of a sublist within a source list" },
  { "dl_findIndices",      tclFindSublist,        (void *) DL_FIND_INDICES,
      "find indices of a search list in a source list" },
  { "dl_findAll",          tclFindSublist,        (void *) DL_FIND_ALL,
      "find all occurences of a sublist within a source list" },
  { "dl_findPatterns",     tclFindSublist,        (void *) DL_FIND_PATTERNS,
      "find all occurences of a sublist within a list of source lists" },
  { "dl_countOccurences",  tclFindSublist,        (void *) DL_COUNT_OCCURENCES,
      "count all occurences of a sublist within a source list" },
  { "dl_mod",              tclCompareDynList,     (void *) DL_MOD,
      "list1 mod list2" },
  { "dl_add",              tclArithDynList,       (void *) DL_ADD,
      "elementwise add" },
  { "dl_sub",              tclArithDynList,       (void *) DL_SUBTRACT,
      "elementwise subtract" },
  { "dl_mult",             tclArithDynList,       (void *) DL_MULTIPLY,
      "elementwise multiply" },
  { "dl_div",              tclArithDynList,       (void *) DL_DIVIDE,
      "elementwise divide" },
  { "dl_pow",              tclArithDynList,       (void *) DL_POW,
      "elementwise pow a,b" },
  { "dl_abs",              tclMathFuncOneArg,     (void *) DL_ABS,
      "elementwise absolute value" },
  { "dl_acos",             tclMathFuncOneArg,     (void *) DL_ACOS,
      "elementwise acos" },
  { "dl_asin",             tclMathFuncOneArg,     (void *) DL_ASIN,
      "elementwise asin" },
  { "dl_atan",             tclMathFuncOneArg,     (void *) DL_ATAN,
      "elementwise atan" },
  { "dl_atan2",            tclArithDynList,       (void *) DL_ATAN2,
      "elementwise atan2" },
  { "dl_fmod",             tclArithDynList,       (void *) DL_FMOD,
      "elementwise fmod" },
  { "dl_ceil",             tclMathFuncOneArg,     (void *) DL_CEIL,
      "smallest integer not less than" },
  { "dl_cos",              tclMathFuncOneArg,     (void *) DL_COS,
      "elementwise cos" },
  { "dl_cosh",             tclMathFuncOneArg,     (void *) DL_COSH,
      "elementwise cosh" },
  { "dl_exp",              tclMathFuncOneArg,     (void *) DL_EXP,
      "elementwise e to the x" },
  { "dl_lgamma",           tclMathFuncOneArg,     (void *) DL_LGAMMA,
      "elementwise log gamma of x" },
  { "dl_floor",            tclMathFuncOneArg,     (void *) DL_FLOOR,
      "elementwise floor" },
  { "dl_round",            tclMathFuncOneArg,     (void *) DL_ROUND,
      "elementwise round" },
  { "dl_log",              tclMathFuncOneArg,     (void *) DL_LOG,
      "natural log" },
  { "dl_log10",            tclMathFuncOneArg,     (void *) DL_LOG10,
      "log base 10" },
  { "dl_sin",              tclMathFuncOneArg,     (void *) DL_SIN,
      "elementwise sine" },
  { "dl_sinh",             tclMathFuncOneArg,     (void *) DL_SINH,
      "elementwise hyperbolic sine" },
  { "dl_sqrt",             tclMathFuncOneArg,     (void *) DL_SQRT,
      "elementwise sqrt" },
  { "dl_tan",              tclMathFuncOneArg,     (void *) DL_TAN,
      "elementwise tan" },
  { "dl_tanh",             tclMathFuncOneArg,     (void *) DL_TANH,
      "elementwise hyperbolic tan" },
  { "dl_recip",            tclListFromList,       (void *) DL_RECIPROCAL,
      "return elementwise reciprocals" },
  { "dl_sign",             tclListFromList,       (void *) DL_SIGN,
      "elementwise sign function" },
  { "dl_negate",           tclListFromList,       (void *) DL_NEGATE,
      "return elementwise reciprocals" },
  { "dl_transpose",        tclListFromList,       (void *) DL_TRANSPOSE,
      "transpose list of lists" },
  { "dl_transposeAt",      tclListFromList,       (void *) DL_TRANSPOSE_AT,
      "transpose list of lists starting at specified level" },
  { "dl_reshape",          tclReshapeList,        NULL,
      "change shape a list" },
  { "dl_restructure",      tclRestructureList,    NULL,
      "restructure a list to match another" },
  { "dl_collapse",         tclListFromList,       (void *) DL_COLLAPSE,
      "collapse a list" },
  { "dl_pack",             tclListFromList,       (void *) DL_PACK,
      "pack a list into lists of lists" },
  { "dl_deepPack",             tclListFromList,   (void *) DL_DEEP_PACK,
      "pack a list into lists of lists at lowest level" },
  { "dl_unpack",           tclListFromList,       (void *) DL_UNPACK,
      "unpack a list of list of lists" },
  { "dl_unpackLists",      tclListFromList,       (void *) DL_UNPACK_LISTS,
      "unpack a list of lists" },
  { "dl_deepUnpack",       tclListFromList,       (void *) DL_DEEP_UNPACK,
      "unpack the lowest level of a list of list of lists" },
  { "dl_diff",             tclListFromList,       (void *) DL_DIFF,
      "return lagged diff of a list" },
  { "dl_gradient",         tclListFromList,       (void *) DL_GRADIENT,
      "return gradients for elements of a list" },
  { "dl_zeroCrossings",    tclListFromList,       (void *) DL_ZERO_CROSSING,
      "find positive and negative zero crossings" },
  { "dl_sort",             tclListFromList,       (void *) DL_SORT,
      "sort elements of a list" },
  { "dl_bsort",            tclListFromList,       (void *) DL_BSORT,
      "sort \"bottom\" elements of a list" },
  { "dl_sortIndices",      tclListFromList,       (void *) DL_SORT_INDICES,
      "get indices of sorted elements" },
  { "dl_bsortIndices",     tclListFromList,       (void *) DL_BSORT_INDICES,
      "return sort indices of \"bottom\" elements of a list" },
  { "dl_rank",             tclListFromList,       (void *) DL_RANK,
      "return list of appearance orders for each unique value" },
  { "dl_recode",           tclListFromList,       (void *) DL_RECODE,
      "generate integer ids for elements of a list" },
  { "dl_recodeWithTies",   tclListFromList,       (void *) DL_RECODE_WITH_TIES,
      "generate recoded ids for elements of a list (mid-rank tie recoding)" },
  { "dl_cut",              tclListFromList,       (void *) DL_CUT,
      "break up continuous list into bins" },
  { "dl_permute",          tclPermuteDynList,     (void *) DL_PERMUTE,
      "permute elements of a list" },
  { "dl_reverse",          tclPermuteDynList,     (void *) DL_REVERSE,
      "reverse elements of a list" },
  { "dl_breverse",         tclPermuteDynList,     (void *) DL_BREVERSE,
      "reverse \"bottom level\" elements of a list of lists" },
  { "dl_reverseAll",       tclPermuteDynList,     (void *) DL_REVERSE_ALL,
      "reverse elements of a list recursively" },
  { "dl_lengths",          tclListFromList,       (void *) DL_LENGTHS,
      "return lengths of lists inside a list" },
  { "dl_unique",           tclListFromList,       (void *) DL_UNIQUE,
      "extract unique elements of a list" },
  { "dl_uniqueNoSort",     tclListFromList,       (void *) DL_UNIQUE_NO_SORT,
      "extract unique elements of a list" },
  { "dl_count",            tclCountLists,         (void *) DL_SINGLE,
      "return number of elements falling in range" },
  { "dl_counts",           tclCountLists,         (void *) DL_RECURSIVE,
      "return a list of count lists" },
  { "dl_lcounts",          tclCountLists,         (void *) DL_REC2,
      "return a preserved list of count lists" },
  { "dl_cross",            tclSortByLists,        (void *) DL_CROSS,
      "make list crossing all list elements" },
  { "dl_uniqueCross",      tclSortByLists,        (void *) DL_UNIQUE_CROSS,
      "make list combining unique list elements" },
  { "dl_sortByList",       tclSortByLists,        (void *) DL_BYLIST,
      "return list sorted by list" },
  { "dl_sortByLists",      tclSortByLists,        (void *) DL_BYLISTS,
      "return list sorted by list of lists" },
  { "dl_sortBySelected",   tclSortByLists,        (void *) DL_BYSELECTED,
      "return list sorted by list of lists" },
  { "dl_length",           tclReduceList,         (void *) DL_LENGTH,
      "return length of lists" },
  { "dl_llength",          tclListFromList,       (void *) DL_LLENGTH,
      "return mapped list lengths" },
  { "dl_depth",            tclReduceList,         (void *) DL_DEPTH,
      "return depth of a list" },
  { "dl_min",              tclReduceList,         (void *) DL_MIN,
      "return min of a list" },
  { "dl_mins",             tclReduceLists,        (void *) DL_MIN,
      "return list of list mins" },
  { "dl_max",              tclReduceList,         (void *) DL_MAX,
      "return max of a list" },
  { "dl_maxs",             tclReduceLists,        (void *) DL_MAX,
      "return list of list max's" },
  { "dl_bmins",            tclReduceLists,        (void *) DL_BMIN,
      "return list of list mins" },
  { "dl_bmaxs",            tclReduceLists,        (void *) DL_BMAX,
      "return list of list max's" },
  { "dl_minIndex",         tclReduceList,         (void *) DL_MIN_INDEX,
      "return index of min of a list" },
  { "dl_maxIndex",         tclReduceList,         (void *) DL_MAX_INDEX,
      "return indices of maxs" },
  { "dl_minIndices",       tclReduceLists,        (void *) DL_MIN_INDEX,
      "return indices of mins" },
  { "dl_maxIndices",       tclReduceLists,        (void *) DL_MAX_INDEX,
      "return index of max of a list" },
  { "dl_minPositions",       tclReduceLists,        (void *) DL_MIN_POSITIONS,
      "return indices of mins, preserving structure" },
  { "dl_maxPositions",       tclReduceLists,        (void *) DL_MAX_POSITIONS,
      "return index of max of a list, preserving structure" },
  { "dl_indices",          tclListFromList,       (void *) DL_INDICES,
      "return indices of nonnull elements" },
  { "dl_sum",              tclReduceList,         (void *) DL_SUM,
      "return sum of a list" },
  { "dl_sums",             tclReduceLists,        (void *) DL_SUM,
      "return list of list sums" },
  { "dl_cumsum",           tclListFromList,       (void *) DL_CUMSUM,
      "return cumulutive sums of a list" },
  { "dl_prod",              tclReduceList,        (void *) DL_PROD,
      "return prod of a list" },
  { "dl_prods",             tclReduceLists,       (void *) DL_PROD,
      "return list of list prods" },
  { "dl_cumprod",           tclListFromList,      (void *) DL_CUMPROD,
      "return cumulutive products of a list" },
  { "dl_mean",             tclReduceList,         (void *) DL_MEAN,
      "return mean value of a list" },
  { "dl_meanList",         tclReduceLists,        (void *) DL_MEAN_LIST,
      "return average list of lists" },
  { "dl_sumList",         tclReduceLists,         (void *) DL_SUMCOLS_LIST,
      "return sums of columns of list of lists" },
  { "dl_means",            tclReduceLists,        (void *) DL_MEAN,
      "return list of list means" },
  { "dl_hmeans",           tclReduceLists,        (void *) DL_HMEAN,
      "return list of list \"horizontal\" means" },
  { "dl_bmeans",           tclReduceLists,        (void *) DL_BMEAN,
      "return list of list \"bottom level\" means" },
  { "dl_bstds",            tclReduceLists,        (void *) DL_BSTD,
      "return list of list \"bottom level\" means" },
  { "dl_bsums",            tclReduceLists,        (void *) DL_BSUMS,
      "return list of \"bottom level\" sums" },
  { "dl_std",              tclReduceList,         (void *) DL_STD,
      "return standard deviation of a list" },
  { "dl_stds",             tclReduceLists,        (void *) DL_STD,
      "return list of list standard devs" },
  { "dl_hstds",            tclReduceLists,        (void *) DL_HSTD,
      "return list of list \"horizontal\" stds" },
  { "dl_var",              tclReduceList,         (void *) DL_VAR,
      "return variance of a list" },
  { "dl_vars",             tclReduceLists,        (void *) DL_VAR,
      "return list of list variances" },
  { "dl_hvars",            tclReduceLists,        (void *) DL_HVAR,
      "return list of list \"horizontal\" vars" },
  { "dl_bins",             tclHistBins,           NULL,
      "generate bins used in histograms" },
  { "dl_hist",             tclHistLists,          (void *) DL_SINGLE,
      "return histogram of a list" },
  { "dl_hists",            tclHistLists,          (void *) DL_RECURSIVE,
      "return a list of histograms of lists" },
  { "dl_sdf",              tclSdfLists,          (void *) DL_SINGLE,
      "return sdf of a list" },
  { "dl_sdfs",             tclSdfLists,          (void *) DL_RECURSIVE,
      "return a list of adaptive sdfs of lists" },
  { "dl_deepSdf",         tclSdfListsRecursive,  (void *) DL_SINGLE,
      "return a list of adaptive sdfs of lists" },
  { "dl_deepSdfs",         tclSdfListsRecursive, (void *) DL_RECURSIVE,
      "return a list of adaptive sdfs of lists" },
  { "dl_parzen",           tclParzenLists,       (void *) DL_SINGLE,
      "return adaptive sdf of a list" },
  { "dl_parzens",          tclParzenLists,       (void *) DL_RECURSIVE,
      "return a list of sdfs of lists" },
  { "dl_conv",             tclArithDynList,      (void *) DL_CONV,
      "convolve data with kernel" },
  { "dl_conv2",             tclArithDynList,      (void *) DL_CONV2,
      "convolve data with recursive kernel" },
  { "dl_idiff",            tclListFromList,      (void *) DL_IDIFF,
      "return interelement differences" },
  { "dl_fill",             tclFillList,           NULL,
      "fill in values from a sparse time series" },


  { "dl_setMatherr",       tclSetMatherr,         NULL, 
      "turn on/off domain checking for math functions" },
  { "dl_pkg",              tclLoadPackage,        NULL, 
      "setup dlsh addon package" },

  { "dl_fileroot",         tclListFromList,       (void *) DL_FILE_ROOT,
      "get file rootname of elements of string list" },
  { "dl_filetail",         tclListFromList,       (void *) DL_FILE_TAIL,
      "get file file \"tail\" of elements of string list" },
  { "dl_filedir",          tclListFromList,       (void *) DL_FILE_DIR,
      "get file dirctory of elements of string list" },
  { "dl_fileext",          tclListFromList,       (void *) DL_FILE_EXT,
      "get file extension of elements of string list" },
  { "dl_filebase",         tclListFromList,       (void *) DL_FILE_BASE,
      "get file basename (no dir, no ext) of elements of string list" },

#if defined(QNX) || defined(WIN32)
  { "dl_tempname",         tclTempName,           NULL,
      "generate a list of one or more temp filenames" },
  { "dl_tmpname",          tclTempName,           NULL,
      "generate a list of one or more temp filenames" },
#endif

  { "dl_dateToDays",       tclDateToDays,    (void *) NULL,
    "return number of days from Jan 1, 1900 to given date" },
  { "dl_daysToDate",       tclDaysToDate,    (void *) NULL,
    "return date correspond to given days after Jan 1, 1900" },


  { "dl_noOp",             tclNoOp,               NULL, 
      "Don't do anything!" },
  { "dl_help",             tclDLHelp,             (void *) DLcommands, 
      "display help" },
  { NULL, NULL, NULL, NULL }
};


static int tclDoTimes(ClientData data, Tcl_Interp * interp, int objc,
		      Tcl_Obj * const objv[]);
static int tclForEach(ClientData data, Tcl_Interp * interp, int objc,
		      Tcl_Obj * const objv[]);
static int tclDynGroupToString(ClientData data, Tcl_Interp * interp, int objc,
			       Tcl_Obj * const objv[]);			       
static int tclDynGroupFromString(ClientData data, Tcl_Interp * interp, 
				 int objc, Tcl_Obj * const objv[]);
static int tclDynListToString(ClientData data, Tcl_Interp * interp, int objc,
			      Tcl_Obj * const objv[]);
static int tclDynListFromString(ClientData data, Tcl_Interp * interp, int objc,
				Tcl_Obj * const objv[]);
static int tclDynGroupToMsgpack(ClientData data, Tcl_Interp * interp, int objc,
			      Tcl_Obj * const objv[]);

static int tclDynGroupToArrow(ClientData data, Tcl_Interp * interp, int objc,
			      Tcl_Obj * const objv[]);
static int tclDynGroupFromArrow(ClientData data, Tcl_Interp * interp, int objc,
			      Tcl_Obj * const objv[]);
static int tclDynGroupFromArrowData(ClientData data, Tcl_Interp * interp, int objc,
			      Tcl_Obj * const objv[]);

static int tclRegexpList(ClientData data, Tcl_Interp * interp, int objc,
			 Tcl_Obj * const objv[]);
static int tclScanList(ClientData data, Tcl_Interp * interp, int objc,
		       Tcl_Obj * const objv[]);


static void deleteDlFunc(ClientData clientData, Tcl_Interp *interp)
{
  DLSHINFO *dlshinfo = (DLSHINFO *) clientData;
  
  Tcl_DeleteHashTable(&dlshinfo->dlTable);
  Tcl_DeleteHashTable(&dlshinfo->dgTable);
  free(dlshinfo);
}

/*****************************************************************************
 *
 * FUNCTION
 *    Dl_Init
 *
 * ARGS
 *    Tcl_Inter *interp
 *
 * DESCRIPTION
 *    Creates tcl commands for creating and manipulating dynLists & dynGroups
 * and initializes two static hash table for referring to them.
 *
 *****************************************************************************/

int Dl_Init(Tcl_Interp *interp)
{
  char *startup_dir, startup_dirbuf[128];
  char *startup_file, startup_filebuf[128];
  char startup[192];
  int i = 0;

  if (
#ifdef USE_TCL_STUBS
      Tcl_InitStubs(interp, "8.5-", 0)
#else
      Tcl_PkgRequire(interp, "Tcl", "8.5-", 0)
#endif
      == NULL) {
    return TCL_ERROR;
  }

  DLSHINFO *dlshinfo = (DLSHINFO *) calloc(1, sizeof(DLSHINFO));
  if (!dlshinfo) return TCL_ERROR;

  dlshinfo->DefaultListIncrement = 15;
  dlshinfo->DefaultGroupIncrement = 15;

  dlshinfo->dgCount = 0;	/* increasing count of dynGroups */
  dlshinfo->dlCount = 0;	/* increasing count of dynLists  */
  dlshinfo->localCount = 0;	/* for naming local variables    */
  dlshinfo->returnCount = 0;	/* for naming returned lists     */
 
  Tcl_InitHashTable(&dlshinfo->dlTable, TCL_STRING_KEYS);
  Tcl_InitHashTable(&dlshinfo->dgTable, TCL_STRING_KEYS);

  dlshinfo->TmpListStack =  (TMPLIST_STACK *) calloc(1, sizeof(TMPLIST_STACK));
  TMPLIST_SIZE(dlshinfo->TmpListStack) = 0;
  TMPLIST_INDEX(dlshinfo->TmpListStack) = -1;
  TMPLIST_TMPLISTS(dlshinfo->TmpListStack) = NULL;
  TMPLIST_INC(dlshinfo->TmpListStack) = 4;
  
  dlshinfo->TmpListRecordList = NULL;

  Tcl_SetAssocData(interp, DLSH_ASSOC_DATA_KEY,
		   NULL,
		   (void *) dlshinfo);

  while (DLcommands[i].name) {
    Tcl_CreateCommand(interp, DLcommands[i].name, 
		      (Tcl_CmdProc *) DLcommands[i].func, 
		      (ClientData) DLcommands[i].cd, 
		      (Tcl_CmdDeleteProc *) NULL);
    i++;
  }

  /* Add the two objectified commands */
  Tcl_CreateObjCommand(interp, "dl_dotimes", tclDoTimes, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dl_foreach", tclForEach, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dg_toString", tclDynGroupToString, 
		       (ClientData) DG_TOFROM_BINARY, NULL);
  Tcl_CreateObjCommand(interp, "dg_fromString", tclDynGroupFromString,
		       (ClientData) DG_TOFROM_BINARY, NULL);
  Tcl_CreateObjCommand(interp, "dg_toString64", tclDynGroupToString, 
		       (ClientData) DG_TOFROM_BASE64, NULL);
  Tcl_CreateObjCommand(interp, "dg_fromString64", tclDynGroupFromString,
		       (ClientData) DG_TOFROM_BASE64, NULL);

  Tcl_CreateObjCommand(interp, "dg_toJSON", tclDynGroupToString, 
		       (ClientData) DG_TOFROM_JSON, NULL);
  Tcl_CreateObjCommand(interp, "dg_json", tclDynGroupToString, 
		       (ClientData) DG_TOFROM_JSON, NULL);
  Tcl_CreateObjCommand(interp, "dg_toHybridJSON", tclDynGroupToString, 
		       (ClientData) DG_TOFROM_JSON_HYBRID, NULL);
  Tcl_CreateObjCommand(interp, "dl_toString", tclDynListToString, 
		       (ClientData) DL_TOFROM_BINARY, NULL);
  Tcl_CreateObjCommand(interp, "dl_toString64", tclDynListToString, 
		       (ClientData) DL_TOFROM_BASE64, NULL);
  Tcl_CreateObjCommand(interp, "dl_toJSON", tclDynListToString, 
		       (ClientData) DL_TOFROM_JSON, NULL);
  Tcl_CreateObjCommand(interp, "dl_json", tclDynListToString, 
		       (ClientData) DL_TOFROM_JSON, NULL);
		       
  Tcl_CreateObjCommand(interp, "dg_toMsgpackFile", tclDynGroupToMsgpack, 
                       (ClientData)DG_TOFROM_MSGPACK_FILE, NULL);
  Tcl_CreateObjCommand(interp, "dg_toHybridMsgpackFile", tclDynGroupToMsgpack, 
                       (ClientData)DG_TOFROM_MSGPACK_HYBRID_FILE, NULL);
  Tcl_CreateObjCommand(interp, "dg_toMsgpackData", tclDynGroupToMsgpack, 
                       (ClientData)DG_TOFROM_MSGPACK_DATA, NULL);
  Tcl_CreateObjCommand(interp, "dg_toHybridMsgpackData", tclDynGroupToMsgpack, 
                       (ClientData)DG_TOFROM_MSGPACK_HYBRID_DATA, NULL);

  Tcl_CreateObjCommand(interp, "dg_toArrowFile", tclDynGroupToArrow, 
                       (ClientData) DG_TOFROM_ARROW_FILE, NULL); 
  Tcl_CreateObjCommand(interp, "dg_toArrow", tclDynGroupToArrow, 
                       (ClientData) DG_TOFROM_ARROW_DATA, NULL);
  Tcl_CreateObjCommand(interp, "dg_fromArrowFile", tclDynGroupFromArrow, 
                       (ClientData) 0, NULL);
  Tcl_CreateObjCommand(interp, "dg_fromArrow", tclDynGroupFromArrowData, 
                       (ClientData) 0, NULL);
	
  Tcl_CreateObjCommand(interp, "dl_fromString", tclDynListFromString, 
		       (ClientData) DL_TOFROM_BINARY, NULL);
  Tcl_CreateObjCommand(interp, "dl_fromString64", tclDynListFromString, 
		       (ClientData) DL_TOFROM_BASE64, NULL);
  Tcl_CreateObjCommand(interp, "dl_regexp", tclRegexpList,
		       (ClientData) DL_REGEXP, NULL);
  Tcl_CreateObjCommand(interp, "dl_regmatch", tclRegexpList,
		       (ClientData) DL_REGMATCH, NULL);
  Tcl_CreateObjCommand(interp, "dl_scan", tclScanList,
		       (ClientData) DL_SCAN_INT, NULL);
  Tcl_CreateObjCommand(interp, "dl_scanInt", tclScanList,
		       (ClientData) DL_SCAN_INT, NULL);
  Tcl_CreateObjCommand(interp, "dl_scanFloat", tclScanList,
		       (ClientData) DL_SCAN_FLOAT, NULL);
  Tcl_CreateObjCommand(interp, "dl_scanOctal", tclScanList,
		       (ClientData) DL_SCAN_OCTAL, NULL);
  Tcl_CreateObjCommand(interp, "dl_scanHex", tclScanList,
		       (ClientData) DL_SCAN_HEX, NULL);
  Tcl_CreateObjCommand(interp, "dl_scanBinary", tclScanList,
		       (ClientData) DL_SCAN_BINARY, NULL);

  Dm_Init(interp);		/* add the matrix functions */

#if 0
  if ((startup_dir = getenv("DLSH_LIBRARY"))) {
    int last = strlen(startup_dir)-1;
    strcpy(startup_dirbuf, startup_dir);

    /* peel off trailing '/' for consistency */
    if (startup_dirbuf[last] == '/') startup_dirbuf[last] = 0;
  }
  else {
    strcpy(startup_dirbuf, DEFAULT_STARTUP_DIR);
  }

  if ((startup_file = getenv("DLSH_RCFILE"))) {
    strcpy(startup_filebuf, startup_file);
  }
  else {
    strcpy(startup_filebuf, DEFAULT_STARTUP_FILE);
  }

  sprintf(startup, "%s/%s", startup_dirbuf, startup_filebuf);
  Tcl_SetVar(interp, "dlsh_library", startup_dirbuf, TCL_GLOBAL_ONLY);


  Tcl_EvalFile(interp, startup);
#endif
  
  raninit();

#ifdef WIN32
  _fmode = _O_BINARY;		/* Make default file i/o binary */
#endif

  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclDLHelp
 *
 * ARGS
 *    Tcl Args
 *
 * DESCRIPTION
 *    Loop through DLCommands and print out help messages
 *
 *****************************************************************************/

static int tclDLHelp (ClientData cd, Tcl_Interp *interp, 
                      int argc, char *argv[])
{
  TCL_COMMANDS *commands = (TCL_COMMANDS *) cd;
  int i = 0;

  while (commands[i].name) {
    if (commands[i].desc && commands[i].desc[0] != 0) {
      printf("%-22s - %s\n", commands[i].name, commands[i].desc);
    }
    i++;
  }
  return TCL_OK;
}


/*****************************************************************************
 **************************  DynGroup Functions  *****************************
 *****************************************************************************/


/*****************************************************************************
 *
 * FUNCTION
 *    tclDynGroupExists
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_exists
 *
 * DESCRIPTION
 *    Returns 1 if dynGroup exists
 *
 *****************************************************************************/

static int tclDynGroupExists (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  if (argc < 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " name", NULL);
      return TCL_ERROR;
  }

  if (tclFindDynGroup(interp, argv[1], NULL) == TCL_OK) 
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
  else 
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclDynGroupDir
 *
 * ARGS
 *    Tcl_Interp *interp
 *
 * DESCRIPTION
 *    Returns a list of all existing dyngroups.
 *
 *****************************************************************************/

static int tclDynGroupDir (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  Tcl_HashEntry *entryPtr;
  Tcl_HashSearch searchEntry;
  Tcl_DString dirList;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  Tcl_DStringInit(&dirList);
  entryPtr = Tcl_FirstHashEntry(&dlinfo->dgTable, &searchEntry);
  if (entryPtr) {
    Tcl_DStringAppendElement(&dirList, Tcl_GetHashKey(&dlinfo->dgTable, entryPtr));
    while ((entryPtr = Tcl_NextHashEntry(&searchEntry))) {
      Tcl_DStringAppendElement(&dirList, Tcl_GetHashKey(&dlinfo->dgTable, entryPtr));
    }
  }
  Tcl_DStringResult(interp, &dirList);
  return TCL_OK;
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclCreateDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_create
 *
 * DESCRIPTION
 *    Creates a new dyngroup
 *
 *****************************************************************************/

static int tclCreateDynGroup (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  DYN_GROUP *dg;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  int increment = dlinfo->DefaultGroupIncrement;
  char groupname[64];

  if (argc > 1) {
    strncpy(groupname, argv[1], 63);
    dlinfo->dgCount++;
  }
  else sprintf(groupname, "group%d", dlinfo->dgCount++);

  if (argc > 2) {
    if (Tcl_GetInt(interp, argv[2], &increment) != TCL_OK) {
      Tcl_SetResult(interp, "dg_create: invalid size argument", TCL_STATIC);
      return TCL_ERROR;
    }
  }

  if (!(dg = dfuCreateNamedDynGroup(groupname, increment))) {
    Tcl_SetResult(interp, "dg_create: error creating new dyngroup", TCL_STATIC);
    return TCL_ERROR;
  }

  return (tclPutGroup(interp, dg));
}

int tclPutGroup(Tcl_Interp *interp, DYN_GROUP *dg) 
{
  Tcl_HashEntry *entryPtr;
  int newentry;
  char groupname[64];
  
  DLSHINFO *dlinfo = (DLSHINFO *) Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if (!dg) return 0;

  if (!DYN_GROUP_NAME(dg)[0]) {
    sprintf(groupname, "group%d", dlinfo->dgCount++);
    strcpy(DYN_GROUP_NAME(dg), groupname);
  }
  else {
    strcpy(groupname, DYN_GROUP_NAME(dg));
  }

  if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, groupname))) {
    char resultstr[128];
    snprintf(resultstr, sizeof(resultstr),
	     "tclPutGroup: group %s already exists", groupname);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
    return TCL_ERROR;
  }

  /*
   * Add to hash table which contains list of open dyngroups
   */
  entryPtr = Tcl_CreateHashEntry(&dlinfo->dgTable, groupname, &newentry);
  Tcl_SetHashValue(entryPtr, dg);

  Tcl_SetResult(interp, groupname, TCL_VOLATILE);
  return TCL_OK;
}

static int tclDgTempName (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  char groupname[64];

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;

  sprintf(groupname, "group%d", dlinfo->dgCount++);
  Tcl_SetResult(interp, groupname, TCL_VOLATILE);
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclCopyDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_copy
 *
 * DESCRIPTION
 *    Copies an existing dynGroup
 *
 *****************************************************************************/

static int tclCopyDynGroup (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  DYN_GROUP *old, *new;
  char newname[64];
  Tcl_HashEntry *entryPtr;
  int newentry;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " oldgroup [newgroup]",
		     (char *) NULL);
    return TCL_ERROR;
  }
  

  if (argc > 2) {
    strncpy(newname, argv[2], 63);
    dlinfo->dgCount++;
  }
  else sprintf(newname, "group%d", dlinfo->dgCount++);

  if (tclFindDynGroup(interp, argv[1], &old) != TCL_OK) return TCL_ERROR;
  
  if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, newname))) {
    char resultstr[128];
    snprintf(resultstr, sizeof(resultstr),
	     "dg_copy: group %s already exists", newname);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
    return TCL_ERROR;
  }
  
  if (!(new = dfuCopyDynGroup(old, newname))) {
    Tcl_SetResult(interp, "dg_copy: error copying group", TCL_STATIC);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open dyngroups
   */
  entryPtr = Tcl_CreateHashEntry(&dlinfo->dgTable, newname, &newentry);
  Tcl_SetHashValue(entryPtr, new);
  
  Tcl_SetResult(interp, newname, TCL_VOLATILE);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclRenameDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_rename
 *
 * DESCRIPTION
 *    Copies an existing dynGroup
 *
 *****************************************************************************/

static int tclRenameDynGroup (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  DYN_GROUP *dg;
  DYN_LIST *dl;
  char *oldname, *newname, *colon;
  Tcl_HashEntry *entryPtr;
  int newentry;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " dyngroup/dyngroup:list newname", (char *) NULL);
    return TCL_ERROR;
  }

  oldname = argv[1];
  newname = argv[2];

  /* Nothing to do since the names are the same...just fake like we did it */
  if (!strcmp(oldname, newname)) {
    Tcl_SetResult(interp, newname, TCL_VOLATILE);
    return TCL_OK;
  }

  /* Without the group:colon notation, just rename the group */
  if (!(colon = strchr(oldname,':'))) {
    if (tclFindDynGroup(interp, oldname, &dg) != TCL_OK) return TCL_ERROR;

    /* If the target name is already a group, delete it */
    if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, newname))) {
      DYN_GROUP *dgold;
      if ((dgold = Tcl_GetHashValue(entryPtr))) dfuFreeDynGroup(dgold);
      Tcl_DeleteHashEntry(entryPtr);
    }

    /* Actually rename the group */
    if (newname != DYN_GROUP_NAME(dg))
      strncpy(DYN_GROUP_NAME(dg), newname, DYN_GROUP_NAME_SIZE-1);
    
    /* Add new name to hash table which contains list of open dyngroups */
    entryPtr = Tcl_CreateHashEntry(&dlinfo->dgTable, DYN_GROUP_NAME(dg), &newentry);
    Tcl_SetHashValue(entryPtr, dg);
    
    /* And remove old name from hash table */
    entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, oldname);
    Tcl_DeleteHashEntry(entryPtr);

    Tcl_SetResult(interp, newname, TCL_VOLATILE);
    return TCL_OK;
  }
  else {
    char *newcolon;		/* colon in newname */
    char resultname[128], groupname[48];
    strncpy(groupname, oldname, colon-oldname);
    groupname[colon-oldname] = 0;
    
    /* 
     * Strip off group name if it was supplied, otherwise we'll get
     * a list in the group called group:group:newname
     */
    if ((newcolon = strchr(newname,':'))) newname = newcolon+1;

    /* Make sure the resulting list name does not already exist */
    sprintf(resultname, "%s:%s", groupname, newname);
    if (tclFindDynList(interp, resultname, NULL) == TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": list \"", resultname, 
		       "\" already exists", NULL);
      return TCL_ERROR;
    }

    /* If the first arg is a named list, rename it */
    if (tclFindDynList(interp, oldname, &dl) == TCL_OK) {
      strncpy(DYN_LIST_NAME(dl), newname, DYN_LIST_NAME_SIZE-1);
      Tcl_SetResult(interp, resultname, TCL_VOLATILE);
      return TCL_OK;
    }
    /* If the notation is group:number try number as a column selector */
    else {
      int id;
      if (tclFindDynGroup(interp, groupname, &dg) != TCL_OK) return TCL_ERROR;
      if (Tcl_GetInt(interp, colon+1, &id) == TCL_OK) {
	if (id < DYN_GROUP_N(dg) && id >= 0) {
	  strncpy(DYN_LIST_NAME(DYN_GROUP_LIST(dg,id)), newname, 
		  DYN_LIST_NAME_SIZE-1);
	  Tcl_SetResult(interp, resultname, TCL_VOLATILE);
	  return TCL_OK;
	}
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, argv[0], ": invalid list id", NULL);
	return TCL_ERROR;
      }
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": list \"", colon+1, 
		       "\" not found in group \"", groupname, "\"", NULL);
      return TCL_ERROR;
    }
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclAppendDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_append
 *
 * DESCRIPTION
 *    Appends second dyngroup onto first
 *
 *****************************************************************************/

static int tclAppendDynGroup (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  DYN_GROUP *dg1, *dg2;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dg_onto dg_from", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynGroup(interp, argv[1], &dg1) != TCL_OK) return TCL_ERROR;
  if (tclFindDynGroup(interp, argv[2], &dg2) != TCL_OK) return TCL_ERROR;

  if (!dynGroupAppend(dg1, dg2)) {
    Tcl_AppendResult(interp, argv[0], ": incompatible dyngroups", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    Tcl_SetResult(interp, argv[1], TCL_VOLATILE);
    return TCL_OK;
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclWriteDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_write
 *
 * DESCRIPTION
 *    Writes out an existing dynGroup
 *
 *****************************************************************************/

static int tclWriteDynGroup (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  DYN_GROUP *dg;
  char *outfile = NULL;
  int format = DF_BINARY;
  int operation = DG_UNCOMPRESSED;
  int status;
  int buffer_increment = 0;	/* use default */
  int oldval;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dyngroup filename",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (tclFindDynGroup(interp, argv[1], &dg) != TCL_OK) return TCL_ERROR;
  buffer_increment = dgEstimateGroupSize(dg);

/* for Windows 32-bit, bail if group is larger that likely memory block */
#ifdef _M_IX86
  if (buffer_increment > 100000000) {
    Tcl_AppendResult(interp, argv[0], ": dyngroup too large for 32-bit environment write",
		     (char *) NULL);
    return TCL_ERROR;
  }
#endif
  
  if (argc < 3) {
    outfile = (char *) malloc(strlen(argv[1])+6);
    sprintf(outfile, "%s.dgz", argv[1]);
    operation = DG_COMPRESSED;	/* Default to writing compressed */
  }
  
  
  if (argc > 2) {		/* If the suffix is .dgz, then compress */
    char *suffix = strrchr(argv[2], '.');
    if (suffix) {
      if (strlen(suffix) == 4) {
	if ((suffix[1] == 'l' && suffix[2] == 'z' && suffix[3] == '4') ||
	    (suffix[1] == 'L' && suffix[2] == 'Z' && suffix[3] == '4')) {
	  operation = DG_UNCOMPRESSED; /* counterintuitive, I know */
	  format = DF_LZ4;
	}
	else if (suffix[3] == 'z') {
	  operation = DG_COMPRESSED;
	}
      }
    } 
    outfile = argv[2];
  }
  
  Tcl_MutexLock(&dgBufferMutex);
  
  dgInitBuffer();
  if (!buffer_increment) {
    oldval = dgSetBufferIncrement(buffer_increment);
  }
  oldval = dgSetBufferIncrement(buffer_increment);
  dgRecordDynGroup(dg);    
  if (operation == DG_UNCOMPRESSED)
    status = dgWriteBuffer(outfile, format);
  else 
    status = dgWriteBufferCompressed(outfile);
    
  dgCloseBuffer();

  dgSetBufferIncrement(oldval);
  
  Tcl_MutexUnlock(&dgBufferMutex);
  
  if (argc < 3) free(outfile);
  
  if (!status) {
    Tcl_AppendResult(interp, "dg_write: error writing file ", outfile, NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTIONS
 *    base64encode/base64decode
 *
 * DESCRIPTION
 *    Move to/from b64 encoding
 *
 * SOURCE
 *    http://en.wikibooks.org/wiki/Algorithm_Implementation/Miscellaneous/Base64
 *
 *****************************************************************************/


static int base64encode(const void* data_buf, int dataLength, char* result, int resultSize)
{
   const char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
   const unsigned char *data = (const unsigned char *)data_buf;
   int resultIndex = 0;
   int x;
   unsigned int n = 0;
   int padCount = dataLength % 3;
   unsigned char n0, n1, n2, n3;
 
   /* increment over the length of the string, three characters at a time */
   for (x = 0; x < dataLength; x += 3) 
   {
      /* these three 8-bit (ASCII) characters become one 24-bit number */
      n = data[x] << 16;
 
      if((x+1) < dataLength)
         n += data[x+1] << 8;
 
      if((x+2) < dataLength)
         n += data[x+2];
 
      /* this 24-bit number gets separated into four 6-bit numbers */
      n0 = (unsigned char)(n >> 18) & 63;
      n1 = (unsigned char)(n >> 12) & 63;
      n2 = (unsigned char)(n >> 6) & 63;
      n3 = (unsigned char)n & 63;
 
      /*
       * if we have one byte available, then its encoding is spread
       * out over two characters
       */
      if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
      result[resultIndex++] = base64chars[n0];
      if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
      result[resultIndex++] = base64chars[n1];
 
      /*
       * if we have only two bytes available, then their encoding is
       * spread out over three chars
       */
      if((x+1) < dataLength)
      {
         if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
         result[resultIndex++] = base64chars[n2];
      }
 
      /*
       * if we have all three bytes available, then their encoding is spread
       * out over four characters
       */
      if((x+2) < dataLength)
      {
         if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
         result[resultIndex++] = base64chars[n3];
      }
   }  
 
   /*
    * create and add padding that is required if we did not have a multiple of 3
    * number of characters available
    */
   if (padCount > 0) 
   { 
      for (; padCount < 3; padCount++) 
      { 
         if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
         result[resultIndex++] = '=';
      } 
   }
   if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
   result[resultIndex] = 0;
   return 1;   /* indicate success */
}

#define B64_WHITESPACE 64
#define B64_EQUALS     65
#define B64_INVALID    66
 
static const unsigned char d[] = {
    66,66,66,66,66,66,66,66,66,64,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,62,66,66,66,63,52,53,
    54,55,56,57,58,59,60,61,66,66,66,65,66,66,66, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,66,66,66,66,66,66,26,27,28,
    29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66
};
 
static int base64decode (char *in, unsigned int inLen, unsigned char *out, unsigned int *outLen) { 
  char *end = in + inLen;
  int buf = 1;
  unsigned int len = 0;
  
  while (in < end) {
    unsigned char c = d[*in++];
    
    switch (c) {
    case B64_WHITESPACE: continue;   /* skip whitespace */
    case B64_INVALID:    return 1;   /* invalid input, return error */
    case B64_EQUALS:                 /* pad character, end of data */
      in = end;
      continue;
    default:
      buf = buf << 6 | c;
      
      /* If the buffer is full, split it into bytes */
      if (buf & 0x1000000) {
	if ((len += 3) > *outLen) return 1; /* buffer overflow */
	*out++ = buf >> 16;
	*out++ = buf >> 8;
	*out++ = buf;
	buf = 1;
      }   
        }
  }
  
  if (buf & 0x40000) {
    if ((len += 2) > *outLen) return 1; /* buffer overflow */
    *out++ = buf >> 10;
    *out++ = buf >> 2;
  }
  else if (buf & 0x1000) {
    if (++len > *outLen) return 1; /* buffer overflow */
    *out++ = buf >> 4;
    }
  
  *outLen = len; /* modify to reflect the actual output size */
  return 0;
}

static int tclDynGroupToArrow(ClientData data, Tcl_Interp * interp, int objc,
                              Tcl_Obj * const objv[])
{
  DYN_GROUP *dg;
  char *dgname;
  int mode = (Tcl_Size) data;
  
  if (mode == DG_TOFROM_ARROW_FILE) {
    // dg_toArrowFile dyngroup filename
    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 1, objv, "dyngroup filename");
      return TCL_ERROR;
    }
    
    dgname = Tcl_GetStringFromObj(objv[1], NULL);
    if (tclFindDynGroup(interp, dgname, &dg) != TCL_OK) return TCL_ERROR;
    
    char *filename = Tcl_GetStringFromObj(objv[2], NULL);
    
    if (dg_to_arrow_file(dg, filename) != 0) {
      Tcl_AppendResult(interp, "dg_toArrowFile: error writing Arrow file", NULL);
      return TCL_ERROR;
    }
    
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1)); // Success
    return TCL_OK;
  }
  
  else if (mode == DG_TOFROM_ARROW_DATA) {
    // dg_toArrowData dyngroup varname
    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 1, objv, "dyngroup varname");
      return TCL_ERROR;
    }
    
    dgname = Tcl_GetStringFromObj(objv[1], NULL);
    if (tclFindDynGroup(interp, dgname, &dg) != TCL_OK) return TCL_ERROR;
    
    uint8_t *arrow_data;
    size_t arrow_size;
    
    if (dg_to_arrow_buffer(dg, &arrow_data, &arrow_size) != 0) {
      Tcl_AppendResult(interp, "dg_toArrowData: error serializing to Arrow format", NULL);
      return TCL_ERROR;
    }
    
    // Create Tcl byte array object
    Tcl_Obj *o = Tcl_NewByteArrayObj(arrow_data, arrow_size);
    free(arrow_data); // Free the allocated data
    
    // Set the variable
    if (Tcl_ObjSetVar2(interp, objv[2], NULL, o, TCL_LEAVE_ERR_MSG) == NULL) {
      return TCL_ERROR;
    }
    
    // Return the size
    Tcl_SetObjResult(interp, Tcl_NewIntObj((int)arrow_size));
    return TCL_OK;
  }
  
  Tcl_AppendResult(interp, "Invalid Arrow conversion mode", NULL);
  return TCL_ERROR;
}

// For reading Arrow files back to DYN_GROUP
static int tclDynGroupFromArrow(ClientData data, Tcl_Interp * interp, int objc,
                                Tcl_Obj * const objv[])
{
  DYN_GROUP *dg;
  char *filename;
  char *dgname;
  
  // dg_fromArrowFile filename dyngroup_name
  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "filename dyngroup_name");
    return TCL_ERROR;
  }
  
  filename = Tcl_GetStringFromObj(objv[1], NULL);
  dgname = Tcl_GetStringFromObj(objv[2], NULL);
  
  dg = arrow_file_to_dg(filename, dgname);
  if (!dg) {
    Tcl_AppendResult(interp, "dg_fromArrowFile: error reading Arrow file", NULL);
    return TCL_ERROR;
  }
  
  return (tclPutGroup(interp, dg));
}

// For reading Arrow data from byte array
static int tclDynGroupFromArrowData(ClientData data, Tcl_Interp * interp, int objc,
                                    Tcl_Obj * const objv[])
{
  DYN_GROUP *dg;
  char *dgname;
  unsigned char *arrow_data;
  Tcl_Size arrow_size;
  
  // dg_fromArrowData arrow_data_var dyngroup_name
  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "arrow_data_var dyngroup_name");
    return TCL_ERROR;
  }
  
  dgname = Tcl_GetStringFromObj(objv[2], NULL);
  
  // Get the byte array from the Tcl variable
  arrow_data = Tcl_GetByteArrayFromObj(objv[1], &arrow_size);
  if (!arrow_data) {
    Tcl_AppendResult(interp, "dg_fromArrowData: invalid arrow data", NULL);
    return TCL_ERROR;
  }
  
  dg = arrow_buffer_to_dg(arrow_data, (size_t)arrow_size, dgname);
  if (!dg) {
    Tcl_AppendResult(interp,
		     "dg_fromArrowData: error deserializing Arrow data", NULL);
    return TCL_ERROR;
  }
  
  return (tclPutGroup(interp, dg));
}


static int tclDynGroupToMsgpack(ClientData data, Tcl_Interp * interp, int objc,
                                Tcl_Obj * const objv[])
{
  DYN_GROUP *dg;
  char *dgname;
  int mode = (Tcl_Size) data;
  
  if (mode == DG_TOFROM_MSGPACK_FILE) {
    // dg_toMsgpackFile dyngroup filename
    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 1, objv, "dyngroup filename");
      return TCL_ERROR;
    }
    
    dgname = Tcl_GetStringFromObj(objv[1], NULL);
    if (tclFindDynGroup(interp, dgname, &dg) != TCL_OK) return TCL_ERROR;
    
    char *filename = Tcl_GetStringFromObj(objv[2], NULL);
    
    if (dg_write_msgpack_file(dg, filename) != 0) {
      Tcl_AppendResult(interp, "dg_toMsgpackFile: error writing MessagePack file", NULL);
      return TCL_ERROR;
    }
    
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1)); // Success
    return TCL_OK;
  }
  
  else if (mode == DG_TOFROM_MSGPACK_DATA) {
    // dg_toMsgpackData dyngroup varname
    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 1, objv, "dyngroup varname");
      return TCL_ERROR;
    }
    
    dgname = Tcl_GetStringFromObj(objv[1], NULL);
    if (tclFindDynGroup(interp, dgname, &dg) != TCL_OK) return TCL_ERROR;
    
    char *msgpack_data;
    size_t msgpack_size;
    
    if (dg_get_msgpack_data(dg, &msgpack_data, &msgpack_size) != 0) {
      Tcl_AppendResult(interp, "dg_toMsgpackData: error serializing to MessagePack format", NULL);
      return TCL_ERROR;
    }
    
    // Create Tcl byte array object
    Tcl_Obj *o = Tcl_NewByteArrayObj((unsigned char*)msgpack_data, msgpack_size);
    free(msgpack_data); // Free the allocated data
    
    // Set the variable
    if (Tcl_ObjSetVar2(interp, objv[2], NULL, o, TCL_LEAVE_ERR_MSG) == NULL) {
      return TCL_ERROR;
    }
    
    // Return the size
    Tcl_SetObjResult(interp, Tcl_NewIntObj((int)msgpack_size));
    return TCL_OK;
  }
  
  else if (mode == DG_TOFROM_MSGPACK_HYBRID_FILE) {
    // dg_toHybridMsgpackFile dyngroup filename
    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 1, objv, "dyngroup filename");
      return TCL_ERROR;
    }
    
    dgname = Tcl_GetStringFromObj(objv[1], NULL);
    if (tclFindDynGroup(interp, dgname, &dg) != TCL_OK) return TCL_ERROR;
    
    char *filename = Tcl_GetStringFromObj(objv[2], NULL);
    
    if (dg_write_hybrid_msgpack_file(dg, filename) != 0) {
      Tcl_AppendResult(interp, "dg_toHybridMsgpackFile: error writing hybrid MessagePack file", NULL);
      return TCL_ERROR;
    }
    
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1)); // Success
    return TCL_OK;
  }
  
  else if (mode == DG_TOFROM_MSGPACK_HYBRID_DATA) {
    // dg_toHybridMsgpackData dyngroup varname
    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 1, objv, "dyngroup varname");
      return TCL_ERROR;
    }
    
    dgname = Tcl_GetStringFromObj(objv[1], NULL);
    if (tclFindDynGroup(interp, dgname, &dg) != TCL_OK) return TCL_ERROR;
    
    char *msgpack_data;
    size_t msgpack_size;
    
    if (dg_get_hybrid_msgpack_data(dg, &msgpack_data, &msgpack_size) != 0) {
      Tcl_AppendResult(interp, "dg_toHybridMsgpackData: error serializing to hybrid MessagePack format", NULL);
      return TCL_ERROR;
    }
    
    // Create Tcl byte array object
    Tcl_Obj *o = Tcl_NewByteArrayObj((unsigned char*)msgpack_data, msgpack_size);
    free(msgpack_data); // Free the allocated data
    
    // Set the variable
    if (Tcl_ObjSetVar2(interp, objv[2], NULL, o, TCL_LEAVE_ERR_MSG) == NULL) {
      return TCL_ERROR;
    }
    
    // Return the size
    Tcl_SetObjResult(interp, Tcl_NewIntObj((int)msgpack_size));
    return TCL_OK;
  }
  
  Tcl_AppendResult(interp, "Invalid MessagePack conversion mode", NULL);
  return TCL_ERROR;
}

static int tclDynGroupToString(ClientData data, Tcl_Interp * interp, int objc,
			       Tcl_Obj * const objv[])
{
  Tcl_Obj * o;
  DYN_GROUP *dg;
  char *dgname;
  int oldval;			/* previous buffer increment */
  int encode64 = 0;
  json_t *json;
  char *json_str;
  int json_flags = 0;
  int row = -1;
  
  if ((Tcl_Size) data == DG_TOFROM_BASE64) encode64 = 1;

  if ((Tcl_Size) data == DG_TOFROM_JSON) {
    if (objc < 2) {
      Tcl_WrongNumArgs(interp, 1, objv, "dyngroup [row]");
      return TCL_ERROR;
    }
    if (objc > 2) {
      if (Tcl_GetIntFromObj(interp, objv[2], &row) != TCL_OK)
	return TCL_ERROR;
    }
  }
  else if ((Tcl_Size) data == DG_TOFROM_JSON_HYBRID) {

  }
  else if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "dyngroup varname");
    return TCL_ERROR;
  }
  
  dgname = Tcl_GetStringFromObj(objv[1], NULL);
  if (tclFindDynGroup(interp, dgname, &dg) != TCL_OK) return TCL_ERROR;

  /* create a json string representation */
  if ((Tcl_Size) data == DG_TOFROM_JSON) {
    if (row < 0) {
      json = dg_to_json(dg);
    }
    else {
      json = dg_element_to_json(dg, row);
    }
    if (!json) {
      Tcl_AppendResult(interp, "dg_toJSON: error creating json object", NULL);
      return TCL_ERROR;
    }
    json_str = json_dumps(json, json_flags);
    json_decref(json);
    if (!json_str) {
      Tcl_AppendResult(interp, "dg_toJSON: error dumping json string", NULL);
      return TCL_ERROR;
    }
    Tcl_SetResult(interp, json_str, TCL_VOLATILE);
    free(json_str);
    return TCL_OK;
  }

  /* create a row oriented json string representation with arrays info following */
  if ((Tcl_Size) data == DG_TOFROM_JSON_HYBRID) {
    json = dg_to_hybrid_json(dg);

    if (!json) {
      Tcl_AppendResult(interp, "dg_toHybridJSON: error creating json object", NULL);
      return TCL_ERROR;
    }
    json_str = json_dumps(json, json_flags);
    json_decref(json);
    if (!json_str) {
      Tcl_AppendResult(interp, "dg_toHybridJSON: error dumping json string", NULL);
      return TCL_ERROR;
    }
    Tcl_SetResult(interp, json_str, TCL_VOLATILE);
    free(json_str);
    return TCL_OK;
  }
  
  Tcl_MutexLock(&dgBufferMutex);
   
  dgInitBuffer();
  oldval = dgSetBufferIncrement(dgEstimateGroupSize(dg));
  dgRecordDynGroup(dg);    
  if (!encode64) {
    o = Tcl_NewByteArrayObj(dgGetBuffer(), dgGetBufferSize());
  }
  else {			/* base 64 encoded as ascii string */
    char *encoded_data;
    int encoded_length, result;
    encoded_length = (((dgGetBufferSize()/3) + (dgGetBufferSize() % 3 > 0)) * 4);
    encoded_data = (char *) calloc(encoded_length, sizeof(char));
    result =  base64encode(dgGetBuffer(),
			   dgGetBufferSize(), encoded_data, encoded_length);
    o = Tcl_NewStringObj(encoded_data, encoded_length);
    free(encoded_data);
  }
  dgCloseBuffer();
  dgSetBufferIncrement(oldval);

  Tcl_MutexUnlock(&dgBufferMutex);

  if (Tcl_ObjSetVar2(interp, objv[2], NULL, o,
		     TCL_LEAVE_ERR_MSG) == NULL)
    return TCL_ERROR;
  
  Tcl_SetObjResult(interp, Tcl_NewIntObj(Tcl_GetCharLength(o)));
  return TCL_OK;
}

static int tclDynListToString(ClientData data, Tcl_Interp * interp, int objc,
			      Tcl_Obj * const objv[])
{
  Tcl_Obj * o;
  DYN_LIST *dl;
  char *dlname;
  int encode64 = 0;
  int nbytes;
  int element = -1;

  json_t *json;
  char *json_str;
  int json_flags = 0;

  if ((Tcl_Size) data == DL_TOFROM_BASE64) encode64 = 1;

  if ((Tcl_Size) data == DL_TOFROM_JSON) {
    if (objc < 2) {
      Tcl_WrongNumArgs(interp, 1, objv, "dynlist [element]");
      return TCL_ERROR;
    }
    if (objc > 2) {
      if (Tcl_GetIntFromObj(interp, objv[2], &element) != TCL_OK)
	return TCL_ERROR;
    }
  }
  else {
    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 1, objv, "dynlist varname");
      return TCL_ERROR;
    }
  }
  
  dlname = Tcl_GetStringFromObj(objv[1], NULL);
  if (tclFindDynList(interp, dlname, &dl) != TCL_OK) return TCL_ERROR;

  /* create a json string representation */
  if ((Tcl_Size) data == DL_TOFROM_JSON) {
    if (element == -1)
      json = dl_to_json(dl);
    else
      json = dl_element_to_json(dl, element);
    
    if (!json) {
      Tcl_AppendResult(interp, "dl_toJSON: error creating json object", NULL);
      return TCL_ERROR;
    }
    if (json_is_array(json)) {
      json_str = json_dumps(json, json_flags);
      json_decref(json);
      
      if (!json_str) {
	Tcl_AppendResult(interp, "dl_toJSON: error dumping json string", NULL);
	return TCL_ERROR;
      }
      Tcl_SetResult(interp, json_str, TCL_VOLATILE);
      free(json_str);
    }
    else if (json_is_string(json)) {
      Tcl_SetObjResult(interp, Tcl_NewStringObj(json_string_value(json),
						json_string_length(json)));
      json_decref(json);
    }
    else if (json_is_integer(json)) {
      Tcl_SetObjResult(interp, Tcl_NewIntObj(json_integer_value(json)));
      json_decref(json);
    }	
    else if (json_is_real(json)) {
      Tcl_SetObjResult(interp, Tcl_NewDoubleObj(json_integer_value(json)));
      json_decref(json);
    }
    else {
      Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
		       ": unable to format JSON element", NULL);
      return TCL_ERROR;
    }
    return TCL_OK;
  }

  
  if (DYN_LIST_DATATYPE(dl) == DF_LIST || DYN_LIST_DATATYPE(dl) == DF_STRING) {
    Tcl_SetResult(interp, "dl_toString: invalid datatype", NULL);
    return TCL_ERROR;
  }

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
  case DF_FLOAT:
    nbytes = 4*DYN_LIST_N(dl);
    break;
  case DF_SHORT:
    nbytes = 2*DYN_LIST_N(dl);
    break;
  case DF_STRING:
    nbytes = 8*DYN_LIST_N(dl);	/* just a guess */
    break;
  case DF_CHAR:
    nbytes = DYN_LIST_N(dl);
    break;
  }
  
  if (!encode64) {
    o = Tcl_NewByteArrayObj(DYN_LIST_VALS(dl), nbytes);
  }

  else {			/* base 64 encoded as ascii string */
    char *encoded_data;
    int encoded_length, result;
    encoded_length = (((nbytes/3) + (nbytes % 3 > 0)) * 4);
    
    encoded_data = (char *) calloc(encoded_length, sizeof(char));
    result =  base64encode(DYN_LIST_VALS(dl), nbytes,
			   encoded_data, encoded_length);
    o = Tcl_NewStringObj(encoded_data, encoded_length);
    free(encoded_data);
  }

  if (Tcl_ObjSetVar2(interp, objv[2], NULL, o,
		     TCL_LEAVE_ERR_MSG) == NULL)
    return TCL_ERROR;
  
  Tcl_SetObjResult(interp, Tcl_NewIntObj(Tcl_GetCharLength(o)));
  return TCL_OK;
}

static int tclDynGroupFromString(ClientData cdata, Tcl_Interp * interp, 
				 int objc, Tcl_Obj * const objv[])
{
  DYN_GROUP *dg;
  Tcl_HashEntry *entryPtr;
  int newentry;
  char *newname = NULL;
  unsigned char *data;
  Tcl_Size length;
  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  int encode64 = 0;
  if ((Tcl_Size) cdata == DG_TOFROM_BASE64) encode64 = 1;
  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "string [newname]");
    return TCL_ERROR;
  }
  
  if (!encode64)
    data = (unsigned char *) Tcl_GetByteArrayFromObj(objv[1], &length);
  else
    data = (unsigned char *) Tcl_GetStringFromObj(objv[1], &length);
  
  if (!data) {
    Tcl_AppendResult(interp, "dg_fromString: invalid dg string", NULL);
    return TCL_ERROR;
  }
  if (length < 5) {
    Tcl_AppendResult(interp, "dg_fromString: invalid dg string", NULL);
    return TCL_ERROR;
  }
  if (objc > 2) {
    newname = Tcl_GetStringFromObj(objv[2], NULL);
    if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, newname))) {
      DYN_GROUP *dgold;
      if ((dgold = Tcl_GetHashValue(entryPtr))) dfuFreeDynGroup(dgold);
      Tcl_DeleteHashEntry(entryPtr);
    }
  }
  if (!(dg = dfuCreateDynGroup(4))) {
    Tcl_SetResult(interp, "dg_fromString: error creating new dyngroup",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  
  /* dynio code not thread safe so need to protect this section */
  Tcl_MutexLock(&dgBufferMutex);
  
  if (!encode64) {
    if (dguBufferToStruct(data, length, dg) != DF_OK) {
      Tcl_MutexUnlock(&dgBufferMutex);  // Don't forget to unlock on error!
      dfuFreeDynGroup(dg);  // Clean up the allocated dg
      Tcl_SetResult(interp, "dg_fromString: file not recognized as dg format",
		    TCL_STATIC);
      return TCL_ERROR;
    }
  }
  else {
    unsigned char *decoded_data;
    unsigned int decoded_length = length;
    int result;
    decoded_data = calloc(decoded_length, sizeof(char));
    result = base64decode((char *) data, length, decoded_data, &decoded_length);
    if (result) {
      free(decoded_data);
      Tcl_MutexUnlock(&dgBufferMutex);  // Unlock on error
      dfuFreeDynGroup(dg);  // Clean up
      char resultstr[128];
      snprintf(resultstr, sizeof(resultstr),
	       "dg_fromString64: error decoding data (%d/%d bytes)", 
	       (int) length, decoded_length);
      Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
      return TCL_ERROR;
    }
    if (dguBufferToStruct(decoded_data, decoded_length, dg) != DF_OK) {
      free(decoded_data);
      Tcl_MutexUnlock(&dgBufferMutex);  // Unlock on error
      dfuFreeDynGroup(dg);  // Clean up
      Tcl_SetResult(interp,
		    "dg_fromString64: file not recognized as dg format",
		    TCL_STATIC);
      return TCL_ERROR;
    }
    free(decoded_data);
  }
  
  Tcl_MutexUnlock(&dgBufferMutex);
  
  if (newname) strncpy(DYN_GROUP_NAME(dg), newname, DYN_GROUP_NAME_SIZE-1);
  if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, DYN_GROUP_NAME(dg)))) {
    DYN_GROUP *dgold;
    if ((dgold = Tcl_GetHashValue(entryPtr))) {
      dfuFreeDynGroup(dgold);
    }
    Tcl_DeleteHashEntry(entryPtr);
  }
  /*
   * Add to hash table which contains list of open dyngroups
   */
  entryPtr = Tcl_CreateHashEntry(&dlinfo->dgTable, DYN_GROUP_NAME(dg), &newentry);
  Tcl_SetHashValue(entryPtr, dg);
  
  Tcl_SetResult(interp, DYN_GROUP_NAME(dg), TCL_VOLATILE);
  return TCL_OK;
}

static int tclDynListFromString(ClientData cdata, Tcl_Interp * interp, 
				int objc, Tcl_Obj * const objv[])
{
  DYN_LIST *dl;
  unsigned char *data;
  char *dlname;
  Tcl_Size length;
  int i;

  int encode64 = 0;
  if ((Tcl_Size) cdata == DG_TOFROM_BASE64) encode64 = 1;

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "string dl");
    return TCL_ERROR;
  }

  dlname = Tcl_GetStringFromObj(objv[2], NULL);
  if (tclFindDynList(interp, dlname, &dl) != TCL_OK) return TCL_ERROR;
  if (DYN_LIST_DATATYPE(dl) == DF_LIST ||
      DYN_LIST_DATATYPE(dl) == DF_STRING) {
    Tcl_AppendResult(interp, "dl_fromString: data type not supported", NULL);
    return TCL_ERROR;
  }
  
  if (!encode64)
    data = (unsigned char *) Tcl_GetByteArrayFromObj(objv[1], &length);
  else
    data = (unsigned char *) Tcl_GetStringFromObj(objv[1], &length);
  
  if (!data) {
    Tcl_AppendResult(interp, "dl_fromString: invalid dl string", NULL);
    return TCL_ERROR;
  }

  if (!encode64) {
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG:
      if (length % 4 == 0) {
	dfuResetDynList(dl);
	for (i = 0; i < length; i+=4)
	  dfuAddDynListLong(dl, *((int *) &data[i]));
      }
      else {
	Tcl_AppendResult(interp, "dl_fromString: invalid data", NULL);
	return TCL_ERROR;
      }
      break;
    case DF_FLOAT:
      if (length % 4 == 0) {
	dfuResetDynList(dl);
	for (i = 0; i < length; i+=4)
	  dfuAddDynListFloat(dl, *((float *) &data[i]));
      }
      else {
	Tcl_AppendResult(interp, "dl_fromString: invalid data", NULL);
	return TCL_ERROR;
      }
      break;
    case DF_SHORT:
      if (length % 2 == 0) {
	dfuResetDynList(dl);
	for (i = 0; i < length; i+=2)
	  dfuAddDynListShort(dl, *((short *) &data[i]));
      }
      else {
	Tcl_AppendResult(interp, "dl_fromString: invalid data", NULL);
	return TCL_ERROR;
      }
      break;
    case DF_CHAR:
      dfuResetDynList(dl);
      for (i = 0; i < length; i++)
	dfuAddDynListChar(dl, data[i]);
      break;
    }
  }
  else {
    unsigned char *decoded_data;
    unsigned int decoded_length = length;
    int result;

    decoded_data = calloc(decoded_length, sizeof(char));
    result = base64decode((char *) data, length, decoded_data, &decoded_length);

    if (result) {
      free(decoded_data);
      char resultstr[128];
      snprintf(resultstr, sizeof(resultstr),
	       "dl_fromString64: error decoding data (%d/%d bytes)", 
	       (int) length, decoded_length);
      Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
      return TCL_ERROR;
    }
    
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG:
      if (decoded_length % 4 == 0) {
	dfuResetDynList(dl);
	for (i = 0; i < decoded_length; i+=4)
	  dfuAddDynListLong(dl, *((int *) &decoded_data[i]));
      }
      else {
	Tcl_AppendResult(interp, "dl_fromString64: invalid data", NULL);
	return TCL_ERROR;
      }
      break;
    case DF_FLOAT:
      if (decoded_length % 4 == 0) {
	dfuResetDynList(dl);
	for (i = 0; i < decoded_length; i+=4)
	  dfuAddDynListFloat(dl, *((float *) &decoded_data[i]));
      }
      else {
	char resultstr[128];
	snprintf(resultstr, sizeof(resultstr),
		 "dl_fromString64: error decoding data (%d/%d bytes)", 
		 (int) length, decoded_length);
	Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
	return TCL_ERROR;
      }
      break;
    case DF_SHORT:
      if (decoded_length % 2 == 0) {
	dfuResetDynList(dl);
	for (i = 0; i < decoded_length; i+=2)
	  dfuAddDynListShort(dl, *((short *) &decoded_data[i]));
      }
      else {
	Tcl_AppendResult(interp, "dl_fromString64: invalid data", NULL);
	return TCL_ERROR;
      }
      break;
    case DF_CHAR:
      dfuResetDynList(dl);
      for (i = 0; i < decoded_length; i++)
	dfuAddDynListChar(dl, decoded_data[i]);
      break;
    }
    free(decoded_data);
  }
  Tcl_SetResult(interp, dlname, TCL_VOLATILE);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclReadDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_read
 *
 * DESCRIPTION
 *    Reads in a dynGroup
 *
 *****************************************************************************/

/* ===========================================================================
 * Uncompress input to output then close both files.
 */
static void gz_uncompress(gzFile in, FILE *out)
{
    char buf[2048];
    int len;

    for (;;) {
        len = gzread(in, buf, sizeof(buf));
        if (len < 0) return;
        if (len == 0) break;

        if ((int)fwrite(buf, 1, (unsigned)len, out) != len) {
	  return;
	}
    }
    if (fclose(out)) return;
    if (gzclose(in) != Z_OK) return;
}

static FILE *uncompress_file(char *filename, char *tempname)
{
  FILE *fp;
  gzFile in;
  int fd;
#ifdef WIN32
  static char *tmpdir = "c:/windows/temp";
  char *fname;
#else
  char fname[] = "/tmp/dgXXXXXX";
#endif

  if (!filename) return NULL;

  if (!(in = gzopen(filename, "rb"))) {
    sprintf(tempname, "file %s not found", filename);
    return 0;
  }

#ifdef WIN32
  fname = tempnam(tmpdir, "dg");
#else
  fd = mkstemp(fname);
  if (fd == -1) {
    strcpy(tempname, "error creating temp file for decompression");
    return 0;
  }
  close(fd);
#endif

  if (!(fp = fopen(fname,"wb"))) {
    strcpy(tempname, "error opening temp file for decompression");
    return 0;
  }
  
  gz_uncompress(in, fp);

  /* DONE in gz_uncompress!  fclose(fp); */

  fp = fopen(fname, "rb");
  if (tempname) strcpy(tempname, fname);

#ifdef WIN32
  free(fname);
#else
  unlink(fname);  /* Remove the temporary file when done */
#endif

  return(fp);
}  


static int tclReadDynGroup (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  DYN_GROUP *dg;
  FILE *fp;
  Tcl_HashEntry *entryPtr;
  int newentry;
  char *newname = NULL, *suffix;
  char tempname[128];

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " file [newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (argc > 2) {
    newname = argv[2];
    if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, newname))) {
      DYN_GROUP *dgold;
      if ((dgold = Tcl_GetHashValue(entryPtr))) dfuFreeDynGroup(dgold);
      Tcl_DeleteHashEntry(entryPtr);
    }
  }

  if (!(dg = dfuCreateDynGroup(4))) {
    Tcl_SetResult(interp, "dg_read: error creating new dyngroup", TCL_STATIC);
    return TCL_ERROR;
  }

  /* No need to uncompress a .dg file */
  if ((suffix = strrchr(argv[1], '.')) && strstr(suffix, "dg") &&
      !strstr(suffix, "dgz")) {
    fp = fopen(argv[1], "rb");
    if (!fp) {
      char resultstr[256];
      snprintf(resultstr, sizeof(resultstr),
	       "dg_read: file %s not found", argv[1]);
      Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
      return TCL_ERROR;
    }
    tempname[0] = 0;
  }
  
  else if ((suffix = strrchr(argv[1], '.')) &&
	   strlen(suffix) == 4 &&
	   ((suffix[1] == 'l' && suffix[2] == 'z' && suffix[3] == '4') ||
	    (suffix[1] == 'L' && suffix[2] == 'Z' && suffix[3] == '4'))) {
    if (dgReadDynGroup(argv[1], dg) == DF_OK) {
      goto process_dg;
    }
    else {
      Tcl_SetResult(interp, "dg_read: error reading .lz4 file", NULL);
      return TCL_ERROR;
    }
  }

  else if ((fp = uncompress_file(argv[1], tempname)) == NULL) {
    char fullname[128];
    sprintf(fullname,"%s.dg", argv[1]);
    if ((fp = uncompress_file(fullname, tempname)) == NULL) {
      sprintf(fullname,"%s.dgz", argv[1]);
      if ((fp = uncompress_file(fullname, tempname)) == NULL) {
	char resultstr[256];
	if (tempname[0] == 'f') { /* 'f'ile not found...*/
	  snprintf(resultstr, sizeof(resultstr),
		   "dg_read: file \"%s\" not found", argv[1]);
	}
	else {
	  snprintf(resultstr, sizeof(resultstr),
		   "dg_read: error opening tempfile");
	}
	Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
	return TCL_ERROR;
      }
    }
  }

  if (!dguFileToStruct(fp, dg)) {
    char resultstr[256];
    snprintf(resultstr, sizeof(resultstr),
	     "dg_read: file %s not recognized as dg format", 
	     argv[1]);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
    fclose(fp);
    if (tempname[0]) unlink(tempname);
    return TCL_ERROR;
  }
  fclose(fp);
  if (tempname[0]) unlink(tempname);

 process_dg:
  if (newname) strncpy(DYN_GROUP_NAME(dg), newname, DYN_GROUP_NAME_SIZE-1);
  if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, DYN_GROUP_NAME(dg)))) {
    DYN_GROUP *dgold;
    if ((dgold = Tcl_GetHashValue(entryPtr))) {
      dfuFreeDynGroup(dgold);
    }
    Tcl_DeleteHashEntry(entryPtr);
  }

  /*
   * Add to hash table which contains list of open dyngroups
   */
  entryPtr = Tcl_CreateHashEntry(&dlinfo->dgTable, DYN_GROUP_NAME(dg), &newentry);
  Tcl_SetHashValue(entryPtr, dg);
  
  Tcl_SetResult(interp, DYN_GROUP_NAME(dg), TCL_VOLATILE);
  return TCL_OK;
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclDeleteDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_delete
 *
 * DESCRIPTION
 *    Deletes an existing dyngroup
 *
 *****************************************************************************/

static int tclDeleteDynGroup (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  DYN_GROUP *dg;
  Tcl_HashEntry *entryPtr;
  int i;
  int operation = (Tcl_Size) data;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if (operation == DG_DELETE_TEMPS) {
    Tcl_HashSearch search;
    char *name;
    for (entryPtr = Tcl_FirstHashEntry(&dlinfo->dgTable, &search);
	 entryPtr != NULL;
	 entryPtr = Tcl_NextHashEntry(&search)) {
      name = Tcl_GetHashKey(&dlinfo->dgTable, entryPtr);
      if (!strncmp(name,"group",5)) {
	Tcl_VarEval(interp, "dg_delete ", name, (char *) NULL);
      }
    }
    dlinfo->dgCount = 0;
  }

  else if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dyngroup(s)", NULL);
    return TCL_ERROR;
  }

  for (i = 1; i < argc; i++) {
    if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, argv[i]))) {
      if ((dg = Tcl_GetHashValue(entryPtr))) dfuFreeDynGroup(dg);
      Tcl_DeleteHashEntry(entryPtr);
    }
  
    /* 
     * If user has specified "dg_delete ALL" iterate through the hash table
     * and recursively call this function to close all open dyngroups
     */
    else if (!strcasecmp(argv[i],"ALL")) {
      Tcl_HashSearch search;
      for (entryPtr = Tcl_FirstHashEntry(&dlinfo->dgTable, &search);
	   entryPtr != NULL;
	   entryPtr = Tcl_NextHashEntry(&search)) {
	Tcl_VarEval(interp, "dg_delete ", Tcl_GetHashKey(&dlinfo->dgTable, entryPtr),
		    (char *) NULL);
      }
      dlinfo->dgCount = 0;
    }
    else {
      Tcl_AppendResult(interp, "dg_delete: dyngroup \"", argv[i], 
		       "\" not found", (char *) NULL);
      return TCL_ERROR;
    }
  }
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclAddNewListDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_addNewList
 *
 * DESCRIPTION
 *    Appends a new list to a dynGroup.  The newly created list is NOT
 * added to the hash table of open dynLists.  These lists are managed
 * by the dynGroup and are deleted when the dynGroup is deleted.
 *
 *****************************************************************************/

static int tclAddNewListDynGroup (ClientData data, Tcl_Interp *interp,
				  int argc, char *argv[])
{
  DYN_GROUP *dg;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  int datatype = DF_LONG, increment = dlinfo->DefaultListIncrement;
  char listname[64];

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " dyngroup name [[datatype] [size]]", NULL);
    return TCL_ERROR;
  }

  if (tclFindDynGroup(interp, argv[1], &dg) != TCL_OK) return TCL_ERROR;
  strncpy(listname, argv[2], 63);

  /* Make sure a list called name doesn't already exist inside the group */

  if (dynGroupFindList(dg, listname)) {
    Tcl_AppendResult(interp, "dg_addNewList: dynlist \"", listname, 
		     "\" already exists in dyngroup \"", argv[1], "\"", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (argc > 3) {
    if (!dynGetDatatypeID(argv[3], &datatype)) {
      Tcl_SetResult(interp, "dg_addNewList: bad datatype", TCL_STATIC);
      return TCL_ERROR;
    }
  }
  if (argc > 4) {
    if (Tcl_GetInt(interp, argv[4], &increment) != TCL_OK) {
      return TCL_ERROR;
    }
  }
  if (increment <= 0) {
    Tcl_SetResult(interp, "dg_addNewList: bad initial size", TCL_STATIC);
    return TCL_ERROR;
  }
  
  dfuAddDynGroupNewList(dg, listname, datatype, increment);

  Tcl_AppendResult(interp, argv[1], ":", listname, (char *) NULL);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclAddExistingListDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_addExistingList
 *
 * DESCRIPTION
 *    Appends an existing list to a dynGroup.  The existing list is
 * removed from the hash table, as it is transferred to the new group.
 *
 *    Lists from other groups are not available to this function
 * (but see dg_copyExistingList).
 *
 *****************************************************************************/

static int tclAddExistingListDynGroup (ClientData data, Tcl_Interp *interp,
				       int argc, char *argv[])
{
  DYN_GROUP *dg;
  DYN_LIST *dl;
  Tcl_HashEntry *entryPtr;
  char oldname[64], newname[64];
  int operation = (Tcl_Size) data;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dyngroup name [newname]",
		     NULL);
    return TCL_ERROR;
  }

  if (tclFindDynGroup(interp, argv[1], &dg) != TCL_OK) return TCL_ERROR;
  strncpy(oldname, argv[2], 63);

  if (argc > 3) strncpy(newname, argv[3], 63);
  else strcpy(newname, oldname);

  /* Make sure a list called name doesn't already exist inside the group */

  if (dynGroupFindList(dg, newname)) {
    Tcl_AppendResult(interp, "dg_addNewList: dynlist \"", newname, 
		     "\" already exists in dyngroup \"", argv[1], "\"", 
		     (char *) NULL);
    return TCL_ERROR;
  }


  switch (operation) {
  case DG_MOVE:
    if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, oldname))) {
      dl = Tcl_GetHashValue(entryPtr);
      Tcl_DeleteHashEntry(entryPtr);
    }
    else {
      char *colon;
      if ((colon = strchr(oldname,':'))) {
	if (tclFindDynList(interp, oldname, &dl) != TCL_OK) {
	  return TCL_ERROR;
	}
	/* peel off group if new name isn't specified */
	/* oldname becomes the newname here...        */
	if ((colon = strchr(newname,':'))) strncpy(oldname, colon+1, 63);
	else strcpy(oldname, newname);
	dfuCopyDynGroupExistingList(dg, oldname, dl);
	Tcl_AppendResult(interp, argv[1], ":", oldname, (char *) NULL);
	return TCL_OK;
      }
      else {
	Tcl_AppendResult(interp, "dynlist \"", oldname, "\" not found", 
			 (char *) NULL);
      }
      return TCL_ERROR;
    }
    dfuAddDynGroupExistingList(dg, newname, dl);
    break;
  case DG_COPY:
    if (tclFindDynList(interp, oldname, &dl) != TCL_OK) {
      return TCL_ERROR;
    }
    dfuCopyDynGroupExistingList(dg, newname, dl);
    break;
  }

  Tcl_AppendResult(interp, argv[1], ":", newname, (char *) NULL);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclResetDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_reset
 *
 * DESCRIPTION
 *    Resets an existing dynGroup
 *
 *****************************************************************************/

static int tclResetDynGroup (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  DYN_GROUP *dg;
  int i;

  if (argc < 1) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dyngroup(s)", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  for (i = 1; i < argc; i++) {
    if (tclFindDynGroup(interp, argv[i], &dg) != TCL_OK) return TCL_ERROR;
    dfuResetDynGroup(dg);
  }

  Tcl_SetResult(interp, DYN_GROUP_NAME(dg), TCL_VOLATILE);
  return TCL_OK;
}




/*****************************************************************************
 *
 * FUNCTION
 *    tclRemoveDynGroupList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_remove
 *
 * DESCRIPTION
 *    Remove a list from a dynGroup
 *
 *****************************************************************************/

static int tclRemoveDynGroupList (ClientData data, Tcl_Interp *interp,
				  int argc, char *argv[])
{
  DYN_GROUP *dg;
  char name[64];
  
  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dyngroup list", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynGroup(interp, argv[1], &dg) != TCL_OK) return TCL_ERROR;

  strncpy(name, DYN_GROUP_NAME(dg), sizeof(name));
  
  if (!dynGroupRemoveList(dg, argv[2])) {
    Tcl_AppendResult(interp, argv[0], ": list \"", argv[2],
		     "\" not found in group \"", argv[1], "\"", (char *) NULL);
    return TCL_ERROR;
  }

  Tcl_SetResult(interp, name, TCL_VOLATILE);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclSetFormat
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_setFormat
 *
 * DESCRIPTION
 *    Set the output format string for a particular datatype
 *
 *****************************************************************************/

static int tclSetFormat(ClientData data, Tcl_Interp *interp,
			int argc, char *argv[])
{
  int datatype;
  char *old;
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " datatype format-string", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (!dynGetDatatypeID(argv[1], &datatype)) {
    Tcl_AppendResult(interp, argv[0], ": bad datatype", (char *) NULL);
    return TCL_ERROR;
  }

  old = dynListSetFormat(datatype, argv[2]);
  if (!old) {
    Tcl_AppendResult(interp, argv[0], ": error setting format string", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  Tcl_SetResult(interp, old, TCL_VOLATILE);
  return TCL_OK;
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclDumpDynGroup
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dg_dump
 *    dg_dumpListNames
 *
 * DESCRIPTION
 *    Dumps an existing dyngroup to stdout or to an open file stream
 *
 *****************************************************************************/

static int tclDumpDynGroup (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  DYN_GROUP *dg;
  int mode;
  int output_format = (Tcl_Size) data;
  int separator = '\t';
  Tcl_Channel outChannel;


  /* Create a tcllist of dyngroup list names */
  if (output_format == DG_TCL_LISTNAMES || output_format == DG_LISTNAMES) {
    int i;
    DYN_LIST *namelist;

    if (argc < 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dyngroup", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    
    if (tclFindDynGroup(interp, argv[1], &dg) != TCL_OK) return TCL_ERROR;
    if (!DYN_GROUP_NLISTS(dg)) return TCL_OK;
    
    namelist = dfuCreateDynList(DF_STRING, DYN_GROUP_NLISTS(dg));
    for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
      dfuAddDynListString(namelist, DYN_LIST_NAME(DYN_GROUP_LIST(dg,i)));
    }    

    if (output_format == DG_TCL_LISTNAMES) {
      tclDynListToTclList(interp, namelist);
      dfuFreeDynList(namelist);
      return TCL_OK;
    }
    else return tclPutList(interp, namelist);
  }

  outChannel = NULL ;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dyngroup [fileID]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynGroup(interp, argv[1], &dg) != TCL_OK) return TCL_ERROR;
  if (!DYN_GROUP_NLISTS(dg)) return TCL_OK;
  
  if (argc > 2) {
    if ((outChannel = Tcl_GetChannel(interp, argv[2], &mode)) == NULL) {
      return TCL_ERROR;
    }
  }
  if (argc > 3) {
    if (Tcl_GetInt(interp, argv[3], &separator) != TCL_OK) {
      return TCL_ERROR;
    }
    else if (separator > 255) {
      Tcl_AppendResult(interp, argv[0], 
		       ": separator must be between 0 and 255",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  switch (output_format) {
  case DG_DUMP:
    dynGroupChanDump(interp, dg, separator, outChannel);
    break;
  case DG_DUMP_LIST_NAMES:
    dynGroupChanDumpListNames(interp, dg, outChannel);
    break;
  }
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclFindDynGroup
 *
 * ARGS
 *    Tcl_Interp *interp, char *name, DYN_GROUP **
 *
 * DESCRIPTION
 *    Searches for dyngroup called name.  If dg is not null, it places
 * a pointer to the dyngroup there.
 *
 *****************************************************************************/

int tclFindDynGroup(Tcl_Interp *interp, char *name, DYN_GROUP **dg)
{
  DYN_GROUP *g;
  Tcl_HashEntry *entryPtr;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, name))) {
    g = Tcl_GetHashValue(entryPtr);
    if (!g) {
      Tcl_SetResult(interp, "bad dyngroup ptr in hash table", TCL_STATIC);
      return TCL_ERROR;
    }
    if (dg) *dg = g;
    return TCL_OK;
  }
  else {
    char outname[64];
    strncpy(outname, name, 63);
    Tcl_AppendResult(interp, "dyngroup \"", outname, "\" not found", 
		     (char *) NULL);
    return TCL_ERROR;
  }
}




/*****************************************************************************
 ***************************  DynList Functions  *****************************
 *****************************************************************************/



/*****************************************************************************
 *
 * FUNCTION
 *    tclLoadPackage
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_pkg
 *
 * DESCRIPTION
 *    Setup an extension package (actually ensure that the dlsh library
 * is loaded and then just call package require).
 *
 *****************************************************************************/

static int tclLoadPackage (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " package", (char *) NULL);
    return TCL_ERROR;
  }
  return
    Tcl_VarEval(interp, "package require ", argv[1], (char *) NULL);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclSetMatherr
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_setMatherr
 *
 * DESCRIPTION
 *    Set a flag for dynlist functions to check their args before
 * applying math functions
 *
 *****************************************************************************/

static int tclSetMatherr (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  int status;
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " status", (char *) NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &status) != TCL_OK)  return TCL_ERROR;
  Tcl_SetObjResult(interp, Tcl_NewIntObj(dynListSetMatherrCheck(status)));
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclDynListDir
 *
 * ARGS
 *    Tcl_Interp *interp
 *
 * DESCRIPTION
 *    Returns a list of all existing dynlists.
 *
 *****************************************************************************/

static int tclDynListDir (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  Tcl_HashEntry *entryPtr;
  Tcl_HashSearch searchEntry;
  Tcl_DString dirList;
  DYN_LIST *dl;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  

  Tcl_DStringInit(&dirList);
  entryPtr = Tcl_FirstHashEntry(&dlinfo->dlTable, &searchEntry);

  if (entryPtr) {
    do {
      char buf[16];
      Tcl_DStringStartSublist(&dirList);
      dl = (DYN_LIST *) Tcl_GetHashValue(entryPtr);
      Tcl_DStringAppendElement(&dirList, Tcl_GetHashKey(&dlinfo->dlTable, entryPtr));
      Tcl_DStringAppendElement(&dirList, 
			       dynGetDatatypeName(DYN_LIST_DATATYPE(dl)));
      sprintf(buf, "%d", DYN_LIST_N(dl));
      Tcl_DStringAppendElement(&dirList, buf);
      sprintf(buf, "%d", DYN_LIST_MAX(dl));
      Tcl_DStringAppendElement(&dirList, buf);
      Tcl_DStringEndSublist(&dirList);
    } while ((entryPtr = Tcl_NextHashEntry(&searchEntry)) != NULL);
  }
  Tcl_DStringResult(interp, &dirList);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclDynListExists
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_exists
 *
 * DESCRIPTION
 *    Returns 1 if dynList exists
 *
 *****************************************************************************/

static int tclDynListExists (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  if (argc < 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " name", NULL);
      return TCL_ERROR;
  }

  if (tclFindDynList(interp, argv[1], NULL) == TCL_OK)
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
  else 
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclDynListSublist
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_sublist
 *
 * DESCRIPTION
 *    Returns 1 if dynList is a sublist
 *
 *****************************************************************************/

static int tclDynListSublist (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  DYN_LIST *dl;

  if (argc < 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist", NULL);
      return TCL_ERROR;
  }

  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK)
    Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
  else if (DYN_LIST_FLAGS(dl) & DL_SUBLIST)
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
  else
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclDynListDatatype
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_datatype
 *
 * DESCRIPTION
 *    Returns string corresponding to datatype contained in list
 *
 *****************************************************************************/

static int tclDynListDatatype (ClientData data, Tcl_Interp *interp,
			       int argc, char *argv[])
{
  DYN_LIST *dl;
  char *name;
  int mode = (Tcl_Size) data;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  switch (mode) {
  case DL_DATATYPE:
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
    name = dynGetDatatypeName(DYN_LIST_DATATYPE(dl));
    if (name) Tcl_SetObjResult(interp, Tcl_NewStringObj(name, -1));
    else Tcl_SetResult(interp, "dl_datatype: unknown dynList datatype",
		       TCL_STATIC);
    return TCL_OK;
    break;
  case DL_IS_MATRIX:
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "matrix \"", argv[1], "\" not found",
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (tclIsDynMatrix(interp, dl) != TCL_OK)
      Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    else
      Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
    return TCL_OK;
    break;
  }
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclCreateDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_create
 *
 * DESCRIPTION
 *    Creates a new dynlist
 *
 *****************************************************************************/

static int tclCreateDynList (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  DYN_LIST *dl;
  Tcl_HashEntry *entryPtr;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;

  int newentry, datatype = DF_LONG, increment = dlinfo->DefaultListIncrement;
  char listname[256];
  int i, startindex;		/* first optional arg */

  if (data == 0) {		/* dl_create */
    if (argc > 1) {
      if (!dynGetDatatypeID(argv[1], &datatype)) {
	Tcl_SetResult(interp, "dl_create: bad datatype", TCL_STATIC);
	return TCL_ERROR;
      }
    }
    startindex = 2;
  } 
  else {
    datatype = (Tcl_Size) data;
    startindex = 1;
  }

  /* One of two places new temporary lists are created */
  /*   also see tclPutList, which converts an existing */
  /*   dynlist into a dynlist for dlsh...              */
  if (dlinfo->TmpListRecordList) dfuAddDynListLong(dlinfo->TmpListRecordList, dlinfo->dlCount);
  sprintf(listname, "%%list%d%%", dlinfo->dlCount++);

  if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, listname))) {
    Tcl_AppendResult(interp,
		     "dl_create: list ", listname, " already exists", NULL);
    return TCL_ERROR;
  }

  if (!(dl = dfuCreateNamedDynList(listname, datatype, increment))) {
    Tcl_SetResult(interp, "dl_create: error creating new dynlist", TCL_STATIC);
    return TCL_ERROR;
  }

  /*
   * Add to hash table which contains list of open dynlists
   */
  entryPtr = Tcl_CreateHashEntry(&dlinfo->dlTable, listname, &newentry);
  Tcl_SetHashValue(entryPtr, dl);

  /* NEW!!! */
  /* Add a local variable that will, when the current proc exits, free list */
  Tcl_SetVar(interp, listname, listname, 0);
  Tcl_TraceVar(interp, listname, 
	       TCL_TRACE_WRITES | TCL_TRACE_UNSETS, 
	       (Tcl_VarTraceProc *) tclDeleteLocalDynList, 
	       (ClientData) strdup(listname));
  
  for (i = startindex; i < argc; i++) {
    if (argv[i][0]) {		/* as int as it's not the empty string */
      if (Tcl_VarEval(interp, "dl_append ", listname, " {", argv[i], "}", 
		      (char *) NULL) != TCL_OK) {
	return TCL_ERROR;
      }
    }
  }
  
  Tcl_SetResult(interp, listname, TCL_VOLATILE);
  return TCL_OK;
}

int tclPutList(Tcl_Interp *interp, DYN_LIST *dl) 
{
  Tcl_HashEntry *entryPtr;
  int newentry;
  char listname[128];

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;

  if (dlinfo->TmpListRecordList) dfuAddDynListLong(dlinfo->TmpListRecordList, dlinfo->dlCount);
  sprintf(listname, "%%list%d%%", dlinfo->dlCount++);

  if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, listname))) {
    Tcl_AppendResult(interp, "dl_create: list ",
		     listname, " already exists", NULL);
    return TCL_ERROR;
  }
  
  if (!dl) {
    Tcl_SetResult(interp, "tclPutList: attempted to add NULL list ptr",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  if (listname != DYN_LIST_NAME(dl))
    strncpy(DYN_LIST_NAME(dl), listname, DYN_LIST_NAME_SIZE-1);
  /*
   * Add to hash table which contains list of open dynlists
   */
  entryPtr = Tcl_CreateHashEntry(&dlinfo->dlTable, listname, &newentry);
  Tcl_SetHashValue(entryPtr, dl);

  /* NEW!!! */
  /* Add a local variable that will, when the current proc exits, free list */
  Tcl_SetVar(interp, listname, listname, 0);
  Tcl_TraceVar(interp, listname, TCL_TRACE_WRITES | TCL_TRACE_UNSETS, 
	       (Tcl_VarTraceProc *) tclDeleteLocalDynList, 
	       (ClientData) strdup(listname));

  Tcl_SetResult(interp, listname, TCL_VOLATILE);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclDeleteDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_delete
 *
 * DESCRIPTION
 *    Deletes an existing dynlist
 *
 *****************************************************************************/

static int tclDeleteDynList (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  int i;
  DYN_LIST *dl;
  Tcl_HashEntry *entryPtr;
  DYN_GROUP *dg;
  int groupid;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
    
  if (argc < 2) {
    Tcl_SetResult(interp, "usage: dl_delete dynlist(s)", TCL_STATIC);
    return TCL_ERROR;
  }

  for (i = 1; i < argc; i++) {
    if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, argv[i]))) {
      if (Tcl_GetVar(interp, argv[i], 0)) {
	Tcl_UnsetVar(interp, argv[i], 0);
      }
      else {
	if ((dl = Tcl_GetHashValue(entryPtr))) dfuFreeDynList(dl);
	Tcl_DeleteHashEntry(entryPtr);
      }
    }
  
    /* 
     * If user has specified "dl_delete ALL" iterate through the hash table
     * and recursively call this function to close all open dynlists
     */
    else if (!strcasecmp(argv[i],"ALL")) {
      Tcl_HashSearch search;
      for (entryPtr = Tcl_FirstHashEntry(&dlinfo->dlTable, &search);
	   entryPtr != NULL;
	   entryPtr = Tcl_NextHashEntry(&search)) {
	Tcl_VarEval(interp, "dl_delete ", Tcl_GetHashKey(&dlinfo->dlTable, entryPtr),
		    (char *) NULL);
      }
    }

    else if (tclFindDynListInGroup(interp, argv[i], &dg, &groupid) == TCL_OK) {
      if (!dynGroupRemoveList(dg, DYN_LIST_NAME(DYN_GROUP_LIST(dg,groupid)))) {
	Tcl_SetResult(interp, "dl_delete: error removing group list", NULL);
	return TCL_ERROR;
      }
    }

    else {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "dl_delete: dynlist \"", argv[i], 
		       "\" not found", (char *) NULL);
      return TCL_ERROR;
    }
  }
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclDeleteTraceDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_deleteTrace
 *
 * DESCRIPTION
 *    Deletes an existing dynlist
 *
 *****************************************************************************/

static int tclDeleteTraceDynList (ClientData data, Tcl_Interp *interp,
				  int argc, char *argv[])
{
  DYN_LIST *dl;
  Tcl_HashEntry *entryPtr;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
    
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist", NULL);
    return TCL_ERROR;
  }

  if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, argv[1]))) {
    if (Tcl_GetVar(interp, argv[1], 0)) {
      Tcl_UnsetVar(interp, argv[1], 0);
    }
    else {
      if ((dl = Tcl_GetHashValue(entryPtr))) dfuFreeDynList(dl);
      Tcl_DeleteHashEntry(entryPtr);
    }
  }
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclCleanDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_clean
 *    dl_cleanReturns
 *
 * DESCRIPTION
 *    Cleans up temporary dynLists (which start with '%', e.g. %list0)
 *    Cleans up return dynLists (which start with '>', e.g. >0<)
 *
 *****************************************************************************/

static int tclCleanDynList (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  Tcl_HashEntry *entryPtr;
  Tcl_HashSearch search;
  char *listname;
  int mode = (Tcl_Size) data;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  for (entryPtr = Tcl_FirstHashEntry(&dlinfo->dlTable, &search);
       entryPtr != NULL;
       entryPtr = Tcl_NextHashEntry(&search)) {
    listname = Tcl_GetHashKey(&dlinfo->dlTable, entryPtr);
    if ((mode == DL_CLEAN_TEMPS && listname && listname[0] == '%') ||
	(mode == DL_CLEAN_RETS && listname && listname[0] == '>')) {
      Tcl_VarEval(interp, "dl_delete ", listname, (char *) NULL);
    }
  }

  /* reset the respective counter */
  if (mode == DL_CLEAN_TEMPS) dlinfo->dlCount = 0;
  else if (mode == DL_CLEAN_RETS) dlinfo->returnCount = 0;

  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclRenameDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_rename
 *
 * DESCRIPTION
 *    Renames a dynList to a list with a new name.
 *
 *****************************************************************************/

static int tclRenameDynList (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " oldname newname", NULL);
    return TCL_ERROR;
  }

  if (Tcl_VarEval(interp, "dl_set ", argv[2], " ", argv[1], (char *) NULL) !=
      TCL_OK) 
    return TCL_ERROR;
  if (Tcl_VarEval(interp, "dl_delete ", argv[1], (char *) NULL) != TCL_OK)
    return TCL_ERROR;

  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclSetDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_set
 *
 * DESCRIPTION
 *    Sets a dynList to a list with a new name.  If the source list
 * is of the temporary form (starting with a %) then it is simply renamed.
 * If it is already a named list, then it is copied and added to the hash
 * table.
 *
 *****************************************************************************/

static int tclSetDynList (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  Tcl_HashEntry *entryPtr;
  int newentry, replace_grouplist = 0;
  DYN_LIST *dl, *newdl = NULL;
  char *newname, *oldname, *colon;
  char gname[DYN_LIST_NAME_SIZE];
  DYN_GROUP *dg;
  int groupid;
  
  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dl_set name dynlist", NULL);
    return TCL_ERROR;
  }


  newname = argv[1];
  oldname = argv[2];

  /* Nothing to do since the names are the same...just fake like we did it */
  if (!strcmp(oldname, newname)) {
    Tcl_SetResult(interp, newname, TCL_VOLATILE);
    return TCL_OK;
  }

  if (tclFindDynList(interp, oldname, &dl) != TCL_OK) 
    return TCL_ERROR;

  /* If the variable to be set already exists, delete it */

  if (tclFindDynListInGroup(interp, newname, &dg, &groupid) == TCL_OK) {
    /* 
     * store away the name, because when the list is replaced,
     * it will take on this name.  In other words, if a list in a
     * group is set to a new list, the group:list name will stay the same
     */
    strncpy(gname, DYN_LIST_NAME(DYN_GROUP_LIST(dg, groupid)), 
	    DYN_LIST_NAME_SIZE-1);
    dfuFreeDynList(DYN_GROUP_LIST(dg, groupid));
    replace_grouplist = 1;
  }

  /* Add a new list to a dyngroup if pre-colon name is a group */
  else if ((colon = strchr(argv[1],':'))) {
    DYN_GROUP *dg;
    char group[64];
    strncpy(group, argv[1], colon-argv[1]);
    group[colon-argv[1]] = 0;

    Tcl_ResetResult(interp);
    if (tclFindDynGroup(interp, group, &dg) == TCL_OK) {
      /* 
       * Here we check if the list to be changed is a sublist of a group
       *   list.  If so, then skip down to the next section, which replaces
       *   sublists leaving the higher structure intact.
       */

      if (tclFindDynList(interp, newname, &newdl) != TCL_OK ||
	  !(DYN_LIST_FLAGS(newdl) & DL_SUBLIST)) { 
	Tcl_ResetResult(interp);
	if ((DYN_LIST_NAME(dl)[0] == '%' || DYN_LIST_NAME(dl)[0] == '>') &&
	    (entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, DYN_LIST_NAME(dl))) &&
	    !(DYN_LIST_FLAGS(dl) & DL_SUBLIST)) {
	  Tcl_DeleteHashEntry(entryPtr);
	  
	  strncpy(DYN_LIST_NAME(dl), colon+1, DYN_LIST_NAME_SIZE-1);
	  dfuAddDynGroupExistingList(dg, DYN_LIST_NAME(dl), dl);
	}
	else {
	  dfuCopyDynGroupExistingList(dg, strchr(newname,':')+1, dl);
	}
	Tcl_SetResult(interp, newname, TCL_VOLATILE);
	return TCL_OK;
      }
    }
    /*
     * If the list still exists, it must be a sublist - which is OK.
     *  Otherwise, return an error because we don't allow normal
     *  lists to have colons in them
     */
    else {
      if (tclFindDynList(interp, newname, &newdl) != TCL_OK) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "dl_set: cannot set list \"", argv[1], 
			 "\"", NULL);
	return TCL_ERROR;
      }
    }
  }

  /* If the target list is in the dlTable, delete it */
  else if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable,newname))) {
    if ((newdl = Tcl_GetHashValue(entryPtr))) dfuFreeDynList(newdl);
    Tcl_DeleteHashEntry(entryPtr);
    newdl = NULL;
  }
  
  /* A temporary list that's in the dlTable can be renamed */
  /*   (temporary lists not in the dlTable are elements of */
  /*    groups and must be copied)                         */
  /*  as can a return list found in the dlTable            */

  if ((DYN_LIST_NAME(dl)[0] == '%' || DYN_LIST_NAME(dl)[0] == '>') &&
      (entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, DYN_LIST_NAME(dl))) &&
      !(DYN_LIST_FLAGS(dl) & DL_SUBLIST)) {
    Tcl_DeleteHashEntry(entryPtr);

    /* Change the element of the group */
    if (replace_grouplist) {
      strncpy(DYN_LIST_NAME(dl), strchr(newname,':')+1, DYN_LIST_NAME_SIZE-1);
      DYN_GROUP_LIST(dg, groupid) = dl;
    }
    
    /* 
     * If the new list can still be found, it's either a sublist or a 
     *  temporary list.  If it's temporary, fail, because I can't think
     *  of any reason (except for an error) for this to be right.  
     *  If a sublist, then reset pointer
     */
    else if (tclFindDynList(interp, newname, &newdl) == TCL_OK) {
      DYN_LIST **parent;
      int index;

      if (!(DYN_LIST_FLAGS(newdl) & DL_SUBLIST)) {
	Tcl_AppendResult(interp, argv[0],
			 ": temporary lists cannot be dl_set", NULL);
	return TCL_ERROR;
      }

      if (tclFindDynListParent(interp, newname, NULL, &parent, &index) != 
	  TCL_OK) {
	return TCL_ERROR;
      }

      dfuFreeDynList(parent[index]);
      parent[index] = dl;
    }
    
    /* Make a new list */
    else {
      strncpy(DYN_LIST_NAME(dl), newname, DYN_LIST_NAME_SIZE-1);
      entryPtr = Tcl_CreateHashEntry(&dlinfo->dlTable, newname, &newentry);
      Tcl_SetHashValue(entryPtr, dl);
    }
  }
  
  else {                        /* Do a recursive copy: leave old list  */
    if (replace_grouplist) {
      DYN_LIST *newlist = dfuCopyDynList(dl);
      strncpy(DYN_LIST_NAME(newlist), 
	      strchr(newname,':')+1, DYN_LIST_NAME_SIZE-1);
      DYN_GROUP_LIST(dg, groupid) = newlist;
    }

    else if (tclFindDynList(interp, newname, &newdl) == TCL_OK) {
      DYN_LIST *newlist = dfuCopyDynList(dl);
      DYN_LIST **parent;
      int index;

      if (tclFindDynListParent(interp, newname, NULL, &parent, &index) != 
	  TCL_OK) {
	return TCL_ERROR;
      }
      dfuFreeDynList(parent[index]);
      parent[index] = newlist;
    }
    else {
      DYN_LIST *newlist = dfuCopyDynList(dl);
      strncpy(DYN_LIST_NAME(newlist), newname, DYN_LIST_NAME_SIZE-1);
      
      /* Get rid of any current list with this name */
      entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, newname);
      if (entryPtr) {
	DYN_LIST *olddl;
	if ((olddl = Tcl_GetHashValue(entryPtr))) dfuFreeDynList(olddl);
	Tcl_DeleteHashEntry(entryPtr);
      }

      entryPtr = Tcl_CreateHashEntry(&dlinfo->dlTable, newname, &newentry);
      Tcl_SetHashValue(entryPtr, newlist);
    }
  }

  Tcl_SetResult(interp, newname, TCL_VOLATILE);
  return TCL_OK;
}




/**************************************************************************/


static char *tclDeleteLocalDynList(ClientData clientData, Tcl_Interp *interp,
				   char *name1, char *name2, int flags) 
{
  Tcl_HashEntry *entryPtr;
  DYN_LIST *dl;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return NULL;
  
  if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, (char *) clientData))) {
    if ((dl = Tcl_GetHashValue(entryPtr))) { 
      dfuFreeDynList(dl);
    }
    Tcl_DeleteHashEntry(entryPtr);
  }

  /*
   * Remove the trace, which will be added back if a new dynlist is
   * being created
   */

  Tcl_UntraceVar(interp, name1, TCL_TRACE_WRITES | TCL_TRACE_UNSETS, 
		 (Tcl_VarTraceProc *) tclDeleteLocalDynList, 
		 (ClientData) clientData);

  if (clientData) free(clientData);
  return NULL;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclLocalDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_local
 *
 * DESCRIPTION
 *    Creates a "local" dynlist which will be destroyed when the current
 * procedure is exited.  This is achieved by assigning the local variable
 * to a Tcl variable and passing a delete procedure that deletes the
 * list (if it still exists) upon deletion of the variable (proc exit).
 *
 *****************************************************************************/

static int tclLocalDynList (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  Tcl_HashEntry *entryPtr;
  int newentry;
  DYN_LIST *dl;
  char *varname, *val;
  char newname[256];

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " name dynlist", NULL);
    return TCL_ERROR;
  }

  varname = argv[1];
  if ( strlen(varname) > (DYN_LIST_NAME_SIZE-8) ) { 
    char resultstr[256];
    snprintf(resultstr, sizeof(resultstr),
	     "dl_local: variable name \"%s\" is too long (%d)",
	     varname, (int) strlen(varname));
    Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
    return TCL_ERROR;
  }

  if (tclFindDynList(interp, argv[2], &dl) != TCL_OK) 
    return TCL_ERROR;

  /* 
   * If the variable is already set, explicity unset it or we'll
   * get nested traces
   *
   *  NOTE:  This can't be done if the variable is to be set to
   *         itself!!! 
   */
  if ((val = (char *) Tcl_GetVar(interp, varname, 0))) {
    DYN_LIST *olddl;
    if (tclFindDynList(interp, val, &olddl) != TCL_OK ||
	olddl != dl)
      Tcl_UnsetVar(interp, varname, 0);
  }

  sprintf(newname, "&%s_%d&", varname, dlinfo->localCount++);

  /* A temporary list that's in the dlTable can be renamed */
  /*   (temporary lists not in the dlTable are elements of */
  /*    groups and must be copied)                         */
  /* Return lists (>#<) can also be renamed                */

  if ((DYN_LIST_NAME(dl)[0] == '%' || DYN_LIST_NAME(dl)[0] == '>') &&
      (entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, DYN_LIST_NAME(dl))) &&
      !(DYN_LIST_FLAGS(dl) & DL_SUBLIST)) {
    Tcl_DeleteHashEntry(entryPtr);

    if (Tcl_GetVar(interp, DYN_LIST_NAME(dl), 0)) {
      Tcl_UntraceVar(interp, DYN_LIST_NAME(dl), 
		     TCL_TRACE_WRITES | TCL_TRACE_UNSETS, 
		     (Tcl_VarTraceProc *) tclDeleteLocalDynList, (ClientData) DYN_LIST_NAME(dl));
      Tcl_UnsetVar(interp, DYN_LIST_NAME(dl), 0);
    }
    
    strncpy(DYN_LIST_NAME(dl), newname, DYN_LIST_NAME_SIZE-1);
    entryPtr = Tcl_CreateHashEntry(&dlinfo->dlTable, newname, &newentry);
    Tcl_SetHashValue(entryPtr, dl);
  }
  else {                        /* Do a recursive copy: leave old list  */
    DYN_LIST *newlist = dfuCopyDynList(dl);
    strncpy(DYN_LIST_NAME(newlist), newname, DYN_LIST_NAME_SIZE-1);
    entryPtr = Tcl_CreateHashEntry(&dlinfo->dlTable, newname, &newentry);
    Tcl_SetHashValue(entryPtr, newlist);
  }

  Tcl_SetVar(interp, varname, newname, 0);
  
  Tcl_TraceVar(interp, varname, TCL_TRACE_WRITES | TCL_TRACE_UNSETS, 
	       (Tcl_VarTraceProc *) tclDeleteLocalDynList, 
	       (ClientData) strdup(newname));

  Tcl_SetResult(interp, newname, TCL_VOLATILE);
  return TCL_OK;
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclReturnDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_return
 *
 * DESCRIPTION
 *    Makes a dynlist the current return value for the interpreter and
 * changes the list's name so that it is protected from dl_clean.
 * Return lists get names of the form >#<.  Unlike %#% templists, return
 * lists are not set for automatic deletion until they are used as an arg
 * to a subsequent function.
 *
 *****************************************************************************/

static int tclReturnDynList (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  Tcl_HashEntry *entryPtr;
  int newentry;
  DYN_LIST *dl;
  char newname[256];

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist", NULL);
    return TCL_ERROR;
  }
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) 
    return TCL_ERROR;

  sprintf(newname, ">%d<", dlinfo->returnCount++);

  /* A temporary list that's in the dlTable can be renamed */
  /*   (temporary lists not in the dlTable are elements of */
  /*    groups and must be copied)                         */
  /* This *doesn't* apply to lists that have already been  */
  /*   dl_return'ed, though!                               */

  if (argv[1][0] != '>' &&
      (entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, DYN_LIST_NAME(dl)))) {
    Tcl_DeleteHashEntry(entryPtr);
    
    if (Tcl_GetVar(interp, DYN_LIST_NAME(dl), 0)) 
      Tcl_UnsetVar(interp, DYN_LIST_NAME(dl), 0);
    
    strncpy(DYN_LIST_NAME(dl), newname, DYN_LIST_NAME_SIZE-1);
    entryPtr = Tcl_CreateHashEntry(&dlinfo->dlTable, newname, &newentry);
    Tcl_SetHashValue(entryPtr, dl);
  }
  else {                        /* Do a recursive copy: leave old list  */
    DYN_LIST *newlist = dfuCopyDynList(dl);
    strncpy(DYN_LIST_NAME(newlist), newname, DYN_LIST_NAME_SIZE-1);
    entryPtr = Tcl_CreateHashEntry(&dlinfo->dlTable, newname, &newentry);
    Tcl_SetHashValue(entryPtr, newlist);
  }

  /* This traces the dl_return'ed variable one level up, such that when */
  /* it goes out of scope, the trace causes dl_deleteTrace to free it   */
  /* If we're at the top level, don't set the trace (not a normal case) */
  {
    char tracecmd[256];
    sprintf(tracecmd, 
	    "if {[info level]} {uplevel \"set %s %s; trace add variable %s {write unset} dl_deleteTrace\"}",
	    newname, newname, newname);
    Tcl_Eval(interp, tracecmd);
  }

  Tcl_SetResult(interp, newname, TCL_VOLATILE);
  return TCL_OK;
}

static int tclNoOp (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  return TCL_OK;
}     

/*****************************************************************************
 *
 * FUNCTION
 *    tclPush/PopTempList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_push/popTemps
 *
 * DESCRIPTION
 *    Keep track of temporary lists created subsequently
 *
 *****************************************************************************/


static int tclPushTmpList (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  int i, j;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);

  /* if there isn't space to add the current tmplist, allocate */  
  if (TMPLIST_INDEX(dlinfo->TmpListStack) == (TMPLIST_SIZE(dlinfo->TmpListStack)-1)) {
    TMPLIST_SIZE(dlinfo->TmpListStack) += TMPLIST_INC(dlinfo->TmpListStack);
    if (TMPLIST_TMPLISTS(dlinfo->TmpListStack)) {
      TMPLIST_TMPLISTS(dlinfo->TmpListStack) = 
	(DYN_LIST **) realloc(TMPLIST_TMPLISTS(dlinfo->TmpListStack), 
			      TMPLIST_SIZE(dlinfo->TmpListStack)*sizeof(DYN_LIST *));
      for (i = 0, j = TMPLIST_SIZE(dlinfo->TmpListStack) - 1; 
	   i < TMPLIST_INC(dlinfo->TmpListStack); i++, j--) {
	TMPLIST_TMPLISTS(dlinfo->TmpListStack)[j] = dfuCreateDynList(DF_LONG, 10);
      }
    }
    else {
      TMPLIST_TMPLISTS(dlinfo->TmpListStack) = 
	(DYN_LIST **) calloc(TMPLIST_SIZE(dlinfo->TmpListStack), sizeof(DYN_LIST *));
      for (i = 0; i < TMPLIST_SIZE(dlinfo->TmpListStack); i++) {
	TMPLIST_TMPLISTS(dlinfo->TmpListStack)[i] = dfuCreateDynList(DF_LONG, 10);
      }
    }
  }
  TMPLIST_INDEX(dlinfo->TmpListStack)++;
  
  /* set tmplist to the top of the stack */
  
  dlinfo->TmpListRecordList = 
    TMPLIST_TMPLISTS(dlinfo->TmpListStack)[TMPLIST_INDEX(dlinfo->TmpListStack)];
  
  return TCL_OK;
}

static int tclPopTmpList (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  int *vals, i;
  char listname[64];

  DYN_LIST *dl;
  Tcl_HashEntry *entryPtr;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;

  /* Pop everything off.. */
  if (argc > 1) {
    while (dlinfo->TmpListStack && TMPLIST_INDEX(dlinfo->TmpListStack) >= 0) {
      vals = (int *) DYN_LIST_VALS(dlinfo->TmpListRecordList);
      for (i = 0; i < DYN_LIST_N(dlinfo->TmpListRecordList); i++) {
	sprintf(listname, "%%list%d%%", vals[i]);
	if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, listname))) {


	  if ((dl = Tcl_GetHashValue(entryPtr))) dfuFreeDynList(dl);


	  Tcl_DeleteHashEntry(entryPtr);
	}
      }
      
      /* Reset the old TmpListRecordList and "pop" it off */
      
      dfuResetDynList(dlinfo->TmpListRecordList);
      TMPLIST_INDEX(dlinfo->TmpListStack)--;
      if (TMPLIST_INDEX(dlinfo->TmpListStack) >= 0) {
	dlinfo->TmpListRecordList = 
	  TMPLIST_TMPLISTS(dlinfo->TmpListStack)[TMPLIST_INDEX(dlinfo->TmpListStack)];
      }
      else dlinfo->TmpListRecordList = NULL;
    }
    return TCL_OK;
  }


  /* Just pop off the last one */

  if (!dlinfo->TmpListStack || TMPLIST_INDEX(dlinfo->TmpListStack) < 0) {
    Tcl_AppendResult(interp, argv[0], ": popped empty templist stack", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  /* For each of the temp lists created, try to delete */
  /*  (It is not an error if the list no longer exists */
  /*   because it may have been renamed or deleted)    */

  vals = (int *) DYN_LIST_VALS(dlinfo->TmpListRecordList);
  for (i = 0; i < DYN_LIST_N(dlinfo->TmpListRecordList); i++) {
    sprintf(listname, "%%list%d%%", vals[i]);
    if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, listname))) {
      if (Tcl_GetVar(interp, listname, 0)) {
	Tcl_UnsetVar(interp, listname, 0);
      }
      else {
	if ((dl = Tcl_GetHashValue(entryPtr))) dfuFreeDynList(dl);
	Tcl_DeleteHashEntry(entryPtr);
      }
    }
  }

  /* Reset the old TmpListRecordList and "pop" it off */

  dfuResetDynList(dlinfo->TmpListRecordList);
  TMPLIST_INDEX(dlinfo->TmpListStack)--;
  if (TMPLIST_INDEX(dlinfo->TmpListStack) >= 0) {
    dlinfo->TmpListRecordList = 
      TMPLIST_TMPLISTS(dlinfo->TmpListStack)[TMPLIST_INDEX(dlinfo->TmpListStack)];
  }
  else dlinfo->TmpListRecordList = NULL;

  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclGetPutDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_get / dl_put
 *
 * DESCRIPTION
 *    Gets/Puts a dynList element
 *
 *****************************************************************************/

static int tclGetPutDynList (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  DYN_LIST *dl = NULL;
  int mode = (Tcl_Size) data;
  int i;

  if (mode == DL_FIRST || mode == DL_LAST || mode == DL_PICKONE) {
    if (argc < 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist", (char *) NULL);
      return TCL_ERROR;
    }
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
    if (!DYN_LIST_N(dl)) return TCL_OK;
    
    switch (mode) {
    case DL_FIRST: i = 0; break;
    case DL_LAST: i = DYN_LIST_N(dl)-1; break;
    case DL_PICKONE: i = ran(DYN_LIST_N(dl)); break;
    }
  }
  else if (mode == DL_PUT && argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist index newval", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  else if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist index", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[2], &i) != TCL_OK) return TCL_ERROR;
  }

  
  if (i < 0 || i >= DYN_LIST_N(dl)) {
    Tcl_AppendResult(interp, argv[0], ": index out of range", NULL);
    return TCL_ERROR;
  }

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      if (mode == DL_PUT) {
	int element;
	if (Tcl_GetInt(interp, argv[3], &element) != TCL_OK) {
	  return TCL_ERROR;
	}
	vals[i] = element;
	Tcl_AppendResult(interp, argv[1], NULL);	
      }
      else Tcl_SetObjResult(interp, Tcl_NewIntObj(vals[i]));
      break;
    }
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      if (mode == DL_PUT) {
	int element;
	if (Tcl_GetInt(interp, argv[3], &element) != TCL_OK) {
	  return TCL_ERROR;
	}
	vals[i] = (short) element;
	Tcl_AppendResult(interp, argv[1], NULL);	
      }
      else Tcl_SetObjResult(interp, Tcl_NewIntObj(vals[i]));
      break;
    }
  case DF_CHAR:
     {
       char *vals = (char *) DYN_LIST_VALS(dl);
       if (mode == DL_PUT) {
	int element;
	if (Tcl_GetInt(interp, argv[3], &element) != TCL_OK) {
	  return TCL_ERROR;
	}
	vals[i] = (char) element;
	Tcl_AppendResult(interp, argv[1], NULL);	
      }
      else Tcl_SetObjResult(interp, Tcl_NewIntObj(vals[i]));
      break;
    }
  case DF_FLOAT:
     {
       float *vals = (float *) DYN_LIST_VALS(dl);
       if (mode == DL_PUT) {
	double element;
	if (Tcl_GetDouble(interp, argv[3], &element) != TCL_OK) {
	  return TCL_ERROR;
	}
	vals[i] = (float) element;
	Tcl_AppendResult(interp, argv[1], NULL);	
      }
      else Tcl_SetObjResult(interp, Tcl_NewDoubleObj(vals[i]));
      break;
    }
  case DF_STRING:
    {
      char **vals = (char **) DYN_LIST_VALS(dl);
      if (mode == DL_PUT) {
	if (vals[i]) free((void *) vals[i]);
	vals[i] = malloc(strlen(argv[3])+1);
	strcpy(vals[i], argv[3]);
	Tcl_AppendResult(interp, argv[1], NULL);
      }
      else {
	Tcl_SetResult(interp, vals[i], TCL_VOLATILE);
      }
      break;
    }
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      if (mode == DL_PUT) {
	DYN_LIST *dl;
	if (tclFindDynList(interp, argv[3], &dl) != TCL_OK) return TCL_ERROR;
	if (vals[i]) dfuFreeDynList(vals[i]);
	vals[i] = dfuCopyDynList(dl);
	Tcl_AppendResult(interp, argv[1], NULL);
      }
      else if (vals[i]) {
	return(tclPutList(interp, dfuCopyDynList(vals[i])));
      }
      break;
    }
  default:
    Tcl_AppendResult(interp, argv[0], ": invalid datatype", (char *) NULL);
    return TCL_ERROR;
    break;
  }
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclAppendDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_append
 *
 * DESCRIPTION
 *    Appends an element to a dynlist
 *
 *****************************************************************************/

static int tclAppendDynList (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  DYN_LIST *dl, *dl2;
  int mode = (Tcl_Size) data;
  int i, start = 2, pos;

  switch (mode) {
  case DL_SPLICE_BEFORE:
  case DL_SPLICE_AFTER:
    if (argc != 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], "list1 list2", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
    if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) return TCL_ERROR;

    if (mode == DL_SPLICE_BEFORE) pos = 0;
    else pos = 1;

    /* Note that the order of arguments is a little screwy for splice... */

    if (!dynListSpliceLists(dl2, dl, pos)) {
      Tcl_AppendResult(interp, argv[0], ": unable to splice lists", 
		       (char *) NULL);
      return TCL_ERROR;
    }

    /*
     * Now decide what to return:
     *   If argv[1] includes a colon, return it
     *   otherwise return the listname (works for things like "1 2 3")
     */
    if (strchr(argv[1],':')) {
      Tcl_AppendResult(interp, argv[1], NULL);
    }
    else {
      Tcl_AppendResult(interp, DYN_LIST_NAME(dl), NULL);
    }
    return TCL_OK;
    break;    
  case DL_INSERT:
    if (argc < 4) {
      Tcl_AppendResult(interp, "usage: ", argv[0], "dynlist pos val(s)", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[2], &pos) != TCL_OK) return TCL_ERROR;
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
    if (pos < 0 || pos > DYN_LIST_N(dl)) {
      Tcl_AppendResult(interp, argv[0], ": insert position out of range", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    start = 3;
    break;
  default:
    if (argc < 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist val(s)", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  }

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int element;
      for (i = start; i < argc; i++, pos++) {
	if (Tcl_GetInt(interp, argv[i], &element) != TCL_OK) {
	  return TCL_ERROR;
	}
	switch (mode) {
	case DL_APPEND:    dfuAddDynListLong(dl, element);          break;
	case DL_PREPEND:   dfuPrependDynListLong(dl, element);      break;
	case DL_INSERT:    dfuInsertDynListLong(dl, element, pos);  break;
	}
      }
      break;
    }
  case DF_SHORT:
    {
      int element;
      for (i = start; i < argc; i++, pos++) {
	if (Tcl_GetInt(interp, argv[i], &element) != TCL_OK) {
	  return TCL_ERROR;
	}
	switch (mode) {
	case DL_APPEND:    dfuAddDynListShort(dl, element);          break;
	case DL_PREPEND:   dfuPrependDynListShort(dl, element);      break;
	case DL_INSERT:    dfuInsertDynListShort(dl, element, pos);  break;
	}
      }
      break;
    }
  case DF_CHAR:
    {
      int element;
      for (i = start; i < argc; i++, pos++) {
	if (Tcl_GetInt(interp, argv[i], &element) != TCL_OK) {
	  return TCL_ERROR;
	}
	switch (mode) {
	case DL_APPEND:    dfuAddDynListChar(dl, element);          break;
	case DL_PREPEND:   dfuPrependDynListChar(dl, element);      break;
	case DL_INSERT:    dfuInsertDynListChar(dl, element, pos);  break;
	}
      }
      break;
    }
  case DF_FLOAT:
    {
      double element;
      for (i = start; i < argc; i++, pos++) {
	if (Tcl_GetDouble(interp, argv[i], &element) != TCL_OK) {
	  return TCL_ERROR;
	}
	switch (mode) {
	case DL_APPEND:    dfuAddDynListFloat(dl, element);          break;
	case DL_PREPEND:   dfuPrependDynListFloat(dl, element);      break;
	case DL_INSERT:    dfuInsertDynListFloat(dl, element, pos);  break;
	}
      }
      break;
    }
  case DF_STRING:
    {
      for (i = start; i < argc; i++, pos++) {
	switch (mode) {
	case DL_APPEND:    dfuAddDynListString(dl, argv[i]);          break;
	case DL_PREPEND:   dfuPrependDynListString(dl, argv[i]);      break;
	case DL_INSERT:    dfuInsertDynListString(dl, argv[i], pos);  break;
	}
      }
      break;
    }
  case DF_LIST:
    {
      DYN_LIST *newdl;
      for (i = start; i < argc; i++, pos++) {
	if (tclFindDynList(interp, argv[i], &newdl) != TCL_OK) 
	  return TCL_ERROR;
	switch (mode) {
	case DL_APPEND:    dfuAddDynListList(dl, newdl);          break;
	case DL_PREPEND:   dfuPrependDynListList(dl, newdl);      break;
	case DL_INSERT:    dfuInsertDynListList(dl, newdl, pos);  break;
	}
      }
      break;
    }
  default:
    Tcl_AppendResult(interp, argv[0], ": invalid datatype", 
		     (char *) NULL);
    return TCL_ERROR;
    break;
  }
  
  /*
   * Now decide what to return:
   *   If argv[1] includes a colon, return it
   *   otherwise return the listname (works for things like "1 2 3")
   */
  if (strchr(argv[1],':')) {
    Tcl_AppendResult(interp, argv[1], NULL);
  }
  else {
    Tcl_AppendResult(interp, DYN_LIST_NAME(dl), NULL);
  }
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclConsDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_concat
 *    dl_combine
 *    dl_interleave
 *    dl_increment
 *
 * DESCRIPTION
 *    Concatenates and combines lists
 *
 *****************************************************************************/

static int tclConsDynList (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  DYN_LIST *dl1, *dl2, *newlist;
  int operation = (Tcl_Size) data;

  if (operation == DL_INCREMENT) {
    if (argc < 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist [index]", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (argc == 3) {
      int index, *vals;
      if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) return TCL_ERROR;
      if (Tcl_GetInt(interp, argv[2], &index) != TCL_OK) return TCL_ERROR;
      if (DYN_LIST_DATATYPE(dl1) != DF_LONG) {
	Tcl_AppendResult(interp, argv[0], ": list must be ints", 
		       (char *) NULL);
	return TCL_ERROR;
      }
      if (index >= DYN_LIST_N(dl1) || index < 0) {
	Tcl_AppendResult(interp, argv[0], ": index out of range", 
			 (char *) NULL);
	return TCL_ERROR;
      }
      vals = (int *) DYN_LIST_VALS(dl1);
      vals[index]++;
      Tcl_SetObjResult(interp, Tcl_NewIntObj(vals[index]));
      return TCL_OK;
    }
  }

  else if (operation == DL_INTERLEAVE) {
    int i;
    DYN_LIST *newlist1;
    
    if (argc < 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist1 dynlist2 ...", 
		       (char *) NULL);
      return TCL_ERROR;
    }

    if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) return TCL_ERROR;
    if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) return TCL_ERROR;

    newlist = dynListInterleave(dl1, dl2);
    for (i = 3; i < argc; i++) {
      if (!newlist) goto finished;
      if (tclFindDynList(interp, argv[i], &dl1) != TCL_OK) {
	dfuFreeDynList(newlist);
	return TCL_ERROR;
      }
      newlist1 = dynListInterleave(newlist, dl1);
      dfuFreeDynList(newlist);
      newlist = newlist1;
    }
    
  finished:
    if (!newlist) {
      Tcl_AppendResult(interp, argv[0], 
		       ": error interleaving lists", (char *) NULL);
      return TCL_ERROR;
    }
    else return(tclPutList(interp, newlist));
  }

  /* 
   * Concat checks that all lists are of the same type and concats
   * them together...
   * Must ensure that all lists are of the same datatype
   */
     
  else if (operation == DL_CONCAT) {
    int datatype, i;
    if (argc < 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist dynlist(s)", 
		       (char *) NULL);
      return TCL_ERROR;
    }

    if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) return TCL_ERROR;
    datatype = DYN_LIST_DATATYPE(dl1);

    /* Check out all the lists */
    for (i = 2; i < argc; i++) {
      if (tclFindDynList(interp, argv[i], &dl2) != TCL_OK) return TCL_ERROR;
      if (DYN_LIST_DATATYPE(dl2) != datatype) {
	Tcl_AppendResult(interp, argv[0], ": lists must be of the same type", 
			 (char *) NULL);
	return TCL_ERROR;
      }
    }

    /* Now do the work */

    for (i = 2; i < argc; i++) {
      tclFindDynList(interp, argv[i], &dl2);
      dynListConcat(dl1, dl2);
    }

    /*
     * Now decide what to return:
     *   If argv[1] includes a colon, return it
     *   otherwise return the listname (works for things like "1 2 3")
     */
    if (strchr(argv[1],':')) {
      Tcl_AppendResult(interp, argv[1], NULL);
    }
    else {
      Tcl_AppendResult(interp, DYN_LIST_NAME(dl1), NULL);
    }
    return TCL_OK;
  }

  else if (operation == DL_COMBINE) {
    int datatype, i, length;
    
    if (argc < 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist dynlist(s)", 
		       (char *) NULL);
      return TCL_ERROR;
    }

    if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) 
      return TCL_ERROR;
    
    /* Use the datatype of the first list to compare to all others */
    datatype = DYN_LIST_DATATYPE(dl1);
    length = DYN_LIST_N(dl1);

    /* Check out all the lists */
    for (i = 2; i < argc; i++) {
      if (tclFindDynList(interp, argv[i], &dl2) != TCL_OK) return TCL_ERROR;
      if (DYN_LIST_DATATYPE(dl2) != datatype) {
	Tcl_AppendResult(interp, argv[0], 
			 ": lists must be of the same type", (char *) NULL);
	return TCL_ERROR;
      }
      length += DYN_LIST_N(dl2);
    }
    
    /* Now do the work */
    newlist = dfuCreateDynList(datatype, length);
    for (i = 1; i < argc; i++) {
      tclFindDynList(interp, argv[i], &dl2);
      dynListConcat(newlist, dl2);
    }
    return(tclPutList(interp, newlist));
  }


  /* For the binary operators... */

  else if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist1 dynlist2", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) return TCL_ERROR;

  if (operation == DL_INCREMENT) {
    if (dynListIncrementCounter(dl1) != 1) {
      Tcl_AppendResult(interp, argv[0], ": cannot increment counter \"",
		       argv[1], "\"", (char *) NULL);
      return TCL_ERROR;
    }
    Tcl_AppendResult(interp, DYN_LIST_NAME(dl1), NULL);
    return TCL_OK;
  }

  if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) return TCL_ERROR;
  
  if (DYN_LIST_DATATYPE(dl1) != DYN_LIST_DATATYPE(dl2)) {
    Tcl_AppendResult(interp, argv[0], 
		     ": cannot combine lists of different types", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclResetDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_reset
 *
 * DESCRIPTION
 *    Resets an existing dynlist
 *
 *****************************************************************************/

static int tclResetDynList (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  DYN_LIST *dl;
  int i;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist(s)", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  for (i = 1; i < argc; i++) {
    if (tclFindDynList(interp, argv[i], &dl) != TCL_OK) return TCL_ERROR;
    dfuResetDynList(dl);
  }

  if (strchr(argv[1],':'))
    Tcl_AppendResult(interp, argv[1], NULL);
  else
    Tcl_AppendResult(interp, DYN_LIST_NAME(dl), NULL);
  return TCL_OK;
}
Tcl_Obj *tclDynListToTclObj(Tcl_Interp *interp, DYN_LIST *dl)
{
  int i;

  if (!dl) {
    return NULL;
  }

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(vals[i]));
      }
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(vals[i]));
      }
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(vals[i]));
      }
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(vals[i]));
      }
    }
    break;
  case DF_STRING:
    {
      char **vals = (char **) DYN_LIST_VALS(dl);
      if (!strcmp(DLFormatTable[FMT_STRING], "%s")) {
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj(vals[i], -1));
	}
      }
      else {			/* should verify string length */
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  char buffer[128];
	  snprintf(buffer, sizeof(buffer), DLFormatTable[FMT_STRING], vals[i]);
	  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj(buffer, -1));
	}
      }
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      Tcl_Obj *sublist;
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	if (!(sublist = tclDynListToTclObj(interp, vals[i]))) {
	  Tcl_DecrRefCount(listPtr);
	  return NULL;
	}
	Tcl_ListObjAppendElement(interp, listPtr, sublist);
      }
    }
    break;
  }
  return listPtr;
}

int tclDynListToTclList(Tcl_Interp *interp, DYN_LIST *dl)
{
  if (!dl || DYN_LIST_N(dl) == 0) {
    Tcl_ResetResult(interp);
    return TCL_OK;
  }

  Tcl_Obj *listPtr = tclDynListToTclObj(interp, dl);
  if (!listPtr) {
    Tcl_AppendResult(interp, "error creating obj from dynlist", NULL);
    return TCL_ERROR;
  }

  Tcl_SetObjResult(interp, listPtr);
  return TCL_OK;
}

/*
 * Currently this command only supports the creation of int and
 * float lists (i.e. no nested lists, and no lists of strings)
 */
DYN_LIST *tclTclListToDynList(Tcl_Interp *interp, char *list)
{
  DYN_LIST *newlist, *flist = NULL, *ilist = NULL;
  int i, j, ival, use_float = 0;
  double fval;
  char **varArgv;
  Tcl_Size varArgc;

  if (Tcl_SplitList(interp, list, &varArgc,
		    (const char ***) &varArgv) != TCL_OK) {
    Tcl_ResetResult(interp);
    return NULL;
  }

  /* default to int list, only switch to float if necessary */
  ilist = dfuCreateDynList(DF_LONG, varArgc);
  for (i = 0; i < varArgc; i++) {
    if (!use_float) {
      if (Tcl_GetInt(interp, varArgv[i], &ival) == TCL_OK) {
	dfuAddDynListLong(ilist, ival);
      }
      else {
	if (Tcl_GetDouble(interp, varArgv[i], &fval) == TCL_OK) {
	  int *ivals = (int *) DYN_LIST_VALS(ilist);
	  flist = dfuCreateDynList(DF_FLOAT, varArgc);
	  /* Copy all the int vals into the float list as floats */
	  for (j = 0; j < i; j++) {
	    dfuAddDynListFloat(flist, ivals[j]);
	  }
	  dfuAddDynListFloat(flist, fval);
	  dfuFreeDynList(ilist);
	  use_float = 1;
	}
	else {			/* not a float either */
	  dfuFreeDynList(ilist);
	  Tcl_Free((void *) varArgv);
	  Tcl_ResetResult(interp);
	  return NULL;
	}
      }
    }
    else {
      if (Tcl_GetDouble(interp, varArgv[i], &fval) == TCL_OK) {
	dfuAddDynListFloat(flist, fval);
      }
      else {
	dfuFreeDynList(flist);
	Tcl_Free((void *) varArgv);
	Tcl_ResetResult(interp);
	return NULL;
      }
    }
  }
  Tcl_Free((void *) varArgv);

  if (use_float) newlist = flist;
  else newlist = ilist;
  tclPutList(interp, newlist);

  Tcl_ResetResult(interp);
  return newlist;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclDumpDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_dump
 *
 * DESCRIPTION
 *    Dumps an existing dynlist to stdout or to an open file stream
 *
 *****************************************************************************/

static int tclDumpDynList (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DYN_LIST *dl;
  Tcl_Channel outChannel = NULL ;
  int separator = '\t';
  int output_format = (Tcl_Size) data;
  int status = 0;

  if (argc < 2) {
    char *params;
    if (output_format == DL_TO_TCL_LIST) params = " dynlist";
    else if (output_format == DL_DUMPBYTES) params = " dynlist filename";
    else if (output_format == DL_DUMPBYTES_AS) 
      params = " ushort dynlist filename";
    else params = " dynlist [fileID]";
    Tcl_AppendResult(interp, "usage: ", argv[0], params, (char *) NULL);
    return TCL_ERROR;
  }

  if (output_format != DL_DUMPBYTES_AS)
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;

  if (output_format == DL_TO_TCL_LIST) {
    tclDynListToTclList(interp, dl);
    return TCL_OK;
  }

  if (output_format == DL_DUMPBYTES) {
    int mode;
    int n;
    FILE *outfp = NULL;

    if (argc != 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], 
		       " dynlist channel|filename", (char *) NULL);
      return TCL_ERROR;
    }

    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG:
    case DF_FLOAT:
    case DF_SHORT:
    case DF_CHAR:
      if (!(outChannel = Tcl_GetChannel(interp, argv[2], &mode))) {
	Tcl_ResetResult(interp);
	outfp = fopen(argv[2], "wb");
	if (!outfp) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": unable to open file ", argv[2], 
			   " for output", NULL);
	  return TCL_ERROR;
	}
      }
      else {
	Tcl_SetChannelOption(interp, outChannel, "-translation", "binary");
      }
      break;
    default:
      Tcl_AppendResult(interp, argv[0], ": binary dump only supported for"
		       " ints, shorts, floats, and chars", NULL);
      return TCL_ERROR;
    }
    
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG:
      if (outChannel) {
	n = Tcl_Write(outChannel, (char *) DYN_LIST_VALS(dl), 
		      sizeof(int)*DYN_LIST_N(dl));
	n /= sizeof(int);
      }
      else {
	n = fwrite((char *) DYN_LIST_VALS(dl), sizeof(int), 
		   DYN_LIST_N(dl), outfp);
      }
      break;
    case DF_FLOAT:
      if (outChannel) {
	n = Tcl_Write(outChannel, (char *) DYN_LIST_VALS(dl), 
		      sizeof(float)*DYN_LIST_N(dl));

	n /= sizeof(float);
      }
      else {
	n = fwrite((char *) DYN_LIST_VALS(dl), sizeof(float), 
		   DYN_LIST_N(dl), outfp);
      }
      break;
    case DF_SHORT:
      if (outChannel) {
	n = Tcl_Write(outChannel, (char *) DYN_LIST_VALS(dl), 
		      sizeof(short)*DYN_LIST_N(dl));

	n /= sizeof(short);
      }
      else {
	n = fwrite((char *) DYN_LIST_VALS(dl), sizeof(short), 
		   DYN_LIST_N(dl), outfp);
      }
      break;
    case DF_CHAR:
      if (outChannel) {
	n = Tcl_Write(outChannel, (char *) DYN_LIST_VALS(dl), 
		      sizeof(char)*DYN_LIST_N(dl));

	n /= sizeof(char);
      }
      else {
	n = fwrite((char *) DYN_LIST_VALS(dl), sizeof(char), 
		   DYN_LIST_N(dl), outfp);
      }
      break;
    default:
      Tcl_AppendResult(interp, argv[0], ": binary dump only supported for"
		       " ints, shorts, floats, and chars", NULL);
      return TCL_ERROR;
    }
    if (n != DYN_LIST_N(dl)) {
      Tcl_AppendResult(interp, argv[0], ": error dumping dynlist", NULL);
      if (outfp) fclose(outfp);
      return TCL_ERROR;
    }
    if (outfp) fclose(outfp);
    return TCL_OK;
  }

  /*********************************************************************/
  /*                         DUMPBYTES_AS                              */
  /*********************************************************************/

  else if (output_format == DL_DUMPBYTES_AS) {
    int outtype;
    int i;
    FILE *outfp;
    if (argc != 4) {
    bytes_as_usage:
      Tcl_AppendResult(interp, "usage: ", argv[0], " ushort dynlist filename", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (!strcmp(argv[1], "ushort") || !strcmp(argv[1], "USHORT")) {
      outtype = DL_DUMP_USHORT;
    }
    else goto bytes_as_usage;

    if (tclFindDynList(interp, argv[2], &dl) != TCL_OK) return TCL_ERROR;

    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG:
    case DF_FLOAT:
    case DF_SHORT:
    case DF_CHAR:
      outfp = fopen(argv[3], "wb");
      if (!outfp) {
	Tcl_AppendResult(interp, argv[0], ": unable to open file ", argv[3], 
			 " for output", NULL);
	return TCL_ERROR;
      }
      break;
    default:
      Tcl_AppendResult(interp, argv[0], ": binary dump only supported for"
		       " ints, shorts, floats, and chars");
      return TCL_ERROR;
    }
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG: 
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	switch (outtype) {
	case DL_DUMP_USHORT: 
	  {
	    unsigned short s;
	    for (i = 0; i < DYN_LIST_N(dl); i++) {
	      s = vals[i];
	      fwrite(&s, sizeof(short), 1, outfp);
	    }
	  }
	}
      }
      break;
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	switch (outtype) {
	case DL_DUMP_USHORT: 
	  {
	    unsigned short s;
	    for (i = 0; i < DYN_LIST_N(dl); i++) {
	      s = vals[i];
	      fwrite(&s, sizeof(short), 1, outfp);
	    }
	  }
	}
      }
    break;
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	switch (outtype) {
	case DL_DUMP_USHORT: 
	  {
	    unsigned short s;
	    for (i = 0; i < DYN_LIST_N(dl); i++) {
	      s = vals[i];
	      fwrite(&s, sizeof(short), 1, outfp);
	    }
	  }
	}
      }
    break;
    case DF_CHAR:
      {
	char *vals = (char *) DYN_LIST_VALS(dl);
	switch (outtype) {
	case DL_DUMP_USHORT: 
	  {
	    unsigned short s;
	    for (i = 0; i < DYN_LIST_N(dl); i++) {
	      s = vals[i];
	      fwrite(&s, sizeof(short), 1, outfp);
	    }
	  }
	}
      }
    break;
    default:
      Tcl_AppendResult(interp, argv[0], ": binary dump only supported for"
		       " ints, shorts, floats, and chars", NULL);
      return TCL_ERROR;
    }
    
    fclose(outfp);
    return TCL_OK;
  }

  if (argc > 2) {
    int mode;
    if (!(outChannel = Tcl_GetChannel(interp, argv[2], &mode))) {
      return TCL_ERROR;
    }
  }
  
  if (argc > 3) {
    if (Tcl_GetInt(interp, argv[3], &separator) != TCL_OK) {
      return TCL_ERROR;
    }
    else if (separator > 255) {
      Tcl_AppendResult(interp, argv[0], 
		       ": separator must be between 0 and 255",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  switch (output_format) {
  case DL_DUMP:
    status = dynListChanDump(interp, dl, outChannel);
    break;
  case DL_DUMP_AS_ROW:
    status = dynListChanDumpAsRow(interp, dl, outChannel, (char) separator);
    break;
  case DL_DUMP_MATRIX:
    status = dynListChanDumpMatrix(interp, dl, outChannel, (char) separator);
    break;
  case DL_DUMP_MATRIX_IN_COLS:
    status = dynListChanDumpMatrixInCols(interp, dl, outChannel, (char) separator);
    break;
  }
  if (!status) {
    Tcl_AppendResult(interp, argv[0], ": invalid dynlist", (char *) NULL);
    return TCL_ERROR;
  }
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclConvertDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_float
 *
 * DESCRIPTION
 *    Convert from one type of dynlist to another
 *
 *****************************************************************************/

static int tclConvertDynList (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  DYN_LIST *dl, *newlist = NULL;
  int operation = (Tcl_Size) data;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist [tclvar]",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  switch (operation) {
  case DL_INT:
    newlist = dynListConvertList(dl, DF_LONG);
    break;
  case DL_FLOAT:
    newlist = dynListConvertList(dl, DF_FLOAT);
    break;
  case DL_CHAR:
    newlist = dynListConvertList(dl, DF_CHAR);
    break;
  case DL_SHORT:
    newlist = dynListConvertList(dl, DF_SHORT);
    break;
  default:
    newlist = NULL;
    break;
  }
  if (!newlist) {
    Tcl_AppendResult(interp, argv[0], ": unable to convert dynlist",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (argc < 3) {
    return(tclPutList(interp, newlist));
  }

#ifdef CONVERT_TO_BYTESTRING
  /* What was I thinking?!? DLS 05/06 */
  else {			/* Put the result in a tcl byte string */
    Tcl_Obj *o, *newvar;
    int n;
    switch (DYN_LIST_DATATYPE(newlist)) {
    case DL_CHAR:     n = DYN_LIST_N(newlist)*sizeof(char);        break;
    case DL_INT:      n = DYN_LIST_N(newlist)*sizeof(int);         break;
    case DL_FLOAT:    n = DYN_LIST_N(newlist)*sizeof(float);       break;
    }
    newvar = Tcl_NewStringObj(argv[2], strlen(argv[2]));
    o = Tcl_NewStringObj(DYN_LIST_VALS(newlist), n);
    dfuFreeDynList(newlist);
    if (Tcl_ObjSetVar2(interp, newvar, NULL, o,
		       TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1) == NULL)
      return TCL_ERROR;
    Tcl_DecrRefCount(newvar);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(n));
    return TCL_OK;
  }
#else
    return(tclPutList(interp, newlist));
#endif
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclUnsignedConvertDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_float
 *
 * DESCRIPTION
 *    Convert from one type of dynlist to another, treat chars as unsigned
 *
 *****************************************************************************/

static int tclUnsignedConvertDynList (ClientData data, Tcl_Interp *interp,
				      int argc, char *argv[])
{
  DYN_LIST *dl, *newlist = NULL;
  int operation = (Tcl_Size) data;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist [tclvar]",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  switch (operation) {
  case DL_INT:
    newlist = dynListUnsignedConvertList(dl, DF_LONG);
    break;
  case DL_FLOAT:
    newlist = dynListUnsignedConvertList(dl, DF_FLOAT);
    break;
  case DL_CHAR:
    newlist = dynListUnsignedConvertList(dl, DF_CHAR);
    break;
  case DL_SHORT:
    newlist = dynListUnsignedConvertList(dl, DF_SHORT);
    break;
  default:
    newlist = NULL;
    break;
  }
  if (!newlist) {
    Tcl_AppendResult(interp, argv[0], ": unable to convert dynlist",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (argc < 3) {
    return(tclPutList(interp, newlist));
  }
  return(tclPutList(interp, newlist));
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclPermuteDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_permute
 *    dl_reverse
 *    dl_reverseAll
 *    dl_breverse
 *
 * DESCRIPTION
 *    Permutes list by second list's indices
 *    Reverses list
 *****************************************************************************/

static int tclPermuteDynList (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  DYN_LIST *dl1, *dl2, *newlist = NULL;
  int operation = (Tcl_Size) data;

  /* Do the reverse here in three easy steps... */
  if (operation == DL_REVERSE || operation == DL_REVERSE_ALL || 
      operation == DL_BREVERSE) {
    if (argc != 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist",
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) return TCL_ERROR;
    switch (operation) {
    case DL_REVERSE:	newlist = dynListReverseList(dl1); break;
    case DL_REVERSE_ALL: newlist = dynListReverseAll(dl1); break;
    case DL_BREVERSE: newlist = dynListBReverseList(dl1); break;
    }
    return(tclPutList(interp, newlist));
  }

  /* Otherwise take care of the permute */
  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist orderlist",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) return TCL_ERROR;

  newlist = dynListPermuteList(dl1, dl2);

  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": cannot permute list \"",
		     argv[1], "\" by \"", argv[2], "\"",  (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclSelectDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_select
 *    dl_choose
 *
 * DESCRIPTION
 *    Select elements from list one, using list2 as either binary
 * values or indices.
 *
 *
 *****************************************************************************/

static int tclSelectDynList (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  DYN_LIST *dl, *selections, *newlist;
  int operation = (Tcl_Size) data;

  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " list selections",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) 
    return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &selections) != TCL_OK) 
    return TCL_ERROR;

  if (operation == DL_SELECT) newlist = dynListSelect(dl, selections);
  else newlist = dynListChoose(dl, selections);

  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], 
		     ": unable to select elements from list \"",
		     argv[1], "\" using \"", argv[2], "\"", (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclReplaceDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_replace
 *    dl_replaceByIndex
 *
 * DESCRIPTION
 *    Replace selected elements from list one, using list2 as either binary
 * values or indices with elements from list3
 *
 *    dl_replaceByIndex does not accept binary selection, ensuring indexing
 *
 *
 *****************************************************************************/

static int tclReplaceDynList (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  DYN_LIST *dl, *selections, *r, *newlist = NULL;
  int mode = (Tcl_Size) data;

  if (argc != 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " list selections replacements", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) 
    return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &selections) != TCL_OK) 
    return TCL_ERROR;
  if (tclFindDynList(interp, argv[3], &r) != TCL_OK) 
    return TCL_ERROR;

  switch (mode) {
  case DL_REPLACE:  
    newlist = dynListReplace(dl, selections, r); 
    break;
  case DL_REPLACE_BY_INDEX:
    newlist = dynListReplaceByIndex(dl, selections, r); 
    break;
  }

  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], 
		     ": unable to replace elements from list \"",
		     argv[1], "\" using \"", argv[2], "\"", (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclFindSublist
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_find
 *
 * DESCRIPTION
 *    Search for a sublist in a source list
 *
 *****************************************************************************/

static int tclFindSublist (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DYN_LIST *dl1, *dl2 = NULL, *result = NULL;
  int n;
  double k;
  int free_dl2 = 0;
  int mode = (Tcl_Size) data;

  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " dynlist dynlist", (char *) NULL);
    return TCL_ERROR;
  }
  if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) return TCL_ERROR;

  if (mode == DL_FIND_PATTERNS || mode == DL_FIND_INDICES) {
    if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) return TCL_ERROR;
  }
  else if (DYN_LIST_DATATYPE(dl1) == DF_LIST) {
    DYN_LIST *dl_2;
    if (tclFindDynList(interp, argv[2], &dl_2) == TCL_OK) {
      if (DYN_LIST_DATATYPE(dl_2) != DF_LIST) {
	free_dl2 = 1;
	dl2 = dfuCreateDynList(DF_LIST, 1);
	dfuAddDynListList(dl2, dl_2);
      }
      else {
	dl2 = dl_2;
      }
    }
  }
  if (!dl2) {
    if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) {
      switch (DYN_LIST_DATATYPE(dl1)) {
      case DF_LIST:
	if (Tcl_GetInt(interp, argv[2], &n) == TCL_OK) {
	  free_dl2 = 1;
	  dl2 = dfuCreateDynList(DF_LONG, 1);
	  dfuAddDynListLong(dl2, n);
	}
	else if (Tcl_GetDouble(interp, argv[2], &k) == TCL_OK) {
	  free_dl2 = 1;
	  dl2 = dfuCreateDynList(DF_FLOAT, 1);
	  dfuAddDynListFloat(dl2, k);
	}
	else {
	  free_dl2 = 1;
	  dl2 = dfuCreateDynList(DF_STRING, 1);
	  dfuAddDynListString(dl2, argv[2]);
	}
	break;
      case DF_LONG:
	if (Tcl_GetInt(interp, argv[2], &n) != TCL_OK) goto usage;
	free_dl2 = 1;
	dl2 = dfuCreateDynList(DF_LONG, 1);
	dfuAddDynListLong(dl2, n);
	break;
      case DF_SHORT:
	if (Tcl_GetInt(interp, argv[2], &n) != TCL_OK) goto usage;
	free_dl2 = 1;
	dl2 = dfuCreateDynList(DF_SHORT, 1);
	dfuAddDynListLong(dl2, (short) n);
	break;
      case DF_CHAR:
	if (Tcl_GetInt(interp, argv[2], &n) != TCL_OK) goto usage;
	free_dl2 = 1;
	dl2 = dfuCreateDynList(DF_CHAR, 1);
	dfuAddDynListLong(dl2, (char) n);
	break;
      case DF_FLOAT:
	if (Tcl_GetDouble(interp, argv[2], &k) != TCL_OK) goto usage;
	free_dl2 = 1;
	dl2 = dfuCreateDynList(DF_FLOAT, 1);
	dfuAddDynListLong(dl2, k);
	break;
      case DF_STRING:
	free_dl2 = 1;
	dl2 = dfuCreateDynList(DF_STRING, 1);
	dfuAddDynListString(dl2, argv[2]);
	break;
      }
    }
  }

  
  switch (mode) {
  case DL_FIND:
    result = dynListFindSublist(dl1, dl2);
    break;
  case DL_FIND_INDICES:
    result = dynListFindIndices(dl1, dl2);
    break;
  case DL_FIND_ALL:
  case DL_FIND_PATTERNS:
    result = dynListFindSublistAll(dl1, dl2);
    break;
  case DL_COUNT_OCCURENCES:
    result = dynListCountOccurences(dl1, dl2);
    break;
  }

  if (free_dl2) dfuFreeDynList(dl2);
  if (!result) {
    if (mode == DL_FIND) {
      Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
      return TCL_OK;
    }
    else {
      Tcl_AppendResult(interp, argv[0], ": bad arguments", (char *) NULL);
      return TCL_ERROR;
    }
  }
  else {
    switch (mode) {
    case DL_FIND:
      tclDynListToTclList(interp, result);
      dfuFreeDynList(result);
      return TCL_OK;
      break;
    case DL_FIND_INDICES:
    case DL_FIND_PATTERNS:
    case DL_FIND_ALL:
    case DL_COUNT_OCCURENCES:
      return(tclPutList(interp, result));
      break;
    default:
      return TCL_OK;
    }
  }

 usage:
  Tcl_ResetResult(interp);
  Tcl_AppendResult(interp, "usage: ", argv[0], 
		   " {dynlist|value} {dynlist|value}",
		   (char *) NULL);
  return TCL_ERROR;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclCompareDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_equal...
 *
 * DESCRIPTION
 *    Elementwise comparator
 *
 *****************************************************************************/

static int tclCompareDynList (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  DYN_LIST *dl1, *dl2, *newlist = NULL;
  int free_dl1 = 0, free_dl2 = 0;
  int operation = (Tcl_Size) data;

  if (operation == DL_AND || operation == DL_OR) {
    if (argc < 3) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "usage: ", argv[0], 
		       " list1 list2 [list3 ...]", (char *) NULL);
      return TCL_ERROR;
    }
  }
  else if (argc != 3) {
  usage:
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " list1 list2", (char *) NULL);
    return TCL_ERROR;
  }


  /*
   * Parse the args in the following way:
   *    Check for list
   *    Make it a string
   */
  
  if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) {
    if ((tclFindDynList(interp, argv[2], &dl2) == TCL_OK) &&
	DYN_LIST_DATATYPE(dl2) == DF_STRING) {
      dl1 = dfuCreateDynList(DF_STRING, 1);
      dfuAddDynListString(dl1, argv[1]);
      free_dl1 = 1;
    }
    else goto usage;
  }

  if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) {
    if (DYN_LIST_DATATYPE(dl1) == DF_STRING || 
	DYN_LIST_DATATYPE(dl1) == DF_LIST) {
      dl2 = dfuCreateDynList(DF_STRING, 1);
      dfuAddDynListString(dl2, argv[2]);
      free_dl2 = 1;
    }
    else {
      if (free_dl1) dfuFreeDynList(dl1);
      goto usage;
    }
  }

  if (DYN_LIST_DATATYPE(dl1) == DF_STRING || 
      DYN_LIST_DATATYPE(dl2) == DF_STRING) {
    if (operation == DL_AND || operation == DL_OR) {
      if (free_dl1) dfuFreeDynList(dl1);
      if (free_dl2) dfuFreeDynList(dl2);
      Tcl_AppendResult(interp, argv[0], ": operation not supported for strings",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  /* Allow greater than two args */
  if (operation == DL_AND || operation == DL_OR) {
    int op, i;
    DYN_LIST *newlist1;
    if (operation == DL_AND) op = DL_RELATION_AND;
    else op = DL_RELATION_OR;

    newlist = dynListRelationListList(dl1, dl2, op);
    for (i = 3; i < argc; i++) {
      if (!newlist) goto finished;
      if (tclFindDynList(interp, argv[i], &dl1) != TCL_OK) {
	dfuFreeDynList(newlist);
	return TCL_ERROR;
      }
      newlist1 = dynListRelationListList(newlist, dl1, op);
      dfuFreeDynList(newlist);
      newlist = newlist1;
    }
    goto finished;
  }
  
  switch (operation) {
  case DL_EQUAL_TO:
    newlist = dynListRelationListList(dl1, dl2, DL_RELATION_EQ);
    break;
  case DL_ONEOF:
    newlist = dynListOneOf(dl1, dl2);
    break;
  case DL_NOT_EQUAL_TO:
    newlist = dynListRelationListList(dl1, dl2, DL_RELATION_NE);
    break;
  case DL_LESS_THAN:
    newlist = dynListRelationListList(dl1, dl2, DL_RELATION_LT);
    break;
  case DL_GREATER_THAN:
    newlist = dynListRelationListList(dl1, dl2, DL_RELATION_GT);
    break;
  case DL_LTE:
    newlist = dynListRelationListList(dl1, dl2, DL_RELATION_LTE);
    break;
  case DL_GTE:
    newlist = dynListRelationListList(dl1, dl2, DL_RELATION_GTE);
    break;
  case DL_MOD:
    newlist = dynListRelationListList(dl1, dl2, DL_RELATION_MOD);
    break;
  case DL_STRINGMATCH:
    if ((DYN_LIST_DATATYPE(dl1) == DF_STRING || 
	 DYN_LIST_DATATYPE(dl1) == DF_LIST) &&
	DYN_LIST_DATATYPE(dl2) == DF_STRING) {
      newlist = dynListStringMatch(dl1, dl2, 0);
    }
  }
  
  if (free_dl1) dfuFreeDynList(dl1);
  if (free_dl2) dfuFreeDynList(dl2);
  
finished:
  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": unable to compare \"", argv[1],
		     "\" with \"", argv[2], "\"", (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclCompareDynListIndices
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_equalIndex...
 *
 * DESCRIPTION
 *    Elementwise comparator
 *
 *****************************************************************************/

static int tclCompareDynListIndices (ClientData data, Tcl_Interp *interp,
				     int argc, char *argv[])
{
  DYN_LIST *dl1, *dl2, *newlist = NULL;
  int free_dl1 = 0, free_dl2 = 0;
  int operation = (Tcl_Size) data;

  if (operation == DL_AND || operation == DL_OR) {
    if (argc < 3) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "usage: ", argv[0], 
		       " list1 list2 [list3 ...]", (char *) NULL);
      return TCL_ERROR;
    }
  }
  else if (argc != 3) {
  usage:
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " list1 list2", (char *) NULL);
    return TCL_ERROR;
  }


  /*
   * Parse the args in the following way:
   *    Check for list
   *    Make it a string
   */
  
  if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) {
    if ((tclFindDynList(interp, argv[2], &dl2) == TCL_OK) &&
	DYN_LIST_DATATYPE(dl2) == DF_STRING) {
      dl1 = dfuCreateDynList(DF_STRING, 1);
      dfuAddDynListString(dl1, argv[1]);
      free_dl1 = 1;
    }
    else goto usage;
  }

  if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) {
    if (DYN_LIST_DATATYPE(dl1) == DF_STRING || 
	DYN_LIST_DATATYPE(dl1) == DF_LIST) {
      dl2 = dfuCreateDynList(DF_STRING, 1);
      dfuAddDynListString(dl2, argv[2]);
      free_dl2 = 1;
    }
    else {
      if (free_dl1) dfuFreeDynList(dl1);
      goto usage;
    }
  }

  if (DYN_LIST_DATATYPE(dl1) == DF_STRING || 
      DYN_LIST_DATATYPE(dl2) == DF_STRING) {
    if (operation == DL_AND || operation == DL_OR) {
      if (free_dl1) dfuFreeDynList(dl1);
      if (free_dl2) dfuFreeDynList(dl2);
      Tcl_AppendResult(interp, argv[0], ": operation not supported for strings",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  /* Allow greater than two args */
  if (operation == DL_AND || operation == DL_OR) {
    int op, i;
    DYN_LIST *newlist1;
    if (operation == DL_AND) op = DL_RELATION_AND;
    else op = DL_RELATION_OR;

    newlist = dynListRelationListListIndices(dl1, dl2, op);
    for (i = 3; i < argc; i++) {
      if (!newlist) goto finished;
      if (tclFindDynList(interp, argv[i], &dl1) != TCL_OK) {
	dfuFreeDynList(newlist);
	return TCL_ERROR;
      }
      newlist1 = dynListRelationListListIndices(newlist, dl1, op);
      dfuFreeDynList(newlist);
      newlist = newlist1;
    }
    goto finished;
  }
  
  switch (operation) {
  case DL_EQUAL_TO:
    newlist = dynListRelationListListIndices(dl1, dl2, DL_RELATION_EQ);
    break;
  case DL_ONEOF:
    newlist = dynListOneOfIndices(dl1, dl2);
    break;
  case DL_NOT_EQUAL_TO:
    newlist = dynListRelationListListIndices(dl1, dl2, DL_RELATION_NE);
    break;
  case DL_LESS_THAN:
    newlist = dynListRelationListListIndices(dl1, dl2, DL_RELATION_LT);
    break;
  case DL_GREATER_THAN:
    newlist = dynListRelationListListIndices(dl1, dl2, DL_RELATION_GT);
    break;
  case DL_LTE:
    newlist = dynListRelationListListIndices(dl1, dl2, DL_RELATION_LTE);
    break;
  case DL_GTE:
    newlist = dynListRelationListListIndices(dl1, dl2, DL_RELATION_GTE);
    break;
  case DL_MOD:
    newlist = dynListRelationListListIndices(dl1, dl2, DL_RELATION_MOD);
    break;
  case DL_STRINGMATCH:
    if ((DYN_LIST_DATATYPE(dl1) == DF_STRING || 
	 DYN_LIST_DATATYPE(dl1) == DF_LIST) &&
	DYN_LIST_DATATYPE(dl2) == DF_STRING) {
      newlist = dynListStringMatch(dl1, dl2, 0);
    }
  }
  
  if (free_dl1) dfuFreeDynList(dl1);
  if (free_dl2) dfuFreeDynList(dl2);
  
finished:
  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": unable to compare \"", argv[1],
		     "\" with \"", argv[2], "\"", (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclDynListNot
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_not
 *
 * DESCRIPTION
 *    Elementwise not operation
 *
 *****************************************************************************/

static int tclDynListNot (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  DYN_LIST *dl1, *newlist = NULL;

  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) return TCL_ERROR;
  newlist = dynListNotList(dl1);
  if (!newlist) {
    Tcl_AppendResult(interp, argv[0], 
		     ": unable to negate list \"", argv[1], "\"",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclArithDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_add
 *    dl_sub
 *    dl_mult
 *    dl_div
 *    dl_conv
 *    dl_pow
 *    dl_atan2
 *    dl_fmod
 *
 * DESCRIPTION
 *    Elementwise/Rowwise arithmetic operations on dynLists.
 *
 *****************************************************************************/

static int tclArithDynList (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  int i;
  DYN_LIST *dl1, *dl2, *newlist = NULL, *newlist1;
  int operation = (Tcl_Size) data;
  int mathop = -1;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " list1 list2 [list3...]",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) return TCL_ERROR;
  
  switch (operation) {
  case DL_ADD:       mathop = DL_MATH_ADD;     break;
  case DL_MULTIPLY:  mathop = DL_MATH_MUL;     break;
  case DL_SUBTRACT:  mathop = DL_MATH_SUB;     break;
  case DL_DIVIDE:    mathop = DL_MATH_DIV;     break;
  }

  if (mathop != -1) {		/* + - * / */
    newlist = dynListArithListList(dl1, dl2, mathop);
    for (i = 3; i < argc; i++) {
      if (!newlist) goto error;
      if (tclFindDynList(interp, argv[i], &dl1) != TCL_OK) {
	dfuFreeDynList(newlist);
	return TCL_ERROR;
      }
      newlist1 = dynListArithListList(newlist, dl1, mathop);
      dfuFreeDynList(newlist);
      newlist = newlist1;
    }
  }  
  else if (operation == DL_POW) {
    newlist = dynListArithListList(dl1, dl2, DL_MATH_POW);
  }
  else if (operation == DL_ATAN2) {
    newlist = dynListArithListList(dl1, dl2, DL_MATH_ATAN2);
  }
  else if (operation == DL_FMOD) {
    newlist = dynListArithListList(dl1, dl2, DL_MATH_FMOD);
  }
  else if (operation == DL_CONV) {
    /*
    if ((DYN_LIST_N(dl2) != DF_LIST) && !(DYN_LIST_N(dl2) % 2)) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], 
		       ": convolution kernel must have odd number of elements",
		       (char *) NULL);
      return TCL_ERROR;
    }
    */
    newlist = dynListConvList(dl1, dl2);
  }
  
  else if (operation == DL_CONV2) {
    newlist = dynListConvList2(dl1, dl2);
  }
  
  if (!newlist) {
  error:
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], 
		     ": unable to combine \"", argv[1], "\" and \"", 
		     argv[2], "\" arithmetically",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclMathFuncOneArg
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_abs, acos, ...
 *
 * DESCRIPTION
 *    Elementwise math functions which take a single argument
 *
 *****************************************************************************/

static int tclMathFuncOneArg (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  DYN_LIST *dl, *newlist;
  double k;
  int n;
  int operation = (Tcl_Size) data;
  int free_dl = 0;

  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) {
    if (Tcl_GetInt(interp, argv[1], &n) == TCL_OK) {
      dl = dfuCreateDynList(DF_LONG, 1);
      dfuAddDynListLong(dl, n);
      free_dl = 1;
    }
    else if (Tcl_GetDouble(interp, argv[1], &k) == TCL_OK) {
      dl = dfuCreateDynList(DF_FLOAT, 1);
      dfuAddDynListFloat(dl, k);
      free_dl = 1;
    }
    else {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "usage: ", argv[0], " {dynlist|#}",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  if (operation >= DL_N_MATHFUNC1) {
    Tcl_AppendResult(interp, argv[0], ": invalid function id",
		     (char *) NULL);
    if (free_dl) dfuFreeDynList(dl);
    return TCL_ERROR;
  }
  
  if (DYN_LIST_DATATYPE(dl) == DF_STRING) {
    Tcl_AppendResult(interp, argv[0], ": invalid list operand",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  newlist = dynListMathOneArg(dl, operation);
  
  if (!newlist) {
    Tcl_ResetResult(interp);
    switch (operation) {
    case DL_SQRT:
      Tcl_AppendResult(interp, argv[0], ": list elements must be nonnegative",
		       (char *) NULL);
      if (free_dl) dfuFreeDynList(dl);
      return TCL_ERROR;
    case DL_LOG:
    case DL_LOG10:
      if (free_dl) dfuFreeDynList(dl);
      Tcl_AppendResult(interp, argv[0], ": list elements must be positive",
		       (char *) NULL);
      return TCL_ERROR;
    case DL_ASIN:
    case DL_ACOS:
    case DL_ATAN:
      if (free_dl) dfuFreeDynList(dl);
      Tcl_AppendResult(interp, argv[0], 
		       ": list elements must be in the range [-1,1]",
		       (char *) NULL);
      return TCL_ERROR;
    default:
      if (free_dl) dfuFreeDynList(dl);
      Tcl_AppendResult(interp, argv[0], ": matherr",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  else {
    if (free_dl) dfuFreeDynList(dl);
    return(tclPutList(interp, newlist));
  }
}


/*
 * Tcl command: dl_srand
 * 
 * Usage:
 *   dl_srand           - returns current seed
 *   dl_srand 0         - auto-seed from system entropy, returns new seed
 *   dl_srand <n>       - set seed to n (n > 0), returns n
 *
 */

/* Declare external functions from randvars.c */
extern int ranset(int seed);
extern int ranget(void);

static int tclSetRandomSeed(ClientData clientData, Tcl_Interp *interp,
                       int argc, char *argv[])
{
    int seed, result;
    
    if (argc == 1) {
        /* No argument - return current seed */
        Tcl_SetObjResult(interp, Tcl_NewIntObj(ranget()));
        return TCL_OK;
    }
    
    if (argc != 2) {
        Tcl_AppendResult(interp, "usage: ", argv[0], " [seed]", NULL);
        return TCL_ERROR;
    }
    
    if (Tcl_GetInt(interp, argv[1], &seed) != TCL_OK) {
        return TCL_ERROR;
    }
    
    result = ranset(seed);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(result));
    return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclGenerateDynList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_zeros
 *    dl_ones
 *
 * DESCRIPTION
 *    Generate a constant or random list of certain size 
 *
 *****************************************************************************/

static int tclGenerateDynList (ClientData data, Tcl_Interp *interp,
			       int argc, char *argv[])
{
  int max;			/* for the integer random number range */
  int size;
  double sizef;
  DYN_LIST *newlist = NULL, *sizelist, *maxlist;
  int freesize = 0;
  int operation = (Tcl_Size) data;

  if (operation == DL_RANDCHOOSE) {
    if (argc != 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " m n",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  else if (operation == DL_IRAND) {
    if (argc != 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " size n (for [0-n))",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  else if (operation == DL_GAUSSIAN_2D) {
    if (argc != 4) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " width height sd",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  else if (operation == DL_SHUFFLE) {
    DYN_LIST *dl;
    if (argc != 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " list",
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
    newlist = dynListShuffleList(dl);
    if (!newlist) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " list",
		       (char *) NULL);
      return TCL_ERROR;
    }
    return(tclPutList(interp, newlist));
  }

  else if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " size",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (operation == DL_GAUSSIAN_2D) {
    int x, y;
    double sd;
    if (Tcl_GetInt(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetDouble(interp, argv[3], &sd) != TCL_OK) return TCL_ERROR;

    newlist = dynListGaussian2D(x, y, sd);
  }
  if (operation == DL_RANDCHOOSE) {
    int m, n;

    if (Tcl_GetInt(interp, argv[1], &m) == TCL_OK &&
	Tcl_GetInt(interp, argv[2], &n) == TCL_OK) {
      if (n > m) {
	Tcl_AppendResult(interp, argv[0], ": n must be <= m",
			 (char *) NULL);
	return TCL_ERROR;
      }
      if (m < 0 || n < 0) {
	Tcl_AppendResult(interp, argv[0], ": n and m must be nonneg",
			 (char *) NULL);
	return TCL_ERROR;
      }
      if (n) newlist = dynListRandChooseInt(m, n);
      else newlist = dfuCreateDynList(DF_LONG, 1);
    }
    else {
      DYN_LIST *dl1, *dl2;
      if (tclFindDynList(interp, argv[1], &dl1) == TCL_OK &&
	  tclFindDynList(interp, argv[2], &dl2) == TCL_OK) {
	newlist = dynListRandChoose(dl1, dl2);
      }
      else {
	Tcl_AppendResult(interp, "usage: ", argv[0], " m n",
			 (char *) NULL);
	return TCL_ERROR;
      }
    }
  }
  if (Tcl_GetInt(interp, argv[1], &size) == TCL_OK) {
    if (size < 0) {
      Tcl_AppendResult(interp, argv[0], ": size must be nonneg", NULL);
      return TCL_ERROR;
    }

    if (operation == DL_IRAND) {
      if (Tcl_GetInt(interp, argv[2], &max) != TCL_OK) 
	return TCL_ERROR;
    }
    
    if (!size) {
      switch (operation) {
      case DL_ZEROS:
      case DL_ONES:
	newlist = dfuCreateDynList(DF_LONG, 1);
	break;
      default:
	newlist = dfuCreateDynList(DF_FLOAT, 1);
	break;
      }
    }
    else {
      switch (operation) {
      case DL_ZEROS:  newlist = dynListZerosInt(size);         break;
      case DL_ONES:   newlist = dynListOnesInt(size);          break;
      case DL_IRAND:  newlist = dynListUniformIRandsInt(size, max); break;
      case DL_URAND:  newlist = dynListUniformRandsInt(size);  break;
      case DL_ZRAND:  newlist = dynListNormalRandsInt(size);   break;
      case DL_RANDFILL:  newlist = dynListRandFillInt(size);   break;
      }
    }
  }
  else if (Tcl_GetDouble(interp, argv[1], &sizef) == TCL_OK) {
    size = (int) sizef;

    if (size < 0) {
      Tcl_AppendResult(interp, argv[0], ": size must be nonneg", NULL);
      return TCL_ERROR;
    }

    if (operation == DL_IRAND) {
      if (Tcl_GetInt(interp, argv[2], &max) != TCL_OK) 
	return TCL_ERROR;
    }
    
    if (!size) newlist = dfuCreateDynList(DF_FLOAT, 1);
    else {
      switch (operation) {
      case DL_ZEROS:  newlist = dynListZerosFloat(size);       break;
      case DL_ONES:   newlist = dynListOnesFloat(size);        break;
      case DL_IRAND:  newlist = dynListUniformIRandsInt(size, max); break;
      case DL_URAND:  newlist = dynListUniformRandsInt(size);  break;
      case DL_ZRAND:  newlist = dynListNormalRandsInt(size);   break;
      case DL_RANDFILL:  newlist = dynListRandFillInt(size);   break;
      }
    }
  }
  else if (tclFindDynList(interp, argv[1], &sizelist) == TCL_OK) {

    if (operation == DL_IRAND) {
      if (tclFindDynList(interp, argv[2], &maxlist) != TCL_OK ||
	  (DYN_LIST_DATATYPE(maxlist) != DF_LONG &&
	   DYN_LIST_DATATYPE(maxlist) != DF_LIST)) {
	Tcl_AppendResult(interp, argv[0], ": irand max must be int list", NULL);
	return TCL_ERROR;
      }
    }

    switch (operation) {
    case DL_ZEROS:
      newlist = dynListZeros(sizelist);
      break;
    case DL_ONES:
      newlist = dynListOnes(sizelist);
      break;
    case DL_URAND:
      newlist = dynListUniformRands(sizelist);
      break;
    case DL_IRAND:
      newlist = dynListUniformIRands(sizelist, maxlist);
      break;
    case DL_ZRAND:
      newlist = dynListNormalRands(sizelist);
      break;
    case DL_RANDFILL:
      newlist = dynListRandFill(sizelist);
      break;
    }
  }
  else {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": invalid size specification (",
		     argv[1], ")", NULL);
    return TCL_ERROR;
  }

  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": unable to create new list",
		     (char *) NULL);
    if (freesize) dfuFreeDynList(sizelist);
    return TCL_ERROR;
  }
  else {
    if (freesize) dfuFreeDynList(sizelist);
    return(tclPutList(interp, newlist));
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclSeries
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_series
 *
 * DESCRIPTION
 *    Create a series of either floats or ints
 *
 *****************************************************************************/

static int tclSeries (ClientData data, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  DYN_LIST *newlist, *startlist, *stoplist, *steplist;
  int alloced_steplist = 0;
  int operation = (Tcl_Size) data;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " start stop [step]",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /* If no decimal place in any of the args, make an int series if possible */

  if (!strchr(argv[1],'.') && !strchr(argv[2],'.') && 
      (argc < 4 || !strchr(argv[3],'.'))) {
    int start, stop, step;
    if (Tcl_GetInt(interp, argv[1], &start) != TCL_OK) 
      goto TRY_LISTS;
    if (Tcl_GetInt(interp, argv[2], &stop) != TCL_OK) 
      goto TRY_LISTS;
    if (argc > 3) {
      if (Tcl_GetInt(interp, argv[3], &step) != TCL_OK) 
	goto TRY_LISTS;
    }
    else {
      if (start < stop) step = 1;
      else if (start > stop) step = -1;
      else step = 0;
    }
    if (operation == DL_SERIES) 
      newlist = dynListSeriesLong(start, stop, step);
    else			/* exclude last value */
      newlist = dynListSeriesLongExclude(start, stop, step);
  }
  
  else {
    double start, stop, step;
    if (Tcl_GetDouble(interp, argv[1], &start) != TCL_OK) 
      goto TRY_LISTS;
    if (Tcl_GetDouble(interp, argv[2], &stop) != TCL_OK) 
      goto TRY_LISTS;
    if (argc > 3) {
      if (Tcl_GetDouble(interp, argv[3], &step) != TCL_OK)
	goto TRY_LISTS;
    }
    else {
      if (start < stop) step = 1.0;
      else if (start > stop) step = -1.0;
      else step = 0.0;
    }
    if (operation == DL_SERIES) 
      newlist = dynListSeriesFloat(start, stop, step);
    else 
      newlist = dynListSeriesFloatExclude(start, stop, step);
  }
  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": unable to create series",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }

TRY_LISTS:
  if (tclFindDynList(interp, argv[1], &startlist) != TCL_OK ||
      tclFindDynList(interp, argv[2], &stoplist) != TCL_OK)
    return TCL_ERROR;

  /* If there is no fourth arg, create a list of ones */
  if (argc < 4) {
    steplist = dynListOnesInt(1);
    alloced_steplist = 1;
  }
  else if (tclFindDynList(interp, argv[3], &steplist) != TCL_OK) {
    return TCL_ERROR;
  }

  if (operation == DL_SERIES) 
    newlist = dynListSeries(startlist, stoplist, steplist, 1);
  else 
    newlist = dynListSeries(startlist, stoplist, steplist, 0);

  if (alloced_steplist) dfuFreeDynList(steplist);
  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": unable to create series",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclListFromList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_recip
 *    dl_negate
 *    dl_sign
 *    dl_transpose
 *    dl_transposeAt
 *    dl_collapse
 *    dl_pack
 *    dl_deepPack
 *    dl_unpack
 *    dl_sort
 *    dl_bsort
 *    dl_sortIndices
 *    dl_bsortIndices
 *    dl_lengths
 *    dl_unique
 *    dl_cross
 *    dl_idiff
 *    dl_cumsum
 *    dl_cumprod
 *    dl_indices
 *    dl_llength
 *    dl_fileroot
 *    dl_filedir
 *    dl_filetail
 *    dl_fileext
 *
 * DESCRIPTION
 *    Elementwise arithmetic operations on dynLists.
 *
 *****************************************************************************/

static int tclListFromList (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  DYN_LIST *dl, *newlist = NULL;
  int operation = (Tcl_Size) data;
  int lag = 1, shift = 1, level = 0;

  if (operation == DL_DIFF) {
    if (argc < 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist [lag]",
		       (char *) NULL);
      return TCL_ERROR;
    }
    else if (argc > 2) {
      if (Tcl_GetInt(interp, argv[2], &lag) != TCL_OK) return TCL_ERROR;
    }
    if (lag <= 0) {
      Tcl_AppendResult(interp, argv[0], ": lag value out of range",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  else if (operation == DL_TRANSPOSE_AT) {
    if (argc < 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist level",
		       (char *) NULL);
      return TCL_ERROR;
    }
    else if (argc > 2) {
      if (Tcl_GetInt(interp, argv[2], &level) != TCL_OK) return TCL_ERROR;
    }
  }
  else if (operation == DL_SUBSHIFT || 
	   operation == DL_SHIFTCYCLE || 
	   operation == DL_BSHIFT || 
	   operation == DL_BSHIFTCYCLE || 
	   operation == DL_SHIFT) {
    if (argc < 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist shift",
		       (char *) NULL);
      return TCL_ERROR;
    }
    else if (argc > 2) {
      if (Tcl_GetInt(interp, argv[2], &shift) != TCL_OK) return TCL_ERROR;
    }
  }
  
  else if ((operation != DL_RECODE &&
	    operation != DL_CUT &&
	    operation != DL_IDIFF) && argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else if ((operation == DL_IDIFF ||
	    operation == DL_RECODE) && argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist [dynlist]",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else if (operation == DL_CUT && argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist dynlist",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  
  switch (operation) {
  case DL_RECIPROCAL:
    {
      DYN_LIST *one = dfuCreateDynList(DF_FLOAT, 1);
      dfuAddDynListFloat(one, 1.0);
      newlist = dynListArithListList(one, dl, DL_MATH_DIV);
      dfuFreeDynList(one);
    }
    break;
  case DL_IDIFF:
    {
      DYN_LIST *dl2;
      int autocorr = 0;
      if (argc > 2) {
	if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) return TCL_ERROR;
      }
      else {
	dl2 = dl;
	autocorr = 1;
      }
      newlist = dynListIdiffLists(dl, dl2, autocorr);
    }
    break;
  case DL_NEGATE:
    newlist = dynListNegateList(dl);
    break;
  case DL_ZERO_CROSSING:
    newlist = dynListZeroCrossingList(dl);
    break;
  case DL_SIGN:
    newlist = dynListSignList(dl);
    break;
  case DL_TRANSPOSE:
    newlist = dynListTransposeList(dl);
    break;
  case DL_TRANSPOSE_AT:
    newlist = dynListTransposeListAt(dl, level);
    break;
  case DL_COLLAPSE:
    newlist = dynListCollapseList(dl);
    break;
  case DL_PACK:
    newlist = dynListPackList(dl);
    break;
  case DL_DEEP_PACK:
    newlist = dynListDeepPackList(dl);
    break;
  case DL_UNPACK:
    newlist = dynListUnpackList(dl);
    break;
  case DL_UNPACK_LISTS:
    newlist = dynListUnpackLists(dl, NULL);
    break;
  case DL_DEEP_UNPACK:
    newlist = dynListDeepUnpackList(dl);
    break;
  case DL_SORT:
    newlist = dynListSortList(dl);
    break;
  case DL_SORT_INDICES:
    newlist = dynListSortListIndices(dl);
    break;
  case DL_BSORT:
    newlist = dynListBSortList(dl);
    break;
  case DL_BSORT_INDICES:
    newlist = dynListBSortListIndices(dl);
    break;
  case DL_RANK:
    newlist = dynListRankOrderedList(dl);
    break;
  case DL_RECODE_WITH_TIES:
    newlist = dynListRecodeWithTiesList(dl);
    break;
  case DL_RECODE:
    {
      DYN_LIST *dl2;

      if (argc > 2) {
	if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) return TCL_ERROR;
	newlist = dynListRecodeListFromList(dl, dl2);
      }
      else {
	newlist = dynListRecodeList(dl);
      }
    }
    break;
  case DL_CUT:
    {
      DYN_LIST *dl2;

      if (argc < 2) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, argv[0], ": no breaks specified",
		     (char *) NULL);
	return TCL_ERROR;
      } else {
	if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) return TCL_ERROR;
	newlist = dynListCutList(dl, dl2);
      }
    }
    break;
  case DL_GRADIENT:
    newlist = dynListGradientList(dl);
    break;
  case DL_LENGTHS:
    newlist = dynListListLengths(dl);
    break;
  case DL_LLENGTH:
    newlist = dynListLLength(dl);
    if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
      tclDynListToTclList(interp, newlist);    
      dfuFreeDynList(newlist);
      return TCL_OK;
    }
    break;
  case DL_UNIQUE:
    newlist = dynListUniqueList(dl);
    break;
  case DL_UNIQUE_NO_SORT:
    newlist = dynListUniqueNoSortList(dl);
    break;
  case DL_CUMSUM:
    newlist = dynListCumSumProdList(dl, DL_SUM_LIST);
    break;
  case DL_CUMPROD:
    newlist = dynListCumSumProdList(dl, DL_PROD_LIST);
    break;
  case DL_INDICES:
    newlist = dynListIndices(dl);
    break;
  case DL_INDEX_LIST:
    newlist = dynListIndexList(dl);
    break;
  case DL_LAST_INDEX_LIST:
    newlist = dynListLastIndexList(dl);
    break;
  case DL_FIRST_INDEX_LIST:
    newlist = dynListFirstIndexList(dl);
    break;
  case DL_DIFF:
    newlist = dynListDiffList(dl, lag);
    break;
  case DL_SHIFTCYCLE:
    newlist = dynListShift(dl, shift, DL_SHIFT_CYCLE);
    break;
  case DL_SHIFT:
    if (shift < 0) {
      shift = -shift;
      newlist = dynListShift(dl, shift, DL_SHIFT_LEFT);
    }
    else {
      newlist = dynListShift(dl, shift, DL_SHIFT_RIGHT);
    }
    break;
  case DL_SUBSHIFT:
    if (shift < 0) {
      shift = -shift;
      newlist = dynListShift(dl, shift, DL_SUBSHIFT_LEFT);
    }
    else {
      newlist = dynListShift(dl, shift, DL_SUBSHIFT_RIGHT);
    }
    break;
  case DL_BSHIFTCYCLE:
    newlist = dynListBShift(dl, shift, DL_SHIFT_CYCLE);
    break;
  case DL_BSHIFT:
    if (shift < 0) {
      shift = -shift;
      newlist = dynListBShift(dl, shift, DL_SHIFT_LEFT);
    }
    else {
      newlist = dynListBShift(dl, shift, DL_SHIFT_RIGHT);
    }
    break;
  case DL_FILE_ROOT:
  case DL_FILE_EXT:
  case DL_FILE_TAIL:
  case DL_FILE_DIR:
  case DL_FILE_BASE:
    newlist = dynListFileString(interp, dl, operation);
    break;
  }
  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": bad operand (", argv[1], ")",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclReshapeList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_reshape
 *
 * DESCRIPTION
 *    Reshape a list into two-dimensional list
 *
 *****************************************************************************/

static int tclReshapeList (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DYN_LIST *dl, *newlist;
  int nrows, ncols;
  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " dynlist {nrows|-} {ncols|-}", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &nrows) != TCL_OK) nrows = -1;
  if (Tcl_GetInt(interp, argv[3], &ncols) != TCL_OK) ncols = -1;
  
  if ((!nrows || nrows == -1) && (!ncols || ncols == -1)) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], "invalid rows / cols specified",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else if ((!nrows || nrows == -1)) {
    nrows = DYN_LIST_N(dl) / ncols;
  }
  else if ((!ncols || ncols == -1)) {
    ncols = DYN_LIST_N(dl) / nrows;
  }
  if (nrows * ncols != DYN_LIST_N(dl)) {
    char resultstr[256];
    snprintf(resultstr, sizeof(resultstr),
	     "%s: cannot create a %dx%d list from %d elements",
	     argv[0], nrows, ncols, DYN_LIST_N(dl));
    Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
    return TCL_ERROR;
  }
  newlist = dynListReshapeList(dl, nrows, ncols);
  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": error reshaping list \"", argv[1], 
		     "\"", (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclRestructureList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_restructure
 *
 * DESCRIPTION
 *    Restructure a list to look like another
 *
 *****************************************************************************/

static int tclRestructureList (ClientData data, Tcl_Interp *interp,
			       int argc, char *argv[])
{
  DYN_LIST *dl1, *dl2, *newlist;
  int start = 0;
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " dynlist_source dynlist_to_match", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) return TCL_ERROR;
  newlist = dynListRestructureList(dl1, dl2, 1, &start, NULL);
  if (!newlist) {
    Tcl_AppendResult(interp, argv[0], 
		     ": error restructuring list", (char *) NULL);
    return TCL_ERROR;
  }
  return(tclPutList(interp, newlist));
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclRepList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_repeat
 *    dl_repeatElements
 *    dl_replicate
 *
 * DESCRIPTION
 *    Repeat elements or whole list n times
 *
 *****************************************************************************/

static int tclRepList (ClientData data, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  DYN_LIST *dl, *newlist = NULL;
  int n;
  int operation = (Tcl_Size) data;

  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist n",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  
  switch (operation) {
  case DL_REPEAT:
    {
      DYN_LIST *rep = NULL;
      if (tclFindDynList(interp, argv[2], &rep) == TCL_OK) {
	if (DYN_LIST_DATATYPE(rep) != DF_LONG &&
	    DYN_LIST_DATATYPE(rep) != DF_LIST) {
	  Tcl_AppendResult(interp, argv[0], ": repeat list invalid (must be ints or lists)",
			   (char *) NULL);
	  return TCL_ERROR;
	}
	newlist = dynListRepeat(dl, rep);
      }
      else {
	Tcl_ResetResult(interp);
	if (Tcl_GetInt(interp, argv[2], &n) != TCL_OK) return TCL_ERROR;
	else { 
	  rep = dfuCreateDynList(DF_LONG, 1);
	  dfuAddDynListLong(rep, n);
	  newlist = dynListRepeat(dl, rep);
	  dfuFreeDynList(rep);
	}
      }
      break;
    }
  case DL_REPEAT_ELTS:
    {
      DYN_LIST *rep = NULL;
      if (tclFindDynList(interp, argv[2], &rep) == TCL_OK) {
	if (DYN_LIST_DATATYPE(rep) != DF_LONG) {
	  Tcl_AppendResult(interp, argv[0], ": repeat list must contain ints",
			   (char *) NULL);
	  return TCL_ERROR;
	}
	newlist = dynListRepeatElements(dl, rep);
      }
      break;
    }
  case DL_REPLICATE:
    if (Tcl_GetInt(interp, argv[2], &n) != TCL_OK) return TCL_ERROR;
    newlist = dynListReplicate(dl, n);
    break;
  }
  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": invalid arguments",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}




/*****************************************************************************
 *
 * FUNCTION
 *    tclFillList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_fill
 *
 * DESCRIPTION
 *    fills a new list using sparse data
 *
 *****************************************************************************/

static int tclFillList (ClientData data, Tcl_Interp *interp,
			int argc, char *argv[])
{
  DYN_LIST *vals, *times, *range, *newlist;
  int res = 1;
  double fres = 1.0;

  if (argc < 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " vals times start stop [res]", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &vals) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &times) != TCL_OK) return TCL_ERROR;

  if (tclGetNumericRange(interp, argv[3], argv[4], &range) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad range specified", (char *) NULL);
    return TCL_ERROR;
  }

  switch (DYN_LIST_DATATYPE(range)) {
  case DF_LONG:
    if (argc > 5) {
      if (Tcl_GetInt(interp, argv[5], &res) != TCL_OK) {
	dfuFreeDynList(range);
	return TCL_ERROR;
      }
    }
    dfuAddDynListLong(range, res);
    break;
  case DF_FLOAT:
    if (argc > 5) {
      if (Tcl_GetDouble(interp, argv[5], &fres) != TCL_OK) {
	dfuFreeDynList(range);
	return TCL_ERROR;
      }
    }
    dfuAddDynListFloat(range, fres);
    break;
  }

  newlist = dynListFillSparse(vals, times, range);

  if (!newlist) {
    dfuFreeDynList(range);
    Tcl_AppendResult(interp, argv[0], 
		     ": error filling sparse list", (char *) NULL);
    return TCL_ERROR;
  }
  else {
    dfuFreeDynList(range);
    return(tclPutList(interp, newlist));
  }
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclCountLists
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_count
 *    dl_counts
 *    dl_lcounts
 *
 * DESCRIPTION
 *    Count elements falling in given range.  Generally used for counting
 * spikes, but can be used (inefficiently) for histograms, etc.
 *
 *****************************************************************************/

static int tclCountLists (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  DYN_LIST *dl, *newlist, *range;
  int count;
  int mode = (Tcl_Size) data;

  if (argc != 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist start stop",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;

  if (tclGetNumericRange(interp, argv[2], argv[3], &range) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad range specified", (char *) NULL);
    return TCL_ERROR;
  }

  if (mode == DL_SINGLE) {
    count = dynListCountList(dl, range);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(count));
    dfuFreeDynList(range);
    return TCL_OK;
  }
  else {
    if (mode == DL_RECURSIVE) newlist = dynListCountLists(dl, range);
    else newlist = dynListListCounts(dl, range);
    if (!newlist) {
      dfuFreeDynList(range);
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, argv[0], ": \"", argv[1], "\" is not countable",
		       (char *) NULL);
      return TCL_ERROR;
    }
    else {
      dfuFreeDynList(range);
      return(tclPutList(interp, newlist));
    }
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclHistBins
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_bins
 *
 * DESCRIPTION
 *   Create bins based on start, stop, nbins values
 *
 *****************************************************************************/

static int tclHistBins (ClientData data, Tcl_Interp *interp,
			int argc, char *argv[])
{
  DYN_LIST *newlist;
  DYN_LIST *range;
  int nbins;

  if (argc != 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " start stop nbins",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (tclGetNumericRange(interp, argv[1], argv[2], &range) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad range specified", (char *) NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[3], &nbins) != TCL_OK) return TCL_ERROR;

  newlist = dynListBins(range, nbins);
  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": unable to create bins list",
		     (char *) NULL);
    dfuFreeDynList(range);
    return TCL_ERROR;
  }
  else {
    dfuFreeDynList(range);
    return(tclPutList(interp, newlist));
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclHistLists
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_hist
 *    dl_hists
 *
 * DESCRIPTION
 *   Create histograms for data in lists or create a list of histograms.
 *
 *****************************************************************************/

static int tclHistLists (ClientData data, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  DYN_LIST *dl, *newlist = NULL;
  DYN_LIST *range;
  int nbins;
  int mode = (Tcl_Size) data;
  
  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist start stop nbins",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  if (tclGetNumericRange(interp, argv[2], argv[3], &range) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad range specified", (char *) NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[4], &nbins) != TCL_OK) return TCL_ERROR;

  switch (mode) {
  case DL_SINGLE:
    newlist = dynListHistList(dl, range, nbins);
    break;
  case DL_RECURSIVE:
    newlist = dynListHistLists(dl, range, nbins);
    break;
  }

  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": bad operand (", argv[1], ")",
		     (char *) NULL);
    dfuFreeDynList(range);
    return TCL_ERROR;
  }
  else {
    dfuFreeDynList(range);
    return(tclPutList(interp, newlist));
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclSdfLists
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_sdf
 *    dl_sdfs
 *
 * DESCRIPTION
 *   Create sdfd for data in lists or create a list of sdfs.
 *
 *****************************************************************************/

static int tclSdfLists (ClientData data, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  DYN_LIST *dl, *newlist = NULL;
  DYN_LIST *startl, *stopl;
  int resolution;
  double start, stop, nsd, ksd;
  int mode = (Tcl_Size) data;

  if (argc != 7) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist start stop",
		     " kernel_sd kernel_nsds resolution",
		     (char *) NULL);
    return TCL_ERROR;
  }

  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &ksd) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[5], &nsd) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[6], &resolution) != TCL_OK) return TCL_ERROR;

  switch (mode) {
  case DL_SINGLE:
    if (Tcl_GetDouble(interp, argv[2], &start) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetDouble(interp, argv[3], &stop) != TCL_OK) return TCL_ERROR;
    newlist = dynListSdfList(dl, start, stop, ksd, nsd, resolution);
    break;
  case DL_RECURSIVE:
    if (tclFindDynList(interp, argv[2], &startl) != TCL_OK) return TCL_ERROR;
    if (tclFindDynList(interp, argv[3], &stopl) != TCL_OK) return TCL_ERROR;
    newlist = dynListSdfLists(dl, startl, stopl, ksd, nsd, resolution);
    break;
  }

  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": bad operand (", argv[1], ")",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclSdfLists
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_deepSdf
 *    dl_deepSdfs
 *
 * DESCRIPTION
 *   Create recursive sdfs for data in lists or create a list of sdfs.
 *
 *****************************************************************************/

static int tclSdfListsRecursive (ClientData data, Tcl_Interp *interp,
				 int argc, char *argv[])
{
  DYN_LIST *dl, *newlist = NULL;
  DYN_LIST *startl, *stopl;
  int resolution;
  double start, stop, nsd, ksd;
  int mode = (Tcl_Size) data;

  if (argc != 7) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist start stop",
		     " kernel_sd kernel_nsds resolution",
		     (char *) NULL);
    return TCL_ERROR;
  }

  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &ksd) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[5], &nsd) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[6], &resolution) != TCL_OK) return TCL_ERROR;

  switch (mode) {
  case DL_SINGLE:
    if (Tcl_GetDouble(interp, argv[2], &start) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetDouble(interp, argv[3], &stop) != TCL_OK) return TCL_ERROR;
    newlist = dynListSdfListR(dl, start, stop, ksd, nsd, resolution);
    break;
  case DL_RECURSIVE:
    if (tclFindDynList(interp, argv[2], &startl) != TCL_OK) return TCL_ERROR;
    if (tclFindDynList(interp, argv[3], &stopl) != TCL_OK) return TCL_ERROR;
    newlist = dynListSdfListsR(dl, startl, stopl, ksd, nsd, resolution);
    break;
  }

  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": bad operand (", argv[1], ")",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclParzenLists
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_parzen
 *    dl_parzens
 *
 * DESCRIPTION
 *   Create adaptive sdfs for data in lists or create a list of sdfs.
 *
 *****************************************************************************/

static int tclParzenLists (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DYN_LIST *dl, *newlist = NULL;
  DYN_LIST *startl, *stopl;
  int resolution;
  double start, stop, nsd, ksd;
  int mode = (Tcl_Size) data;

  if (argc != 7) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist start stop",
		     " pilot_sd nsd resolution",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &ksd) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[5], &nsd) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[6], &resolution) != TCL_OK) return TCL_ERROR;

  switch (mode) {
  case DL_SINGLE:
    if (Tcl_GetDouble(interp, argv[2], &start) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetDouble(interp, argv[3], &stop) != TCL_OK) return TCL_ERROR;
    newlist = dynListParzenList(dl, start, stop, ksd, nsd, resolution);
    break;
  case DL_RECURSIVE:
    if (tclFindDynList(interp, argv[2], &startl) != TCL_OK) return TCL_ERROR;
    if (tclFindDynList(interp, argv[3], &stopl) != TCL_OK) return TCL_ERROR;
    newlist = dynListParzenLists(dl, startl, stopl, ksd, nsd, resolution);
    break;
  }

  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": bad operand (", argv[1], ")",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclReduceList
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_length
 *    dl_depth
 *    dl_min
 *    dl_max
 *    dl_sum
 *    dl_prod
 *    dl_mean
 *    dl_std
 *    dl_var
 *
 * DESCRIPTION
 *    Reduce list by returning scalar value corresponding to min, max, sum,
 * mean, std, etc...
 *
 *****************************************************************************/

static int tclReduceList (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  DYN_LIST *dl, *retlist, *collapsed;
  float retval = 0.0;
  int intval;

  int operation = (Tcl_Size) data;

  /* Only min / max can accept multiple args */
  if (argc > 2) {
    if (operation == DL_MAX || operation == DL_MIN) {
      int i, mathop;
      DYN_LIST *dl1, *dl2, *newlist = NULL, *newlist1;

      if (tclFindDynList(interp, argv[1], &dl1) != TCL_OK) return TCL_ERROR;
      if (tclFindDynList(interp, argv[2], &dl2) != TCL_OK) return TCL_ERROR;
  
      switch (operation) {
      case DL_MIN:       mathop = DL_MATH_MIN;     break;
      case DL_MAX:       mathop = DL_MATH_MAX;     break;
      }

      newlist = dynListArithListList(dl1, dl2, mathop);
      for (i = 3; i < argc; i++) {
	if (!newlist) goto error;
	if (tclFindDynList(interp, argv[i], &dl1) != TCL_OK) {
	  dfuFreeDynList(newlist);
	  return TCL_ERROR;
	}
	newlist1 = dynListArithListList(newlist, dl1, mathop);
	dfuFreeDynList(newlist);
	newlist = newlist1;
      }
      if (!newlist) {
      error:
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, argv[0], ": bad arguments", (char *) NULL);
	return TCL_ERROR;
      }
      else {
	return(tclPutList(interp, newlist));
      }
    }  
  }

  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;

  switch (operation) {
  case DL_LENGTH:
    intval = DYN_LIST_N(dl);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(intval));
    return TCL_OK;
    break;
  case DL_MIN_INDEX:
    if (DYN_LIST_N(dl) == 0) return TCL_OK;
    intval = dynListMinListIndex(dl);
    if (intval < 0) {
      Tcl_AppendResult(interp, argv[0], ": can't find min of list\"",
		       argv[1], "\"", (char *) NULL);
      return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(intval));
    return TCL_OK;
    break;
  case DL_MAX_INDEX:
    if (DYN_LIST_N(dl) == 0) return TCL_OK;
    intval = dynListMaxListIndex(dl);
    if (intval < 0) {
      Tcl_AppendResult(interp, argv[0], ": can't find max of list\"",
		       argv[1], "\"", (char *) NULL);
      return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(intval));
    return TCL_OK;
    break;
  case DL_MIN:
    retlist = dynListMinMaxList(dl, DL_MINIMUM);
    tclDynListToTclList(interp, retlist);    
    dfuFreeDynList(retlist);
    return TCL_OK;
    break;
  case DL_MAX:
    retlist = dynListMinMaxList(dl, DL_MAXIMUM);
    tclDynListToTclList(interp, retlist);    
    dfuFreeDynList(retlist);
    return TCL_OK;
    break;
  case DL_SUM:
    retlist = dynListSumProdList(dl, DL_SUM_LIST);
    tclDynListToTclList(interp, retlist);    
    dfuFreeDynList(retlist);
    return TCL_OK;
    break;
  case DL_PROD:
    retlist = dynListSumProdList(dl, DL_PROD_LIST);
    tclDynListToTclList(interp, retlist);    
    dfuFreeDynList(retlist);
    return TCL_OK;
    break;
  case DL_DEPTH:
    intval = dynListDepth(dl, 0);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(intval));
    return TCL_OK;
    break;
  case DL_MEAN:
    if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
      collapsed = dynListCollapseList(dl);
      if (!collapsed) {
	Tcl_AppendResult(interp, argv[0], ": invalid argument", (char *) NULL);
	return TCL_ERROR;
      }
      retval = dynListMeanList(collapsed);
      dfuFreeDynList(collapsed);
    }
    else {
      retval = dynListMeanList(dl);
    }
    break;
  case DL_STD:
    if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
      collapsed = dynListCollapseList(dl);
      if (!collapsed) {
	Tcl_AppendResult(interp, argv[0], ": invalid argument", (char *) NULL);
	return TCL_ERROR;
      }
      retval = dynListStdList(collapsed);
      dfuFreeDynList(collapsed);
    }
    else {
      retval = dynListStdList(dl);
    }
    break;
  case DL_VAR:
    if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
      collapsed = dynListCollapseList(dl);
      if (!collapsed) {
	Tcl_AppendResult(interp, argv[0], ": invalid argument", (char *) NULL);
	return TCL_ERROR;
      }
      retval = dynListVarList(collapsed);
      dfuFreeDynList(collapsed);
    }
    else {
      retval = dynListVarList(dl);
    }
    break;
  }
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj(retval));
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclReduceLists
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_min
 *    dl_max
 *    dl_sum
 *    dl_prod
 *    dl_mean
 *    dl_std
 *    dl_var
 *
 * DESCRIPTION
 *    Reduce list of lists by returning list of scalar values corresponding to 
 * min, max, sum, mean, std, etc. of each list.
 *
 *****************************************************************************/

static int tclReduceLists (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DYN_LIST *dl, *newlist = NULL;
  int operation = (Tcl_Size) data;

  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;

  switch (operation) {
  case DL_MIN:
    newlist = dynListMinLists(dl);
    break;
  case DL_MAX:
    newlist = dynListMaxLists(dl);
    break;
  case DL_BMIN:
    newlist = dynListBMinMaxList(dl, DL_MINIMUM);
    break;
  case DL_BMAX:
    newlist = dynListBMinMaxList(dl, DL_MAXIMUM);
    break;
  case DL_MIN_INDEX:
    newlist = dynListMinMaxIndices(dl, DL_MINIMUM);
    break;
  case DL_MAX_INDEX:
    newlist = dynListMinMaxIndices(dl, DL_MAXIMUM);
    break;
  case DL_MIN_POSITIONS:
    newlist = dynListMinMaxPositions(dl, DL_MINIMUM);
    break;
  case DL_MAX_POSITIONS:
    newlist = dynListMinMaxPositions(dl, DL_MAXIMUM);
    break;
  case DL_SUM:
    newlist = dynListSumLists(dl);
    break;
  case DL_PROD:
    newlist = dynListProdLists(dl);
    break;
  case DL_MEAN:
    newlist = dynListMeanLists(dl);
    break;
  case DL_HMEAN:
    newlist = dynListHMeanLists(dl);
    break;
  case DL_BMEAN:
    newlist = dynListBMeanLists(dl);
    break;
  case DL_MEAN_LIST:
    newlist = dynListAverageList(dl);
    break;
  case DL_SUMCOLS_LIST:
    newlist = dynListSumColsList(dl);
    break;
  case DL_BSUMS:
    newlist = dynListBSumLists(dl);
    break;
  case DL_STD:
    newlist = dynListStdLists(dl);
    break;
  case DL_BSTD:
    newlist = dynListBStdLists(dl);
    break;
  case DL_HSTD:
    newlist = dynListHStdLists(dl);
    break;
  case DL_VAR:
    newlist = dynListVarLists(dl);
    break;
  case DL_HVAR:
    newlist = dynListHVarLists(dl);
    break;
  }
  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": bad operand (", argv[1], ")",
		     (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclSortByLists
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_sortByList
 *    dl_sortByLists
 *    dl_Cross
 *    dl_uniqueCross
 *
 * DESCRIPTION
 *   Take a list a separate out based on a second, categorical list.  For
 * example, a list of heights could be sorted into two lists of heights using
 * gender as a categorical sort variable.
 *
 *****************************************************************************/

static int tclSortByLists (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  DYN_LIST *dl, *categories, *newlist = NULL;
  int mode = (Tcl_Size) data;
  int startarg = 1;

  switch (mode) {
  case DL_BYLIST:
    if (argc != 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist category",
		       (char *) NULL);
      return TCL_ERROR;
    }

    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
    if (!DYN_LIST_N(dl)) goto emptyList;

    if (tclFindDynList(interp, argv[2], &categories) != TCL_OK) 
      return TCL_ERROR;
    if (!DYN_LIST_N(categories)) goto emptyList;


    newlist = dynListSortListByList(dl, categories);
    break;
  case DL_BYSELECTED:
    {
      char **varArgv, **selArgv;
      Tcl_Size varArgc, selArgc;
      DYN_LIST *varlist, *sellist;

      if (argc != 4) {
	Tcl_AppendResult(interp, "usage: ", argv[0], 
			 " data variables selections", (char *) NULL);
	return TCL_ERROR;
      }
      if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) 
	return TCL_ERROR;
      if (!DYN_LIST_N(dl)) goto emptyList;
      
      if (Tcl_SplitList(interp, argv[2], &varArgc,
			(const char ***) &varArgv) != TCL_OK) {
	return TCL_ERROR;
      }
      if (Tcl_SplitList(interp, argv[3], &selArgc,
			(const char ***) &selArgv) != TCL_OK) {
	Tcl_Free((void *) varArgv);
	return TCL_ERROR;
      }
      if (varArgc != selArgc) {
	Tcl_Free((void *) varArgv);
	Tcl_Free((void *) selArgv);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, argv[0], 
			 ": unequal numbers of categories and selections",
			 (char *) NULL);
	return TCL_ERROR;
      }
      if (tclMakeListList(interp, varArgc, varArgv, &varlist) != TCL_OK) {
	Tcl_Free((void *) varArgv);
	Tcl_Free((void *) selArgv);
	return TCL_ERROR;
      }
      if (tclMakeListList(interp, selArgc, selArgv, &sellist) != TCL_OK) {
	Tcl_Free((void *) varArgv);
	Tcl_Free((void *) selArgv);
	dfuFreeDynList(varlist);
	return TCL_ERROR;
      }
      newlist = dynListSortListByLists(dl, varlist, sellist);
      Tcl_Free((void *) varArgv);
      Tcl_Free((void *) selArgv);
      dfuFreeDynList(varlist);
      dfuFreeDynList(sellist);
    }
    break;
  case DL_BYLISTS:
    if (argc < 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist category(s)",
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
    if (!DYN_LIST_N(dl)) goto emptyList;
    startarg = 2;
    /* FALL THRU */
  case DL_UNIQUE_CROSS:
  case DL_CROSS:
    if (mode != DL_BYLISTS && argc < 2) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " dynlist category(s)",
		       (char *) NULL);
      return TCL_ERROR;
    }
    {
      int i;
      DYN_LIST *slist;
      
      categories = dfuCreateDynList(DF_LIST, argc-startarg);
      
      /* Find lists and check to make sure they are the proper length */
      for (i = startarg; i < argc; i++) {
	if (tclFindDynList(interp, argv[i], &slist) != TCL_OK) 
	  return TCL_ERROR;
	
	if (mode == DL_BYLISTS && (DYN_LIST_N(slist) != DYN_LIST_N(dl))) {
	  Tcl_ResetResult(interp);
	  Tcl_AppendResult(interp, argv[0], ": sortlist \"", argv[i], 
			   "\" is not the same length as value list \"",
			   argv[1], "\"", (char *) NULL);
	  dfuFreeDynList(categories);
	  return TCL_ERROR;
	  
	}
	else {
	  if (!DYN_LIST_N(slist)) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, argv[0], ": sort list is empty",
			     (char *) NULL);
	    dfuFreeDynList(categories);
	    return TCL_ERROR;
	  }
	  dfuAddDynListList(categories, slist);
	}
      }
      
      if (!DYN_LIST_N(categories)) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, argv[0], ": category list is empty",
			 (char *) NULL);
	dfuFreeDynList(categories);
	return TCL_ERROR;
      }

      switch (mode) {
      case DL_CROSS:
	newlist = dynListCrossLists(categories);
	break;
      case DL_UNIQUE_CROSS:
	newlist = dynListUniqueCrossLists(categories);
	break;
      case DL_BYLISTS:
	newlist = dynListSortListByLists(dl, categories, categories);
	break;
      }
      dfuFreeDynList(categories);
    }
    break;
  }
  
  if (!newlist) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, argv[0], ": bad argument(s)", (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }

 emptyList:
  Tcl_ResetResult(interp);
  Tcl_AppendResult(interp, argv[0], ": cannot sort empty list \"", argv[1], 
		   "\"", (char *) NULL);
  return TCL_ERROR;
}




/*****************************************************************************
 *
 * FUNCTION
 *    tclFileName
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_fileroot
 *    dl_filetail
 *    dl_filedir
 *
 * DESCRIPTION
 *    Return a list of processed filenames
 *
 *****************************************************************************/


static int file_string_func(Tcl_Interp *interp, char *name, int op,
			      char *buf, int buflen)
{
  Tcl_Size argc;
  char *dot;
  char **argv;

  buf[0] = 0;			/* Empty string */

  if (!name || !name[0]) return 0;

  Tcl_SplitPath(name, &argc, (const char ***) &argv);

  switch(op) {
  case DL_FILE_ROOT:
    {
      Tcl_DString resultPtr;
      if ((dot = strrchr(argv[argc-1], '.')))
	*dot = 0;
      Tcl_DStringInit(&resultPtr);
      Tcl_JoinPath(argc, (const char * const*) argv, &resultPtr);
      strncpy(buf, Tcl_DStringValue(&resultPtr), buflen);
      Tcl_DStringFree(&resultPtr);
      break;
    }
  case DL_FILE_DIR:
    {
      Tcl_DString resultPtr;
      Tcl_DStringInit(&resultPtr);
      if (argc > 1) argc--;
      Tcl_JoinPath(argc, (const char * const*) argv, &resultPtr);
      strncpy(buf, Tcl_DStringValue(&resultPtr), buflen);
      Tcl_DStringFree(&resultPtr);
      break;
    }
  case DL_FILE_TAIL:
    strncpy(buf, argv[argc-1], buflen);
    break;
  case DL_FILE_EXT:
    if ((dot = strrchr(argv[argc-1], '.')))
      strncpy(buf, dot+1, buflen);
    break;
  case DL_FILE_BASE:
    if ((dot = strrchr(argv[argc-1], '.'))) {
      *dot = 0;
    }
    strncpy(buf, argv[argc-1], buflen);
    break;
  }
  Tcl_Free((char *) argv);

  return 1;
}

static DYN_LIST *dynListFileString(Tcl_Interp *interp, DYN_LIST *dl, int op)
{
  int i;
  DYN_LIST *newlist = NULL;
  if (!dl) return NULL;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_STRING:
    {
      char buf[256];
      char **vals = (char **) DYN_LIST_VALS(dl);
      newlist = dfuCreateDynList(DF_STRING, DYN_LIST_N(dl));
      if (!newlist) return(NULL);
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	file_string_func(interp, vals[i], op, buf, sizeof(buf));
	dfuAddDynListString(newlist, buf);
      }
      break;
    }
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curlist;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListFileString(interp, vals[i], op);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    break;
  }
  return(newlist);
}


static int tclRegexpList(ClientData data, Tcl_Interp * interp, int objc,
			 Tcl_Obj * const objv[])
{
  char *dlname;
  DYN_LIST *dl;
  Tcl_RegExp regExpr;
  DYN_LIST *newlist;
  int i;
  int cflags = TCL_REG_ADVANCED;
  int matchonly = 0;

  static const char *const options[] = {
    "-expanded",	"-line",	"-linestop",	"-lineanchor",
    "-nocase",	"--",		NULL
  };
  enum options {
    REGEXP_EXPANDED,REGEXP_LINE,	REGEXP_LINESTOP,REGEXP_LINEANCHOR,
    REGEXP_NOCASE,	REGEXP_LAST
  };

  /*
    cflags info (from Tcl CAPI man page)

    TCL_REG_ADVANCED
    Compile advanced regular expressions ('ARE's). 
    This mode corresponds to the normal regular expression syntax 
    accepted by the Tcl regexp and regsub commands.

    TCL_REG_EXTENDED
    Compile extended regular expressions ('ERE's). This mode 
    corresponds to the regular expression syntax recognized by Tcl 8.0 
    and earlier versions.

    TCL_REG_BASIC
    Compile basic regular expressions ('BRE's). 
    This mode corresponds to the regular expression syntax 
    recognized by common Unix utilities like sed and grep. 
    This is the default if no flags are specified.

    TCL_REG_EXPANDED
    Compile the regular expression (basic, extended, or advanced) 
    using an expanded syntax that allows comments and whitespace. 
    This mode causes non-backslashed non-bracket-expression 
    white space and #-to-end-of-line comments to be ignored.

    TCL_REG_QUOTE
    Compile a literal string, with all characters 
    treated as ordinary characters.

    TCL_REG_NOCASE
    Compile for matching that ignores upper/lower case distinctions.

    TCL_REG_NEWLINE
    Compile for newline-sensitive matching. By default, 
    newline is a completely ordinary character with no special 
    meaning in either regular expressions or strings. With this flag, 
    '[^' bracket expressions and '.' never match newline, 
    '^' matches an empty string after any newline in addition 
    to its normal function, and '$' matches an empty string 
    before any newline in addition to its normal function. 
    REG_NEWLINE is the bit-wise OR of REG_NLSTOP and REG_NLANCH.

    TCL_REG_NLSTOP
    Compile for partial newline-sensitive matching, with the 
    behavior of '[^' bracket expressions and '.' affected, but 
    not the behavior of '^' and '$'. In this mode, '[^' bracket 
    expressions and '.' never match newline.

    TCL_REG_NLANCH
    Compile for inverse partial newline-sensitive matching, 
    with the behavior of '^' and '$' (the 'anchors') affected, 
    but not the behavior of '[^' bracket expressions and '.'. 
    In this mode '^' matches an empty string after any newline 
    in addition to its normal function, and '$' matches an empty 
    string before any newline in addition to its normal function.

    TCL_REG_NOSUB
    Compile for matching that reports only success or failure, 
    not what was matched. This reduces compile overhead and may 
    improve performance. Subsequent calls to Tcl_RegExpGetInfo or 
    Tcl_RegExpRange will not report any match information.
    
    TCL_REG_CANMATCH
    Compile for matching that reports the potential to complete 
    a partial match given more text (see below).
    
    Only one of TCL_REG_EXTENDED, TCL_REG_ADVANCED, TCL_REG_BASIC, 
    and TCL_REG_QUOTE may be specified.
  */


  if ((Tcl_Size) data == DL_REGMATCH) matchonly = 1;

  for (i = 1; i < objc; i++) {
    const char *name;
    int index;
    
    name = Tcl_GetStringFromObj(objv[i], NULL);
    if (name[0] != '-') {
      break;
    }
    if (Tcl_GetIndexFromObj(interp, objv[i], 
			    options, "option", TCL_EXACT,
			    &index) != TCL_OK) {
      return TCL_ERROR;
    }
    switch ((enum options) index) {
    case REGEXP_NOCASE:
      cflags |= TCL_REG_NOCASE;
      break;
    case REGEXP_EXPANDED:
      cflags |= TCL_REG_EXPANDED;
      break;
    case REGEXP_LINE:
      cflags |= TCL_REG_NEWLINE;
      break;
    case REGEXP_LINESTOP:
      cflags |= TCL_REG_NLSTOP;
      break;
    case REGEXP_LINEANCHOR:
      cflags |= TCL_REG_NLANCH;
      break;
    default:
      break;
    }
  }

  objc -= i;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "?-option(s)? dl regexp");
    return TCL_ERROR;
  }

  objv += i;
  
  dlname = Tcl_GetStringFromObj(objv[0], NULL);
  if (tclFindDynList(interp, dlname, &dl) != TCL_OK) return TCL_ERROR;

  regExpr = Tcl_GetRegExpFromObj(interp, objv[1], cflags);
  if (!regExpr) return TCL_ERROR;

  newlist = dynListRegexpString(interp, dl, regExpr, matchonly);

  if (!newlist) {
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}


static DYN_LIST *dynListRegexpString(Tcl_Interp *interp, DYN_LIST *dl, 
				     Tcl_RegExp regexp, int matchonly)
{
  int i, j;
  DYN_LIST *newlist = NULL, *matchlist;
  Tcl_Obj *textObj, *matchObj;
  int offset = 0;
  int nmatches = -1;
  int eflags = 0;		/* control Beginning of Line / EOL */
  Tcl_RegExpInfo regexp_info;
  Tcl_RegExpIndices *matchinds;

  if (!dl) return NULL;

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_STRING:
    {
      char **vals = (char **) DYN_LIST_VALS(dl);
      int matches;
      textObj = Tcl_NewStringObj("",-1);

      if (!matchonly) {
	newlist = dfuCreateDynList(DF_LIST, DYN_LIST_N(dl));
	if (!newlist) return(NULL);

	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  Tcl_SetStringObj(textObj, vals[i], -1);
	  matches = Tcl_RegExpExecObj(interp, regexp, textObj, 
				      offset, nmatches, eflags);
	  if (matches < 0) {
	    dfuFreeDynList(newlist);
	    return NULL;
	  }
	  
	  matchlist = dfuCreateDynList(DF_STRING, DYN_LIST_N(dl));
	  
	  if (matches == 1) {
	    Tcl_RegExpGetInfo(regexp, &regexp_info);
	    matchinds = regexp_info.matches;
	    for (j = 0; j <= regexp_info.nsubs; j++) {
	      matchObj = Tcl_GetRange(textObj, matchinds[j].start, 
				      matchinds[j].end-1);
	      dfuAddDynListString(matchlist, Tcl_GetString(matchObj));
	      Tcl_DecrRefCount(matchObj);
	    }
	  }
	  dfuMoveDynListList(newlist, matchlist);
	}
      }

      else {			/* match only - return 1 or 0 */
	newlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
	if (!newlist) return(NULL);

	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  Tcl_SetStringObj(textObj, vals[i], -1);
	  matches = Tcl_RegExpExecObj(interp, regexp, textObj, 
				      offset, nmatches, eflags);
	  if (matches < 0) {
	    dfuFreeDynList(newlist);
	    return NULL;
	  }
	  else dfuAddDynListLong(newlist, matches);
	}
      }
      Tcl_DecrRefCount(textObj);
      break;
    }
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curlist;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListRegexpString(interp, vals[i], regexp, matchonly);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    break;
  }
  return(newlist);
}




static int tclScanList(ClientData data, Tcl_Interp * interp, int objc,
		       Tcl_Obj * const objv[])
{
  char *dlname, *procname;
  DYN_LIST *dl;
  DYN_LIST *newlist;
  int i;
  int base = 0;
  int scan_int = 1;
  
  static const char *const options[] = {
    "-d", "-o", "-x", "-b", "-f",NULL
  };
  enum options {
    SCAN_DECIMAL, SCAN_OCTAL, SCAN_HEXADECIMAL, SCAN_BINARY, SCAN_FLOAT,
  };

  procname = Tcl_GetString(objv[0]); /* for errors */
  switch((Tcl_Size) data) {
   case DL_SCAN_INT: base = 10; scan_int = 1; break;
   case DL_SCAN_FLOAT: base = 1; scan_int = 0; break;
   case DL_SCAN_OCTAL: base = 8; scan_int = 1; break;
   case DL_SCAN_BINARY: base = 2; scan_int = 1; break;
   case DL_SCAN_HEX: base = 16; scan_int = 1; break;
  }

  
  for (i = 1; i < objc; i++) {
    const char *name;
    int index;
    
    name = Tcl_GetStringFromObj(objv[i], NULL);
    if (name[0] != '-') {
      break;
    }
    if (Tcl_GetIndexFromObj(interp, objv[i], options, "option", TCL_EXACT,
			    &index) != TCL_OK) {
      return TCL_ERROR;
    }
    switch ((enum options) index) {
    case SCAN_DECIMAL:
      base = 10;
      scan_int = 1;
      break;
    case SCAN_OCTAL:
      base = 8;
      scan_int = 1;
      break;
    case SCAN_HEXADECIMAL:
      base = 16;
      scan_int = 1;
      break;
    case SCAN_BINARY:
      base = 2;
      scan_int = 1;
      break;
    case SCAN_FLOAT:
      base = 1;
      scan_int = 0;
      break;
    }
  }

  objc -= i;

  if (objc < 1) {
    Tcl_WrongNumArgs(interp, 1, objv, "?-option(s)? dl");
    return TCL_ERROR;
  }

  objv += i;
  
  dlname = Tcl_GetStringFromObj(objv[0], NULL);
  if (tclFindDynList(interp, dlname, &dl) != TCL_OK) return TCL_ERROR;
  
  newlist = dynListScanString(interp, dl, base, scan_int);

  if (!newlist) {
    Tcl_AppendResult(interp, procname, 
		     ": error scanning strings", (char *) NULL);
    return TCL_ERROR;
  }
  else {
    return(tclPutList(interp, newlist));
  }
}


static DYN_LIST *dynListScanString(Tcl_Interp *interp, DYN_LIST *dl, 
				   int base, int scan_int)
{
  int i;
  DYN_LIST *newlist = NULL;
  char *format = "%d";
  int intVal;
  float floatVal;
  int result;
  
  if (!dl) return NULL;
  
  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_STRING:
    {
      char **vals = (char **) DYN_LIST_VALS(dl);

      if (scan_int == 1) {
	newlist = dfuCreateDynList(DF_LONG, DYN_LIST_N(dl));
      }
      else {
	newlist = dfuCreateDynList(DF_FLOAT, DYN_LIST_N(dl));
      }
      if (!newlist) return(NULL);
      
      if (scan_int == 0) {
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  format = "%f";
	  result = sscanf(vals[i], format, &floatVal);
	  if (!result) {
	  result_error:
	    dfuFreeDynList(newlist);
	    return NULL;
	  }
	  dfuAddDynListFloat(newlist, floatVal);
	}
      }
      else if (base == 2) {
	char *endptr;
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  intVal = strtol(vals[i], &endptr, 2);
	  if (*endptr != '\0') goto result_error;
	  dfuAddDynListLong(newlist, intVal);
	}
      }
      else {
	if (base == 0 || base == 10) format = "%d";
	else if (base == 8) format = "%o";
	else if (base == 16) format = "%x";
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  result = sscanf(vals[i], format, &intVal);
	  if (result != 1) goto result_error;
	  dfuAddDynListLong(newlist, intVal);
	}
      }
      break;
    }
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      DYN_LIST *curlist;
      newlist = dfuCreateDynList(DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
      for (i = 0; i < DYN_LIST_N(dl); i++) {
	curlist = dynListScanString(interp, vals[i], base, scan_int);
	if (!curlist) {
	  dfuFreeDynList(newlist);
	  return NULL;
	}
	dfuMoveDynListList(newlist, curlist);
      }
    }
    break;
  }
  
  return(newlist);
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclTempName
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dl_tempname
 *    dl_tmpname
 *
 * DESCRIPTION
 *    Return a list of one or more names that can be used for temporary files
 *
 * NOTE
 *    Under Linux this function is not recommended - we return and error on this
 *  platform and recommend using "file tempfile"
 *
 *****************************************************************************/

static int tclTempName (ClientData data, Tcl_Interp *interp,
			int argc, char *argv[])
{
  int i, j;
  static char *dir = "/usr/tmp";
  static char *prf = "dl:";
  DYN_LIST *prelist = NULL;
  
  /* Command parsing loop, looking for option pairs */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i],"-prefix")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no prefix specified", (char *) NULL);
	  return TCL_ERROR;
	}
	prf = argv[i+1];
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      if (!strcmp(argv[i],"-dir")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no directory specified", (char *) NULL);
	  return TCL_ERROR;
	}
	dir = argv[i+1];
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
    }
  }

  if (!prelist) {
#ifndef QNX
	  Tcl_SetResult(interp, tempnam(dir, prf), TCL_DYNAMIC);
#else
	  Tcl_SetResult(interp, tmpnam(NULL), TCL_VOLATILE);
#endif	  
  }
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclGetNumericRange
 *
 * ARGS
 *    Tcl_Interp *interp, start, stop
 *
 * DESCRIPTION
 *    Creates a dynlist with two elements containing start / stop
 *
 *****************************************************************************/

int tclGetNumericRange(Tcl_Interp *interp, char *startstr, char *stopstr, 
		       DYN_LIST **rangep)
{
  DYN_LIST *range = NULL;
  DYN_LIST *startlist = NULL, *stoplist = NULL;

  double fstart, fstop;
  int start, stop;
  
  if (Tcl_GetDouble(interp, startstr, &fstart) == TCL_OK &&
      Tcl_GetDouble(interp, stopstr, &fstop) == TCL_OK) {
    range = dfuCreateDynList(DF_FLOAT, 2);
    dfuAddDynListFloat(range, fstart);
    dfuAddDynListFloat(range, fstop);
  }
  else if (Tcl_GetInt(interp, startstr, &start) == TCL_OK &&
	   Tcl_GetInt(interp, stopstr, &stop) == TCL_OK) {
    range = dfuCreateDynList(DF_LONG, 2);
    dfuAddDynListLong(range, start);
    dfuAddDynListLong(range, stop);
  }

  else if (tclFindDynList(interp, startstr, &startlist) == TCL_OK &&
	   tclFindDynList(interp, stopstr, &stoplist) == TCL_OK) {
    int i;
    DYN_LIST *rlist;
    float *fvals0, *fvals1;
    int *ivals0, *ivals1;
    
    Tcl_ResetResult(interp);
    if (DYN_LIST_N(startlist) != DYN_LIST_N(stoplist)) return TCL_ERROR;
    switch (DYN_LIST_DATATYPE(startlist)) {
    case DF_FLOAT:
      fvals0 = (float *) DYN_LIST_VALS(startlist);
      switch (DYN_LIST_DATATYPE(stoplist)) {
      case DF_FLOAT:
	fvals1 = (float *) DYN_LIST_VALS(stoplist);
	range = dfuCreateDynList(DF_LIST, DYN_LIST_N(startlist));
	for (i = 0; i < DYN_LIST_N(startlist); i++) {
	  rlist = dfuCreateDynList(DF_FLOAT, 2);
	  dfuAddDynListFloat(rlist, fvals0[i]);
	  dfuAddDynListFloat(rlist, fvals1[i]);
	  dfuMoveDynListList(range, rlist);
	}
	break;
      case DF_LONG:
	ivals1 = (int *) DYN_LIST_VALS(stoplist);
	range = dfuCreateDynList(DF_LIST, DYN_LIST_N(startlist));
	for (i = 0; i < DYN_LIST_N(startlist); i++) {
	  rlist = dfuCreateDynList(DF_FLOAT, 2);
	  dfuAddDynListFloat(rlist, fvals0[i]);
	  dfuAddDynListLong(rlist, ivals1[i]);
	  dfuMoveDynListList(range, rlist);
	}
	break;
      default:
	return TCL_ERROR;
	break;
      }
      break;
    case DF_LONG:
      ivals0 = (int *) DYN_LIST_VALS(startlist);
      switch (DYN_LIST_DATATYPE(stoplist)) {
      case DF_FLOAT:
	fvals1 = (float *) DYN_LIST_VALS(stoplist);
	range = dfuCreateDynList(DF_LIST, DYN_LIST_N(startlist));
	for (i = 0; i < DYN_LIST_N(startlist); i++) {
	  rlist = dfuCreateDynList(DF_FLOAT, 2);
	  dfuAddDynListLong(rlist, ivals0[i]);
	  dfuAddDynListFloat(rlist, fvals1[i]);
	  dfuMoveDynListList(range, rlist);
	}
	break;
      case DF_LONG:
	ivals1 = (int *) DYN_LIST_VALS(stoplist);
	range = dfuCreateDynList(DF_LIST, DYN_LIST_N(startlist));
	for (i = 0; i < DYN_LIST_N(startlist); i++) {
	  rlist = dfuCreateDynList(DF_LONG, 2);
	  dfuAddDynListLong(rlist, ivals0[i]);
	  dfuAddDynListLong(rlist, ivals1[i]);
	  dfuMoveDynListList(range, rlist);
	}
	break;
      default:
	return TCL_ERROR;
	break;
      }
      break;
    default:
      return TCL_ERROR;
    }
  }
  
  if (range) {
    if (rangep) *rangep = range;
    return TCL_OK;
  }
  Tcl_ResetResult(interp);
  return TCL_ERROR;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclMakeListList
 *
 * ARGS
 *    Tcl_Interp *interp, int n, char **names, DYN_LIST **
 *
 * DESCRIPTION
 *    Creates a list of n lists using lists named names
 *
 *****************************************************************************/

int tclMakeListList(Tcl_Interp *interp, int n, char **names, DYN_LIST **dlp)
{
  DYN_LIST *dll = dfuCreateDynList(DF_LIST, n);
  DYN_LIST *dl;
  int i;
  
  for (i = 0; i < n; i++) {
    if (tclFindDynList(interp, names[i], &dl) != TCL_OK) {
      dfuFreeDynList(dll);
      if (dlp) *dlp = NULL;
      return TCL_ERROR;
    }
    dfuAddDynListList(dll, dl);
  }
  
  *dlp = dll;
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclFindSubDynList
 *
 * ARGS
 *    Tcl_Interp *interp, DYN_LIST *list, char *name
 *
 * DESCRIPTION
 *    Looks for a subscripted dynlist in list.
 *
 *****************************************************************************/

int tclFindSubDynList(Tcl_Interp *interp, char *dlname, DYN_LIST *dll, 
		      char *sel, DYN_LIST **dl)
{
  int listnum, start, stop, last;
  DYN_LIST **vals;
  char *colon;

  colon = strchr(sel,':');
  if (colon && DYN_LIST_DATATYPE(dll) != DF_LIST) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "dl_find: dynlist \"", dlname, 
		     "\" is not a list of lists", (char *) NULL);
    return TCL_ERROR;
  }

  /* Peel off multiple selector id's and call recursively */
  if (colon) {
    DYN_LIST *interdl = NULL;
    
    /* turned these from statics to automatics 7/18 to address memory */
    /* overlap when accessing deep lists */
    char sel0[16];
    char newdlname[64];

    /* Reject because a number this long can't be an index */
    if (colon-sel > 15) return TCL_ERROR;
    strncpy(sel0, sel, colon-sel);
    sel0[colon-sel] = 0;

    if (tclFindSubDynList(interp, dlname, dll, sel0, &interdl) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "dl_find: \"", sel0, 
		       "\" is an invalid list selector for list ", 
		       dlname, (char *) NULL);
      return TCL_ERROR;
    }

    strcpy(newdlname, dlname);
    strcat(newdlname, ":");
    strcat(newdlname, sel0);

    return(tclFindSubDynList(interp, newdlname, interdl, colon+1, dl));
  }

  /* The default subscripting mode... */
#if 0
  if (Tcl_ExprLong(interp, sel, &listnum) != TCL_OK) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "dl_find: \"", sel, 
		     "\" is an invalid list selector", (char *) NULL);
    return TCL_ERROR;
  }
  if (listnum >= 0 && listnum < DYN_LIST_N(dll)) {
    switch(DYN_LIST_DATATYPE(dll)) {
    case DF_LIST:
      vals = (DYN_LIST **) DYN_LIST_VALS(dll);
      if (vals[listnum]) {
	if (dl) {
	  *dl = vals[listnum];
	  DYN_LIST_FLAGS(*dl) |= DL_SUBLIST;
	}
	return TCL_OK;
      }
    default:
      {
	DYN_LIST *newdl;
	newdl = dfuCreateDynList(DYN_LIST_DATATYPE(dll), 1);
	dynListCopyElement(dll, listnum, newdl);
	tclPutList(interp, newdl);
	Tcl_ResetResult(interp);
	*dl = newdl;
	return TCL_OK;
      }
    }
  }
#else
  /*
   * Here we insert fancier subscripting.
   */

  /* <> -> do a dl_choose */
  last = strlen(sel)-1;

  /* Only need to test if there are more than digits in selector */
  if (last && sel[0] == '<' && sel[last] == '>') {
    char lname[64];
    DYN_LIST *seldl, *newdl;
    strncpy(lname, &sel[1], last-1);
    lname[last-1] = 0;
    if (tclFindDynList(interp, lname, &seldl) != TCL_OK) return TCL_ERROR;
    newdl = dynListChoose(dll, seldl);
    if (newdl) {
      tclPutList(interp, newdl);
      Tcl_ResetResult(interp);
      *dl = newdl;
      return TCL_OK;
    }
    else {
      Tcl_AppendResult(interp, "invalid choices: ", lname, NULL);
      return TCL_ERROR;
    }
  }
  
    /* () -> do a dl_select */
  else if (last && sel[0] == '(' && sel[last] == ')') {
    char lname[64];
    DYN_LIST *seldl, *newdl;
    strncpy(lname, &sel[1], last-1);
    lname[last-1] = 0;
    if (tclFindDynList(interp, lname, &seldl) != TCL_OK) {
      int relation, off;
      DYN_LIST *complist;
      /* Here we do some interesting parsing */
      switch (lname[0]) {
      case '>': 
	if (lname[1] == '=') { relation = DL_RELATION_GTE; off = 2; }
	else { relation = DL_RELATION_GT; off = 1; }
	break;
      case '<': 
	if (lname[1] == '=') { relation = DL_RELATION_LTE; off = 2; }
	else { relation = DL_RELATION_LT; off = 1; }
	break;
      case '!':
	if (lname[1] == '=') { relation = DL_RELATION_NE; off = 2; }
	else { relation = DL_RELATION_NE; off = 1; }
	break;
      case '=':
	if (lname[1] == '=') { relation = DL_RELATION_EQ; off = 2; }
	else { relation = DL_RELATION_EQ; off = 1; }
	break;
      default:
	Tcl_AppendResult(interp, "invalid selection list:", lname, NULL);
	return TCL_ERROR;
	break;
      }
      if (tclFindDynList(interp, &lname[1], &complist)) return TCL_ERROR;
      seldl = dynListRelationListList(dll, complist, relation);
      newdl = dynListSelect(dll, seldl);
      dfuFreeDynList(seldl);
    }
    else newdl = dynListSelect(dll, seldl);
    if (newdl) {
      tclPutList(interp, newdl);
      Tcl_ResetResult(interp);
      *dl = newdl;
      return TCL_OK;
    }
    else {
      Tcl_AppendResult(interp, "invalid selectors: ", lname, NULL);
      return TCL_ERROR;
    }
  }
  
  /* Allow single integer or range specification */
  if (!strchr(sel,'-')) {
    if (Tcl_GetInt(interp, sel, &start) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "dl_find: \"", sel, 
		       "\" is an invalid list selector", (char *) NULL);
      return TCL_ERROR;
    }
    else stop = start;
  }
  else if (sscanf(sel, "%d-%d", &start, &stop) != 2) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "dl_find: \"", sel, 
		     "\" is an invalid range specified", (char *) NULL);
    return TCL_ERROR;
  }

  if (start >= 0 && start < DYN_LIST_N(dll) &&
      stop >= start && stop < DYN_LIST_N(dll)) {
    switch(DYN_LIST_DATATYPE(dll)) {
    case DF_LIST:		/* special if only one sublist is desired */
      if (stop == start) {
	listnum = start;
	vals = (DYN_LIST **) DYN_LIST_VALS(dll);
	if (vals[listnum]) {
	  if (dl) {
	    *dl = vals[listnum];
	    DYN_LIST_FLAGS(*dl) |= DL_SUBLIST;
	  }
	  return TCL_OK;
	}
      }
    default:
      {
	DYN_LIST *newdl;
	int i;

	if (!dl) {
	  Tcl_ResetResult(interp);
	  return TCL_OK;
	}

	newdl = dfuCreateDynList(DYN_LIST_DATATYPE(dll), stop-start+1);
	for (i = start; i <= stop; i++) {
	  dynListCopyElement(dll, i, newdl);
	}
	tclPutList(interp, newdl);
	Tcl_ResetResult(interp);
	*dl = newdl;
	return TCL_OK;
      }
    }
  }
#endif

  Tcl_ResetResult(interp);
  Tcl_AppendResult(interp, "dl_find: \"", sel, 
		   "\" out of range for list \"", dlname,
		   "\"", (char *) NULL);
  return TCL_ERROR;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclFindDynList
 *
 * ARGS
 *    Tcl_Interp *interp, char *name, DYN_LIST **
 *
 * DESCRIPTION
 *    Looks for dynlist called name and takes into account group:listname
 * notation.  If dl is not null, it is assigned the pointer to the list.
 *    Also, if the a:b notation is used and a is not a group, a is
 * checked to see if it's a list of lists and if b is a number.  If so, and
 * if b is less than the number of lists in a, dl is set to a[b].
 *
 *****************************************************************************/

int tclFindDynList(Tcl_Interp *interp, char *name, DYN_LIST **dl)
{
  Tcl_HashEntry *entryPtr;
  char *colon;
  DYN_LIST *list;
  char outname[64];

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if (dl) *dl = NULL;		/* initialize so that upon failure
				 * it's set to NULL 
				 */

  if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, name))) {
    list = Tcl_GetHashValue(entryPtr);
    if (!list) {
      Tcl_SetResult(interp, "bad dynlist ptr in hash table", TCL_STATIC);
      return TCL_ERROR;
    }
    if (dl) {
      *dl = list;
      DYN_LIST_FLAGS(*dl) &= ~DL_SUBLIST; /* Flag as top-level list */
    }
    
    /*
     * If a dl_return'd dynlist is looked up, then set a trace to delete
     * it when the current procedure exits (as long as it doesn't already
     * have a trace)
     */
     /*
      * This is actually a very *bad* way to track, because the variable
      * can be deleted too soon.  Note that this code has now been experimentally
      * moved to the returnDynList function, which traces at time of creation
      * instead of waiting for first access
        DLS, 5/11
     */
#if TRACE_RETURNS_AT_FIND
    if (name[0] == '>' && !(Tcl_GetVar(interp, name, 0))) {
      if (!Tcl_VarTraceInfo(interp, name, 0,
			    (Tcl_VarTraceProc *) tclDeleteLocalDynList, 
			    NULL)) {
	Tcl_SetVar(interp, name, name, 0);
	Tcl_TraceVar(interp, name, TCL_TRACE_WRITES | TCL_TRACE_UNSETS, 
		     (Tcl_VarTraceProc *) tclDeleteLocalDynList, 
		     (ClientData) strdup(name));
      }
    }
#endif
    return TCL_OK;
  }

  /*
   * See if the list is specified as a tcllist (e.g. { 1 2 3 } )
   */
  colon = strchr(name, ':');
  if (!colon && (list = tclTclListToDynList(interp, name))) {
    if (dl) {
      *dl = list;
      DYN_LIST_FLAGS(*dl) &= ~DL_SUBLIST; /* Flag as top-level list */
    }
    return TCL_OK;
  }
  /*
   * Check for and process the group:listname notation
   */
  else if (colon) {
    int len;
    DYN_GROUP *dg;
    char groupname[128], listname[128];
    
    /* Get the group */

    len = colon-name;
    if (len > 127) len = 127;	/* arbitrary max */

    strncpy(groupname, name, len);
    groupname[colon-name] = 0;
    
    if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, groupname))) {
      if (!(dg = Tcl_GetHashValue(entryPtr))) {
	strncpy(outname, groupname, 63);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "dl_find: invalid dgptr ", outname, 
			 " in table", (char *) NULL);
	return TCL_ERROR;
      }
    }
    
    /* This searches for a *list* named groupname that is a list of lists */
    else if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, groupname))) {
      DYN_LIST *dll;

      if (!(dll = Tcl_GetHashValue(entryPtr))) {
	strncpy(outname, groupname, 63);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "dl_find: invalid dlptr ", outname, 
			 " in table", (char *) NULL);
	return TCL_ERROR;
      }
      strncpy(listname, colon+1, 63);
      return(tclFindSubDynList(interp, groupname, dll, listname, dl));
    }
    else {
      Tcl_ResetResult(interp);
      strncpy(outname, groupname, 63);
      Tcl_AppendResult(interp, "dl_find: dyngroup \"", outname, 
		       "\" not found", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    
    /* And see if the list exists inside the group */

    /* Could make this a loop, to allow depths greater than one */
    strncpy(listname, colon+1, 63);
    if ((colon = strchr(listname,':'))) {
      char lname[64], sel[32];
      
      /* Get the actual listname */
      if (colon-listname > 63) printf("Aha! (2)\n");
      strncpy(lname, listname, colon-listname);
      lname[colon-listname] = 0;
      if (!(list = dynGroupFindList(dg, lname))) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "dl_find: dynlist \"", lname, 
			 "\" not found in dyngroup \"", groupname, "\"",
			 (char *) NULL);
	return TCL_ERROR;
      }
      strncpy(sel, colon+1, 31);
      return (tclFindSubDynList(interp, lname, list, sel, dl));
    }

    if ((list = dynGroupFindList(dg, listname))) {
      if (dl) {
	*dl = list;
	DYN_LIST_FLAGS(*dl) |= DL_SUBLIST;
      }
      return TCL_OK;
    }
    else {
      char outname_list[64];

      Tcl_ResetResult(interp);
      strncpy(outname, groupname, 63);
      strncpy(outname_list, listname, 63);

      Tcl_AppendResult(interp, "dl_find: dynlist \"", outname_list, 
		       "\" not found in dyngroup \"", outname, "\"",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  else {
    Tcl_ResetResult(interp);
    strncpy(outname, name, 63);
    Tcl_AppendResult(interp, "dynlist \"", outname, "\" not found", 
		     (char *) NULL);
    return TCL_ERROR;
  }
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclFindSubDynListParent
 *
 * ARGS
 *    Tcl_Interp *interp, DYN_LIST *list, char *name
 *
 * DESCRIPTION
 *    Looks for a subscripted dynlist in list and 
 *    sets parent/index if non-null
 *
 *****************************************************************************/

static int tclFindSubDynListParent(Tcl_Interp *interp, char *dlname, 
				   DYN_LIST *dll, 
				   char *sel, DYN_LIST **dl, 
				   DYN_LIST ***parent, int *index)
{
  long lnum;
  int listnum;

  DYN_LIST **vals;
  char *colon;

  colon = strchr(sel,':');
  if (DYN_LIST_DATATYPE(dll) != DF_LIST) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "dl_find: dynlist \"", dlname, 
		     "\" is not a list of lists", (char *) NULL);
    return TCL_ERROR;
  }


  /* Peel off multiple selector id's and call recursively */
  if (colon) {
    DYN_LIST *interdl;
    char sel0[16];
    char newdlname[64];

    /* Reject because a number this long can't be an index */
    if (colon-sel > 15) return TCL_ERROR;
    strncpy(sel0, sel, colon-sel);
    sel0[colon-sel] = 0;

    if (tclFindSubDynListParent(interp, dlname, dll, sel0, &interdl, 
				parent, index) != TCL_OK) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "dl_find: \"", sel0, 
		       "\" is an invalid list selector for list ", 
		       dlname, (char *) NULL);
      return TCL_ERROR;
    }

    strcpy(newdlname, dlname);
    strcat(newdlname, ":");
    strcat(newdlname, sel0);

    return(tclFindSubDynListParent(interp, newdlname, interdl, colon+1, dl, 
				   parent, index));
  }

  if (Tcl_ExprLong(interp, sel, &lnum) != TCL_OK) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "dl_find: \"", sel, 
		     "\" is an invalid list selector", (char *) NULL);
    return TCL_ERROR;
  }

  listnum = (int) lnum;

  if (listnum >= 0 && listnum < DYN_LIST_N(dll)) {
    vals = (DYN_LIST **) DYN_LIST_VALS(dll);
    if (vals[listnum]) {
      if (dl) {
	*dl = vals[listnum];
	DYN_LIST_FLAGS(*dl) |= DL_SUBLIST;
      }

      if (parent) *parent = vals;
      if (index) *index = listnum;

      return TCL_OK;
    }
  }
  Tcl_ResetResult(interp);
  Tcl_AppendResult(interp, "dl_find: \"", sel, 
		   "\" out of range for list \"", dlname,
		   "\"", (char *) NULL);
  return TCL_ERROR;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclFindDynListParent
 *
 * ARGS
 *    Tcl_Interp *interp, char *name, DYN_LIST **
 *
 * DESCRIPTION
 *    Looks for dynlist called name and takes into account group:listname
 * notation.  If dl is not null, it is assigned the pointer to the list.
 *    Also, if the a:b notation is used and a is not a group, a is checked to
 * see if it's a list of lists and if b is a number.  If so, and if b is less
 * than the number of lists in a, dl is set to a[b].
 *
 *****************************************************************************/

static int tclFindDynListParent(Tcl_Interp *interp, char *name, DYN_LIST **dl,
				DYN_LIST ***parent, int *index)
{
  Tcl_HashEntry *entryPtr;
  char *colon;
  DYN_LIST *list;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
    
  if (dl) *dl = NULL;		/* initialize so that upon failure
				 * it's set to NULL 
				 */

  if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, name))) {
    list = Tcl_GetHashValue(entryPtr);
    if (!list) {
      Tcl_SetResult(interp, "bad dynlist ptr in hash table", TCL_STATIC);
      return TCL_ERROR;
    }
    if (dl) {
      *dl = list;
      DYN_LIST_FLAGS(*dl) &= ~DL_SUBLIST; /* Flag as top-level list */
    }
    return TCL_OK;
  }

  /*
   * See if the list is specified as a tcllist (e.g. { 1 2 3 } )
   */
  else if ((list = tclTclListToDynList(interp, name))) {
    if (dl) {
      *dl = list;
      DYN_LIST_FLAGS(*dl) &= ~DL_SUBLIST; /* Flag as top-level list */
    }
    return TCL_OK;
  }
  /*
   * Check for and process the group:listname notation
   */
  else if ((colon = strchr(name,':'))) {
    DYN_GROUP *dg;
    char groupname[64], listname[64];
    
    /* Get the group */

    if (colon-name > 63) printf("Aha! (1)\n");
    strncpy(groupname, name, colon-name);
    groupname[colon-name] = 0;
    
    if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, groupname))) {
      if (!(dg = Tcl_GetHashValue(entryPtr))) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "dl_find: invalid dgptr ", groupname, 
			 " in table", (char *) NULL);
	return TCL_ERROR;
      }
    }
    
    /* This searches for a *list* named groupname that is a list of lists */
    else if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dlTable, groupname))) {
      DYN_LIST *dll;

      if (!(dll = Tcl_GetHashValue(entryPtr))) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "dl_find: invalid dlptr ", groupname, 
			 " in table", (char *) NULL);
	return TCL_ERROR;
      }
      strncpy(listname, colon+1, 63);

      return(tclFindSubDynListParent(interp, groupname, dll, listname, dl,
				     parent, index));
    }
    else {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "dl_find: dyngroup \"", groupname, 
		       "\" not found", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    
    /* And see if the list exists inside the group */

    /* Could make this a loop, to allow depths greater than one */
    strncpy(listname, colon+1, 63);
    if ((colon = strchr(listname,':'))) {
      char lname[64], sel[32];
      
      /* Get the actual listname */
      if (colon-listname > 63) printf("Aha! (2)\n");
      strncpy(lname, listname, colon-listname);
      lname[colon-listname] = 0;
      if (!(list = dynGroupFindList(dg, lname))) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "dl_find: dynlist \"", lname, 
			 "\" not found in dyngroup \"", groupname, "\"",
			 (char *) NULL);
	return TCL_ERROR;
      }
      strncpy(sel, colon+1, 31);
      return (tclFindSubDynListParent(interp, lname, list, sel, dl, 
				      parent, index));
    }

    if ((list = dynGroupFindList(dg, listname))) {
      if (dl) {
	*dl = list;
	DYN_LIST_FLAGS(*dl) |= DL_SUBLIST;
      }
      return TCL_OK;
    }
    else {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "dl_find: dynlist \"", listname, 
		       "\" not found in dyngroup \"", groupname, "\"",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  else {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "dynlist \"", name, "\" not found", 
		     (char *) NULL);
    return TCL_ERROR;
  }
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclFindDynListInGroup
 *
 * ARGS
 *    Tcl_Interp *interp, char *name, DYN_GROUP **, int *index
 *
 * DESCRIPTION
 *    Looks for dynlist called name inside of a group
 *
 *****************************************************************************/

int tclFindDynListInGroup(Tcl_Interp *interp, char *name, 
			  DYN_GROUP **dgp, int *index)
{
  Tcl_HashEntry *entryPtr;
  char *colon;
  DYN_GROUP *dg;
  int listid;

  DLSHINFO *dlinfo = Tcl_GetAssocData(interp, DLSH_ASSOC_DATA_KEY, NULL);
  if (!dlinfo) return TCL_ERROR;
  
  if (dgp) *dgp = NULL;		/* initialize so that upon failure */
  if (index) *index = 0;        /* it's set to NULL                */

  if ((colon = strchr(name,':'))) {
    char groupname[64], listname[64];
    
    /* Get the group */
    
    if (colon-name > 63) printf("Aha! (3)\n");
    strncpy(groupname, name, colon-name);
    groupname[colon-name] = 0;
    
    if ((entryPtr = Tcl_FindHashEntry(&dlinfo->dgTable, groupname))) {
      if (!(dg = Tcl_GetHashValue(entryPtr))) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "dl_find: invalid dgptr ", groupname, 
			 " in table", (char *) NULL);
	return TCL_ERROR;
      }
    }
    else {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "dl_find: dyngroup \"", groupname, 
		       "\" not found", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    
    /* And see if the list exists inside the group */
    
    strncpy(listname, colon+1, 63);
    if ((listid = dynGroupFindListID(dg, listname)) >= 0) {
      if (dgp) *dgp = dg;
      if (index) *index = listid;
      return TCL_OK;
    }
    else {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "dl_find: dynlist \"", listname, 
		       "\" not found in dyngroup \"", groupname, "\"",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  else {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "group dynlist \"", name, "\" not found", 
		     (char *) NULL);
    return TCL_ERROR;
  }
}


/***************************************************************************/
/*                            Output Routines                              */
/***************************************************************************/

static 
int write_or_result(Tcl_Interp * interp, Tcl_Channel chan, char * data,
		    int len)
{
  if (chan) return(Tcl_Write(chan, data, len)) ;
  else Tcl_AppendResult(interp, data, NULL) ;
  return(TCL_OK) ;
}

void dynGroupChanDumpListNames(Tcl_Interp * interp, DYN_GROUP *dg,
			       Tcl_Channel chan)
{
  DYN_LIST *cols = dynListOnesInt(dynGroupMaxCols(dg));
  dynGroupChanDumpSelectedListNames(interp, dg, cols, chan);
  dfuFreeDynList(cols);
}

int dynGroupChanDumpSelectedListNames(Tcl_Interp * interp, DYN_GROUP *dg,
				      DYN_LIST *cols, Tcl_Channel chan)
{
  char buf[128];
  Tcl_Channel errChannel;
  int i, n = 0;

  if (DYN_LIST_N(cols) < DYN_GROUP_NLISTS(dg)) {
    errChannel = Tcl_GetStdChannel(TCL_STDERR);
    if (errChannel) {
      Tcl_Write(errChannel,
		"dynGroupChanDumpSelectedListNames(): invalid cols ", -1);
      Tcl_Write(errChannel, "\n", 1);
    }
    return(0);
  }
  
  write_or_result(interp, chan, "%%%\n%%% Dyngroup:\t", -1);
  write_or_result(interp, chan, DYN_GROUP_NAME(dg), -1);
  write_or_result(interp, chan, "\n%%%\n", -1);
  
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    if (!dynListTestVal(cols,i)) continue;
    sprintf(buf, "%%%%%% Col %2d:\t%s", 
	    n++, DYN_LIST_NAME(DYN_GROUP_LIST(dg,i)));
    write_or_result(interp, chan, buf, -1);
    if (chan || i < DYN_GROUP_NLISTS(dg)-1)
      write_or_result(interp, chan, "\n", -1);
  }
  return(1);
}

void dynGroupChanDump(Tcl_Interp * interp, DYN_GROUP *dg, char separator,
	Tcl_Channel chan)
{
  DYN_LIST *cols = dynListOnesInt(dynGroupMaxCols(dg));
  DYN_LIST *rows = dynListOnesInt(dynGroupMaxRows(dg));

  dynGroupChanDumpSelected(interp, dg, rows, cols, separator, chan);

  dfuFreeDynList(rows);
  dfuFreeDynList(cols);
}

int dynGroupChanDumpSelected(Tcl_Interp * interp, DYN_GROUP *dg,
	DYN_LIST *rows, DYN_LIST *cols, char separator, Tcl_Channel chan)
{
  int i, j, n = 0, last, addquotes = 0;
  int maxlistsize = dynGroupMaxRows(dg);
  Tcl_Channel errChannel;
  char sep_string[2] ;	/* must always write NULL terminated strings */
  char quote_string[2] ;/* must always write NULL terminated strings */

  if (maxlistsize == 0) return(1);
  if (DYN_LIST_N(rows) < maxlistsize || 
      DYN_LIST_N(cols) < DYN_GROUP_NLISTS(dg)) {
    errChannel = Tcl_GetStdChannel(TCL_STDERR);
    if (errChannel) {
      Tcl_Write(errChannel,
		"dynGroupChanDumpSelected(): invalid rows/cols", -1);
      Tcl_Write(errChannel, "\n", 1);
    }
    return(0);
  }

  if (separator == ',') {
    addquotes = 1;
    quote_string[0] = '"';
    quote_string[1] = 0;
  }

  sep_string[0] = separator ;
  sep_string[1] = 0 ;
  last = maxlistsize-1;
  for (i = 0; i < maxlistsize; i++) {
    if (!dynListTestVal(rows,i)) continue;
    for (j = 0, n = 0; j < DYN_GROUP_NLISTS(dg); j++) {
      if (!dynListTestVal(cols,j)) continue;
      if (n++) write_or_result(interp, chan, sep_string, 1);
      if (addquotes) write_or_result(interp, chan, quote_string, 1);
      if (!dynListPrintValChan(interp, DYN_GROUP_LIST(dg,j), i, chan)) {
	if (DYN_LIST_DATATYPE(DYN_GROUP_LIST(dg,j)) == DF_FLOAT ||
	    DYN_LIST_DATATYPE(DYN_GROUP_LIST(dg,j)) == DF_LIST)
	  write_or_result(interp, chan, "   *   ", -1);
	else write_or_result(interp, chan, ".", 1);
      }
      if (addquotes) write_or_result(interp, chan, quote_string, 1);
    }
    if (chan || i < last) write_or_result(interp, chan, "\n", 1);
  }
  return(1);
}


int dynListChanDump(Tcl_Interp * interp, DYN_LIST *dl, Tcl_Channel chan)
{
  int i;
  int last;
  
  if (!dl) return(0);
  
  last = DYN_LIST_N(dl)-1;
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (!dynListPrintValChan(interp, dl, i, chan)) {
      if (DYN_LIST_DATATYPE(dl) == DF_FLOAT ||
	  DYN_LIST_DATATYPE(dl) == DF_LIST)
	write_or_result(interp, chan, "   *   ", -1);
      else write_or_result(interp, chan, ".", 1);
    }
    if (chan || i < last)
      write_or_result(interp, chan, "\n", 1);
  }
  return(1);
}

int dynListChanDumpMatrix(Tcl_Interp * interp,DYN_LIST *dl, Tcl_Channel chan,
	char separator)
{
  int i, last;
  
  DYN_LIST **vals;
  
  if (!dl) return(0);
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(0);
  vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  last = DYN_LIST_N(dl)-1;
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    dynListChanDumpAsRow(interp, vals[i], chan, separator);
    if (!chan && i < last) 
      write_or_result(interp, chan, "\n", 1);
  }
  return(1);
}


int dynListChanDumpMatrixInCols(Tcl_Interp * interp, DYN_LIST *dl,
	Tcl_Channel chan, char separator)
{
  int i, j, maxlength, last;
  DYN_LIST **vals;
  char sep_string[2] ;	/* only write NULL terminated strings */

  if (!dl) return(0);
  if (DYN_LIST_DATATYPE(dl) != DF_LIST) return(0);
  vals = (DYN_LIST **) DYN_LIST_VALS(dl);
  maxlength =  dynListMaxListLength(dl);
  
  sep_string[0] = separator ;
  sep_string[1] = 0 ;
  last = maxlength-1;
  for (i = 0; i < maxlength; i++) {
    for (j = 0; j < DYN_LIST_N(dl); j++) {
      if (j) write_or_result(interp, chan, sep_string, 1);
      if (!dynListPrintValChan(interp, vals[j], i, chan)) {
	if (DYN_LIST_DATATYPE(dl) == DF_FLOAT ||
	    DYN_LIST_DATATYPE(dl) == DF_LIST)
	  write_or_result(interp, chan, "   *   ", -1);
	else write_or_result(interp, chan, ".", 1);
      }
    }
    if (chan || i < last) 
      write_or_result(interp, chan, "\n", 1);
  }
  return(1);
}


int dynListChanDumpAsRow(Tcl_Interp * interp, DYN_LIST *dl, Tcl_Channel chan,
	char separator)
{
  int i, last;
  char sep_string[2] ;	/* only write NULL terminated strings */

  if (!dl) return(0);

  sep_string[0] = separator ;
  sep_string[1] = 0 ;
  last = DYN_LIST_N(dl)-1;
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    if (i) write_or_result(interp, chan, sep_string, 1);
    if (!dynListPrintValChan(interp, dl, i, chan)) {
      if (DYN_LIST_DATATYPE(dl) == DF_FLOAT ||
	  DYN_LIST_DATATYPE(dl) == DF_LIST)
	write_or_result(interp, chan, "   *   ", -1);
      else write_or_result(interp, chan, ".", 1);
    }
  }
  if (chan || i < last) write_or_result(interp, chan, "\n", 1);
  return(1);
}

int dynListPrintValChan(Tcl_Interp * interp, DYN_LIST *dl, int i,
	Tcl_Channel chan)
{
  char buf[32];
  if (!dl) return(0);
  if (i < 0 || DYN_LIST_N(dl) <= i) return(0);

  switch (DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    {
      int *vals = (int *) DYN_LIST_VALS(dl);
      sprintf(buf, DLFormatTable[FMT_LONG], (int) vals[i]);
      write_or_result(interp, chan, buf, -1);
    }
    break;
  case DF_SHORT:
    {
      short *vals = (short *) DYN_LIST_VALS(dl);
      sprintf(buf, DLFormatTable[FMT_SHORT], vals[i]);
      write_or_result(interp, chan, buf, -1);
    }
    break;
  case DF_FLOAT:
    {
      float *vals = (float *) DYN_LIST_VALS(dl);
      sprintf(buf, DLFormatTable[FMT_FLOAT], vals[i]);
      write_or_result(interp, chan, buf, -1);
    }
    break;
  case DF_CHAR:
    {
      char *vals = (char *) DYN_LIST_VALS(dl);
      sprintf(buf, DLFormatTable[FMT_CHAR], vals[i]);
      write_or_result(interp, chan, buf, -1);
    }
    break;
  case DF_LIST:
    {
      DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
      sprintf(buf, DLFormatTable[FMT_LIST], DYN_LIST_DATATYPE(vals[i]), 
	      DYN_LIST_N(vals[i]));
      write_or_result(interp, chan, buf, -1);
    }
    break;
  case DF_STRING:
    {
      char strbuf[1024];
      char **vals = (char **) DYN_LIST_VALS(dl);
      sprintf(strbuf, DLFormatTable[FMT_STRING], vals[i]);
      write_or_result(interp, chan, strbuf, -1);
    }
    break;
  }
  return(1);
}



/***************************************************************\
* tclDoTimes
*    Executes a script operation repeatedly, like a for loop
*
* Arguments:
*    var - tcl variable name
*    count - number of iterations to perform
*    body - tcl code to execute
\***************************************************************/

static int tclDoTimes(ClientData data, Tcl_Interp * interp, int objc,
	Tcl_Obj * const objv[])
{
	int i, count;
	Tcl_Obj * o;

	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "var count body");
		return TCL_ERROR;
	}
	if ((o = Tcl_ObjSetVar2(interp, objv[1], NULL, Tcl_NewObj(),
		TCL_LEAVE_ERR_MSG)) == NULL)
		return TCL_ERROR;
	if (Tcl_GetIntFromObj(interp, objv[2], &count) != TCL_OK) {
		Tcl_UnsetVar2(interp, Tcl_GetStringFromObj(objv[1], NULL),
			NULL, TCL_LEAVE_ERR_MSG);
		return TCL_ERROR;
	}
	for (i = 0; i < count; i++) {
		Tcl_SetIntObj(o, i);
		if (Tcl_EvalObj(interp, objv[3]) != TCL_OK) return TCL_ERROR;
	}
	return Tcl_UnsetVar2(interp, Tcl_GetStringFromObj(objv[1], NULL),
		NULL, TCL_LEAVE_ERR_MSG);
}


/***************************************************************\
* tclForEach
*    Executes a script operation repeatedly, like a for loop
*
* Arguments:
*    var - tcl variable name
*    vals - number of iterations to perform
*    body - tcl code to execute
\***************************************************************/

static int forEachLong(Tcl_Interp * interp, Tcl_Obj * obj, DYN_LIST * dl,
		       Tcl_Obj * body);
static int forEachShort(Tcl_Interp * interp, Tcl_Obj * obj, DYN_LIST * dl,
			Tcl_Obj * body);
static int forEachFloat(Tcl_Interp * interp, Tcl_Obj * obj, DYN_LIST * dl,
			Tcl_Obj * body);
static int forEachString(Tcl_Interp * interp, Tcl_Obj * obj, DYN_LIST * dl,
			 Tcl_Obj * body);

static int tclForEach(ClientData data, Tcl_Interp * interp, int objc,
		      Tcl_Obj * const objv[])
{
  int i;
  Tcl_Obj * o;
  DYN_LIST * dl;
  
  if (objc != 4) {
    Tcl_WrongNumArgs(interp, 1, objv, "var vals body");
    return TCL_ERROR;
  }
  if ((o = Tcl_ObjSetVar2(interp, objv[1], NULL, Tcl_NewObj(),
			  TCL_LEAVE_ERR_MSG)) == NULL)
    return TCL_ERROR;
  if (tclFindDynList(interp, Tcl_GetStringFromObj(objv[2], NULL), &dl)
      != TCL_OK) {
    Tcl_UnsetVar2(interp, Tcl_GetStringFromObj(objv[1], NULL),
		  NULL, TCL_LEAVE_ERR_MSG);
    return TCL_ERROR;
  }
  switch(DYN_LIST_DATATYPE(dl)) {
  case DF_LONG:
    i = forEachLong(interp, o, dl, objv[3]); break;
  case DF_SHORT:
    i = forEachShort(interp, o, dl, objv[3]); break;
  case DF_FLOAT:
    i = forEachFloat(interp, o, dl, objv[3]); break;
  case DF_STRING:
    i = forEachString(interp, o, dl, objv[3]); break;
  }
  Tcl_UnsetVar2(interp, Tcl_GetStringFromObj(objv[1], NULL),
		NULL, TCL_LEAVE_ERR_MSG);
  return i;
}


/*
 * Procedures for handling different data types within above procedure.
 */

static int forEachLong(Tcl_Interp * interp, Tcl_Obj * obj, DYN_LIST * dl,
		       Tcl_Obj * body)
{
  int i;
  int *vals = (int *) DYN_LIST_VALS(dl);
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    Tcl_SetLongObj(obj, vals[i]);
    if (Tcl_EvalObj(interp, body) != TCL_OK) return TCL_ERROR;
  }
  return TCL_OK;
}


static int forEachShort(Tcl_Interp * interp, Tcl_Obj * obj, DYN_LIST * dl,
			Tcl_Obj * body)
{
  int i;
  short *vals = (short *) DYN_LIST_VALS(dl);
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    Tcl_SetIntObj(obj, vals[i]);
    if (Tcl_EvalObj(interp, body) != TCL_OK) return TCL_ERROR;
  }
  return TCL_OK;
}


static int forEachFloat(Tcl_Interp * interp, Tcl_Obj * obj, DYN_LIST * dl,
			Tcl_Obj * body)
{
  int i;
  float *vals = (float *) DYN_LIST_VALS(dl);
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    Tcl_SetDoubleObj(obj, vals[i]);
    if (Tcl_EvalObj(interp, body) != TCL_OK) return TCL_ERROR;
  }
  return TCL_OK;
}


static int forEachString(Tcl_Interp * interp, Tcl_Obj * obj, DYN_LIST * dl,
			 Tcl_Obj * body)
{
  int i;
  char **vals = (char **) DYN_LIST_VALS(dl);
  
  for (i = 0; i < DYN_LIST_N(dl); i++) {
    Tcl_SetStringObj(obj, vals[i], -1);
    if (Tcl_EvalObj(interp, body) != TCL_OK) return TCL_ERROR;
  }
  return TCL_OK;
}

/* Date functions (replacing IMSL equivalents) */
	

/*****************************************************************************
 *
 * FUNCTION
 *    tclDaysToDate
 *
 * ARGS
 *    Tcl Args
 *
 * LABLIB FUNCTION
 *    trDaysToDate
 *
 * DESCRIPTION
 *    Returns days from Jan 1 1990 to given date
 *
 *****************************************************************************/

static int tclDaysToDate (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  int days;
  int day, month, year;
  extern void trDaysToDate(int, int *, int *, int *);

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " days", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetInt(interp, argv[1], &days) != TCL_OK) return TCL_ERROR;

  trDaysToDate(days, &day, &month, &year);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(day));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(month));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(year));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclDateToDays
 *
 * ARGS
 *    Tcl Args
 *
 * LABLIB FUNCTION
 *    trDateToDays
 *
 * DESCRIPTION
 *    Returns days from Jan 1 1990 to given date
 *
 *****************************************************************************/

static int tclDateToDays (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  int retval;
  int day, month, year;
  extern int trDateToDays(int, int, int);

  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " day month year", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetInt(interp, argv[1], &day) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &month) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &year) != TCL_OK) return TCL_ERROR;

  retval = trDateToDays(day, month, year);
  Tcl_SetIntObj(Tcl_GetObjResult(interp), retval);
  return TCL_OK;
}
