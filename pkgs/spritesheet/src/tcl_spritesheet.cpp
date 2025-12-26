#include "spritesheet_extract.h"
#include <tcl.h>
#include <json.hpp>
#include <tinyxml2.h>
#include <fstream>
#include <sstream>

using namespace spritesheet;
using json = nlohmann::json;
using namespace tinyxml2;

// Helper: Convert Point vector to Tcl list
static Tcl_Obj* points_to_tcl_list(Tcl_Interp* interp, 
                                   const std::vector<Point>& points) {
    Tcl_Obj* list = Tcl_NewListObj(0, NULL);
    for (const auto& p : points) {
        Tcl_Obj* pair = Tcl_NewListObj(0, NULL);
        Tcl_ListObjAppendElement(interp, pair, Tcl_NewDoubleObj(p.x));
        Tcl_ListObjAppendElement(interp, pair, Tcl_NewDoubleObj(p.y));
        Tcl_ListObjAppendElement(interp, list, pair);
    }
    return list;
}

// Helper: Convert Polygon vector to Tcl list
static Tcl_Obj* polygons_to_tcl_list(Tcl_Interp* interp,
                                     const std::vector<Polygon>& polygons) {
    Tcl_Obj* list = Tcl_NewListObj(0, NULL);
    for (const auto& poly : polygons) {
        Tcl_Obj* dict = Tcl_NewDictObj();
        Tcl_DictObjPut(interp, dict, 
                      Tcl_NewStringObj("vertices", -1),
                      points_to_tcl_list(interp, poly.vertices));
        Tcl_DictObjPut(interp, dict,
                      Tcl_NewStringObj("convex", -1),
                      Tcl_NewBooleanObj(poly.is_convex));
        Tcl_DictObjPut(interp, dict,
                      Tcl_NewStringObj("vertex_count", -1),
                      Tcl_NewIntObj(poly.vertices.size()));
        Tcl_ListObjAppendElement(interp, list, dict);
    }
    return list;
}

// Helper: Parse parameters
static bool parse_collision_params(Tcl_Interp* interp,
                                   int objc, Tcl_Obj* const objv[],
                                   int start_idx,
                                   DecomposeParams& params,
                                   unsigned char& alpha_threshold) {
    alpha_threshold = 128;
    params.simplify_epsilon = 2.0f;
    params.min_area = 10.0f;
    params.max_vertices = 8;
    params.collinear_threshold = 0.1f;
    
    for (int i = start_idx; i < objc; i += 2) {
        if (i + 1 >= objc) {
            Tcl_SetResult(interp, (char*)"Missing value for option", TCL_STATIC);
            return false;
        }
        
        const char* opt = Tcl_GetString(objv[i]);
        Tcl_Obj* val = objv[i + 1];
        
        if (strcmp(opt, "-threshold") == 0) {
            int tmp;
            if (Tcl_GetIntFromObj(interp, val, &tmp) != TCL_OK) return false;
            alpha_threshold = (unsigned char)tmp;
        }
        else if (strcmp(opt, "-epsilon") == 0) {
            double tmp;
            if (Tcl_GetDoubleFromObj(interp, val, &tmp) != TCL_OK) return false;
            params.simplify_epsilon = tmp;
        }
        else if (strcmp(opt, "-min_area") == 0) {
            double tmp;
            if (Tcl_GetDoubleFromObj(interp, val, &tmp) != TCL_OK) return false;
            params.min_area = tmp;
        }
        else if (strcmp(opt, "-max_vertices") == 0) {
            int tmp;
            if (Tcl_GetIntFromObj(interp, val, &tmp) != TCL_OK) return false;
            params.max_vertices = tmp;
        }
        else if (strcmp(opt, "-collinear") == 0) {
            double tmp;
            if (Tcl_GetDoubleFromObj(interp, val, &tmp) != TCL_OK) return false;
            params.collinear_threshold = tmp;
        }
    }
    
    return true;
}

