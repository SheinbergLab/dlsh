#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <tcl.h>
#include <bink.h>
#include "lodepng.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#pragma warning (disable:4244)
#pragma warning (disable:4305)
#define DllEntryPoint DllMain
#define EXPORT(a,b) __declspec(dllexport) a b
#else
#define DllEntryPoint
#define EXPORT a b
#include <unistd.h>
#endif

static Tcl_HashTable BinkTable;
static int binkCount = 0;		/* increasing count of binkfiles  */

static int tclBinkCreateCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkDestroyCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkInfoCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkGotoCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkWaitCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkNextFrameCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkGetFramesCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkGetFrameCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkGetFramePNGCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkGetFrameNumCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkSetSoundOnOffCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkSetFrameRateCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);
static int tclBinkSetSoundTrackCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *const o[]);

typedef struct {
  HBINK bink;
  unsigned char *buffer;
} BINKOBJ;

typedef  struct {
	char *cmd;
	int (*f)();
	int minargs, maxargs;
	int subcmds;
	char *usage;
} cmdOptions;

static cmdOptions subcmdVec[] = {
  {"info",	        tclBinkInfoCmd,		        1, 1,	0,
   "handle"},

  {"open",              tclBinkCreateCmd, 		1, 2,	0,
   "binkfile ?flags?"},

  {"goto",              tclBinkGotoCmd, 		2, 2,	0,
   "binkfile frame"},

  {"wait",		tclBinkWaitCmd,		        1, 1,	0,
   "handle"},

  {"nextframe",		tclBinkNextFrameCmd,	        1, 1,	0,
   "handle"},

  {"getframes",		tclBinkGetFramesCmd,	        1, 1,	0,
   "handle"},

  {"getframe",		tclBinkGetFrameCmd,	        1, 1,	0,
   "handle"},

  {"getframepng",	tclBinkGetFramePNGCmd,	        1, 1,	0,
   "handle"},

  {"getframenum",	tclBinkGetFrameNumCmd,	        1, 1,	0,
   "handle"},

  {"setsound",	        tclBinkSetSoundOnOffCmd,        2, 2,	0,
   "handle 0|1"},

  {"setframerate",      tclBinkSetFrameRateCmd, 	2, 2,	0,
   "rate rate_div"},

  {"setsoundtrack",     tclBinkSetSoundTrackCmd, 	2, 10,	0,
   "rate rate_div"},

  {"destroy",		tclBinkDestroyCmd,		1, 1,	0,
   "handle"},

  {"close",		tclBinkDestroyCmd,		1, 1,	0,
   "handle"},

  {"destroyAll",	tclBinkDestroyCmd,		0, 1,	0,
   ""},
};


static char BinkErrBuff[4096];

static int tclSetDoubleResult(Tcl_Interp * interp, double result)
{
  Tcl_Obj *old = Tcl_GetObjResult(interp);
  Tcl_SetDoubleObj(old, result);
  return TCL_OK;
}

static int tclSetIntResult(Tcl_Interp * interp, int result)
{
  Tcl_Obj *old = Tcl_GetObjResult(interp);
  Tcl_SetIntObj(old, result);
  return TCL_OK;
}

static int addBinkToTable(Tcl_HashTable *table, BINKOBJ *b, char *buf)
{
  Tcl_HashEntry *entryPtr;
  int newentry;
  if (!b) return 0;
  sprintf(buf, "bink%d", binkCount++);
  entryPtr = Tcl_CreateHashEntry(table, buf, &newentry);
  Tcl_SetHashValue(entryPtr, b);
  return TCL_OK;
}

/* 
 * findBink()
 *   Find the HBINK corresponding to the given handle
 *   If the handle (or table) doesn't exist and null_ok is 0, return TCL_ERROR
 *   If null_ok is one, then set im to NULL, but return TCL_OK
 */
