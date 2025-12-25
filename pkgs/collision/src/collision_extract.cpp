#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "collision_extract.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstring>

namespace collision {

// ============================================================================
// Image Implementation
// ============================================================================

Image::~Image() {
    if (data) {
        stbi_image_free(data);
        data = nullptr;
    }
}

Image::Image(Image&& other) noexcept
    : data(other.data), width(other.width), height(other.height), channels(other.channels) {
    other.data = nullptr;
    other.width = 0;
    other.height = 0;
    other.channels = 0;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        if (data) stbi_image_free(data);
        data = other.data;
        width = other.width;
        height = other.height;
        channels = other.channels;
        other.data = nullptr;
        other.width = 0;
        other.height = 0;
        other.channels = 0;
    }
    return *this;
}

Image load_image(const std::string& path) {
    Image img;
    img.data = stbi_load(path.c_str(), &img.width, &img.height, &img.channels, 0);
    if (!img.data) {
        throw std::runtime_error(std::string("Failed to load image: ") + path);
    }
    return img;
}

// ============================================================================
// Basic Image Operations
// ============================================================================

Image extract_frame(const Image& sheet, int x, int y, int width, int height) {
    Image frame;
    frame.width = width;
    frame.height = height;
    frame.channels = sheet.channels;
    frame.data = (unsigned char*)malloc(width * height * sheet.channels);
    
    if (!frame.data) {
        throw std::runtime_error("Failed to allocate frame memory");
    }
    
    for (int py = 0; py < height; py++) {
        for (int px = 0; px < width; px++) {
            for (int c = 0; c < sheet.channels; c++) {
                int src_x = x + px;
                int src_y = y + py;
                if (src_x >= 0 && src_x < sheet.width && src_y >= 0 && src_y < sheet.height) {
                    int src_idx = (src_y * sheet.width + src_x) * sheet.channels + c;
                    int dst_idx = (py * width + px) * sheet.channels + c;
                    frame.data[dst_idx] = sheet.data[src_idx];
                } else {
                    int dst_idx = (py * width + px) * sheet.channels + c;
                    frame.data[dst_idx] = 0;
                }
            }
        }
    }
    
    return frame;
}

std::vector<bool> create_alpha_mask(const Image& frame, unsigned char threshold) {
    std::vector<bool> mask(frame.width * frame.height, false);
    
    for (int y = 0; y < frame.height; y++) {
        for (int x = 0; x < frame.width; x++) {
            bool opaque = false;
            if (frame.channels >= 4) {
                opaque = frame.get(x, y, 3) > threshold;
            } else {
                unsigned char lum = frame.get(x, y, 0);
                if (frame.channels >= 3) {
                    lum = (frame.get(x, y, 0) + frame.get(x, y, 1) + frame.get(x, y, 2)) / 3;
                }
                opaque = lum > threshold;
            }
            mask[y * frame.width + x] = opaque;
        }
    }
    
    return mask;
}

// ============================================================================
// Contour Tracing
// ============================================================================

std::vector<Point> trace_contour(const std::vector<bool>& mask, int width, int height) {
    std::vector<Point> contour;
    
    int start_x = -1, start_y = -1;
    for (int y = 0; y < height && start_x < 0; y++) {
        for (int x = 0; x < width; x++) {
            if (mask[y * width + x]) {
                start_x = x;
                start_y = y;
                break;
            }
        }
    }
    
    if (start_x < 0) return contour;
    
    const int dx[] = {1, 1, 0, -1, -1, -1, 0, 1};
    const int dy[] = {0, 1, 1, 1, 0, -1, -1, -1};
    
    int x = start_x, y = start_y;
    int dir = 0;
    
    std::vector<bool> visited(width * height, false);
    
    do {
        contour.push_back(Point(x, y));
        visited[y * width + x] = true;
        
        bool found = false;
        for (int i = 0; i < 8; i++) {
            int check_dir = (dir + i) % 8;
            int nx = x + dx[check_dir];
            int ny = y + dy[check_dir];
            
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                if (mask[ny * width + nx]) {
                    x = nx;
                    y = ny;
                    dir = (check_dir + 6) % 8;
                    found = true;
                    break;
                }
            }
        }
        
        if (!found) break;
        if (contour.size() > width * height) break;
        
    } while (x != start_x || y != start_y || contour.size() < 4);
    
    return contour;
}

// ============================================================================
// Douglas-Peucker Simplification
// ============================================================================

static float point_line_distance(const Point& p, const Point& a, const Point& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float norm = std::sqrt(dx*dx + dy*dy);
    
    if (norm < 1e-6f) {
        float px = p.x - a.x;
        float py = p.y - a.y;
        return std::sqrt(px*px + py*py);
    }
    
    return std::abs((p.x - a.x) * dy - (p.y - a.y) * dx) / norm;
}

