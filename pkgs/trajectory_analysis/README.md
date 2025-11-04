# Trajectory Analysis - Tcl Extension

A high-performance Tcl extension for analyzing 2D trajectory data using Kernel Density Estimation (KDE). Designed for neuroscience experiments involving gravity-based ball trajectories, this module identifies collision hotspots, compares trajectory distributions, and provides uncertainty analysis.

## Overview

This package provides three Tcl commands for analyzing trajectory data from physics simulations (e.g., ball drops onto planks). It uses KDE to create heat maps showing where collisions are most likely to occur, with support for:

- **Turn detection** and analysis at trajectory inflection points
- **Spatial density estimation** using 2D Gaussian kernels
- **Directional variance** analysis for trajectory uncertainty
- **Comparative analysis** between different trajectory sets
- **Explicit boundary control** for reproducible comparisons

## Features

- **Three analysis modes**: density, uncertainty, and combined
- **Peak detection** to identify collision hotspots
- **Trajectory comparison** with difference/ratio heat maps and statistical correlation
- **Flexible parameterization**: grid size, bandwidths, angle thresholds
- **Explicit bounds** for consistent cross-experiment comparisons
- **Integration with DYN_LIST/DYN_GROUP** data structures

## Dependencies

- **Tcl 8.5+** - Core Tcl library
- **dlsh** - Dynamic list shell (DYN_LIST/DYN_GROUP infrastructure)
  - `df.h` - Data format definitions
  - `dynio.h` - Dynamic I/O operations
  - `dfana.h` - Data format analysis
  - `tcl_dl.h` - Tcl dynamic list interface
  - `dfu_helpers.h` - Helper utilities
- **Standard C libraries**: `math.h`, `stdlib.h`, `string.h`

## Building

### CMake (Recommended)

```bash
mkdir build && cd build
cmake ..
make
make install
```

### Manual Compilation

```bash
gcc -shared -fPIC -o trajectory_analysis.so trajectory_analysis.c \
    -I/usr/include/tcl8.6 \
    -I/path/to/dlsh/include \
    -L/path/to/dlsh/lib \
    -ltcl8.6 -ldlsh -lm
```

## Usage

### Loading the Package

```tcl
package require trajectory_analysis
```

### Commands

#### 1. `trajectory_analyze` - Basic KDE Analysis

Analyzes a single set of trajectories to find collision hotspots.

```tcl
trajectory_analyze traj_list ?options?
```

**Options:**
- `-grid_size N` - Heat map resolution (default: 100)
- `-bandwidth_x BW` - Horizontal smoothing bandwidth (default: 5.0)
- `-bandwidth_y BW` - Vertical smoothing bandwidth (default: 5.0)
- `-angle_threshold RAD` - Minimum angle for turn detection (default: 0.5)
- `-threshold VAL` - Peak detection threshold (default: 0.01)
- `-bounds {x_min x_max y_min y_max}` - Explicit analysis bounds

**Returns:** DYN_GROUP containing:
- `peaks_x`, `peaks_y` - Peak locations
- `peak_values` - Peak intensities
- `kde_grid` - Full heat map
- `grid_info` - Boundary information
- `metadata` - Analysis parameters

**Example:**
```tcl
# Load trajectory data into DYN_LIST
set trajectories [load_trajectory_data "experiment_01.dat"]

# Analyze with default parameters
set results [trajectory_analyze $trajectories]

# Analyze with custom parameters and explicit bounds
set results [trajectory_analyze $trajectories \
    -grid_size 150 \
    -bandwidth_x 3.0 \
    -bandwidth_y 3.0 \
    -angle_threshold 0.3 \
    -bounds {0 100 0 50}]

# Extract peaks
set peaks_x [dyn_group_get $results peaks_x]
set peaks_y [dyn_group_get $results peaks_y]
```

#### 2. `trajectory_analyze_enhanced` - Advanced Analysis with Uncertainty

Provides enhanced analysis including directional variance and trajectory uncertainty.

```tcl
trajectory_analyze_enhanced traj_list ?options?
```

**Options:**
- All options from `trajectory_analyze`, plus:
- `-variance_radius R` - Radius for variance computation (default: 2.0)
- `-variance_weight W` - Weight of variance in combined mode (default: 2.0)
- `-mode MODE` - Analysis mode: `density`, `uncertainty`, or `combined` (default: `combined`)

**Modes:**
- `density` - Traditional KDE of turn points
- `uncertainty` - Directional variance heat map
- `combined` - Variance-weighted density estimation

**Returns:** DYN_GROUP with same structure as `trajectory_analyze`, plus:
- Additional metadata about analysis mode and variance parameters

**Example:**
```tcl
# Density-only analysis
set density_results [trajectory_analyze_enhanced $trajectories -mode density]

# Uncertainty-only analysis
set uncertainty_results [trajectory_analyze_enhanced $trajectories -mode uncertainty]

# Combined analysis with custom parameters
set combined_results [trajectory_analyze_enhanced $trajectories \
    -mode combined \
    -variance_radius 3.0 \
    -variance_weight 1.5 \
    -bounds {0 100 0 50}]
```

