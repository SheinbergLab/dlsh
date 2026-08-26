/*************************************************************************
 *
 *  NAME
 *    dslog.c
 *
 *  DESCRIPTION
 *    ess_ds_logger conversion support
 *
 *  AUTHOR
 *    DLS
 *
 *************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <df.h>
#include <dynio.h>
#include "datapoint.h"
#include <dslog.h>
#define DSERV_LOG_HEADER_SIZE    16

#define E_BEGINOBS 19
#define E_ENDOBS   20

/*
 * Per-conversion state.  These fields used to be function statics, which
 * poisoned every file after the first in a long-lived process (the df
 * subprocess converts hundreds per day): time zeros and first-obs anchors
 * were latched once per process, so obs_times was only ever correct for
 * the first file read.
 */
typedef struct essdg_ctx_s {
  int have_t0;          /* seen the first record yet?                   */
  uint64_t file_t0;     /* very first record's timestamp (never moves)  */
  uint64_t time_zero;   /* e_pre anchor: first record or last BEGINOBS  */
  int got_first_obs;
  uint64_t first_obs;   /* first BEGINOBS timestamp                     */
  DYN_LIST *obs_start_ms; /* per-obs BEGINOBS ms from file_t0 (or NULL) */
} essdg_ctx_t;

/* Blob datatypes are opaque binary payloads (one unit per datapoint, e.g.
   a JPEG frame).  They get file-level <blob>/<blobt> columns rather than
   per-obs concatenation: a frame requested late in an obs can be encoded
   asynchronously and logged after ENDOBS, so log POSITION cannot assign it
   to an obs -- its capture TIMESTAMP can, and extraction does the pairing
   (df::File camera helpers). */
static int is_blob_dstype(int type)
{
  return (type == DSERV_JPEG || type == DSERV_PPM);
}

/* ds_datatype_t and DF_* are different enum spaces.  Empty-obs placeholder
   sublists used to be created with the raw ds type (DSERV_FLOAT -> DF_CHAR,
   DSERV_INT -> DF_FLOAT, ...), making columns type-heterogeneous whenever
   an obs had no samples. */
static int df_type_for_dstype(int type)
{
  switch (type) {
  case DSERV_BYTE:   return DF_CHAR;
  case DSERV_STRING: return DF_STRING;
  case DSERV_JSON:   return DF_STRING;
  case DSERV_FLOAT:  return DF_FLOAT;
  case DSERV_DOUBLE: return DF_FLOAT;
  case DSERV_SHORT:  return DF_SHORT;
  case DSERV_JPEG:   return DF_CHAR;
  case DSERV_PPM:    return DF_CHAR;
  case DSERV_INT:    return DF_LONG;
  default:           return DF_LONG;
  }
}

/* Number of decoded values add_dpoint_to_list() appends for a record --
   what lets a reader split a per-obs concatenated <ds> column back into
   its records (paired with the per-record <dst> timestamps). */
static int dpoint_val_count(ds_datapoint_t *d)
{
  switch (d->data.type) {
  case DSERV_BYTE:   return d->data.len;
  case DSERV_FLOAT:  return d->data.len / sizeof(float);
  case DSERV_SHORT:  return d->data.len / sizeof(short);
  case DSERV_INT:    return d->data.len / sizeof(int);
  case DSERV_DOUBLE: return d->data.len / sizeof(double);
  case DSERV_STRING: return 1;
  case DSERV_JSON:   return 1;
  default:           return 0;
  }
}

static int addDservObsPeriod(
			     FILE *fp,        /* open data file FP   */
			     int nvars,       /* number of dpoints   */
			     char **varnames, /* name of datapoints  */
			     int *vartypes,   /* type of datapoints  */
			     int *val_cols,   /* <ds> column ids     */
			     int *time_cols,  /* <dst> column ids    */
			     int *cnt_cols,   /* <dsn> column ids    */
			     DYN_GROUP *dg);   /* output dg          */

/*****************************************************************************/
/****************************** FILE I/O SUPPORT *****************************/
/*****************************************************************************/

void dpoint_free(ds_datapoint_t *d)
{
  if (d) {
    if (d->varname) {
      free(d->varname);
    }
    if (d->data.buf) {
      free(d->data.buf);
    }
    free(d);
  }
}

int dslog_write_header(int fd, uint64_t timestamp)
{
  unsigned char buf[DSERV_LOG_HEADER_SIZE];

  if (fd < 0) return 0;
  
  buf[0] = 'd';
  buf[1] = 's';
  buf[2] = 'l';
  buf[3] = 'o';
  buf[4] = 'g';

  buf[5] = DSERV_LOG_CURRENT_VERSION;

  memcpy((unsigned char *) &buf[8], &timestamp, sizeof(uint64_t));
  
  if (write(fd, buf, DSERV_LOG_HEADER_SIZE) != DSERV_LOG_HEADER_SIZE) {
    return 0;
  }
  return 1;
}

int dpoint_write(int fd, ds_datapoint_t *dpoint)
{
#if 0
  printf("%s(%d) %d %d %d\n", dpoint->varname, dpoint->varlen,
	   dpoint->data.e.dtype, dpoint->flags, dpoint->data.len);
#endif
  
  if (write(fd, &dpoint->varlen, sizeof(uint16_t)) != sizeof(uint16_t))
    goto write_error;

  if (write(fd, dpoint->varname, (int) dpoint->varlen) != (int) dpoint->varlen)
    goto write_error;

  if (write(fd, &dpoint->timestamp, sizeof(uint64_t)) != sizeof(uint64_t))
    goto write_error;

  if (write(fd, &dpoint->flags, sizeof(uint32_t)) != sizeof(uint32_t))
    goto write_error;

  if (write(fd, &dpoint->data.type, sizeof (ds_datatype_t)) != sizeof (ds_datatype_t))
    goto write_error;

  if (write(fd, &dpoint->data.len, sizeof(uint32_t)) != sizeof(uint32_t))
    goto write_error;

  if (dpoint->data.len) {
    if (write(fd, dpoint->data.buf, dpoint->data.len) != dpoint->data.len)
      goto write_error;
  }
  return 0;
  
 write_error:
  return -1;
}

/*
 * dslog_read_header
 * 
 *  Given open file stream, read header and return settings
 *
 * Input: 
 *   FILE *fp - open file pointer
 *
 * Output:
 *   int *version             - if non-null, set version of this file
 *   uint64_t *timestamp      - if non-null, set microsec timestamp 
 * 
 * Return:
 *   -1: error reading data
 *    0: file not recognized as ess_ds_log file
 *    1: OK
 */
int dslog_read_header(FILE *fp, int *version, uint64_t *timestamp)
{
  unsigned char header[DSERV_LOG_HEADER_SIZE];
  uint64_t *ts;
  
  if (fread(header, 1, DSERV_LOG_HEADER_SIZE, fp) != DSERV_LOG_HEADER_SIZE) {
    return -1;
  }
  if (header[0] != 'd' || header[1] != 's' ||
      header[2] != 'l' || header[3] != 'o' ||
      header[4] != 'g') {
    return 0;
  }

  /* Need to have a version set */
  if (!header[5]) return 0;
  
  if (version) *version = (int) header[5];

  
  if (timestamp) {
    ts = (uint64_t *) &header[8];
    *timestamp = *ts;
  }

  return 1;
}


/*
 * dpoint_read
 * 
 *  Given open file stream, read next dpoint from open file
 *
 * Input: 
 *   FILE *fp - open file pointer (advanced past header)
 *
 * Output:
 *   char **var                - if non-null, set to pointer of var
 *   ESS_DS_DATA **data        - if non-null, set to pointer of data
 * 
 * Return:
 *   -1: Error reading data
 *    0: EOF
 *    1: OK
 */
