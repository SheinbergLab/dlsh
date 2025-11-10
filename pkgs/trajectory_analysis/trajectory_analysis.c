/*************************************************************************
 *
 * NAME
 *      trajectory_analysis.c
 *
 * PURPOSE
 *      Tcl module for analyzing 2D trajectory data using KDE to identify
 *      collision hotspots in gravity-based ball falls onto planks
 *      Enhanced with heat map comparison capabilities
 *
 * AUTHOR
 *      Generated for dlsh integration
 *
 *************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <tcl.h>

#include <df.h>
#include <dynio.h>
#include <dfana.h>
#include <tcl_dl.h>

#include "dfu_helpers.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Module version
#define TRAJECTORY_VERSION "1.2"

// Structure for 2D points
typedef struct {
    double x, y;
} Point2D;

// Structure for trajectory data
typedef struct {
    Point2D *points;
    int count;
    int capacity;
} Trajectory;

// Structure for rotated rectangle (plank/obstacle)
typedef struct {
    double center_x, center_y;  // Center position
    double width, height;       // Dimensions
    double rotation;            // Rotation angle in radians
} RotatedRect;

// Enhanced trajectory structure with directional information
typedef struct {
    Point2D *points;
    double *directions;      // Direction at each point
    double *dir_variances;   // Local directional variance (not used currently)
    int count;
    int capacity;
} EnhancedTrajectory;

// Structure for KDE grid
typedef struct {
    double *values;
    int width, height;
    double x_min, x_max, y_min, y_max;
    double cell_width, cell_height;
} KDEGrid;

// Structure for comparative analysis results
typedef struct {
    KDEGrid *grid_set1;       // Heat map for first trajectory set
    KDEGrid *grid_set2;       // Heat map for second trajectory set
    KDEGrid *difference_grid; // Difference (set1 - set2)
    KDEGrid *ratio_grid;      // Ratio (set1 / set2)
    double correlation;       // Spatial correlation coefficient
    double mean_difference;   // Mean absolute difference
    double max_difference;    // Maximum absolute difference
    int significant_cells;    // Number of significantly different cells
} ComparisonResult;

// Function prototypes
static int TrajectoryAnalyzeCmd(ClientData clientData, Tcl_Interp *interp, 
                               int objc, Tcl_Obj *const objv[]);
static int TrajectoryCompareCmd(ClientData clientData, Tcl_Interp *interp,
                               int objc, Tcl_Obj *const objv[]);

// Helper functions
static int extractTrajectoriesFromDynList(Tcl_Interp *interp, DYN_LIST *dl, 
                                         Trajectory **trajectories, int *count);
static int extractEnhancedTrajectoriesFromDynList(Tcl_Interp *interp, DYN_LIST *dl,
                                                  EnhancedTrajectory **trajectories, int *count);
static int computeDirectionalInfo(EnhancedTrajectory *traj);
static double computeLocalDirectionalVariance(EnhancedTrajectory *trajectories,
                                              int traj_count, double x, double y,
                                              double radius);
static KDEGrid* computeUncertaintyMap(EnhancedTrajectory *trajectories, int traj_count,
                                     int grid_width, int grid_height,
                                     double x_min, double x_max, double y_min, double y_max,
                                     double variance_radius);
static KDEGrid* computeVarianceWeightedKDE(EnhancedTrajectory *trajectories, int traj_count,
                                          Point2D *points, int point_count,
                                          int grid_width, int grid_height,
                                          double bandwidth_x, double bandwidth_y,
                                          double x_min, double x_max, double y_min, double y_max,
                                          double variance_radius, double variance_weight);
static int computeTrajectoryTurns(Trajectory *traj, Point2D **turns, int *turn_count,
                                 double angle_threshold);
static KDEGrid* computeKDE2D(Point2D *points, int point_count, 
                            int grid_width, int grid_height,
                            double bandwidth_x, double bandwidth_y,
                            double x_min, double x_max, double y_min, double y_max);
static int findKDEPeaks(KDEGrid *grid, Point2D **peaks, double **peak_values, 
                       int *peak_count, double threshold);
static double gaussian2D(double x, double y, double sigma_x, double sigma_y);
static void freeTrajectories(Trajectory *trajectories, int count);
static void freeKDEGrid(KDEGrid *grid);

static DYN_GROUP* createResultGroup(Point2D *peaks, 
                                   double *peak_values, int peak_count,
                                   KDEGrid *grid);

static void applySaturationToGrid(KDEGrid *grid, double gamma, const char *saturation_mode);
static void applyPowerLawSaturation(KDEGrid *grid, double gamma);
static void applyLogSaturation(KDEGrid *grid, double scale);
static void applySigmoidSaturation(KDEGrid *grid, double midpoint_percentile, double steepness);
static double computePercentile(double *values, int count, double percentile);

// For object based density
static int ObjectDensityCmd(ClientData clientData, Tcl_Interp *interp,
                           int objc, Tcl_Obj *const objv[]);
static KDEGrid* computeObjectDensity(RotatedRect *objects, int object_count,
                                    int grid_width, int grid_height,
                                    double x_min, double x_max,
                                    double y_min, double y_max,
                                    double bandwidth);
static int pointInRotatedRect(double px, double py, RotatedRect *rect);
static void computeObjectBoundingBox(RotatedRect *objects, int count,
                                    double *x_min, double *x_max,
                                    double *y_min, double *y_max);

static int resampleTrajectoryUniform(Trajectory *traj, double spacing,
                                    Point2D **resampled_points, int *resampled_count);

// comparison functions
static KDEGrid* allocateKDEGrid(int width, int height, 
                               double x_min, double x_max, 
                               double y_min, double y_max);
static KDEGrid* copyKDEGrid(KDEGrid *source);
static void normalizeKDEGrid(KDEGrid *grid);
static KDEGrid* computeKDEDifference(KDEGrid *grid1, KDEGrid *grid2);
static KDEGrid* computeKDERatio(KDEGrid *grid1, KDEGrid *grid2, double epsilon);
static double computeSpatialCorrelation(KDEGrid *grid1, KDEGrid *grid2);
static ComparisonResult* compareKDEGrids(KDEGrid *grid1, KDEGrid *grid2,
                                        const char *comparison_mode,
                                        double significance_threshold);
static DYN_GROUP* createComparisonResultGroup(ComparisonResult *comp_result,
                                             const char *comparison_mode,
                                             const char *analysis_type);
static void freeComparisonResult(ComparisonResult *result);
static void computeBoundingBox(Point2D *points, int count,
                              double *x_min, double *x_max,
                              double *y_min, double *y_max);
static void mergeBoundingBoxes(double x1_min, double x1_max, double y1_min, double y1_max,
                              double x2_min, double x2_max, double y2_min, double y2_max,
                              double *x_min, double *x_max, double *y_min, double *y_max);

/*
 * Gaussian kernel for 2D KDE
 */
static double gaussian2D(double x, double y, double sigma_x, double sigma_y) {
    double term1 = (x * x) / (2.0 * sigma_x * sigma_x);
    double term2 = (y * y) / (2.0 * sigma_y * sigma_y);
    double norm = 1.0 / (2.0 * M_PI * sigma_x * sigma_y);
    return norm * exp(-(term1 + term2));
}


