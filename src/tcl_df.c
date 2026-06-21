/*****************************************************************************
 *
 * NAME
 *      tcl_df.c
 *
 * PURPOSE
 *      Provides code for accessing data in df files from tcl
 *
 * AUTHOR
 *      DLS, JUL-95
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(SUN4) || defined(LYNX)
#include <unistd.h>
#endif

#include <tcl.h>
#include <df.h>
#include <dfana.h>
#include <labtcl.h>
#include <tcl_dl.h>

#include <utilc.h>


enum GET_TAGS { GET_EV, GET_EV_TIME, GET_EV_DATA, GET_SP, 
		  GET_EM, GET_EM_H, GET_EM_V };
/*
 * Local table for holding df file handles
 */

static Tcl_HashTable dfTable;	/* table of open data files             */

/*
 * Publically called functions
 */

int  Df_Init(Tcl_Interp *interp);
     
/*****************************************************************************
 *                           TCL Bound Functions 
 *****************************************************************************/


static int  tclDFHelp                 (ClientData, Tcl_Interp *, int, char **);
static int  tclOpenDataFile           (ClientData, Tcl_Interp *, int, char **);
static int  tclDataFileOpen           (ClientData, Tcl_Interp *, int, char **);
static int  tclCloseDataFile          (ClientData, Tcl_Interp *, int, char **);
static int  tclWriteDataFile          (ClientData, Tcl_Interp *, int, char **);
static int  tclGetDataFile            (ClientData, Tcl_Interp *, int, char **);
static int  tclSetDataFile            (ClientData, Tcl_Interp *, int, char **);
static int  tclObsGetDataFile         (ClientData, Tcl_Interp *, int, char **);

static int  tclGetDF                 (DATA_FILE *, Tcl_Interp *, int, char **);
static int  tclGetObs                (DATA_FILE *, Tcl_Interp *, int, char **);

static int  tclEvGetObs    (DATA_FILE *, int mode, Tcl_Interp *, int, char **);
static int  tclSpGetObs    (DATA_FILE *, int mode, Tcl_Interp *, int, char **);
static int  tclEmGetObs    (DATA_FILE *, int mode, Tcl_Interp *, int, char **);

static TCL_COMMANDS DFcommands[] = {
  { "df_open",           tclOpenDataFile,       (ClientData) NULL,
      "open a df file" },
  { "df_loaded",         tclDataFileOpen,       (ClientData) NULL,
      "returns 1 if a df file is loaded" },
  { "df_close",          tclCloseDataFile,      (ClientData) NULL,
      "close an open df file" },
  { "df_write",          tclWriteDataFile,      (ClientData) NULL,
      "write back an open df file" },
  { "df_get",            tclGetDataFile,        (ClientData) NULL,
      "get info about a df file" },
  { "df_set",            tclSetDataFile,        (ClientData) NULL,
      "set certain info in a df file" },
  { "df_evget",          tclObsGetDataFile,     (ClientData) GET_EV,
      "get event data from a df file" },
  { "df_evgetTime",      tclObsGetDataFile,     (ClientData) GET_EV_TIME,
      "get event data from a df file" },
  { "df_evgetData",      tclObsGetDataFile,     (ClientData) GET_EV_DATA,
      "get event data from a df file" },
  { "df_spget",          tclObsGetDataFile,     (ClientData) GET_SP,
      "get spike data from a df file" },
  { "df_emget",          tclObsGetDataFile,     (ClientData) GET_EM,
      "get eye movement data from a df file" },
  { "df_emgetHoriz",     tclObsGetDataFile,     (ClientData) GET_EM_H,
      "get horizontal eye movement data from a df file" },
  { "df_emgetVert",      tclObsGetDataFile,     (ClientData) GET_EM_V,
      "get vertical eye movement data from a df file" },
  { "df_help",             tclDFHelp,           (void *) DFcommands, 
      "display help" },
  { NULL, NULL, NULL, NULL }

};
/*****************************************************************************
 *
 * FUNCTION
 *    Df_Init
 *
 * ARGS
 *    Tcl_Inter *interp
 *
 * DESCRIPTION
 *    Creates tcl commands for opening, closing, and writing df files and
 * initializes a static df hash table.
 *
 *****************************************************************************/

int Df_Init(Tcl_Interp *interp)
{
  int i = 0;

  while (DFcommands[i].name) {
    Tcl_CreateCommand(interp, DFcommands[i].name,
		      (Tcl_CmdProc *) DFcommands[i].func, 
		      (ClientData) DFcommands[i].cd, 
		      (Tcl_CmdDeleteProc *) NULL);
    i++;
  }

  Tcl_InitHashTable(&dfTable, TCL_STRING_KEYS);

  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclDFHelp
 *
 * ARGS
 *    Tcl Args
 *
 * DESCRIPTION
 *    Loop through DFCommands and print out help messages
 *
 *****************************************************************************/

static int tclDFHelp (ClientData cd, Tcl_Interp *interp, 
                      int argc, char *argv[])
{
  TCL_COMMANDS *commands = (TCL_COMMANDS *) cd;
  int i = 0;

  while (commands[i].name) {
    printf("%-22s - %s\n", commands[i].name, commands[i].desc);
    i++;
  }
  return TCL_OK;
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclOpenDataFile
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    df_open
 *
 * DESCRIPTION
 *    Opens a df file and creates tcl command of the same name
 *
 *****************************************************************************/

static int tclOpenDataFile (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  FILE *fp;
  DATA_FILE *df;
  int newentry;
  Tcl_HashEntry *entryPtr;
  static char filebase[255];
  
  if (argc != 2) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("usage: df_open filename", -1));
    return TCL_ERROR;
  }

  file_basename(filebase, argv[1]);

  if ((entryPtr = Tcl_FindHashEntry(&dfTable, filebase))) {
    char resultstr[128];
    snprintf(resultstr, sizeof(resultstr),
	     "df_open: dffile %s already open", filebase);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
    return TCL_ERROR;
  }

  if (!(df = dfuCreateDataFile())) {
    Tcl_SetObjResult(interp,
		     Tcl_NewStringObj("df_open: error creating new df structure", -1));
    return TCL_ERROR;
  }

  if ((fp = fopen (argv[1], "rb")) == NULL) {
    char resultstr[128];
    snprintf(resultstr, sizeof(resultstr),
	    "df_open: file %s not found", argv[1]);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
    return TCL_ERROR;
  }

  if (!dfuFileToStruct(fp, df)) {
    char resultstr[128];
    snprintf(resultstr, sizeof(resultstr),
	     "df_open: file %s not recognized as df format", 
	     argv[1]);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(resultstr, -1));
    return TCL_ERROR;
  }
  fclose(fp);
  
  /*
   * Add to hash table which contains list of open df's
   */
  entryPtr = Tcl_CreateHashEntry(&dfTable, filebase, &newentry);
  Tcl_SetHashValue(entryPtr, df);
  
  Tcl_SetObjResult(interp, Tcl_NewStringObj(filebase, -1));
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclDataFileOpen
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    df_loaded
 *
 * DESCRIPTION
 *    Returns 1 if a df file is already loaded
 *
 *****************************************************************************/