int dpoint_read(FILE *fp, ds_datapoint_t **dpoint)
{
  ds_datapoint_t *dp;
  uint16_t varlen;

  //  static int i = 0;
  
  if (fread(&varlen, sizeof(uint16_t), 1, fp) != 1) return 0;
  
  dp = (ds_datapoint_t *) malloc(sizeof(ds_datapoint_t));

  dp->varlen = varlen;
  dp->varname = malloc(varlen+1); /* we add the null term */
  if (!dp->varname) goto memory_error;
  if (fread(dp->varname, dp->varlen, 1, fp) != 1)
    goto read_error;
  dp->varname[dp->varlen] = '\0'; /* set null term here   */
  if (fread(&(dp->timestamp), sizeof(uint64_t), 1, fp) != 1)
    goto read_error;
  if (fread(&(dp->flags), sizeof(uint32_t), 1, fp) != 1)
    goto read_error;

  if (fread(&(dp->data.type), sizeof(ds_datatype_t), 1, fp) != 1)
    goto read_error;

  if (fread(&(dp->data.len), sizeof(uint32_t), 1, fp) != 1)
    goto read_error;

  //  printf("[%4d] %s (%d)\n", i++, dp->varname, dp->data.len);

  /* A truncated/corrupt log can carry a garbage data.len that drives a
     huge malloc before the fread below fails.  For suspiciously large
     lengths only (the common small-record path pays nothing), verify the
     data actually fits in what remains of the file. */
  if (dp->data.len > (1u << 26)) {   /* 64 MB */
    long pos = ftell(fp);
    if (pos >= 0 && fseek(fp, 0, SEEK_END) == 0) {
      long end = ftell(fp);
      (void) fseek(fp, pos, SEEK_SET);
      if (end >= 0 && (uint64_t) dp->data.len > (uint64_t)(end - pos))
	goto read_error;
    }
  }

  if (dp->data.len) {
    dp->data.buf = malloc(dp->data.len);
    if (!dp->data.buf) goto memory_error;
    if (fread(dp->data.buf, dp->data.len, 1, fp) != 1) {
      free(dp->data.buf);
      goto read_error;
    }
  }
  else dp->data.buf = NULL;

  if (dpoint) *dpoint = dp;

  return 1;
 read_error:
  free(dp->varname);
  free(dp);
  return -1;
 memory_error:
  free(dp->varname);
  free(dp);
  return -2;
}

static int addObsPeriod(DYN_GROUP *dg,
		 DYN_LIST *evt_names,
		 DYN_LIST *evt_types,
		 DYN_LIST *evt_subtypes,
		 DYN_LIST *evt_times,
		 DYN_LIST *evt_params,
		 DYN_LIST *misc,
		 DYN_LIST *ems,
		 DYN_LIST *ems2,
		 DYN_LIST *sp_types,
		 DYN_LIST *sp_channels,
		 DYN_LIST *sp_inputs,
		 DYN_LIST *sp_times,
		 int fileindx, int obsindx,
		 FILE *fp,
		 DYN_LIST *obs_times,
		 essdg_ctx_t *ctx);

static int addSeparatedEvent(DYN_LIST *types, DYN_LIST *subtypes, DYN_LIST *times, 
		      DYN_LIST *params, ds_datapoint_t *ev, uint64_t evtime);
static int addEvent(DYN_LIST *evtdata, ds_datapoint_t *ev, uint64_t evtime);



static DYN_LIST *create_val_list(ds_datatype_t type, int len, unsigned char *buf)
{
  int i, n;
  DYN_LIST *dl = NULL;
  char *vals;
  double *d;

  if (!len) {
    switch (type) {
    case DSERV_DG:
    case DSERV_BYTE:
    case DSERV_JPEG:
    case DSERV_PPM:
      dl = dfuCreateDynList(DF_CHAR, 1);
      break;
    case DSERV_DOUBLE:
    case DSERV_FLOAT:
      dl = dfuCreateDynList(DF_FLOAT, 1);
      break;
    case DSERV_SHORT:
      dl = dfuCreateDynList(DF_SHORT, 1);
      break;
    case DSERV_NONE:
    case DSERV_INT:
      dl = dfuCreateDynList(DF_LONG, 1);
      break;
    case DSERV_STRING:
    case DSERV_JSON:
      dl = dfuCreateDynList(DF_STRING, 1);
      break;
    default:
      break;
    }
  }
  else {
    switch(type) {
    case DSERV_DG:
    case DSERV_BYTE:
    case DSERV_JPEG:
    case DSERV_PPM:
      /* opaque bytes: a DF_CHAR vector of the raw payload */
      vals = malloc(len);
      memcpy(vals, buf, len);
      dl = dfuCreateDynListWithVals(DF_CHAR, len, vals);
      break;
    case DSERV_FLOAT:
      vals = malloc(len);
      memcpy(vals, buf, len);
      n = len/sizeof(float);
      dl = dfuCreateDynListWithVals(DF_FLOAT, n, vals);
      break;
    case DSERV_SHORT:
      vals = malloc(len);
      memcpy(vals, buf, len);
      n = len/sizeof(short);
      dl = dfuCreateDynListWithVals(DF_SHORT, n, vals);
      break;
    case DSERV_INT:
      vals = malloc(len);
      memcpy(vals, buf, len);
      n = len/sizeof(int);
      dl = dfuCreateDynListWithVals(DF_LONG, n, vals);
      break;
    case DSERV_DOUBLE:
      d = (double *) buf;
      n = len/sizeof(double);
      dl = dfuCreateDynList(DF_FLOAT, n);

      /* was `i < len`: read 8x past the buffer and appended 8x too many */
      for (i = 0; i < n; i++) {
	dfuAddDynListFloat(dl, (float) d[i]);
      }
      break;
    case DSERV_STRING:
    case DSERV_JSON:
      dl = dfuCreateDynList(DF_STRING, 1);
      vals = malloc(len+1);
      memcpy(vals, buf, len);
      vals[len] = '\0';
      dfuAddDynListString(dl, (char *) vals);
      free(vals);

      break;
    default:
      /* DSERV_INT64 and friends: no 64-bit dg type exists, so these fall
	 through to the empty placeholder below rather than truncating. */
      break;
    }
  }
  /* Never return NULL: unhandled (type,len) combinations (e.g. a non-empty
     DSERV_NONE, or a type not in the switch) would otherwise feed a null list
     into the essdg column builders and mis-align / crash them. An empty list
     is the correct "no value here" placeholder and keeps rows aligned. */
  if (!dl) dl = dfuCreateDynList(DF_LONG, 1);
  return dl;
}

