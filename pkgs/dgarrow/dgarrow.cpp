// dgarrow.cpp - Simple working version with recursive lists
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <vector>
#include <string>

#include <arrow/api.h>
#include <arrow/ipc/api.h>
#include <arrow/io/api.h>

#include <tcl.h>

extern "C" {
#include "df.h"
#include "dynio.h"
#include "tcl_dl.h"
}

// Forward declarations
std::shared_ptr<arrow::Array> dynlist_to_arrow_array(DYN_LIST* dl);
std::shared_ptr<arrow::Field> dynlist_to_arrow_field(DYN_LIST* dl);
DYN_LIST* arrow_array_to_dynlist(std::shared_ptr<arrow::Array> array, const std::string& name);

// Helper function to determine Arrow data type from DYN_LIST
std::shared_ptr<arrow::DataType> dynlist_to_arrow_type(DYN_LIST* dl) {
    if (!dl) return nullptr;
    
    switch (DYN_LIST_DATATYPE(dl)) {
        case DF_LONG:
            return arrow::int32();
        case DF_SHORT:
            return arrow::int16();
        case DF_CHAR:
            return arrow::uint8();
        case DF_FLOAT:
            return arrow::float32();
        case DF_STRING:
            return arrow::utf8();
        case DF_LIST: {
            // Determine the type of nested lists
            if (DYN_LIST_N(dl) > 0) {
                DYN_LIST** vals = (DYN_LIST**)DYN_LIST_VALS(dl);
                for (int i = 0; i < DYN_LIST_N(dl); i++) {
                    if (vals[i]) {
                        auto nested_type = dynlist_to_arrow_type(vals[i]);
                        if (nested_type) {
                            return arrow::list(nested_type);
                        }
                    }
                }
            }
            // Default to list of floats
            return arrow::list(arrow::float32());
        }
        default:
            return nullptr;
    }
}