static void douglas_peucker_impl(const std::vector<Point>& points,
                                 int start, int end,
                                 float epsilon,
                                 std::vector<bool>& keep) {
    if (end <= start + 1) return;
    
    float max_dist = 0;
    int max_idx = start;
    
    for (int i = start + 1; i < end; i++) {
        float dist = point_line_distance(points[i], points[start], points[end]);
        if (dist > max_dist) {
            max_dist = dist;
            max_idx = i;
        }
    }
    
    if (max_dist > epsilon) {
        keep[max_idx] = true;
        douglas_peucker_impl(points, start, max_idx, epsilon, keep);
        douglas_peucker_impl(points, max_idx, end, epsilon, keep);
    }
}

std::vector<Point> simplify_polygon(const std::vector<Point>& points, float epsilon) {
    if (points.size() < 3) return points;
    
    std::vector<bool> keep(points.size(), false);
    keep[0] = true;
    keep[points.size() - 1] = true;
    
    douglas_peucker_impl(points, 0, points.size() - 1, epsilon, keep);
    
    std::vector<Point> simplified;
    for (size_t i = 0; i < points.size(); i++) {
        if (keep[i]) {
            simplified.push_back(points[i]);
        }
    }
    
    return simplified;
}

// ============================================================================
// Bayazit Convex Decomposition
// ============================================================================

static float cross(const Point& p1, const Point& p2, const Point& p3) {
    return (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
}

static bool is_reflex(const std::vector<Point>& poly, int i) {
    int n = poly.size();
    const Point& p1 = poly[(i - 1 + n) % n];
    const Point& p2 = poly[i];
    const Point& p3 = poly[(i + 1) % n];
    return cross(p1, p2, p3) < 0;
}

static bool segment_in_polygon(const std::vector<Point>& poly,
                               const Point& p1, const Point& p2) {
    Point mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
    int crossings = 0;
    int n = poly.size();
    
    for (int i = 0; i < n; i++) {
        const Point& a = poly[i];
        const Point& b = poly[(i + 1) % n];
        
        if (((a.y <= mid.y) && (b.y > mid.y)) || ((a.y > mid.y) && (b.y <= mid.y))) {
            float x_intersect = a.x + (mid.y - a.y) / (b.y - a.y) * (b.x - a.x);
            if (mid.x < x_intersect) {
                crossings++;
            }
        }
    }
    
    return (crossings % 2) == 1;
}

static int find_best_diagonal(const std::vector<Point>& poly, int reflex_idx) {
    int n = poly.size();
    const Point& reflex = poly[reflex_idx];
    
    int best = -1;
    float best_dist = std::numeric_limits<float>::max();
    
    for (int i = 0; i < n; i++) {
        if (i == reflex_idx || i == (reflex_idx - 1 + n) % n || i == (reflex_idx + 1) % n) {
            continue;
        }
        
        const Point& candidate = poly[i];
        
        if (!segment_in_polygon(poly, reflex, candidate)) {
            continue;
        }
        
        bool intersects = false;
        for (int j = 0; j < n && !intersects; j++) {
            const Point& e1 = poly[j];
            const Point& e2 = poly[(j + 1) % n];
            
            if (j == reflex_idx || j == i || (j + 1) % n == reflex_idx || (j + 1) % n == i) {
                continue;
            }
            
            float d1 = cross(e1, e2, reflex);
            float d2 = cross(e1, e2, candidate);
            float d3 = cross(reflex, candidate, e1);
            float d4 = cross(reflex, candidate, e2);
            
            if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
                ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
                intersects = true;
            }
        }
        
        if (!intersects) {
            float dx = candidate.x - reflex.x;
            float dy = candidate.y - reflex.y;
            float dist = dx*dx + dy*dy;
            
            if (dist < best_dist) {
                best_dist = dist;
                best = i;
            }
        }
    }
    
    return best;
}

static bool is_convex(const std::vector<Point>& poly) {
    if (poly.size() < 3) return false;
    
    bool sign = false;
    int n = poly.size();
    
    for (int i = 0; i < n; i++) {
        const Point& p1 = poly[i];
        const Point& p2 = poly[(i + 1) % n];
        const Point& p3 = poly[(i + 2) % n];
        
        float c = cross(p1, p2, p3);
        
        if (i == 0) {
            sign = c > 0;
        } else if ((c > 0) != sign) {
            return false;
        }
    }
    
    return true;
}

static void bayazit_decompose(const std::vector<Point>& poly,
                              std::vector<Polygon>& output,
                              const DecomposeParams& params) {
    int n = poly.size();
    
    if (n < 3) return;
    
    bool all_convex = true;
    for (int i = 0; i < n; i++) {
        if (is_reflex(poly, i)) {
            all_convex = false;
            break;
        }
    }
    
    if (all_convex) {
        Polygon result;
        result.vertices = poly;
        result.is_convex = true;
        output.push_back(result);
        return;
    }
    
    for (int i = 0; i < n; i++) {
        if (is_reflex(poly, i)) {
            int split_idx = find_best_diagonal(poly, i);
            
            if (split_idx >= 0) {
                std::vector<Point> poly1, poly2;
                
                int idx = split_idx;
                while (idx != i) {
                    poly1.push_back(poly[idx]);
                    idx = (idx + 1) % n;
                }
                poly1.push_back(poly[i]);
                
                idx = i;
                while (idx != split_idx) {
                    poly2.push_back(poly[idx]);
                    idx = (idx + 1) % n;
                }
                poly2.push_back(poly[split_idx]);
                
                bayazit_decompose(poly1, output, params);
                bayazit_decompose(poly2, output, params);
                return;
            }
        }
    }
    
    Polygon result;
    result.vertices = poly;
    result.is_convex = false;
    output.push_back(result);
}