static DYN_LIST *add_dpoint_to_list(DYN_LIST *dl, ds_datapoint_t *dpoint)
{
  int i, n;
  char *vals;
  double *d;

  if (!dpoint) return NULL;

  switch(dpoint->data.type) {
  case DSERV_BYTE:
  case DSERV_JPEG:
  case DSERV_PPM:
    if (!dl) dl = dfuCreateDynList(DF_CHAR, dpoint->data.len);
    if (!dl) return NULL;
    for (i = 0; i < dpoint->data.len; i++) {
      dfuAddDynListChar(dl, ((unsigned char *) dpoint->data.buf)[i]);
    }
    break;
  case DSERV_FLOAT:
    n = dpoint->data.len/sizeof(float);
    if (!dl) dl = dfuCreateDynList(DF_FLOAT, n);
    if (!dl) return NULL;
    for (i = 0; i < n; i++) {
      dfuAddDynListFloat(dl, ((float *) dpoint->data.buf)[i]);
    }
    break;
  case DSERV_SHORT:
    n = dpoint->data.len/sizeof(short);
    if (!dl) dl = dfuCreateDynList(DF_SHORT, n);
    if (!dl) return NULL;
    for (i = 0; i < n; i++) {
      dfuAddDynListShort(dl, ((short *) dpoint->data.buf)[i]);
    }
    break;
  case DSERV_INT:
    n = dpoint->data.len/sizeof(int);
    if (!dl) dl = dfuCreateDynList(DF_LONG, n);
    if (!dl) return NULL;
    for (i = 0; i < n; i++) {
      dfuAddDynListLong(dl, ((int *) dpoint->data.buf)[i]);
    }
    break;
  case DSERV_DOUBLE:
    n = dpoint->data.len/sizeof(double);
    if (!dl) dl = dfuCreateDynList(DF_FLOAT, n);
    if (!dl) return NULL;
    d = (double *) dpoint->data.buf;
    for (i = 0; i < n; i++) {
      dfuAddDynListFloat(dl, (float) d[i]);
    }
    break;
  case DSERV_STRING:
  case DSERV_JSON:
    /* used to unconditionally create a fresh list here, leaking the old
       one and keeping only the LAST string of the obs */
    if (!dl) dl = dfuCreateDynList(DF_STRING, 1);
    if (!dl) return NULL;
    vals = malloc(dpoint->data.len+1);
    memcpy(vals, dpoint->data.buf, dpoint->data.len);
    vals[dpoint->data.len] = '\0';
    dfuAddDynListString(dl, (char *) vals);
    free(vals);
    break;
  case DSERV_NONE:
    /* A valueless timing marker (e.g. a stim photon-sync signal sent via
       dserv_send_evt with no payload). There is no value to append; just
       ensure a non-null, correctly-aligned placeholder list exists. */
    if (!dl) dl = dfuCreateDynList(DF_LONG, 1);
    break;
  default:
    break;
  }
  /* Never hand a null back to the column builders (keeps rows aligned and
     avoids the null-list path entirely for unhandled/empty datapoints). */
  if (!dl) dl = dfuCreateDynList(DF_LONG, 1);
  return dl;
}


/* 
 * NAME 
 *   ds_log_to_dg
 *
 * DESCRIPTION
 *   convert a log file to a dynamic group
 *
 * RETURNS
 *    DSLOG_OK:             success
 *    DSLOG_FileNotFound:   can't open log file
 *    DSLOG_FileUnreadable: error reading data
 *    DSLOG_InvalidFormat:  file not recognized as a dslog
 *
 *   Upon success, if outdg != NULL, it will be set to the new dynamic group
 */
int dslog_to_dg(char *filename, DYN_GROUP **outdg)
{  
  int j;
  DYN_LIST *varnames, *timestamps, *values, *vallist;
  
  ds_datapoint_t *d;
  
  FILE *fp;
  int result;
  int version;
  double start_sec, time_sec;
  uint64_t timestamp;

  int datatype;
  char *name, evt_namebuf[32];
  
  DYN_GROUP *dg;
  
  fp = fopen(filename, "rb");
  if (!fp) {
    return DSLOG_FileNotFound;
  }
  
  if ((result = dslog_read_header(fp, &version, &timestamp)) != 1) {
    if (result == -1) {
      fclose(fp);
      return DSLOG_FileUnreadable;
    }
    if (result == 0) {
      fclose(fp);
      return DSLOG_InvalidFormat;
    }
  }
  
  start_sec = timestamp/1000000.;
  
  dg = dfuCreateNamedDynGroup(filename, 3);
  
  j = dfuAddDynGroupNewList(dg, "varname", DF_STRING, 200);
  varnames = DYN_GROUP_LIST(dg, j);
  
  j = dfuAddDynGroupNewList(dg, "timestamp", DF_FLOAT, 200);
  timestamps = DYN_GROUP_LIST(dg, j);
  
  j = dfuAddDynGroupNewList(dg, "vals", DF_LIST, 200);
  values = DYN_GROUP_LIST(dg, j);
  
  time_sec = start_sec;
  
  /* Insert initial event which includes open timestamp and version */
  dfuAddDynListString(varnames, "logger:open");
  dfuAddDynListFloat(timestamps, time_sec-start_sec);
  vallist = create_val_list(DSERV_INT, sizeof(int), (unsigned char *) &version);
  dfuMoveDynListList(values, vallist);
  
  while (dpoint_read(fp, &d) > 0) {
    time_sec =  d->timestamp/1000000.;

    if (d->data.e.dtype == DSERV_EVT) {
      sprintf(evt_namebuf, "evt:%d:%d", d->data.e.type, d->data.e.subtype);
      name = evt_namebuf;
      datatype = d->data.e.puttype;
    }
    else {
      name = d->varname;
      datatype = d->data.type;
    }

    dfuAddDynListString(varnames, name);
    dfuAddDynListFloat(timestamps, time_sec-start_sec);
    vallist = create_val_list(datatype, d->data.len, d->data.buf);
    if (!vallist) {
      fprintf(stderr, "invalid list %s, type %d\n", d->varname, datatype);
    }
    else {
      dfuMoveDynListList(values, vallist);
    }
    dpoint_free(d);
  }

  fclose(fp);

  /* return the newly created dg in outdg */
  if (outdg) *outdg = dg;
  else dfuFreeDynGroup(dg);

  return DSLOG_OK;
}


static void get_dgname(char *name, char *dgname)
{
  char *start, *end, *p;
    
  p = strrchr(name, '/');
  if (!p) start = name;
  else start = p+1;
  p = strrchr(start, '.');
  if (!p) end = start+strlen(start);
  else end = p;
  memcpy(dgname, start, end-start);
  dgname[end-start] = '\0';
}

static int add_emdata(DYN_LIST *info, DYN_LIST *h, DYN_LIST *v,
	       int interval, int nchans, uint16_t *data, int nsamples,
	       int offset)
{
  int i, skip = nchans-2;
  
  if (!DYN_LIST_N(info)) {
    dfuAddDynListLong(info, interval);
  }
  for (i = offset; i < nsamples; i+=skip) {
    // need to set horizontal/vertical ordering here
    dfuAddDynListShort(v, data[i++]-2048);
    dfuAddDynListShort(h, data[i++]-2048);
  }
  return 0;
}

static int add_event_names(DYN_LIST *evt_names, char *namelist, int len)
{
  int n;
  char *newline;
  char *str = namelist;
  char namestr[64];

  dfuResetDynList(evt_names);
  
  while ((newline = strchr(str, '\n'))) {
    n = newline-str;
    memcpy(namestr, str, n);
    namestr[n] = '\0';
    dfuAddDynListString(evt_names, namestr);
    str = newline+1;
  }
  return DYN_LIST_N(evt_names);
}