static int findBink(Tcl_Interp *interp, 
		    Tcl_HashTable *table, char *handle, 
		    BINKOBJ **b, int null_ok)
{
  BINKOBJ *bink;
  Tcl_HashEntry *entryPtr;

  if ((entryPtr = Tcl_FindHashEntry(table, handle))) {
    bink = Tcl_GetHashValue(entryPtr);
    if (!bink) {
      Tcl_SetResult(interp, "bad bink ptr in hash table", TCL_STATIC);
      return TCL_ERROR;
    }
    if (b) *b = bink;
    return TCL_OK;
  }

  else {
    Tcl_AppendResult(interp, "binkfile \"", handle, "\" not found", 
		     (char *) NULL);
    return TCL_ERROR;
  }
}


int
binkCmd(ClientData clientData, Tcl_Interp *interp, 
	int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *)clientData;
  int subi;
  char buf[100];
  if (argc < 2) {
    Tcl_SetResult(interp,
		  "wrong # args: should be \"bink option ...\"", 
		  TCL_STATIC);
    return TCL_ERROR;
  }
  
  /* Find the subcommand. */
  for (subi = 0; subi < (sizeof subcmdVec) / (sizeof subcmdVec[0]); 
       subi++) {
    if (strcmp(subcmdVec[subi].cmd, Tcl_GetString(objv[1])) == 0) {
      /* Check arg count. */
      if (argc - 2 < subcmdVec[subi].minargs ||
	  argc - 2 > subcmdVec[subi].maxargs) {
	sprintf(buf, "wrong # args: should be \"bink %s %s\"",
		subcmdVec[subi].cmd,  subcmdVec[subi].usage);
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
	return TCL_ERROR;
      }

      /* Call the subcommand function. */
      Tcl_ObjCmdProc *subcmd = (Tcl_ObjCmdProc *) subcmdVec[subi].f;
      return subcmd(binkTable, interp, argc, objv);
    }
  }
  
  /* If we get here, the option doesn't match. */
  Tcl_AppendResult(interp, "bad option \"", 
		   Tcl_GetString(objv[1]), "\": should be ", 0);
  for (subi = 0; subi < (sizeof subcmdVec) / (sizeof subcmdVec[0]); 
       subi++)
    Tcl_AppendResult(interp, (subi > 0 ? ", " : ""),
		     subcmdVec[subi].cmd, 0);
  return TCL_ERROR;
}


static int
tclBinkCreateCmd(ClientData clientData,
		 Tcl_Interp *interp,
		 int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;  
  HBINK bink;
  BINKOBJ *b;
  char *cmd, buf[50];
  int flags = BINKNOSKIP;
  
  cmd = Tcl_GetString(objv[1]);

  if (cmd[0] == 'o') {	/* open */
    char *filename = Tcl_GetString(objv[2]);
    if (argc > 3) {
      if (Tcl_GetIntFromObj(interp, objv[3], &flags) != TCL_OK) 
	return TCL_ERROR;
    }
    if (!(bink = BinkOpen(filename, flags))) {
      Tcl_AppendResult(interp, "unable to open bink file ", 
		       filename, 0);
      return TCL_ERROR;
    }
  }
  b = (BINKOBJ *) calloc(1, sizeof(BINKOBJ));
  b->bink = bink;
  b->buffer = (unsigned char *) calloc(b->bink->Width*b->bink->Height*4, 
				       sizeof(unsigned char));
  addBinkToTable(binkTable, b, buf);
  Tcl_SetResult(interp, buf, TCL_VOLATILE);
  return TCL_OK;
}