#### 3. `trajectory_compare` - Comparative Analysis

Compares two sets of trajectories and computes statistical differences.

```tcl
trajectory_compare traj_list1 traj_list2 ?options?
```

**Options:**
- `-grid_size N` - Heat map resolution (default: 100)
- `-bandwidth_x BW` - Horizontal smoothing bandwidth (default: 5.0)
- `-bandwidth_y BW` - Vertical smoothing bandwidth (default: 5.0)
- `-angle_threshold RAD` - Minimum angle for turn detection (default: 0.5)
- `-comparison_mode MODE` - Comparison type: `difference`, `ratio`, or `both` (default: `both`)
- `-significance_thresh VAL` - Threshold for significant differences (default: 0.01)
- `-bounds {x_min x_max y_min y_max}` - Explicit analysis bounds

**Returns:** DYN_GROUP containing:
- `kde_grid_1`, `kde_grid_2` - Heat maps for each trajectory set
- `difference_grid` - Pointwise difference (set1 - set2)
- `ratio_grid` - Pointwise ratio (set1 / set2)
- `correlation` - Spatial correlation coefficient
- `mean_difference` - Mean absolute difference
- `max_difference` - Maximum absolute difference
- `significant_cells` - Count of significantly different cells
- `grid_info` - Boundary information
- `metadata` - Comparison parameters

**Example:**
```tcl
# Load two trajectory sets
set traj_control [load_trajectory_data "control.dat"]
set traj_experimental [load_trajectory_data "experimental.dat"]

# Compare with default parameters
set comparison [trajectory_compare $traj_control $traj_experimental]

# Compare with explicit bounds (ensures same coordinate system)
set comparison [trajectory_compare $traj_control $traj_experimental \
    -comparison_mode both \
    -significance_thresh 0.05 \
    -bounds {0 100 0 50}]

# Extract comparison metrics
set correlation [dyn_list_get $comparison metadata correlation]
set mean_diff [dyn_list_get $comparison metadata mean_difference]

puts "Spatial correlation: $correlation"
puts "Mean difference: $mean_diff"
```

## Data Format

### Input: Trajectories as DYN_LIST

Trajectories must be provided as a DYN_LIST containing lists of lists:

```
DYN_LIST [
    Trajectory_1 [
        [x1, x2, x3, ...]  # X coordinates
        [y1, y2, y3, ...]  # Y coordinates
    ]
    Trajectory_2 [
        [x1, x2, x3, ...]
        [y1, y2, y3, ...]
    ]
    ...
]
```

### Output: Results as DYN_GROUP

Results are returned as a DYN_GROUP containing multiple DYN_LISTs with named fields. Access using standard DYN_GROUP/DYN_LIST operations.

## Explicit Bounds

The `-bounds` option ensures consistent analysis regions across multiple experiments:

```tcl
# Without explicit bounds - auto-computed with 10% padding
set results1 [trajectory_analyze $traj1]
set results2 [trajectory_analyze $traj2]
# Results may have different coordinate systems!

# With explicit bounds - guaranteed same coordinate system
set bounds {0 100 0 50}
set results1 [trajectory_analyze $traj1 -bounds $bounds]
set results2 [trajectory_analyze $traj2 -bounds $bounds]
# Results are directly comparable
```

This is especially important for:
- **Trajectory comparison** across conditions
- **Temporal analysis** of the same experiment over time
- **Batch processing** of multiple trials
- **Reproducible research** with consistent parameters

## Algorithm Details

### Turn Detection

Trajectories are analyzed to find "turn points" where the direction changes by more than `angle_threshold`. These points typically correspond to ball collisions with planks.

### Kernel Density Estimation

Uses 2D Gaussian kernels to create smooth heat maps:

```
K(x, y) = (1 / (2π σ_x σ_y)) * exp(-(x²/2σ_x² + y²/2σ_y²))
```

Where `σ_x = bandwidth_x` and `σ_y = bandwidth_y`.

### Directional Variance

Computes circular variance of trajectory directions within a local neighborhood, measuring trajectory uncertainty.

### Peak Detection

Identifies local maxima in the KDE grid above the specified threshold, representing collision hotspots.

## Performance Considerations

- **Grid size**: Higher resolution = better detail but slower computation
- **Bandwidth**: Larger values = smoother heat maps but may obscure fine details
- **Trajectory count**: Linear scaling with number of trajectories
- **Memory**: Primarily determined by grid size (width × height × 8 bytes per cell)

## Version History

### v1.1 (Current)
- Added explicit bounds support for all three commands
- Enhanced comparison capabilities
- Improved documentation

### v1.0
- Initial release
- Basic KDE analysis
- Enhanced directional variance analysis
- Trajectory comparison

## License

MIT License - see [LICENSE](LICENSE) file for details.

Copyright (c) 2025 David Sheinberg, Brown University


*(Add your license information here)*

## Citation

If you use this software in your research, please cite:

*(Add citation information here)*

## Authors

DLS + Claude.io