static int addObsPeriod(DYN_GROUP *dg,
		 DYN_LIST *evt_names,
		 DYN_LIST *evt_types, DYN_LIST *evt_subtypes,
		 DYN_LIST *evt_times,
		 DYN_LIST *evt_params,
		 DYN_LIST *misc, DYN_LIST *ems, DYN_LIST *ems2,
		 DYN_LIST *spk_types,
		 DYN_LIST *spk_channels,
		 DYN_LIST *spk_inputs,
		 DYN_LIST *spk_times,
		 int fileindx, int obsindx,
		 FILE *fp,
		 DYN_LIST *obs_times,
		 essdg_ctx_t *ctx)
{
  ds_datapoint_t *d;
  int i;
  uint64_t evtime;
  int64_t thisobs;

  DYN_LIST *types, *subtypes, *times, *params;
  DYN_LIST *emdata, *emdata2, *h, *v, *info;
#ifdef HAVE_SPIKES
  int status, onspiket = -1, offspiket, lastsp = -1;
  DYN_LIST *s_types, *s_channels, *s_inputs, *s_times;
#endif
  int nchan = 2;
  int interval = 5;
  int moved_em = 0;

  char getting_trial = 0;

  types = dfuCreateDynList(DF_LONG, 10);
  subtypes = dfuCreateDynList(DF_LONG, 10);
  times = dfuCreateDynList(DF_LONG, 10);
  params = dfuCreateDynList(DF_LIST, 10);
  emdata = dfuCreateDynList(DF_LIST, 10);
  emdata2 = dfuCreateDynList(DF_LIST, 10);

  info = dfuCreateDynList(DF_LONG, 1);
  h = dfuCreateDynList(DF_SHORT, 1024);
  v = dfuCreateDynList(DF_SHORT, 1024);

  /*
   * This is the dpoint_read() loop for the program.  It is the
   * main worker, filling up arrays with em and spikes, filling up the
   * recording buffers with tags and values, etc.  This is called once per
   * observation period.
   */

  while (dpoint_read(fp, &d) > 0) {
    // stimdg
    if (d->data.type == DSERV_DG) {
      // d->varname is groupname so create columns d->varname::list_name
      DYN_GROUP *subdg;
      char listname[256];
      subdg = dfuCreateDynGroup(16);
      dguBufferToStruct(d->data.buf, d->data.len, subdg);
      for (i = 0; i < DYN_GROUP_N(subdg); i++) {
	snprintf(listname, sizeof(listname), "<%s>%s",
		 d->varname, DYN_LIST_NAME(DYN_GROUP_LIST(subdg, i)));
	dfuCopyDynGroupExistingList(dg, listname,
				    DYN_GROUP_LIST(subdg, i));
      }
      dfuFreeDynGroup(subdg);
      dpoint_free(d);
      continue;
    }

    // event names
    if (d->data.type == DSERV_STRING &&
	!strcmp(d->varname, "eventlog/names")) {
      add_event_names(evt_names, (char *) d->data.buf, d->data.len);
      dpoint_free(d);
      continue;
    }

    // eye movements
    if (!strcmp(d->varname, "ain/vals")) {
      add_emdata(info, h, v, interval, nchan,
		 (uint16_t *) d->data.buf, d->data.len/sizeof(uint16_t), 0);
      if (nchan > 2) {
	// add second set of data
      }
      dpoint_free(d);
      continue;
    }

    if (!ctx->have_t0) {
      ctx->file_t0 = d->timestamp;
      ctx->time_zero = d->timestamp;
      ctx->have_t0 = 1;
    }

    evtime = d->timestamp - ctx->time_zero;

    if (d->data.e.dtype == DSERV_EVT) {
      if (d->data.e.type == E_BEGINOBS) {
	if (getting_trial++) {
	  fprintf(stderr, "WARNING: BeginObs found with no EndObs\n");
	  dpoint_free(d);
	  goto done;
	}

	if (!ctx->got_first_obs) {
	  ctx->first_obs = d->timestamp;
	  ctx->got_first_obs = 1;
	}
	ctx->time_zero = d->timestamp;
	evtime = 0;

	dfuResetDynList(types);
	dfuResetDynList(subtypes);
	dfuResetDynList(times);
	dfuResetDynList(params);

	dfuResetDynList(emdata);
	dfuResetDynList(emdata2);
	dfuResetDynList(info);
	dfuResetDynList(h);
	dfuResetDynList(v);
      }

      if (getting_trial) {
	addSeparatedEvent(types, subtypes, times, params, d, evtime);
      }
      else
	addEvent(misc, d, evtime);

      if (d->data.e.type == E_ENDOBS) {
	if (getting_trial) {	/* can be false if user quit before next beginobs */
	  dfuAddDynListList(evt_types, types);
	  dfuAddDynListList(evt_subtypes, subtypes);
	  dfuAddDynListList(evt_times, times);
	  dfuAddDynListList(evt_params, params);

	  dfuMoveDynListList(emdata, info);
	  dfuMoveDynListList(emdata, h);
	  dfuMoveDynListList(emdata, v);

	  dfuMoveDynListList(ems, emdata);
	  if (nchan > 2 && ems2) {
	    dfuMoveDynListList(ems2, emdata2);
	  } else {
	    dfuFreeDynList(emdata2);
	  }
	  moved_em = 1;
#if 0
	  dfuMoveDynListList(spk_types, s_types);
	  dfuMoveDynListList(spk_channels, s_channels);
	  dfuMoveDynListList(spk_inputs, s_inputs);
	  dfuMoveDynListList(spk_times, s_times);
#endif
	  /* both in ms, anchored so they stay correct across files and
	     past 2^31 us (~36 min) into a session */
	  thisobs = (int64_t)(ctx->time_zero - ctx->first_obs);
	  dfuAddDynListLong(obs_times, (int)(thisobs/1000));
	  if (ctx->obs_start_ms) {
	    int64_t from_t0 = (int64_t)(ctx->time_zero - ctx->file_t0);
	    dfuAddDynListLong(ctx->obs_start_ms, (int)(from_t0/1000));
	  }
	}
	dfuFreeDynList(types);
	dfuFreeDynList(subtypes);
	dfuFreeDynList(times);
	dfuFreeDynList(params);
	if (!moved_em) {
	  dfuFreeDynList(emdata);
	  dfuFreeDynList(emdata2);
	  dfuFreeDynList(info);
	  dfuFreeDynList(h);
	  dfuFreeDynList(v);
	}

	dpoint_free(d);

	return(1);
      }
      else {
	dpoint_free(d);
      }
    }
    else {
      /* non-event, non-special datapoint: handled by the later <ds>/
	 <session>/<blob> passes -- this pass leaked one copy of every
	 such record (em, extio, ...): tens of MB per file in the
	 long-lived df subprocess */
      dpoint_free(d);
    }
  }

 done:
  dfuFreeDynList(types);
  dfuFreeDynList(subtypes);
  dfuFreeDynList(times);
  dfuFreeDynList(params);
  dfuFreeDynList(emdata);
  dfuFreeDynList(emdata2);
  dfuFreeDynList(info);
  dfuFreeDynList(h);
  dfuFreeDynList(v);

  return(0);
}


static int addEvent(DYN_LIST *evtdata, ds_datapoint_t *d, uint64_t evtime)
{
  DYN_LIST *evt, *data = NULL, *params = NULL;

  evt = dfuCreateDynList(DF_LIST, 2);
  data = dfuCreateDynList(DF_LONG, 4);

  dfuAddDynListLong(data, d->data.e.type);
  dfuAddDynListLong(data, d->data.e.subtype);
  dfuAddDynListLong(data, evtime/1000); /* convert to ms for now */

  params = create_val_list( d->data.e.puttype, d->data.len, d->data.buf);

  dfuAddDynListList(evt, data);
  dfuFreeDynList(data);

  if (!params) params = dfuCreateDynList(DF_LONG, 1);
  dfuAddDynListList(evt, params);
  dfuFreeDynList(params);
	
  dfuAddDynListList(evtdata, evt);
  dfuFreeDynList(evt);
  return 1;
}

