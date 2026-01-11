/*
 * tcl_json.cpp - Tcl dict/list to JSON conversion using nlohmann::json
 *
 * Provides commands:
 *   dict_to_json $dict ?-deep? ?-pretty?   - Convert Tcl dict to JSON object
 *   list_to_json $list ?-deep? ?-pretty?   - Convert Tcl list to JSON array
 *   value_to_json $value ?-deep? ?-pretty? - Auto-detect and convert
 *   json_to_dict $json                     - Convert JSON to Tcl dict/list
 *   json_valid $json                       - Check if string is valid JSON
 *   json_get $json path                    - Extract value by dot-separated path
 *   json_type $json                        - Return JSON value type
 *
 * Options:
 *   -deep   : Recursively convert all nested structures to JSON
 *   -pretty : Format output with indentation
 *
 * Default behavior (shallow):
 *   - Top-level dict keys/values are converted
 *   - Simple values (strings, numbers, booleans) become JSON primitives
 *   - Complex nested values (lists with >1 element) are preserved as 
 *     Tcl-formatted strings in the JSON output
 *
 * This shallow default is intentional for structures like ESS variants
 * where nested dicts/lists have specific Tcl semantics that should be
 * preserved for round-tripping back to Tcl.
 *
 * With -deep flag:
 *   - All nested structures are recursively converted to JSON
 *   - Dicts become JSON objects, lists become JSON arrays
 *   - Use when you know the structure is unambiguous
 *
 * Type conversion rules:
 *   - Tcl dict          -> JSON object
 *   - Tcl list          -> JSON array  
 *   - Integer string    -> JSON number
 *   - Float string      -> JSON number  
 *   - "true"/"false"    -> JSON boolean
 *   - "null"            -> JSON null
 *   - Other strings     -> JSON string
 *
 * Examples:
 *   dict_to_json {name test count 42}
 *   # {"count":42,"name":"test"}
 *
 *   dict_to_json {name test items {a b c}}
 *   # {"items":"a b c","name":"test"}  (items preserved as Tcl string)
 *
 *   dict_to_json {name test items {a b c}} -deep
 *   # {"items":["a","b","c"],"name":"test"}  (items converted to array)
 *
 *   json_to_dict {{"name":"test","count":42}}
 *   # name test count 42
 *
 *   json_get {{"user":{"name":"alice","id":123}}} user.name
 *   # alice
 *
 *   json_type {[1,2,3]}
 *   # array
 */

#include <tcl.h>
#include "nlohmann/json.hpp"
#include <string>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <vector>

using json = nlohmann::json;

// Forward declarations
static json TclObjToJson(Tcl_Interp* interp, Tcl_Obj* obj, bool deep);
static json TclDictToJson(Tcl_Interp* interp, Tcl_Obj* dictObj, bool deep);
static json TclListToJson(Tcl_Interp* interp, Tcl_Obj* listObj, bool deep);
static Tcl_Obj* JsonToTclObj(Tcl_Interp* interp, const json& j);

/*
 * Check if a Tcl_Obj is a valid dict
 */
static bool IsTclDict(Tcl_Obj* obj) {
    Tcl_DictSearch search;
    Tcl_Obj* key;
    Tcl_Obj* value;
    int done;
    
    // Try to iterate - if it works, it's a dict
    if (Tcl_DictObjFirst(NULL, obj, &search, &key, &value, &done) == TCL_OK) {
        Tcl_DictObjDone(&search);
        return true;
    }
    return false;
}

/*
 * Convert a Tcl string value to appropriate JSON primitive type
 */
static json TclStringToJsonPrimitive(const char* str, int length) {
    // Empty string
    if (length == 0) {
        return json("");
    }
    
    // Check for null
    if (strcmp(str, "null") == 0) {
        return json(nullptr);
    }
    
    // Check for boolean
    if (strcmp(str, "true") == 0) {
        return json(true);
    }
    if (strcmp(str, "false") == 0) {
        return json(false);
    }
    
    // Try integer (including negative)
    char* endptr;
    long long llval = strtoll(str, &endptr, 10);
    if (*endptr == '\0' && endptr != str) {
        return json(llval);
    }
    
    // Try double (must contain . or e/E to distinguish from int)
    if (strchr(str, '.') || strchr(str, 'e') || strchr(str, 'E')) {
        double dval = strtod(str, &endptr);
        if (*endptr == '\0' && endptr != str) {
            return json(dval);
        }
    }
    
    // Default: string
    return json(std::string(str, length));
}

