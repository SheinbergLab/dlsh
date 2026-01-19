/*************************************************************************
 *
 *  NAME
 *    dfana.h
 *
 *  DESCRIPTION
 *    Functions for extracting data/events from the df structure.
 *
 *  AUTHOR
 *    DLS
 *
 ************************************************************************/

enum DL_MATH_TYPES { DL_MATH_ADD, DL_MATH_SUB, DL_MATH_MUL, DL_MATH_DIV,
		     DL_MATH_POW, DL_MATH_MIN, DL_MATH_MAX, DL_MATH_ATAN2,
                     DL_MATH_FMOD };
enum DL_GEN_TYPES { DL_GEN_ZEROS, DL_GEN_ONES, DL_GEN_URAND, DL_GEN_ZRAND,
		  DL_GEN_RANDFILL, DL_GEN_RANDCHOOSE };
enum DL_RELATION_TYPES { DL_RELATION_OR, DL_RELATION_AND, DL_RELATION_EQ, 
			 DL_RELATION_NE, DL_RELATION_LT, DL_RELATION_LTE,
			 DL_RELATION_GT, DL_RELATION_GTE, DL_RELATION_MOD };
enum DL_MINMAX_TYPES { DL_MINIMUM, DL_MAXIMUM };
enum DL_SUMPROD_TYPES { DL_SUM_LIST, DL_PROD_LIST };
enum DL_SHIFT_TYPES { DL_SHIFT_CYCLE, DL_SHIFT_LEFT, DL_SHIFT_RIGHT,
                      DL_SUBSHIFT_LEFT, DL_SUBSHIFT_RIGHT};
enum EM_MODES { EM_ALL, EM_HORIZ, EM_VERT };

struct TableEntry {
  char *name;
  int  id;
};

enum DL_FORMAT_IDS { FMT_LONG, FMT_SHORT, FMT_FLOAT,
		     FMT_STRING, FMT_LIST, FMT_CHAR, N_FMT_STRINGS };
#define MAX_FORMAT_STRING  128
extern char DLFormatTable[][128];	/* contains format strings for data */

