/*
 * Publically called functions from the tcl_dl module.
 */

#define DEFAULT_STARTUP_DIR  "/usr/local/lib/dlsh"
#define DEFAULT_STARTUP_FILE "dlshrc"
#ifdef WIN32
#include <fcntl.h>		/* for _O_BINARY */
#pragma warning (disable:4244)
#pragma warning (disable:4305)
#pragma warning (disable:4761)
#pragma warning (disable:4133)
#pragma warning (disable:4996)
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define DLLEXP _declspec(dllexport)
#else
#define DLLEXP
#endif

/*
 * Tmplist stack structure for pushing & popping tmp list names
 */

typedef struct {
  int size;
  int index;
  int increment;
  DYN_LIST **lists;
} TMPLIST_STACK;


#define TMPLIST_SIZE(t)        ((t)->size)
#define TMPLIST_INDEX(t)       ((t)->index)
#define TMPLIST_INC(t)         ((t)->increment)
#define TMPLIST_TMPLISTS(t)    ((t)->lists)

typedef struct _dlshinfo {
  /*
   * Local tables for holding dynGroups and dynLists
   */
  Tcl_HashTable dlTable;	/* stores dynLists  */
  Tcl_HashTable dgTable;	/* stores dynGroups */

  /*
   * amounts to grow by dynamically
   */
  int DefaultListIncrement;
  int DefaultGroupIncrement;
  
  /*
   * variables used to manage groups and lists
   */
  int dgCount;		/* increasing count of dynGroups */
  int dlCount;	        /* increasing count of dynLists  */
  int localCount;       /* for naming local variables    */
  int returnCount;      /* for naming returned lists     */

  /*
   * for managing temporary list storage
   */
  /* Set by the Obj wrappers for dl_set / dg_addExistingList: may the value
     argument be moved into a dyngroup, or must it be copied because someone
     else still holds it?  A value living in a variable is shared; a transient
     command result is not.  The list's own refcount cannot tell these apart,
     because both look like exactly one reference. */
  int absorbMayMove;

  /* Set once dl_clean has reset the name counter, after which a generated
     name can collide with a leftover hidden variable.  Until then names are
     unique and list creation can skip the check entirely -- it is on the
     hot path. */
  int namesRecycled;

  TMPLIST_STACK *TmpListStack;
  DYN_LIST *TmpListRecordList;
    
  
} DLSHINFO;

#ifdef __cplusplus
extern "C" {
#endif

extern DLLEXP int tclPutList(Tcl_Interp *interp, DYN_LIST *dl);
extern DLLEXP int tclPutGroup(Tcl_Interp *interp, DYN_GROUP *dg);
extern DLLEXP int tclDynListToTclList(Tcl_Interp *interp, DYN_LIST *dl);
extern DLLEXP int tclFindDynList (Tcl_Interp *i, char *name, DYN_LIST **dl);
extern DLLEXP int tclFindDynListInGroup (Tcl_Interp *interp, char *name, 
					    DYN_GROUP **dg, int *);
extern  DLLEXP int tclFindDynGroup(Tcl_Interp *interp,
				      char *name, DYN_GROUP **dg);

DYN_LIST *tclTclListToDynList(Tcl_Interp *interp, char *list);

int tclGetNumericRange(Tcl_Interp *interp, char *start, char *stop, DYN_LIST **);
int tclMakeListList(Tcl_Interp *interp, int n, char **names, DYN_LIST **dlp);

int Dm_Init(Tcl_Interp *interp);
int tclIsDynMatrix(Tcl_Interp *interp, DYN_LIST *dl);
int tclFindDynMatrix(Tcl_Interp *interp, char *name, DYN_LIST **dl);

void dynGroupChanDumpListNames(Tcl_Interp * interp, DYN_GROUP *dg,
	Tcl_Channel chan);
int dynGroupChanDumpSelectedListNames(Tcl_Interp * interp, DYN_GROUP *dg,
	DYN_LIST *cols, Tcl_Channel chan);
void dynGroupChanDump(Tcl_Interp * interp, DYN_GROUP *dg, char separator,
	Tcl_Channel chan);
int dynGroupChanDumpSelected(Tcl_Interp * interp, DYN_GROUP *dg,
	DYN_LIST *rows, DYN_LIST *cols, char separator, Tcl_Channel chan);
int dynListChanDump(Tcl_Interp * interp, DYN_LIST *dl, Tcl_Channel chan);
int dynListChanDumpMatrix(Tcl_Interp * interp, DYN_LIST *dl,
	Tcl_Channel chan, char separator);
int dynListChanDumpMatrixInCols(Tcl_Interp * interp, DYN_LIST *dl,
	Tcl_Channel chan, char separator);
int dynListChanDumpAsRow(Tcl_Interp * interp, DYN_LIST *dl, Tcl_Channel chan,
	char separator);
int dynListPrintValChan(Tcl_Interp * interp, DYN_LIST *dl, int i,
	Tcl_Channel chan);

#ifdef __cplusplus
}
#endif
