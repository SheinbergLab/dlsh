/**
 * dgarrow.h - Arrow C Data Interface for DYN_GROUP/DYN_LIST
 * 
 * Lightweight Arrow IPC serialization using only the C Data Interface ABI.
 * No external Arrow library dependencies required.
 * 
 * Compatible with PyArrow, Arrow JS, R arrow package, and all Arrow implementations.
 */

#ifndef DGARROW_H
#define DGARROW_H

#ifdef __cplusplus
extern "C" {
#endif

// Serialization functions
int dg_to_arrow_buffer(DYN_GROUP* dg, uint8_t** data, size_t* size);
int dg_to_arrow_file(DYN_GROUP* dg, const char* filename);

// Deserialization functions
DYN_GROUP* arrow_buffer_to_dg(const uint8_t* data, size_t size, const char* group_name);
DYN_GROUP* arrow_file_to_dg(const char* filename, const char* group_name);

#ifdef __cplusplus
}
#endif

#endif /* DGARROW_H */
