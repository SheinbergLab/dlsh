/*
 * NAME
 *   tcl_dslog.c
 *
 * DESCRIPTION
 *   Tcl interface for reading and manipulating dslog files
 *
 * AUTHOR
 *   DLS, 06/11
 */

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <tcl.h>
#include <df.h>
#include <tcl_dl.h>
#include <math.h>
#include <dslog.h>

#define DSLOG_READ     1
#define DSLOG_READ_ESS 2

/*
 * Handle management for streaming dslog I/O
 */

#define DSLOG_MODE_READ  0
#define DSLOG_MODE_WRITE 1

typedef struct dslog_handle {
  int mode;             /* DSLOG_MODE_READ or DSLOG_MODE_WRITE */
  FILE *fp;             /* for reading */
  int fd;               /* for writing */
  int version;          /* file version from header */
  uint64_t timestamp;   /* header timestamp */
} dslog_handle_t;

static int dslog_handle_count = 0;

static void dslog_handle_free(dslog_handle_t *h)
{
  if (!h) return;
  if (h->mode == DSLOG_MODE_READ && h->fp) {
    fclose(h->fp);
  } else if (h->mode == DSLOG_MODE_WRITE && h->fd >= 0) {
    close(h->fd);
  }
  free(h);
}

/*
 * Convert a ds_datapoint_t to a Tcl dict
 */
static Tcl_Obj *dpoint_to_dict(ds_datapoint_t *dp)
{
  Tcl_Obj *dict = Tcl_NewDictObj();

  Tcl_DictObjPut(NULL, dict,
		 Tcl_NewStringObj("varname", -1),
		 Tcl_NewStringObj(dp->varname, dp->varlen));

  Tcl_DictObjPut(NULL, dict,
		 Tcl_NewStringObj("timestamp", -1),
		 Tcl_NewWideIntObj((Tcl_WideInt) dp->timestamp));

  Tcl_DictObjPut(NULL, dict,
		 Tcl_NewStringObj("flags", -1),
		 Tcl_NewIntObj((int) dp->flags));

  Tcl_DictObjPut(NULL, dict,
		 Tcl_NewStringObj("type", -1),
		 Tcl_NewIntObj((int) dp->data.type));

  Tcl_DictObjPut(NULL, dict,
		 Tcl_NewStringObj("len", -1),
		 Tcl_NewIntObj((int) dp->data.len));

  /* store data as byte array (works for all types including strings) */
  Tcl_DictObjPut(NULL, dict,
		 Tcl_NewStringObj("data", -1),
		 Tcl_NewByteArrayObj(dp->data.buf, dp->data.len));

  return dict;
}

/*
 * Convert a Tcl dict to a ds_datapoint_t (caller must free with dpoint_free)
 */
