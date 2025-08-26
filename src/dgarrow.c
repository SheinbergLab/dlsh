// dgarrow.c - Arrow IPC format serialization/deserialization for DYN_GROUP
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nanoarrow/nanoarrow.h>
#include <nanoarrow/nanoarrow_ipc.h>

#include <tcl.h>
#include "df.h"
#include "dynio.h"
#include "tcl_dl.h"
#include "dgarrow.h"

// Debug output control
#ifdef DEBUG_ARROW
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while(0)
#endif

// Always print errors to stderr
#define ERROR_PRINT(...) fprintf(stderr, __VA_ARGS__)

/*************************************************************************************/
/********************************* SERIALIZATION *************************************/
/*************************************************************************************/

// Helper to determine Arrow type from DYN_LIST datatype
static enum ArrowType dynlist_get_arrow_type(DYN_LIST* dl) {
    if (!dl) return NANOARROW_TYPE_NA;
    
    switch (DYN_LIST_DATATYPE(dl)) {
        case DF_LONG:   return NANOARROW_TYPE_INT32;
        case DF_SHORT:  return NANOARROW_TYPE_INT16;
        case DF_CHAR:   return NANOARROW_TYPE_UINT8;
        case DF_FLOAT:  return NANOARROW_TYPE_FLOAT;
        case DF_STRING: return NANOARROW_TYPE_STRING;
        case DF_LIST:   return NANOARROW_TYPE_LIST;
        default:        return NANOARROW_TYPE_NA;
    }
}

// Recursively determine the ultimate child type for nested lists
static enum ArrowType get_ultimate_child_type(DYN_LIST* dl) {
    if (!dl) {
        ERROR_PRINT("ERROR: get_ultimate_child_type called with NULL DYN_LIST\n");
        return NANOARROW_TYPE_NA;
    }
    
    if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
        return dynlist_get_arrow_type(dl);
    }
    
    // For lists, examine elements to determine child type
    DYN_LIST** vals = (DYN_LIST**)DYN_LIST_VALS(dl);
    if (DYN_LIST_N(dl) == 0) {
        ERROR_PRINT("ERROR: Empty top-level nested list - cannot determine child type\n");
        return NANOARROW_TYPE_NA;
    }
    
    // Look for first non-empty sublist to determine type
    for (int i = 0; i < DYN_LIST_N(dl); i++) {
        if (!vals[i]) {
            ERROR_PRINT("ERROR: NULL sublist at index %d\n", i);
            return NANOARROW_TYPE_NA;
        }
        
        // If this sublist has elements, use it to determine type
        if (DYN_LIST_N(vals[i]) > 0) {
            return get_ultimate_child_type(vals[i]);
        }
        // If empty sublist, continue to next one
    }
    
    // All sublists are empty - we need to check their declared types
    // Use the first sublist's type (they should all be the same)
    return get_ultimate_child_type(vals[0]);
}


// Helper to build schema recursively for arbitrary nesting depth
static int build_schema_from_dynlist(DYN_LIST* dl, struct ArrowSchema* schema) {
    if (!dl || !schema) return -1;
    
    if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
        // This is a list - build list schema
        if (ArrowSchemaInitFromType(schema, NANOARROW_TYPE_LIST) != NANOARROW_OK) {
            return -1;
        }
        
        if (ArrowSchemaSetName(schema, DYN_LIST_NAME(dl)) != NANOARROW_OK) {
            ArrowSchemaRelease(schema);
            return -1;
        }
        
        // Find first non-null child to determine structure
        DYN_LIST** vals = (DYN_LIST**)DYN_LIST_VALS(dl);
        DYN_LIST* sample = NULL;
        for (int i = 0; i < DYN_LIST_N(dl); i++) {
            if (vals[i]) {
                sample = vals[i];
                break;
            }
        }
        
        if (!sample) {
            ERROR_PRINT("ERROR: All children are NULL\n");
            ArrowSchemaRelease(schema);
            return -1;
        }
        
        // Recursively build child schema
        if (build_schema_from_dynlist(sample, schema->children[0]) != 0) {
            ArrowSchemaRelease(schema);
            return -1;
        }
        
        // Set conventional name for list items
        if (ArrowSchemaSetName(schema->children[0], "item") != NANOARROW_OK) {
            ArrowSchemaRelease(schema);
            return -1;
        }
    } else {
        // Primitive type - build appropriate schema
        enum ArrowType type = dynlist_get_arrow_type(dl);
        if (ArrowSchemaInitFromType(schema, type) != NANOARROW_OK) {
            return -1;
        }
        
        if (ArrowSchemaSetName(schema, DYN_LIST_NAME(dl)) != NANOARROW_OK) {
            ArrowSchemaRelease(schema);
            return -1;
        }
    }
    
    return 0;
}