// Recursive function to convert DYN_LIST to Arrow Array
std::shared_ptr<arrow::Array> dynlist_to_arrow_array(DYN_LIST* dl) {
    if (!dl) return nullptr;
    
    switch (DYN_LIST_DATATYPE(dl)) {
        case DF_LONG: {
            arrow::Int32Builder builder;
            int* vals = (int*)DYN_LIST_VALS(dl);
            for (int i = 0; i < DYN_LIST_N(dl); i++) {
                auto status = builder.Append(vals[i]);
                if (!status.ok()) return nullptr;
            }
            std::shared_ptr<arrow::Array> array;
            auto status = builder.Finish(&array);
            return status.ok() ? array : nullptr;
        }
        
        case DF_SHORT: {
            arrow::Int16Builder builder;
            short* vals = (short*)DYN_LIST_VALS(dl);
            for (int i = 0; i < DYN_LIST_N(dl); i++) {
                auto status = builder.Append(vals[i]);
                if (!status.ok()) return nullptr;
            }
            std::shared_ptr<arrow::Array> array;
            auto status = builder.Finish(&array);
            return status.ok() ? array : nullptr;
        }
        
        case DF_CHAR: {
            arrow::UInt8Builder builder;
            char* vals = (char*)DYN_LIST_VALS(dl);
            for (int i = 0; i < DYN_LIST_N(dl); i++) {
                auto status = builder.Append((uint8_t)vals[i]);
                if (!status.ok()) return nullptr;
            }
            std::shared_ptr<arrow::Array> array;
            auto status = builder.Finish(&array);
            return status.ok() ? array : nullptr;
        }
        
        case DF_FLOAT: {
            arrow::FloatBuilder builder;
            float* vals = (float*)DYN_LIST_VALS(dl);
            for (int i = 0; i < DYN_LIST_N(dl); i++) {
                auto status = builder.Append(vals[i]);
                if (!status.ok()) return nullptr;
            }
            std::shared_ptr<arrow::Array> array;
            auto status = builder.Finish(&array);
            return status.ok() ? array : nullptr;
        }

        case DF_STRING: {
            arrow::StringBuilder builder;
            char** vals = (char**)DYN_LIST_VALS(dl);
            for (int i = 0; i < DYN_LIST_N(dl); i++) {
                arrow::Status status;
                if (vals[i]) {
                    status = builder.Append(vals[i]);
                } else {
                    status = builder.AppendNull();
                }
                if (!status.ok()) return nullptr;
            }
            std::shared_ptr<arrow::Array> array;
            auto status = builder.Finish(&array);
            return status.ok() ? array : nullptr;
        }
        
        case DF_LIST: {
            // Handle nested lists recursively
            auto nested_type = dynlist_to_arrow_type(dl);
            if (!nested_type || nested_type->id() != arrow::Type::LIST) {
                return nullptr;
            }
            
            auto list_type = std::static_pointer_cast<arrow::ListType>(nested_type);
            auto value_type = list_type->value_type();
            
            // Create list builder
            std::unique_ptr<arrow::ArrayBuilder> value_builder;
            auto status = arrow::MakeBuilder(arrow::default_memory_pool(), value_type, &value_builder);
            if (!status.ok()) return nullptr;
            
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), std::move(value_builder));
            
            // Build array of individual nested arrays
            std::vector<std::shared_ptr<arrow::Array>> nested_arrays;
            DYN_LIST** vals = (DYN_LIST**)DYN_LIST_VALS(dl);
            
            for (int i = 0; i < DYN_LIST_N(dl); i++) {
                if (vals[i]) {
                    // Recursively convert each nested list to Arrow array
                    auto nested_array = dynlist_to_arrow_array(vals[i]);
                    nested_arrays.push_back(nested_array);
                } else {
                    nested_arrays.push_back(nullptr);
                }
            }
            
            // Now build the list array from the nested arrays
            for (int i = 0; i < DYN_LIST_N(dl); i++) {
                status = list_builder.Append();
                if (!status.ok()) return nullptr;
                
                if (nested_arrays[i]) {
                    auto nested_array = nested_arrays[i];
                    // Append all elements from the nested array
                    for (int64_t j = 0; j < nested_array->length(); j++) {
                        // Cast the value builder to the appropriate type and append
                        if (value_type->id() == arrow::Type::FLOAT) {
                            auto float_builder = static_cast<arrow::FloatBuilder*>(list_builder.value_builder());
                            auto float_array = std::static_pointer_cast<arrow::FloatArray>(nested_array);
                            if (!float_array->IsNull(j)) {
                                status = float_builder->Append(float_array->Value(j));
                                if (!status.ok()) return nullptr;
                            }
                        } else if (value_type->id() == arrow::Type::INT32) {
                            auto int_builder = static_cast<arrow::Int32Builder*>(list_builder.value_builder());
                            auto int_array = std::static_pointer_cast<arrow::Int32Array>(nested_array);
                            if (!int_array->IsNull(j)) {
                                status = int_builder->Append(int_array->Value(j));
                                if (!status.ok()) return nullptr;
                            }
                        }
                        // Add other types as needed...
                    }
                }
            }
            
            std::shared_ptr<arrow::Array> array;
            status = list_builder.Finish(&array);
            return status.ok() ? array : nullptr;
        }
        
        default:
            return nullptr;
    }
}

// Helper function to get Arrow field type from DYN_LIST
std::shared_ptr<arrow::Field> dynlist_to_arrow_field(DYN_LIST* dl) {
    if (!dl) return nullptr;
    
    std::string name(DYN_LIST_NAME(dl));
    auto arrow_type = dynlist_to_arrow_type(dl);
    
    return arrow_type ? arrow::field(name, arrow_type) : nullptr;
}

