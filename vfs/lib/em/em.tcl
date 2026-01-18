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

# Source the biquadratic fitting implementation
source [file join [file dirname [info script]] biquadratic.tcl]

namespace eval em {
    
    #
    # Biquadratic calibration - wrapper around biquadratic:: namespace
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
        if {[llength $eye_x] < 9} {
            puts "em::biquadratic_fit: Need at least 9 points, have [llength $eye_x]"
            return ""
        }
        
        if {[catch {
            set coeffs [biquadratic::fit $eye_x $eye_y $calib_x $calib_y]
        } err]} {
            puts "em::biquadratic_fit: $err"
            return ""
        }
        
        return $coeffs
    }
    
    #
    # Apply biquadratic transform to eye data (scalar version)
    #
    # Arguments:
    #   coeffs - list of 9 coefficients {a0 a1 a2 a3 a4 a5 a6 a7 a8}
    #   x      - raw horizontal eye position (scalar)
    #   y      - raw vertical eye position (scalar)
    #
    # Returns:
    #   transformed position (scalar)
    #
    proc biquadratic_transform {coeffs x y} {
        return [biquadratic::evaluate $coeffs $x $y]
    }
    
    #
    # Apply biquadratic transform to dl lists
    #
    # Arguments:
    #   coeffs - list of 9 coefficients {a0 a1 a2 a3 a4 a5 a6 a7 a8}
    #   x      - raw horizontal eye position (dl list)
    #   y      - raw vertical eye position (dl list)
    #
    # Returns:
    #   transformed position (dl list)
    #
    proc biquadratic_transform_dl {coeffs x y} {
        lassign $coeffs a0 a1 a2 a3 a4 a5 a6 a7 a8
        
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
    # Apply full biquadratic calibration to dl lists (both X and Y)
    #
    proc biquadratic_calibrate_dl {coeffs h_raw v_raw} {
        lassign $coeffs x_coeffs y_coeffs
        
        set h_cal [biquadratic_transform_dl $x_coeffs $h_raw $v_raw]
        set v_cal [biquadratic_transform_dl $y_coeffs $h_raw $v_raw]
        
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
    #   {rms_x rms_y} in same units as calibration targets
    #
    proc biquadratic_rms {coeffs eye_x eye_y calib_x calib_y} {
        return [biquadratic::calculate_rms_error $coeffs $eye_x $eye_y $calib_x $calib_y]
    }
    
    #
    # Calculate combined RMS error (single value)
    #
    proc biquadratic_rms_combined {coeffs eye_x eye_y calib_x calib_y} {
        lassign [biquadratic_rms $coeffs $eye_x $eye_y $calib_x $calib_y] rms_x rms_y
        return [expr {sqrt(($rms_x*$rms_x + $rms_y*$rms_y) / 2.0)}]
    }
    
    #
    # Print coefficients for debugging
    #
    proc biquadratic_print {coeffs} {
        biquadratic::print_coefficients $coeffs
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
            
            set pred_x [biquadratic::evaluate $x_coeffs $ex $ey]
            set pred_y [biquadratic::evaluate $y_coeffs $ex $ey]
            
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