// Helper to initialize array structure recursively (without data)
static int init_array_from_schema(struct ArrowSchema* schema, struct ArrowArray* array) {
    if (!schema || !array) return -1;
    
    if (strcmp(schema->format, "+l") == 0) {
        // List type
        if (ArrowArrayInitFromType(array, NANOARROW_TYPE_LIST) != NANOARROW_OK) {
            return -1;
        }
        
        if (ArrowArrayStartAppending(array) != NANOARROW_OK) {
            ArrowArrayRelease(array);
            return -1;
        }
        
        if (ArrowArrayAllocateChildren(array, 1) != NANOARROW_OK) {
            ArrowArrayRelease(array);
            return -1;
        }
        
        // Recursively initialize child
        if (init_array_from_schema(schema->children[0], array->children[0]) != 0) {
            ArrowArrayRelease(array);
            return -1;
        }
    } else {
        // Primitive type
        enum ArrowType type = NANOARROW_TYPE_NA;
        if (strcmp(schema->format, "i") == 0) type = NANOARROW_TYPE_INT32;
        else if (strcmp(schema->format, "s") == 0) type = NANOARROW_TYPE_INT16;
        else if (strcmp(schema->format, "C") == 0) type = NANOARROW_TYPE_UINT8;
        else if (strcmp(schema->format, "f") == 0) type = NANOARROW_TYPE_FLOAT;
        else if (strcmp(schema->format, "g") == 0) type = NANOARROW_TYPE_DOUBLE;
        else if (strcmp(schema->format, "u") == 0) type = NANOARROW_TYPE_STRING;
        
        if (ArrowArrayInitFromType(array, type) != NANOARROW_OK) {
            return -1;
        }
        
        if (ArrowArrayStartAppending(array) != NANOARROW_OK) {
            ArrowArrayRelease(array);
            return -1;
        }
    }
    
    return 0;
}

// Helper to append DYN_LIST data to Arrow array recursively
static int append_dynlist_data(DYN_LIST* dl, struct ArrowArray* array) {
    if (!dl || !array) return -1;
    
    if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
        // List of lists - process each sublist
        DYN_LIST** vals = (DYN_LIST**)DYN_LIST_VALS(dl);
        int n = DYN_LIST_N(dl);
        
        for (int i = 0; i < n; i++) {
            if (!vals[i]) {
                ERROR_PRINT("ERROR: NULL sublist at index %d\n", i);
                return -1;
            }
            
            // Recursively append sublist data
            if (append_dynlist_data(vals[i], array->children[0]) != 0) {
                return -1;
            }
            
            // Mark end of this list element
            if (ArrowArrayFinishElement(array) != NANOARROW_OK) {
                return -1;
            }
        }
    } else {
        // Primitive list - append all values
        int n = DYN_LIST_N(dl);
        
        switch (DYN_LIST_DATATYPE(dl)) {
            case DF_LONG: {
                int* vals = (int*)DYN_LIST_VALS(dl);
                for (int i = 0; i < n; i++) {
                    if (ArrowArrayAppendInt(array, vals[i]) != NANOARROW_OK) return -1;
                }
                break;
            }
            case DF_SHORT: {
                short* vals = (short*)DYN_LIST_VALS(dl);
                for (int i = 0; i < n; i++) {
                    if (ArrowArrayAppendInt(array, vals[i]) != NANOARROW_OK) return -1;
                }
                break;
            }
            case DF_CHAR: {
                char* vals = (char*)DYN_LIST_VALS(dl);
                for (int i = 0; i < n; i++) {
                    if (ArrowArrayAppendUInt(array, (uint64_t)(unsigned char)vals[i]) != NANOARROW_OK) return -1;
                }
                break;
            }
            case DF_FLOAT: {
                float* vals = (float*)DYN_LIST_VALS(dl);
                for (int i = 0; i < n; i++) {
                    if (ArrowArrayAppendDouble(array, (double)vals[i]) != NANOARROW_OK) return -1;
                }
                break;
            }
            case DF_STRING: {
                char** vals = (char**)DYN_LIST_VALS(dl);
                for (int i = 0; i < n; i++) {
                    if (!vals[i]) {
                        ERROR_PRINT("ERROR: NULL string at index %d\n", i);
                        return -1;
                    }
                    struct ArrowStringView sv = { .data = vals[i], .size_bytes = strlen(vals[i]) };
                    if (ArrowArrayAppendString(array, sv) != NANOARROW_OK) return -1;
                }
                break;
            }
        }
    }
    
    return 0;
}

// Helper to finish building arrays recursively (bottom-up)
static int finish_array_recursive(struct ArrowArray* array, struct ArrowSchema* schema) {
    if (!array || !schema) return -1;
    
    struct ArrowError error;
    
    // If this is a list with children, finish children first (bottom-up)
    if (strcmp(schema->format, "+l") == 0 && array->children && array->children[0]) {
        if (finish_array_recursive(array->children[0], schema->children[0]) != 0) {
            return -1;
        }
    }
    
    // Now finish this array
    if (ArrowArrayFinishBuildingDefault(array, &error) != NANOARROW_OK) {
        ERROR_PRINT("ERROR: Failed to finish array: %s\n", error.message);
        return -1;
    }
    
    return 0;
}

