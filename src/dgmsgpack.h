/*
 * dgmsgpack.h
 * MessagePack I/O support for dynamic groups
 */

#ifndef DGMSGPACK_H
#define DGMSGPACK_H

#include <stddef.h>
#include "df.h"  // for DYN_GROUP and DYN_LIST

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert DYN_GROUP to MessagePack buffer (columnar format)
 * Returns 0 on success, -1 on failure
 * Caller must free() the returned buffer
 */
int dg_to_msgpack_buffer(DYN_GROUP *dg, char **buffer, size_t *buffer_size);

/**
 * Convert DYN_GROUP to hybrid MessagePack format
 * Primitives in rows, arrays in lookup table - efficient for frontend processing
 * Returns 0 on success, -1 on failure
 * Caller must free() the returned buffer
 */
int dg_to_hybrid_msgpack_buffer(DYN_GROUP *dg, char **buffer, size_t *buffer_size);

/**
 * Write DYN_GROUP to MessagePack file (columnar format)
 * Returns 0 on success, -1 on failure
 */
int dg_write_msgpack_file(DYN_GROUP *dg, const char *filename);

/**
 * Write DYN_GROUP to hybrid MessagePack file
 * Returns 0 on success, -1 on failure
 */
int dg_write_hybrid_msgpack_file(DYN_GROUP *dg, const char *filename);

/**
 * Get MessagePack data as buffer for network transmission (columnar format)
 * Returns 0 on success, -1 on failure
 * Caller must free() the returned buffer
 */
int dg_get_msgpack_data(DYN_GROUP *dg, char **data, size_t *size);

/**
 * Get hybrid MessagePack data as buffer for network transmission
 * Returns 0 on success, -1 on failure
 * Caller must free() the returned buffer
 */
int dg_get_hybrid_msgpack_data(DYN_GROUP *dg, char **data, size_t *size);

#ifdef __cplusplus
}
#endif

#endif // DGMSGPACK_H