// XML to JSON conversion
static json xml_to_aseprite_json(const std::string& xml_path) {
    XMLDocument doc;
    if (doc.LoadFile(xml_path.c_str()) != XML_SUCCESS) {
        throw std::runtime_error("Failed to load XML file");
    }
    
    XMLElement* atlas = doc.FirstChildElement("TextureAtlas");
    if (!atlas) {
        throw std::runtime_error("Missing TextureAtlas element");
    }
    
    const char* image_path = atlas->Attribute("imagePath");
    if (!image_path) {
        throw std::runtime_error("Missing imagePath attribute");
    }
    
    json output;
    output["meta"]["image"] = image_path;
    output["meta"]["format"] = "xml_atlas";
    
    json frames_obj = json::object();
    
    for (XMLElement* sub = atlas->FirstChildElement("SubTexture");
         sub != nullptr;
         sub = sub->NextSiblingElement("SubTexture")) {
        
        std::string name = sub->Attribute("name");
        
        json frame_data;
        frame_data["frame"]["x"] = sub->IntAttribute("x");
        frame_data["frame"]["y"] = sub->IntAttribute("y");
        frame_data["frame"]["w"] = sub->IntAttribute("width");
        frame_data["frame"]["h"] = sub->IntAttribute("height");
        
        if (sub->Attribute("frameX")) {
            frame_data["spriteSourceSize"]["x"] = sub->IntAttribute("frameX");
            frame_data["spriteSourceSize"]["y"] = sub->IntAttribute("frameY");
            frame_data["sourceSize"]["w"] = sub->IntAttribute("frameWidth");
            frame_data["sourceSize"]["h"] = sub->IntAttribute("frameHeight");
            frame_data["trimmed"] = true;
        } else {
            frame_data["trimmed"] = false;
        }
        
        frames_obj[name] = frame_data;
    }
    
    output["frames"] = frames_obj;
    
    return output;
}

// collision::extract
static int Tcl_ExtractCollision(ClientData, Tcl_Interp* interp,
                                int objc, Tcl_Obj* const objv[]) {
    if (objc < 6) {
        Tcl_WrongNumArgs(interp, 1, objv, 
                        "sprite_path x y width height ?options?");
        return TCL_ERROR;
    }
    
    const char* path = Tcl_GetString(objv[1]);
    int x, y, w, h;
    
    if (Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[4], &w) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[5], &h) != TCL_OK) return TCL_ERROR;
    
    DecomposeParams params;
    unsigned char threshold;
    
    if (!parse_collision_params(interp, objc, objv, 6, params, threshold)) {
        return TCL_ERROR;
    }
    
    try {
        CollisionData data = extract_collision(path, x, y, w, h,
                                              threshold,
                                              params.simplify_epsilon,
                                              params.min_area);
        
        Tcl_Obj* result = Tcl_NewDictObj();
        Tcl_DictObjPut(interp, result,
                      Tcl_NewStringObj("width", -1),
                      Tcl_NewIntObj(data.frame_width));
        Tcl_DictObjPut(interp, result,
                      Tcl_NewStringObj("height", -1),
                      Tcl_NewIntObj(data.frame_height));
        Tcl_DictObjPut(interp, result,
                      Tcl_NewStringObj("fixtures", -1),
                      polygons_to_tcl_list(interp, data.fixtures));
        Tcl_DictObjPut(interp, result,
                      Tcl_NewStringObj("fixture_count", -1),
                      Tcl_NewIntObj(data.fixtures.size()));
        
        Tcl_SetObjResult(interp, result);
        return TCL_OK;
        
    } catch (const std::exception& e) {
        Tcl_SetResult(interp, (char*)e.what(), TCL_VOLATILE);
        return TCL_ERROR;
    }
}