// Recursive helper to rebuild DYN_LIST from Arrow array
DYN_LIST* arrow_array_to_dynlist(std::shared_ptr<arrow::Array> array, const std::string& name) {
    if (!array) return nullptr;
    
    switch (array->type_id()) {
        case arrow::Type::INT32: {
            auto int32_array = std::static_pointer_cast<arrow::Int32Array>(array);
            DYN_LIST* dl = dfuCreateNamedDynList(const_cast<char*>(name.c_str()), DF_LONG, 100);
            for (int64_t i = 0; i < array->length(); i++) {
                if (!int32_array->IsNull(i)) {
                    dfuAddDynListLong(dl, int32_array->Value(i));
                }
            }
            return dl;
        }
        
        case arrow::Type::INT16: {
            auto int16_array = std::static_pointer_cast<arrow::Int16Array>(array);
            DYN_LIST* dl = dfuCreateNamedDynList(const_cast<char*>(name.c_str()), DF_SHORT, 100);
            for (int64_t i = 0; i < array->length(); i++) {
                if (!int16_array->IsNull(i)) {
                    dfuAddDynListShort(dl, int16_array->Value(i));
                }
            }
            return dl;
        }
        
        case arrow::Type::UINT8: {
            auto uint8_array = std::static_pointer_cast<arrow::UInt8Array>(array);
            DYN_LIST* dl = dfuCreateNamedDynList(const_cast<char*>(name.c_str()), DF_CHAR, 100);
            for (int64_t i = 0; i < array->length(); i++) {
                if (!uint8_array->IsNull(i)) {
                    dfuAddDynListChar(dl, (char)uint8_array->Value(i));
                }
            }
            return dl;
        }
        
        case arrow::Type::FLOAT: {
            auto float_array = std::static_pointer_cast<arrow::FloatArray>(array);
            DYN_LIST* dl = dfuCreateNamedDynList(const_cast<char*>(name.c_str()), DF_FLOAT, 100);
            for (int64_t i = 0; i < array->length(); i++) {
                if (!float_array->IsNull(i)) {
                    dfuAddDynListFloat(dl, float_array->Value(i));
                }
            }
            return dl;
        }
        
        case arrow::Type::STRING: {
            auto string_array = std::static_pointer_cast<arrow::StringArray>(array);
            DYN_LIST* dl = dfuCreateNamedDynList(const_cast<char*>(name.c_str()), DF_STRING, 100);
            for (int64_t i = 0; i < array->length(); i++) {
                if (!string_array->IsNull(i)) {
                    std::string str_val = string_array->GetString(i);
                    dfuAddDynListString(dl, const_cast<char*>(str_val.c_str()));
                }
            }
            return dl;
        }
        
        case arrow::Type::LIST: {
            auto list_array = std::static_pointer_cast<arrow::ListArray>(array);
            DYN_LIST* dl = dfuCreateNamedDynList(const_cast<char*>(name.c_str()), DF_LIST, 100);
            
            for (int64_t i = 0; i < array->length(); i++) {
                if (!list_array->IsNull(i)) {
                    // Get the slice for this list element
                    auto slice = list_array->value_slice(i);
                    
                    // Recursively convert the slice back to a DYN_LIST
                    std::string nested_name = "nested";
                    DYN_LIST* nested_list = arrow_array_to_dynlist(slice, nested_name);
                    
                    if (nested_list) {
                        dfuAddDynListList(dl, nested_list);
                    }
                }
            }
            return dl;
        }
        
        default:
            return nullptr;
    }
}

// Internal serialization function
std::shared_ptr<arrow::Buffer> serialize_to_arrow(DYN_GROUP* dg) {
    if (!dg || DYN_GROUP_NLISTS(dg) == 0) {
        return nullptr;
    }
    
    // Check that all lists have the same length
    int expected_length = -1;
    for (int i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
        DYN_LIST* dl = DYN_GROUP_LIST(dg, i);
        if (!dl) continue;
        
        if (expected_length == -1) {
            expected_length = DYN_LIST_N(dl);
        } else if (DYN_LIST_N(dl) != expected_length) {
            return nullptr; // Mismatched lengths
        }
    }
    
    // Build schema and arrays
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    
    for (int i = 0; i < DYN_GROUP_NLISTS(dg); i++) {
        DYN_LIST* dl = DYN_GROUP_LIST(dg, i);
        if (!dl) continue;
        
        auto field = dynlist_to_arrow_field(dl);
        auto array = dynlist_to_arrow_array(dl);
        
        if (field && array) {
            fields.push_back(field);
            arrays.push_back(array);
        }
    }
    
    if (fields.empty()) {
        return nullptr;
    }
    
    // Create and serialize table
    auto schema = arrow::schema(fields);
    auto table = arrow::Table::Make(schema, arrays);
    
    auto output_stream = arrow::io::BufferOutputStream::Create().ValueOrDie();
    auto writer_result = arrow::ipc::MakeStreamWriter(output_stream, schema);
    if (!writer_result.ok()) return nullptr;
    
    auto writer = writer_result.ValueOrDie();
    auto status = writer->WriteTable(*table);
    if (!status.ok()) return nullptr;
    
    status = writer->Close();
    if (!status.ok()) return nullptr;
    
    auto buffer_result = output_stream->Finish();
    if (!buffer_result.ok()) return nullptr;
    
    return buffer_result.ValueOrDie();
}