static double computePercentile(double *values, int count, double percentile) {
    if (count == 0) return 0.0;
    
    // Create sorted copy
    double *sorted = malloc(count * sizeof(double));
    if (!sorted) return 0.0;
    
    memcpy(sorted, values, count * sizeof(double));
    
    // Simple insertion sort (could use qsort for large arrays)
    for (int i = 1; i < count; i++) {
        double key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
    
    int index = (int)(percentile * count);
    if (index >= count) index = count - 1;
    if (index < 0) index = 0;
    
    double result = sorted[index];
    free(sorted);
    
    return result;
}

/*
 * Apply power law saturation to grid
 * gamma < 1.0 compresses high values, revealing alternative paths
 * gamma = 1.0 is linear (no change)
 * gamma > 1.0 enhances high values
 */
static void applyPowerLawSaturation(KDEGrid *grid, double gamma) {
    if (gamma == 1.0) return;  // No transformation needed
    
    int total_cells = grid->width * grid->height;
    
    // Find max value for normalization
    double max_val = 0.0;
    for (int i = 0; i < total_cells; i++) {
        if (grid->values[i] > max_val) {
            max_val = grid->values[i];
        }
    }
    
    if (max_val == 0.0) return;  // Empty grid
    
    // Apply power law transformation
    for (int i = 0; i < total_cells; i++) {
        double normalized = grid->values[i] / max_val;
        grid->values[i] = pow(normalized, gamma) * max_val;
    }
}

/*
 * Apply logarithmic saturation to grid
 * Naturally compresses high values while preserving low values
 */
static void applyLogSaturation(KDEGrid *grid, double scale) {
    int total_cells = grid->width * grid->height;
    
    // Find max value to determine appropriate scaling
    double max_val = 0.0;
    for (int i = 0; i < total_cells; i++) {
        if (grid->values[i] > max_val) {
            max_val = grid->values[i];
        }
    }
    
    if (max_val == 0.0) return;
    
    // Apply log transformation: log(1 + x/scale)
    // Scale controls compression strength
    for (int i = 0; i < total_cells; i++) {
        grid->values[i] = scale * log(1.0 + grid->values[i] / scale);
    }
}

/*
 * Apply sigmoid saturation to grid
 * Creates smooth S-curve compression with adjustable midpoint and steepness
 */
static void applySigmoidSaturation(KDEGrid *grid, double midpoint_percentile, 
                                   double steepness) {
  int total_cells = grid->width * grid->height;
  
  // Find max value
  double max_val = 0.0;
  for (int i = 0; i < total_cells; i++) {
    if (grid->values[i] > max_val) {
      max_val = grid->values[i];
    }
  }
  
  if (max_val == 0.0) return;
  
  // Find midpoint for adaptive scaling
  double midpoint = computePercentile(grid->values, total_cells, midpoint_percentile);
  if (midpoint == 0.0) midpoint = max_val * 0.18;
  
  // Scale factor for compression
  double scale = steepness / midpoint;
  
  // Apply Reinhard tone mapping (HDR-style compression)
  for (int i = 0; i < total_cells; i++) {
    double x = grid->values[i];
    double scaled = x * scale;
    
    // Reinhard: compresses high values smoothly
    double compressed = scaled / (1.0 + scaled);
    
    // Rescale to preserve magnitude
    grid->values[i] = compressed * max_val * (1.0 + scale);
  }
}

/*
 * Main saturation dispatcher function
 */
static void applySaturationToGrid(KDEGrid *grid, double gamma, const char *saturation_mode) {
    if (!grid || !saturation_mode) return;
    
    if (strcmp(saturation_mode, "power") == 0) {
        applyPowerLawSaturation(grid, gamma);
    } else if (strcmp(saturation_mode, "log") == 0) {
        // For log mode, gamma acts as scale parameter
        applyLogSaturation(grid, gamma);
    } else if (strcmp(saturation_mode, "sigmoid") == 0) {
        // For sigmoid mode, gamma acts as steepness parameter
        // Use 50th percentile (median) as midpoint
        applySigmoidSaturation(grid, 0.5, gamma);
    }
    // "none" or unrecognized modes do nothing
}



/*
 * Helper function to normalize angle to [-PI, PI]
 */
static double normalizeAngle(double angle) {
    while (angle > M_PI) angle -= 2.0 * M_PI;
    while (angle < -M_PI) angle += 2.0 * M_PI;
    return angle;
}

/*
 * Compute directional information for a trajectory
 */
static int computeDirectionalInfo(EnhancedTrajectory *traj) {
    if (traj->count < 2) return 0;
    
    // Allocate arrays for directions
    traj->directions = malloc(traj->count * sizeof(double));
    if (!traj->directions) return 0;
    
    // Compute directions between consecutive points
    for (int i = 0; i < traj->count - 1; i++) {
        double dx = traj->points[i+1].x - traj->points[i].x;
        double dy = traj->points[i+1].y - traj->points[i].y;
        traj->directions[i] = atan2(dy, dx);
    }
    
    // Set last direction same as previous
    if (traj->count > 1) {
        traj->directions[traj->count-1] = traj->directions[traj->count-2];
    }
    
    return 1;
}

/*
 * Compute local directional variance across multiple trajectories
 * Uses circular variance to measure directional uncertainty
 */
static double computeLocalDirectionalVariance(EnhancedTrajectory *trajectories,
                                              int traj_count,
                                              double x, double y,
                                              double radius) {
    double *directions = malloc(1000 * sizeof(double)); // Temp storage
    int dir_count = 0;
    
    if (!directions) return 0.0;
    
    // Collect directions from all trajectories within radius
    for (int t = 0; t < traj_count; t++) {
        for (int i = 0; i < trajectories[t].count; i++) {
            double dx = trajectories[t].points[i].x - x;
            double dy = trajectories[t].points[i].y - y;
            double dist = sqrt(dx*dx + dy*dy);
            
            if (dist <= radius && dir_count < 1000) {
                directions[dir_count++] = trajectories[t].directions[i];
            }
        }
    }
    
    if (dir_count < 2) {
        free(directions);
        return 0.0;
    }
    
    // Compute circular variance of directions
    // Mean direction vector
    double sin_sum = 0.0, cos_sum = 0.0;
    for (int i = 0; i < dir_count; i++) {
        sin_sum += sin(directions[i]);
        cos_sum += cos(directions[i]);
    }
    
    double R = sqrt(sin_sum*sin_sum + cos_sum*cos_sum) / dir_count;
    
    // Circular variance (0 = no variance, 1 = maximum variance)
    double circular_variance = 1.0 - R;
    
    free(directions);
    return circular_variance;
}

/*
 * Pure uncertainty map computation
 */
static KDEGrid* computeUncertaintyMap(EnhancedTrajectory *trajectories,
                                     int traj_count,
                                     int grid_width, int grid_height,
                                     double x_min, double x_max,
                                     double y_min, double y_max,
                                     double variance_radius) {
    KDEGrid *grid = malloc(sizeof(KDEGrid));
    if (!grid) return NULL;
    
    grid->width = grid_width;
    grid->height = grid_height;
    grid->x_min = x_min;
    grid->x_max = x_max;
    grid->y_min = y_min;
    grid->y_max = y_max;
    grid->cell_width = (x_max - x_min) / grid_width;
    grid->cell_height = (y_max - y_min) / grid_height;
    
    grid->values = calloc(grid_width * grid_height, sizeof(double));
    if (!grid->values) {
        free(grid);
        return NULL;
    }
    
    // Compute pure uncertainty map
    for (int gy = 0; gy < grid_height; gy++) {
        for (int gx = 0; gx < grid_width; gx++) {
            double grid_x = x_min + (gx + 0.5) * grid->cell_width;
            double grid_y = y_min + (gy + 0.5) * grid->cell_height;
            
            double uncertainty = computeLocalDirectionalVariance(
                trajectories, traj_count, grid_x, grid_y, variance_radius);
            
            grid->values[gy * grid_width + gx] = uncertainty;
        }
    }
    
    return grid;
}

/*
 * Enhanced KDE computation with variance weighting
 */
static KDEGrid* computeVarianceWeightedKDE(EnhancedTrajectory *trajectories,
                                          int traj_count,
                                          Point2D *points, int point_count,
                                          int grid_width, int grid_height,
                                          double bandwidth_x, double bandwidth_y,
                                          double x_min, double x_max,
                                          double y_min, double y_max,
                                          double variance_radius,
                                          double variance_weight) {
    KDEGrid *grid = malloc(sizeof(KDEGrid));
    if (!grid) return NULL;
    
    grid->width = grid_width;
    grid->height = grid_height;
    grid->x_min = x_min;
    grid->x_max = x_max;
    grid->y_min = y_min;
    grid->y_max = y_max;
    grid->cell_width = (x_max - x_min) / grid_width;
    grid->cell_height = (y_max - y_min) / grid_height;
    
    grid->values = calloc(grid_width * grid_height, sizeof(double));
    if (!grid->values) {
        free(grid);
        return NULL;
    }
    
    // Compute KDE values for each grid cell
    for (int gy = 0; gy < grid_height; gy++) {
        for (int gx = 0; gx < grid_width; gx++) {
            double grid_x = x_min + (gx + 0.5) * grid->cell_width;
            double grid_y = y_min + (gy + 0.5) * grid->cell_height;
            
            // Compute directional variance at this location
            double dir_variance = computeLocalDirectionalVariance(
                trajectories, traj_count, grid_x, grid_y, variance_radius);
            
            // Compute traditional KDE value
            double kde_value = 0.0;
            for (int p = 0; p < point_count; p++) {
                double dx = grid_x - points[p].x;
                double dy = grid_y - points[p].y;
                kde_value += gaussian2D(dx, dy, bandwidth_x, bandwidth_y);
            }
            kde_value /= point_count;
            
            // Weight by directional variance (higher variance = more interesting)
            double variance_factor = 1.0 + variance_weight * dir_variance;
            grid->values[gy * grid_width + gx] = kde_value * variance_factor;
        }
    }
    
    return grid;
}

/*
 * Compute trajectory turns based on angle changes
 */
static int computeTrajectoryTurns(Trajectory *traj, Point2D **turns, int *turn_count,
                                 double angle_threshold) {
    if (traj->count < 3) {
        *turns = NULL;
        *turn_count = 0;
        return 1;
    }

    Point2D *temp_turns = malloc(traj->count * sizeof(Point2D));
    if (!temp_turns) return 0;

    int count = 0;
    
    for (int i = 1; i < traj->count - 1; i++) {
        // Calculate vectors
        double v1x = traj->points[i].x - traj->points[i-1].x;
        double v1y = traj->points[i].y - traj->points[i-1].y;
        double v2x = traj->points[i+1].x - traj->points[i].x;
        double v2y = traj->points[i+1].y - traj->points[i].y;
        
        // Calculate angle between vectors
        double dot = v1x * v2x + v1y * v2y;
        double mag1 = sqrt(v1x * v1x + v1y * v1y);
        double mag2 = sqrt(v2x * v2x + v2y * v2y);
        
        if (mag1 > 1e-10 && mag2 > 1e-10) {
            double cos_angle = dot / (mag1 * mag2);
            // Clamp to valid range for acos
            cos_angle = fmax(-1.0, fmin(1.0, cos_angle));
            double angle = acos(cos_angle);
            
            if (angle > angle_threshold) {
                temp_turns[count++] = traj->points[i];
            }
        }
    }
    
    if (count > 0) {
        *turns = malloc(count * sizeof(Point2D));
        if (!*turns) {
            free(temp_turns);
            return 0;
        }
        memcpy(*turns, temp_turns, count * sizeof(Point2D));
    } else {
        *turns = NULL;
    }
    
    *turn_count = count;
    free(temp_turns);
    return 1;
}

/*
 * Compute 2D KDE on a grid
 */
static KDEGrid* computeKDE2D(Point2D *points, int point_count, 
                            int grid_width, int grid_height,
                            double bandwidth_x, double bandwidth_y,
                            double x_min, double x_max, double y_min, double y_max) {
    KDEGrid *grid = malloc(sizeof(KDEGrid));
    if (!grid) return NULL;
    
    grid->width = grid_width;
    grid->height = grid_height;
    grid->x_min = x_min;
    grid->x_max = x_max;
    grid->y_min = y_min;
    grid->y_max = y_max;
    grid->cell_width = (x_max - x_min) / grid_width;
    grid->cell_height = (y_max - y_min) / grid_height;
    
    grid->values = calloc(grid_width * grid_height, sizeof(double));
    if (!grid->values) {
        free(grid);
        return NULL;
    }
    
    // Compute KDE values for each grid cell
    for (int gy = 0; gy < grid_height; gy++) {
        for (int gx = 0; gx < grid_width; gx++) {
            double grid_x = x_min + (gx + 0.5) * grid->cell_width;
            double grid_y = y_min + (gy + 0.5) * grid->cell_height;
            
            double kde_value = 0.0;
            for (int p = 0; p < point_count; p++) {
                double dx = grid_x - points[p].x;
                double dy = grid_y - points[p].y;
                kde_value += gaussian2D(dx, dy, bandwidth_x, bandwidth_y);
            }
            
            grid->values[gy * grid_width + gx] = kde_value / point_count;
        }
    }
    
    return grid;
}

/*
 * Find peaks in KDE grid using simple local maxima detection
 */
static int findKDEPeaks(KDEGrid *grid, Point2D **peaks, double **peak_values, 
                       int *peak_count, double threshold) {
    int max_peaks = grid->width * grid->height / 4;
    Point2D *temp_peaks = malloc(max_peaks * sizeof(Point2D));
    double *temp_values = malloc(max_peaks * sizeof(double));
    
    if (!temp_peaks || !temp_values) {
        free(temp_peaks);
        free(temp_values);
        return 0;
    }
    
    int count = 0;
    
    // Find local maxima (exclude border cells)
    for (int y = 1; y < grid->height - 1; y++) {
        for (int x = 1; x < grid->width - 1; x++) {
            int idx = y * grid->width + x;
            double value = grid->values[idx];
            
            if (value < threshold) continue;
            
            // Check if it's a local maximum
            int is_peak = 1;
            for (int dy = -1; dy <= 1 && is_peak; dy++) {
                for (int dx = -1; dx <= 1 && is_peak; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int neighbor_idx = (y + dy) * grid->width + (x + dx);
                    if (grid->values[neighbor_idx] > value) {
                        is_peak = 0;
                    }
                }
            }
            
            if (is_peak && count < max_peaks) {
                double peak_x = grid->x_min + (x + 0.5) * grid->cell_width;
                double peak_y = grid->y_min + (y + 0.5) * grid->cell_height;
                temp_peaks[count].x = peak_x;
                temp_peaks[count].y = peak_y;
                temp_values[count] = value;
                count++;
            }
        }
    }
    
    if (count > 0) {
        *peaks = malloc(count * sizeof(Point2D));
        *peak_values = malloc(count * sizeof(double));
        if (!*peaks || !*peak_values) {
            free(*peaks);
            free(*peak_values);
            free(temp_peaks);
            free(temp_values);
            return 0;
        }
        memcpy(*peaks, temp_peaks, count * sizeof(Point2D));
        memcpy(*peak_values, temp_values, count * sizeof(double));
    } else {
        *peaks = NULL;
        *peak_values = NULL;
    }
    
    *peak_count = count;
    free(temp_peaks);
    free(temp_values);
    return 1;
}

/*
 * Allocate a new KDE grid with given dimensions
 */
static KDEGrid* allocateKDEGrid(int width, int height,
                               double x_min, double x_max,
                               double y_min, double y_max) {
    KDEGrid *grid = malloc(sizeof(KDEGrid));
    if (!grid) return NULL;
    
    grid->width = width;
    grid->height = height;
    grid->x_min = x_min;
    grid->x_max = x_max;
    grid->y_min = y_min;
    grid->y_max = y_max;
    grid->cell_width = (x_max - x_min) / width;
    grid->cell_height = (y_max - y_min) / height;
    
    grid->values = calloc(width * height, sizeof(double));
    if (!grid->values) {
        free(grid);
        return NULL;
    }
    
    return grid;
}

/*
 * Create a deep copy of a KDE grid
 */
static KDEGrid* copyKDEGrid(KDEGrid *source) {
    if (!source) return NULL;
    
    KDEGrid *copy = allocateKDEGrid(source->width, source->height,
                                   source->x_min, source->x_max,
                                   source->y_min, source->y_max);
    if (!copy) return NULL;
    
    memcpy(copy->values, source->values, 
           source->width * source->height * sizeof(double));
    
    return copy;
}

/*
 * Normalize KDE grid to unit sum (probability distribution)
 */
static void normalizeKDEGrid(KDEGrid *grid) {
    if (!grid || !grid->values) return;
    
    double sum = 0.0;
    int total_cells = grid->width * grid->height;
    
    for (int i = 0; i < total_cells; i++) {
        sum += grid->values[i];
    }
    
    if (sum > 1e-10) {
        for (int i = 0; i < total_cells; i++) {
            grid->values[i] /= sum;
        }
    }
}

/*
 * Compute difference between two KDE grids (grid1 - grid2)
 */
static KDEGrid* computeKDEDifference(KDEGrid *grid1, KDEGrid *grid2) {
    if (!grid1 || !grid2) return NULL;
    
    if (grid1->width != grid2->width || grid1->height != grid2->height) {
        return NULL;
    }
    
    KDEGrid *diff = allocateKDEGrid(grid1->width, grid1->height,
                                   grid1->x_min, grid1->x_max,
                                   grid1->y_min, grid1->y_max);
    if (!diff) return NULL;
    
    int total_cells = grid1->width * grid1->height;
    for (int i = 0; i < total_cells; i++) {
        diff->values[i] = grid1->values[i] - grid2->values[i];
    }
    
    return diff;
}

/*
 * Compute ratio between two KDE grids (grid1 / grid2)
 */
static KDEGrid* computeKDERatio(KDEGrid *grid1, KDEGrid *grid2, double epsilon) {
    if (!grid1 || !grid2) return NULL;
    
    if (grid1->width != grid2->width || grid1->height != grid2->height) {
        return NULL;
    }
    
    KDEGrid *ratio = allocateKDEGrid(grid1->width, grid1->height,
                                    grid1->x_min, grid1->x_max,
                                    grid1->y_min, grid1->y_max);
    if (!ratio) return NULL;
    
    int total_cells = grid1->width * grid1->height;
    for (int i = 0; i < total_cells; i++) {
        double denominator = grid2->values[i] + epsilon;
        ratio->values[i] = grid1->values[i] / denominator;
    }
    
    return ratio;
}

/*
 * Compute spatial correlation between two KDE grids
 */
static double computeSpatialCorrelation(KDEGrid *grid1, KDEGrid *grid2) {
    if (!grid1 || !grid2) return 0.0;
    
    if (grid1->width != grid2->width || grid1->height != grid2->height) {
        return 0.0;
    }
    
    int total_cells = grid1->width * grid1->height;
    
    // Compute means
    double mean1 = 0.0, mean2 = 0.0;
    for (int i = 0; i < total_cells; i++) {
        mean1 += grid1->values[i];
        mean2 += grid2->values[i];
    }
    mean1 /= total_cells;
    mean2 /= total_cells;
    
    // Compute correlation coefficient
    double cov = 0.0;
    double var1 = 0.0, var2 = 0.0;
    
    for (int i = 0; i < total_cells; i++) {
        double diff1 = grid1->values[i] - mean1;
        double diff2 = grid2->values[i] - mean2;
        cov += diff1 * diff2;
        var1 += diff1 * diff1;
        var2 += diff2 * diff2;
    }
    
    if (var1 < 1e-10 || var2 < 1e-10) {
        return 0.0;
    }
    
    return cov / sqrt(var1 * var2);
}


/*
 * Check if a point is inside a rotated rectangle
 */
static int pointInRotatedRect(double px, double py, RotatedRect *rect) {
    // Translate point to rectangle's coordinate system
    double dx = px - rect->center_x;
    double dy = py - rect->center_y;
    
    // Rotate point by negative angle to align with rectangle axes
    double cos_theta = cos(-rect->rotation);
    double sin_theta = sin(-rect->rotation);
    
    double local_x = dx * cos_theta - dy * sin_theta;
    double local_y = dx * sin_theta + dy * cos_theta;
    
    // Check if point is within unrotated rectangle
    return (fabs(local_x) <= rect->width / 2.0 && 
            fabs(local_y) <= rect->height / 2.0);
}

/*
 * Compute bounding box for objects
 */
static void computeObjectBoundingBox(RotatedRect *objects, int count,
                                    double *x_min, double *x_max,
                                    double *y_min, double *y_max) {
    int first = 1;
    
    for (int i = 0; i < count; i++) {
        // Compute corners of rotated rectangle
        double hw = objects[i].width / 2.0;
        double hh = objects[i].height / 2.0;
        double cos_theta = cos(objects[i].rotation);
        double sin_theta = sin(objects[i].rotation);
        
        // Four corners in local coordinates
        double corners_local[4][2] = {
            {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
        };
        
        // Transform to world coordinates and track bounds
        for (int c = 0; c < 4; c++) {
            double world_x = objects[i].center_x + 
                           corners_local[c][0] * cos_theta - 
                           corners_local[c][1] * sin_theta;
            double world_y = objects[i].center_y + 
                           corners_local[c][0] * sin_theta + 
                           corners_local[c][1] * cos_theta;
            
            if (first) {
                *x_min = *x_max = world_x;
                *y_min = *y_max = world_y;
                first = 0;
            } else {
                if (world_x < *x_min) *x_min = world_x;
                if (world_x > *x_max) *x_max = world_x;
                if (world_y < *y_min) *y_min = world_y;
                if (world_y > *y_max) *y_max = world_y;
            }
        }
    }
}

/*
 * Compute object density grid
 * 
 * Creates a density map where each object contributes an oriented Gaussian blob
 * that is mostly circular with slight elongation along the object's major axis
 */
static KDEGrid* computeObjectDensity(RotatedRect *objects, int object_count,
                                    int grid_width, int grid_height,
                                    double x_min, double x_max,
                                    double y_min, double y_max,
                                    double bandwidth) {
    
    KDEGrid *grid = allocateKDEGrid(grid_width, grid_height, x_min, x_max, y_min, y_max);
    if (!grid) return NULL;
    
    double cell_width = (x_max - x_min) / grid_width;
    double cell_height = (y_max - y_min) / grid_height;
    
    // For each grid cell
    for (int gy = 0; gy < grid_height; gy++) {
        for (int gx = 0; gx < grid_width; gx++) {
            double cell_x = x_min + (gx + 0.5) * cell_width;
            double cell_y = y_min + (gy + 0.5) * cell_height;
            
            double density = 0.0;
            
            // Accumulate contribution from each object
            for (int obj = 0; obj < object_count; obj++) {
                // Transform grid point into object's local coordinate system
                double dx = cell_x - objects[obj].center_x;
                double dy = cell_y - objects[obj].center_y;
                
                // Rotate by negative angle to align with rectangle axes
                double cos_theta = cos(-objects[obj].rotation);
                double sin_theta = sin(-objects[obj].rotation);
                
                double local_x = dx * cos_theta - dy * sin_theta;
                double local_y = dx * sin_theta + dy * cos_theta;
                
                // Anisotropic Gaussian: mostly circular with slight elongation
                // Use max/min to make it orientation-independent
                double dim_max = fmax(objects[obj].width, objects[obj].height);
                double dim_min = fmin(objects[obj].width, objects[obj].height);
                double avg_size = (dim_max + dim_min) / 2.0;
                
                // Mix 90% isotropic + 10% anisotropic
                // Major axis gets the larger dimension, minor axis gets smaller
                double sigma_major = (0.9 * avg_size + 0.1 * dim_max) / 2.0 * bandwidth;
                double sigma_minor = (0.9 * avg_size + 0.1 * dim_min) / 2.0 * bandwidth;
                
                // Determine which local axis is which
                // If width > height, use sigma_major for x-axis, else swap
                double sigma_x, sigma_y;
                if (objects[obj].width > objects[obj].height) {
                    sigma_x = sigma_major;
                    sigma_y = sigma_minor;
                } else {
                    sigma_x = sigma_minor;
                    sigma_y = sigma_major;
                }
                
                // 2D Gaussian in local coordinates
                double term_x = (local_x * local_x) / (2.0 * sigma_x * sigma_x);
                double term_y = (local_y * local_y) / (2.0 * sigma_y * sigma_y);
                double kernel_val = exp(-(term_x + term_y));
                
                // No normalization - let larger objects be more prominent
                
                density += kernel_val;
            }
            
            grid->values[gy * grid_width + gx] = density;
        }
    }
    
    return grid;
}

/*
 * Compare two KDE grids and compute comprehensive metrics
 */
static ComparisonResult* compareKDEGrids(KDEGrid *grid1, KDEGrid *grid2,
                                        const char *comparison_mode,
                                        double significance_threshold) {
    if (!grid1 || !grid2) return NULL;
    
    ComparisonResult *result = malloc(sizeof(ComparisonResult));
    if (!result) return NULL;
    
    // Initialize
    result->grid_set1 = NULL;
    result->grid_set2 = NULL;
    result->difference_grid = NULL;
    result->ratio_grid = NULL;
    result->correlation = 0.0;
    result->mean_difference = 0.0;
    result->max_difference = 0.0;
    result->significant_cells = 0;
    
    // Copy normalized versions of input grids
    result->grid_set1 = copyKDEGrid(grid1);
    result->grid_set2 = copyKDEGrid(grid2);
    
    if (!result->grid_set1 || !result->grid_set2) {
        freeComparisonResult(result);
        return NULL;
    }
    
    normalizeKDEGrid(result->grid_set1);
    normalizeKDEGrid(result->grid_set2);
    
    // Compute correlation
    result->correlation = computeSpatialCorrelation(result->grid_set1, 
                                                   result->grid_set2);
    
    // Compute difference grid if requested
    if (strcmp(comparison_mode, "difference") == 0 || 
        strcmp(comparison_mode, "both") == 0) {
        result->difference_grid = computeKDEDifference(result->grid_set1,
                                                      result->grid_set2);
        if (!result->difference_grid) {
            freeComparisonResult(result);
            return NULL;
        }
        
        // Compute difference statistics
        int total_cells = result->difference_grid->width * result->difference_grid->height;
        double sum_abs_diff = 0.0;
        result->max_difference = 0.0;
        
        for (int i = 0; i < total_cells; i++) {
            double abs_diff = fabs(result->difference_grid->values[i]);
            sum_abs_diff += abs_diff;
            if (abs_diff > result->max_difference) {
                result->max_difference = abs_diff;
            }
            if (abs_diff > significance_threshold) {
                result->significant_cells++;
            }
        }
        
        result->mean_difference = sum_abs_diff / total_cells;
    }
    
    // Compute ratio grid if requested
    if (strcmp(comparison_mode, "ratio") == 0 || 
        strcmp(comparison_mode, "both") == 0) {
        double epsilon = 1e-10;
        result->ratio_grid = computeKDERatio(result->grid_set1,
                                            result->grid_set2, epsilon);
        if (!result->ratio_grid) {
            freeComparisonResult(result);
            return NULL;
        }
    }
    
    return result;
}

/*
 * Compute bounding box for a set of points
 */
static void computeBoundingBox(Point2D *points, int count,
                              double *x_min, double *x_max,
                              double *y_min, double *y_max) {
    if (count == 0) return;
    
    *x_min = *x_max = points[0].x;
    *y_min = *y_max = points[0].y;
    
    for (int i = 1; i < count; i++) {
        if (points[i].x < *x_min) *x_min = points[i].x;
        if (points[i].x > *x_max) *x_max = points[i].x;
        if (points[i].y < *y_min) *y_min = points[i].y;
        if (points[i].y > *y_max) *y_max = points[i].y;
    }
}

/*
 * Merge two bounding boxes
 */
static void mergeBoundingBoxes(double x1_min, double x1_max, double y1_min, double y1_max,
                              double x2_min, double x2_max, double y2_min, double y2_max,
                              double *x_min, double *x_max, double *y_min, double *y_max) {
    *x_min = fmin(x1_min, x2_min);
    *x_max = fmax(x1_max, x2_max);
    *y_min = fmin(y1_min, y2_min);
    *y_max = fmax(y1_max, y2_max);
}

/*
 * Create result DYN_GROUP with comparison data
 * Uses dfu_helpers for simplified data management
 */
static DYN_GROUP* createComparisonResultGroup(ComparisonResult *comp_result,
                                             const char *comparison_mode,
                                             const char *analysis_type) {
    if (!comp_result) return NULL;
    
    DYN_GROUP *result_group = dfuCreateGroup(10);
    if (!result_group) return NULL;
    
    // Create metadata list
    DYN_LIST *metadata = dfuCreateMetadataList("metadata");
    if (metadata) {
        dfuAddMetadata(metadata, "analysis_type", analysis_type);
        dfuAddMetadata(metadata, "comparison_mode", comparison_mode);
        dfuAddMetadataDouble(metadata, "correlation", comp_result->correlation);
        
        if (comp_result->difference_grid) {
            dfuAddMetadataDouble(metadata, "mean_difference", comp_result->mean_difference);
            dfuAddMetadataDouble(metadata, "max_difference", comp_result->max_difference);
            dfuAddMetadataInt(metadata, "significant_cells", comp_result->significant_cells);
        }
        
        dfuAddDynGroupExistingList(result_group, "metadata", metadata);
    }
    
    // Create grid_info list
    float grid_info_vals[6] = {
        (float)comp_result->grid_set1->x_min,
        (float)comp_result->grid_set1->x_max,
        (float)comp_result->grid_set1->y_min,
        (float)comp_result->grid_set1->y_max,
        (float)comp_result->grid_set1->width,
        (float)comp_result->grid_set1->height
    };
    dfuAddFloatListToGroup(result_group, "grid_info", grid_info_vals, 6);
    
    // Add grid data
    int total_cells = comp_result->grid_set1->width * comp_result->grid_set1->height;
    
    dfuAddDoubleListToGroup(result_group, "kde_set1", 
                           comp_result->grid_set1->values, total_cells);
    dfuAddDoubleListToGroup(result_group, "kde_set2",
                           comp_result->grid_set2->values, total_cells);
    
    if (comp_result->difference_grid) {
        dfuAddDoubleListToGroup(result_group, "difference",
                               comp_result->difference_grid->values, total_cells);
    }
    
    if (comp_result->ratio_grid) {
        dfuAddDoubleListToGroup(result_group, "ratio",
                               comp_result->ratio_grid->values, total_cells);
    }
    
    return result_group;
}

/*
 * Free comparison result structure
 */
static void freeComparisonResult(ComparisonResult *result) {
    if (!result) return;
    
    if (result->grid_set1) freeKDEGrid(result->grid_set1);
    if (result->grid_set2) freeKDEGrid(result->grid_set2);
    if (result->difference_grid) freeKDEGrid(result->difference_grid);
    if (result->ratio_grid) freeKDEGrid(result->ratio_grid);
    
    free(result);
}

/*
 * Extract trajectory data from DYN_LIST structure
 * Matches the original pattern exactly
 */
static int extractTrajectoriesFromDynList(Tcl_Interp *interp, DYN_LIST *dl, 
                                         Trajectory **trajectories, int *count) {
    if (DYN_LIST_DATATYPE(dl) != DF_LIST) {
        Tcl_SetResult(interp, "Input must be a list of trajectory lists", TCL_STATIC);
        return 0;
    }
    Trajectory *traj;
    
    *count = DYN_LIST_N(dl);
    traj = malloc(*count * sizeof(Trajectory));
    if (!traj) {
        Tcl_SetResult(interp, "Memory allocation failed", TCL_STATIC);
        return 0;
    }
    
    DYN_LIST **trajectory_lists = (DYN_LIST **)DYN_LIST_VALS(dl);
    DYN_LIST *x_dl, *y_dl;
    float *xvals, *yvals;
    
    for (int i = 0; i < *count; i++) {
        DYN_LIST *traj_list = trajectory_lists[i];
        
        if (DYN_LIST_DATATYPE(traj_list) != DF_LIST ||
            DYN_LIST_N(traj_list) != 2) {
            freeTrajectories(traj, i);
            Tcl_SetResult(interp, "trajectory must be x and y lists", TCL_STATIC);
            return 0;
        }

        x_dl = ((DYN_LIST **) DYN_LIST_VALS(traj_list))[0];
        y_dl = ((DYN_LIST **) DYN_LIST_VALS(traj_list))[1];
        if (DYN_LIST_N(x_dl) != DYN_LIST_N(y_dl)) {
            freeTrajectories(traj, i);
            Tcl_SetResult(interp, "x and y lists not of equal length", TCL_STATIC);
            return 0;
        }

        if (DYN_LIST_DATATYPE(x_dl) != DF_FLOAT ||
            DYN_LIST_DATATYPE(y_dl) != DF_FLOAT) {
            freeTrajectories(traj, i);
            Tcl_SetResult(interp, "x and y lists must be of type float", TCL_STATIC);
            return 0;
        }
        xvals = (float *) DYN_LIST_VALS(x_dl);
        yvals = (float *) DYN_LIST_VALS(y_dl);
        
        int point_count = DYN_LIST_N(x_dl);
        traj[i].count = point_count;
        traj[i].capacity = point_count;
        traj[i].points = malloc(point_count * sizeof(Point2D));
        
        if (!traj[i].points) {
            freeTrajectories(traj, i);
            Tcl_SetResult(interp, "memory allocation failed for trajectory points", TCL_STATIC);
            return 0;
        }
        
        for (int j = 0; j < point_count; j++) {
            traj[i].points[j].x = xvals[j];
            traj[i].points[j].y = yvals[j];
        }
    }
    /* set output if requested */
    if (trajectories) *trajectories = traj;
    return 1;
}

/*
 * Extract enhanced trajectories from DYN_LIST and compute directional info
 */
static int extractEnhancedTrajectoriesFromDynList(Tcl_Interp *interp, DYN_LIST *dl,
                                                  EnhancedTrajectory **trajectories, int *count) {
    // First extract as regular trajectories
    Trajectory *simple_trajs;
    if (!extractTrajectoriesFromDynList(interp, dl, &simple_trajs, count)) {
        return 0;
    }
    
    // Allocate enhanced trajectories
    *trajectories = calloc(*count, sizeof(EnhancedTrajectory));
    if (!*trajectories) {
        freeTrajectories(simple_trajs, *count);
        Tcl_SetResult(interp, "Memory allocation failed for enhanced trajectories", TCL_STATIC);
        return 0;
    }
    
    // Convert and compute directional info
    for (int i = 0; i < *count; i++) {
        (*trajectories)[i].points = simple_trajs[i].points;
        (*trajectories)[i].count = simple_trajs[i].count;
        (*trajectories)[i].capacity = simple_trajs[i].capacity;
        (*trajectories)[i].directions = NULL;
        (*trajectories)[i].dir_variances = NULL;
        
        // Compute directional information
        if (!computeDirectionalInfo(&(*trajectories)[i])) {
            // Cleanup on failure
            for (int j = 0; j <= i; j++) {
                free((*trajectories)[j].directions);
            }
            free(*trajectories);
            free(simple_trajs);
            Tcl_SetResult(interp, "Failed to compute directional info", TCL_STATIC);
            return 0;
        }
    }
    
    free(simple_trajs); // Free the wrapper array, but points are now owned by enhanced trajs
    return 1;
}


/*
 * Resample trajectory at uniform spatial intervals
 * 
 * This removes temporal weighting where the ball spends more time,
 * creating a uniform density "tube" along the trajectory path.
 * 
 * Returns 1 on success, 0 on failure
 */
static int resampleTrajectoryUniform(Trajectory *traj, double spacing,
                                    Point2D **resampled_points, int *resampled_count) {
    if (!traj || traj->count < 2 || spacing <= 0.0) {
        return 0;
    }
    
    // Estimate maximum possible points (pessimistic: every original point could spawn multiple)
    // Calculate total path length for better estimate
    double total_length = 0.0;
    for (int i = 1; i < traj->count; i++) {
        double dx = traj->points[i].x - traj->points[i-1].x;
        double dy = traj->points[i].y - traj->points[i-1].y;
        total_length += sqrt(dx*dx + dy*dy);
    }
    
    int max_points = (int)(total_length / spacing) + 10;  // +10 for safety
    if (max_points < traj->count) max_points = traj->count;
    
    Point2D *points = malloc(max_points * sizeof(Point2D));
    if (!points) return 0;
    
    int count = 0;
    
    // Start with first point
    points[count++] = traj->points[0];
    
    double accumulated = 0.0;
    Point2D last = traj->points[0];
    
    for (int i = 1; i < traj->count && count < max_points; i++) {
        double dx = traj->points[i].x - last.x;
        double dy = traj->points[i].y - last.y;
        double seg_dist = sqrt(dx*dx + dy*dy);
        
        if (seg_dist < 1e-10) continue;  // Skip duplicate points
        
        accumulated += seg_dist;
        
        // Emit points along this segment at regular spacing intervals
        while (accumulated >= spacing && count < max_points - 1) {
            // Calculate fraction along segment for next resampled point
            double remaining_in_seg = seg_dist - (accumulated - seg_dist - spacing);
            double frac = remaining_in_seg / seg_dist;
            
            if (frac < 0.0) frac = 0.0;
            if (frac > 1.0) frac = 1.0;
            
            Point2D new_pt;
            new_pt.x = last.x + frac * dx;
            new_pt.y = last.y + frac * dy;
            
            points[count++] = new_pt;
            
            // Update for next iteration along this segment
            last = new_pt;
            accumulated -= spacing;
            
            // Recompute remaining segment
            dx = traj->points[i].x - last.x;
            dy = traj->points[i].y - last.y;
            seg_dist = sqrt(dx*dx + dy*dy);
        }
        
        // Move to next original point
        last = traj->points[i];
    }
    
    // Always add final point
    if (count < max_points) {
        points[count++] = traj->points[traj->count - 1];
    }
    
    *resampled_points = points;
    *resampled_count = count;
    
    return 1;
}


/*
 * Create result group for basic analysis
 * Uses original pattern
 */
static DYN_GROUP* createResultGroup(Point2D *peaks, double *peak_values,
                                   int peak_count, KDEGrid *grid) {
    DYN_GROUP *result_group = dfuCreateDynGroup(5);
    if (!result_group) return NULL;
    
    // Create peaks_x list
    DYN_LIST *peaks_x = dfuCreateDynList(DF_FLOAT, peak_count);
    if (peaks_x) {
        for (int i = 0; i < peak_count; i++) {
            dfuAddDynListFloat(peaks_x, (float)peaks[i].x);
        }
        strncpy(DYN_LIST_NAME(peaks_x), "peaks_x", DYN_LIST_NAME_SIZE-1);
        dfuAddDynGroupExistingList(result_group, "peaks_x", peaks_x);
    }
    
    // Create peaks_y list
    DYN_LIST *peaks_y = dfuCreateDynList(DF_FLOAT, peak_count);
    if (peaks_y) {
        for (int i = 0; i < peak_count; i++) {
            dfuAddDynListFloat(peaks_y, (float)peaks[i].y);
        }
        strncpy(DYN_LIST_NAME(peaks_y), "peaks_y", DYN_LIST_NAME_SIZE-1);
        dfuAddDynGroupExistingList(result_group, "peaks_y", peaks_y);
    }
    
    // Create peak_values list
    DYN_LIST *peak_vals = dfuCreateDynList(DF_FLOAT, peak_count);
    if (peak_vals) {
        for (int i = 0; i < peak_count; i++) {
            dfuAddDynListFloat(peak_vals, (float)peak_values[i]);
        }
        strncpy(DYN_LIST_NAME(peak_vals), "peak_values", DYN_LIST_NAME_SIZE-1);
        dfuAddDynGroupExistingList(result_group, "peak_values", peak_vals);
    }
    
    // Create grid metadata
    DYN_LIST *grid_info = dfuCreateDynList(DF_FLOAT, 6);
    if (grid_info) {
        dfuAddDynListFloat(grid_info, (float)grid->x_min);
        dfuAddDynListFloat(grid_info, (float)grid->x_max);
        dfuAddDynListFloat(grid_info, (float)grid->y_min);
        dfuAddDynListFloat(grid_info, (float)grid->y_max);
        dfuAddDynListFloat(grid_info, (float)grid->width);
        dfuAddDynListFloat(grid_info, (float)grid->height);
        strncpy(DYN_LIST_NAME(grid_info), "grid_info", DYN_LIST_NAME_SIZE-1);
        dfuAddDynGroupExistingList(result_group, "grid_info", grid_info);
    }
    
    // Create grid values
    DYN_LIST *grid_matrix = dfuCreateDynList(DF_FLOAT, grid->width * grid->height);
    if (grid_matrix) {
        for (int i = 0; i < grid->width * grid->height; i++) {
            dfuAddDynListFloat(grid_matrix, (float)grid->values[i]);
        }
        strncpy(DYN_LIST_NAME(grid_matrix), "kde_grid", DYN_LIST_NAME_SIZE-1);
        dfuAddDynGroupExistingList(result_group, "kde_grid", grid_matrix);
    }
    
    return result_group;
}

/*
 * trajectory_compare - Compare two KDE grids
 * 
 * Usage: trajectory_compare <grid1> <grid2> -grid_size N ?options?
 * 
 * Arguments:
 *   grid1, grid2: DYN_LIST names containing grid values
 * 
 * Required Options:
 *   -grid_size N             Grid dimensions (assumes square NxN grid)
 * 
 * Optional:
 *   -comparison_mode MODE    "difference", "ratio", or "both" (default: "both")
 *   -significance_thresh VAL Threshold for significant differences (default: 0.01)
 * 
 * Returns: DYN_GROUP with comparison results
 * 
 * Note: Grids must have been created with identical bounds and dimensions.
 */
static int TrajectoryCompareCmd(ClientData clientData, Tcl_Interp *interp,
                               int objc, Tcl_Obj *const objv[]) {
    (void)clientData;
    
    // Default parameters
    const char *comparison_mode = "both";
    double significance_threshold = 0.01;
    int grid_size = 0;
    int have_grid_size = 0;
    
    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 1, objv,
                        "grid1 grid2 -grid_size N "
                        "?-comparison_mode MODE? ?-significance_thresh VAL?");
        return TCL_ERROR;
    }
    
    // Parse options
    for (int i = 3; i < objc; i += 2) {
        if (i + 1 >= objc) {
            Tcl_SetResult(interp, "Missing value for option", TCL_STATIC);
            return TCL_ERROR;
        }
        
        const char *option = Tcl_GetString(objv[i]);
        
        if (strcmp(option, "-grid_size") == 0) {
            if (Tcl_GetIntFromObj(interp, objv[i+1], &grid_size) != TCL_OK) {
                return TCL_ERROR;
            }
            have_grid_size = 1;
        } else if (strcmp(option, "-comparison_mode") == 0) {
            comparison_mode = Tcl_GetString(objv[i+1]);
            if (strcmp(comparison_mode, "difference") != 0 &&
                strcmp(comparison_mode, "ratio") != 0 &&
                strcmp(comparison_mode, "both") != 0) {
                Tcl_SetResult(interp, 
                            "Mode must be 'difference', 'ratio', or 'both'",
                            TCL_STATIC);
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-significance_thresh") == 0) {
            if (Tcl_GetDoubleFromObj(interp, objv[i+1], &significance_threshold) != TCL_OK) {
                return TCL_ERROR;
            }
        } else {
            Tcl_AppendResult(interp, "Unknown option: ", option, NULL);
            return TCL_ERROR;
        }
    }
    
    // Verify grid_size was provided
    if (!have_grid_size) {
        Tcl_SetResult(interp, "Must specify -grid_size N", TCL_STATIC);
        return TCL_ERROR;
    }
    
    if (grid_size <= 0) {
        Tcl_SetResult(interp, "Grid size must be positive", TCL_STATIC);
        return TCL_ERROR;
    }
    
    // Get the two grid lists
    char *grid_name1 = Tcl_GetString(objv[1]);
    char *grid_name2 = Tcl_GetString(objv[2]);
    
    DYN_LIST *grid_list1, *grid_list2;
    
    if (tclFindDynList(interp, grid_name1, &grid_list1) != TCL_OK) {
        return TCL_ERROR;
    }
    
    if (tclFindDynList(interp, grid_name2, &grid_list2) != TCL_OK) {
        return TCL_ERROR;
    }
    
    // Verify list lengths match expected grid size
    int expected_length = grid_size * grid_size;
    int n1 = DYN_LIST_N(grid_list1);
    int n2 = DYN_LIST_N(grid_list2);
    
    if (n1 != expected_length) {
        char msg[256];
        snprintf(msg, sizeof(msg), 
                "Grid1 length (%d) doesn't match grid_size %d (expected %d)",
                n1, grid_size, expected_length);
        Tcl_SetResult(interp, msg, TCL_VOLATILE);
        return TCL_ERROR;
    }
    
    if (n2 != expected_length) {
        char msg[256];
        snprintf(msg, sizeof(msg), 
                "Grid2 length (%d) doesn't match grid_size %d (expected %d)",
                n2, grid_size, expected_length);
        Tcl_SetResult(interp, msg, TCL_VOLATILE);
        return TCL_ERROR;
    }
    
    // Create KDEGrid structures (bounds are dummy values [0,1] x [0,1])
    KDEGrid *kde1 = allocateKDEGrid(grid_size, grid_size, 0.0, 1.0, 0.0, 1.0);
    KDEGrid *kde2 = allocateKDEGrid(grid_size, grid_size, 0.0, 1.0, 0.0, 1.0);
    
    if (!kde1 || !kde2) {
        if (kde1) freeKDEGrid(kde1);
        if (kde2) freeKDEGrid(kde2);
        Tcl_SetResult(interp, "Failed to allocate KDE grids", TCL_STATIC);
        return TCL_ERROR;
    }
    
    // Copy grid values
    float *values1 = (float *)DYN_LIST_VALS(grid_list1);
    float *values2 = (float *)DYN_LIST_VALS(grid_list2);
    
    for (int i = 0; i < expected_length; i++) {
        kde1->values[i] = (double)values1[i];
        kde2->values[i] = (double)values2[i];
    }
    
    // Perform comparison
    ComparisonResult *comp_result = compareKDEGrids(kde1, kde2,
						    comparison_mode,
						    significance_threshold);
    
    freeKDEGrid(kde1);
    freeKDEGrid(kde2);
    
    if (!comp_result) {
        Tcl_SetResult(interp, "Failed to compare grids", TCL_STATIC);
        return TCL_ERROR;
    }
    
    // Create result group
    DYN_GROUP *result = createComparisonResultGroup(comp_result, 
                                                   comparison_mode,
                                                   "grid_comparison");
    
    freeComparisonResult(comp_result);
    
    if (!result) {
        Tcl_SetResult(interp, "Failed to create result group", TCL_STATIC);
        return TCL_ERROR;
    }
    
    // Put result in Tcl interpreter
    if (tclPutGroup(interp, result) != TCL_OK) {
        dfuFreeDynGroup(result);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}


/*
 * Helper functions for cleanup
 */
static void freeTrajectories(Trajectory *trajectories, int count) {
    if (trajectories) {
        for (int i = 0; i < count; i++) {
            free(trajectories[i].points);
        }
        free(trajectories);
    }
}

static void freeEnhancedTrajectories(EnhancedTrajectory *trajectories, int count) {
    if (trajectories) {
        for (int i = 0; i < count; i++) {
            free(trajectories[i].points);
            free(trajectories[i].directions);
            free(trajectories[i].dir_variances);
        }
        free(trajectories);
    }
}

static void freeKDEGrid(KDEGrid *grid) {
    if (grid) {
        free(grid->values);
        free(grid);
    }
}

/*
 * Enhanced trajectory analysis command with directional variance
 * 
 * Usage: trajectory_analyze_enhanced traj_list ?options?
 * Options:
 *   -mode MODE           Analysis mode: density, uncertainty, or combined (default: combined)
 *   -variance_radius R   Radius for variance computation (default: 2.0)
 *   -variance_weight W   Weight factor for variance in combined mode (default: 2.0)
 *   ... plus all standard trajectory_analyze options
 */
static int TrajectoryAnalyzeCmd(ClientData clientData, Tcl_Interp *interp,
                                       int objc, Tcl_Obj *const objv[]) {
    (void)clientData;
    
    // Default parameters
    int grid_size = 100;
    double bandwidth_x = 5.0;
    double bandwidth_y = 5.0;
    double angle_threshold = 0.5;
    double threshold = 0.01;
    double variance_radius = 2.0;
    double variance_weight = 2.0;
    const char *analysis_mode = "combined"; // density, uncertainty, or combined
    const char *analysis_type = "turns";     // turns or path
    const char *saturation_mode = "none";
    double gamma = 1.0;

    double resample_spacing = 0.0;  // 0 = no resampling
    int do_resample = 0;
    
    // Optional explicit bounds
    int use_explicit_bounds = 0;
    double explicit_x_min = 0.0, explicit_x_max = 0.0;
    double explicit_y_min = 0.0, explicit_y_max = 0.0;
    
    if (objc < 2) {
       Tcl_WrongNumArgs(interp, 1, objv,
                        "traj_list ?-grid_size N? ?-bandwidth_x BW? ?-bandwidth_y BW? "
                        "?-angle_threshold RAD? ?-threshold VAL? ?-variance_radius R? "
                        "?-variance_weight W? ?-mode MODE? ?-analysis_type TYPE? "
                        "?-saturation MODE? ?-gamma VAL? ?-resample_spacing DIST? "
                        "?-bounds {x_min x_max y_min y_max}?");
        return TCL_ERROR;
    }
    
    // Parse options
    for (int i = 2; i < objc; i += 2) {
        if (i + 1 >= objc) {
            Tcl_SetResult(interp, "Missing value for option", TCL_STATIC);
            return TCL_ERROR;
        }
        
        const char *option = Tcl_GetString(objv[i]);
        
        if (strcmp(option, "-grid_size") == 0) {
            if (Tcl_GetIntFromObj(interp, objv[i+1], &grid_size) != TCL_OK) {
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-bandwidth_x") == 0) {
            if (Tcl_GetDoubleFromObj(interp, objv[i+1], &bandwidth_x) != TCL_OK) {
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-bandwidth_y") == 0) {
            if (Tcl_GetDoubleFromObj(interp, objv[i+1], &bandwidth_y) != TCL_OK) {
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-angle_threshold") == 0) {
            if (Tcl_GetDoubleFromObj(interp, objv[i+1], &angle_threshold) != TCL_OK) {
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-threshold") == 0) {
            if (Tcl_GetDoubleFromObj(interp, objv[i+1], &threshold) != TCL_OK) {
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-variance_radius") == 0) {
            if (Tcl_GetDoubleFromObj(interp, objv[i+1], &variance_radius) != TCL_OK) {
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-variance_weight") == 0) {
            if (Tcl_GetDoubleFromObj(interp, objv[i+1], &variance_weight) != TCL_OK) {
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-mode") == 0) {
            analysis_mode = Tcl_GetString(objv[i+1]);
            if (strcmp(analysis_mode, "density") != 0 &&
                strcmp(analysis_mode, "uncertainty") != 0 &&
                strcmp(analysis_mode, "combined") != 0) {
                Tcl_SetResult(interp, "Mode must be 'density', 'uncertainty', or 'combined'",
                            TCL_STATIC);
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-analysis_type") == 0) {
            analysis_type = Tcl_GetString(objv[i+1]);
            if (strcmp(analysis_type, "turns") != 0 &&
                strcmp(analysis_type, "path") != 0) {
                Tcl_SetResult(interp, "Analysis type must be 'turns' or 'path'",
                            TCL_STATIC);
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-bounds") == 0) {
            // Parse bounds list: {x_min x_max y_min y_max}
	  Tcl_Size bounds_objc;
            Tcl_Obj **bounds_objv;
            
            if (Tcl_ListObjGetElements(interp, objv[i+1], &bounds_objc, &bounds_objv) != TCL_OK) {
                return TCL_ERROR;
            }
            
            if (bounds_objc != 4) {
                Tcl_SetResult(interp, "Bounds must be {x_min x_max y_min y_max}", TCL_STATIC);
                return TCL_ERROR;
            }
            
            if (Tcl_GetDoubleFromObj(interp, bounds_objv[0], &explicit_x_min) != TCL_OK ||
                Tcl_GetDoubleFromObj(interp, bounds_objv[1], &explicit_x_max) != TCL_OK ||
                Tcl_GetDoubleFromObj(interp, bounds_objv[2], &explicit_y_min) != TCL_OK ||
                Tcl_GetDoubleFromObj(interp, bounds_objv[3], &explicit_y_max) != TCL_OK) {
                return TCL_ERROR;
            }
            
            use_explicit_bounds = 1;
        } else if (strcmp(option, "-saturation") == 0) {      
	  saturation_mode = Tcl_GetString(objv[i+1]);         
        } else if (strcmp(option, "-gamma") == 0) {           
	  if (Tcl_GetDoubleFromObj(interp, objv[i+1], &gamma) != TCL_OK) {  
	    gamma = 1.0;    
	  }                 
        } else if (strcmp(option, "-resample_spacing") == 0) {
	  if (Tcl_GetDoubleFromObj(interp, objv[i+1], &resample_spacing) != TCL_OK) {
	    return TCL_ERROR;
	  }
	  if (resample_spacing > 0.0) {
	    do_resample = 1;
	  }
	  
        } else {
	  Tcl_AppendResult(interp, "Unknown option: ", option, NULL);
	  return TCL_ERROR;
        }
    }
    
    // Get trajectory data
    char *traj_name = Tcl_GetString(objv[1]);
    DYN_LIST *trajectory_list;
    
    if (tclFindDynList(interp, traj_name, &trajectory_list) != TCL_OK) {
        return TCL_ERROR;
    }
    
    // Extract enhanced trajectories
    EnhancedTrajectory *trajectories;
    int trajectory_count;
    
    if (!extractEnhancedTrajectoriesFromDynList(interp, trajectory_list,
                                               &trajectories, &trajectory_count)) {
        return TCL_ERROR;
    }

    // Apply spatial resampling if requested (only for path analysis)
    if (do_resample && strcmp(analysis_type, "path") == 0) {
        for (int t = 0; t < trajectory_count; t++) {
            Point2D *resampled;
            int resampled_count;
            
            // Create temporary simple trajectory for resampling
            Trajectory temp;
            temp.points = trajectories[t].points;
            temp.count = trajectories[t].count;
            temp.capacity = trajectories[t].capacity;
            
            if (resampleTrajectoryUniform(&temp, resample_spacing,
                                         &resampled, &resampled_count)) {
                // Replace trajectory points with resampled version
                // Don't free trajectories[t].points yet - it's still owned by temp
                trajectories[t].points = resampled;
                trajectories[t].count = resampled_count;
                trajectories[t].capacity = resampled_count;
                
                // Free old points (now temp.points)
                free(temp.points);
                
                // Recompute directional info for new points
                if (trajectories[t].directions) {
                    free(trajectories[t].directions);
                    trajectories[t].directions = NULL;
                }
                computeDirectionalInfo(&trajectories[t]);
            }
        }
    }
    
    // For density and combined modes, we need points (either turns or full path)
    Point2D *all_points = NULL;
    int total_turns = 0;
    
    if (strcmp(analysis_mode, "density") == 0 || strcmp(analysis_mode, "combined") == 0) {
        if (strcmp(analysis_type, "path") == 0) {
            // Path mode: collect ALL trajectory points
            for (int i = 0; i < trajectory_count; i++) {
                if (trajectories[i].count > 0) {
                    all_points = realloc(all_points,
                                        (total_turns + trajectories[i].count) * sizeof(Point2D));
                    if (all_points) {
                        memcpy(&all_points[total_turns], trajectories[i].points,
                              trajectories[i].count * sizeof(Point2D));
                        total_turns += trajectories[i].count;
                    }
                }
            }
            
            if (total_turns == 0) {
                freeEnhancedTrajectories(trajectories, trajectory_count);
                Tcl_SetResult(interp, "No points found in trajectories", TCL_STATIC);
                return TCL_ERROR;
            }
        } else {
            // Turns mode: compute turns for all trajectories
            for (int i = 0; i < trajectory_count; i++) {
                Point2D *turns;
                int turn_count;
                
                // Convert to simple trajectory for turn computation
                Trajectory simple_traj;
                simple_traj.points = trajectories[i].points;
                simple_traj.count = trajectories[i].count;
                simple_traj.capacity = trajectories[i].capacity;
                
                if (computeTrajectoryTurns(&simple_traj, &turns, &turn_count, angle_threshold)) {
                    if (turn_count > 0) {
                        all_points = realloc(all_points, (total_turns + turn_count) * sizeof(Point2D));
                        if (all_points) {
                            memcpy(&all_points[total_turns], turns, turn_count * sizeof(Point2D));
                            total_turns += turn_count;
                        }
                        free(turns);
                    }
                }
            }
            
            if (total_turns == 0 && strcmp(analysis_mode, "density") == 0) {
                freeEnhancedTrajectories(trajectories, trajectory_count);
                Tcl_SetResult(interp, "No turns found in trajectories", TCL_STATIC);
                return TCL_ERROR;
            }
        }
    }
    
    // Find bounding box
    double x_min, x_max, y_min, y_max;
    
    if (use_explicit_bounds) {
        // Use user-provided bounds
        x_min = explicit_x_min;
        x_max = explicit_x_max;
        y_min = explicit_y_min;
        y_max = explicit_y_max;
    } else {
        // Auto-compute from data
        if (total_turns > 0) {
            computeBoundingBox(all_points, total_turns, &x_min, &x_max, &y_min, &y_max);
        } else {
            // For uncertainty-only mode, find bounding box from all trajectory points
            int first_point = 1;
            for (int t = 0; t < trajectory_count; t++) {
                for (int i = 0; i < trajectories[t].count; i++) {
                    if (first_point) {
                        x_min = x_max = trajectories[t].points[i].x;
                        y_min = y_max = trajectories[t].points[i].y;
                        first_point = 0;
                    } else {
                        if (trajectories[t].points[i].x < x_min) x_min = trajectories[t].points[i].x;
                        if (trajectories[t].points[i].x > x_max) x_max = trajectories[t].points[i].x;
                        if (trajectories[t].points[i].y < y_min) y_min = trajectories[t].points[i].y;
                        if (trajectories[t].points[i].y > y_max) y_max = trajectories[t].points[i].y;
                    }
                }
            }
        }
        
        // Add padding
        double x_range = x_max - x_min;
        double y_range = y_max - y_min;
        x_min -= x_range * 0.1;
        x_max += x_range * 0.1;
        y_min -= y_range * 0.1;
        y_max += y_range * 0.1;
    }
    
    // Compute analysis grid based on mode
    KDEGrid *result_grid = NULL;
    
    if (strcmp(analysis_mode, "uncertainty") == 0) {
        // Pure uncertainty analysis
        result_grid = computeUncertaintyMap(trajectories, trajectory_count,
                                          grid_size, grid_size,
                                          x_min, x_max, y_min, y_max,
                                          variance_radius);
    } else if (strcmp(analysis_mode, "combined") == 0) {
        // Combined density + uncertainty analysis
        result_grid = computeVarianceWeightedKDE(trajectories, trajectory_count,
                                               all_points, total_turns,
                                               grid_size, grid_size,
                                               bandwidth_x, bandwidth_y,
                                               x_min, x_max, y_min, y_max,
                                               variance_radius, variance_weight);
    }
    else {
        // Default to density-only analysis
        result_grid = computeKDE2D(all_points, total_turns, grid_size, grid_size,
                                 bandwidth_x, bandwidth_y,
                                 x_min, x_max, y_min, y_max);
    }
    
    if (!result_grid) {
        if (all_points) free(all_points);
        freeEnhancedTrajectories(trajectories, trajectory_count);
        Tcl_SetResult(interp, "Failed to compute analysis grid", TCL_STATIC);
        return TCL_ERROR;
    }

    if (strcmp(analysis_type, "path") == 0 && strcmp(analysis_mode, "density") == 0) {
      // Extract saturation parameters from command line
      const char *saturation_mode = "none";
      double gamma = 1.0;
      
      // Scan through command line arguments for saturation options
      for (int i = 2; i < objc; i += 2) {
	if (i + 1 >= objc) break;
	const char *opt = Tcl_GetString(objv[i]);
        
	if (strcmp(opt, "-saturation") == 0) {
	  saturation_mode = Tcl_GetString(objv[i + 1]);
	} else if (strcmp(opt, "-gamma") == 0) {
	  if (Tcl_GetDoubleFromObj(interp, objv[i + 1], &gamma) != TCL_OK) {
	    gamma = 1.0;  // Use default on parse error
	  }
	}
      }
      
      // Apply saturation transformation to the grid
      if (strcmp(saturation_mode, "none") != 0) {
	applySaturationToGrid(result_grid, gamma, saturation_mode);
      }
    }
    
    // Find peaks
    Point2D *peaks;
    double *peak_values;
    int peak_count;
    
    if (!findKDEPeaks(result_grid, &peaks, &peak_values, &peak_count, threshold)) {
        if (all_points) free(all_points);
        freeEnhancedTrajectories(trajectories, trajectory_count);
        freeKDEGrid(result_grid);
        Tcl_SetResult(interp, "Failed to find peaks", TCL_STATIC);
        return TCL_ERROR;
    }
    
    // Create result group with metadata about analysis mode
    DYN_GROUP *result = dfuCreateGroup(10);
    if (!result) {
        free(peaks);
        free(peak_values);
        if (all_points) free(all_points);
        freeEnhancedTrajectories(trajectories, trajectory_count);
        freeKDEGrid(result_grid);
        Tcl_SetResult(interp, "Failed to create result group", TCL_STATIC);
        return TCL_ERROR;
    }
    
    // Add metadata
    DYN_LIST *metadata = dfuCreateMetadataList("metadata");
    if (metadata) {
        dfuAddMetadata(metadata, "analysis_type", analysis_type);  // "turns" or "path"
        dfuAddMetadata(metadata, "mode", analysis_mode);           // "density", "uncertainty", "combined"
        dfuAddMetadataDouble(metadata, "variance_radius", variance_radius);
        if (strcmp(analysis_mode, "combined") == 0) {
            dfuAddMetadataDouble(metadata, "variance_weight", variance_weight);
        }
	if (strcmp(analysis_type, "path") == 0 && strcmp(analysis_mode, "density") == 0) {
	  // Add saturation parameters to metadata
	  const char *saturation_mode = "none";
	  double gamma = 1.0;
	  
	  for (int i = 2; i < objc; i += 2) {
            if (i + 1 >= objc) break;
            const char *opt = Tcl_GetString(objv[i]);
            if (strcmp(opt, "-saturation") == 0) {
	      saturation_mode = Tcl_GetString(objv[i + 1]);
            } else if (strcmp(opt, "-gamma") == 0) {
	      Tcl_GetDoubleFromObj(interp, objv[i + 1], &gamma);
            }
	  }
	  
	  dfuAddMetadata(metadata, "saturation_mode", saturation_mode);
	  dfuAddMetadataDouble(metadata, "gamma", gamma);
	}

        if (do_resample) {
            dfuAddMetadataDouble(metadata, "resample_spacing", resample_spacing);
        }	
	
        dfuAddMetadataInt(metadata, "peak_count", peak_count);
        dfuAddDynGroupExistingList(result, "metadata", metadata);
    }
    
    // Add peak data
    if (peak_count > 0) {
        DYN_LIST *peaks_x = dfuCreateDynList(DF_FLOAT, peak_count);
        DYN_LIST *peaks_y = dfuCreateDynList(DF_FLOAT, peak_count);
        DYN_LIST *peak_vals = dfuCreateDynList(DF_FLOAT, peak_count);
        
        if (peaks_x && peaks_y && peak_vals) {
            for (int i = 0; i < peak_count; i++) {
                dfuAddDynListFloat(peaks_x, (float)peaks[i].x);
                dfuAddDynListFloat(peaks_y, (float)peaks[i].y);
                dfuAddDynListFloat(peak_vals, (float)peak_values[i]);
            }
            strncpy(DYN_LIST_NAME(peaks_x), "peaks_x", DYN_LIST_NAME_SIZE-1);
            strncpy(DYN_LIST_NAME(peaks_y), "peaks_y", DYN_LIST_NAME_SIZE-1);
            strncpy(DYN_LIST_NAME(peak_vals), "peak_values", DYN_LIST_NAME_SIZE-1);
            
            dfuAddDynGroupExistingList(result, "peaks_x", peaks_x);
            dfuAddDynGroupExistingList(result, "peaks_y", peaks_y);
            dfuAddDynGroupExistingList(result, "peak_values", peak_vals);
        }
    }
    
    // Add grid info
    float grid_info_vals[6] = {
        (float)x_min, (float)x_max,
        (float)y_min, (float)y_max,
        (float)result_grid->width, (float)result_grid->height
    };
    dfuAddFloatListToGroup(result, "grid_info", grid_info_vals, 6);
    
    // Add grid data
    int total_cells = result_grid->width * result_grid->height;
    dfuAddDoubleListToGroup(result, "analysis_grid", result_grid->values, total_cells);
    
    // Cleanup
    free(peaks);
    free(peak_values);
    if (all_points) free(all_points);
    freeEnhancedTrajectories(trajectories, trajectory_count);
    freeKDEGrid(result_grid);
    
    // Put result in Tcl interpreter
    if (tclPutGroup(interp, result) != TCL_OK) {
        dfuFreeDynGroup(result);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}



/*
 * Tcl command: object_density
 * 
 * Creates a density grid from world objects (planks, obstacles)
 * 
 * Usage: object_density <object_list> ?options?
 * 
 * object_list format: list of objects, each object is:
 *   {center_x center_y width height rotation_rad}
 * 
 * Options:
 *   -grid_size N         Grid resolution (default: 100)
 *   -bandwidth BW        Gaussian bandwidth (default: 5.0)
 *   -bounds {x_min x_max y_min y_max}  Explicit bounds
 * 
 * Returns: DYN_GROUP with same format as trajectory_analyze
 *          (grid_info, analysis_grid)
 */
static int ObjectDensityCmd(ClientData clientData, Tcl_Interp *interp,
                           int objc, Tcl_Obj *const objv[]) {
    (void)clientData;
    
    // Default parameters
    int grid_size = 100;
    double bandwidth = 5.0;
    
    // Optional explicit bounds
    int use_explicit_bounds = 0;
    double explicit_x_min = 0.0, explicit_x_max = 0.0;
    double explicit_y_min = 0.0, explicit_y_max = 0.0;
    
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv,
                        "object_list ?-grid_size N? ?-bandwidth BW? "
                        "?-bounds {x_min x_max y_min y_max}?");
        return TCL_ERROR;
    }
    
    // Parse object list
    Tcl_Size obj_count;
    Tcl_Obj **obj_list;
    
    if (Tcl_ListObjGetElements(interp, objv[1], &obj_count, &obj_list) != TCL_OK) {
        return TCL_ERROR;
    }
    
    if (obj_count == 0) {
        Tcl_SetResult(interp, "Empty object list", TCL_STATIC);
        return TCL_ERROR;
    }
    
    // Allocate objects
    RotatedRect *objects = malloc(obj_count * sizeof(RotatedRect));
    if (!objects) {
        Tcl_SetResult(interp, "Memory allocation failed", TCL_STATIC);
        return TCL_ERROR;
    }
    
    // Parse each object
    for (Tcl_Size i = 0; i < obj_count; i++) {
        Tcl_Size elem_count;
        Tcl_Obj **elems;
        
        if (Tcl_ListObjGetElements(interp, obj_list[i], &elem_count, &elems) != TCL_OK) {
            free(objects);
            return TCL_ERROR;
        }
        
        if (elem_count != 5) {
            free(objects);
            Tcl_SetResult(interp, 
                         "Each object must be {center_x center_y width height rotation}",
                         TCL_STATIC);
            return TCL_ERROR;
        }
        
        // Parse object parameters
        if (Tcl_GetDoubleFromObj(interp, elems[0], &objects[i].center_x) != TCL_OK ||
            Tcl_GetDoubleFromObj(interp, elems[1], &objects[i].center_y) != TCL_OK ||
            Tcl_GetDoubleFromObj(interp, elems[2], &objects[i].width) != TCL_OK ||
            Tcl_GetDoubleFromObj(interp, elems[3], &objects[i].height) != TCL_OK ||
            Tcl_GetDoubleFromObj(interp, elems[4], &objects[i].rotation) != TCL_OK) {
            free(objects);
            return TCL_ERROR;
        }
    }
    
    // Parse options
    for (int i = 2; i < objc; i += 2) {
        if (i + 1 >= objc) {
            free(objects);
            Tcl_SetResult(interp, "Missing value for option", TCL_STATIC);
            return TCL_ERROR;
        }
        
        const char *option = Tcl_GetString(objv[i]);
        
        if (strcmp(option, "-grid_size") == 0) {
            if (Tcl_GetIntFromObj(interp, objv[i+1], &grid_size) != TCL_OK) {
                free(objects);
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-bandwidth") == 0) {
            if (Tcl_GetDoubleFromObj(interp, objv[i+1], &bandwidth) != TCL_OK) {
                free(objects);
                return TCL_ERROR;
            }
        } else if (strcmp(option, "-bounds") == 0) {
            Tcl_Size bounds_objc;
            Tcl_Obj **bounds_objv;
            
            if (Tcl_ListObjGetElements(interp, objv[i+1], &bounds_objc, &bounds_objv) != TCL_OK) {
                free(objects);
                return TCL_ERROR;
            }
            
            if (bounds_objc != 4) {
                free(objects);
                Tcl_SetResult(interp, "Bounds must be {x_min x_max y_min y_max}", TCL_STATIC);
                return TCL_ERROR;
            }
            
            if (Tcl_GetDoubleFromObj(interp, bounds_objv[0], &explicit_x_min) != TCL_OK ||
                Tcl_GetDoubleFromObj(interp, bounds_objv[1], &explicit_x_max) != TCL_OK ||
                Tcl_GetDoubleFromObj(interp, bounds_objv[2], &explicit_y_min) != TCL_OK ||
                Tcl_GetDoubleFromObj(interp, bounds_objv[3], &explicit_y_max) != TCL_OK) {
                free(objects);
                return TCL_ERROR;
            }
            
            use_explicit_bounds = 1;
        } else {
            free(objects);
            Tcl_AppendResult(interp, "Unknown option: ", option, NULL);
            return TCL_ERROR;
        }
    }
    
    // Compute bounding box
    double x_min, x_max, y_min, y_max;
    
    if (use_explicit_bounds) {
        x_min = explicit_x_min;
        x_max = explicit_x_max;
        y_min = explicit_y_min;
        y_max = explicit_y_max;
    } else {
        computeObjectBoundingBox(objects, obj_count, &x_min, &x_max, &y_min, &y_max);
        
        // Add padding
        double x_range = x_max - x_min;
        double y_range = y_max - y_min;
        x_min -= x_range * 0.1;
        x_max += x_range * 0.1;
        y_min -= y_range * 0.1;
        y_max += y_range * 0.1;
    }
    
    // Compute object density grid
    KDEGrid *grid = computeObjectDensity(objects, obj_count,
                                        grid_size, grid_size,
                                        x_min, x_max, y_min, y_max,
                                        bandwidth);
    
    free(objects);
    
    if (!grid) {
        Tcl_SetResult(interp, "Failed to compute object density", TCL_STATIC);
        return TCL_ERROR;
    }
    
    // Create result group (same format as trajectory_analyze)
    DYN_GROUP *result = dfuCreateGroup(5);
    if (!result) {
        freeKDEGrid(grid);
        Tcl_SetResult(interp, "Failed to create result group", TCL_STATIC);
        return TCL_ERROR;
    }
    
    // Add metadata
    DYN_LIST *metadata = dfuCreateMetadataList("metadata");
    if (metadata) {
        dfuAddMetadata(metadata, "analysis_type", "object_density");
        dfuAddMetadataInt(metadata, "object_count", obj_count);
        dfuAddMetadataDouble(metadata, "bandwidth", bandwidth);
        dfuAddDynGroupExistingList(result, "metadata", metadata);
    }
    
    // Add grid info
    float grid_info_vals[6] = {
        (float)x_min, (float)x_max,
        (float)y_min, (float)y_max,
        (float)grid->width, (float)grid->height
    };
    dfuAddFloatListToGroup(result, "grid_info", grid_info_vals, 6);
    
    // Add grid data
    int total_cells = grid->width * grid->height;
    dfuAddDoubleListToGroup(result, "analysis_grid", grid->values, total_cells);
    
    // Cleanup
    freeKDEGrid(grid);
    
    // Put result in Tcl interpreter
    if (tclPutGroup(interp, result) != TCL_OK) {
        dfuFreeDynGroup(result);
        return TCL_ERROR;
    }
    
    return TCL_OK;
}

/*
 * Package initialization function
 */
int DLLEXPORT Trajectory_analysis_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.5-", 0) == NULL) {
        return TCL_ERROR;
    }

    if (Tcl_PkgRequire(interp, "dlsh", "1.2", 0) == NULL) {
      return TCL_ERROR;
    }

    // Create commands
    Tcl_CreateObjCommand(interp, "trajectory_analyze",
                        TrajectoryAnalyzeCmd, NULL, NULL);
    
    // Create comparison command
    Tcl_CreateObjCommand(interp, "trajectory_compare",
                        TrajectoryCompareCmd, NULL, NULL);
    
    // Provide package
    if (Tcl_PkgProvide(interp, "trajectory_analysis",
                      TRAJECTORY_VERSION) != TCL_OK) {
        return TCL_ERROR;
    }
    // Create object density command
    Tcl_CreateObjCommand(interp, "object_density",
			 ObjectDensityCmd, NULL, NULL);
    
    return TCL_OK;
}

// For static linking
int DLLEXPORT Trajectory_analysis_SafeInit(Tcl_Interp *interp) {
    return Trajectory_analysis_Init(interp);
}