// collision::extract_json
static int Tcl_ExtractCollisionJson(ClientData, Tcl_Interp* interp,
                                    int objc, Tcl_Obj* const objv[]) {
    if (objc < 6) {
        Tcl_WrongNumArgs(interp, 1, objv, 
                        "sprite_path x y width height ?options?");
        return TCL_ERROR;
    }
    
    const char* path = Tcl_GetString(objv[1]);
    int x, y, w, h;
    
    if (Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[4], &w) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[5], &h) != TCL_OK) return TCL_ERROR;
    
    DecomposeParams params;
    unsigned char threshold;
    bool pretty = false;
    
    if (!parse_collision_params(interp, objc, objv, 6, params, threshold)) {
        return TCL_ERROR;
    }
    
    for (int i = 6; i < objc; i += 2) {
        const char* opt = Tcl_GetString(objv[i]);
        if (strcmp(opt, "-pretty") == 0) {
            int tmp;
            if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &tmp) != TCL_OK) {
                return TCL_ERROR;
            }
            pretty = (tmp != 0);
            break;
        }
    }
    
    try {
        CollisionData data = extract_collision(path, x, y, w, h,
                                              threshold,
                                              params.simplify_epsilon,
                                              params.min_area);
        
        json fixtures = json::array();
        for (const auto& fix : data.fixtures) {
            json vertices = json::array();
            for (const auto& v : fix.vertices) {
                vertices.push_back({{"x", v.x}, {"y", v.y}});
            }
            fixtures.push_back({
                {"vertices", vertices},
                {"convex", fix.is_convex},
                {"vertex_count", fix.vertices.size()}
            });
        }
        
        json output = {
            {"width", data.frame_width},
            {"height", data.frame_height},
            {"visual_bounds", {
                {"x", data.visual_bounds.content_x},
                {"y", data.visual_bounds.content_y},
                {"w", data.visual_bounds.content_width},
                {"h", data.visual_bounds.content_height}
            }},
            {"canvas_size", {
                {"w", data.visual_bounds.canvas_width},
                {"h", data.visual_bounds.canvas_height}
            }},
            {"fixtures", fixtures},
            {"fixture_count", data.fixtures.size()}
        };
        
        std::string json_str = pretty ? output.dump(2) : output.dump();
        Tcl_SetObjResult(interp, Tcl_NewStringObj(json_str.c_str(), -1));
        
        return TCL_OK;
        
    } catch (const std::exception& e) {
        Tcl_SetResult(interp, (char*)e.what(), TCL_VOLATILE);
        return TCL_ERROR;
    }
}

