/*
 * dg_json.c
 *  JSON I/O support for dynamic groups
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <df.h>
#include <dynio.h>

#include <jansson.h>

json_t *dl_to_json(DYN_LIST *dl)
{
  int i;  
  json_t *array = json_array();

  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    json_t *subarray;
    
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    DYN_LIST *curlist;

    for (i = 0; i < DYN_LIST_N(dl); i++) {
      curlist = (DYN_LIST *) vals[i];
      if (!curlist) {
	json_decref(array);
	return NULL;
      }
      subarray = dl_to_json(curlist);
      if (!subarray) {
	json_decref(array);
	return NULL;
      }
      json_array_append_new(array, subarray);
    }
    return array;
  }

  else {
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG:
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  json_array_append_new(array, json_integer(vals[i]));
	}
      }
      break;
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  json_array_append_new(array, json_integer(vals[i]));
	}
      }
      break;      
    case DF_CHAR:
      {
	char *vals = (char *) DYN_LIST_VALS(dl);
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  json_array_append_new(array, json_integer(vals[i]));
	}
      }
      break;      
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  json_array_append_new(array, json_real(vals[i]));
	}
      }
      break;      
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	for (i = 0; i < DYN_LIST_N(dl); i++) {
	  json_array_append_new(array, json_string(vals[i]));
	}
	break;
      }
    }
  }
  return array;
}

json_t *dl_element_to_json(DYN_LIST *dl, int element_id)
{
  if (element_id >= DYN_LIST_N(dl)) return NULL;

  if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
    json_t *subarray = NULL;
    
    DYN_LIST **vals = (DYN_LIST **) DYN_LIST_VALS(dl);
    DYN_LIST *curlist;

    curlist = (DYN_LIST *) vals[element_id];
    if (curlist) {
      subarray = dl_to_json(curlist);
    }
    return subarray;
  }

  else {
    switch (DYN_LIST_DATATYPE(dl)) {
    case DF_LONG:
      {
	int *vals = (int *) DYN_LIST_VALS(dl);
	return json_integer(vals[element_id]);
      }
      break;
    case DF_SHORT:
      {
	short *vals = (short *) DYN_LIST_VALS(dl);
	return json_integer(vals[element_id]);
      }
      break;      
    case DF_CHAR:
      {
	char *vals = (char *) DYN_LIST_VALS(dl);
	return json_integer(vals[element_id]);
      }
      break;      
    case DF_FLOAT:
      {
	float *vals = (float *) DYN_LIST_VALS(dl);
	return json_real(vals[element_id]);
      }
      break;      
    case DF_STRING:
      {
	char **vals = (char **) DYN_LIST_VALS(dl);
	return json_string(vals[element_id]);
      }
      break;
    default:
      return NULL;
    }
  }
}


json_t *dg_to_json(DYN_GROUP *dg)
{
  int i;
  json_t *json_dg, *json_list;

  json_dg = json_object();
  
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    json_list = dl_to_json(DYN_GROUP_LIST(dg,i));
    if (!json_list) {
      json_decref(json_dg);
      return NULL;
    }
    json_object_set_new(json_dg, DYN_LIST_NAME(DYN_GROUP_LIST(dg, i)), json_list);    
  }  
  return json_dg;
}


json_t *dg_element_to_json(DYN_GROUP *dg, int row)
{
  int i;
  json_t *json_dg, *json_list;
  DYN_LIST *dl;
  
  json_dg = json_object();
  
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    dl = DYN_GROUP_LIST(dg, i);
    if (row < DYN_LIST_N(dl)) {
      json_t *obj = dl_element_to_json(dl, row);
      if (obj) {
	json_object_set_new(json_dg, DYN_LIST_NAME(DYN_GROUP_LIST(dg, i)), obj);
      }
    }
  }
  
  return json_dg;
}

json_t *dg_to_hybrid_json(DYN_GROUP *dg)
{
  int i, j, max_rows = 0;
  json_t *rows_array, *array_refs, *result;
  
  if (!dg || DYN_GROUP_NLISTS(dg) == 0) return NULL;
  
  // Find max rows across all lists
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    DYN_LIST *dl = DYN_GROUP_LIST(dg, i);
    if (dl && DYN_LIST_N(dl) > max_rows) {
      max_rows = DYN_LIST_N(dl);
    }
  }
  
  // Create array lookup table (columnar format for nested arrays)
  array_refs = json_object();
  if (!array_refs) return NULL;
  
  for (i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
    DYN_LIST *dl = DYN_GROUP_LIST(dg, i);
    if (dl && DYN_LIST_DATATYPE(dl) == DF_LIST) {
      json_t *array_data = dl_to_json(dl);
      if (array_data) {
        json_object_set_new(array_refs, DYN_LIST_NAME(dl), array_data);
      }
    }
  }
  
  // Create row array
  rows_array = json_array();
  if (!rows_array) {
    json_decref(array_refs);
    return NULL;
  }
  
  for (i = 0; i < max_rows; i++) {
    json_t *row = json_object();
    if (!row) {
      json_decref(array_refs);
      json_decref(rows_array);
      return NULL;
    }
    
    // Add fields to each row
    for (j = 0; j < DYN_GROUP_NLISTS(dg); j++) {
      DYN_LIST *dl = DYN_GROUP_LIST(dg, j);
      if (!dl) continue;
      
      const char *name = DYN_LIST_NAME(dl);
      
      if (DYN_LIST_DATATYPE(dl) == DF_LIST) {
        // For nested arrays, store reference index
        json_object_set_new(row, name, json_integer(i));
      } else {
        // For primitives, include the actual value
        json_t *element = dl_element_to_json(dl, i);
        if (element) {
          json_object_set_new(row, name, element);
        } else {
          json_object_set_new(row, name, json_null());
        }
      }
    }
    
    json_array_append_new(rows_array, row);
  }
  
  // Create hybrid structure
  result = json_object();
  if (!result) {
    json_decref(array_refs);
    json_decref(rows_array);
    return NULL;
  }
  
  json_object_set_new(result, "name", json_string(DYN_GROUP_NAME(dg)));
  json_object_set_new(result, "rows", rows_array);
  json_object_set_new(result, "arrays", array_refs);
  
  return result;
}