/*
 * Convert Tcl dict to JSON object
 * If deep=false, complex values are preserved as Tcl-formatted strings
 * If deep=true, recursively convert all values
 */
static json TclDictToJson(Tcl_Interp* interp, Tcl_Obj* dictObj, bool deep) {
    json result = json::object();
    
    Tcl_DictSearch search;
    Tcl_Obj* key;
    Tcl_Obj* value;
    int done;
    
    if (Tcl_DictObjFirst(interp, dictObj, &search, &key, &value, &done) != TCL_OK) {
        throw std::runtime_error(Tcl_GetString(Tcl_GetObjResult(interp)));
    }
    
    while (!done) {
        const char* keyStr = Tcl_GetString(key);
        
        if (deep) {
            // Recursively convert value
            result[keyStr] = TclObjToJson(interp, value, true);
        } else {
            // Shallow: check if value is simple or complex
            Tcl_Size length;
            const char* valStr = Tcl_GetStringFromObj(value, &length);
            
            Tcl_Size listLen;
            if (Tcl_ListObjLength(NULL, value, &listLen) == TCL_OK && listLen > 1) {
                // Complex value (list with multiple elements) - preserve as Tcl string
                result[keyStr] = std::string(valStr, length);
            } else {
                // Simple value - convert to appropriate JSON type
                result[keyStr] = TclStringToJsonPrimitive(valStr, length);
            }
        }
        
        Tcl_DictObjNext(&search, &key, &value, &done);
    }
    
    Tcl_DictObjDone(&search);
    return result;
}

/*
 * Convert Tcl list to JSON array
 * If deep=false, complex elements are preserved as Tcl-formatted strings
 * If deep=true, recursively convert all elements
 */
static json TclListToJson(Tcl_Interp* interp, Tcl_Obj* listObj, bool deep) {
    json result = json::array();
    
    Tcl_Size length;
    if (Tcl_ListObjLength(interp, listObj, &length) != TCL_OK) {
        throw std::runtime_error(Tcl_GetString(Tcl_GetObjResult(interp)));
    }
    
    for (int i = 0; i < length; i++) {
        Tcl_Obj* elem;
        Tcl_ListObjIndex(interp, listObj, i, &elem);
        
        if (deep) {
            // Recursively convert element
            result.push_back(TclObjToJson(interp, elem, true));
        } else {
            // Shallow: check if element is simple or complex
            Tcl_Size elemLen;
            const char* elemStr = Tcl_GetStringFromObj(elem, &elemLen);
            
            Tcl_Size subListLen;
            if (Tcl_ListObjLength(NULL, elem, &subListLen) == TCL_OK && subListLen > 1) {
                // Complex element - preserve as Tcl string
                result.push_back(std::string(elemStr, elemLen));
            } else {
                // Simple element - convert to appropriate JSON type
                result.push_back(TclStringToJsonPrimitive(elemStr, elemLen));
            }
        }
    }
    
    return result;
}

/*
 * Auto-detect and convert Tcl value to JSON
 * 
 * @param deep - if true, recursively convert nested structures
 *               if false, preserve complex nested values as Tcl strings
 */
static json TclObjToJson(Tcl_Interp* interp, Tcl_Obj* obj, bool deep) {
    Tcl_Size length;
    const char* str = Tcl_GetStringFromObj(obj, &length);
    
    // Empty string should stay as empty string, not become empty object
    if (length == 0) {
        return json("");
    }
    
    // Check list length first
    Tcl_Size listLen;
    if (Tcl_ListObjLength(NULL, obj, &listLen) != TCL_OK) {
        // Not a valid list, treat as primitive
        return TclStringToJsonPrimitive(str, length);
    }
    
    // Single element - treat as primitive
    if (listLen <= 1) {
        return TclStringToJsonPrimitive(str, length);
    }
    
    // Multiple elements - check if it's a dict
    if (IsTclDict(obj)) {
        return TclDictToJson(interp, obj, deep);
    }
    
    // Treat as list
    return TclListToJson(interp, obj, deep);
}

/*
 * Convert JSON value to Tcl object
 * Objects become dicts, arrays become lists, primitives become strings
 */