static int Tcl_ProcessSpritesheetJson(ClientData, Tcl_Interp* interp,
                                      int objc, Tcl_Obj* const objv[]) {
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "aseprite_json ?options?");
        return TCL_ERROR;
    }
    
    const char* input_path = Tcl_GetString(objv[1]);
    
    DecomposeParams params;
    unsigned char threshold = 128;
    bool pretty = false;
    
    if (!parse_collision_params(interp, objc, objv, 2, params, threshold)) {
        return TCL_ERROR;
    }
    
    for (int i = 2; i < objc; i += 2) {
        const char* opt = Tcl_GetString(objv[i]);
        if (strcmp(opt, "-pretty") == 0) {
            int tmp;
            Tcl_GetBooleanFromObj(interp, objv[i + 1], &tmp);
            pretty = (tmp != 0);
        }
    }
    
    try {
        std::ifstream ase_file(input_path);
        if (!ase_file.is_open()) {
            std::string err = "Cannot open file: ";
            err += input_path;
            Tcl_SetResult(interp, (char*)err.c_str(), TCL_VOLATILE);
            return TCL_ERROR;
        }
        
        json ase_json;
        ase_file >> ase_json;
        ase_file.close();
        
        // ERROR HANDLING: Check required fields
        if (!ase_json.contains("meta")) {
            Tcl_SetResult(interp, (char*)"Invalid Aseprite JSON: missing 'meta'", TCL_STATIC);
            return TCL_ERROR;
        }
        
        if (!ase_json["meta"].contains("image")) {
            Tcl_SetResult(interp, (char*)"Invalid Aseprite JSON: missing 'meta.image'", TCL_STATIC);
            return TCL_ERROR;
        }
        
        if (!ase_json["meta"].contains("size")) {
            Tcl_SetResult(interp, (char*)"Invalid Aseprite JSON: missing 'meta.size'", TCL_STATIC);
            return TCL_ERROR;
        }
        
        if (!ase_json.contains("frames")) {
            Tcl_SetResult(interp, (char*)"Invalid Aseprite JSON: missing 'frames'", TCL_STATIC);
            return TCL_ERROR;
        }
        
        std::string sprite_path = ase_json["meta"]["image"];
        
        std::string input_dir = input_path;
        size_t last_slash = input_dir.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            input_dir = input_dir.substr(0, last_slash + 1);
        } else {
            input_dir = "";
        }
        sprite_path = input_dir + sprite_path;
        
        json frames = ase_json["frames"];
        json output;
        
        int frame_count = 0;
        int total_fixtures = 0;
        int max_canvas_w = 0, max_canvas_h = 0;
        int max_content_w = 0, max_content_h = 0;
        
        for (auto& [frame_name, frame_data] : frames.items()) {
            // ERROR HANDLING: Check frame has required fields
            if (!frame_data.contains("frame")) {
                continue;  // Skip malformed frame
            }
            
            int x = frame_data["frame"]["x"];
            int y = frame_data["frame"]["y"];
            int w = frame_data["frame"]["w"];
            int h = frame_data["frame"]["h"];
            
            // EXTRACT DURATION (milliseconds)
            int duration_ms = 100;  // Default 100ms = 10fps
            if (frame_data.contains("duration")) {
                duration_ms = frame_data["duration"];
            }
            
            CollisionData coll = extract_collision(sprite_path, x, y, w, h,
                                                   threshold,
                                                   params.simplify_epsilon,
                                                   params.min_area);
            
            json fixtures = json::array();
            for (const auto& fix : coll.fixtures) {
                json vertices = json::array();
                for (const auto& v : fix.vertices) {
                    vertices.push_back({{"x", v.x}, {"y", v.y}});
                }
                fixtures.push_back({
                    {"vertices", vertices},
                    {"convex", fix.is_convex},
                    {"vertex_count", fix.vertices.size()}
                });
                total_fixtures++;
            }
            
            output[frame_name] = {
                {"frame_rect", {
                    {"x", x},
                    {"y", y},
                    {"w", w},
                    {"h", h}
                }},
                {"duration", duration_ms},  // ADD DURATION
                {"width", coll.frame_width},
                {"height", coll.frame_height},
                {"visual_bounds", {
                    {"x", coll.visual_bounds.content_x},
                    {"y", coll.visual_bounds.content_y},
                    {"w", coll.visual_bounds.content_width},
                    {"h", coll.visual_bounds.content_height}
                }},
                {"canvas_size", {
                    {"w", coll.visual_bounds.canvas_width},
                    {"h", coll.visual_bounds.canvas_height}
                }},
                {"fixtures", fixtures},
                {"fixture_count", coll.fixtures.size()}
            };
            
            max_canvas_w = std::max(max_canvas_w, coll.visual_bounds.canvas_width);
            max_canvas_h = std::max(max_canvas_h, coll.visual_bounds.canvas_height);
            max_content_w = std::max(max_content_w, coll.visual_bounds.content_width);
            max_content_h = std::max(max_content_h, coll.visual_bounds.content_height);
            
            frame_count++;
        }
        
        // ====================================================================
        // PARSE ANIMATIONS with TIMING
        // ====================================================================
        json animations = json::object();
        if (ase_json["meta"].contains("frameTags")) {
            for (auto& tag : ase_json["meta"]["frameTags"]) {
                std::string anim_name = tag["name"];
                int from = tag["from"];
                int to = tag["to"];
                
                // Build frame list
                json frame_list = json::array();
                int total_duration_ms = 0;
                
                for (int i = from; i <= to; i++) {
                    frame_list.push_back(i);
                    
                    // Sum durations for this animation
                    // Need to find frame by index - frames dict uses names
                    int frame_idx = 0;
                    for (auto& [fn, fd] : frames.items()) {
                        if (frame_idx == i) {
                            if (fd.contains("duration")) {
                                total_duration_ms += (int)fd["duration"];
                            } else {
                                total_duration_ms += 100;  // default
                            }
                            break;
                        }
                        frame_idx++;
                    }
                }
                
                int num_frames = to - from + 1;
                double avg_duration_ms = (double)total_duration_ms / num_frames;
                double fps = 1000.0 / avg_duration_ms;
                
                animations[anim_name] = {
                    {"frames", frame_list},
                    {"from", from},
                    {"to", to},
                    {"frame_count", num_frames},
                    {"fps", fps},  // Calculated average FPS
                    {"duration_ms", total_duration_ms}  // Total animation time
                };
                
                // Copy direction if present
                if (tag.contains("direction")) {
                    animations[anim_name]["direction"] = tag["direction"];
                }
            }
        }
        
        output["_metadata"] = {
            {"source", input_path},
            {"sprite_sheet", sprite_path},
            {"image", ase_json["meta"]["image"]},
            {"texture_width", ase_json["meta"]["size"]["w"]},
            {"texture_height", ase_json["meta"]["size"]["h"]},
            {"frame_count", frame_count},
            {"total_fixtures", total_fixtures},
            {"canonical_canvas", {
                {"w", max_canvas_w},
                {"h", max_canvas_h}
            }},
            {"canonical_content", {
                {"w", max_content_w},
                {"h", max_content_h}
            }},
            {"animations", animations},
            {"parameters", {
                {"alpha_threshold", threshold},
                {"simplify_epsilon", params.simplify_epsilon},
                {"min_area", params.min_area},
                {"max_vertices", params.max_vertices}
            }}
        };
        
        // Add app version if available
        if (ase_json["meta"].contains("app")) {
            output["_metadata"]["aseprite_app"] = ase_json["meta"]["app"];
        }
        if (ase_json["meta"].contains("version")) {
            output["_metadata"]["aseprite_version"] = ase_json["meta"]["version"];
        }
        
        std::string json_str = pretty ? output.dump(2) : output.dump();
        Tcl_SetObjResult(interp, Tcl_NewStringObj(json_str.c_str(), -1));
        
        return TCL_OK;
        
    } catch (const std::exception& e) {
        Tcl_SetResult(interp, (char*)e.what(), TCL_VOLATILE);
        return TCL_ERROR;
    }
}

