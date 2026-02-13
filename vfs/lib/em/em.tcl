#
# em.tcl - Eye movement analysis package
#
# Provides eye tracking analysis utilities including:
#   - Biquadratic calibration fitting and transformation
#   - Calibration extraction from datafiles and application to raw streams
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
    # Normalize timestamps to seconds from first sample of each trial
    #
    # Arguments:
    #   timestamps - dl list of timestamps in seconds (nested, one per trial)
    #
    # Returns:
    #   dl list of timestamps in seconds, relative to first sample of each trial
    #
    proc normalize_timestamps {timestamps} {
        dl_local start_times [dl_choose $timestamps [dl_llist [dl_ilist 0]]]
        dl_return [dl_sub $timestamps $start_times]
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
    
    ######################################################################
    #                    Calibration Extraction                          #
    ######################################################################
    
    #
    # Extract biquadratic calibration coefficients from a loaded dg
    #
    # Calibration data is stored as an out-of-band session datapoint
    # in the <session>em/biquadratic column.  The value is a dict:
    #   source, filename, timestamp, x_coeffs, y_coeffs, rms_x, rms_y,
    #   rms_error, n_trials
    #
    # Arguments:
    #   g - loaded dg (from dslog::readESS or dg_read of obs file)
    #
    # Returns:
    #   calibration dict, or "" if not found
    #
    proc extract_calibration_from_dg {g} {
        if {![dl_exists $g:<session>em/biquadratic]} {
            return ""
        }
        
        # Session column holds one datapoint at index :0
        # dl_tcllist returns it wrapped in extra {}'s, so lindex to unwrap
        set calib_dict [lindex [dl_tcllist $g:<session>em/biquadratic:0] 0]
        
        if {$calib_dict eq ""} {
            return ""
        }
        
        # Validate required keys
        if {![dict exists $calib_dict x_coeffs] || ![dict exists $calib_dict y_coeffs]} {
            puts "em::extract_calibration_from_dg: missing x_coeffs or y_coeffs"
            return ""
        }
        
        return $calib_dict
    }
    
    #
    # Extract calibration from a df::File object
    #
    # Arguments:
    #   f - df::File object (already opened)
    #
    # Returns:
    #   calibration dict or "" if not found
    #
    proc extract_calibration {f} {
        return [extract_calibration_from_dg [$f group]]
    }
    
    #
    # Get just the {x_coeffs y_coeffs} pair from a calibration dict
    #
    # This is the format expected by biquadratic_calibrate_dl and
    # apply_calibration.
    #
    # Arguments:
    #   calib - calibration dict (from extract_calibration)
    #
    # Returns:
    #   {x_coeffs y_coeffs} or "" if calib is empty
    #
    proc calibration_coeffs {calib} {
        if {$calib eq ""} {
            return ""
        }
        return [list [dict get $calib x_coeffs] [dict get $calib y_coeffs]]
    }
    
    #
    # Log calibration provenance for diagnostics
    #
    proc calibration_info {calib} {
        if {$calib eq ""} {
            puts "em::calibration: none"
            return
        }
        set source "unknown"
        set rms "?"
        set n "?"
        if {[dict exists $calib source]} { set source [dict get $calib source] }
        if {[dict exists $calib rms_error]} { set rms [format "%.4f" [dict get $calib rms_error]] }
        if {[dict exists $calib n_trials]} { set n [dict get $calib n_trials] }
        puts "em::calibration: source=$source rms=${rms}deg n_trials=$n"
    }
    
    ######################################################################
    #              Calibration Application (dl vectorized)               #
    ######################################################################
    
    #
    # Apply biquadratic calibration to nested per-trial eye data
    #
    # Transforms raw eye position (e.g., P1-P4 difference in pixels)
    # to calibrated position (degrees visual angle) using biquadratic
    # polynomial coefficients from emcalib.
    #
    # Works on nested dl lists (one sublist per trial) because
    # dl_mult/dl_add broadcast across nested structure.
    #
    # Arguments:
    #   coeffs - {x_coeffs y_coeffs} from calibration_coeffs
    #   h_raw  - nested dl list of raw horizontal position
    #   v_raw  - nested dl list of raw vertical position
    #
    # Returns:
    #   list of two dl names: {h_deg v_deg}
    #
    proc apply_calibration {coeffs h_raw v_raw} {
        lassign $coeffs x_coeffs y_coeffs
        
        dl_local h_deg [biquadratic_transform_dl $x_coeffs $h_raw $v_raw]
        dl_local v_deg [biquadratic_transform_dl $y_coeffs $h_raw $v_raw]
        
        dl_return [dl_llist $h_deg $v_deg]
    }
    
    ######################################################################
    #              Process Raw Streams (main entry point)                #
    ######################################################################
    
    #
    # Process raw eye tracking streams into standard format
    #
    # This is the main entry point called from extract functions.
    # Takes raw em data streams, separates x/y, computes difference
    # signals, optionally applies biquadratic calibration, and stores
    # everything as columns in the output dg.
    #
    # Arguments:
    #   g       - dg to store results in (adds columns to existing dg)
    #   streams - dict with keys: pupil, p1, p4, time, pupil_r, blink, frame_id
    #             each value is a nested dl list (one per trial)
    #             pupil, p1, p4 are interleaved x,y
    #   args    - optional key-value pairs:
    #               -calibration {x_coeffs y_coeffs}  (from calibration_coeffs)
    #               -prefix string                     (column name prefix, default "")
    #
    # Creates columns (with optional prefix):
    #   Raw:        pupil_x, pupil_y, p1_x, p1_y, p4_x, p4_y
    #   Eye signal: eye_raw_h, eye_raw_v  (realtime-computed, with inversions applied)
    #   Calibrated: em_h_deg, em_v_deg  (only if -calibration provided)
    #   Timing:     em_time, em_seconds
    #   Other:      pupil_r, in_blink, frame_id
    #
    # Difference signals (p1p4, pupil_cr) are not stored as they are
    # trivially recomputable from the raw components.
    # eye_raw_h/v are kept because they include the inversion settings
    # from acquisition and are the signal the biquadratic was fit to.
    #
    proc process_raw_streams {g streams args} {
        # Parse options
        set calibration ""
        set prefix ""
        foreach {key val} $args {
            switch -- $key {
                -calibration { set calibration $val }
                -prefix      { set prefix $val }
            }
        }
        
        # Compute minimum lengths across all streams
        set length_args [list]
        foreach {key interleaved} {pupil 1 p1 1 p4 1 eye_raw 1 time 0 pupil_r 0 blink 0 frame_id 0} {
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
        
        #
        # Apply biquadratic calibration: raw pixels -> degrees visual angle
        #
        # Uses the eye_raw stream (eyetracking/raw from the realtime system),
        # which is the same signal the biquadratic was fit to.  This accounts
        # for any sign conventions or inversions applied at acquisition time.
        #
        # Difference signals (p1p4, pupil_cr) are not stored as they are
        # trivially recomputable from the raw components above.
        #
        if {$calibration ne "" && [dict exists $streams eye_raw]} {
            dl_local raw_xy [separate_xy [dict get $streams eye_raw] $ns]
            dl_set $g:${prefix}eye_raw_h $raw_xy:0
            dl_set $g:${prefix}eye_raw_v $raw_xy:1
            
            dl_local cal [apply_calibration $calibration \
                $g:${prefix}eye_raw_h $g:${prefix}eye_raw_v]
            dl_set $g:${prefix}em_h_deg $cal:0
            dl_set $g:${prefix}em_v_deg $cal:1
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
    proc biquadratic_transform {coeffs x y} {
        return [biquadratic::evaluate $coeffs $x $y]
    }
    
    #
    # Apply biquadratic transform to dl lists (flat or nested)
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
    # Apply full biquadratic calibration (both X and Y, scalar)
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
        
        dl_local h_cal [biquadratic_transform_dl $x_coeffs $h_raw $v_raw]
        dl_local v_cal [biquadratic_transform_dl $y_coeffs $h_raw $v_raw]
        
        dl_return [dl_llist $h_cal $v_cal]
    }
    
    #
    # Calculate RMS error of biquadratic fit
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
