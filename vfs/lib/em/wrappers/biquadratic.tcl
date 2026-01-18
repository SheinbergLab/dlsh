#!/usr/bin/env tclsh

package require math::linearalgebra
package require math::constants

# Biquadratic 2D Transform Implementation
# Uses tcllib math packages for linear algebra operations

namespace eval biquadratic {
    namespace import ::math::linearalgebra::*
    
    # Create the design matrix for biquadratic fit
    # Terms: 1, x, y, x², y², xy, x²y, xy², x²y²
    proc create_design_matrix {x_coords y_coords} {
        set n [llength $x_coords]
        set A {}
        
        for {set i 0} {$i < $n} {incr i} {
            set x [lindex $x_coords $i]
            set y [lindex $y_coords $i]
            
            set x2 [expr {$x * $x}]
            set y2 [expr {$y * $y}]
            set xy [expr {$x * $y}]
            set x2y [expr {$x2 * $y}]
            set xy2 [expr {$x * $y2}]
            set x2y2 [expr {$x2 * $y2}]
            
            # Row: [1, x, y, x², y², xy, x²y, xy², x²y²]
            lappend A [list 1.0 $x $y $x2 $y2 $xy $x2y $xy2 $x2y2]
        }
        
        return $A
    }
    
    # Fit single biquadratic surface to data points
    proc fit_single {x_coords y_coords z_values} {
        set n [llength $x_coords]
        
        # Validate input
        if {$n != [llength $y_coords] || $n != [llength $z_values]} {
            error "Input arrays must have same length"
        }
        
        if {$n < 9} {
            error "Need at least 9 points for biquadratic fit"
        }
        
        # Create design matrix
        set A [create_design_matrix $x_coords $y_coords]
        
        # Convert z_values to column vector
        set z_vector {}
        foreach z $z_values {
            lappend z_vector [list $z]
        }
        
        # Solve A * coeffs = z using linear algebra
        # For overdetermined system, use least squares: coeffs = (A'A)^-1 * A' * z
        if {$n > 9} {
            set At [transpose $A]
            set AtA [matmul $At $A]
            set Atz [matmul $At $z_vector]
            set coeffs [solveGauss $AtA $Atz]
        } else {
            # Exactly determined system
            set coeffs [solveGauss $A $z_vector]
        }
        
        # Extract coefficients from column vector
        set coeff_list {}
        foreach row $coeffs {
            lappend coeff_list [lindex $row 0]
        }
        
        return $coeff_list
    }
    
    # Fit biquadratic transformation (x and y corrections)
    proc fit {grid_x_coords grid_y_coords uncal_x_values uncal_y_values} {
        # Fit transformation for x correction
        set x_coeffs [fit_single $grid_x_coords $grid_y_coords $uncal_x_values]
        
        # Fit transformation for y correction  
        set y_coeffs [fit_single $grid_x_coords $grid_y_coords $uncal_y_values]
        
        return [list $x_coeffs $y_coeffs]
    }
    
    # Evaluate biquadratic surface at given point
    proc evaluate {coeffs x y} {
        lassign $coeffs a0 a1 a2 a3 a4 a5 a6 a7 a8
        
        set x2 [expr {$x * $x}]
        set y2 [expr {$y * $y}]
        set xy [expr {$x * $y}]
        set x2y [expr {$x2 * $y}]
        set xy2 [expr {$x * $y2}]
        set x2y2 [expr {$x2 * $y2}]
        
        set result [expr {$a0 + $a1*$x + $a2*$y + $a3*$x2 + $a4*$y2 + 
                         $a5*$xy + $a6*$x2y + $a7*$xy2 + $a8*$x2y2}]
        
        return $result
    }
    
    # Apply transformation to convert coordinates
    proc transform {coeffs grid_x grid_y} {
        lassign $coeffs x_coeffs y_coeffs
        
        set cal_x [evaluate $x_coeffs $grid_x $grid_y]
        set cal_y [evaluate $y_coeffs $grid_x $grid_y]
        
        return [list $cal_x $cal_y]
    }
    