// collision::xml_to_json
static int Tcl_XmlToJson(ClientData, Tcl_Interp* interp,
                         int objc, Tcl_Obj* const objv[]) {
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "xml_path ?-pretty bool?");
        return TCL_ERROR;
    }
    
    const char* xml_path = Tcl_GetString(objv[1]);
    bool pretty = false;
    
    for (int i = 2; i < objc; i += 2) {
        if (i + 1 >= objc) {
            Tcl_SetResult(interp, (char*)"Missing value for -pretty", TCL_STATIC);
            return TCL_ERROR;
        }
        
        const char* opt = Tcl_GetString(objv[i]);
        if (strcmp(opt, "-pretty") == 0) {
            int tmp;
            if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &tmp) != TCL_OK) {
                return TCL_ERROR;
            }
            pretty = (tmp != 0);
        }
    }
    
    try {
        json converted = xml_to_aseprite_json(xml_path);
        std::string json_str = pretty ? converted.dump(2) : converted.dump();
        
        Tcl_SetObjResult(interp, Tcl_NewStringObj(json_str.c_str(), -1));
        return TCL_OK;
        
    } catch (const std::exception& e) {
        Tcl_SetResult(interp, (char*)e.what(), TCL_VOLATILE);
        return TCL_ERROR;
    }
}