static Tcl_Obj* JsonToTclObj(Tcl_Interp* interp, const json& j) {
    switch (j.type()) {
        case json::value_t::null:
            return Tcl_NewStringObj("", 0);
            
        case json::value_t::boolean:
            return Tcl_NewStringObj(j.get<bool>() ? "true" : "false", -1);
            
        case json::value_t::number_integer:
            return Tcl_NewWideIntObj(j.get<long long>());
            
        case json::value_t::number_unsigned:
            return Tcl_NewWideIntObj(j.get<unsigned long long>());
            
        case json::value_t::number_float:
            return Tcl_NewDoubleObj(j.get<double>());
            
        case json::value_t::string:
            {
                std::string s = j.get<std::string>();
                return Tcl_NewStringObj(s.c_str(), s.length());
            }
            
        case json::value_t::array:
            {
                Tcl_Obj* listObj = Tcl_NewListObj(0, NULL);
                for (const auto& elem : j) {
                    Tcl_ListObjAppendElement(interp, listObj, JsonToTclObj(interp, elem));
                }
                return listObj;
            }
            
        case json::value_t::object:
            {
                Tcl_Obj* dictObj = Tcl_NewDictObj();
                for (auto it = j.begin(); it != j.end(); ++it) {
                    Tcl_Obj* keyObj = Tcl_NewStringObj(it.key().c_str(), it.key().length());
                    Tcl_Obj* valObj = JsonToTclObj(interp, it.value());
                    Tcl_DictObjPut(interp, dictObj, keyObj, valObj);
                }
                return dictObj;
            }
            
        default:
            return Tcl_NewStringObj("", 0);
    }
}

/*
 * Split a path string by delimiter
 */
static std::vector<std::string> SplitPath(const std::string& path, char delim = '.') {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, delim)) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

/*
 * Tcl command: dict_to_json $dict ?-deep? ?-pretty?
 * 
 * Converts a Tcl dict to a JSON object string.
 * 
 * Options:
 *   -deep   : Recursively convert nested structures (default: shallow)
 *   -pretty : Indent output for readability
 * 
 * Default (shallow) mode preserves complex nested values as Tcl-formatted
 * strings, which is safer for structures like ESS variants where nested
 * dicts/lists have specific Tcl semantics.
 */
static int DictToJsonCmd(ClientData clientData, Tcl_Interp* interp,
                         int objc, Tcl_Obj* const objv[]) {
    bool pretty = false;
    bool deep = false;
    
    if (objc < 2 || objc > 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "dict ?-deep? ?-pretty?");
        return TCL_ERROR;
    }
    
    // Parse options
    for (int i = 2; i < objc; i++) {
        const char* opt = Tcl_GetString(objv[i]);
        if (strcmp(opt, "-pretty") == 0) {
            pretty = true;
        } else if (strcmp(opt, "-deep") == 0) {
            deep = true;
        } else {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown option: %s", opt));
            return TCL_ERROR;
        }
    }
    
    try {
        json result = TclDictToJson(interp, objv[1], deep);
        std::string output = pretty ? result.dump(2) : result.dump();
        Tcl_SetObjResult(interp, Tcl_NewStringObj(output.c_str(), output.length()));
        return TCL_OK;
    } catch (const std::exception& e) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("dict_to_json: %s", e.what()));
        return TCL_ERROR;
    }
}

/*
 * Tcl command: list_to_json $list ?-deep? ?-pretty?
 *
 * Converts a Tcl list to a JSON array string.
 * 
 * Options:
 *   -deep   : Recursively convert nested structures (default: shallow)
 *   -pretty : Indent output for readability
 */
static int ListToJsonCmd(ClientData clientData, Tcl_Interp* interp,
                         int objc, Tcl_Obj* const objv[]) {
    bool pretty = false;
    bool deep = false;
    
    if (objc < 2 || objc > 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "list ?-deep? ?-pretty?");
        return TCL_ERROR;
    }
    
    // Parse options
    for (int i = 2; i < objc; i++) {
        const char* opt = Tcl_GetString(objv[i]);
        if (strcmp(opt, "-pretty") == 0) {
            pretty = true;
        } else if (strcmp(opt, "-deep") == 0) {
            deep = true;
        } else {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown option: %s", opt));
            return TCL_ERROR;
        }
    }
    
    try {
        json result = TclListToJson(interp, objv[1], deep);
        std::string output = pretty ? result.dump(2) : result.dump();
        Tcl_SetObjResult(interp, Tcl_NewStringObj(output.c_str(), output.length()));
        return TCL_OK;
    } catch (const std::exception& e) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("list_to_json: %s", e.what()));
        return TCL_ERROR;
    }
}