    # Create a standard 3x3 grid with normalized coordinates
    proc create_3x3_grid {} {
        set x_coords {}
        set y_coords {}
        
        for {set i -1} {$i <= 1} {incr i} {
            for {set j -1} {$j <= 1} {incr j} {
                lappend x_coords $j
                lappend y_coords $i
            }
        }
        
        return [list $x_coords $y_coords]
    }
    
    # Print transformation coefficients
    proc print_coefficients {coeffs} {
        set terms {1 x y x² y² xy x²y xy² x²y²}
        lassign $coeffs x_coeffs y_coeffs
        
        puts "Biquadratic Transformation Coefficients:"
        puts "cal_x = a₀ + a₁x + a₂y + a₃x² + a₄y² + a₅xy + a₆x²y + a₇xy² + a₈x²y²"
        puts "cal_y = b₀ + b₁x + b₂y + b₃x² + b₄y² + b₅xy + b₆x²y + b₇xy² + b₈x²y²"
        puts ""
        
        puts "X-transformation coefficients:"
        for {set i 0} {$i < [llength $x_coeffs]} {incr i} {
            set coeff [lindex $x_coeffs $i]
            set term [lindex $terms $i]
            puts [format "a%d (%s): %12.6f" $i $term $coeff]
        }
        
        puts ""
        puts "Y-transformation coefficients:"
        for {set i 0} {$i < [llength $y_coeffs]} {incr i} {
            set coeff [lindex $y_coeffs $i]
            set term [lindex $terms $i]
            puts [format "b%d (%s): %12.6f" $i $term $coeff]
        }
    }
    
    # Calculate RMS error for transformation
    proc calculate_rms_error {coeffs grid_x_coords grid_y_coords uncal_x_values uncal_y_values} {
        lassign $coeffs x_coeffs y_coeffs
        
        set sum_sq_error_x 0.0
        set sum_sq_error_y 0.0
        set n [llength $grid_x_coords]
        
        for {set i 0} {$i < $n} {incr i} {
            set grid_x [lindex $grid_x_coords $i]
            set grid_y [lindex $grid_y_coords $i]
            set uncal_x_actual [lindex $uncal_x_values $i]
            set uncal_y_actual [lindex $uncal_y_values $i]
            
            set uncal_x_predicted [evaluate $x_coeffs $grid_x $grid_y]
            set uncal_y_predicted [evaluate $y_coeffs $grid_x $grid_y]
            
            set error_x [expr {$uncal_x_actual - $uncal_x_predicted}]
            set error_y [expr {$uncal_y_actual - $uncal_y_predicted}]
            
            set sum_sq_error_x [expr {$sum_sq_error_x + $error_x * $error_x}]
            set sum_sq_error_y [expr {$sum_sq_error_y + $error_y * $error_y}]
        }
        
        set rms_x [expr {sqrt($sum_sq_error_x / $n)}]
        set rms_y [expr {sqrt($sum_sq_error_y / $n)}]
        
        return [list $rms_x $rms_y]
    }
}

package require math::linearalgebra
package require math::constants

# Stampe 1993 Biquadratic 2D Transform Implementation
# Compatible with biquadratic.tcl interface but using reduced polynomial + quadrant corrections
# Based on: Stampe, 1993, Heuristic filtering and reliable calibration methods 
# for video-based pupil-tracking systems

namespace eval stampe_biquadratic {
    namespace import ::math::linearalgebra::*
    
    # Create the design matrix for reduced biquadratic fit (Stampe 1993)
    # Uses only first 4 terms: x, y, x², y² (no constant term or cross terms in base fit)
    proc create_design_matrix {x_coords y_coords} {
        set n [llength $x_coords]
        set A {}
        
        for {set i 0} {$i < $n} {incr i} {
            set x [lindex $x_coords $i]
            set y [lindex $y_coords $i]
            
            set x2 [expr {$x * $x}]
            set y2 [expr {$y * $y}]
            
            # Row: [x, y, x², y²] - note: no constant term in Stampe method
            lappend A [list $x $y $x2 $y2]
        }
        
        return $A
    }
    