// Main conversion function using the helpers
static int dynlist_to_nanoarrow_array(DYN_LIST* dl, struct ArrowArray* array, 
                                      struct ArrowSchema* schema) {
    if (!dl || !array || !schema) return -1;
    
    DEBUG_PRINT("DEBUG: Converting DYN_LIST '%s', type=%d, n=%d\n", 
                DYN_LIST_NAME(dl), DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
    
    // For non-list types (plain columns), use the simpler direct approach
    if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
        // Build simple schema
        enum ArrowType type = dynlist_get_arrow_type(dl);
        if (ArrowSchemaInitFromType(schema, type) != NANOARROW_OK) {
            return -1;
        }
        
        if (ArrowSchemaSetName(schema, DYN_LIST_NAME(dl)) != NANOARROW_OK) {
            ArrowSchemaRelease(schema);
            return -1;
        }
        
        // Initialize array
        if (ArrowArrayInitFromType(array, type) != NANOARROW_OK) {
            return -1;
        }
        
        if (ArrowArrayStartAppending(array) != NANOARROW_OK) {
            ArrowArrayRelease(array);
            return -1;
        }
        
        // Append data directly
        int n = DYN_LIST_N(dl);
        switch (DYN_LIST_DATATYPE(dl)) {
            case DF_LONG: {
                int* vals = (int*)DYN_LIST_VALS(dl);
                for (int i = 0; i < n; i++) {
                    if (ArrowArrayAppendInt(array, vals[i]) != NANOARROW_OK) {
                        ArrowArrayRelease(array);
                        return -1;
                    }
                }
                break;
            }
            case DF_SHORT: {
                short* vals = (short*)DYN_LIST_VALS(dl);
                for (int i = 0; i < n; i++) {
                    if (ArrowArrayAppendInt(array, vals[i]) != NANOARROW_OK) {
                        ArrowArrayRelease(array);
                        return -1;
                    }
                }
                break;
            }
            case DF_CHAR: {
                char* vals = (char*)DYN_LIST_VALS(dl);
                for (int i = 0; i < n; i++) {
                    if (ArrowArrayAppendUInt(array, (uint64_t)(unsigned char)vals[i]) != NANOARROW_OK) {
                        ArrowArrayRelease(array);
                        return -1;
                    }
                }
                break;
            }
            case DF_FLOAT: {
                float* vals = (float*)DYN_LIST_VALS(dl);
                for (int i = 0; i < n; i++) {
                    if (ArrowArrayAppendDouble(array, (double)vals[i]) != NANOARROW_OK) {
                        ArrowArrayRelease(array);
                        return -1;
                    }
                }
                break;
            }
            case DF_STRING: {
                char** vals = (char**)DYN_LIST_VALS(dl);
                for (int i = 0; i < n; i++) {
                    if (!vals[i]) {
                        ERROR_PRINT("ERROR: NULL string at index %d\n", i);
                        ArrowArrayRelease(array);
                        return -1;
                    }
                    struct ArrowStringView sv = { .data = vals[i], .size_bytes = strlen(vals[i]) };
                    if (ArrowArrayAppendString(array, sv) != NANOARROW_OK) {
                        ArrowArrayRelease(array);
                        return -1;
                    }
                }
                break;
            }
        }
        
        // Finish building
        struct ArrowError error;
        if (ArrowArrayFinishBuildingDefault(array, &error) != NANOARROW_OK) {
            ERROR_PRINT("ERROR: Failed to finish array: %s\n", error.message);
            ArrowArrayRelease(array);
            return -1;
        }
        
        return 0;
    }
    
    // For nested lists, use the recursive approach
    // Step 1: Build schema recursively
    if (build_schema_from_dynlist(dl, schema) != 0) {
        ERROR_PRINT("ERROR: Failed to build schema\n");
        return -1;
    }
    
    // Step 2: Initialize array structure based on schema
    if (init_array_from_schema(schema, array) != 0) {
        ERROR_PRINT("ERROR: Failed to initialize array\n");
        return -1;
    }
    
    // Step 3: Append data recursively
    if (append_dynlist_data(dl, array) != 0) {
        ERROR_PRINT("ERROR: Failed to append data\n");
        ArrowArrayRelease(array);
        return -1;
    }
    
    // Step 4: Finish building arrays bottom-up
    if (finish_array_recursive(array, schema) != 0) {
        ERROR_PRINT("ERROR: Failed to finish arrays\n");
        ArrowArrayRelease(array);
        return -1;
    }
    
    DEBUG_PRINT("DEBUG: Successfully converted DYN_LIST '%s'\n", DYN_LIST_NAME(dl));
    return 0;
}