static int dict_to_dpoint(Tcl_Interp *interp, Tcl_Obj *dict,
			  ds_datapoint_t **out)
{
  Tcl_Obj *val;
  ds_datapoint_t *dp;
  const char *varname;
  Tcl_Size varlen;
  Tcl_WideInt ts;
  int intval;
  Tcl_Size datalen;
  const unsigned char *databuf;

  dp = (ds_datapoint_t *) calloc(1, sizeof(ds_datapoint_t));
  if (!dp) {
    Tcl_SetResult(interp, "memory allocation failed", TCL_STATIC);
    return TCL_ERROR;
  }

  /* varname */
  if (Tcl_DictObjGet(interp, dict, Tcl_NewStringObj("varname", -1),
		     &val) != TCL_OK || !val) {
    free(dp);
    Tcl_SetResult(interp, "dict missing 'varname'", TCL_STATIC);
    return TCL_ERROR;
  }
  varname = Tcl_GetStringFromObj(val, &varlen);
  dp->varlen = (uint16_t) varlen;
  dp->varname = (char *) malloc(varlen + 1);
  memcpy(dp->varname, varname, varlen);
  dp->varname[varlen] = '\0';

  /* timestamp */
  if (Tcl_DictObjGet(interp, dict, Tcl_NewStringObj("timestamp", -1),
		     &val) != TCL_OK || !val) {
    dpoint_free(dp);
    Tcl_SetResult(interp, "dict missing 'timestamp'", TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetWideIntFromObj(interp, val, &ts) != TCL_OK) {
    dpoint_free(dp);
    return TCL_ERROR;
  }
  dp->timestamp = (uint64_t) ts;

  /* flags */
  if (Tcl_DictObjGet(interp, dict, Tcl_NewStringObj("flags", -1),
		     &val) != TCL_OK || !val) {
    dpoint_free(dp);
    Tcl_SetResult(interp, "dict missing 'flags'", TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp, val, &intval) != TCL_OK) {
    dpoint_free(dp);
    return TCL_ERROR;
  }
  dp->flags = (uint32_t) intval;

  /* type */
  if (Tcl_DictObjGet(interp, dict, Tcl_NewStringObj("type", -1),
		     &val) != TCL_OK || !val) {
    dpoint_free(dp);
    Tcl_SetResult(interp, "dict missing 'type'", TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp, val, &intval) != TCL_OK) {
    dpoint_free(dp);
    return TCL_ERROR;
  }
  dp->data.type = (ds_datatype_t) intval;

  /* data */
  if (Tcl_DictObjGet(interp, dict, Tcl_NewStringObj("data", -1),
		     &val) != TCL_OK || !val) {
    dpoint_free(dp);
    Tcl_SetResult(interp, "dict missing 'data'", TCL_STATIC);
    return TCL_ERROR;
  }
  databuf = Tcl_GetByteArrayFromObj(val, &datalen);
  dp->data.len = (uint32_t) datalen;
  if (datalen > 0) {
    dp->data.buf = (unsigned char *) malloc(datalen);
    memcpy(dp->data.buf, databuf, datalen);
  } else {
    dp->data.buf = NULL;
  }

  *out = dp;
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    dslogReadCmd
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    dslog::read
 *
 * DESCRIPTION
 *    Read a dslog file into dlsh
 *
 ****************************************************************************/

static int dslogReadCmd (ClientData data, Tcl_Interp *interp,
			 int objc, Tcl_Obj *objv[])
{
  int rc;
  int type = (Tcl_Size) data;
  
  DYN_GROUP *dg;
  
  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "path");
    return TCL_ERROR;
  }

  switch (type) {
  case DSLOG_READ:
    rc = dslog_to_dg(Tcl_GetString(objv[1]), &dg);
    break;
  case DSLOG_READ_ESS:
    rc = dslog_to_essdg(Tcl_GetString(objv[1]), &dg);
    break;
  default:
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]), ": read type not supported",
                     (char *) NULL);
    return TCL_ERROR;
  }
  
  switch (rc) {
  case DSLOG_FileNotFound:
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]), ": not found",
                     (char *) NULL);
    return TCL_ERROR;
    break;
  case DSLOG_FileUnreadable:
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]), ": error reading file",
                     (char *) NULL);
    return TCL_ERROR;
    break;
  case DSLOG_InvalidFormat:
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]), ": not recognized",
                     (char *) NULL);
    return TCL_ERROR;
    break;
  }

  return (tclPutGroup(interp, dg));
}

/*****************************************************************************
 *
 * FUNCTION
 *    dslogOpenCmd
 *
 * TCL FUNCTION
 *    dslog::open path mode
 *
 * DESCRIPTION
 *    Open a dslog file for reading or writing. Returns a handle string.
 *
 ****************************************************************************/

static int dslogOpenCmd(ClientData data, Tcl_Interp *interp,
			int objc, Tcl_Obj *objv[])
{
  const char *path, *mode;
  dslog_handle_t *h;
  char handle_name[32];

  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "path r|w");
    return TCL_ERROR;
  }

  path = Tcl_GetString(objv[1]);
  mode = Tcl_GetString(objv[2]);

  h = (dslog_handle_t *) calloc(1, sizeof(dslog_handle_t));
  if (!h) {
    Tcl_SetResult(interp, "memory allocation failed", TCL_STATIC);
    return TCL_ERROR;
  }
  h->fd = -1;

  if (mode[0] == 'r') {
    int rc;
    h->mode = DSLOG_MODE_READ;
    h->fp = fopen(path, "rb");
    if (!h->fp) {
      free(h);
      Tcl_AppendResult(interp, "cannot open file: ", path, NULL);
      return TCL_ERROR;
    }
    rc = dslog_read_header(h->fp, &h->version, &h->timestamp);
    if (rc <= 0) {
      fclose(h->fp);
      free(h);
      Tcl_AppendResult(interp, "not a valid dslog file: ", path, NULL);
      return TCL_ERROR;
    }
  } else if (mode[0] == 'w') {
    struct timeval tv;
    h->mode = DSLOG_MODE_WRITE;
    h->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (h->fd < 0) {
      free(h);
      Tcl_AppendResult(interp, "cannot create file: ", path, NULL);
      return TCL_ERROR;
    }
    gettimeofday(&tv, NULL);
    h->timestamp = (uint64_t) tv.tv_sec * 1000000ULL + tv.tv_usec;
    h->version = DSERV_LOG_CURRENT_VERSION;
    if (!dslog_write_header(h->fd, h->timestamp)) {
      close(h->fd);
      free(h);
      Tcl_SetResult(interp, "error writing dslog header", TCL_STATIC);
      return TCL_ERROR;
    }
  } else {
    free(h);
    Tcl_AppendResult(interp, "mode must be 'r' or 'w', got: ", mode, NULL);
    return TCL_ERROR;
  }

  /* create handle name and store as Tcl assoc data */
  snprintf(handle_name, sizeof(handle_name), "dslog%d", dslog_handle_count++);
  Tcl_SetAssocData(interp, handle_name,
		   (Tcl_InterpDeleteProc *) dslog_handle_free, h);

  Tcl_SetObjResult(interp, Tcl_NewStringObj(handle_name, -1));
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    dslogCloseCmd
 *
 * TCL FUNCTION
 *    dslog::close handle
 *
 ****************************************************************************/