    # Fit transformation following the emcalib.tcl pattern exactly
    # This creates a transformation that maps FROM uncalibrated coordinates TO grid coordinates
    # Just like the original biquadratic, but using Stampe 1993 method
    proc fit {uncal_x_values uncal_y_values grid_x_coords grid_y_coords} {
        set n [llength $grid_x_coords]
        
        # Validate input
        if {$n != [llength $grid_y_coords] || $n != [llength $uncal_x_values] || $n != [llength $uncal_y_values]} {
            error "Input arrays must have same length"
        }
        
        if {$n < 9} {
            error "Need at least 9 points for Stampe biquadratic fit"
        }
        
        # Step 1: Find center point and compute offsets (like emcalib.tcl)
        set center_idx -1
        for {set i 0} {$i < $n} {incr i} {
            set gx [lindex $grid_x_coords $i]
            set gy [lindex $grid_y_coords $i]
            if {abs($gx) < 0.001 && abs($gy) < 0.001} {
                set center_idx $i
                break
            }
        }
        
        if {$center_idx == -1} {
            error "No center point (0,0) found in calibration data"
        }
        
        # Compute offsets from center point uncalibrated values
        set xoff [expr {-1.0 * [lindex $uncal_x_values $center_idx]}]
        set yoff [expr {-1.0 * [lindex $uncal_y_values $center_idx]}]
        
        # Step 2: Select cardinal points (first 4 non-center) for base polynomial
        set cardinal_indices {}
        for {set i 0} {$i < $n} {incr i} {
            set gx [lindex $grid_x_coords $i]
            set gy [lindex $grid_y_coords $i]
            # Cardinal points: either x or y is zero, but not both
            if {($i != $center_idx) && ([llength $cardinal_indices] < 4)} {
                lappend cardinal_indices $i
            }
        }
        
        # Extract data for the 4 cardinal points and apply offset
        set cardinal_uncal_x {}
        set cardinal_uncal_y {}
        set cardinal_grid_x {}
        set cardinal_grid_y {}
        
        foreach idx $cardinal_indices {
            lappend cardinal_uncal_x [expr {[lindex $uncal_x_values $idx] + $xoff}]
            lappend cardinal_uncal_y [expr {[lindex $uncal_y_values $idx] + $yoff}]
            lappend cardinal_grid_x [lindex $grid_x_coords $idx]
            lappend cardinal_grid_y [lindex $grid_y_coords $idx]
        }
        
        # Step 3: Solve for base coefficients (uncal -> grid)
        # For X: grid_x = b*uncal_x + c*uncal_y + d*uncal_x² + e*uncal_y²
        set A_x [create_design_matrix $cardinal_uncal_x $cardinal_uncal_y]
        set grid_x_vector {}
        foreach gx $cardinal_grid_x {
            lappend grid_x_vector [list $gx]
        }
        set x_coeffs_raw [solveGauss $A_x $grid_x_vector]
        set coeffs_bcde {}
        foreach row $x_coeffs_raw {
            lappend coeffs_bcde [lindex $row 0]
        }
        
        # For Y: grid_y = g*uncal_x + h*uncal_y + i*uncal_x² + j*uncal_y²
        set A_y [create_design_matrix $cardinal_uncal_x $cardinal_uncal_y]
        set grid_y_vector {}
        foreach gy $cardinal_grid_y {
            lappend grid_y_vector [list $gy]
        }
        set y_coeffs_raw [solveGauss $A_y $grid_y_vector]
        set coeffs_ghij {}
        foreach row $y_coeffs_raw {
            lappend coeffs_ghij [lindex $row 0]
        }
        
        # Step 4: Compute quadrant corrections using corner points (last 4 points)
        set corner_indices {}
        set corner_count 0
        for {set i 0} {$i < $n} {incr i} {
            set gx [lindex $grid_x_coords $i]
            set gy [lindex $grid_y_coords $i]
            # Corner points: both gx and gy are non-zero
            if {abs($gx) > 0.5 && abs($gy) > 0.5} {
                lappend corner_indices $i
                incr corner_count
                if {$corner_count >= 4} break
            }
        }
        
        # Calculate predicted values for corners and compute m,n corrections
        set corner_errors_x {}
        set corner_errors_y {}
        set corner_quadrants {}
        set corner_xy_products {}
        
        foreach idx $corner_indices {
            set uncal_x [expr {[lindex $uncal_x_values $idx] + $xoff}]
            set uncal_y [expr {[lindex $uncal_y_values $idx] + $yoff}]
            set actual_grid_x [lindex $grid_x_coords $idx]
            set actual_grid_y [lindex $grid_y_coords $idx]
            
            # Predict grid coordinates using base polynomial
            lassign $coeffs_bcde b c d e
            lassign $coeffs_ghij g h i j
            
            set pred_grid_x [expr {$b*$uncal_x + $c*$uncal_y + $d*$uncal_x*$uncal_x + $e*$uncal_y*$uncal_y}]
            set pred_grid_y [expr {$g*$uncal_x + $h*$uncal_y + $i*$uncal_x*$uncal_x + $j*$uncal_y*$uncal_y}]
            
            # Determine quadrant
            set quadrant -1
            if {$pred_grid_x < 0 && $pred_grid_y >= 0} { set quadrant 0 }
            if {$pred_grid_x >= 0 && $pred_grid_y >= 0} { set quadrant 1 }
            if {$pred_grid_x < 0 && $pred_grid_y < 0} { set quadrant 2 }
            if {$pred_grid_x >= 0 && $pred_grid_y < 0} { set quadrant 3 }
            
            set error_x [expr {$actual_grid_x - $pred_grid_x}]
            set error_y [expr {$actual_grid_y - $pred_grid_y}]
            set xy_product [expr {$pred_grid_x * $pred_grid_y}]
            
            lappend corner_errors_x $error_x
            lappend corner_errors_y $error_y
            lappend corner_quadrants $quadrant
            lappend corner_xy_products $xy_product
        }
        
        # Compute m and n coefficients for each quadrant
        set coeffs_m {0.0 0.0 0.0 0.0}
        set coeffs_n {0.0 0.0 0.0 0.0}
        
        for {set q 0} {$q < 4} {incr q} {
            set m_sum 0.0
            set n_sum 0.0
            set count 0
            
            for {set i 0} {$i < [llength $corner_quadrants]} {incr i} {
                if {[lindex $corner_quadrants $i] == $q} {
                    set xy_prod [lindex $corner_xy_products $i]
                    if {abs($xy_prod) > 1e-10} {
                        set m_val [expr {[lindex $corner_errors_x $i] / $xy_prod}]
                        set n_val [expr {[lindex $corner_errors_y $i] / $xy_prod}]
                        set m_sum [expr {$m_sum + $m_val}]
                        set n_sum [expr {$n_sum + $n_val}]
                        incr count
                    }
                }
            }
            
            if {$count > 0} {
                lset coeffs_m $q [expr {$m_sum / $count}]
                lset coeffs_n $q [expr {$n_sum / $count}]
            }
        }
        
        # Store Stampe-specific data
        set stampe_data [dict create \
                            xoff $xoff \
                            yoff $yoff \
                            coeffs_bcde $coeffs_bcde \
                            coeffs_ghij $coeffs_ghij \
                            coeffs_m $coeffs_m \
                            coeffs_n $coeffs_n]
        
        return $stampe_data
    }
    