static int tclDataFileOpen (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  static char filebase[255];
  Tcl_HashEntry *entryPtr;

  if (argc != 2) {
    Tcl_SetObjResult(interp,
		     Tcl_NewStringObj("usage: df_loaded dfhandle", -1));
    return TCL_ERROR;
  }

  file_basename(filebase, argv[1]);
  if ((entryPtr = Tcl_FindHashEntry(&dfTable, argv[1])) ||
      (entryPtr = Tcl_FindHashEntry(&dfTable, filebase))) {
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
  }
  Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclCloseDataFile
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    df_close
 *
 * DESCRIPTION
 *    Close and free a df file; remove tcl command and remove from hash table
 *
 *****************************************************************************/

static int tclCloseDataFile (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  DATA_FILE *df;
  static char filebase[255];
  Tcl_HashEntry *entryPtr;

  if (argc != 2) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("usage: df_close dfhandle", -1));
    return TCL_ERROR;
  }

  file_basename(filebase, argv[1]);
  if ((entryPtr = Tcl_FindHashEntry(&dfTable, argv[1])) ||
      (entryPtr = Tcl_FindHashEntry(&dfTable, filebase))) {
    Tcl_DeleteHashEntry(entryPtr);
    if ((df = Tcl_GetHashValue(entryPtr))) dfuFreeDataFile(df);
    return TCL_OK;
  }

  /* 
   * If user has specified "df_close ALL" iterate through the hash table
   * and recursively call this function to close all open df's
   */
  else if (!strcasecmp(argv[1],"ALL")) {
    Tcl_HashSearch search;
    for (entryPtr = Tcl_FirstHashEntry(&dfTable, &search);
	 entryPtr != NULL;
	 entryPtr = Tcl_NextHashEntry(&search)) {
      Tcl_VarEval(interp, "df_close ", Tcl_GetHashKey(&dfTable, entryPtr),
		  (char *) NULL);
    }
    return TCL_OK;
  }


  else {
    Tcl_AppendResult(interp, "df_close: dffile ", argv[1], " not found", 
		     (char *) NULL);
    return TCL_ERROR;
  }
}

/*****************************************************************************
 *
 * FUNCTION
 *    tclWriteDataFile
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    df_write
 *
 * DESCRIPTION
 *    Write a df file out to an ascii or binary file (or stdout)
 *
 *****************************************************************************/