static int dslogCloseCmd(ClientData data, Tcl_Interp *interp,
			 int objc, Tcl_Obj *objv[])
{
  const char *handle_name;

  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "handle");
    return TCL_ERROR;
  }

  handle_name = Tcl_GetString(objv[1]);
  if (!Tcl_GetAssocData(interp, handle_name, NULL)) {
    Tcl_AppendResult(interp, "invalid handle: ", handle_name, NULL);
    return TCL_ERROR;
  }

  /* Tcl_DeleteAssocData calls our dslog_handle_free callback */
  Tcl_DeleteAssocData(interp, handle_name);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    dslogNextCmd
 *
 * TCL FUNCTION
 *    dslog::next handle
 *
 * DESCRIPTION
 *    Read next datapoint. Returns dict or empty string at EOF.
 *
 ****************************************************************************/

static int dslogNextCmd(ClientData data, Tcl_Interp *interp,
			int objc, Tcl_Obj *objv[])
{
  const char *handle_name;
  dslog_handle_t *h;
  ds_datapoint_t *dp = NULL;
  int rc;

  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "handle");
    return TCL_ERROR;
  }

  handle_name = Tcl_GetString(objv[1]);
  h = (dslog_handle_t *) Tcl_GetAssocData(interp, handle_name, NULL);
  if (!h) {
    Tcl_AppendResult(interp, "invalid handle: ", handle_name, NULL);
    return TCL_ERROR;
  }
  if (h->mode != DSLOG_MODE_READ) {
    Tcl_SetResult(interp, "handle not open for reading", TCL_STATIC);
    return TCL_ERROR;
  }

  rc = dpoint_read(h->fp, &dp);
  if (rc == 0) {
    /* EOF - return empty string */
    Tcl_ResetResult(interp);
    return TCL_OK;
  }
  if (rc < 0) {
    Tcl_SetResult(interp, "error reading datapoint", TCL_STATIC);
    return TCL_ERROR;
  }

  Tcl_SetObjResult(interp, dpoint_to_dict(dp));
  dpoint_free(dp);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    dslogPutCmd
 *
 * TCL FUNCTION
 *    dslog::put handle dict
 *
 * DESCRIPTION
 *    Write a datapoint dict to an output dslog file.
 *
 ****************************************************************************/

static int dslogPutCmd(ClientData data, Tcl_Interp *interp,
		       int objc, Tcl_Obj *objv[])
{
  const char *handle_name;
  dslog_handle_t *h;
  ds_datapoint_t *dp = NULL;

  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "handle dict");
    return TCL_ERROR;
  }

  handle_name = Tcl_GetString(objv[1]);
  h = (dslog_handle_t *) Tcl_GetAssocData(interp, handle_name, NULL);
  if (!h) {
    Tcl_AppendResult(interp, "invalid handle: ", handle_name, NULL);
    return TCL_ERROR;
  }
  if (h->mode != DSLOG_MODE_WRITE) {
    Tcl_SetResult(interp, "handle not open for writing", TCL_STATIC);
    return TCL_ERROR;
  }

  if (dict_to_dpoint(interp, objv[2], &dp) != TCL_OK) {
    return TCL_ERROR;
  }

  if (dpoint_write(h->fd, dp) < 0) {
    dpoint_free(dp);
    Tcl_SetResult(interp, "error writing datapoint", TCL_STATIC);
    return TCL_ERROR;
  }

  dpoint_free(dp);
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    dslogSkipCmd
 *
 * TCL FUNCTION
 *    dslog::skip handle ?n?
 *
 * DESCRIPTION
 *    Skip N datapoints (default 1). Returns number actually skipped.
 *
 ****************************************************************************/