    # Transform uncalibrated coordinates to grid coordinates using Stampe method
    proc transform_uncal_to_grid {stampe_data uncal_x uncal_y} {
        dict with stampe_data {
            # Apply offsets
            set adj_uncal_x [expr {$uncal_x + $xoff}]
            set adj_uncal_y [expr {$uncal_y + $yoff}]
            
            # Base polynomial transformation
            lassign $coeffs_bcde b c d e
            lassign $coeffs_ghij g h i j
            
            set base_grid_x [expr {$b*$adj_uncal_x + $c*$adj_uncal_y + $d*$adj_uncal_x*$adj_uncal_x + $e*$adj_uncal_y*$adj_uncal_y}]
            set base_grid_y [expr {$g*$adj_uncal_x + $h*$adj_uncal_y + $i*$adj_uncal_x*$adj_uncal_x + $j*$adj_uncal_y*$adj_uncal_y}]
            
            # Determine quadrant and apply correction
            set quadrant -1
            if {$base_grid_x < 0 && $base_grid_y >= 0} { set quadrant 0 }
            if {$base_grid_x >= 0 && $base_grid_y >= 0} { set quadrant 1 }
            if {$base_grid_x < 0 && $base_grid_y < 0} { set quadrant 2 }
            if {$base_grid_x >= 0 && $base_grid_y < 0} { set quadrant 3 }
            
            set correction_x 0.0
            set correction_y 0.0
            
            if {$quadrant >= 0} {
                set xy_product [expr {$base_grid_x * $base_grid_y}]
                set m_coeff [lindex $coeffs_m $quadrant]
                set n_coeff [lindex $coeffs_n $quadrant]
                set correction_x [expr {$m_coeff * $xy_product}]
                set correction_y [expr {$n_coeff * $xy_product}]
            }
            
            set final_grid_x [expr {$base_grid_x + $correction_x}]
            set final_grid_y [expr {$base_grid_y + $correction_y}]
            
            return [list $final_grid_x $final_grid_y]
        }
    }