static int
tclBinkDestroyCmd(ClientData clientData,
		  Tcl_Interp *interp,
		  int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;  
  Tcl_HashEntry *entryPtr;
  BINKOBJ *binkobj;
  char *cmd = Tcl_GetString(objv[1]);

  if (cmd[7] == 'A') {		/* destroyAll */
    Tcl_HashSearch search;
    for (entryPtr = Tcl_FirstHashEntry(binkTable, &search);
	 entryPtr != NULL;
	 entryPtr = Tcl_NextHashEntry(&search)) {
      binkCount = 0;
      binkobj = Tcl_GetHashValue(entryPtr);
      BinkClose(binkobj->bink);
      free(binkobj->buffer);
      Tcl_DeleteHashEntry(entryPtr);
    }
    return TCL_OK;
  }

  if ((entryPtr = Tcl_FindHashEntry(binkTable, Tcl_GetString(objv[2])))) {
    if ((binkobj = Tcl_GetHashValue(entryPtr))) {
      BinkClose(binkobj->bink);
      free(binkobj->buffer);
    }
    Tcl_DeleteHashEntry(entryPtr);
    return TCL_OK;
  }
  else {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "bink destroy: bink \"", Tcl_GetString(objv[2]), 
		     "\" not found", (char *) NULL);
    return TCL_ERROR;
  }
}

/*
 * return frame as PNG memory string
 */
static int tclBinkGetFrameCmd (ClientData clientData,
			       Tcl_Interp *interp,
			       int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;  
  int w = -1, h, d = 4, pitch;
  BINKOBJ *b;
  char *cmd;
  int ref = 0;
  unsigned char *pixels;
  
  cmd = Tcl_GetString(objv[1]);

  /* Get the src pointer. */
  if (findBink(interp, binkTable, Tcl_GetString(objv[2]),
		&b, 0) != TCL_OK) return TCL_ERROR;

  if (argc > 3) {
    if (Tcl_GetIntFromObj(interp, objv[3], &w) != TCL_OK) 
      return TCL_ERROR;
  }
  if (w <= 0) 
    w = b->bink->Width;
  
  h = (int) (w*((double)b->bink->Height/b->bink->Width));
  pitch = w*d;
  
  pixels = (unsigned char *) calloc(h*pitch, sizeof(char));
  BinkDoFrame(b->bink);
  if (BinkCopyToBuffer(b->bink, pixels, pitch, h, 0, 0,
		       BINKNOSKIP | BINKSURFACE32RGBx)) {
    Tcl_SetResult(interp, "bink: copying to buffer", TCL_STATIC);
      return TCL_ERROR;
  }

  Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(pixels, h*pitch));
  free(pixels);
  return TCL_OK;
}

/*
 * return frame as PNG memory string
 */
static int tclBinkGetFramePNGCmd (ClientData clientData,
				  Tcl_Interp *interp,
				  int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;  
  int w = -1, h, d = 4, pitch;
  BINKOBJ *b;
  char *cmd;
  int ref = 0;
  unsigned char *pixels;
  
  cmd = Tcl_GetString(objv[1]);

  /* Get the src pointer. */
  if (findBink(interp, binkTable, Tcl_GetString(objv[2]),
		&b, 0) != TCL_OK) return TCL_ERROR;

  if (argc > 3) {
    if (Tcl_GetIntFromObj(interp, objv[3], &w) != TCL_OK) 
      return TCL_ERROR;
  }
  if (w <= 0) 
    w = b->bink->Width;
  
  h = (int) (w*((double)b->bink->Height/b->bink->Width));
  pitch = w*d;
  
  pixels = (unsigned char *) calloc(h*pitch, sizeof(char));
  BinkDoFrame(b->bink);
  if (BinkCopyToBuffer(b->bink, pixels, pitch, h, 0, 0,
		       BINKNOSKIP | BINKSURFACE32RGBx)) {
    Tcl_SetResult(interp, "bink: copying to buffer", TCL_STATIC);
      return TCL_ERROR;
  }

  unsigned char *out;
  size_t outsize;
  unsigned error = lodepng_encode32(&out, &outsize,
				    pixels, b->bink->Width, b->bink->Height);
  /* free raw pixels */
  free(pixels);
  if (error) {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]), ": error creating png from bink frame",
		     NULL);
    return TCL_ERROR;
  }
  Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(out, outsize));
  free(out);
  
  return TCL_OK;
}

static int tclBinkWaitCmd (ClientData clientData,
			   Tcl_Interp *interp,
			   int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;
  BINKOBJ *b;
  char *cmd;

  cmd = Tcl_GetString(objv[1]);

  /* Get the src pointer. */
  if (findBink(interp, binkTable, Tcl_GetString(objv[2]),
		&b, 0) != TCL_OK) return TCL_ERROR;

  return(tclSetIntResult(interp, BinkWait(b->bink)));

}

