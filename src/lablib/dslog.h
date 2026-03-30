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