    proc drift_correct {params raw_x raw_y} {
	# User is looking at center (0, 0)
	# We want: transform(raw_x, raw_y) -> (0, 0)
	
	# Extract current parameters
	lassign $params old_offset_x old_offset_y b c d e g h i j \
	    m0 m1 m2 m3 n0 n1 n2 n3
	
	# Apply current offset
	set adj_x [expr {$raw_x + $old_offset_x}]
	set adj_y [expr {$raw_y + $old_offset_y}]
	
	# Compute base polynomial (what we'd get with current offsets)
	set adj_x2 [expr {$adj_x * $adj_x}]
	set adj_y2 [expr {$adj_y * $adj_y}]
	set base_x [expr {$b*$adj_x + $c*$adj_y + $d*$adj_x2 + $e*$adj_y2}]
	set base_y [expr {$g*$adj_x + $h*$adj_y + $i*$adj_x2 + $j*$adj_y2}]
	
	# New offset needed to make base_x and base_y equal to zero
	# We want: b*(raw_x + new_offset_x) + ... = 0
	# Simple approximation (assumes linear dominance near center):
	set delta_offset_x [expr {-$base_x / $b}]
	set delta_offset_y [expr {-$base_y / $h}]
	
	# Update offsets
	set new_offset_x [expr {$old_offset_x + $delta_offset_x}]
	set new_offset_y [expr {$old_offset_y + $delta_offset_y}]
	
	# Return updated parameters
	return [list $new_offset_x $new_offset_y $b $c $d $e $g $h $i $j \
		    $m0 $m1 $m2 $m3 $n0 $n1 $n2 $n3]
    }
    
    
    # Evaluate biquadratic surface at given point (compatible interface)
    # Note: This should transform FROM uncalibrated TO grid coordinates
    proc evaluate {coeffs uncal_x uncal_y} {
        # Check if this is Stampe format (has 3 elements)
        if {[llength $coeffs] == 3} {
            lassign $coeffs x_coeffs y_coeffs stampe_data
            # Transform uncalibrated to grid coordinates
            lassign [transform_uncal_to_grid $stampe_data $uncal_x $uncal_y] grid_x grid_y
            return $grid_x  ; # Return X coordinate by default
        } else {
            # Fall back to standard biquadratic evaluation
            lassign $coeffs a0 a1 a2 a3 a4 a5 a6 a7 a8
            
            set x2 [expr {$uncal_x * $uncal_x}]
            set y2 [expr {$uncal_y * $uncal_y}]
            set xy [expr {$uncal_x * $uncal_y}]
            set x2y [expr {$x2 * $uncal_y}]
            set xy2 [expr {$uncal_x * $y2}]
            set x2y2 [expr {$x2 * $y2}]
            
            set result [expr {$a0 + $a1*$uncal_x + $a2*$uncal_y + $a3*$x2 + $a4*$y2 + 
                             $a5*$xy + $a6*$x2y + $a7*$xy2 + $a8*$x2y2}]
            
            return $result
        }
    }
    
