#
# em.tcl - Eye movement analysis package
#
# Provides eye tracking analysis utilities including:
#   - Biquadratic calibration fitting and transformation
#   - (Future: saccade detection, fixation analysis, etc.)
#
# This package has no dserv dependencies and can be used standalone.
#

package provide em 1.0

namespace eval em {
    
    #
    # Biquadratic calibration fitting
    #
    # Maps raw eye position (e.g., P1-P4 difference) to calibrated screen position
    # using a biquadratic polynomial:
    #
    #   X = a₀ + a₁x + a₂y + a₃x² + a₄y² + a₅xy + a₆x²y + a₇xy² + a₈x²y²
    #   Y = b₀ + b₁x + b₂y + b₃x² + b₄y² + b₅xy + b₆x²y + b₇xy² + b₈x²y²
    #
    # where (x,y) are raw eye measurements and (X,Y) are calibrated positions
    #
    
    #
    # Fit biquadratic mapping from eye position to calibration targets
    #
    # Arguments:
    #   eye_x   - list of raw eye horizontal positions
    #   eye_y   - list of raw eye vertical positions  
    #   calib_x - list of known calibration target X positions
    #   calib_y - list of known calibration target Y positions
    #
    # Returns:
    #   {x_coeffs y_coeffs} where each is a list of 9 coefficients
    #   Returns "" on failure
    #
    proc biquadratic_fit {eye_x eye_y calib_x calib_y} {
        set n [llength $eye_x]
        
        if {$n < 9} {
            puts "em::biquadratic_fit: Need at least 9 points, have $n"
            return ""
        }
        
        if {[llength $eye_y] != $n || [llength $calib_x] != $n || [llength $calib_y] != $n} {
            puts "em::biquadratic_fit: Input lists must have same length"
            return ""
        }
        
        # Build design matrix A where each row is:
        # [1, x, y, x², y², xy, x²y, xy², x²y²]
        set A [list]
        for {set i 0} {$i < $n} {incr i} {
            set x [lindex $eye_x $i]
            set y [lindex $eye_y $i]
            set x2 [expr {$x * $x}]
            set y2 [expr {$y * $y}]
            set xy [expr {$x * $y}]
            set x2y [expr {$x2 * $y}]
            set xy2 [expr {$x * $y2}]
            set x2y2 [expr {$x2 * $y2}]
            lappend A [list 1.0 $x $y $x2 $y2 $xy $x2y $xy2 $x2y2]
        }
        
        # Solve for X coefficients: A * x_coeffs = calib_x
        set x_coeffs [least_squares_solve $A $calib_x]
        
        # Solve for Y coefficients: A * y_coeffs = calib_y
        set y_coeffs [least_squares_solve $A $calib_y]
        
        if {$x_coeffs eq "" || $y_coeffs eq ""} {
            return ""
        }
        
        return [list $x_coeffs $y_coeffs]
    }
    
    #
    # Solve least squares problem A * x = b
    # Uses normal equations: x = (A'A)^(-1) * A'b
    #
    proc least_squares_solve {A b} {
        set m [llength $A]
        set n [llength [lindex $A 0]]
        
        # Compute A' * A (n x n matrix)
        set AtA [list]
        for {set i 0} {$i < $n} {incr i} {
            set row [list]
            for {set j 0} {$j < $n} {incr j} {
                set sum 0.0
                for {set k 0} {$k < $m} {incr k} {
                    set sum [expr {$sum + [lindex $A $k $i] * [lindex $A $k $j]}]
                }
                lappend row $sum
            }
            lappend AtA $row
        }
        
        # Compute A' * b (n x 1 vector)
        set Atb [list]
        for {set i 0} {$i < $n} {incr i} {
            set sum 0.0
            for {set k 0} {$k < $m} {incr k} {
                set sum [expr {$sum + [lindex $A $k $i] * [lindex $b $k]}]
            }
            lappend Atb $sum
        }
        
        # Solve AtA * x = Atb using Gaussian elimination with partial pivoting
        return [gauss_solve $AtA $Atb]
    }
    
    #
    # Solve linear system using Gaussian elimination with partial pivoting
    #
    proc gauss_solve {A b} {
        set n [llength $A]
        
        # Make copies we can modify
        set M [list]
        foreach row $A {
            lappend M [list {*}$row]
        }
        set x [list {*}$b]
        
        # Forward elimination with partial pivoting
        for {set col 0} {$col < $n} {incr col} {
            # Find pivot
            set max_val [expr {abs([lindex $M $col $col])}]
            set max_row $col
            for {set row [expr {$col + 1}]} {$row < $n} {incr row} {
                set val [expr {abs([lindex $M $row $col])}]
                if {$val > $max_val} {
                    set max_val $val
                    set max_row $row
                }
            }
            
            if {$max_val < 1e-12} {
                puts "em::gauss_solve: Matrix is singular or nearly singular"
                return ""
            }
            
            # Swap rows if needed
            if {$max_row != $col} {
                set tmp [lindex $M $col]
                lset M $col [lindex $M $max_row]
                lset M $max_row $tmp
                
                set tmp [lindex $x $col]
                lset x $col [lindex $x $max_row]
                lset x $max_row $tmp
            }
            
            # Eliminate column
            set pivot [lindex $M $col $col]
            for {set row [expr {$col + 1}]} {$row < $n} {incr row} {
                set factor [expr {[lindex $M $row $col] / $pivot}]
                lset x $row [expr {[lindex $x $row] - $factor * [lindex $x $col]}]
                for {set j $col} {$j < $n} {incr j} {
                    lset M $row $j [expr {[lindex $M $row $j] - $factor * [lindex $M $col $j]}]
                }
            }
        }
        
        # Back substitution
        for {set row [expr {$n - 1}]} {$row >= 0} {incr row -1} {
            set sum [lindex $x $row]
            for {set j [expr {$row + 1}]} {$j < $n} {incr j} {
                set sum [expr {$sum - [lindex $M $row $j] * [lindex $x $j]}]
            }
            lset x $row [expr {$sum / [lindex $M $row $row]}]
        }
        
        return $x
    }
    
