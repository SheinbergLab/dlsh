// dserv datapoints

#ifndef DATAPOINT_H
#define DATAPOINT_H

#include <stdint.h>

typedef enum {
  DSERV_BYTE = 0,
  DSERV_STRING,
  DSERV_FLOAT,
  DSERV_DOUBLE,
  DSERV_SHORT,
  DSERV_INT,
  DSERV_DG,
  DSERV_SCRIPT,
  DSERV_TRIGGER_SCRIPT,		/* will always be delivered to trigger thread */
  DSERV_EVT,
  DSERV_NONE,
  DSERV_UNKNOWN,
} ds_datatype_t;

typedef enum {
  DSERV_DPOINT_NOT_INITIALIZED = 0x01,
  DSERV_DPOINT_FREE_FLAG = 0x02,
  DSERV_DPOINT_LOGPAUSE_FLAG = 0x04,
  DSERV_DPOINT_LOGSTART_FLAG = 0x08,
  DSERV_DPOINT_SHUTDOWN_FLAG = 0x10,
  DSERV_DPOINT_LOGFLUSH_FLAG = 0x20,
} ds_datapoint_flag_t;
  
enum { DSERV_CREATE, DSERV_CLEAR, DSERV_SET, DSERV_GET, DSERV_GET_EVENT };
enum { DSERV_GET_FIRST_KEY, DSERV_GET_NEXT_KEY };

#define DPOINT_BINARY_MSG_CHAR '>'
#define DPOINT_BINARY_FIXED_LENGTH (128)

typedef struct ds_event_info_s {
  uint8_t dtype;
  uint8_t type;
  uint8_t subtype;
  uint8_t puttype;
} ds_event_info_t;

typedef struct ds_data
{
  union {
    ds_datatype_t type;
    ds_event_info_t e;
  };
  uint32_t len;
  unsigned char *buf;
} ds_data_t;

typedef struct ds_datapoint
{
  uint64_t timestamp;
  uint32_t flags;
  uint16_t varlen;                  // len of string + 1 (for NULL term)
  char *varname;
  ds_data_t data;
} ds_datapoint_t;

#endif /* DATAPOINT_H */