#ifdef __cplusplus
extern "C" {
#endif
  
int dynListSetMatherrCheck(int);

int dynGetDatatypeID(char *tname, int *tid);
char *dynGetDatatypeName(int tid);

int  dynGroupMaxRows(DYN_GROUP *dg);
int  dynGroupMaxCols(DYN_GROUP *dg);

DYN_LIST *dynGroupFindList(DYN_GROUP *dg, char *name);
int dynGroupFindListID(DYN_GROUP *dg, char *name);

int dynGroupRemoveList(DYN_GROUP *dg, char *name);
int dynGroupSetList(DYN_GROUP *dg, char *name, DYN_LIST *dl);

void dynGroupDump(DYN_GROUP *dg, char separator, FILE *stream);
int  dynGroupDumpSelected(DYN_GROUP *dg, DYN_LIST *rows, DYN_LIST *cols,
			  char separator, FILE *stream);
void dynGroupDumpListNames(DYN_GROUP *dg, FILE *stream);
int dynGroupDumpSelectedListNames(DYN_GROUP *dg, DYN_LIST *cols, FILE *stream);

int dynGroupAppend(DYN_GROUP *dg1, DYN_GROUP *dg2);

DYN_LIST *dynListConvertList(DYN_LIST *dl, int type);
DYN_LIST *dynListUnsignedConvertList(DYN_LIST *dl, int type);

int dynListConcat(DYN_LIST *dl1, DYN_LIST *dl2);
DYN_LIST *dynListCombine(DYN_LIST *dl1, DYN_LIST *dl2);
DYN_LIST *dynListInterleave(DYN_LIST *dl1, DYN_LIST *dl2);
DYN_LIST *dynListReplace(DYN_LIST *dl, DYN_LIST *selections, DYN_LIST *r);
DYN_LIST *dynListReplaceByIndex(DYN_LIST *dl, DYN_LIST *s, DYN_LIST *r);
DYN_LIST *dynListWhere(DYN_LIST *mask, DYN_LIST *if_true, DYN_LIST *if_false);
  
DYN_LIST *dynListSelect(DYN_LIST *dl, DYN_LIST *selections);
DYN_LIST *dynListChoose(DYN_LIST *dl, DYN_LIST *selections);

DYN_LIST *dynListIndexList(DYN_LIST *dl);
DYN_LIST *dynListFirstIndexList(DYN_LIST *dl);
DYN_LIST *dynListLastIndexList(DYN_LIST *dl);

DYN_LIST *dynListOneOf(DYN_LIST *dl1, DYN_LIST *dl2);
DYN_LIST *dynListOneOfIndices(DYN_LIST *dl1, DYN_LIST *dl2);
DYN_LIST *dynListRelationListList(DYN_LIST *dl1, DYN_LIST *dl2, int op);
DYN_LIST *dynListRelationListListIndices(DYN_LIST *dl1, DYN_LIST *dl2, int op);
DYN_LIST *dynListNotList(DYN_LIST *dl);

int dynListAnyList(DYN_LIST *dl);
DYN_LIST *dynListAnyLists(DYN_LIST *dl);
int dynListAllList(DYN_LIST *dl);
DYN_LIST *dynListAllLists(DYN_LIST *dl);
  
DYN_LIST *dynListZerosFloat(int size);
DYN_LIST *dynListOnesFloat(int size);

DYN_LIST *dynListZerosInt(int size);
DYN_LIST *dynListOnesInt(int size);
DYN_LIST *dynListUniformRandsInt(int size);
DYN_LIST *dynListNormalRandsInt(int size);
DYN_LIST *dynListRandFillInt(int size);
DYN_LIST *dynListRandChooseInt(int m, int n);
DYN_LIST *dynListUniformIRandsInt(int size, int max);
DYN_LIST *dynListShuffleList(DYN_LIST *dl);

DYN_LIST *dynListOnes(DYN_LIST *sizelist);
DYN_LIST *dynListZeros(DYN_LIST *sizelist);
DYN_LIST *dynListUniformRands(DYN_LIST *sizelist);
DYN_LIST *dynListNormalRands(DYN_LIST *sizelist);
DYN_LIST *dynListRandFill(DYN_LIST *sizelist);
DYN_LIST *dynListRandChoose(DYN_LIST *ms, DYN_LIST *ns);
DYN_LIST *dynListUniformIRands(DYN_LIST *size, DYN_LIST *maxlist);
DYN_LIST *dynListGaussian2D(int, int, float);

DYN_LIST *dynListBins(DYN_LIST *range, int nbins);
DYN_LIST *dynListSeries(DYN_LIST *starts, DYN_LIST *stops, DYN_LIST *steps, 
			int type);
DYN_LIST *dynListSeriesFloat(float start, float stop, float step);
DYN_LIST *dynListSeriesFloatExclude(float start, float stop, float step);
DYN_LIST *dynListSeriesFloatT(float start, float stop, float step, int type);
DYN_LIST *dynListSeriesLong(long start, long stop, long step);
DYN_LIST *dynListSeriesLongExclude(long start, long stop, long step);
DYN_LIST *dynListSeriesLongT(long start, long stop, long step, int type);

DYN_LIST *dynListFindSublist(DYN_LIST *source, DYN_LIST *pattern);
DYN_LIST *dynListFindIndices(DYN_LIST *source, DYN_LIST *search);
DYN_LIST *dynListFindSublistAll(DYN_LIST *source, DYN_LIST *pattern);
DYN_LIST *dynListCountOccurences(DYN_LIST *source, DYN_LIST *pattern);
DYN_LIST *dynListFillSparse(DYN_LIST *vals, DYN_LIST *times, DYN_LIST *range);
DYN_LIST *dynListRepeat(DYN_LIST *dl, DYN_LIST *rep);
DYN_LIST *dynListRepeatElements(DYN_LIST *dl, DYN_LIST *rep);
DYN_LIST *dynListReplicate(DYN_LIST *dl, int n);

int dynListIncrementCounter(DYN_LIST *dl);
int dynListAddNullElement(DYN_LIST *newlist);

DYN_LIST *dynListArithListList(DYN_LIST *l1, DYN_LIST *l2, int func);
DYN_LIST *dynListAddListList(DYN_LIST *d1, DYN_LIST *d2);
DYN_LIST *dynListSubListList(DYN_LIST *d1, DYN_LIST *d2);
DYN_LIST *dynListMultListList(DYN_LIST *l1, DYN_LIST *l2);
DYN_LIST *dynListDivListList(DYN_LIST *l1, DYN_LIST *l2);
DYN_LIST *dynListPermuteList(DYN_LIST *list, DYN_LIST *order);
DYN_LIST *dynListReverseList(DYN_LIST *dl);
DYN_LIST *dynListReverseAll(DYN_LIST *dl);
DYN_LIST *dynListBReverseList(DYN_LIST *dl);

DYN_LIST *dynListShift(DYN_LIST *dl, int shifter, int mode);
DYN_LIST *dynListBShift(DYN_LIST *dl, int shifter, int mode);
DYN_LIST *dynListIndices(DYN_LIST *dl);

int dynListIsMatrix(DYN_LIST *dl);
int dynMatrixDims(DYN_LIST *m, int *nrows, int *ncols);
DYN_LIST *dynMatrixReshape(DYN_LIST *dl, int rows, int cols);
DYN_LIST *dynMatrixIdentity(int nrows);
DYN_LIST *dynMatrixZeros(int nrows, int ncols);
DYN_LIST *dynMatrixUrands(int nrows, int ncols);
DYN_LIST *dynMatrixZrands(int nrows, int ncols);
DYN_LIST *dynMatrixInverse(DYN_LIST *M);
DYN_LIST *dynMatrixLUInverse(DYN_LIST *M);
DYN_LIST *dynMatrixLudcmp(DYN_LIST *M);
DYN_LIST *dynMatrixDiag(DYN_LIST *M);
DYN_LIST *dynMatrixColMeans(DYN_LIST *M);
DYN_LIST *dynMatrixRowMeans(DYN_LIST *M);
DYN_LIST *dynMatrixColSums(DYN_LIST *M);
DYN_LIST *dynMatrixRowSums(DYN_LIST *M);
DYN_LIST *dynMatrixCenterRows(DYN_LIST *m, DYN_LIST *v);
DYN_LIST *dynMatrixCenterCols(DYN_LIST *m, DYN_LIST *v);

DYN_LIST *dynMatrixAddFloat(DYN_LIST *m1, float);
DYN_LIST *dynMatrixSubFloat(DYN_LIST *m1, float);
DYN_LIST *dynMatrixMultFloat(DYN_LIST *m1, float);
DYN_LIST *dynMatrixDivFloat(DYN_LIST *m1, float);
DYN_LIST *dynMatrixAdd(DYN_LIST *m1, DYN_LIST *m2);
DYN_LIST *dynMatrixSubtract(DYN_LIST *m1, DYN_LIST *m2);
DYN_LIST *dynMatrixMultiply(DYN_LIST *m1, DYN_LIST *m2);

int dynListDump(DYN_LIST *dl, FILE *stream);
int dynListDumpAsRow(DYN_LIST *dl, FILE *stream, char separator);
int dynListDumpMatrix(DYN_LIST *dl, FILE *stream, char separator);
int dynListDumpMatrixInCols(DYN_LIST *dl, FILE *stream, char separator);

DYN_LIST *dynListElementList(DYN_LIST *source, int i);
DYN_LIST *dynListCopyElementList(DYN_LIST *source, int i);

char *dynListSetFormat(int type, char *format);
int dynListPrintVal(DYN_LIST *dl, int i, FILE *stream);
int dynListTestValBetween(DYN_LIST *dl, int i, int min, int max);
int dynListTestVal(DYN_LIST *dl, int i);
int dynListCompareElements(DYN_LIST *l1, int i1, DYN_LIST *l2, int i2);
int dynListCopyElement(DYN_LIST *source, int i, DYN_LIST *dest);

DYN_LIST *dynListStringMatch(DYN_LIST *, DYN_LIST *, int);

int dynListSetValLong(DYN_LIST *dl, int i, int val);
int dynListSetIntVals(DYN_LIST *dl, int val, int n);

int dynListDepth(DYN_LIST *dl, int level);

DYN_LIST *dynListReciprocal(DYN_LIST *dl);
DYN_LIST *dynListSignList(DYN_LIST *dl);
DYN_LIST *dynListNegateList(DYN_LIST *dl);
DYN_LIST *dynListLength(DYN_LIST *dl);
DYN_LIST *dynListLLength(DYN_LIST *dl);

DYN_LIST *dynListReshapeList(DYN_LIST *dl, int nrows, int ncols);
DYN_LIST *dynListRestructureList(DYN_LIST *, DYN_LIST *, int, int *, int *);
DYN_LIST *dynListSpliceLists(DYN_LIST *dl, DYN_LIST *dl2, int pos);
DYN_LIST *dynListListLengths(DYN_LIST *dl);
DYN_LIST *dynListCollapseList(DYN_LIST *dl);
DYN_LIST *dynListPackList(DYN_LIST *dl);
DYN_LIST *dynListDeepPackList(DYN_LIST *dl);
DYN_LIST *dynListUnpackList(DYN_LIST *dl);
DYN_LIST *dynListUnpackLists(DYN_LIST *dl, DYN_LIST *out);
DYN_LIST *dynListDeepUnpackList(DYN_LIST *dl);
DYN_LIST *dynListSortList(DYN_LIST *dl);
DYN_LIST *dynListBSortList(DYN_LIST *dl);
DYN_LIST *dynListSortListIndices(DYN_LIST *dl);
DYN_LIST *dynListBSortListIndices(DYN_LIST *dl);
DYN_LIST *dynListUniqueList(DYN_LIST *dl);
DYN_LIST *dynListUniqueNoSortList(DYN_LIST *dl);
DYN_LIST *dynListTransposeList(DYN_LIST *dl);
DYN_LIST *dynListTransposeListAt(DYN_LIST *dl, int level);
DYN_LIST *dynListUniqueCrossLists(DYN_LIST *categories);
DYN_LIST *dynListCrossLists(DYN_LIST *lists);

DYN_LIST *dynListSortListByList(DYN_LIST *data, DYN_LIST *categories);
DYN_LIST *dynListSortListByLists(DYN_LIST *data, DYN_LIST *categories, 
				 DYN_LIST *selections);

DYN_LIST *dynListRecodeList(DYN_LIST *dl);
DYN_LIST *dynListRecodeListFromList(DYN_LIST *dl, DYN_LIST *dl2);
DYN_LIST *dynListRecodeWithTiesList(DYN_LIST *dl);

DYN_LIST *dynListCutList(DYN_LIST *dl, DYN_LIST *breaks);

DYN_LIST *dynListRankOrderedList(DYN_LIST *dl);

DYN_LIST *dynListMinMaxList(DYN_LIST *dl, int op);
DYN_LIST *dynListMinMaxPositions(DYN_LIST *dl, int op);

DYN_LIST *dynListMaxLists(DYN_LIST *dl);
float dynListMaxList(DYN_LIST *dl);

DYN_LIST *dynListMinLists(DYN_LIST *dl);
float dynListMinList(DYN_LIST *dl);

int dynListMinListIndex(DYN_LIST *dl);
int dynListMaxListIndex(DYN_LIST *dl);
DYN_LIST *dynListMinMaxIndices(DYN_LIST *dl, int op);

DYN_LIST *dynListBMinMaxList(DYN_LIST *dl, int op);

DYN_LIST *dynListLengthLists(DYN_LIST *dl);
long dynListLengthList(DYN_LIST *dl);

DYN_LIST *dynListSumProdList(DYN_LIST *dl, int op);
DYN_LIST *dynListSumLists(DYN_LIST *dl);
DYN_LIST *dynListBSumLists(DYN_LIST *dl);
DYN_LIST *dynListProdLists(DYN_LIST *dl);

DYN_LIST *dynListCumSumProdList(DYN_LIST *dl, int op);

DYN_LIST *dynListMeanLists(DYN_LIST *dl);
DYN_LIST *dynListHMeanLists(DYN_LIST *dl);
DYN_LIST *dynListBMeanLists(DYN_LIST *dl);
float dynListMeanList(DYN_LIST *dl);
DYN_LIST *dynListAverageList(DYN_LIST *dl);
DYN_LIST *dynListSumColsList(DYN_LIST *dl);

DYN_LIST *dynListHistLists(DYN_LIST *dl, DYN_LIST *range, int);
DYN_LIST *dynListHistList(DYN_LIST *dl, DYN_LIST *range, int);

DYN_LIST *dynListStdLists(DYN_LIST *dl);
DYN_LIST *dynListHStdLists(DYN_LIST *dl);
DYN_LIST *dynListBStdLists(DYN_LIST *dl);
float dynListStdList(DYN_LIST *dl);

DYN_LIST *dynListVarLists(DYN_LIST *dl);
DYN_LIST *dynListHVarLists(DYN_LIST *dl);
float dynListVarList(DYN_LIST *dl);

DYN_LIST *dynListCountLists(DYN_LIST *dl, DYN_LIST *range);
long dynListCountList(DYN_LIST *dl, DYN_LIST *range);

DYN_LIST *dynListListCounts(DYN_LIST *dl, DYN_LIST *range);
DYN_LIST *dynListListCount(DYN_LIST *dl, DYN_LIST *range);

DYN_LIST *dynListDiffList(DYN_LIST *dl, int lag);
DYN_LIST *dynListGradientList(DYN_LIST *dl);
DYN_LIST *dynListIdiffLists(DYN_LIST *dl1, DYN_LIST *dl2, int autocorr);
DYN_LIST *dynListZeroCrossingList(DYN_LIST *dl);

DYN_LIST *dynListSdfFullLists(DYN_LIST *dl, float ksd, float knsd, int resol);
DYN_LIST *dynListSdfFullList(DYN_LIST *dl, float ksd, float knsd, int resol);
DYN_LIST *dynListSdfLists(DYN_LIST *dl, DYN_LIST *start, DYN_LIST *stop, float ksd,
			  float knsd, int resolution);
DYN_LIST *dynListSdfList(DYN_LIST *dl, float start, float stop, float ksd, 
			 float knsd, int resol);
DYN_LIST *dynListSdfListsR(DYN_LIST *dl, DYN_LIST *start, DYN_LIST *stop, float ksd,
			  float knsd, int resolution);
DYN_LIST *dynListSdfListR(DYN_LIST *dl, float start, float stop, float ksd, 
			 float knsd, int resol);
DYN_LIST *dynListParzenLists(DYN_LIST *dl, DYN_LIST *start, DYN_LIST *stop, 
			     float ksd, float nsd, int resolution);
DYN_LIST *dynListParzenList(DYN_LIST *dl, float start, float stop, float ksd, 
			    float nsd, int resol);


int dynListMaxListLength(DYN_LIST *dl);
int dynListMinListLength(DYN_LIST *dl);

DYN_LIST *dynListLowess(DYN_LIST *dlx, DYN_LIST *dly, float f, int nsteps,
			float delta);

DYN_LIST *dynListConvList(DYN_LIST *dl, DYN_LIST *kernel);
DYN_LIST *dynListConvList2(DYN_LIST *dl, DYN_LIST *kernel);

/**********************************
 ************* Math Function Tables
 **********************************/

enum DL_MATHFUNC1    { DL_ABS, DL_ACOS, DL_ASIN, DL_ATAN, DL_CEIL, 
		       DL_COS, DL_COSH, DL_EXP, DL_FLOOR,
		       DL_LOG, DL_LOG10, DL_ROUND, DL_SIN,
		       DL_SINH, DL_SQRT, DL_TAN, DL_TANH, DL_LGAMMA,
		       DL_N_MATHFUNC1 };

typedef double (*MATH_FUNC1)(double);
     
struct MathFunc1 {
  char *name;
  MATH_FUNC1 mfunc;
};

extern struct MathFunc1 Math1Table[];

DYN_LIST *dynListMathOneArg(DYN_LIST *dl, int func_id);

EV_LIST *evGetList(OBS_P *obsp, int tag, int *np);
int evGetTagID(char *tagName, int *tag);

int evGetTimeWithData(OBS_P *obsp, int tag, int data, int *evdata, int *time);
int evGetDataAtTime(OBS_P *obsp, int tag, int time, int *evdata);
int evGetDataAtOrAfterTime(OBS_P *obsp, int tag, int time, int *evdata, int *);
int evGetDataAfterTime(OBS_P *obsp, int tag, int time, int *evdata, int *rt);
int evGetDataBeforeTime(OBS_P *obsp, int tag, int time, int *evdata, int *rt);
int evGetDataAtOrBeforeTime(OBS_P *obsp, int t, int time, int *evdata, int *);
int evGetWithDataAtTime(OBS_P *obsp, int tag, int data, int time, int *evdata);
int evGetWithDataAtOrAfterTime(OBS_P *obsp, int tag, int data, int time, 
			       int *evd, int *);
int evGetWithDataAfterTime(OBS_P *obsp, int tag, int data, int time, 
			   int *evdata, int *rt);
int evGetWithDataBeforeTime(OBS_P *obsp, int tag, int time, int data,
			    int *evdata, int *rt);
int evGetWithDataAtOrBeforeTime(OBS_P *obsp, int tag, int data, int time, 
				int *evd, int *);
int evGetWithNotDataAtTime(OBS_P *obsp, int tag, int data, int time, 
			   int *evdata);
int evGetWithNotDataAtOrAfterTime(OBS_P *obsp, int tag, int data, int time, 
				  int *evd, int *);
int evGetWithNotDataAfterTime(OBS_P *obsp, int tag, int data, int time, 
			      int *evdata, int *rt);
int evGetWithNotDataBeforeTime(OBS_P *obsp, int tag, int time, int data,
			       int *evdata, int *rt);
int evGetWithNotDataAtOrBeforeTime(OBS_P *obsp, int tag, int data, int time, 
				   int *evd, int *);


DYN_LIST *evGetSpikes(OBS_P *obsp, int start, int stop, int offset);

int evGetMeanEMPositions (OBS_P *obsp, float *hmean, float *vmean,
			  int *startp, int *stopp);
DYN_LIST *evGetEMData(OBS_P *obsp, int start, int stop, 
		       int *zerostartp, int *zerostopp, int);

float *sdfMakeSdf (float start, int duration, float *spikes, int nspikes,
		   float *kernel, int ksize);
float *sdfMakeAdaptiveSdf (int start, int duration, float *spikes, int nspikes,
			   float pilot_sd, float nsd);
float *sdfMakeKernel (float ksd, float nsd, int *ksize);



#define DGFL(g,n)      (dynGroupFindList(g,n))
#define DGFL_ID(g,n)   (dynGroupFindListID(g,n))


void lowess(float *x, float *y, int n, float f, int nsteps, float delta,
	   float *ys, float *rw, float *res);

#ifdef __cplusplus
}
#endif
  
