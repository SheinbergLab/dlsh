#
# em.tcl - Eye movement analysis package
#
# Provides eye tracking analysis utilities including:
#   - Biquadratic calibration fitting and transformation
#   - Raw data stream processing (x/y separation, P1-P4 computation)
#   - Timestamp normalization
#   - (Future: saccade detection, fixation analysis, etc.)
#
# This package has no dserv dependencies and can be used standalone.
#

package provide em 1.0

# Source the biquadratic fitting implementation
source [file join [file dirname [info script]] biquadratic.tcl]

namespace eval em {
    
    ######################################################################
    #                    Raw Data Stream Processing                      #
    ######################################################################
    
    #
    # Separate interleaved x,y data into separate x and y lists
    #
    # Eye tracking data often comes as interleaved {x0 y0 x1 y1 x2 y2 ...}
    # This separates it into {x0 x1 x2 ...} and {y0 y1 y2 ...}
    #
    # Uses dl_select with repeating pattern: {1 0} takes even indices (x),
    # {0 1} takes odd indices (y) by cycling the mask across all elements.
    #
    # Arguments:
    #   interleaved - dl list of interleaved x,y values (flat or nested)
    #   ns          - optional: dl list of sample counts per trial (for truncation)
    #                 if provided, each trial is truncated to ns[i]*2 elements
    #
    # Returns:
    #   dl list of two elements: {x_data y_data}
    #   Access as: $result:0 (x) and $result:1 (y)
    #
    proc separate_xy {interleaved {ns ""}} {
        if {$ns ne ""} {
            # Truncate each trial to consistent length before separating
            dl_local v [dl_fromto 0 [dl_mult $ns 2]]
            dl_local truncated [dl_choose $interleaved $v]
            dl_local x [dl_select $truncated [dl_llist [dl_ilist 1 0]]]
            dl_local y [dl_select $truncated [dl_llist [dl_ilist 0 1]]]
        } else {
            dl_local x [dl_select $interleaved [dl_llist [dl_ilist 1 0]]]
            dl_local y [dl_select $interleaved [dl_llist [dl_ilist 0 1]]]
        }
        dl_return [dl_llist $x $y]
    }
    
    #
    # Compute P1-P4 difference (dual Purkinje eye position)
    #
    # The P1-P4 difference is the standard measure for dual-Purkinje
    # eye trackers, representing eye rotation independent of translation.
    #
    # Arguments:
    #   p1_x, p1_y - first Purkinje reflection position (dl lists)
    #   p4_x, p4_y - fourth Purkinje reflection position (dl lists)
    #
    # Returns:
    #   dl list of two elements: {h_diff v_diff}
    #   Access as: $result:0 (h) and $result:1 (v)
    #
    proc compute_p1p4 {p1_x p1_y p4_x p4_y} {
        dl_local h [dl_sub $p1_x $p4_x]
        dl_local v [dl_sub $p1_y $p4_y]
        dl_return [dl_llist $h $v]
    }
    
    #
    # Compute pupil-CR difference (pupil minus corneal reflection)
    #
    # Common measure for video-based eye trackers with CR tracking.
    #
    # Arguments:
    #   pupil_x, pupil_y - pupil center position (dl lists)
    #   cr_x, cr_y       - corneal reflection position (dl lists)
    #
    # Returns:
    #   dl list of two elements: {h_diff v_diff}
    #   Access as: $result:0 (h) and $result:1 (v)
    #
    proc compute_pupil_cr {pupil_x pupil_y cr_x cr_y} {
        dl_local h [dl_sub $pupil_x $cr_x]
        dl_local v [dl_sub $pupil_y $cr_y]
        dl_return [dl_llist $h $v]
    }
    
    #
    # Normalize timestamps to seconds from first sample
    #
    # Arguments:
    #   timestamps - dl list of timestamps in microseconds (nested, one per trial)
    #
    # Returns:
    #   dl list of timestamps in seconds, relative to first sample of each trial
    #
    proc normalize_timestamps {timestamps} {
        # Get first timestamp from each trial
        dl_local start_times [dl_choose $timestamps [dl_llist [dl_ilist 0]]]
        dl_local relative [dl_sub $timestamps $start_times]
        dl_return [dl_div $relative 1000000.0]
    }
    