// Validation function to check DYN_GROUP structure before conversion
static int validate_dyn_group_for_arrow(DYN_GROUP* dg) {
    if (!dg || DYN_GROUP_NLISTS(dg) == 0) {
        DEBUG_PRINT("DEBUG: Invalid DYN_GROUP: null or empty\n");
        return -1;
    }
    
    int expected_length = -1;
    
    // Check each top-level list
    for (int i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
        DYN_LIST* dl = DYN_GROUP_LIST(dg, i);
        if (!dl) {
            DEBUG_PRINT("DEBUG: Column %d is null\n", i);
            return -1;
        }
        
        // Check length consistency (rectangular requirement)
        if (expected_length == -1) {
            expected_length = DYN_LIST_N(dl);
        } else if (DYN_LIST_N(dl) != expected_length) {
            DEBUG_PRINT("DEBUG: Non-rectangular data: column %d has %d elements, expected %d\n",
                        i, DYN_LIST_N(dl), expected_length);
            return -1;
        }
        
        // For nested lists, validate type consistency within each sublist
        if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
            enum ArrowType expected_child_type = get_ultimate_child_type(dl);
            if (expected_child_type == NANOARROW_TYPE_NA) {
                DEBUG_PRINT("DEBUG: Could not determine child type for nested column %d\n", i);
                return -1;
            }
            
            // Check that all sublists exist and have consistent ultimate types
            DYN_LIST** sublist_vals = (DYN_LIST**)DYN_LIST_VALS(dl);
            for (int j = 0; j < DYN_LIST_N(dl); j++) {
                if (!sublist_vals[j]) {
                    ERROR_PRINT("ERROR: NULL sublist at column %d, index %d - this should never happen\n", i, j);
                    return -1;
                }
                enum ArrowType sublist_type = get_ultimate_child_type(sublist_vals[j]);
                if (sublist_type != expected_child_type) {
                    ERROR_PRINT("ERROR: Type inconsistency in column %d, sublist %d: expected %d, got %d\n",
                                i, j, expected_child_type, sublist_type);
                    return -1;
                }
            }
        }
    }
    
    DEBUG_PRINT("DEBUG: DYN_GROUP validation passed: %d columns, %d rows\n", 
                DYN_GROUP_NLISTS(dg), expected_length);
    return 0;
}

