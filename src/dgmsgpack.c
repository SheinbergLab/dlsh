/*
 * dgmsgpack.c
 * MessagePack I/O support for dynamic groups - mirrors dgjson.c structure
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <df.h>
#include <dynio.h>

#include <msgpack.h>

// Pack DYN_LIST to MessagePack (equivalent to dl_to_json)
int dl_to_msgpack(DYN_LIST *dl, msgpack_packer *pk)
{
    int i;
    
    if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
        // Pack array header
        if (msgpack_pack_array(pk, DYN_LIST_N(dl)) != 0) return -1;
        
        DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
        DYN_LIST *curlist;

        for (i = 0; i < DYN_LIST_N(dl); i++) {
            curlist = (DYN_LIST *) vals[i];
            if (!curlist) {
                // Pack null for missing sublists
                if (msgpack_pack_nil(pk) != 0) return -1;
            } else {
                if (dl_to_msgpack(curlist, pk) != 0) return -1;
            }
        }
        return 0;
    }

    // Pack array header for primitive types
    if (msgpack_pack_array(pk, DYN_LIST_N(dl)) != 0) return -1;

    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG:
        {
            int *vals = (int *) DYN_LIST_VALS(dl);
            for (i = 0; i < DYN_LIST_N(dl); i++) {
                if (msgpack_pack_int32(pk, vals[i]) != 0) return -1;
            }
        }
        break;
    case DF_SHORT:
        {
            short *vals = (short *) DYN_LIST_VALS(dl);
            for (i = 0; i < DYN_LIST_N(dl); i++) {
                if (msgpack_pack_int16(pk, vals[i]) != 0) return -1;
            }
        }
        break;      
    case DF_CHAR:
        {
            char *vals = (char *) DYN_LIST_VALS(dl);
            for (i = 0; i < DYN_LIST_N(dl); i++) {
                if (msgpack_pack_int8(pk, vals[i]) != 0) return -1;
            }
        }
        break;      
    case DF_FLOAT:
        {
            float *vals = (float *) DYN_LIST_VALS(dl);
            for (i = 0; i < DYN_LIST_N(dl); i++) {
                if (msgpack_pack_float(pk, vals[i]) != 0) return -1;
            }
        }
        break;      
    case DF_STRING:
        {
            char **vals = (char **) DYN_LIST_VALS(dl);
            for (i = 0; i < DYN_LIST_N(dl); i++) {
                if (vals[i]) {
                    size_t len = strlen(vals[i]);
                    if (msgpack_pack_str(pk, len) != 0) return -1;
                    if (msgpack_pack_str_body(pk, vals[i], len) != 0) return -1;
                } else {
                    if (msgpack_pack_nil(pk) != 0) return -1;
                }
            }
            break;
        }
    }
    return 0;
}

// Pack entire DYN_GROUP to MessagePack (equivalent to dg_to_json)
int dg_to_msgpack_buffer(DYN_GROUP *dg, char **buffer, size_t *buffer_size)
{
    int i;
    msgpack_sbuffer sbuf;
    msgpack_packer pk;
    
    if (!dg || !buffer || !buffer_size) return -1;
    
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    
    // Pack as map with column_name -> array pairs
    if (msgpack_pack_map(&pk, DYN_GROUP_NLISTS(dg)) != 0) {
        msgpack_sbuffer_destroy(&sbuf);
        return -1;
    }
    
    for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
        DYN_LIST *dl = DYN_GROUP_LIST(dg, i);
        const char *name = DYN_LIST_NAME(dl);
        
        // Pack column name as key
        size_t name_len = strlen(name);
        if (msgpack_pack_str(&pk, name_len) != 0 ||
            msgpack_pack_str_body(&pk, name, name_len) != 0) {
            msgpack_sbuffer_destroy(&sbuf);
            return -1;
        }
        
        // Pack column data as value
        if (dl_to_msgpack(dl, &pk) != 0) {
            msgpack_sbuffer_destroy(&sbuf);
            return -1;
        }
    }
    
    // Transfer ownership of buffer to caller
    *buffer_size = sbuf.size;
    *buffer = malloc(sbuf.size);
    if (!*buffer) {
        msgpack_sbuffer_destroy(&sbuf);
        return -1;
    }
    
    memcpy(*buffer, sbuf.data, sbuf.size);
    msgpack_sbuffer_destroy(&sbuf);
    
    return 0;
}

// Hybrid format (equivalent to dg_to_hybrid_json)
int dg_to_hybrid_msgpack_buffer(DYN_GROUP *dg, char **buffer, size_t *buffer_size)
{
    int i, j, max_rows = 0;
    msgpack_sbuffer sbuf;
    msgpack_packer pk;
    
    if (!dg || DYN_GROUP_NLISTS(dg) == 0 || !buffer || !buffer_size) return -1;
    
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    
    // Find max rows across all lists
    for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
        DYN_LIST *dl = DYN_GROUP_LIST(dg, i);
        if (dl && DYN_LIST_N(dl) > max_rows) {
            max_rows = DYN_LIST_N(dl);
        }
    }
    
    // Pack root object with 3 fields: name, rows, arrays
    if (msgpack_pack_map(&pk, 3) != 0) goto error;
    
    // Pack name
    if (msgpack_pack_str(&pk, 4) != 0 || 
        msgpack_pack_str_body(&pk, "name", 4) != 0) goto error;
    
    const char *group_name = DYN_GROUP_NAME(dg);
    size_t name_len = strlen(group_name);
    if (msgpack_pack_str(&pk, name_len) != 0 ||
        msgpack_pack_str_body(&pk, group_name, name_len) != 0) goto error;
    
    // Pack rows array
    if (msgpack_pack_str(&pk, 4) != 0 || 
        msgpack_pack_str_body(&pk, "rows", 4) != 0) goto error;
    
    if (msgpack_pack_array(&pk, max_rows) != 0) goto error;
    
    for (i = 0; i < max_rows; i++) {
        // Count non-list columns for this row's map size
        int map_size = 0;
        for (j = 0; j < DYN_GROUP_NLISTS(dg); j++) {
            DYN_LIST *dl = DYN_GROUP_LIST(dg, j);
            if (dl && DYN_LIST_DATATYPE(dl) != DF_LIST && i < DYN_LIST_N(dl)) {
                map_size++;
            }
        }
        
        if (msgpack_pack_map(&pk, map_size) != 0) goto error;
        
        // Pack primitive values for this row
        for (j = 0; j < DYN_GROUP_NLISTS(dg); j++) {
            DYN_LIST *dl = DYN_GROUP_LIST(dg, j);
            if (!dl || DYN_LIST_DATATYPE(dl) == DF_LIST || i >= DYN_LIST_N(dl)) continue;
            
            const char *col_name = DYN_LIST_NAME(dl);
            size_t col_name_len = strlen(col_name);
            
            // Pack field name
            if (msgpack_pack_str(&pk, col_name_len) != 0 ||
                msgpack_pack_str_body(&pk, col_name, col_name_len) != 0) goto error;
            
            // Pack field value based on type
            switch (DYN_LIST_DATATYPE(dl)) {
            case DF_LONG:
                {
                    int *vals = (int *) DYN_LIST_VALS(dl);
                    if (msgpack_pack_int32(&pk, vals[i]) != 0) goto error;
                }
                break;
            case DF_SHORT:
                {
                    short *vals = (short *) DYN_LIST_VALS(dl);
                    if (msgpack_pack_int16(&pk, vals[i]) != 0) goto error;
                }
                break;
            case DF_CHAR:
                {
                    char *vals = (char *) DYN_LIST_VALS(dl);
                    if (msgpack_pack_int8(&pk, vals[i]) != 0) goto error;
                }
                break;
            case DF_FLOAT:
                {
                    float *vals = (float *) DYN_LIST_VALS(dl);
                    if (msgpack_pack_float(&pk, vals[i]) != 0) goto error;
                }
                break;
            case DF_STRING:
                {
                    char **vals = (char **) DYN_LIST_VALS(dl);
                    if (vals[i]) {
                        size_t str_len = strlen(vals[i]);
                        if (msgpack_pack_str(&pk, str_len) != 0 ||
                            msgpack_pack_str_body(&pk, vals[i], str_len) != 0) goto error;
                    } else {
                        if (msgpack_pack_nil(&pk) != 0) goto error;
                    }
                }
                break;
            }
        }
    }
    
    // Pack arrays lookup table
    if (msgpack_pack_str(&pk, 6) != 0 || 
        msgpack_pack_str_body(&pk, "arrays", 6) != 0) goto error;
    
    // Count list columns
    int list_count = 0;
    for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
        DYN_LIST *dl = DYN_GROUP_LIST(dg, i);
        if (dl && DYN_LIST_DATATYPE(dl) == DF_LIST) {
            list_count++;
        }
    }
    
    if (msgpack_pack_map(&pk, list_count) != 0) goto error;
    
    for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
        DYN_LIST *dl = DYN_GROUP_LIST(dg, i);
        if (dl && DYN_LIST_DATATYPE(dl) == DF_LIST) {
            const char *col_name = DYN_LIST_NAME(dl);
            size_t col_name_len = strlen(col_name);
            
            // Pack array name
            if (msgpack_pack_str(&pk, col_name_len) != 0 ||
                msgpack_pack_str_body(&pk, col_name, col_name_len) != 0) goto error;
            
            // Pack array data
            if (dl_to_msgpack(dl, &pk) != 0) goto error;
        }
    }
    
    // Transfer ownership of buffer to caller
    *buffer_size = sbuf.size;
    *buffer = malloc(sbuf.size);
    if (!*buffer) goto error;
    
    memcpy(*buffer, sbuf.data, sbuf.size);
    msgpack_sbuffer_destroy(&sbuf);
    
    return 0;

error:
    msgpack_sbuffer_destroy(&sbuf);
    return -1;
}

// Convenience functions for file I/O (matching your JSON API)
int dg_write_msgpack_file(DYN_GROUP *dg, const char *filename)
{
    char *buffer = NULL;
    size_t buffer_size = 0;
    
    if (dg_to_msgpack_buffer(dg, &buffer, &buffer_size) != 0) {
        return -1;
    }
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        free(buffer);
        return -1;
    }
    
    size_t written = fwrite(buffer, 1, buffer_size, fp);
    fclose(fp);
    free(buffer);
    
    return (written == buffer_size) ? 0 : -1;
}

int dg_write_hybrid_msgpack_file(DYN_GROUP *dg, const char *filename)
{
    char *buffer = NULL;
    size_t buffer_size = 0;
    
    if (dg_to_hybrid_msgpack_buffer(dg, &buffer, &buffer_size) != 0) {
        return -1;
    }
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        free(buffer);
        return -1;
    }
    
    size_t written = fwrite(buffer, 1, buffer_size, fp);
    fclose(fp);
    free(buffer);
    
    return (written == buffer_size) ? 0 : -1;
}

// Get MessagePack data as buffer (for network transmission)
int dg_get_msgpack_data(DYN_GROUP *dg, char **data, size_t *size)
{
    return dg_to_msgpack_buffer(dg, data, size);
}

int dg_get_hybrid_msgpack_data(DYN_GROUP *dg, char **data, size_t *size)
{
    return dg_to_hybrid_msgpack_buffer(dg, data, size);
}