/*************************************************************************
 *
 * NAME
 *      dfu_helpers.h
 *
 * PURPOSE
 *      Helper functions for simplified DYN_LIST and DYN_GROUP operations.
 *      Provides a higher-level API for common data manipulation tasks.
 *
 * DESCRIPTION
 *      This library wraps the lower-level df/dynio API to provide more
 *      convenient functions for:
 *      - Creating and populating lists from arrays
 *      - Extracting data from groups
 *      - Storing metadata as key-value pairs
 *      - Converting between double and float arrays
 *
 *************************************************************************/

#ifndef DFU_HELPERS_H
#define DFU_HELPERS_H

/*
 * Create a DYN_LIST from a double array
 * Automatically converts to float for storage
 */
static inline DYN_LIST* dfuCreateDoubleList(const double *values, int count, 
                                           const char *name) {
    DYN_LIST *list = dfuCreateDynList(DF_FLOAT, count);
    if (!list) return NULL;
    
    for (int i = 0; i < count; i++) {
        dfuAddDynListFloat(list, (float)values[i]);
    }
    
    if (name) {
        strncpy(DYN_LIST_NAME(list), name, DYN_LIST_NAME_SIZE-1);
    }
    
    return list;
}

/*
 * Create a DYN_LIST from a float array
 */
static inline DYN_LIST* dfuCreateFloatList(const float *values, int count,
                                          const char *name) {
    DYN_LIST *list = dfuCreateDynList(DF_FLOAT, count);
    if (!list) return NULL;
    
    for (int i = 0; i < count; i++) {
        dfuAddDynListFloat(list, values[i]);
    }
    
    if (name) {
        strncpy(DYN_LIST_NAME(list), name, DYN_LIST_NAME_SIZE-1);
    }
    
    return list;
}

/*
 * Extract float values from a DYN_LIST into a newly allocated double array
 * Caller must free the returned array
 */
static inline double* dfuGetDoublesFromList(DYN_LIST *list, int *count) {
    if (!list || DYN_LIST_DATATYPE(list) != DF_FLOAT) {
        if (count) *count = 0;
        return NULL;
    }
    
    int n = DYN_LIST_N(list);
    if (count) *count = n;
    
    double *values = malloc(n * sizeof(double));
    if (!values) return NULL;
    
    float *fvals = (float *)DYN_LIST_VALS(list);
    for (int i = 0; i < n; i++) {
        values[i] = (double)fvals[i];
    }
    
    return values;
}

/*
 * Extract float values from a DYN_LIST (no conversion)
 * Returns pointer to internal data - do not free!
 */
static inline float* dfuGetFloatsFromList(DYN_LIST *list, int *count) {
    if (!list || DYN_LIST_DATATYPE(list) != DF_FLOAT) {
        if (count) *count = 0;
        return NULL;
    }
    
    if (count) *count = DYN_LIST_N(list);
    return (float *)DYN_LIST_VALS(list);
}

/*
 * Get a named list from a DYN_GROUP
 * Returns NULL if not found
 */
static inline DYN_LIST* dfuGetNamedList(DYN_GROUP *group, const char *name) {
  if (!group || !name) return NULL;
  
  int n_lists = DYN_GROUP_N(group);
  for (int i = 0; i < n_lists; i++) {
    DYN_LIST *list = (DYN_LIST *)DYN_GROUP_LIST(group, i);
    if (list && strcmp(DYN_LIST_NAME(list), name) == 0) {
      return list;
    }
  }
  
  return NULL;
}

/*
 * Create a metadata list with key-value pairs stored as strings
 * Keys and values are stored sequentially: key1, val1, key2, val2, ...
 */
static inline DYN_LIST* dfuCreateMetadataList(const char *name) {
    DYN_LIST *list = dfuCreateDynList(DF_STRING, 10); // Initial capacity
    if (!list) return NULL;
    
    if (name) {
        strncpy(DYN_LIST_NAME(list), name, DYN_LIST_NAME_SIZE-1);
    }
    
    return list;
}

/*
 * Add a key-value pair to a metadata list
 */
static inline int dfuAddMetadata(DYN_LIST *metadata, const char *key, 
                                const char *value) {
    if (!metadata || !key || !value) return 0;
    if (DYN_LIST_DATATYPE(metadata) != DF_STRING) return 0;
    
    dfuAddDynListString(metadata, (char *)key);
    dfuAddDynListString(metadata, (char *)value);
    return 1;
}

/*
 * Add a key with a double value to metadata list (converted to string)
 */
static inline int dfuAddMetadataDouble(DYN_LIST *metadata, const char *key,
                                      double value) {
    if (!metadata || !key) return 0;
    
    char value_str[64];
    snprintf(value_str, sizeof(value_str), "%.6e", value);
    
    return dfuAddMetadata(metadata, key, value_str);
}