int dg_to_arrow_buffer(DYN_GROUP* dg, uint8_t** data, size_t* size) {
    if (!dg || !data || !size) {
        return -1;
    }
    
    // Validate structure before attempting conversion
    if (validate_dyn_group_for_arrow(dg) != 0) {
        return -1;
    }
    
    int expected_length = DYN_LIST_N(DYN_GROUP_LIST(dg, 0)); // We know this exists from validation

    struct ArrowError error;
    struct ArrowBuffer buffer;
    ArrowBufferInit(&buffer);
    
    // Create schema for struct (record batch)
    struct ArrowSchema schema = {0};
    if (ArrowSchemaInitFromType(&schema, NANOARROW_TYPE_STRUCT) != NANOARROW_OK) {
        return -1;
    }
    
    if (ArrowSchemaAllocateChildren(&schema, DYN_GROUP_NLISTS(dg)) != NANOARROW_OK) {
        ArrowSchemaRelease(&schema);
        return -1;
    }
    
    // Create array for struct
    struct ArrowArray array = {0};
    if (ArrowArrayInitFromType(&array, NANOARROW_TYPE_STRUCT) != NANOARROW_OK) {
        ArrowSchemaRelease(&schema);
        return -1;
    }
    
    if (ArrowArrayAllocateChildren(&array, DYN_GROUP_NLISTS(dg)) != NANOARROW_OK) {
        ArrowSchemaRelease(&schema);
        ArrowArrayRelease(&array);
        return -1;
    }
    
    // Convert each DYN_LIST to its corresponding column
    for (int i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
        DYN_LIST* dl = DYN_GROUP_LIST(dg, i);
        if (!dl) continue;
        
        DEBUG_PRINT("DEBUG: About to convert column %d ('%s'), type=%d, length=%d\n", 
                    i, DYN_LIST_NAME(dl), DYN_LIST_DATATYPE(dl), DYN_LIST_N(dl));
        
        if (dynlist_to_nanoarrow_array(dl, array.children[i], schema.children[i]) != 0) {
            DEBUG_PRINT("DEBUG: Failed to convert column %d ('%s')\n", i, DYN_LIST_NAME(dl));
            ArrowArrayRelease(&array);
            ArrowSchemaRelease(&schema);
            return -1;
        }
        
        DEBUG_PRINT("DEBUG: Column %d ('%s') converted successfully, length=%lld\n", 
                    i, DYN_LIST_NAME(dl), array.children[i]->length);
        
        // Verify length consistency
        if (array.children[i]->length != expected_length) {
            ERROR_PRINT("Length mismatch: expected %d, got %lld for column %d\n", 
                        expected_length, array.children[i]->length, i);
            ArrowArrayRelease(&array);
            ArrowSchemaRelease(&schema);
            return -1;
        }
    }
    
    // Set struct array properties
    array.length = expected_length;
    array.null_count = 0;
    
    // IMPORTANT: Allocate validity bitmap for struct array
    // Struct arrays need a validity bitmap even if there are no nulls
    array.n_buffers = 1;
    array.buffers = (const void**)malloc(sizeof(void*));
    if (!array.buffers) {
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        return -1;
    }
    
    // Create validity bitmap (all valid - no nulls)
    size_t validity_bytes = (expected_length + 7) / 8;
    uint8_t* validity = (uint8_t*)malloc(validity_bytes);
    if (!validity) {
        free((void*)array.buffers);
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        return -1;
    }
    memset(validity, 0xFF, validity_bytes); // All bits set = all valid
    array.buffers[0] = validity;
    
    DEBUG_PRINT("DEBUG: Struct array setup complete, length=%lld, null_count=%lld\n", 
                array.length, array.null_count);
    
    // Set up IPC writer with buffer
    struct ArrowIpcOutputStream stream;
    if (ArrowIpcOutputStreamInitBuffer(&stream, &buffer) != NANOARROW_OK) {
        DEBUG_PRINT("DEBUG: Failed to init IPC output stream\n");
        free(validity);
        free((void*)array.buffers);
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        ArrowBufferReset(&buffer);
        return -1;
    }
    
    struct ArrowIpcWriter writer;
    if (ArrowIpcWriterInit(&writer, &stream) != NANOARROW_OK) {
        DEBUG_PRINT("DEBUG: Failed to init IPC writer\n");
        free(validity);
        free((void*)array.buffers);
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        ArrowBufferReset(&buffer);
        return -1;
    }
    
    // Write schema
    DEBUG_PRINT("DEBUG: Writing schema...\n");
    if (ArrowIpcWriterWriteSchema(&writer, &schema, &error) != NANOARROW_OK) {
        ERROR_PRINT("Error writing schema: %s\n", error.message);
        ArrowIpcWriterReset(&writer);
        free(validity);
        free((void*)array.buffers);
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        ArrowBufferReset(&buffer);
        return -1;
    }
    DEBUG_PRINT("DEBUG: Schema written successfully\n");
    
    // Create ArrayView from Array for writing
    struct ArrowArrayView array_view;
    DEBUG_PRINT("DEBUG: Initializing array view...\n");
    if (ArrowArrayViewInitFromSchema(&array_view, &schema, &error) != NANOARROW_OK) {
        ERROR_PRINT("Error initializing array view: %s\n", error.message);
        ArrowIpcWriterReset(&writer);
        free(validity);
        free((void*)array.buffers);
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        ArrowBufferReset(&buffer);
        return -1;
    }
    
    DEBUG_PRINT("DEBUG: Setting array in array view...\n");
    if (ArrowArrayViewSetArray(&array_view, &array, &error) != NANOARROW_OK) {
        ERROR_PRINT("Error setting array view: %s\n", error.message);
        ArrowArrayViewReset(&array_view);
        ArrowIpcWriterReset(&writer);
        free(validity);
        free((void*)array.buffers);
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        ArrowBufferReset(&buffer);
        return -1;
    }
    DEBUG_PRINT("DEBUG: Array view ready\n");
    
    // Write the record batch
    DEBUG_PRINT("DEBUG: Writing record batch...\n");
    if (ArrowIpcWriterWriteArrayView(&writer, &array_view, &error) != NANOARROW_OK) {
        ERROR_PRINT("Error writing array view: %s\n", error.message);
        ArrowArrayViewReset(&array_view);
        ArrowIpcWriterReset(&writer);
        free(validity);
        free((void*)array.buffers);
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        ArrowBufferReset(&buffer);
        return -1;
    }
    DEBUG_PRINT("DEBUG: Record batch written successfully\n");
    
    // Transfer buffer ownership to caller
    *size = buffer.size_bytes;
    *data = (uint8_t*)malloc(*size);
    if (!*data) {
        ArrowArrayViewReset(&array_view);
        ArrowIpcWriterReset(&writer);
        free(validity);
        free((void*)array.buffers);
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        ArrowBufferReset(&buffer);
        return -1;
    }
    
    memcpy(*data, buffer.data, *size);
    DEBUG_PRINT("DEBUG: Buffer copied, size=%zu bytes\n", *size);
    
    // Clean up
    ArrowArrayViewReset(&array_view);
    ArrowIpcWriterReset(&writer);
    free(validity);
    free((void*)array.buffers);
    ArrowArrayRelease(&array);
    ArrowSchemaRelease(&schema);
    ArrowBufferReset(&buffer);
    
    DEBUG_PRINT("DEBUG: Cleanup complete, returning success\n");
    return 0;
}

// Convenience function to write to file
int dg_to_arrow_file(DYN_GROUP* dg, const char* filename) {
    uint8_t* buffer = NULL;
    size_t buffer_size = 0;
    
    if (dg_to_arrow_buffer(dg, &buffer, &buffer_size) != 0) {
        return -1;
    }
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        free(buffer);
        return -1;
    }
    
    size_t written = fwrite(buffer, 1, buffer_size, fp);
    fclose(fp);
    free(buffer);
    
    return (written == buffer_size) ? 0 : -1;
}