static int tclBinkNextFrameCmd (ClientData clientData,
				Tcl_Interp *interp,
				int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;
  BINKOBJ *b;
  char *cmd;

  cmd = Tcl_GetString(objv[1]);

  /* Get the src pointer. */
  if (findBink(interp, binkTable, Tcl_GetString(objv[2]),
		&b, 0) != TCL_OK) return TCL_ERROR;

  BinkNextFrame(b->bink);
  return TCL_OK;

}

static int tclBinkSetSoundOnOffCmd (ClientData clientData,
				    Tcl_Interp *interp,
				    int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;
  BINKOBJ *b;
  char *cmd;
  int onoff;

  cmd = Tcl_GetString(objv[1]);

  /* Get the src pointer. */
  if (findBink(interp, binkTable, Tcl_GetString(objv[2]),
		&b, 0) != TCL_OK) return TCL_ERROR;

  if (Tcl_GetIntFromObj(interp, objv[3], &onoff) != TCL_OK) return TCL_ERROR;

  BinkSetSoundOnOff(b->bink, onoff);
  return TCL_OK;

}

static int tclBinkGetFramesCmd (ClientData clientData,
				Tcl_Interp *interp,
				int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;
  BINKOBJ *b;
  char *cmd;

  cmd = Tcl_GetString(objv[1]);

  /* Get the src pointer. */
  if (findBink(interp, binkTable, Tcl_GetString(objv[2]),
		&b, 0) != TCL_OK) return TCL_ERROR;

  return(tclSetIntResult(interp, b->bink->Frames));

}

static int tclBinkGetFrameNumCmd (ClientData clientData,
				  Tcl_Interp *interp,
				  int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;
  BINKOBJ *b;
  char *cmd;

  cmd = Tcl_GetString(objv[1]);

  /* Get the src pointer. */
  if (findBink(interp, binkTable, Tcl_GetString(objv[2]),
		&b, 0) != TCL_OK) return TCL_ERROR;

  return(tclSetIntResult(interp, b->bink->FrameNum));

}

static int tclBinkSetFrameRateCmd (ClientData clientData,
				   Tcl_Interp *interp,
				   int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;
  char *cmd;
  int frame_rate, frame_rate_div;

  cmd = Tcl_GetString(objv[1]);

  if (Tcl_GetIntFromObj(interp, objv[2], &frame_rate) != TCL_OK)
    return TCL_ERROR;
  if (Tcl_GetIntFromObj(interp, objv[3], &frame_rate_div) != TCL_OK)
    return TCL_ERROR;

  BinkSetFrameRate(frame_rate, frame_rate_div);
  return(TCL_OK);
}

static int tclBinkSetSoundTrackCmd (ClientData clientData,
				    Tcl_Interp *interp,
				    int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;
  char *cmd;
  int track_count, track_id;
  uint32_t track_ids[64];
  int i, n;

  cmd = Tcl_GetString(objv[1]);

  if (Tcl_GetIntFromObj(interp, objv[2], &track_count) != TCL_OK)
    return TCL_ERROR;
  for (i = 0; i < track_count; i++) track_ids[i] = 0;
  if (argc > 3) {
    for (i = 3, n = 0; i < (unsigned) argc; i++) {
      if (Tcl_GetIntFromObj(interp, objv[i], &track_id) != TCL_OK)
	return TCL_ERROR;
      track_ids[n++] = track_id;
    }
  }

  BinkSetSoundTrack(track_count, track_ids);
  return(TCL_OK);
}