    # Apply transformation - this should be INVERSE of fitting direction
    # Since fit creates uncal->grid, transform should do grid->uncal for compatibility
    proc transform {coeffs grid_x grid_y} {
        # This is the tricky part - we need to invert the transformation
        # For now, we'll implement a simple iterative solver
        
        if {[llength $coeffs] == 3} {
            lassign $coeffs x_coeffs y_coeffs stampe_data
            
            # Initial guess: assume linear relationship
            set uncal_x $grid_x
            set uncal_y $grid_y
            
            # Newton-Raphson iteration to find uncal coordinates that produce grid coordinates
            for {set iter 0} {$iter < 10} {incr iter} {
                lassign [transform_uncal_to_grid $stampe_data $uncal_x $uncal_y] pred_grid_x pred_grid_y
                
                set error_x [expr {$grid_x - $pred_grid_x}]
                set error_y [expr {$grid_y - $pred_grid_y}]
                
                # Check convergence
                if {abs($error_x) < 1e-6 && abs($error_y) < 1e-6} {
                    break
                }
                
                # Simple gradient step (could be improved with Jacobian)
                set step_size 0.5
                set uncal_x [expr {$uncal_x + $step_size * $error_x}]
                set uncal_y [expr {$uncal_y + $step_size * $error_y}]
            }
            
            return [list $uncal_x $uncal_y]
        } else {
            # Fall back to standard biquadratic transform
            lassign $coeffs x_coeffs y_coeffs
            
            # This would also need inverse solving for true compatibility
            # For now, assume direct evaluation
            set cal_x [evaluate $x_coeffs $grid_x $grid_y]
            set cal_y [evaluate $y_coeffs $grid_x $grid_y]
            
            return [list $cal_x $cal_y]
        }
    }
    
    # Create a standard 3x3 grid with normalized coordinates
    proc create_3x3_grid {} {
        set x_coords {}
        set y_coords {}
        
        for {set i -1} {$i <= 1} {incr i} {
            for {set j -1} {$j <= 1} {incr j} {
                lappend x_coords $j
                lappend y_coords $i
            }
        }
        
        return [list $x_coords $y_coords]
    }
    