/*************************************************************************************/
/******************************** DESERIALIZATION ************************************/
/*************************************************************************************/

// Helper to convert Arrow format string to DF type
static int arrow_type_to_df_type(const char* format) {
    if (!format) return -1;
    
    if (strcmp(format, "i") == 0) return DF_LONG;        // int32
    else if (strcmp(format, "s") == 0) return DF_SHORT;  // int16
    else if (strcmp(format, "C") == 0) return DF_CHAR;   // uint8
    else if (strcmp(format, "f") == 0) return DF_FLOAT;  // float
    else if (strcmp(format, "g") == 0) return DF_FLOAT;  // double -> float
    else if (strcmp(format, "u") == 0) return DF_STRING; // string
    else if (strcmp(format, "+l") == 0) return DF_LIST;  // list
    else return -1; // Unsupported type
}

// Recursively convert Arrow array view to DYN_LIST
static DYN_LIST* nanoarrow_array_to_dynlist(const struct ArrowArrayView* array_view, 
                                             const struct ArrowSchema* schema, 
                                             const char* name) {
    if (!array_view || !schema) return NULL;
    
    int64_t n = array_view->length;
    
    DEBUG_PRINT("DEBUG: Converting array: name='%s', format='%s', length=%lld, offset=%lld\n", 
                name ? name : "unnamed", schema->format, n, array_view->offset);
    
    // Handle LIST type recursively  
    if (schema->format && strcmp(schema->format, "+l") == 0) {
        char* list_name = strdup(name ? name : "list");
        DYN_LIST* dl = dfuCreateNamedDynList(list_name, DF_LIST, (int)n);
        free(list_name);
        if (!dl) {
            ERROR_PRINT("ERROR: Failed to create list DYN_LIST\n");
            return NULL;
        }
        
        // Check we have the expected structure
        if (!array_view->children || array_view->n_children != 1) {
            ERROR_PRINT("ERROR: List array view missing child\n");
            dfuFreeDynList(dl);
            return NULL;
        }
        
        const struct ArrowArrayView* child_view = array_view->children[0];
        const struct ArrowSchema* child_schema = schema->children[0];
        
        // Get the offset buffer - accounting for the array view's own offset
        const int32_t* offsets = (const int32_t*)array_view->buffer_views[1].data.as_int32;
        if (!offsets) {
            ERROR_PRINT("ERROR: List offsets buffer is NULL\n");
            dfuFreeDynList(dl);
            return NULL;
        }
        
        DEBUG_PRINT("DEBUG: Processing list with %lld elements, child has %lld total elements\n",
                    n, child_view->length);
        
        // Process each list element
        for (int64_t i = 0; i < n; i++) {
            // Check for null
            if (ArrowArrayViewIsNull(array_view, i)) {
                DEBUG_PRINT("DEBUG: List element %lld is null (shouldn't happen in our case)\n", i);
                // For our use case, we shouldn't have null lists, but handle it anyway
                // Create an empty list
                DYN_LIST* empty_list = dfuCreateNamedDynList("empty", 
                                                              arrow_type_to_df_type(child_schema->format), 
                                                              0);
                dfuMoveDynListList(dl, empty_list);
                continue;
            }
            
            // Get the range for this list element
            // Note: offsets are relative to the parent's offset
            int64_t actual_index = array_view->offset + i;
            int32_t start_offset = offsets[actual_index];
            int32_t end_offset = offsets[actual_index + 1];
            int32_t list_length = end_offset - start_offset;
            
            DEBUG_PRINT("DEBUG: List element %lld: start=%d, end=%d, length=%d\n",
                        i, start_offset, end_offset, list_length);
            
            // Create a new sliced view for this sublist
            struct ArrowArrayView sliced_view = *child_view;  // Start with a copy
            sliced_view.offset = start_offset;
            sliced_view.length = list_length;
            
            // Convert the sliced view to a DYN_LIST
            char sublist_name[64];
            snprintf(sublist_name, sizeof(sublist_name), "sublist_%lld", i);
            DYN_LIST* nested_list = nanoarrow_array_to_dynlist(&sliced_view, 
                                                                child_schema, 
                                                                sublist_name);
            if (!nested_list) {
                ERROR_PRINT("ERROR: Failed to convert sublist %lld\n", i);
                dfuFreeDynList(dl);
                return NULL;
            }
            
            DEBUG_PRINT("DEBUG: Created sublist with %d elements\n", DYN_LIST_N(nested_list));
            
            // Move the nested list into our list of lists
            dfuMoveDynListList(dl, nested_list);
        }
        
        DEBUG_PRINT("DEBUG: Successfully created list with %d sublists\n", DYN_LIST_N(dl));
        return dl;
    }
    
    // Handle primitive types 
    int df_type = arrow_type_to_df_type(schema->format);
    if (df_type == -1) {
        DEBUG_PRINT("DEBUG: Unknown format '%s'\n", schema->format);
        return NULL;
    }
    
    char* col_name = strdup(name ? name : "column");
    DYN_LIST* dl = dfuCreateNamedDynList(col_name, df_type, (int)n);
    free(col_name);
    if (!dl) return NULL;
    
    // Account for offset when reading primitive arrays
    int64_t offset = array_view->offset;
    
    switch (df_type) {
        case DF_LONG: {
            const int32_t* src = array_view->buffer_views[1].data.as_int32;
            if (!src) {
                DEBUG_PRINT("DEBUG: DF_LONG buffer is NULL\n");
                dfuFreeDynList(dl);
                return NULL;
            }
            for (int64_t i = 0; i < n; i++) {
                int val = ArrowArrayViewIsNull(array_view, i) ? 0 : (int)src[offset + i];
                dfuAddDynListLong(dl, val);
            }
            break;
        }
        
        case DF_SHORT: {
            const int16_t* src = array_view->buffer_views[1].data.as_int16;
            if (!src) {
                DEBUG_PRINT("DEBUG: DF_SHORT buffer is NULL\n");
                dfuFreeDynList(dl);
                return NULL;
            }
            for (int64_t i = 0; i < n; i++) {
                short val = ArrowArrayViewIsNull(array_view, i) ? 0 : (short)src[offset + i];
                dfuAddDynListShort(dl, val);
            }
            break;
        }
        
        case DF_CHAR: {
            const uint8_t* src = array_view->buffer_views[1].data.as_uint8;
            if (!src) {
                DEBUG_PRINT("DEBUG: DF_CHAR buffer is NULL\n");
                dfuFreeDynList(dl);
                return NULL;
            }
            for (int64_t i = 0; i < n; i++) {
                char val = ArrowArrayViewIsNull(array_view, i) ? 0 : (char)src[offset + i];
                dfuAddDynListChar(dl, val);
            }
            break;
        }
        
        case DF_FLOAT: {
            if (strcmp(schema->format, "f") == 0) {
                const float* src = array_view->buffer_views[1].data.as_float;
                if (!src) {
                    dfuFreeDynList(dl);
                    return NULL;
                }
                for (int64_t i = 0; i < n; i++) {
                    float val = ArrowArrayViewIsNull(array_view, i) ? 0.0f : src[offset + i];
                    dfuAddDynListFloat(dl, val);
                }
            } else {
                const double* src = array_view->buffer_views[1].data.as_double;
                if (!src) {
                    dfuFreeDynList(dl);
                    return NULL;
                }
                for (int64_t i = 0; i < n; i++) {
                    float val = ArrowArrayViewIsNull(array_view, i) ? 0.0f : (float)src[offset + i];
                    dfuAddDynListFloat(dl, val);
                }
            }
            break;
        }
        
        case DF_STRING: {
            const int32_t* offsets = array_view->buffer_views[1].data.as_int32;
            const char* str_data = (const char*)array_view->buffer_views[2].data.as_char;
            
            if (!offsets || !str_data) {
                DEBUG_PRINT("DEBUG: DF_STRING buffers are NULL\n");
                dfuFreeDynList(dl);
                return NULL;
            }
            
            for (int64_t i = 0; i < n; i++) {
                if (ArrowArrayViewIsNull(array_view, i)) {
                    dfuAddDynListString(dl, ""); // Add empty string for nulls
                } else {
                    int64_t actual_index = offset + i;
                    int32_t start = offsets[actual_index];
                    int32_t end = offsets[actual_index + 1];
                    int32_t len = end - start;
                    
                    char* val = (char*)malloc(len + 1);
                    if (val) {
                        memcpy(val, str_data + start, len);
                        val[len] = '\0';
                        dfuAddDynListString(dl, val);
                        free(val); // dfuAddDynListString makes its own copy
                    }
                }
            }
            break;
        }
    }
    
    DEBUG_PRINT("DEBUG: Created DYN_LIST '%s' with %d elements\n", DYN_LIST_NAME(dl), DYN_LIST_N(dl));
    return dl;
}