static int tclBinkGotoCmd (ClientData clientData,
			   Tcl_Interp *interp,
			   int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;
  BINKOBJ *b;
  char *cmd;
  int frame;

  cmd = Tcl_GetString(objv[1]);

  /* Get the src pointer. */
  if (findBink(interp, binkTable, Tcl_GetString(objv[2]),
		&b, 0) != TCL_OK) return TCL_ERROR;

  if (Tcl_GetIntFromObj(interp, objv[3], &frame) != TCL_OK) return TCL_ERROR;

  if (frame > b->bink->Frames) frame = b->bink->Frames;
  BinkGoto(b->bink, frame, 0);
  return(TCL_OK);
}

static int tclBinkInfoCmd (ClientData clientData,
			   Tcl_Interp *interp,
			   int argc, Tcl_Obj *const objv[])
{
  Tcl_HashTable *binkTable = (Tcl_HashTable *) clientData;
  BINKOBJ *b;
  BINKSUMMARY summary;
  char *cmd;
  
  cmd = Tcl_GetString(objv[1]);

  /* Get the src pointer. */
  if (findBink(interp, binkTable, Tcl_GetString(objv[2]),
		&b, 0) != TCL_OK) return TCL_ERROR;

  BinkGetSummary(b->bink, &summary);

  char infobuf[128];
  snprintf(infobuf, sizeof(infobuf), "%d %d %.3f %d %.0f", 
	  summary.Width, summary.Height,
	  summary.FileFrameRate / (float) summary.FileFrameRateDiv,
	  summary.TotalFrames,
	  1000.*((float) (summary.TotalFrames) /
	   ((float) (summary.FileFrameRate)/summary.FileFrameRateDiv)));
  Tcl_SetObjResult(interp, Tcl_NewStringObj(infobuf, -1));
  return(TCL_OK);
}


static void set_constants(Tcl_Interp *interp)
{
  Tcl_SetVar(interp, "BINK(FRAMERATE)",         "0x00001000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(PRELOADALL)",        "0x00002000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(SNDTRACK)",          "0x00004000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(OLDFRAMEFORMAT)",    "0x00008000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(RBINVERT)",          "0x00010000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(GRAYSCALE)",         "0x00020000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(NOMMX)",             "0x00040000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(NOSKIP)",            "0x00080000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(ALPHA)",             "0x00100000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(NOFILLIOBUF)",       "0x00200000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(SIMULATE)",          "0x00400000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(FILEHANDLE)",        "0x00800000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(IOSIZE)",            "0x01000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(IOPROCESSOR)",       "0x02000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(FROMMEMORY)",        "0x04000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(NOTHREADEDIO)",      "0x08000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(SURFACEFAST)",       "0x00000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(SURFACESLOW)",       "0x08000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(SURFACEDIRECT)",     "0x04000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(COPYALL)",           "0x80000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(COPY2XH)",           "0x10000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(COPY2XHI)",          "0x20000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(COPY2XW)",           "0x30000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(COPY2XWH)",          "0x40000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(COPY2XWHI)",         "0x50000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(COPY1XI)",           "0x60000000", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "BINK(COPYNOSCALING)",     "0x70000000", TCL_GLOBAL_ONLY);
}

/* 
 * Initialize the package.
 */
#ifdef WIN32
EXPORT(int,Tcl_bink_Init) _ANSI_ARGS_((Tcl_Interp *interp))
#else
int Tcl_bink_Init(Tcl_Interp *interp) 
#endif
{
  
  if (
#ifdef USE_TCL_STUBS
      Tcl_InitStubs(interp, "8.5", 0)
#else
      Tcl_PkgRequire(interp, "Tcl", "8.5", 0)
#endif
      == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_PkgProvide(interp, "bink", "2.0") != TCL_OK) {
    return TCL_ERROR;
  }

  //  BinkSoundUseDirectSound(0);

  set_constants(interp);
  Tcl_InitHashTable(&BinkTable, TCL_STRING_KEYS);
  Tcl_CreateObjCommand(interp, "bink", 
		       binkCmd, (ClientData) &BinkTable, 
		       (Tcl_CmdDeleteProc *)NULL);
  return TCL_OK;
}

int
Bink_SafeInit(Tcl_Interp *interp) 
{
  return Tcl_bink_Init(interp);
}