    # Print transformation coefficients (compatible interface)
    proc print_coefficients {coeffs} {
        if {[llength $coeffs] == 3} {
            lassign $coeffs x_coeffs y_coeffs stampe_data
            dict with stampe_data {
                puts "Stampe 1993 Biquadratic Transformation Coefficients:"
                puts "Method: Reduced polynomial + quadrant corrections"
                puts "Direction: Uncalibrated -> Grid coordinates"
                puts ""
                puts "Base polynomial: grid = b*uncal_x + c*uncal_y + d*uncal_x² + e*uncal_y²"
                puts "Quadrant correction: += m[q] * h * v (where q = quadrant)"
                puts ""
                
                puts "X-transformation (uncal -> grid_x):"
                puts [format "  X Offset: %12.6f" $xoff]
                puts [format "  b (uncal_x):  %12.6f" [lindex $coeffs_bcde 0]]
                puts [format "  c (uncal_y):  %12.6f" [lindex $coeffs_bcde 1]]
                puts [format "  d (uncal_x²): %12.6f" [lindex $coeffs_bcde 2]]
                puts [format "  e (uncal_y²): %12.6f" [lindex $coeffs_bcde 3]]
                puts "  Quadrant corrections (m):"
                for {set i 0} {$i < 4} {incr i} {
                    puts [format "    m[%d]: %12.6f" $i [lindex $coeffs_m $i]]
                }
                
                puts ""
                puts "Y-transformation (uncal -> grid_y):"
                puts [format "  Y Offset: %12.6f" $yoff]
                puts [format "  g (uncal_x):  %12.6f" [lindex $coeffs_ghij 0]]
                puts [format "  h (uncal_y):  %12.6f" [lindex $coeffs_ghij 1]]
                puts [format "  i (uncal_x²): %12.6f" [lindex $coeffs_ghij 2]]
                puts [format "  j (uncal_y²): %12.6f" [lindex $coeffs_ghij 3]]
                puts "  Quadrant corrections (n):"
                for {set i 0} {$i < 4} {incr i} {
                    puts [format "    n[%d]: %12.6f" $i [lindex $coeffs_n $i]]
                }
            }
        } else {
            puts "Standard biquadratic coefficients (use biquadratic::print_coefficients)"
        }
    }
    
    # Calculate RMS error for transformation (compatible interface)
    proc calculate_rms_error {coeffs grid_x_coords grid_y_coords uncal_x_values uncal_y_values} {
        set sum_sq_error_x 0.0
        set sum_sq_error_y 0.0
        set n [llength $grid_x_coords]
        
        if {[llength $coeffs] == 3} {
            lassign $coeffs x_coeffs y_coeffs stampe_data
            
            for {set i 0} {$i < $n} {incr i} {
                set uncal_x [lindex $uncal_x_values $i]
                set uncal_y [lindex $uncal_y_values $i]
                set actual_grid_x [lindex $grid_x_coords $i]
                set actual_grid_y [lindex $grid_y_coords $i]
                
                # Transform uncalibrated to grid coordinates and compare
                lassign [transform_uncal_to_grid $stampe_data $uncal_x $uncal_y] predicted_grid_x predicted_grid_y
                
                set error_x [expr {$actual_grid_x - $predicted_grid_x}]
                set error_y [expr {$actual_grid_y - $predicted_grid_y}]
                
                set sum_sq_error_x [expr {$sum_sq_error_x + $error_x * $error_x}]
                set sum_sq_error_y [expr {$sum_sq_error_y + $error_y * $error_y}]
            }
        } else {
            # Standard biquadratic error calculation
            for {set i 0} {$i < $n} {incr i} {
                set grid_x [lindex $grid_x_coords $i]
                set grid_y [lindex $grid_y_coords $i]
                set uncal_x_actual [lindex $uncal_x_values $i]
                set uncal_y_actual [lindex $uncal_y_values $i]
                
                lassign [transform $coeffs $grid_x $grid_y] uncal_x_predicted uncal_y_predicted
                
                set error_x [expr {$uncal_x_actual - $uncal_x_predicted}]
                set error_y [expr {$uncal_y_actual - $uncal_y_predicted}]
                
                set sum_sq_error_x [expr {$sum_sq_error_x + $error_x * $error_x}]
                set sum_sq_error_y [expr {$sum_sq_error_y + $error_y * $error_y}]
            }
        }
        
        set rms_x [expr {sqrt($sum_sq_error_x / $n)}]
        set rms_y [expr {sqrt($sum_sq_error_y / $n)}]
        
        return [list $rms_x $rms_y]
    }
}