static int dslogSkipCmd(ClientData data, Tcl_Interp *interp,
			int objc, Tcl_Obj *objv[])
{
  const char *handle_name;
  dslog_handle_t *h;
  int n = 1, skipped = 0;

  if (objc < 2 || objc > 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "handle ?n?");
    return TCL_ERROR;
  }

  handle_name = Tcl_GetString(objv[1]);
  h = (dslog_handle_t *) Tcl_GetAssocData(interp, handle_name, NULL);
  if (!h) {
    Tcl_AppendResult(interp, "invalid handle: ", handle_name, NULL);
    return TCL_ERROR;
  }
  if (h->mode != DSLOG_MODE_READ) {
    Tcl_SetResult(interp, "handle not open for reading", TCL_STATIC);
    return TCL_ERROR;
  }

  if (objc == 3) {
    if (Tcl_GetIntFromObj(interp, objv[2], &n) != TCL_OK) {
      return TCL_ERROR;
    }
  }

  for (int i = 0; i < n; i++) {
    ds_datapoint_t *dp = NULL;
    int rc = dpoint_read(h->fp, &dp);
    if (rc <= 0) break;
    dpoint_free(dp);
    skipped++;
  }

  Tcl_SetObjResult(interp, Tcl_NewIntObj(skipped));
  return TCL_OK;
}

/*****************************************************************************
 *
 * FUNCTION
 *    dslogInfoCmd
 *
 * TCL FUNCTION
 *    dslog::info handle
 *
 * DESCRIPTION
 *    Return dict with version and header timestamp for an open handle.
 *
 ****************************************************************************/

static int dslogInfoCmd(ClientData data, Tcl_Interp *interp,
			int objc, Tcl_Obj *objv[])
{
  const char *handle_name;
  dslog_handle_t *h;
  Tcl_Obj *dict;

  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "handle");
    return TCL_ERROR;
  }

  handle_name = Tcl_GetString(objv[1]);
  h = (dslog_handle_t *) Tcl_GetAssocData(interp, handle_name, NULL);
  if (!h) {
    Tcl_AppendResult(interp, "invalid handle: ", handle_name, NULL);
    return TCL_ERROR;
  }

  dict = Tcl_NewDictObj();
  Tcl_DictObjPut(NULL, dict,
		 Tcl_NewStringObj("version", -1),
		 Tcl_NewIntObj(h->version));
  Tcl_DictObjPut(NULL, dict,
		 Tcl_NewStringObj("timestamp", -1),
		 Tcl_NewWideIntObj((Tcl_WideInt) h->timestamp));
  Tcl_DictObjPut(NULL, dict,
		 Tcl_NewStringObj("mode", -1),
		 Tcl_NewStringObj(h->mode == DSLOG_MODE_READ ? "r" : "w", 1));

  Tcl_SetObjResult(interp, dict);
  return TCL_OK;
}

/*****************************************************************************
 *
 * EXPORT
 *
 *****************************************************************************/

#ifdef WIN32
EXPORT(int,Dslog_Init) (Tcl_Interp *interp)
#else
int Dslog_Init(Tcl_Interp *interp)
#endif
{
  
  if (
#ifdef USE_TCL_STUBS
      Tcl_InitStubs(interp, "8.5-", 0)
#else
      Tcl_PkgRequire(interp, "Tcl", "8.5-", 0)
#endif
      == NULL) {
    return TCL_ERROR;
  }
  if (Tcl_PkgRequire(interp, "dlsh", "1.2", 0) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_PkgProvide(interp, "dslog", "1.0") != TCL_OK) {
    return TCL_ERROR;
  }
  
  Tcl_Eval(interp, "namespace eval dslog {}");
  
  Tcl_CreateObjCommand(interp, "dslog::read",
		       (Tcl_ObjCmdProc *) dslogReadCmd,
		       (ClientData) DSLOG_READ,
		       (Tcl_CmdDeleteProc *) NULL);
  
  Tcl_CreateObjCommand(interp, "dslog::readESS",
		       (Tcl_ObjCmdProc *) dslogReadCmd,
		       (ClientData) DSLOG_READ_ESS,
		       (Tcl_CmdDeleteProc *) NULL);

  /* streaming I/O commands */
  Tcl_CreateObjCommand(interp, "dslog::open",
		       (Tcl_ObjCmdProc *) dslogOpenCmd,
		       (ClientData) NULL,
		       (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateObjCommand(interp, "dslog::close",
		       (Tcl_ObjCmdProc *) dslogCloseCmd,
		       (ClientData) NULL,
		       (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateObjCommand(interp, "dslog::next",
		       (Tcl_ObjCmdProc *) dslogNextCmd,
		       (ClientData) NULL,
		       (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateObjCommand(interp, "dslog::put",
		       (Tcl_ObjCmdProc *) dslogPutCmd,
		       (ClientData) NULL,
		       (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateObjCommand(interp, "dslog::skip",
		       (Tcl_ObjCmdProc *) dslogSkipCmd,
		       (ClientData) NULL,
		       (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateObjCommand(interp, "dslog::info",
		       (Tcl_ObjCmdProc *) dslogInfoCmd,
		       (ClientData) NULL,
		       (Tcl_CmdDeleteProc *) NULL);

  return TCL_OK;
 }





