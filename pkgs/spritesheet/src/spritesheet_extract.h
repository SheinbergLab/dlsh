#pragma once
#include <vector>
#include <string>

namespace spritesheet {

// Simple image wrapper for stb_image
struct Image {
    unsigned char* data;
    int width, height, channels;
    
    Image() : data(nullptr), width(0), height(0), channels(0) {}
    ~Image();
    
    // No copying (uses raw pointer)
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    
    // Move semantics
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    
    // Pixel access (x, y, channel)
    unsigned char get(int x, int y, int c) const {
        if (x < 0 || x >= width || y < 0 || y >= height || c >= channels) return 0;
        return data[(y * width + x) * channels + c];
    }
    
    int spectrum() const { return channels; }
};

struct ImageSize {
    int width;
    int height;
};

ImageSize get_image_size(const std::string& path);

// Load image from file
Image load_image(const std::string& path);

// Shape detection structures
struct Point {
    double x, y;
     Point(double x_ = 0, double y_ = 0) : x(x_), y(y_) {}
};

struct CircleCandidate {
    Point center;
    double radius;
    double fit_error;  // Normalized variance from perfect circle
};

struct Box {
    Point min, max;
    double width, height;
    double aspect_ratio;
};

// Shape detection functions
CircleCandidate fit_circle(const std::vector<Point>& points);
bool is_roughly_circular(const std::vector<Point>& points, double threshold = 0.1);
Box fit_bounding_box(const std::vector<Point>& points);
bool is_roughly_rectangular(const std::vector<Point>& points, double corner_tolerance = 0.15);


struct Polygon {
    std::vector<Point> vertices;
    bool is_convex;
};

struct VisualBounds {
    int content_x, content_y;
    int content_width, content_height;
    int canvas_width, canvas_height;
};

struct CollisionData {
    std::vector<Polygon> fixtures;
    int frame_width, frame_height;
    VisualBounds visual_bounds;
};

struct DecomposeParams {
    float simplify_epsilon = 2.0f;
    float min_area = 10.0f;
    int max_vertices = 8;
    float collinear_threshold = 0.1f;
};

// Frame extraction
Image extract_frame(const Image& sheet, int x, int y, int width, int height);

// Alpha mask creation (returns flat vector, access as mask[y * width + x])
std::vector<bool> create_alpha_mask(const Image& frame, unsigned char threshold = 128);

// Contour tracing (takes flat mask)
std::vector<Point> trace_contour(const std::vector<bool>& mask, int width, int height);

// Polygon simplification
std::vector<Point> simplify_polygon(const std::vector<Point>& points, float epsilon = 2.0);

// Convex decomposition
std::vector<Polygon> decompose_convex(const std::vector<Point>& polygon,
                                     const DecomposeParams& params = DecomposeParams());

// Visual bounds computation
VisualBounds compute_visual_bounds(const Image& frame, unsigned char alpha_threshold = 128);

// Polygon filtering
std::vector<Polygon> filter_polygons(const std::vector<Polygon>& polygons,
                                    float min_area = 10.0, int min_vertices = 3);

// High-level extraction
CollisionData extract_collision(const std::string& sprite_path,
                               int frame_x, int frame_y,
                               int frame_width, int frame_height,
                               unsigned char alpha_threshold = 128,
                               float simplify_epsilon = 2.0,
                               float min_area = 10.0);

// Helper for polygon area
float polygon_area(const std::vector<Point>& poly);

} // namespace collision