/*
 * Tcl command: value_to_json $value ?-deep? ?-pretty?
 *
 * Auto-detects type and converts to appropriate JSON.
 * 
 * Options:
 *   -deep   : Recursively convert nested structures (default: shallow)
 *   -pretty : Indent output for readability
 */
static int ValueToJsonCmd(ClientData clientData, Tcl_Interp* interp,
                          int objc, Tcl_Obj* const objv[]) {
    bool pretty = false;
    bool deep = false;
    
    if (objc < 2 || objc > 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "value ?-deep? ?-pretty?");
        return TCL_ERROR;
    }
    
    // Parse options
    for (int i = 2; i < objc; i++) {
        const char* opt = Tcl_GetString(objv[i]);
        if (strcmp(opt, "-pretty") == 0) {
            pretty = true;
        } else if (strcmp(opt, "-deep") == 0) {
            deep = true;
        } else {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown option: %s", opt));
            return TCL_ERROR;
        }
    }
    
    try {
        json result = TclObjToJson(interp, objv[1], deep);
        std::string output = pretty ? result.dump(2) : result.dump();
        Tcl_SetObjResult(interp, Tcl_NewStringObj(output.c_str(), output.length()));
        return TCL_OK;
    } catch (const std::exception& e) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("value_to_json: %s", e.what()));
        return TCL_ERROR;
    }
}

/*
 * Tcl command: json_to_dict $json
 *
 * Converts a JSON string to a Tcl dict (for objects) or list (for arrays).
 * 
 * Type conversions:
 *   JSON object  -> Tcl dict
 *   JSON array   -> Tcl list
 *   JSON string  -> Tcl string
 *   JSON number  -> Tcl number (int or double)
 *   JSON boolean -> "true" or "false"
 *   JSON null    -> "" (empty string)
 */
static int JsonToDictCmd(ClientData clientData, Tcl_Interp* interp,
                         int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "json");
        return TCL_ERROR;
    }
    
    const char* jsonStr = Tcl_GetString(objv[1]);
    
    // Handle empty input
    if (jsonStr[0] == '\0') {
        Tcl_SetObjResult(interp, Tcl_NewDictObj());
        return TCL_OK;
    }
    
    try {
        json j = json::parse(jsonStr);
        Tcl_SetObjResult(interp, JsonToTclObj(interp, j));
        return TCL_OK;
    } catch (const json::parse_error& e) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("json_to_dict: parse error: %s", e.what()));
        return TCL_ERROR;
    } catch (const std::exception& e) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("json_to_dict: %s", e.what()));
        return TCL_ERROR;
    }
}

/*
 * Tcl command: json_valid $json
 *
 * Returns 1 if the string is valid JSON, 0 otherwise.
 * Does not throw an error on invalid JSON.
 */
static int JsonValidCmd(ClientData clientData, Tcl_Interp* interp,
                        int objc, Tcl_Obj* const objv[]) {
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "json");
        return TCL_ERROR;
    }
    
    const char* jsonStr = Tcl_GetString(objv[1]);
    
    bool valid = json::accept(jsonStr);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(valid ? 1 : 0));
    return TCL_OK;
}

/*
 * Tcl command: json_get $json path
 *
 * Extracts a value from JSON by dot-separated path.
 * For array access, use numeric indices: "items.0.name"
 * 
 * Returns the value as a Tcl object (dict, list, or primitive).
 * Returns empty string if path doesn't exist.
 *
 * Examples:
 *   json_get {{"user":{"name":"alice"}}} user.name
 *   # alice
 *
 *   json_get {{"items":[{"id":1},{"id":2}]}} items.1.id
 *   # 2
 */