    #
    # Apply biquadratic transform to eye data
    #
    # Arguments:
    #   coeffs - list of 9 coefficients {a0 a1 a2 a3 a4 a5 a6 a7 a8}
    #   x      - raw horizontal eye position (scalar or dl list)
    #   y      - raw vertical eye position (scalar or dl list)
    #
    # Returns:
    #   transformed position (scalar or dl list)
    #
    proc biquadratic_transform {coeffs x y} {
        lassign $coeffs a0 a1 a2 a3 a4 a5 a6 a7 a8
        
        # Check if inputs are dl lists or scalars
        if {[string match "&*&" $x]} {
            # dl list inputs - use vectorized operations
            dl_local x2   [dl_mult $x $x]
            dl_local y2   [dl_mult $y $y]
            dl_local xy   [dl_mult $x $y]
            dl_local x2y  [dl_mult $x2 $y]
            dl_local xy2  [dl_mult $x $y2]
            dl_local x2y2 [dl_mult $x2 $y2]
            
            dl_local result [dl_add $a0 \
                [dl_mult $a1 $x] \
                [dl_mult $a2 $y] \
                [dl_mult $a3 $x2] \
                [dl_mult $a4 $y2] \
                [dl_mult $a5 $xy] \
                [dl_mult $a6 $x2y] \
                [dl_mult $a7 $xy2] \
                [dl_mult $a8 $x2y2]]
            
            dl_return $result
        } else {
            # Scalar inputs
            set x2 [expr {$x * $x}]
            set y2 [expr {$y * $y}]
            set xy [expr {$x * $y}]
            set x2y [expr {$x2 * $y}]
            set xy2 [expr {$x * $y2}]
            set x2y2 [expr {$x2 * $y2}]
            
            return [expr {$a0 + $a1*$x + $a2*$y + $a3*$x2 + $a4*$y2 + \
                          $a5*$xy + $a6*$x2y + $a7*$xy2 + $a8*$x2y2}]
        }
    }
    
    #
    # Apply full biquadratic calibration (both X and Y)
    #
    # Arguments:
    #   coeffs - {x_coeffs y_coeffs} from biquadratic_fit
    #   h_raw  - raw horizontal eye position
    #   v_raw  - raw vertical eye position
    #
    # Returns:
    #   {h_calibrated v_calibrated}
    #
    proc biquadratic_calibrate {coeffs h_raw v_raw} {
        lassign $coeffs x_coeffs y_coeffs
        
        set h_cal [biquadratic_transform $x_coeffs $h_raw $v_raw]
        set v_cal [biquadratic_transform $y_coeffs $h_raw $v_raw]
        
        return [list $h_cal $v_cal]
    }
    
    #
    # Calculate RMS error of biquadratic fit
    #
    # Arguments:
    #   coeffs  - {x_coeffs y_coeffs} from biquadratic_fit
    #   eye_x   - list of raw eye horizontal positions
    #   eye_y   - list of raw eye vertical positions
    #   calib_x - list of known calibration target X positions
    #   calib_y - list of known calibration target Y positions
    #
    # Returns:
    #   RMS error in same units as calibration targets
    #
    proc biquadratic_rms {coeffs eye_x eye_y calib_x calib_y} {
        lassign $coeffs x_coeffs y_coeffs
        
        set n [llength $eye_x]
        set sum_sq 0.0
        
        for {set i 0} {$i < $n} {incr i} {
            set ex [lindex $eye_x $i]
            set ey [lindex $eye_y $i]
            set cx [lindex $calib_x $i]
            set cy [lindex $calib_y $i]
            
            set pred_x [biquadratic_transform $x_coeffs $ex $ey]
            set pred_y [biquadratic_transform $y_coeffs $ex $ey]
            
            set err_x [expr {$pred_x - $cx}]
            set err_y [expr {$pred_y - $cy}]
            
            set sum_sq [expr {$sum_sq + $err_x*$err_x + $err_y*$err_y}]
        }
        
        return [expr {sqrt($sum_sq / (2.0 * $n))}]
    }
    
    #
    # Calculate per-point errors for diagnostics
    #
    proc biquadratic_errors {coeffs eye_x eye_y calib_x calib_y} {
        lassign $coeffs x_coeffs y_coeffs
        
        set n [llength $eye_x]
        set errors [list]
        
        for {set i 0} {$i < $n} {incr i} {
            set ex [lindex $eye_x $i]
            set ey [lindex $eye_y $i]
            set cx [lindex $calib_x $i]
            set cy [lindex $calib_y $i]
            
            set pred_x [biquadratic_transform $x_coeffs $ex $ey]
            set pred_y [biquadratic_transform $y_coeffs $ex $ey]
            
            set err_x [expr {$pred_x - $cx}]
            set err_y [expr {$pred_y - $cy}]
            set err_dist [expr {sqrt($err_x*$err_x + $err_y*$err_y)}]
            
            lappend errors [dict create \
                point $i \
                calib_x $cx calib_y $cy \
                pred_x $pred_x pred_y $pred_y \
                err_x $err_x err_y $err_y \
                err_dist $err_dist]
        }
        
        return $errors
    }
}