# Example usage comparing both methods
proc compare_methods {} {
    # Create a 3x3 grid
    lassign [stampe_biquadratic::create_3x3_grid] grid_x_coords grid_y_coords
    
    # Example uncalibrated measurements at each grid point
    set uncal_x_values {}
    set uncal_y_values {}
    
    # Simulate some calibration data with distortion
    foreach gx $grid_x_coords gy $grid_y_coords {
        # Simulate uncalibrated readings with some nonlinear distortion
        set uncal_x [expr {$gx + 0.1*$gx*$gx + 0.05*$gx*$gy + 0.02*rand()}]
        set uncal_y [expr {$gy + 0.1*$gy*$gy + 0.03*$gx*$gy + 0.02*rand()}]
        
        lappend uncal_x_values $uncal_x
        lappend uncal_y_values $uncal_y
    }
    
    puts "Comparison of Biquadratic Methods"
    puts "================================="
    puts ""
    
    # Print input data
    puts "Grid Reference Points and Uncalibrated Measurements:"
    puts [format "%s %8s %8s %12s %12s" "Point" "Grid_X" "Grid_Y" "Uncal_X" "Uncal_Y"]
    puts [string repeat "-" 50]
    for {set i 0} {$i < [llength $grid_x_coords]} {incr i} {
        puts [format "%5d %8.1f %8.1f %12.6f %12.6f" \
              $i [lindex $grid_x_coords $i] [lindex $grid_y_coords $i] \
              [lindex $uncal_x_values $i] [lindex $uncal_y_values $i]]
    }
    puts ""
    
    # Fit using Stampe 1993 method
    puts "Fitting with Stampe 1993 method..."
    set stampe_coeffs [stampe_biquadratic::fit $grid_x_coords $grid_y_coords $uncal_x_values $uncal_y_values]
    
    puts ""
    stampe_biquadratic::print_coefficients $stampe_coeffs
    
    # Calculate fit quality for Stampe method
    lassign [stampe_biquadratic::calculate_rms_error $stampe_coeffs $grid_x_coords $grid_y_coords $uncal_x_values $uncal_y_values] rms_x_stampe rms_y_stampe
    puts ""
    puts [format "Stampe 1993 RMS Error - X: %.6f, Y: %.6f" $rms_x_stampe $rms_y_stampe]
    
    # Test forward transformation (uncal -> grid) at grid points
    puts ""
    puts "Verification - Transform uncalibrated coordinates to grid:"
    puts [format "%s %12s %12s %8s %8s %8s %8s" "Point" "Uncal_X" "Uncal_Y" "Pred_Gx" "Pred_Gy" "Error_X" "Error_Y"]
    puts [string repeat "-" 70]
    
    lassign $stampe_coeffs x_coeffs y_coeffs stampe_data
    for {set i 0} {$i < [llength $grid_x_coords]} {incr i} {
        set uncal_x [lindex $uncal_x_values $i]
        set uncal_y [lindex $uncal_y_values $i]
        set actual_gx [lindex $grid_x_coords $i]
        set actual_gy [lindex $grid_y_coords $i]
        
        lassign [stampe_biquadratic::transform_uncal_to_grid $stampe_data $uncal_x $uncal_y] pred_gx pred_gy
        
        set err_x [expr {$actual_gx - $pred_gx}]
        set err_y [expr {$actual_gy - $pred_gy}]
        
        puts [format "%5d %12.6f %12.6f %8.3f %8.3f %8.6f %8.6f" \
              $i $uncal_x $uncal_y $pred_gx $pred_gy $err_x $err_y]
    }
    
    # Test inverse transformation at an intermediate point
    set test_gx 0.5
    set test_gy -0.3
    lassign [stampe_biquadratic::transform $stampe_coeffs $test_gx $test_gy] interp_uncal_x interp_uncal_y
    puts ""
    puts [format "Inverse transform at grid position (%.1f, %.1f): Uncal_X=%.6f, Uncal_Y=%.6f" \
          $test_gx $test_gy $interp_uncal_x $interp_uncal_y]
    
    puts ""
    puts "Note: This implementation follows the Stampe 1993 method with the same"
    puts "interface as biquadratic.tcl. The fit creates uncal->grid transformation,"
    puts "while transform does the inverse (grid->uncal) for compatibility."
}