static int JsonGetCmd(ClientData clientData, Tcl_Interp* interp,
                      int objc, Tcl_Obj* const objv[]) {
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "json path");
        return TCL_ERROR;
    }
    
    const char* jsonStr = Tcl_GetString(objv[1]);
    const char* pathStr = Tcl_GetString(objv[2]);
    
    try {
        json j = json::parse(jsonStr);
        
        // Navigate the path
        std::vector<std::string> parts = SplitPath(pathStr);
        json* current = &j;
        
        for (const auto& part : parts) {
            if (current->is_object()) {
                if (current->contains(part)) {
                    current = &(*current)[part];
                } else {
                    // Path not found
                    Tcl_SetObjResult(interp, Tcl_NewStringObj("", 0));
                    return TCL_OK;
                }
            } else if (current->is_array()) {
                // Try to parse as index
                char* endptr;
                long idx = strtol(part.c_str(), &endptr, 10);
                if (*endptr == '\0' && idx >= 0 && idx < (long)current->size()) {
                    current = &(*current)[idx];
                } else {
                    // Invalid index
                    Tcl_SetObjResult(interp, Tcl_NewStringObj("", 0));
                    return TCL_OK;
                }
            } else {
                // Can't navigate into primitive
                Tcl_SetObjResult(interp, Tcl_NewStringObj("", 0));
                return TCL_OK;
            }
        }
        
        Tcl_SetObjResult(interp, JsonToTclObj(interp, *current));
        return TCL_OK;
        
    } catch (const json::parse_error& e) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("json_get: parse error: %s", e.what()));
        return TCL_ERROR;
    } catch (const std::exception& e) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("json_get: %s", e.what()));
        return TCL_ERROR;
    }
}

/*
 * Tcl command: json_type $json ?path?
 *
 * Returns the JSON type of the value: "object", "array", "string", 
 * "number", "boolean", "null", or "invalid".
 *
 * If path is provided, returns the type of the value at that path.
 */
static int JsonTypeCmd(ClientData clientData, Tcl_Interp* interp,
                       int objc, Tcl_Obj* const objv[]) {
    if (objc < 2 || objc > 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "json ?path?");
        return TCL_ERROR;
    }
    
    const char* jsonStr = Tcl_GetString(objv[1]);
    
    try {
        json j = json::parse(jsonStr);
        
        // Navigate to path if provided
        if (objc == 3) {
            const char* pathStr = Tcl_GetString(objv[2]);
            std::vector<std::string> parts = SplitPath(pathStr);
            
            for (const auto& part : parts) {
                if (j.is_object() && j.contains(part)) {
                    j = j[part];
                } else if (j.is_array()) {
                    char* endptr;
                    long idx = strtol(part.c_str(), &endptr, 10);
                    if (*endptr == '\0' && idx >= 0 && idx < (long)j.size()) {
                        j = j[idx];
                    } else {
                        Tcl_SetObjResult(interp, Tcl_NewStringObj("invalid", -1));
                        return TCL_OK;
                    }
                } else {
                    Tcl_SetObjResult(interp, Tcl_NewStringObj("invalid", -1));
                    return TCL_OK;
                }
            }
        }
        
        const char* typeStr;
        switch (j.type()) {
            case json::value_t::null:            typeStr = "null"; break;
            case json::value_t::boolean:         typeStr = "boolean"; break;
            case json::value_t::number_integer:  typeStr = "number"; break;
            case json::value_t::number_unsigned: typeStr = "number"; break;
            case json::value_t::number_float:    typeStr = "number"; break;
            case json::value_t::string:          typeStr = "string"; break;
            case json::value_t::array:           typeStr = "array"; break;
            case json::value_t::object:          typeStr = "object"; break;
            default:                             typeStr = "unknown"; break;
        }
        
        Tcl_SetObjResult(interp, Tcl_NewStringObj(typeStr, -1));
        return TCL_OK;
        
    } catch (const json::parse_error& e) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("invalid", -1));
        return TCL_OK;
    } catch (const std::exception& e) {
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("json_type: %s", e.what()));
        return TCL_ERROR;
    }
}

/*
 * Register all commands with an interpreter
 * Call this from your dserv initialization for each interp
 */
extern "C" {

int TclJson_RegisterCommands(Tcl_Interp* interp) {
    Tcl_CreateObjCommand(interp, "dict_to_json", DictToJsonCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "list_to_json", ListToJsonCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "value_to_json", ValueToJsonCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "json_to_dict", JsonToDictCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "json_valid", JsonValidCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "json_get", JsonGetCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "json_type", JsonTypeCmd, NULL, NULL);
    return TCL_OK;
}

/*
 * Standard Tcl package initialization (if loading as extension)
 */
int Tcljson_Init(Tcl_Interp* interp) {
    if (Tcl_InitStubs(interp, "8.6-", 0) == NULL) {
        return TCL_ERROR;
    }
    
    TclJson_RegisterCommands(interp);
    
    Tcl_PkgProvide(interp, "tcljson", "1.0");
    return TCL_OK;
}

} // extern "C"
