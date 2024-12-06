/*
 * helper functions for handling dslog files created by dserv
 */

typedef enum
{
  DSLOG_OK, DSLOG_FileNotFound, DSLOG_FileUnreadable, DSLOG_InvalidFormat, DSLOG_RCS
} DSLOG_RC;

int dslog_to_dg(char *filename, DYN_GROUP **outdg);
int dslog_to_essdg(char *filename, DYN_GROUP **outdg);