static int addSeparatedEvent(DYN_LIST *types, DYN_LIST *subtypes, DYN_LIST *times, 
		      DYN_LIST *prms, ds_datapoint_t *d, uint64_t evtime)
{
  DYN_LIST *params = NULL;

  dfuAddDynListLong(types, d->data.e.type);
  dfuAddDynListLong(subtypes, d->data.e.subtype);
  dfuAddDynListLong(times, evtime/1000); /* convert to ms for now */

  params = create_val_list( d->data.e.puttype, d->data.len, d->data.buf);

  if (!params) params = dfuCreateDynList(DF_LONG, 1);
  dfuAddDynListList(prms, params);
  dfuFreeDynList(params);

  return 1;
}

/*
 * dslog_find_dsvars
 *
 *  find varnames in an open ess data file that should be included as additional data
 *  rewinds the file pointer to first datapoint after header before return
 *
 *  Also finds "session" variables - datapoints that occur outside of obs periods
 *  (before first E_BEGINOBS or after last E_ENDOBS). These are returned in
 *  session_varnames and session_types if non-NULL.
 *
 *  Blob-typed variables (JPEG/PPM payloads, e.g. camera frames) are
 *  collected separately in blob_varnames -- whole-file, not split by obs
 *  position -- and excluded from the <ds>/<session> lists (see
 *  dslog_add_blob_vars).
 */
static int dslog_find_dsvars(FILE *fp, DYN_LIST **varnames, DYN_LIST **types,
			     DYN_LIST **session_varnames, DYN_LIST **session_types,
			     DYN_LIST **blob_varnames)
{
  int i;
  int nvars = 0;
  int session_nvars = 0;
  int getting_trial = 0; /* only consider datapoints inside obs periods */
  int had_first_obs = 0; /* track if we've seen any obs period yet */
  ds_datapoint_t *d;
  DYN_LIST *typelist, *specialvars, *namelist;
  DYN_LIST *session_namelist, *session_typelist;
  DYN_LIST *blob_namelist;
  int special = 0, existing = 0;
  char *string;

  /* start at beginning */
  rewind(fp);

  /* if not dslog, return */
  if (dslog_read_header(fp, NULL, NULL) != 1) {
    return 0;
  }

  /* store list of found variables (trial-oriented) */
  namelist = dfuCreateDynList(DF_STRING, 10);
  typelist = dfuCreateDynList(DF_LONG, 10);

  /* store list of session variables (outside obs periods) */
  session_namelist = dfuCreateDynList(DF_STRING, 10);
  session_typelist = dfuCreateDynList(DF_LONG, 10);

  /* blob variables, in or out of obs */
  blob_namelist = dfuCreateDynList(DF_STRING, 4);

  /* these are handled by the main conversion function or are internal.
     NOTE the logger writes its synthetic obs markers with a COLON
     ("logger:beginobs", LogClient.h) -- the slash spellings here never
     matched, so every modern file grew two spurious empty <session>
     columns.  Both spellings are filtered to keep old and new readers
     quiet. */
  specialvars = dfuCreateDynList(DF_STRING, 10);
  dfuAddDynListString(specialvars, "eventlog/names");
  dfuAddDynListString(specialvars, "eventlog/events");
  dfuAddDynListString(specialvars, "ain/vals");
  dfuAddDynListString(specialvars, "logger/beginobs");
  dfuAddDynListString(specialvars, "logger/endobs");
  dfuAddDynListString(specialvars, "logger:beginobs");
  dfuAddDynListString(specialvars, "logger:endobs");

  while (dpoint_read(fp, &d) > 0) {
    if (d->data.e.dtype == DSERV_EVT) {
      if (d->data.e.type == E_BEGINOBS) {
	if (getting_trial++) {
	  /* should not happen, but indicates a problem data file */
	  dpoint_free(d);
	  dfuFreeDynList(namelist);
	  dfuFreeDynList(typelist);
	  dfuFreeDynList(session_namelist);
	  dfuFreeDynList(session_typelist);
	  dfuFreeDynList(blob_namelist);
	  dfuFreeDynList(specialvars);
	  rewind(fp);
	  dslog_read_header(fp, NULL, NULL);
	  /* outputs must not dangle on the error path */
	  if (varnames) *varnames = NULL;
	  if (types) *types = NULL;
	  if (session_varnames) *session_varnames = NULL;
	  if (session_types) *session_types = NULL;
	  if (blob_varnames) *blob_varnames = NULL;
	  return -1;
	}
	had_first_obs = 1;
	dpoint_free(d);
	continue;
      }

      else if (d->data.e.type == E_ENDOBS) {
	dpoint_free(d);
	getting_trial = 0;
	continue;
      }
    }

    /* does it match one of our known ESS vars? */
    for (special = 0, i = 0; i < DYN_LIST_N(specialvars); i++) {
      string = ((char **) DYN_LIST_VALS(specialvars))[i];
      if (!strcmp(d->varname, string)) {
	special = 1;
	break;
      }
    }

    /* skip special vars and DG types (DGs are unpacked in addObsPeriod) */
    if (special || d->data.type == DSERV_DG) {
      dpoint_free(d);
      continue;
    }

    /* blobs get their own file-level columns, wherever they occur */
    if (is_blob_dstype(d->data.type)) {
      for (existing = 0, i = 0; i < DYN_LIST_N(blob_namelist); i++) {
	string = ((char **) DYN_LIST_VALS(blob_namelist))[i];
	if (!strcmp(d->varname, string)) {
	  existing = 1;
	  break;
	}
      }
      if (!existing) {
	dfuAddDynListString(blob_namelist, d->varname);
      }
      dpoint_free(d);
      continue;
    }

    /* see if this is an "extra" varname we want to log */
    if (getting_trial) {
      /* Trial-oriented variable */

      /* does it match a variable already added? */
      for (existing = 0, i = 0; i < DYN_LIST_N(namelist); i++) {
	string = ((char **) DYN_LIST_VALS(namelist))[i];
	if (!strcmp(d->varname, string)) {
	  existing = 1;
	  break;
	}
      }

      /* if new, add to results */
      if (!existing) {
	nvars++;
	dfuAddDynListString(namelist, d->varname);
	dfuAddDynListLong(typelist, d->data.type);
      }
    }
    else {
      /* Session variable - outside obs periods */

      /* does it match a session variable already added? */
      for (existing = 0, i = 0; i < DYN_LIST_N(session_namelist); i++) {
	string = ((char **) DYN_LIST_VALS(session_namelist))[i];
	if (!strcmp(d->varname, string)) {
	  existing = 1;
	  break;
	}
      }

      /* if new, add to session results */
      if (!existing) {
	session_nvars++;
	dfuAddDynListString(session_namelist, d->varname);
	dfuAddDynListLong(session_typelist, d->data.type);
      }
    }
    dpoint_free(d);
  }

  /* return fp to first datapoint after header */
  rewind(fp);
  dslog_read_header(fp, NULL, NULL);

  dfuFreeDynList(specialvars);

  if (varnames) *varnames = namelist;
  else dfuFreeDynList(namelist);

  if (types) *types = typelist;
  else dfuFreeDynList(typelist);

  if (session_varnames) *session_varnames = session_namelist;
  else dfuFreeDynList(session_namelist);

  if (session_types) *session_types = session_typelist;
  else dfuFreeDynList(session_typelist);

  if (blob_varnames) *blob_varnames = blob_namelist;
  else dfuFreeDynList(blob_namelist);

  return nvars;
}


/*
 * dslog_add_session_vars
 *
 *  Add session-level variables (those outside obs periods) to the output dg.
 *  These are added as lists with <session> prefix, where each occurrence
 *  of the variable is appended to the list.
 */