    #
    # Compute minimum lengths across multiple data streams
    #
    # Eye tracking data streams may have slightly different lengths due to
    # timing issues. This finds the minimum length for each trial so we
    # can truncate all streams to a consistent size.
    #
    # Arguments:
    #   args - alternating: dl_list is_interleaved dl_list is_interleaved ...
    #          is_interleaved should be 1 for x,y interleaved data, 0 otherwise
    #
    # Returns:
    #   dl list of minimum sample counts per trial
    #
    # Example:
    #   dl_local ns [em::compute_min_lengths $em_pupil 1 $em_p1 1 $em_time 0]
    #
    proc compute_min_lengths {args} {
        dl_local lengths [dl_llist]
        
        foreach {data is_interleaved} $args {
            if {$is_interleaved} {
                dl_append $lengths [dl_div [dl_lengths $data] 2]
            } else {
                dl_append $lengths [dl_lengths $data]
            }
        }
        
        # Transpose and take min across streams for each trial
        dl_return [dl_mins [dl_transpose $lengths]]
    }
    
    #
    # Truncate a data stream to specified lengths
    #
    # Arguments:
    #   data           - nested dl list of data
    #   ns             - dl list of target lengths per trial
    #   is_interleaved - if true, multiply ns by 2 for interleaved x,y data
    #
    # Returns:
    #   truncated dl list
    #
    proc truncate_to_length {data ns {is_interleaved 0}} {
        if {$is_interleaved} {
            dl_local v [dl_fromto 0 [dl_mult $ns 2]]
        } else {
            dl_local v [dl_fromto 0 $ns]
        }
        dl_return [dl_choose $data $v]
    }
    
    #
    # Process raw eye tracking streams into standard format
    #
    # This is a convenience function that takes raw em streams and produces
    # separated x,y components with consistent lengths, storing results in
    # the provided dg.
    #
    # Arguments:
    #   g       - dg to store results in (adds columns to existing dg)
    #   streams - dict with keys: pupil, p1, p4, time, pupil_r, blink, frame_id
    #             each value is a nested dl list (one per trial)
    #             pupil, p1, p4 are interleaved x,y
    #   prefix  - optional prefix for output column names (default: "")
    #
    # Creates columns (with optional prefix):
    #   pupil_x, pupil_y, p1_x, p1_y, p4_x, p4_y,
    #   p1p4_h, p1p4_v, pupil_cr_h, pupil_cr_v,
    #   em_time, em_seconds, pupil_r, in_blink, frame_id
    #
    proc process_raw_streams {g streams {prefix ""}} {
        # Compute minimum lengths across all streams
        set length_args [list]
        foreach {key interleaved} {pupil 1 p1 1 p4 1 time 0 pupil_r 0 blink 0 frame_id 0} {
            if {[dict exists $streams $key]} {
                lappend length_args [dict get $streams $key] $interleaved
            }
        }
        
        if {[llength $length_args] == 0} {
            return
        }
        
        dl_local ns [compute_min_lengths {*}$length_args]
        
        # Process interleaved streams (x,y pairs)
        foreach key {pupil p1 p4} {
            if {[dict exists $streams $key]} {
                dl_local xy [separate_xy [dict get $streams $key] $ns]
                dl_set $g:${prefix}${key}_x $xy:0
                dl_set $g:${prefix}${key}_y $xy:1
            }
        }
        
        # Compute P1-P4 if we have both
        if {[dict exists $streams p1] && [dict exists $streams p4]} {
            dl_local p1p4 [compute_p1p4 \
                $g:${prefix}p1_x $g:${prefix}p1_y \
                $g:${prefix}p4_x $g:${prefix}p4_y]
            dl_set $g:${prefix}p1p4_h $p1p4:0
            dl_set $g:${prefix}p1p4_v $p1p4:1
        }
        
        # Compute pupil-CR if we have pupil and p1 (CR1)
        if {[dict exists $streams pupil] && [dict exists $streams p1]} {
            dl_local pcr [compute_pupil_cr \
                $g:${prefix}pupil_x $g:${prefix}pupil_y \
                $g:${prefix}p1_x $g:${prefix}p1_y]
            dl_set $g:${prefix}pupil_cr_h $pcr:0
            dl_set $g:${prefix}pupil_cr_v $pcr:1
        }
        
        # Process scalar streams (truncate to consistent length)
        if {[dict exists $streams time]} {
            dl_set $g:${prefix}em_time [truncate_to_length [dict get $streams time] $ns 0]
            dl_set $g:${prefix}em_seconds [normalize_timestamps $g:${prefix}em_time]
        }
        
        if {[dict exists $streams pupil_r]} {
            dl_set $g:${prefix}pupil_r [truncate_to_length [dict get $streams pupil_r] $ns 0]
        }
        
        if {[dict exists $streams blink]} {
            dl_set $g:${prefix}in_blink [truncate_to_length [dict get $streams blink] $ns 0]
        }
        
        if {[dict exists $streams frame_id]} {
            dl_set $g:${prefix}frame_id [truncate_to_length [dict get $streams frame_id] $ns 0]
        }
    }
    
    ######################################################################
    #                    Biquadratic Calibration                         #
    ######################################################################
    
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