// Internal deserialization function  
DYN_GROUP* deserialize_from_arrow(const uint8_t* data, size_t size, const char* group_name) {
    auto buffer = arrow::Buffer::Wrap(data, size);
    auto input_stream = std::make_shared<arrow::io::BufferReader>(buffer);
    
    auto reader_result = arrow::ipc::RecordBatchStreamReader::Open(input_stream);
    if (!reader_result.ok()) return nullptr;
    
    auto reader = reader_result.ValueOrDie();
    
    std::vector<std::shared_ptr<arrow::RecordBatch>> batches;
    while (true) {
        auto batch_result = reader->Next();
        if (!batch_result.ok()) break;
        auto batch = batch_result.ValueOrDie();
        if (!batch) break;
        batches.push_back(batch);
    }
    
    if (batches.empty()) return nullptr;
    
    auto schema = batches[0]->schema();
    DYN_GROUP* dg = dfuCreateNamedDynGroup(const_cast<char*>(group_name), schema->num_fields());
    
    for (int field_idx = 0; field_idx < schema->num_fields(); field_idx++) {
        auto field = schema->field(field_idx);
        auto column = batches[0]->column(field_idx);
        
        // Use the recursive helper to rebuild the DYN_LIST
        DYN_LIST* rebuilt_list = arrow_array_to_dynlist(column, field->name());
        if (rebuilt_list) {
            dfuAddDynGroupExistingList(dg, const_cast<char*>(field->name().c_str()), rebuilt_list);
        }
    }
    
    return dg;
}

// C-linkage wrapper functions
extern "C" {

int dg_write_arrow_file(DYN_GROUP* dg, const char* filename) {
    auto buffer = serialize_to_arrow(dg);
    if (!buffer) return -1;
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    size_t written = fwrite(buffer->data(), 1, buffer->size(), fp);
    fclose(fp);
    
    return (written == buffer->size()) ? 0 : -1;
}

int dg_get_arrow_data(DYN_GROUP* dg, uint8_t** data, size_t* size) {
    auto buffer = serialize_to_arrow(dg);
    if (!buffer) {
        *data = nullptr;
        *size = 0;
        return -1;
    }
    
    *size = buffer->size();
    *data = (uint8_t*)malloc(*size);
    if (!*data) {
        *size = 0;
        return -1;
    }
    
    memcpy(*data, buffer->data(), *size);
    return 0;
}

DYN_GROUP* dg_read_arrow_file(const char* filename, const char* group_name) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return nullptr;
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t* buffer = (uint8_t*)malloc(file_size);
    if (!buffer) {
        fclose(fp);
        return nullptr;
    }
    
    fread(buffer, 1, file_size, fp);
    fclose(fp);
    
    DYN_GROUP* result = deserialize_from_arrow(buffer, file_size, group_name);
    free(buffer);
    
    return result;
}

DYN_GROUP* dg_deserialize_from_arrow_with_name(const uint8_t* data, size_t size, const char* group_name) {
    return deserialize_from_arrow(data, size, group_name);
}


} // extern "C"

static int dg_to_arrow_file(ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    DYN_GROUP *dg;
    char *dgname;
    
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "dyngroup filename");
        return TCL_ERROR;
    }
    
    dgname = Tcl_GetStringFromObj(objv[1], NULL);
    if (tclFindDynGroup(interp, dgname, &dg) != TCL_OK) return TCL_ERROR;
    
    char *filename = Tcl_GetStringFromObj(objv[2], NULL);
    
    auto buffer = serialize_to_arrow(dg);
    if (!buffer) {
        Tcl_AppendResult(interp, "dg_toArrowFile_full: error serializing to Arrow", NULL);
        return TCL_ERROR;
    }
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        Tcl_AppendResult(interp, "dg_toArrowFile_full: error opening file", NULL);
        return TCL_ERROR;
    }
    
    size_t written = fwrite(buffer->data(), 1, buffer->size(), fp);
    fclose(fp);
    
    if (written != buffer->size()) {
        Tcl_AppendResult(interp, "dg_toArrowFile_full: error writing file", NULL);
        return TCL_ERROR;
    }
    
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1)); // Success
    return TCL_OK;
}

