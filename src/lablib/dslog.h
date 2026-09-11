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
  DSLOG_OK, DSLOG_FileNotFound, DSLOG_FileUnreadable, DSLOG_InvalidFormat, DSLOG_RCS
} DSLOG_RC;

#ifdef __cplusplus
extern "C" {
#endif

int dslog_to_dg(char *filename, DYN_GROUP **outdg);
int dslog_to_essdg(char *filename, DYN_GROUP **outdg);

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