static std::vector<Point> reduce_vertices(const std::vector<Point>& poly, int max_vertices) {
    if (poly.size() <= max_vertices) return poly;
    
    float epsilon = 1.0f;
    std::vector<Point> simplified = poly;
    
    while (simplified.size() > max_vertices && epsilon < 100.0f) {
        simplified = simplify_polygon(poly, epsilon);
        epsilon *= 1.5f;
    }
    
    return simplified;
}

std::vector<Polygon> decompose_convex(const std::vector<Point>& polygon,
                                     const DecomposeParams& params) {
    std::vector<Polygon> result;
    
    if (polygon.size() < 3) return result;
    
    std::vector<Point> simplified = simplify_polygon(polygon, params.simplify_epsilon);
    
    if (simplified.size() < 3) return result;
    
    bayazit_decompose(simplified, result, params);
    
    std::vector<Polygon> final_result;
    for (auto& poly : result) {
        if (poly.vertices.size() > params.max_vertices) {
            poly.vertices = reduce_vertices(poly.vertices, params.max_vertices);
        }
        
        if (polygon_area(poly.vertices) >= params.min_area) {
            final_result.push_back(poly);
        }
    }
    
    return final_result;
}

// ============================================================================
// Visual Bounds
// ============================================================================

VisualBounds compute_visual_bounds(const Image& frame, unsigned char alpha_threshold) {
    VisualBounds bounds;
    bounds.canvas_width = frame.width;
    bounds.canvas_height = frame.height;
    
    int min_x = frame.width;
    int min_y = frame.height;
    int max_x = 0;
    int max_y = 0;
    
    bool found_opaque = false;
    
    for (int y = 0; y < frame.height; y++) {
        for (int x = 0; x < frame.width; x++) {
            bool opaque = false;
            
            if (frame.channels >= 4) {
                opaque = frame.get(x, y, 3) > alpha_threshold;
            } else {
                unsigned char lum = frame.get(x, y, 0);
                if (frame.channels >= 3) {
                    lum = (frame.get(x, y, 0) + frame.get(x, y, 1) + frame.get(x, y, 2)) / 3;
                }
                opaque = lum > alpha_threshold;
            }
            
            if (opaque) {
                found_opaque = true;
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }
    
    if (found_opaque) {
        bounds.content_x = min_x;
        bounds.content_y = min_y;
        bounds.content_width = max_x - min_x + 1;
        bounds.content_height = max_y - min_y + 1;
    } else {
        bounds.content_x = 0;
        bounds.content_y = 0;
        bounds.content_width = 0;
        bounds.content_height = 0;
    }
    
    return bounds;
}

// ============================================================================
// Helpers
// ============================================================================

float polygon_area(const std::vector<Point>& poly) {
    float area = 0;
    int n = poly.size();
    for (int i = 0; i < n; i++) {
        const Point& p1 = poly[i];
        const Point& p2 = poly[(i + 1) % n];
        area += p1.x * p2.y - p2.x * p1.y;
    }
    return std::abs(area) * 0.5f;
}

std::vector<Polygon> filter_polygons(const std::vector<Polygon>& polygons,
                                    float min_area, int min_vertices) {
    std::vector<Polygon> filtered;
    
    for (const auto& poly : polygons) {
        if (poly.vertices.size() < min_vertices) continue;
        if (polygon_area(poly.vertices) < min_area) continue;
        filtered.push_back(poly);
    }
    
    return filtered;
}

// ============================================================================
// High-level Extraction
// ============================================================================

CollisionData extract_collision(const std::string& sprite_path,
                               int frame_x, int frame_y,
                               int frame_width, int frame_height,
                               unsigned char alpha_threshold,
                               float simplify_epsilon,
                               float min_area) {
    CollisionData result;
    result.frame_width = frame_width;
    result.frame_height = frame_height;
    
    try {
        Image sheet = load_image(sprite_path);
        Image frame = extract_frame(sheet, frame_x, frame_y, frame_width, frame_height);
        
        result.visual_bounds = compute_visual_bounds(frame, alpha_threshold);
        
        std::vector<bool> mask = create_alpha_mask(frame, alpha_threshold);
        std::vector<Point> contour = trace_contour(mask, frame.width, frame.height);
        
        if (!contour.empty()) {
            DecomposeParams params;
            params.simplify_epsilon = simplify_epsilon;
            params.min_area = min_area;
            result.fixtures = decompose_convex(contour, params);
        }
        
    } catch (const std::exception& e) {
        // Return empty on error
    }
    
    return result;
}

} // namespace collision