static int tclWriteDataFile (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  DATA_FILE *df;
  Tcl_HashEntry *entryPtr;
  static char filebase[255];
  char *outfile = "";
  int format = DF_ASCII;

  if (argc < 2) {
    Tcl_SetObjResult(interp,
		     Tcl_NewStringObj("usage: df_write dfhandle [[ascii|binary] filename]", -1));
    return TCL_ERROR;
  }

  if (argc > 2) {
    if (!strncasecmp(argv[2],"asc",3)) format = DF_ASCII;
    if (!strncasecmp(argv[2],"bin",3)) format = DF_BINARY;
    else {
      format = DF_BINARY;
      outfile = argv[2];
    }
  }

  if (argc > 3) {
    outfile = argv[3];
  }

  file_basename(filebase, argv[1]);
  if ((entryPtr = Tcl_FindHashEntry(&dfTable, argv[1])) ||
      (entryPtr = Tcl_FindHashEntry(&dfTable, filebase))) {
    if ((df = Tcl_GetHashValue(entryPtr))) {
      dfInitBuffer();		           /* initialize df dump buffer      */
      dfRecordDataFile(df);                /* record the DATA_FILE structure */
      dfWriteBuffer(outfile, format);
      dfCloseBuffer();
    }
    return(TCL_OK);
  }
  else {
    Tcl_AppendResult(interp, "df_write: dffile ", argv[1], " not found", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclGetDataFile
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    df_get
 *
 * DESCRIPTION
 *    Gets info from a df file
 *
 * CALLS
 *    tclGetObs
 *    tclGetDF
 *
 *****************************************************************************/

static int tclGetDataFile (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DATA_FILE *df;
  Tcl_HashEntry *entryPtr;
  char filebase[255], *colon;

  /*
   * This allows calls of the form "df_get dfhandle:1 ..." as a shorthand
   * for df_get dfhandle obsinfo 1 ...
   */
  if (argc > 1 && strchr(argv[1],':')) {
    if ((colon = strchr(argv[1],':'))) {
      strncpy(filebase, argv[1], colon-argv[1]);
      filebase[colon-argv[1]] = 0;

      if ((entryPtr = Tcl_FindHashEntry(&dfTable, filebase))) {
	if (!(df = Tcl_GetHashValue(entryPtr))) {
	  Tcl_AppendResult(interp, "df_get: dffileptr ", filebase, 
			   " not found", (char *) NULL);
	  return TCL_ERROR;
	}
      }
      else {
	Tcl_AppendResult(interp, "df_get: dffile ", filebase, " not found", 
			 (char *) NULL);
	return TCL_ERROR;
      }
      strncpy(argv[1], colon+1, strlen(colon+1));
      argv[1][strlen(colon+1)] = 0;

      return(tclGetObs(df, interp, argc, argv));
    }
  }

  /*
   * If "df_get list" is specified, then a list of all open df files
   * is returned
   */
  else if (argc > 1 && !strcasecmp(argv[1],"LIST")) {
    Tcl_DString dfList;
    Tcl_HashSearch search;

    Tcl_DStringInit(&dfList);

    for (entryPtr = Tcl_FirstHashEntry(&dfTable, &search);
	 entryPtr != NULL;
	 entryPtr = Tcl_NextHashEntry(&search)) {
      Tcl_DStringAppendElement(&dfList, Tcl_GetHashKey(&dfTable, entryPtr));
    }
    Tcl_DStringResult(interp, &dfList);
    return TCL_OK;
  }


  /* 
   * Otherwise make sure there are at least 3 args and that the third matches
   * either "df" or "obs".
   */

  else if (argc < 2) {
    Tcl_SetObjResult(interp,
		     Tcl_NewStringObj("usage: df_get dfhandle[:obsp] ...", -1));
    return TCL_ERROR;
  }

  
  file_basename(filebase, argv[1]);
  if ((entryPtr = Tcl_FindHashEntry(&dfTable, argv[1])) ||
      (entryPtr = Tcl_FindHashEntry(&dfTable, filebase))) {
    if (!(df = Tcl_GetHashValue(entryPtr))) {
      Tcl_AppendResult(interp, "df_get: dffile ", argv[1], " not found", 
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  else {
    Tcl_AppendResult(interp, "df_get: dffile ", argv[1], " not found", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  return(tclGetDF(df, interp, argc-1, &argv[1]));
}


static int tclGetDF(DATA_FILE *df, Tcl_Interp *interp, int argc, char *argv[])
{
  static char *usage_message = "usage:\tdf_get dfhandle dfinfo"
    " { FILENAME, TIME, FILENUM,\n"
    "\tCOMMENT, NAUXFILES, AUXFILE, EXP,\n"
    "\tTESTMODE, NSTIMTYPES, NOBSP }";

  if (argc < 2) {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }

  if (!df) {
    Tcl_SetResult(interp, "df_get: bad df pointer", TCL_STATIC);
    return TCL_ERROR;
  }

  /* strings */
  if (!strncasecmp(argv[1], "FILENA", 6))
    Tcl_SetResult(interp, DF_FILENAME(DF_DFINFO(df)), TCL_VOLATILE);
  else if (!strncasecmp(argv[1], "COM", 3))
    Tcl_SetResult(interp, DF_COMMENT(DF_DFINFO(df)), TCL_VOLATILE);
  else if (!strncasecmp(argv[1], "AUX", 3)) {
    int index, naux = DF_NAUXFILES(DF_DFINFO(df));
    if (argc < 3) {
      Tcl_SetResult(interp, "df_get: No AUXFILE index specified", TCL_STATIC);
      return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[2], &index) != TCL_OK) return TCL_ERROR;
    if (index >= naux) {
      Tcl_SetResult(interp, "df_get: invalid auxfile id", TCL_STATIC);
      return TCL_ERROR;
    }
    if (DF_AUXFILE(DF_DFINFO(df),index))
      Tcl_SetResult(interp, DF_AUXFILE(DF_DFINFO(df),index), TCL_STATIC);
  }

  /* ints */
  else if (!strncasecmp(argv[1], "TIM", 3))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(DF_TIME(DF_DFINFO(df))));
  else if (!strncasecmp(argv[1], "FILENU", 6))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(DF_FILENUM(DF_DFINFO(df))));
  else if (!strncasecmp(argv[1], "EXP", 3))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(DF_EXPERIMENT(DF_DFINFO(df))));
  else if (!strncasecmp(argv[1], "TES", 3))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(DF_TESTMODE(DF_DFINFO(df))));
  else if (!strncasecmp(argv[1], "NST", 3))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(DF_NSTIMTYPES(DF_DFINFO(df))));
  else if (!strncasecmp(argv[1], "NOB", 3))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(DF_NOBSP(df)));
  else if (!strncasecmp(argv[1], "NAUX", 4))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(DF_NAUXFILES(DF_DFINFO(df))));

  /* mismatch */
  else {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}


static int tclGetObs(DATA_FILE *df, Tcl_Interp *interp, int argc, char *argv[])
{
  int index;
  OBS_P *o;

  static char *usage_message = "usage:\tdf_get dfhandle obsinfo obsnum "
    "{ FILENUM, INDEX, BLOCK, OBSP,\n"
    "\tSTATUS, DURATION, NTRIALS }\n";

  if (argc < 3) {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }

  if (Tcl_GetInt(interp, argv[1], &index) != TCL_OK) {
    return TCL_ERROR;
  }
  if (index >= DF_NOBSP(df) || index < 0) {
    Tcl_SetResult(interp, "df_get: obsp index out of range", TCL_STATIC);
    return TCL_ERROR;
  }
  
  o = DF_OBSP(df, index);
  if (!o) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("df_get: bad obsp pointer", -1));
    return TCL_ERROR;
  }

  /* ints */
  if (!strncasecmp(argv[2], "FIL", 3))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(OBS_FILENUM(OBSP_INFO(o))));
  else if (!strncasecmp(argv[2], "IND", 3))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(OBS_INDEX(OBSP_INFO(o))));
  else if (!strncasecmp(argv[2], "BLO", 3))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(OBS_BLOCK(OBSP_INFO(o))));
  else if (!strncasecmp(argv[2], "OBS", 3))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(OBS_OBSP(OBSP_INFO(o))));
  else if (!strncasecmp(argv[2], "DUR", 3))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(OBS_DURATION(OBSP_INFO(o))));
  else if (!strncasecmp(argv[2], "NTR", 3))
    Tcl_SetObjResult(interp, Tcl_NewIntObj(OBS_NTRIALS(OBSP_INFO(o))));
  else if (!strncasecmp(argv[2], "STA", 3) ||
	   !strncasecmp(argv[2], "EOT", 3)) {
    Tcl_SetObjResult(interp, Tcl_NewIntObj(OBS_STATUS(OBSP_INFO(o))));
  }

  /* mismatch */
  else {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }
  
  return TCL_OK;
}


