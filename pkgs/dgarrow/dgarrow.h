// dgarrow.h
#ifndef DGARROW_H
#define DGARROW_H

#include <stdint.h>
#include <stddef.h>
#include "df.h"  // for DYN_GROUP

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write a DYN_GROUP to an Arrow IPC file
 * Returns 0 on success, -1 on failure
 */
int dg_write_arrow_file(DYN_GROUP* dg, const char* filename);

/**
 * Get Arrow serialized data as byte array
 * Returns 0 on success, -1 on failure
 * Caller must free() the returned data
 */
int dg_get_arrow_data(DYN_GROUP* dg, uint8_t** data, size_t* size);

/**
 * Deserialize Arrow IPC data back to DYN_GROUP with custom name
 * Returns new DYN_GROUP or NULL on failure  
 * Caller must free the returned DYN_GROUP with dfuFreeDynGroup()
 */
DYN_GROUP* dg_deserialize_from_arrow_with_name(const uint8_t* data, size_t size, const char* group_name);

/**
 * Read a DYN_GROUP from an Arrow IPC file
 * Returns new DYN_GROUP or NULL on failure
 * Caller must free the returned DYN_GROUP with dfuFreeDynGroup()
 */
DYN_GROUP* dg_read_arrow_file(const char* filename, const char* group_name);

#ifdef __cplusplus
}
#endif

#endif // DGARROW_H