static int dslog_add_session_vars(FILE *fp, DYN_LIST *namelist,
				  DYN_LIST *typelist, DYN_GROUP *dg)
{  
  int i, j, len;
  ds_datapoint_t *d;
  
  int result;
  int getting_trial = 0;
  int nvars;
  
  char *listname, **listnames;
  char **varnames;
  int *vartypes, *columnlist;
  DYN_LIST **dls;  /* accumulate values for each session var */
  
  if (!namelist || !typelist) return 0;
  if (DYN_LIST_N(namelist) != DYN_LIST_N(typelist)) return 0;
  nvars = DYN_LIST_N(namelist);
  if (nvars == 0) return 0;
  
  rewind(fp);
  if ((result = dslog_read_header(fp, NULL, NULL)) != 1) {
    return result == -1 ? DSLOG_FileUnreadable : DSLOG_InvalidFormat;
  }

  /* pointers to all varname strings we will monitor */
  varnames = (char **) DYN_LIST_VALS(namelist);
  vartypes = (int *) DYN_LIST_VALS(typelist);

  /* create listnames for output dg with <session> prefix */
  listnames = calloc(nvars, sizeof(char *));
  
  /* convert ':' to '/' in point names to be compatible with dlsh dgs */
  for (j = 0; j < nvars; j++) { 
    len = strlen(varnames[j]);
    /* make space for <session> prefix and NULL */
    listnames[j] = calloc(len + 9 + 1, sizeof(char));
    memcpy(listnames[j], "<session>", 9);
    for (i = 0; i < len; i++) {
      if (varnames[j][i] == ':')
	listnames[j][i+9] = '/';
      else
	listnames[j][i+9] = varnames[j][i];
    }
  }

  columnlist = (int *) calloc(nvars, sizeof(int));
  dls = (DYN_LIST **) calloc(nvars, sizeof(DYN_LIST *));

  /* add new columns to dg and track indices */
  for (i = 0; i < nvars; i++) {
    columnlist[i] = dfuAddDynGroupNewList(dg, listnames[i], DF_LIST, 10);
    free(listnames[i]);
  }
  free(listnames);

  /* scan through file collecting session variable values */
  while (dpoint_read(fp, &d) > 0) {
    /* track obs period state */
    if (d->data.e.dtype == DSERV_EVT) {
      if (d->data.e.type == E_BEGINOBS) {
	getting_trial = 1;
      }
      else if (d->data.e.type == E_ENDOBS) {
	getting_trial = 0;
      }
      dpoint_free(d);
      continue;
    }

    /* only process variables outside obs periods */
    if (!getting_trial) {
      for (i = 0; i < nvars; i++) {
	if (!strcmp(d->varname, varnames[i])) {
	  /* add this value to the accumulated list for this var */
	  dls[i] = add_dpoint_to_list(dls[i], d);
	  break;
	}
      }
    }
    dpoint_free(d);
  }

  /* move accumulated lists into the dg columns */
  for (i = 0; i < nvars; i++) {
    if (dls[i]) {
      dfuMoveDynListList(DYN_GROUP_LIST(dg, columnlist[i]), dls[i]);
    }
  }

  free(dls);
  free(columnlist);
  return 0;
}


/* build a column name: <prefix>varname with ':' translated to '/' */
static char *make_ds_listname(const char *prefix, const char *varname)
{
  int i, len = strlen(varname), plen = strlen(prefix);
  char *listname = calloc(len + plen + 1, sizeof(char));
  memcpy(listname, prefix, plen);
  for (i = 0; i < len; i++) {
    if (varname[i] == ':')
      listname[i+plen] = '/';
    else
      listname[i+plen] = varname[i];
  }
  return listname;
}

static int dslog_add_dsvars(FILE *fp, DYN_LIST *namelist,
			    DYN_LIST *typelist, DYN_GROUP *dg)
{
  int i;
  int result;
  int obsindx = 0;
  int nvars;

  char *listname;
  char **varnames;
  int *vartypes, *val_cols, *time_cols, *cnt_cols;

  if (!namelist || !typelist) return 0;
  if (DYN_LIST_N(namelist) != DYN_LIST_N(typelist)) return 0;
  nvars = DYN_LIST_N(namelist);

  rewind(fp);
  if ((result = dslog_read_header(fp, NULL, NULL)) != 1) {
    if (result == -1) {
      return DSLOG_FileUnreadable;
    }
    if (result == 0) {
      return DSLOG_InvalidFormat;
    }
  }

  /* pointers to all varname strings we will monitor */
  varnames = (char **) DYN_LIST_VALS(namelist);
  vartypes = (int *) DYN_LIST_VALS(typelist);

  val_cols = (int *) calloc(nvars, sizeof(int));
  time_cols = (int *) calloc(nvars, sizeof(int));
  cnt_cols = (int *) calloc(nvars, sizeof(int));

  /* Three parallel columns per variable, each one sublist per obs:
   *   <ds>name   the decoded values, concatenated across the obs's records
   *              (unchanged from the historical format)
   *   <dst>name  per-RECORD timestamps, ms from that obs's BEGINOBS --
   *              the same clock and anchor as e_times, so datapoint
   *              records and events sort on one axis
   *   <dsn>name  per-RECORD value counts, which split the concatenated
   *              <ds> values back into records (log buffering coalesces
   *              several datapoints into one record; extio ain blocks
   *              carry many samples per record)
   */
  for (i = 0; i < nvars; i++) {
    listname = make_ds_listname("<ds>", varnames[i]);
    val_cols[i] = dfuAddDynGroupNewList(dg, listname, DF_LIST, 32);
    free(listname);
    listname = make_ds_listname("<dst>", varnames[i]);
    time_cols[i] = dfuAddDynGroupNewList(dg, listname, DF_LIST, 32);
    free(listname);
    listname = make_ds_listname("<dsn>", varnames[i]);
    cnt_cols[i] = dfuAddDynGroupNewList(dg, listname, DF_LIST, 32);
    free(listname);
  }

  while (addDservObsPeriod(fp, nvars, varnames, vartypes,
			   val_cols, time_cols, cnt_cols, dg)) {
    obsindx++;
  }

  free(val_cols);
  free(time_cols);
  free(cnt_cols);
  return 0;
}

/*
 * dslog_add_blob_vars
 *
 *  File-level columns for blob-typed variables (JPEG/PPM frames):
 *
 *    <blob>name   DF_LIST -- one DF_CHAR byte vector per record
 *    <blobt>name  DF_LONG -- per-record ms from the file's first record
 *                 (same anchor as obs_start_ms, so per-obs request events
 *                  can be paired with capture times: request_abs_ms =
 *                  obs_start_ms[obs] + e_time)
 *
 *  Deliberately NOT split per obs by log position: a frame requested near
 *  the end of an obs is encoded asynchronously and can be written after
 *  ENDOBS (or into the next obs's span).  The record's TIMESTAMP is the
 *  capture time; assignment to obs/trials happens at extraction, by
 *  pairing with the request events.
 */