/*****************************************************************************
 *
 * FUNCTION
 *    tclObsGetDataFile
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    df_evget
 *    df_spget
 *    df_emget
 *
 * DESCRIPTION
 *    Driver function for getting observation period based df data
 *
 * CALLS
 *    tclEvGetObs
 *    tclSpGetObs
 *    tclEmGetObs
 *
 *****************************************************************************/

static int tclObsGetDataFile (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  DATA_FILE *df;
  Tcl_HashEntry *entryPtr;
  char filebase[255], *colon;
  int operation = (Tcl_Size) data;

  /*
   * This allows calls of the form "df_??get dfhandle:1 ..." as a shorthand
   * for df_get dfhandle obsinfo 1 ...
   */
  if (argc > 1 && strchr(argv[1],':')) {
    int i, retval;
    char **newargv;
    if ((colon = strchr(argv[1],':'))) {
      strncpy(filebase, argv[1], colon-argv[1]);
      filebase[colon-argv[1]] = 0;
      
      if ((entryPtr = Tcl_FindHashEntry(&dfTable, filebase)))
	df = Tcl_GetHashValue(entryPtr);

      else {
	Tcl_AppendResult(interp, argv[0], ": dffile \"", argv[1], 
			 "\" not found", 
			 (char *) NULL);
	return TCL_ERROR;
      }
      
      newargv = (char **) calloc(argc, sizeof (char *));
      for (i = 0; i < argc; i++) newargv[i] = strdup(argv[i]);
      
      strncpy(newargv[1], colon+1, strlen(colon+1));
      newargv[1][strlen(colon+1)] = 0;
      switch (operation) {
      case GET_EV:
      case GET_EV_TIME:
      case GET_EV_DATA:
	retval = tclEvGetObs(df, operation, interp, argc, newargv);
	for (i = 0; i < argc; i++) free(newargv[i]);
	free(newargv);
	return retval;
	break;
      case GET_SP:
	retval = tclSpGetObs(df, operation, interp, argc, newargv);
	for (i = 0; i < argc; i++) free(newargv[i]);
	free(newargv);
	return retval;
	break;
      case GET_EM:
      case GET_EM_H:
      case GET_EM_V:
	retval = tclEmGetObs(df, operation, interp, argc, newargv);
	for (i = 0; i < argc; i++) free(newargv[i]);
	free(newargv);
	return retval;
	break;
      default:
	return TCL_OK;
	break;
      }
    }
  }
  
  /* 
   * Otherwise make sure there are at least 3 args
   */
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " dfhandle obsindex ...",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  file_basename(filebase, argv[1]);
  if ((entryPtr = Tcl_FindHashEntry(&dfTable, argv[1])) ||
      (entryPtr = Tcl_FindHashEntry(&dfTable, filebase))) {
    df = Tcl_GetHashValue(entryPtr);
  }
  else {
    Tcl_AppendResult(interp, argv[0], ": dffile \"", argv[1], 
		     "\" not found", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  switch (operation) {
  case GET_EV:
  case GET_EV_TIME:
  case GET_EV_DATA:
    return(tclEvGetObs(df, operation, interp, argc-1, &argv[1]));
    break;
  case GET_SP:
    return(tclSpGetObs(df, operation, interp, argc-1, &argv[1]));
    break;
  case GET_EM:
  case GET_EM_H:
  case GET_EM_V:
    return(tclEmGetObs(df, operation, interp, argc-1, &argv[1]));
    break;
  default:
    return TCL_OK;
    break;
  }
}


static int tclEvGetObs(DATA_FILE *df, int mode, 
		       Tcl_Interp *interp, int argc, char *argv[])
{
  int index, status, i, j;
  OBS_P *o;

  static char *usage_message = 
    "usage: df_evget dfhandle{:| }obsindex lookupfunc ev_tag ...";
  static char *lfunc_message = 
    "df_evget event lookup functions:\n"
    "  Data                   tag           -> all data for tag\n"
    "  Times                  tag           -> all times for tag\n" 
    "  TimeWithData           tag data      -> evdata and time of tag event\n"
    "  DataAtTime             tag time      -> evdata for tag at time\n"
    "  DataAfterTime          tag time      -> evdata for tag after time\n"
    "  DataAtOrAfterTime      tag time      -> evdata for tag at/after time\n"
    "  DataBeforeTime         tag time      -> evdata for tag before time\n"
    "  DataAtOrBeforeTime     tag time      -> "
      "evdata for tag at/before time\n"
    "  WithDataAtTime         tag data time -> "
      "evdata for tag with data at time\n"
    "  WithDataAfterTime      tag data time -> "
      "evdata for tag with data after time\n"
    "  WithDataAtOrAfterTime  tag data time -> "
      "evdata for tag with data at/after time\n"
    "  WithDataBeforeTime     tag data time -> "
      "evdata for tag with data before time\n"
    "  WithDataAtOrBeforeTime tag data time -> "
      "evdata for tag with data at/before time\n"
    "  WithNotDataAtTime         tag data time -> "
      "evdata for tag without data at time\n"
    "  WithNotDataAfterTime      tag data time -> "
      "evdata for tag without data after time\n"
    "  WithNotDataAtOrAfterTime  tag data time -> "
      "evdata for tag without data at/after time\n"
    "  WithNotDataBeforeTime     tag data time -> "
      "evdata for tag without data before time\n"
    "  WithNotDataAtOrBeforeTime tag data time -> "
      "evdata for tag without data at/before time";

  if (argc < 3) {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }
  else if (argc < 4) {
    Tcl_SetResult(interp, lfunc_message, TCL_STATIC);
    return TCL_ERROR;
  }

  /*
   * Get a pointer to the obs period
   */
  if (Tcl_GetInt(interp, argv[1], &index) != TCL_OK) {
    return TCL_ERROR;
  }
  if (index >= DF_NOBSP(df) || index < 0) {
    Tcl_SetResult(interp, "df_evget: obsp index out of range", TCL_STATIC);
    return TCL_ERROR;
  }
  
  o = DF_OBSP(df, index);
  if (!o) {
    Tcl_SetResult(interp, "df_evget: bad obsp pointer", TCL_STATIC);
    return TCL_ERROR;
  }

  /* 
   * Lookup functions 
   */

  /***
   ***  Times:  returns all times for tag
   ***  Data: returns all data for tag
   ***/
  if ((!strcasecmp(argv[2], "Times") && (status = 1)) ||
      (!strcasecmp(argv[2], "Data") && (status = 2))) {
    int tag, nparams;
    EV_LIST *evlist;
    char numstr[32];
    Tcl_DString resultStr;
    
    if (!(evGetTagID(argv[3], &tag))) goto BadTag;

    evlist = evGetList(o, tag, &nparams);
    if (!evlist) {
      Tcl_SetResult(interp, "df_evget: evlist not found", TCL_STATIC);
      return TCL_ERROR;
    }
    
    Tcl_DStringInit(&resultStr);
    for (i = 0 ; i < EV_LIST_N(evlist)/nparams; i++) {
      if (status == 1) {                     /* TIMES */
	snprintf(numstr, sizeof(numstr), "%d", EV_LIST_TIME(evlist,i));
	Tcl_DStringAppendElement(&resultStr, numstr);
      }
      else {                                 /* VALS  */
	for (j = 0; j < nparams; j++) {
	  snprintf(numstr, sizeof(numstr), "%d", EV_LIST_VAL(evlist, i*nparams+j));
	  Tcl_DStringAppendElement(&resultStr, numstr);
	}
      }
    }
    Tcl_DStringResult(interp, &resultStr);
    return TCL_OK;
  }


  /***
   ***  TimeWithData: returns time and evdata when tag with data occured
   ***/
  if (!strncasecmp(argv[2], "TimeWithData", 12)) {
    int tag, result, data, time, evdata[5];
    if (argc < 5) {
      Tcl_SetObjResult(interp,
		       Tcl_NewStringObj("df_evget [TimeWithData]: no data specified", -1));
      return TCL_ERROR;
    }
    if (!(evGetTagID(argv[3],&tag))) goto BadTag;
    if (Tcl_GetInt(interp, argv[4], &data) != TCL_OK) {
      Tcl_SetObjResult(interp,
		       Tcl_NewStringObj("df_evget [TimeWithData]: bad data specified", -1));;
      return TCL_ERROR;
    }
    if ((result = evGetTimeWithData(o, tag, data, evdata, &time))) {
      char numstr[32];
      int i;
      Tcl_DString resultStr;
      Tcl_DStringInit(&resultStr);

      if (mode == GET_EV_TIME || mode == GET_EV) {
	snprintf(numstr, sizeof(numstr), "%d", time);
	Tcl_DStringAppendElement(&resultStr, numstr);
      }
      
      if (mode == GET_EV_DATA || mode == GET_EV) {
	for (i = 0; i < result; i++) {
	  snprintf(numstr, sizeof(numstr), "%d", evdata[i]);
	  Tcl_DStringAppendElement(&resultStr, numstr);
	}
      }
      Tcl_DStringResult(interp, &resultStr);
    }
    return TCL_OK;
  }

  /***
   ***  WithDataAtTime: returns evdata when tag with data & time occured
   ***  WithNotDataAtTime: returns evdata when tag without data & time occured
   ***/
  if ((!strcasecmp(argv[2], "WithDataAtTime") && (status = 1)) ||
      (!strcasecmp(argv[2], "WithNotDataAtTime") && (status = 2))) {
    int tag, result = 0, data, time, evdata[5];
    if (argc < 6) {
      Tcl_AppendResult(interp, "df_evget [", argv[2], 
		       "]: must specify data and time", (char *) NULL);
      return TCL_ERROR;
    }
    if (!(evGetTagID(argv[3],&tag))) goto BadTag;
    if (Tcl_GetInt(interp, argv[4], &data) != TCL_OK) {
      Tcl_AppendResult(interp, "df_evget [", argv[2], 
		       "]: bad data specified", (char *) NULL);
      return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[5], &time) != TCL_OK) {
      Tcl_AppendResult(interp, "df_evget [", argv[2], 
		       "]: bad time specified", (char *) NULL);
      return TCL_ERROR;
    }

    if (status == 1)
      result = evGetWithDataAtTime(o, tag, data, time, evdata);
    else if (status == 2)
      result = evGetWithNotDataAtTime(o, tag, data, time, evdata);      
    if (result) {
      char numstr[32];
      int i;
      Tcl_DString resultStr;
      Tcl_DStringInit(&resultStr);
      
      if (mode == GET_EV_TIME || mode == GET_EV) {
	snprintf(numstr, sizeof(numstr), "%d", time);
	Tcl_DStringAppendElement(&resultStr, numstr);
      }

      if (mode == GET_EV_DATA || mode == GET_EV) {
	for (i = 0; i < result; i++) {
	  snprintf(numstr, sizeof(numstr), "%d", evdata[i]);
	  Tcl_DStringAppendElement(&resultStr, numstr);
	}
      }
      Tcl_DStringResult(interp, &resultStr);
    }
    return TCL_OK;
  }

  /***
   ***  DataAtTime: returns evdata for tag occuring at time
   ***/
  if (!strcasecmp(argv[2], "DataAtTime")) {
    int tag, result = 0, time, evdata[5];
    if (argc < 5) {
      Tcl_SetObjResult(interp, Tcl_NewStringObj("df_evget [DataAtTime]: must specify data and time", -1));
      return TCL_ERROR;
    }
    if (!(evGetTagID(argv[3],&tag))) goto BadTag;
    if (Tcl_GetInt(interp, argv[4], &time) != TCL_OK) {
      Tcl_SetObjResult(interp,
		       Tcl_NewStringObj("df_evget [DataAtTime]: bad time specified", -1));
      return TCL_ERROR;
    }
    if ((result = evGetDataAtTime(o, tag, time, evdata))) {
      char numstr[32];
      int i;
      Tcl_DString resultStr;
      Tcl_DStringInit(&resultStr);

      if (mode == GET_EV_TIME || mode == GET_EV) {
	snprintf(numstr, sizeof(numstr), "%d", time);
	Tcl_DStringAppendElement(&resultStr, numstr);
      }
      
      if (mode == GET_EV_DATA || mode == GET_EV) {
	for (i = 0; i < result; i++) {
	  snprintf(numstr, sizeof(numstr), "%d", evdata[i]);
	  Tcl_DStringAppendElement(&resultStr, numstr);
	}
      }
      Tcl_DStringResult(interp, &resultStr);
    }
    return TCL_OK;
  }


  /***
   ***  DataAtOrAfterTime: returns evdata for tag occuring at/after time
   ***  DataAfterTime: returns evdata for tag occuring after time
   ***  DataAtOrBeforeTime: returns evdata for tag occuring at/before time
   ***  DataBeforeTime: returns evdata for tag occuring before time
   ***/
  if ((!strcasecmp(argv[2], "DataAtOrAfterTime") && (status = 1)) ||
      (!strcasecmp(argv[2], "DataAfterTime") && (status = 2)) ||
      (!strcasecmp(argv[2], "DataAtOrBeforeTime") && (status = 3)) ||
      (!strcasecmp(argv[2], "DataBeforeTime") && (status = 4))) {
    int tag, result = 0, time, actualtime, evdata[5];

    if (argc < 5) {
      Tcl_AppendResult(interp, "df_evget [", argv[2], "]: must specify time",
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (!(evGetTagID(argv[3],&tag))) goto BadTag;
    if (Tcl_GetInt(interp, argv[4], &time) != TCL_OK) {
      Tcl_AppendResult(interp, "df_evget [", argv[2], "]: bad time specified",
		       (char *) NULL);
      return TCL_ERROR;
    }
    switch (status) {
    case 1: 
      result = evGetDataAtOrAfterTime(o, tag, time, evdata, &actualtime);
      break;
    case 2: 
      result = evGetDataAfterTime(o, tag, time, evdata, &actualtime);
      break;
    case 3: 
      result = evGetDataAtOrBeforeTime(o, tag, time, evdata, &actualtime);
      break;
    case 4: 
      result = evGetDataBeforeTime(o, tag, time, evdata, &actualtime);
      break;
    }
    if (result) {
      char numstr[32];
      int i;
      Tcl_DString resultStr;
      Tcl_DStringInit(&resultStr);

      if (mode == GET_EV_TIME || mode == GET_EV) {
	snprintf(numstr, sizeof(numstr), "%d", actualtime);
	Tcl_DStringAppendElement(&resultStr, numstr);
      }
      
      if (mode == GET_EV_DATA || mode == GET_EV) {
	for (i = 0; i < result; i++) {
	  snprintf(numstr, sizeof(numstr), "%d", evdata[i]);
	  Tcl_DStringAppendElement(&resultStr, numstr);
	}
      }
      Tcl_DStringResult(interp, &resultStr);
    }
    return TCL_OK;
  }

  /***
   ***  DataAtOrAfterTime: returns evdata for tag occuring at/after time
   ***  DataAfterTime: returns evdata for tag occuring after time
   ***  DataAtOrBeforeTime: returns evdata for tag occuring at/before time
   ***  DataBeforeTime: returns evdata for tag occuring before time
   ***/
  if ((!strcasecmp(argv[2], "WithDataAtOrAfterTime") && (status = 1)) ||
      (!strcasecmp(argv[2], "WithDataAfterTime") && (status = 2)) ||
      (!strcasecmp(argv[2], "WithDataAtOrBeforeTime") && (status = 3)) ||
      (!strcasecmp(argv[2], "WithDataBeforeTime") && (status = 4)) ||
      (!strcasecmp(argv[2], "WithNotDataAtOrAfterTime") && (status = 5)) ||
      (!strcasecmp(argv[2], "WithNotDataAfterTime") && (status = 6)) ||
      (!strcasecmp(argv[2], "WithNotDataAtOrBeforeTime") && (status = 7)) ||
      (!strcasecmp(argv[2], "WithNotDataBeforeTime") && (status = 8))) {
    int tag, result = 0, time, data, actualtime, evdata[5];

    if (argc < 6) {
      Tcl_AppendResult(interp, "df_evget [", argv[2], 
		       "]: must specify data and time", (char *) NULL);
      return TCL_ERROR;
    }
    if (!(evGetTagID(argv[3],&tag))) goto BadTag;
    if (Tcl_GetInt(interp, argv[4], &data) != TCL_OK) {
      Tcl_AppendResult(interp, "df_evget [", argv[2], "]: bad data specified",
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[5], &time) != TCL_OK) {
      Tcl_AppendResult(interp, "df_evget [", argv[2], "]: bad time specified",
		       (char *) NULL);
      return TCL_ERROR;
    }
    switch (status) {
    case 1: 
      result = evGetWithDataAtOrAfterTime(o, tag, data, time, 
					  evdata, &actualtime);
      break;
    case 2: 
      result = evGetWithDataAfterTime(o, tag, data, time, 
				      evdata, &actualtime);
      break;
    case 3: 
      result = evGetWithDataAtOrBeforeTime(o, tag, data, time, 
					   evdata, &actualtime);
      break;
    case 4: 
      result = evGetWithDataBeforeTime(o, tag, data, time, 
				       evdata, &actualtime);
      break;
    case 5: 
      result = evGetWithNotDataAtOrAfterTime(o, tag, data, time, 
					     evdata, &actualtime);
      break;
    case 6: 
      result = evGetWithNotDataAfterTime(o, tag, data, time, 
					 evdata, &actualtime);
      break;
    case 7: 
      result = evGetWithNotDataAtOrBeforeTime(o, tag, data, time, 
					      evdata, &actualtime);
      break;
    case 8: 
      result = evGetWithNotDataBeforeTime(o, tag, data, time, 
					  evdata, &actualtime);
      break;
    }
    if (result) {
      char numstr[32];
      int i;
      Tcl_DString resultStr;
      Tcl_DStringInit(&resultStr);

      if (mode == GET_EV_TIME || mode == GET_EV) {
	snprintf(numstr, sizeof(numstr), "%d", actualtime);
	Tcl_DStringAppendElement(&resultStr, numstr);
      }

      if (mode == GET_EV_DATA || mode == GET_EV) {
	for (i = 0; i < result; i++) {
	  snprintf(numstr, sizeof(numstr), "%d", evdata[i]);
	  Tcl_DStringAppendElement(&resultStr, numstr);
	}
      }
      Tcl_DStringResult(interp, &resultStr);
    }
    return TCL_OK;
  }

  /* mismatch */
  else {
    Tcl_SetResult(interp, lfunc_message, TCL_STATIC);
    return TCL_ERROR;
  }

 BadTag:
  Tcl_SetObjResult(interp, Tcl_NewStringObj("df_evget: bad tag specified", -1));
  return TCL_ERROR;
}




static int tclSpGetObs(DATA_FILE *df, int mode,
		       Tcl_Interp *interp, int argc, char *argv[])
{
  DYN_LIST *slist;
  int index, start, stop, offset;
  OBS_P *o;
  
  static char *usage_message = 
    "usage: df_spget dfhandle{:| }obsindex start stop offset";

  if (argc < 4) {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }

  if (Tcl_GetInt(interp, argv[1], &index) != TCL_OK) {
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[2], &start) != TCL_OK) {
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[3], &stop) != TCL_OK) {
    return TCL_ERROR;
  }
  if (argc > 4) {
    if (Tcl_GetInt(interp, argv[4], &offset) != TCL_OK) {
      return TCL_ERROR;
    }
  }
  else {
    offset = start;
  }

  /*
   * Get a pointer to the obs period
   */
  if (index >= DF_NOBSP(df) || index < 0) {
    Tcl_SetResult(interp, "df_spget: obsp index out of range", TCL_STATIC);
    return TCL_ERROR;
  }
  o = DF_OBSP(df, index);
  if (!o) {
    Tcl_SetResult(interp, "df_spget: bad obsp pointer", TCL_STATIC);
    return TCL_ERROR;
  }

  if (!(slist = evGetSpikes(o, start, stop, offset))) {
    Tcl_SetResult(interp, "df_spget: error retrieving spike data", TCL_STATIC);
    return TCL_ERROR;
  }
  
  return(tclPutList(interp, slist));
}

static int tclEmGetObs(DATA_FILE *df, int mode,
		       Tcl_Interp *interp, int argc, char *argv[])
{
  DYN_LIST *emdata;
  int index, start, stop;
  int *fixstartp = NULL, *fixstopp = NULL, fixstart, fixstop;
  OBS_P *o;
  
  static char *usage_message = 
    "usage: df_emget df{:| }index {start stop|mean|wind|size} [{fixstart fixstop}]";

  if (argc < 3) {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }

  if (Tcl_GetInt(interp, argv[1], &index) != TCL_OK) {
    return TCL_ERROR;
  }

  /*
   * Get a pointer to the obs period
   */
  if (index >= DF_NOBSP(df) || index < 0) {
    Tcl_SetResult(interp, "df_emget: obsp index out of range", TCL_STATIC);
    return TCL_ERROR;
  }
  o = DF_OBSP(df, index);
  if (!o) {
    Tcl_SetResult(interp, "df_emget: bad obsp pointer", TCL_STATIC);
    return TCL_ERROR;
  }

  /* Get Default Mean EM Position */
  if (argc == 3 && !strncasecmp(argv[2],"MEAN",4)) {
    float hmean, vmean;
    if (!evGetMeanEMPositions(o, &hmean, &vmean, NULL, NULL)) {
      Tcl_SetResult(interp, "df_emget: error retrieving em data", TCL_STATIC);
      return TCL_ERROR;
    }
    else {
      Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
      Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(hmean));
      Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(vmean));
      Tcl_SetObjResult(interp, listPtr);
      return TCL_OK;
    }
  }

  if (argc == 3 && !strncasecmp(argv[2],"WINS",4)) {
    float scale = EM_PNT_DEG(OBSP_EMDATA(o))*2.0;
    Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(interp, listPtr,
			     Tcl_NewDoubleObj((EM_WINDOW(OBSP_EMDATA(o))[2]-
					       EM_WINDOW(OBSP_EMDATA(o))[0])/
					      scale));
    Tcl_ListObjAppendElement(interp, listPtr,
			     Tcl_NewDoubleObj((EM_WINDOW(OBSP_EMDATA(o))[3]-
					       EM_WINDOW(OBSP_EMDATA(o))[1])/
					      scale));
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
  }

  if (argc == 3 && !strncasecmp(argv[2],"WIND",4)) {
    Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(interp, listPtr,
			     Tcl_NewIntObj(EM_WINDOW(OBSP_EMDATA(o))[0]));
    Tcl_ListObjAppendElement(interp, listPtr,
			     Tcl_NewIntObj(EM_WINDOW(OBSP_EMDATA(o))[1]));
    Tcl_ListObjAppendElement(interp, listPtr,
			     Tcl_NewIntObj(EM_WINDOW(OBSP_EMDATA(o))[2]));
    Tcl_ListObjAppendElement(interp, listPtr,
			     Tcl_NewIntObj(EM_WINDOW(OBSP_EMDATA(o))[3]));
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
  }

  /* Get Specified Mean EM Position */
  if (argc == 5 && !strncasecmp(argv[2],"MEAN",4)) {
    float hmean, vmean;
    if (Tcl_GetInt(interp, argv[3], &start) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[4], &stop) != TCL_OK)  return TCL_ERROR;
    if (!evGetMeanEMPositions(o, &hmean, &vmean, &start, &stop)) {
      Tcl_SetResult(interp, "df_emget: error finding em mean pos", TCL_STATIC);
      return TCL_ERROR;
    }
    else {
      Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
      Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(hmean));
      Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(vmean));
      Tcl_SetObjResult(interp, listPtr);
      return TCL_OK;
    }
  }
  
  
  if (argc < 4) {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[2], &start) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &stop) != TCL_OK)  return TCL_ERROR;
  
  if (argc > 5) {
    if (Tcl_GetInt(interp, argv[4], &fixstart) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[5], &fixstop) != TCL_OK)  return TCL_ERROR;
    fixstartp = &fixstart;
    fixstopp = &fixstop;
  }
  
  switch (mode) {
  case GET_EM:     mode = EM_ALL;      break;
  case GET_EM_H:   mode = EM_HORIZ;    break;
  case GET_EM_V:   mode = EM_VERT;     break;
  }
  if (!(emdata = evGetEMData(o, start, stop, fixstartp, fixstopp, mode))) {
    Tcl_SetResult(interp, "df_emget: error retrieving em data", TCL_STATIC);
    return TCL_ERROR;
  }
  return(tclPutList(interp, emdata));
}