// Main deserialization function
DYN_GROUP* arrow_buffer_to_dg(const uint8_t* data, size_t size, const char* group_name) {
    if (!data || size == 0) return NULL;
    
    struct ArrowError error;
    
    // Create input stream from buffer
    struct ArrowBuffer input_buffer;
    ArrowBufferInit(&input_buffer);
    
    if (ArrowBufferReserve(&input_buffer, size) != NANOARROW_OK) {
        return NULL;
    }
    memcpy(input_buffer.data, data, size);
    input_buffer.size_bytes = size;
    
    struct ArrowIpcInputStream input_stream;
    if (ArrowIpcInputStreamInitBuffer(&input_stream, &input_buffer) != NANOARROW_OK) {
        ArrowBufferReset(&input_buffer);
        return NULL;
    }
    
    // Create array stream reader
    struct ArrowArrayStream stream;
    if (ArrowIpcArrayStreamReaderInit(&stream, &input_stream, NULL) != NANOARROW_OK) {
        ArrowBufferReset(&input_buffer);
        return NULL;
    }
    
    // Get schema
    struct ArrowSchema schema;
    if (ArrowArrayStreamGetSchema(&stream, &schema, &error) != NANOARROW_OK) {
        ERROR_PRINT("Error getting schema: %s\n", error.message);
        ArrowArrayStreamRelease(&stream);
        ArrowBufferReset(&input_buffer);
        return NULL;
    }
    
    // Read first record batch
    struct ArrowArray array;
    if (ArrowArrayStreamGetNext(&stream, &array, &error) != NANOARROW_OK) {
        ERROR_PRINT("Error reading array: %s\n", error.message);
        ArrowSchemaRelease(&schema);
        ArrowArrayStreamRelease(&stream);
        ArrowBufferReset(&input_buffer);
        return NULL;
    }
    
    DEBUG_PRINT("DEBUG: Read record batch: length=%lld, n_children=%lld\n", 
                array.length, array.n_children);
    
    // Create array view
    struct ArrowArrayView array_view;
    if (ArrowArrayViewInitFromSchema(&array_view, &schema, &error) != NANOARROW_OK) {
        ERROR_PRINT("Error creating array view: %s\n", error.message);
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        ArrowArrayStreamRelease(&stream);
        ArrowBufferReset(&input_buffer);
        return NULL;
    }
    
    if (ArrowArrayViewSetArray(&array_view, &array, &error) != NANOARROW_OK) {
        ERROR_PRINT("Error setting array view: %s\n", error.message);
        ArrowArrayViewReset(&array_view);
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        ArrowArrayStreamRelease(&stream);
        ArrowBufferReset(&input_buffer);
        return NULL;
    }
    
    // Create DYN_GROUP - fix const warning
    char* mutable_group_name = strdup(group_name ? group_name : "deserialized");
    DYN_GROUP* dg = dfuCreateNamedDynGroup(mutable_group_name, (int)schema.n_children);
    free(mutable_group_name);
    
    if (!dg) {
        ArrowArrayViewReset(&array_view);
        ArrowArrayRelease(&array);
        ArrowSchemaRelease(&schema);
        ArrowArrayStreamRelease(&stream);
        ArrowBufferReset(&input_buffer);
        return NULL;
    }
    
    // Convert each column
    for (int64_t i = 0; i < schema.n_children; i++) {
        const char* column_name = schema.children[i]->name;
        if (!column_name) column_name = "unnamed";
        
        DEBUG_PRINT("DEBUG: Processing column %lld: name='%s', format='%s'\n", 
                    i, column_name, schema.children[i]->format);
        
        DYN_LIST* dl = nanoarrow_array_to_dynlist(array_view.children[i], 
                                                  schema.children[i], 
                                                  column_name);
        if (!dl) {
            DEBUG_PRINT("DEBUG: Failed to convert column %lld\n", i);
            dfuFreeDynGroup(dg);
            ArrowArrayViewReset(&array_view);
            ArrowArrayRelease(&array);
            ArrowSchemaRelease(&schema);
            ArrowArrayStreamRelease(&stream);
            ArrowBufferReset(&input_buffer);
            return NULL;
        }
        
        // Add to group using existing list
        char* mutable_name = strdup(column_name);
        int list_index = dfuAddDynGroupExistingList(dg, mutable_name, dl);
        free(mutable_name);
        
        if (list_index < 0) {
            DEBUG_PRINT("DEBUG: Failed to add list to group for column %lld\n", i);
            dfuFreeDynList(dl);
            dfuFreeDynGroup(dg);
            ArrowArrayViewReset(&array_view);
            ArrowArrayRelease(&array);
            ArrowSchemaRelease(&schema);
            ArrowArrayStreamRelease(&stream);
            ArrowBufferReset(&input_buffer);
            return NULL;
        }
        
        DEBUG_PRINT("DEBUG: Added list '%s' to group at index %d\n", column_name, list_index);
    }
    
    DEBUG_PRINT("DEBUG: Created DYN_GROUP '%s' with %d lists\n", 
                DYN_GROUP_NAME(dg), DYN_GROUP_NLISTS(dg));
    
    // Clean up
    ArrowArrayViewReset(&array_view);
    ArrowArrayRelease(&array);
    ArrowSchemaRelease(&schema);
    ArrowArrayStreamRelease(&stream);
    ArrowBufferReset(&input_buffer);
    
    return dg;
}

// Convenience function to read from file
DYN_GROUP* arrow_file_to_dg(const char* filename, const char* group_name) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    // Read into buffer
    uint8_t* buffer = (uint8_t*)malloc(size);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    
    size_t read_size = fread(buffer, 1, size, fp);
    fclose(fp);
    
    if (read_size != size) {
        free(buffer);
        return NULL;
    }
    
    // Deserialize
    DYN_GROUP* dg = arrow_buffer_to_dg(buffer, size, group_name);
    free(buffer);
    
    return dg;
}