static int dslog_add_blob_vars(FILE *fp, DYN_LIST *namelist, DYN_GROUP *dg,
			       essdg_ctx_t *ctx)
{
  int i, result, nvars;
  ds_datapoint_t *d;
  char *listname;
  char **varnames;
  int *val_cols, *time_cols;
  DYN_LIST *payload;

  if (!namelist) return 0;
  nvars = DYN_LIST_N(namelist);
  if (nvars == 0) return 0;

  rewind(fp);
  if ((result = dslog_read_header(fp, NULL, NULL)) != 1) {
    return result == -1 ? DSLOG_FileUnreadable : DSLOG_InvalidFormat;
  }

  varnames = (char **) DYN_LIST_VALS(namelist);
  val_cols = (int *) calloc(nvars, sizeof(int));
  time_cols = (int *) calloc(nvars, sizeof(int));

  for (i = 0; i < nvars; i++) {
    listname = make_ds_listname("<blob>", varnames[i]);
    val_cols[i] = dfuAddDynGroupNewList(dg, listname, DF_LIST, 8);
    free(listname);
    listname = make_ds_listname("<blobt>", varnames[i]);
    time_cols[i] = dfuAddDynGroupNewList(dg, listname, DF_LONG, 8);
    free(listname);
  }

  while (dpoint_read(fp, &d) > 0) {
    /* a file with no events still needs a time anchor */
    if (!ctx->have_t0) {
      ctx->file_t0 = d->timestamp;
      ctx->time_zero = d->timestamp;
      ctx->have_t0 = 1;
    }
    if (is_blob_dstype(d->data.type)) {
      for (i = 0; i < nvars; i++) {
	if (!strcmp(d->varname, varnames[i])) {
	  payload = create_val_list(d->data.type, d->data.len, d->data.buf);
	  dfuMoveDynListList(DYN_GROUP_LIST(dg, val_cols[i]), payload);
	  dfuAddDynListLong(DYN_GROUP_LIST(dg, time_cols[i]),
			    (int)((int64_t)(d->timestamp - ctx->file_t0)/1000));
	  break;
	}
      }
    }
    dpoint_free(d);
  }

  free(val_cols);
  free(time_cols);
  return 0;
}


int dslog_to_essdg(char *filename, DYN_GROUP **outdg)
{  
  int j;
  ds_datapoint_t *d;
  
  int result;
  int version;
  double start_msec, time_msec;
  uint64_t timestamp;
  int datatype, etype, esubtype;
  int endindx = -1;
  FILE *fp;
  DYN_GROUP *dg;

  static char *name, dgname[64];

  int non_obs_oriented = 0;

  fp = fopen(filename, "rb");
  if (!fp) {
    return DSLOG_FileNotFound;
  }
  
  if ((result = dslog_read_header(fp, &version, &timestamp)) != 1) {
    if (result == -1) {
      fclose(fp);
      return DSLOG_FileUnreadable;
    }
    if (result == 0) {
      fclose(fp);
      return DSLOG_InvalidFormat;
    }
  }

  start_msec = timestamp/1000.;

  name = filename;
  get_dgname(name, dgname);
  
  
  if (non_obs_oriented) {
    DYN_LIST *varnames, *timestamps, *values, *vallist, *types, *subtypes;

    dg = dfuCreateNamedDynGroup(dgname, 3);
    
    j = dfuAddDynGroupNewList(dg, "varname", DF_STRING, 200);
    varnames = DYN_GROUP_LIST(dg, j);
    
    j = dfuAddDynGroupNewList(dg, "timestamp", DF_FLOAT, 200);
    timestamps = DYN_GROUP_LIST(dg, j);
    
    j = dfuAddDynGroupNewList(dg, "type", DF_LONG, 200);
    types = DYN_GROUP_LIST(dg, j);
    
    j = dfuAddDynGroupNewList(dg, "subtype", DF_LONG, 200);
    subtypes = DYN_GROUP_LIST(dg, j);
    
    j = dfuAddDynGroupNewList(dg, "vals", DF_LIST, 200);
    values = DYN_GROUP_LIST(dg, j);
    
    time_msec = start_msec;
    
    /* Insert initial event which includes open timestamp and version */
    dfuAddDynListString(varnames, "logger:open");
    dfuAddDynListFloat(timestamps, time_msec-start_msec);
    
    dfuAddDynListLong(types, version);
    dfuAddDynListLong(subtypes, 0);
    
    vallist = create_val_list(DSERV_NONE, 0, NULL);
    dfuMoveDynListList(values, vallist);
    
    while (dpoint_read(fp, &d) > 0) {
      time_msec =  d->timestamp/1000.;
      dfuAddDynListString(varnames, d->varname);
      dfuAddDynListFloat(timestamps, time_msec-start_msec);
      
      if (d->data.e.dtype == DSERV_EVT)
	datatype = d->data.e.puttype;
      else
	datatype = d->data.e.dtype;
      
      etype = d->data.e.type;
      esubtype = d->data.e.subtype;
      dfuAddDynListLong(types, etype);
      dfuAddDynListLong(subtypes, esubtype);
      
      vallist = create_val_list(datatype, d->data.len, d->data.buf);
      dfuMoveDynListList(values, vallist);
    }
  }

  else {
    int obsindx = 0, fileindx = 0;
    DYN_LIST *evt_names, *evt_types, *evt_subtypes, *evt_times, *evt_params;
    DYN_LIST *ems, *ems2, *misc;
    DYN_LIST *spk_types, *spk_channels, *spk_times, *spk_inputs;
    DYN_LIST *obs_times, *obs_start_ms;

    DYN_LIST *extra_vars, *extra_types;

    essdg_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    dg = dfuCreateNamedDynGroup(dgname, 12);
    
    j = dfuAddDynGroupNewList(dg, "e_pre", DF_LIST, 10);
    misc = DYN_GROUP_LIST(dg, j);
    
    j = dfuAddDynGroupNewList(dg, "e_names", DF_STRING, 64);
    evt_names = DYN_GROUP_LIST(dg, j);
    
    j = dfuAddDynGroupNewList(dg, "e_types", DF_LIST, 64);
    evt_types = DYN_GROUP_LIST(dg, j);
    
    j = dfuAddDynGroupNewList(dg, "e_subtypes", DF_LIST, 64);
    evt_subtypes = DYN_GROUP_LIST(dg, j);
    
    j = dfuAddDynGroupNewList(dg, "e_times", DF_LIST, 64);
    evt_times = DYN_GROUP_LIST(dg, j);
    
    j = dfuAddDynGroupNewList(dg, "e_params", DF_LIST, 64);
    evt_params = DYN_GROUP_LIST(dg, j);
    
    j = dfuAddDynGroupNewList(dg, "ems", DF_LIST, 64);
    ems = DYN_GROUP_LIST(dg, j);

    j = dfuAddDynGroupNewList(dg, "ems2", DF_LIST, 64);
    ems2 = DYN_GROUP_LIST(dg, j);
    
    j = dfuAddDynGroupNewList(dg, "spk_types", DF_LIST, 64);
    spk_types = DYN_GROUP_LIST(dg, j);

    j = dfuAddDynGroupNewList(dg, "spk_channels", DF_LIST, 64);
    spk_channels = DYN_GROUP_LIST(dg, j);

    j = dfuAddDynGroupNewList(dg, "spk_inputs", DF_LIST, 64);
    spk_inputs = DYN_GROUP_LIST(dg, j);

    j = dfuAddDynGroupNewList(dg, "spk_times", DF_LIST, 64);
    spk_times = DYN_GROUP_LIST(dg, j);

    j = dfuAddDynGroupNewList(dg, "obs_times", DF_LONG, 64);
    obs_times = DYN_GROUP_LIST(dg, j);

    /* each obs's BEGINOBS in ms from the file's FIRST record -- the common
       axis that lets per-obs event times (ms from BEGINOBS) be compared
       with file-level <blobt> capture times */
    j = dfuAddDynGroupNewList(dg, "obs_start_ms", DF_LONG, 64);
    obs_start_ms = DYN_GROUP_LIST(dg, j);
    ctx.obs_start_ms = obs_start_ms;

    while(addObsPeriod(dg, evt_names, evt_types, evt_subtypes,
		       evt_times, evt_params,
		       misc, ems, ems2,
		       spk_types, spk_channels, spk_inputs, spk_times,
		       fileindx, obsindx, fp, obs_times, &ctx)) {
      obsindx++;
      if (endindx > 0 && obsindx >= endindx) break;
    }

    /* now go back and add extra vars */
    {
      DYN_LIST *session_vars, *session_types, *blob_vars;

      dslog_find_dsvars(fp, &extra_vars, &extra_types,
			&session_vars, &session_types, &blob_vars);
      dslog_add_dsvars(fp, extra_vars, extra_types, dg);

      /* add session-level variables (outside obs periods) */
      dslog_add_session_vars(fp, session_vars, session_types, dg);

      /* file-level blob columns (camera frames), paired by timestamp */
      dslog_add_blob_vars(fp, blob_vars, dg, &ctx);

      if (extra_vars) dfuFreeDynList(extra_vars);
      if (extra_types) dfuFreeDynList(extra_types);
      if (session_vars) dfuFreeDynList(session_vars);
      if (session_types) dfuFreeDynList(session_types);
      if (blob_vars) dfuFreeDynList(blob_vars);
    }
  }

  fclose(fp);

  /* return the newly created dg in outdg */
  if (outdg) *outdg = dg;
  else dfuFreeDynGroup(dg);

  return 0;
}