/// collision::process_xml_spritesheet
static int Tcl_ProcessXmlSpritesheet(ClientData, Tcl_Interp* interp,
                                     int objc, Tcl_Obj* const objv[]) {
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "xml_path ?options?");
        return TCL_ERROR;
    }
    
    const char* xml_path = Tcl_GetString(objv[1]);
    
    DecomposeParams params;
    unsigned char threshold = 128;
    bool pretty = false;
    
    if (!parse_collision_params(interp, objc, objv, 2, params, threshold)) {
        return TCL_ERROR;
    }
    
    for (int i = 2; i < objc; i += 2) {
        const char* opt = Tcl_GetString(objv[i]);
        if (strcmp(opt, "-pretty") == 0) {
            int tmp;
            Tcl_GetBooleanFromObj(interp, objv[i + 1], &tmp);
            pretty = (tmp != 0);
        }
    }
    
    try {
        // Convert XML to Aseprite JSON format
        json ase_json = xml_to_aseprite_json(xml_path);
        
        // ERROR HANDLING: Check required fields
        if (!ase_json.contains("meta")) {
            Tcl_SetResult(interp, (char*)"Invalid XML: missing metadata after conversion", TCL_STATIC);
            return TCL_ERROR;
        }
        
        if (!ase_json["meta"].contains("image")) {
            Tcl_SetResult(interp, (char*)"Invalid XML: missing image path", TCL_STATIC);
            return TCL_ERROR;
        }
        
        if (!ase_json["meta"].contains("size")) {
            Tcl_SetResult(interp, (char*)"Invalid XML: missing texture size", TCL_STATIC);
            return TCL_ERROR;
        }
        
        if (!ase_json.contains("frames")) {
            Tcl_SetResult(interp, (char*)"Invalid XML: missing frames", TCL_STATIC);
            return TCL_ERROR;
        }
        
        std::string sprite_path = ase_json["meta"]["image"];
        
        std::string xml_dir = xml_path;
        size_t last_slash = xml_dir.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            xml_dir = xml_dir.substr(0, last_slash + 1);
        } else {
            xml_dir = "";
        }
        sprite_path = xml_dir + sprite_path;
        
        json frames = ase_json["frames"];
        json output;
        
        int frame_count = 0;
        int total_fixtures = 0;
        int max_canvas_w = 0, max_canvas_h = 0;
        int max_content_w = 0, max_content_h = 0;
        
        for (auto& [frame_name, frame_data] : frames.items()) {
            // ERROR HANDLING: Skip malformed frames
            if (!frame_data.contains("frame")) {
                continue;
            }
            
            int x = frame_data["frame"]["x"];
            int y = frame_data["frame"]["y"];
            int w = frame_data["frame"]["w"];
            int h = frame_data["frame"]["h"];
            
            CollisionData coll = extract_collision(sprite_path, x, y, w, h,
                                                   threshold,
                                                   params.simplify_epsilon,
                                                   params.min_area);
            
            json fixtures = json::array();
            for (const auto& fix : coll.fixtures) {
                json vertices = json::array();
                for (const auto& v : fix.vertices) {
                    vertices.push_back({{"x", v.x}, {"y", v.y}});
                }
                fixtures.push_back({
                    {"vertices", vertices},
                    {"convex", fix.is_convex},
                    {"vertex_count", fix.vertices.size()}
                });
                total_fixtures++;
            }
            
            output[frame_name] = {
                // ADD FRAME RECTANGLE (for texture UVs)
                {"frame_rect", {
                    {"x", x},
                    {"y", y},
                    {"w", w},
                    {"h", h}
                }},
                {"width", coll.frame_width},
                {"height", coll.frame_height},
                {"visual_bounds", {
                    {"x", coll.visual_bounds.content_x},
                    {"y", coll.visual_bounds.content_y},
                    {"w", coll.visual_bounds.content_width},
                    {"h", coll.visual_bounds.content_height}
                }},
                {"canvas_size", {
                    {"w", coll.visual_bounds.canvas_width},
                    {"h", coll.visual_bounds.canvas_height}
                }},
                {"fixtures", fixtures},
                {"fixture_count", coll.fixtures.size()}
            };
            
            max_canvas_w = std::max(max_canvas_w, coll.visual_bounds.canvas_width);
            max_canvas_h = std::max(max_canvas_h, coll.visual_bounds.canvas_height);
            max_content_w = std::max(max_content_w, coll.visual_bounds.content_width);
            max_content_h = std::max(max_content_h, coll.visual_bounds.content_height);
            
            frame_count++;
        }
        
        output["_metadata"] = {
            {"source", xml_path},
            {"source_format", "xml_atlas"},
            {"sprite_sheet", sprite_path},
            
            // ADD TEXTURE METADATA (matches Aseprite output)
            {"image", ase_json["meta"]["image"]},
            {"texture_width", ase_json["meta"]["size"]["w"]},
            {"texture_height", ase_json["meta"]["size"]["h"]},
            
            {"frame_count", frame_count},
            {"total_fixtures", total_fixtures},
            {"canonical_canvas", {
                {"w", max_canvas_w},
                {"h", max_canvas_h}
            }},
            {"canonical_content", {
                {"w", max_content_w},
                {"h", max_content_h}
            }},
            {"parameters", {
                {"alpha_threshold", threshold},
                {"simplify_epsilon", params.simplify_epsilon},
                {"min_area", params.min_area},
                {"max_vertices", params.max_vertices}
            }}
        };
        
        // Note: XML atlases don't have animation data, so no animations block
        // If you want to support animations, they'd need to be in a separate file
        
        std::string json_str = pretty ? output.dump(2) : output.dump();
        Tcl_SetObjResult(interp, Tcl_NewStringObj(json_str.c_str(), -1));
        
        return TCL_OK;
        
    } catch (const std::exception& e) {
        Tcl_SetResult(interp, (char*)e.what(), TCL_VOLATILE);
        return TCL_ERROR;
    }
}