/*
 * Add a key with an integer value to metadata list (converted to string)
 */
static inline int dfuAddMetadataInt(DYN_LIST *metadata, const char *key,
                                   int value) {
    if (!metadata || !key) return 0;
    
    char value_str[64];
    snprintf(value_str, sizeof(value_str), "%d", value);
    
    return dfuAddMetadata(metadata, key, value_str);
}

/*
 * Get a value from a metadata list by key
 * Returns NULL if not found
 * Note: Returns pointer to internal string - do not free!
 */
static inline const char* dfuGetMetadataValue(DYN_LIST *metadata, const char *key, const char *default_value) {
    if (!metadata || !key) return default_value;
    if (DYN_LIST_DATATYPE(metadata) != DF_STRING) return default_value;
    
    int n = DYN_LIST_N(metadata);
    char **strings = (char **)DYN_LIST_VALS(metadata);
    
    // Search for key (stored at even indices)
    for (int i = 0; i < n - 1; i += 2) {
        if (strcmp(strings[i], key) == 0) {
            return strings[i + 1]; // Return value at next index
        }
    }
    
    return default_value;
}

/*
 * Get a double value from metadata
 * Returns 0.0 if not found or conversion fails
 */
static inline double dfuGetMetadataDouble(DYN_LIST *metadata, const char *key) {
  const char *str = dfuGetMetadataValue(metadata, key, NULL);
  if (!str) return 0.0;
  
  return atof(str);
}

/*
 * Get an integer value from metadata
 * Returns 0 if not found or conversion fails
 */
static inline int dfuGetMetadataInt(DYN_LIST *metadata, const char *key) {
  const char *str = dfuGetMetadataValue(metadata, key, NULL);
  if (!str) return 0;
    
  return atoi(str);
}

/*
 * Create a simple DYN_GROUP with estimated capacity
 */
static inline DYN_GROUP* dfuCreateGroup(int estimated_lists) {
    // Add some headroom
    return dfuCreateDynGroup(estimated_lists + 5);
}

/*
 * Add a double array as a named list to a group
 */
static inline int dfuAddDoubleListToGroup(DYN_GROUP *group, const char *name,
                                         const double *values, int count) {
    if (!group || !name || !values || count <= 0) return 0;
    
    DYN_LIST *list = dfuCreateDoubleList(values, count, name);
    if (!list) return 0;
    
    dfuAddDynGroupExistingList(group, (char *) name, list);
    return 1;
}

/*
 * Add a float array as a named list to a group
 */
static inline int dfuAddFloatListToGroup(DYN_GROUP *group, const char *name,
                                        const float *values, int count) {
    if (!group || !name || !values || count <= 0) return 0;
    
    DYN_LIST *list = dfuCreateFloatList(values, count, name);
    if (!list) return 0;
    
    dfuAddDynGroupExistingList(group, (char *) name, list);
    return 1;
}

/*
 * Extract grid dimensions from grid_info list
 * grid_info format: [x_min, x_max, y_min, y_max, width, height]
 */
static inline int dfuGetGridDimensions(DYN_LIST *grid_info, 
                                      double *x_min, double *x_max,
                                      double *y_min, double *y_max,
                                      int *width, int *height) {
    if (!grid_info) return 0;
    
    int count;
    float *vals = dfuGetFloatsFromList(grid_info, &count);
    if (!vals || count < 6) return 0;
    
    if (x_min) *x_min = vals[0];
    if (x_max) *x_max = vals[1];
    if (y_min) *y_min = vals[2];
    if (y_max) *y_max = vals[3];
    if (width) *width = (int)vals[4];
    if (height) *height = (int)vals[5];
    
    return 1;
}

/*
 * Print metadata list contents (for debugging)
 */
static inline void dfuPrintMetadata(DYN_LIST *metadata) {
    if (!metadata || DYN_LIST_DATATYPE(metadata) != DF_STRING) {
        printf("Invalid metadata list\n");
        return;
    }
    
    int n = DYN_LIST_N(metadata);
    char **strings = (char **)DYN_LIST_VALS(metadata);
    
    printf("Metadata (%d items):\n", n / 2);
    for (int i = 0; i < n - 1; i += 2) {
        printf("  %s: %s\n", strings[i], strings[i + 1]);
    }
}


static inline DYN_LIST *dfuGetGroupList(DYN_GROUP *g, char *name)
{
  int i;
  for (i = 0; i < DYN_GROUP_N(g); i++) {
    if (!strcmp(name, DYN_LIST_NAME(DYN_GROUP_LIST(g, i))))
      return DYN_GROUP_LIST(g, i);
  }
  return NULL;
}

#endif /* DFU_HELPERS_H */