/*********************************************************************/
/*             extract dataserver data in obs format                 */
/*********************************************************************/

static int addDservObsPeriod(
			     FILE *fp,        /* open data file FP   */
			     int nvars,       /* number of dpoints   */
			     char **varnames, /* name of datapoints  */
			     int *vartypes,   /* type of datapoints  */
			     int *val_cols,   /* <ds> column ids     */
			     int *time_cols,  /* <dst> column ids    */
			     int *cnt_cols,   /* <dsn> column ids    */
			     DYN_GROUP *dg)   /* output dg           */
{
  ds_datapoint_t *d;
  char getting_trial = 0;
  int i, matched;
  uint64_t obs_t0 = 0;

  /* per-var accumulators: values (concatenated), record times, counts */
  DYN_LIST **dls = (DYN_LIST **) calloc(nvars, sizeof(DYN_LIST *));
  DYN_LIST **tls = (DYN_LIST **) calloc(nvars, sizeof(DYN_LIST *));
  DYN_LIST **nls = (DYN_LIST **) calloc(nvars, sizeof(DYN_LIST *));

  /*
   * Call dpoint_read() to move through next obs period
   */

  while (dpoint_read(fp, &d) > 0) {
    matched = 0;
    /* check for varname match */
    if (getting_trial) {
      for (i = 0; i < nvars; i++) {
	if (!strcmp(d->varname, varnames[i])) {
	  dls[i] = add_dpoint_to_list(dls[i], d);
	  if (!tls[i]) tls[i] = dfuCreateDynList(DF_LONG, 8);
	  dfuAddDynListLong(tls[i],
			    (int)((int64_t)(d->timestamp - obs_t0)/1000));
	  if (!nls[i]) nls[i] = dfuCreateDynList(DF_LONG, 8);
	  dfuAddDynListLong(nls[i], dpoint_val_count(d));
	  dpoint_free(d);
	  matched = 1;
	  break;
	}
      }
      if (matched) continue;
    }

    if (d->data.e.dtype == DSERV_EVT) {
      if (d->data.e.type == E_BEGINOBS) {
	if (getting_trial++) {
	  fprintf(stderr, "WARNING: BeginObs found with no EndObs\n");
	  dpoint_free(d);
	  for (i = 0; i < nvars; i++) {
	    if (dls[i]) dfuFreeDynList(dls[i]);
	    if (tls[i]) dfuFreeDynList(tls[i]);
	    if (nls[i]) dfuFreeDynList(nls[i]);
	  }
	  free(dls); free(tls); free(nls);
	  return 0;
	}
	obs_t0 = d->timestamp;
	dpoint_free(d);
      }
      else if (d->data.e.type == E_ENDOBS) {
	dpoint_free(d);
	for (i = 0; i < nvars; i++) {
	  if (dls[i]) {
	    dfuMoveDynListList(DYN_GROUP_LIST(dg, val_cols[i]), dls[i]);
	    dfuMoveDynListList(DYN_GROUP_LIST(dg, time_cols[i]), tls[i]);
	    dfuMoveDynListList(DYN_GROUP_LIST(dg, cnt_cols[i]), nls[i]);
	  }
	  else {
	    /* this obs was empty: empty sublists, correctly typed (the
	       raw ds type was fed to dfuCreateDynList here before --
	       a different enum space, so empty obs got wrongly-typed
	       placeholders and columns went type-heterogeneous) */
	    DYN_LIST *empty = dfuCreateDynList(df_type_for_dstype(vartypes[i]), 1);
	    dfuMoveDynListList(DYN_GROUP_LIST(dg, val_cols[i]), empty);
	    dfuMoveDynListList(DYN_GROUP_LIST(dg, time_cols[i]),
			       dfuCreateDynList(DF_LONG, 1));
	    dfuMoveDynListList(DYN_GROUP_LIST(dg, cnt_cols[i]),
			       dfuCreateDynList(DF_LONG, 1));
	  }
	}
	free(dls); free(tls); free(nls);
	return 1;
      }
      else {
	dpoint_free(d);
      }
    }
    else {
      dpoint_free(d);   /* non-matching, non-event record (was leaked) */
    }
  }

  for (i = 0; i < nvars; i++) {
    if (dls[i]) dfuFreeDynList(dls[i]);
    if (tls[i]) dfuFreeDynList(tls[i]);
    if (nls[i]) dfuFreeDynList(nls[i]);
  }
  free(dls); free(tls); free(nls);
  return 0;
}

#if 0
static int dslog_get_dpointdg(char *filename, int nvars, char **varnames, DYN_GROUP **outdg)
{  
  int i, j, len;
  ds_datapoint_t *d;
  
  int result;
  int version;
  FILE *fp;
  DYN_GROUP *dg;
  uint64_t timestamp;
  DYN_LIST *dl = NULL;
  int obsindx = 0;
  
  static char *name, dgname[64], *listname, **listnames;
  
  fp = fopen(filename, "rb");
  if (!fp) {
    return DSLOG_FileNotFound;
  }
  
  if ((result = dslog_read_header(fp, &version, &timestamp)) != 1) {
    if (result == -1) {
      fclose(fp);
      return DSLOG_FileUnreadable;
    }
    if (result == 0) {
      fclose(fp);
      return DSLOG_InvalidFormat;
    }
  }

  /* create listnames for output dg */
  listnames = calloc(nvars, sizeof(char *));

  /* convert ':' to '/' in point names to be compatible with dlsh dgs */
  for (j = 0; j < nvars; j++) { 
    len = strlen(varnames[j]);
    listnames[j] = calloc(len+1, sizeof(char));
    for (i = 0; i < len; i++) {
      if (varnames[j][i] == ':') listnames[j][i] = '/';
      else listnames[j][i] = varnames[j][i];
    }
  }

  /* create output dg */
  dg = dfuCreateDynGroup(nvars);
  for (i = 0; i < nvars; i++) {
    dfuAddDynGroupNewList(dg, listnames[i], DF_LIST, 32);
  }
  
  while (addDservObsPeriod(fp, nvars, varnames, dg)) {
    obsindx++;
  }

  free(listnames);
  
  /* return the newly created dg in outdg */
  if (outdg) *outdg = dg;
  else dfuFreeDynGroup(dg);

  fclose(fp);
  return 0;
}
#endif