/*****************************************************************************
 *
 * FUNCTION
 *    tclSetDataFile
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    df_set
 *
 * DESCRIPTION
 *    Sets info from a df file
 *
 * CALLS
 *    tclSetDF
 *
 *****************************************************************************/

static int tclSetDataFile (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DATA_FILE *df;
  Tcl_HashEntry *entryPtr;
  char filebase[255];

  static char *usage_message = "usage:\tdf_set dfhandle TAG value\n"
    "TAGS = { FILENAME, FILENUM, COMMENT, EXP, TESTMODE, NSTIMTYPES }\n";

  if (argc < 4) {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }

  file_basename(filebase, argv[1]);
  if ((entryPtr = Tcl_FindHashEntry(&dfTable, argv[1])) ||
      (entryPtr = Tcl_FindHashEntry(&dfTable, filebase))) {
    df = Tcl_GetHashValue(entryPtr);
  }
  else {
    Tcl_AppendResult(interp, "df_set: dffile ", argv[1], " not found", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /* strings */
  if (!strncasecmp(argv[2], "FILENA", 6)) {
    if (DF_FILENAME(DF_DFINFO(df))) free((void *) DF_FILENAME(DF_DFINFO(df)));
    DF_FILENAME(DF_DFINFO(df)) = (char *) malloc(strlen(argv[3])+1);
    strcpy(DF_FILENAME(DF_DFINFO(df)), argv[3]);
    return TCL_OK;
  }

  else if (!strncasecmp(argv[2], "COM", 3)) {
    if (DF_COMMENT(DF_DFINFO(df))) free((void *) DF_COMMENT(DF_DFINFO(df)));
    DF_COMMENT(DF_DFINFO(df)) = (char *) malloc(strlen(argv[3])+1);
    strcpy(DF_COMMENT(DF_DFINFO(df)), argv[3]);
    return TCL_OK;
  }

  else if (!strncasecmp(argv[2], "AUX", 3)) {
    int i, index, naux = DF_NAUXFILES(DF_DFINFO(df));
    char **newptr;
    if (argc < 5) {
      Tcl_SetResult(interp, "usage: dl_set auxfile index text", TCL_STATIC);
      return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[3], &index) != TCL_OK) return TCL_ERROR;
    if (index >= naux) {
      newptr = (char **) calloc(index+1, sizeof(char **));
      for (i = 0; i < naux; i++) {
	newptr[i] = DF_AUXFILE(DF_DFINFO(df),i);
      }
      if (DF_AUXFILES(DF_DFINFO(df))) 
	free((void *) DF_AUXFILES(DF_DFINFO(df)));
      DF_AUXFILES(DF_DFINFO(df)) = newptr;
      DF_NAUXFILES(DF_DFINFO(df)) = index+1;
    }
    if (DF_AUXFILE(DF_DFINFO(df),index)) {
      free((void *)(DF_AUXFILE(DF_DFINFO(df),index)));
    }
    DF_AUXFILE(DF_DFINFO(df),index) = (char *) malloc(strlen(argv[4])+1);
    strcpy(DF_AUXFILE(DF_DFINFO(df),index), argv[4]);
    return TCL_OK;
  }

  /* ints */
  else {
    int val, *valptr;

    if (!strncasecmp(argv[2], "FILENU", 6))
      valptr = &DF_FILENUM(DF_DFINFO(df));
    else if (!strncasecmp(argv[2], "EXP", 3))
      valptr = &DF_EXPERIMENT(DF_DFINFO(df));
    else if (!strncasecmp(argv[2], "TES", 3))
      valptr = &DF_TESTMODE(DF_DFINFO(df));
    else if (!strncasecmp(argv[2], "NST", 3))
      valptr = &DF_NSTIMTYPES(DF_DFINFO(df));
    else {
      Tcl_SetResult(interp, usage_message, TCL_STATIC);
      return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[3], &val) != TCL_OK) return TCL_ERROR;
    *valptr = val;
    return TCL_OK;
  }
}
