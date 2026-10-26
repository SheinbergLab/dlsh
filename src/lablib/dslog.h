/*
 * helper functions for handling dslog files created by dserv
 */

#ifndef DSLOG_H
#define DSLOG_H

#include <stdio.h>
#include "datapoint.h"

#define DSERV_LOG_CURRENT_VERSION 3

typedef enum
{
  DSLOG_OK, DSLOG_FileNotFound, DSLOG_FileUnreadable, DSLOG_InvalidFormat,
  DSLOG_BadRecord,   /* a record's content did not fit the request; the
                        caller's errbuf says which record and why */
  DSLOG_RCS
} DSLOG_RC;

#ifdef __cplusplus
extern "C" {
#endif

int dslog_to_dg(char *filename, DYN_GROUP **outdg);
int dslog_to_essdg(char *filename, DYN_GROUP **outdg);

/* dslog_to_dg with two options.  `filter`, when non-NULL, is asked about
   every record's displayed name (the varname, or evt:TYPE:SUBTYPE for an
   event record, and the synthetic logger:open row) and only the records it
   accepts are decoded and appended; the file is still scanned end to end.
   `absolute` makes the <timestamp> column DF_INT64 epoch microseconds, the
   record timestamps exactly as logged, instead of float/double seconds from
   the first record.  With a NULL filter and absolute 0 the result is
   identical to dslog_to_dg. */
typedef int (*dslog_name_filter_t)(void *ctx, const char *name);
int dslog_to_dg_ex(char *filename, dslog_name_filter_t filter, void *ctx,
                   int absolute, DYN_GROUP **outdg);

/* Fixed-width array datapoint -> one row per record, one column per
   element.  The group is named after the datapoint and holds <timestamp>
   (DF_INT64 epoch us), <seconds> (DF_DOUBLE from the file's first record)
   and then one list per field: DF_INT64 for DSERV_INT64 and DSERV_INT
   payloads, DF_DOUBLE / DF_FLOAT / DF_SHORT for DSERV_DOUBLE / FLOAT /
   SHORT.  A record whose element count is not nfields, or whose type
   differs from the first record's, or no record at all, is DSLOG_BadRecord
   with the reason in errbuf. */
int dslog_unpack(char *filename, const char *name, int nfields,
                 char **fields, DYN_GROUP **outdg,
                 char *errbuf, size_t errlen);

/* JSON datapoint (DSERV_JSON, or DSERV_STRING holding an object) -> one
   row per record, one column per top-level scalar key in first-seen order:
   integers and booleans DF_INT64, reals DF_DOUBLE (a key that is integer in
   one record and real in another is promoted to double), strings
   DF_STRING.  Objects and arrays are not columns.  A key absent from a
   record, or null in it, holds the column's missing marker: INT64_MIN, NaN
   or "".  A record that does not parse as a JSON object, a key whose type
   changes between string and number, or no record at all, is
   DSLOG_BadRecord with the reason in errbuf. */
int dslog_json_table(char *filename, const char *name, DYN_GROUP **outdg,
                     char *errbuf, size_t errlen);

/* Whether DSERV_DOUBLE and DSERV_INT64 datapoints become DF_DOUBLE and
   DF_INT64 columns (1), or are narrowed to float / dropped as they always
   were (0, the default).  On, the per-record time columns <dst>NAME and
   <blobt>NAME are DF_DOUBLE too -- still milliseconds from the same
   anchors, but keeping the microsecond fraction instead of truncating to
   whole ms.  Off by default because a dg holding the 8-byte tags cannot
   be opened by any reader built before those tags existed; turn it on per
   rig once every consumer of its files has been updated.
   dslog_set_wide_types returns the previous setting. */
int dslog_set_wide_types(int on);
int dslog_get_wide_types(void);

/* Low-level datapoint I/O for stream manipulation */
int dslog_read_header(FILE *fp, int *version, uint64_t *timestamp);
int dslog_write_header(int fd, uint64_t timestamp);
int dpoint_read(FILE *fp, ds_datapoint_t **dpoint);
int dpoint_write(int fd, ds_datapoint_t *dpoint);
void dpoint_free(ds_datapoint_t *d);

#ifdef __cplusplus
}
#endif

#endif /* DSLOG_H */