static int dg_to_arrow(ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    DYN_GROUP *dg;
    char *dgname;
    
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "dyngroup varname");
        return TCL_ERROR;
    }
    
    dgname = Tcl_GetStringFromObj(objv[1], NULL);
    if (tclFindDynGroup(interp, dgname, &dg) != TCL_OK) return TCL_ERROR;
    
    auto buffer = serialize_to_arrow(dg);
    if (!buffer) {
        Tcl_AppendResult(interp, "dg_toArrowData_full: error serializing to Arrow", NULL);
        return TCL_ERROR;
    }
    
    // Create Tcl byte array object
    Tcl_Obj *o = Tcl_NewByteArrayObj((unsigned char*)buffer->data(), buffer->size());
    
    // Set the variable
    if (Tcl_ObjSetVar2(interp, objv[2], NULL, o, TCL_LEAVE_ERR_MSG) == NULL) {
        return TCL_ERROR;
    }
    
    // Return the size
    Tcl_SetObjResult(interp, Tcl_NewIntObj((int)buffer->size()));
    return TCL_OK;
}

// For reading Arrow files back to DYN_GROUP
static int dg_from_arrow_file(ClientData data, Tcl_Interp * interp, int objc,
			      Tcl_Obj * const objv[])
{
  DYN_GROUP *dg;
  char *filename;
  char *dgname;
  
  // dg_fromArrowFile filename dyngroup_name
  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "filename dyngroup_name");
    return TCL_ERROR;
  }
  
  filename = Tcl_GetStringFromObj(objv[1], NULL);
  dgname = Tcl_GetStringFromObj(objv[2], NULL);
  
  dg = dg_read_arrow_file(filename, dgname);
  if (!dg) {
    Tcl_AppendResult(interp, "dg_fromArrowFile: error reading Arrow file", NULL);
    return TCL_ERROR;
  }
  
  return (tclPutGroup(interp, dg));
}

// For reading Arrow data from byte array
static int dg_from_arrow(ClientData data, Tcl_Interp * interp, int objc,
			 Tcl_Obj * const objv[])
{
  DYN_GROUP *dg;
  char *dgname;
  unsigned char *arrow_data;
  Tcl_Size arrow_size;
  
  // dg_fromArrowData arrow_data_var dyngroup_name
  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "arrow_data_var dyngroup_name");
    return TCL_ERROR;
  }
  
  dgname = Tcl_GetStringFromObj(objv[2], NULL);
  
  // Get the byte array from the Tcl variable
  arrow_data = Tcl_GetByteArrayFromObj(objv[1], &arrow_size);
  if (!arrow_data) {
    Tcl_AppendResult(interp, "dg_fromArrowData: invalid arrow data", NULL);
    return TCL_ERROR;
  }
  
  dg = dg_deserialize_from_arrow_with_name(arrow_data,
					   (size_t) arrow_size, dgname);
  if (!dg) {
    Tcl_AppendResult(interp,
		     "dg_fromArrowData: error deserializing Arrow data", NULL);
    return TCL_ERROR;
  }
  
  return (tclPutGroup(interp, dg));
}


// Extension initialization function
extern "C" int Dgarrow_Init(Tcl_Interp *interp)
{
    if (Tcl_InitStubs(interp, "9.0", 0) == nullptr) {
        return TCL_ERROR;
    }

    if (Tcl_PkgProvide(interp, "dgarrow_full", "1.0") != TCL_OK) {
        return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp, "dg_toArrowFile", dg_to_arrow_file, 
                         (ClientData)0, NULL);
    Tcl_CreateObjCommand(interp, "dg_toArrow", dg_to_arrow, 
                         (ClientData)0, NULL);

    Tcl_CreateObjCommand(interp, "dg_fromArrowFile", dg_from_arrow_file,
			 (ClientData) 0, NULL);
    Tcl_CreateObjCommand(interp, "dg_fromArrow", dg_from_arrow, 
			 (ClientData) 0, NULL);
    
    return TCL_OK;
}