// spritesheet::process - auto-detect format
static int Tcl_ProcessAuto(ClientData cd, Tcl_Interp* interp,
                           int objc, Tcl_Obj* const objv[]) {
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "file_path ?options?");
        return TCL_ERROR;
    }
    
    const char* path = Tcl_GetString(objv[1]);
    
    // Simple extension check
    const char* ext = strrchr(path, '.');
    if (!ext) {
        Tcl_SetResult(interp, (char*)"No file extension found", TCL_STATIC);
        return TCL_ERROR;
    }
    
    if (strcmp(ext, ".json") == 0) {
        return Tcl_ProcessSpritesheetJson(cd, interp, objc, objv);
    } else if (strcmp(ext, ".xml") == 0) {
        return Tcl_ProcessXmlSpritesheet(cd, interp, objc, objv);
    } else {
        Tcl_SetResult(interp, (char*)"Unknown file format (use .json or .xml)", TCL_STATIC);
        return TCL_ERROR;
    }
}

extern "C" int Spritesheet_Init(Tcl_Interp* interp) {
    if (Tcl_InitStubs(interp, "8.6-", 0) == NULL) {
        return TCL_ERROR;
    }
    
    if (Tcl_PkgProvide(interp, "spritesheet", "1.0") != TCL_OK) {
        return TCL_ERROR;
    }
    
    // Format-specific commands
    Tcl_CreateObjCommand(interp, "spritesheet::process_aseprite",
                        Tcl_ProcessSpritesheetJson, NULL, NULL);
    
    Tcl_CreateObjCommand(interp, "spritesheet::process_xml",
                        Tcl_ProcessXmlSpritesheet, NULL, NULL);
    
    Tcl_CreateObjCommand(interp, "spritesheet::extract_collision",
                        Tcl_ExtractCollision, NULL, NULL);
    
    // Convenience: Auto-detect format
    Tcl_CreateObjCommand(interp, "spritesheet::process",
                        Tcl_ProcessAuto, NULL, NULL);

    
    return TCL_OK;
}