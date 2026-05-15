# mp_sim --
#   Headless simulator and design-spec compiler for "cryptic motion"
#   pulsed-coherence random-dot kinematograms. Mirrors the dot-update
#   semantics of stim2's motionpatch.c without a display, so trial
#   ensembles can be run in bulk for figure generation, parameter
#   sweeps, and statistical verification of the manipulation.
#
#   Two layers:
#     Layer A (design): compile a high-level spec dict into a per-frame
#                       "state timeline" dynamic group. The timeline is
#                       a strict subset of motionpatch_logExport's
#                       schema, so a recorded trial is also a valid
#                       timeline and the sim/recording can share an
#                       analysis pipeline.
#     Layer B (kernel): consume a state timeline plus (n_dots, dt, seed)
#                       and produce a per-frame + per-dot log dg whose
#                       schema matches motionpatch_logExport exactly.
#
#   Conventions:
#     - direction stored in radians
#     - speed in patch-local-units per second
#     - lifetime_s in seconds
#     - mask_offset_x/y in patch-local units
#     - dot positions in [-0.5, 0.5] (toroidal wrap)
#
#   See testmp_sim.tcl for usage.

package provide mp_sim 0.3
package require dlsh

namespace eval mp_sim {
    variable pi 3.14159265358979323846
}

# ============================================================
# Layer A.0: envelope + trajectory dispatchers
# ============================================================
#
# An envelope is a Tcl dict describing how the modulating value
# evolves over time. atomic kinds compute a per-frame value directly;
# compositors (product, sum) recurse into a list of sub-envelopes and
# combine their outputs. Adding a new kind = one switch case in
# eval_envelope + a small private proc.
#
# Kinds:
#   {kind flat              base_coh V}                    -- constant
#   {kind sum_gaussians     n_pulses N sigma_ms S         -- pulse train
#                           ?centers C? base_coh V}
#   {kind cosine_ramp       t0 T0 t1 T1 base_coh V}        -- raised-cosine 0->base over [t0, t1]
#   {kind gate              t0 T0 t1 T1 base_coh V}        -- rectangular 1 inside [t0, t1]
#   {kind sigmoid           t0 T0 tau TAU base_coh V}      -- logistic ramp
#   {kind trapezoid_train   centers {t0 t1 ...}            -- trapezoidal pulse train
#                           plateau_dur P ease_dur E base_coh V}
#   {kind product           parts {<env1> <env2> ...}}     -- multiply parts
#   {kind sum               parts {<env1> <env2> ...}      -- add parts (clamp to base_coh)
#                           base_coh V}
#   {kind callback          proc procname ?args A?}        -- arbitrary user fn

# mp_sim::eval_envelope env ts
#   Evaluate an envelope dict at time grid ts (a dl). Returns a dl of
#   per-frame values clamped to [0, base_coh] for the leaf kinds; the
#   compositors do their own clamping to keep intermediate intermediate
#   composition well-defined.
proc mp_sim::eval_envelope {env ts} {
    set kind [dict get $env kind]
    # dl_return at every dispatch level: the inner _env_* procs return
    # dl-return-named lists (>#<) that live only in their caller's
    # scope; passing them through this dispatcher with a plain `return`
    # would let them be reaped before compile_spec consumes them.
    switch -- $kind {
        flat              { dl_return [mp_sim::_env_flat              $env $ts] }
        sum_gaussians     { dl_return [mp_sim::_env_sum_gaussians     $env $ts] }
        cosine_ramp       { dl_return [mp_sim::_env_cosine_ramp       $env $ts] }
        gate              { dl_return [mp_sim::_env_gate              $env $ts] }
        sigmoid           { dl_return [mp_sim::_env_sigmoid           $env $ts] }
        trapezoid_train   { dl_return [mp_sim::_env_trapezoid_train   $env $ts] }
        product           { dl_return [mp_sim::_env_product           $env $ts] }
        sum               { dl_return [mp_sim::_env_sum               $env $ts] }
        callback          { dl_return [mp_sim::_env_callback          $env $ts] }
        default           { error "mp_sim::eval_envelope: unknown kind '$kind'" }
    }
}

# mp_sim::collect_tile_times env duration
#   Walk an envelope dict (recursing into compositors) and return a
#   flat Tcl list of all pulse-center times. Used for resolving
#   bounce phase=peak/trough regardless of how the envelope was
#   composed.
proc mp_sim::collect_tile_times {env duration} {
    set kind [dict get $env kind]
    switch -- $kind {
        sum_gaussians {
            if {[dict exists $env centers]} {
                return [dict get $env centers]
            } else {
                set n_pulses [dict get $env n_pulses]
                return [mp_sim::evenly_spaced_pulse_centers $n_pulses $duration]
            }
        }
        trapezoid_train {
            if {[dict exists $env centers]} {
                return [dict get $env centers]
            } else {
                return [list]
            }
        }
        product - sum {
            set out [list]
            foreach sub [dict get $env parts] {
                lappend out {*}[mp_sim::collect_tile_times $sub $duration]
            }
            return $out
        }
        default { return [list] }
    }
}

# ---- Atomic envelope kinds -----------------------------------------

proc mp_sim::_env_base_coh {env {default 1.0}} {
    if {[dict exists $env base_coh]} { return [dict get $env base_coh] }
    return $default
}

proc mp_sim::_env_flat {env ts} {
    set base [mp_sim::_env_base_coh $env]
    dl_return [dl_mult $base [dl_ones [dl_length $ts]]]
}

proc mp_sim::_env_sum_gaussians {env ts} {
    set base [mp_sim::_env_base_coh $env]
    set sigma_s [expr {[dict get $env sigma_ms] / 1000.0}]
    if {[dict exists $env centers]} {
        set centers [dict get $env centers]
    } else {
        # 'centers' wasn't pre-resolved; we don't know duration here so
        # require either centers or n_pulses+duration upstream. For
        # backwards compat, allow a duration field on the envelope.
        set n_pulses [dict get $env n_pulses]
        set dur [expr {[dict exists $env duration] ? [dict get $env duration] : ([dl_get $ts [expr {[dl_length $ts] - 1}]])}]
        set centers [mp_sim::evenly_spaced_pulse_centers $n_pulses $dur]
    }
    set n [dl_length $ts]
    if {$sigma_s <= 0.0 || [llength $centers] == 0} {
        return [dl_mult $base [dl_ones $n]]
    }
    dl_local sum [dl_zeros $n]
    foreach c $centers {
        dl_local z [dl_div [dl_sub $ts $c] $sigma_s]
        dl_local g [dl_exp [dl_mult -0.5 [dl_mult $z $z]]]
        dl_local sum [dl_add $sum $g]
    }
    dl_local v [dl_mult $base $sum]
    dl_local hi [dl_gte $v $base]
    dl_local v  [dl_add [dl_mult [dl_sub 1.0 $hi] $v] [dl_mult $hi $base]]
    dl_local lo [dl_lt $v 0.0]
    dl_local v  [dl_mult [dl_sub 1.0 $lo] $v]
    dl_return $v
}

# Raised cosine: rises from 0 to base over [t0, t1], stays at base
# for t > t1, is 0 for t < t0. Useful as an onset ramp or as the
# building block for trapezoid_train ease segments.
proc mp_sim::_env_cosine_ramp {env ts} {
    variable pi
    set base [mp_sim::_env_base_coh $env]
    set t0 [dict get $env t0]
    set t1 [dict get $env t1]
    set n [dl_length $ts]
    if {$t1 <= $t0} {
        # degenerate ramp -> step
        dl_local mask [dl_gte $ts $t0]
        return [dl_mult $base $mask]
    }
    set width [expr {$t1 - $t0}]
    # phase = clamp((t-t0)/width, 0, 1)
    dl_local phase [dl_div [dl_sub $ts $t0] $width]
    dl_local hi [dl_gte $phase 1.0]
    dl_local lo [dl_lt $phase 0.0]
    dl_local phase [dl_add [dl_mult [dl_sub 1.0 $hi] $phase] [dl_mult $hi 1.0]]
    dl_local phase [dl_mult [dl_sub 1.0 $lo] $phase]
    # raised cosine: 0.5 * (1 - cos(pi * phase))
    dl_local v [dl_mult [expr {0.5 * $base}] \
                       [dl_sub 1.0 [dl_cos [dl_mult $pi $phase]]]]
    dl_return $v
}

proc mp_sim::_env_gate {env ts} {
    set base [mp_sim::_env_base_coh $env]
    set t0 [dict get $env t0]
    set t1 [dict get $env t1]
    dl_local hi [dl_gte $ts $t0]
    dl_local lo [dl_lt  $ts $t1]
    dl_local mask [dl_mult $hi $lo]
    dl_return [dl_mult $base $mask]
}

proc mp_sim::_env_sigmoid {env ts} {
    set base [mp_sim::_env_base_coh $env]
    set t0  [dict get $env t0]
    set tau [dict get $env tau]
    if {$tau <= 0.0} {
        # degenerate -> step at t0
        dl_local mask [dl_gte $ts $t0]
        return [dl_mult $base $mask]
    }
    dl_local z [dl_div [dl_sub $ts $t0] $tau]
    dl_local v [dl_div 1.0 [dl_add 1.0 [dl_exp [dl_mult -1.0 $z]]]]
    dl_return [dl_mult $base $v]
}

# Trapezoidal pulse train: each pulse is a raised-cosine rise of
# ease_dur, plateau of plateau_dur at base, raised-cosine fall of
# ease_dur. centers is a Tcl list of pulse PEAK midpoints.
#
# Per-pulse layout, centered at c:
#   [c - plateau/2 - ease,  c - plateau/2]  -- rise
#   [c - plateau/2,         c + plateau/2]  -- plateau
#   [c + plateau/2,         c + plateau/2 + ease]  -- fall
#
# Pulses are summed; if the user spaces them too closely they overlap
# and the sum is clamped to base_coh.
proc mp_sim::_env_trapezoid_train {env ts} {
    variable pi
    set base    [mp_sim::_env_base_coh $env]
    set plateau [dict get $env plateau_dur]
    set ease    [dict get $env ease_dur]
    set centers [dict get $env centers]
    set n [dl_length $ts]
    dl_local sum [dl_zeros $n]
    foreach c $centers {
        set rise_t0 [expr {$c - $plateau / 2.0 - $ease}]
        set rise_t1 [expr {$c - $plateau / 2.0}]
        set plat_t1 [expr {$c + $plateau / 2.0}]
        set fall_t1 [expr {$c + $plateau / 2.0 + $ease}]
        # Rise: raised-cosine ramp 0 -> 1 over [rise_t0, rise_t1]
        if {$ease > 0.0} {
            dl_local rise_phase [dl_div [dl_sub $ts $rise_t0] $ease]
            dl_local rise_hi    [dl_gte $rise_phase 1.0]
            dl_local rise_lo    [dl_lt  $rise_phase 0.0]
            dl_local rise_phase [dl_add [dl_mult [dl_sub 1.0 $rise_hi] $rise_phase] $rise_hi]
            dl_local rise_phase [dl_mult [dl_sub 1.0 $rise_lo] $rise_phase]
            dl_local rise [dl_mult 0.5 [dl_sub 1.0 [dl_cos [dl_mult $pi $rise_phase]]]]
        } else {
            dl_local rise [dl_gte $ts $rise_t1]
        }
        # Plateau gate: 1 inside [rise_t1, plat_t1)
        dl_local plat [dl_mult [dl_gte $ts $rise_t1] [dl_lt $ts $plat_t1]]
        # Fall: raised-cosine 1 -> 0 over [plat_t1, fall_t1]
        if {$ease > 0.0} {
            dl_local fall_phase [dl_div [dl_sub $ts $plat_t1] $ease]
            dl_local fall_hi    [dl_gte $fall_phase 1.0]
            dl_local fall_lo    [dl_lt  $fall_phase 0.0]
            dl_local fall_phase [dl_add [dl_mult [dl_sub 1.0 $fall_hi] $fall_phase] $fall_hi]
            dl_local fall_phase [dl_mult [dl_sub 1.0 $fall_lo] $fall_phase]
            dl_local fall [dl_mult 0.5 [dl_add 1.0 [dl_cos [dl_mult $pi $fall_phase]]]]
        } else {
            dl_local fall [dl_lt $ts $plat_t1]
        }
        # Pulse contribution: rise where t<rise_t1, plateau in between,
        # fall where rise_t1<=t<plat_t1+ease. Use indicator masks to
        # combine without double-counting.
        dl_local in_rise [dl_mult [dl_gte $ts $rise_t0] [dl_lt $ts $rise_t1]]
        dl_local in_plat [dl_mult [dl_gte $ts $rise_t1] [dl_lt $ts $plat_t1]]
        dl_local in_fall [dl_mult [dl_gte $ts $plat_t1] [dl_lt $ts $fall_t1]]
        dl_local pulse [dl_add \
                          [dl_mult $in_rise $rise] \
                          [dl_mult $in_plat 1.0] \
                          [dl_mult $in_fall $fall]]
        dl_local sum [dl_add $sum $pulse]
    }
    dl_local v [dl_mult $base $sum]
    dl_local hi [dl_gte $v $base]
    dl_local v [dl_add [dl_mult [dl_sub 1.0 $hi] $v] [dl_mult $hi $base]]
    dl_local lo [dl_lt $v 0.0]
    dl_local v [dl_mult [dl_sub 1.0 $lo] $v]
    dl_return $v
}

# ---- Compositors ---------------------------------------------------

proc mp_sim::_env_product {env ts} {
    set parts [dict get $env parts]
    if {[llength $parts] == 0} {
        return [dl_ones [dl_length $ts]]
    }
    dl_local v [mp_sim::eval_envelope [lindex $parts 0] $ts]
    foreach p [lrange $parts 1 end] {
        dl_local sub [mp_sim::eval_envelope $p $ts]
        dl_local v [dl_mult $v $sub]
    }
    dl_return $v
}

proc mp_sim::_env_sum {env ts} {
    set parts [dict get $env parts]
    set base [mp_sim::_env_base_coh $env]
    set n [dl_length $ts]
    dl_local v [dl_zeros $n]
    foreach p $parts {
        dl_local sub [mp_sim::eval_envelope $p $ts]
        dl_local v [dl_add $v $sub]
    }
    dl_local hi [dl_gte $v $base]
    dl_local v [dl_add [dl_mult [dl_sub 1.0 $hi] $v] [dl_mult $hi $base]]
    dl_local lo [dl_lt $v 0.0]
    dl_local v [dl_mult [dl_sub 1.0 $lo] $v]
    dl_return $v
}

proc mp_sim::_env_callback {env ts} {
    set p [dict get $env proc]
    set args_dict [expr {[dict exists $env args] ? [dict get $env args] : [dict create]}]
    dl_return [{*}$p $ts $args_dict]
}

# ---- Envelope duration resolution ---------------------------------
#
# Some envelope kinds need a 'duration' to compute their centers (e.g.
# sum_gaussians with n_pulses, or trapezoid_train if we ever support
# a similar shorthand). This walks the dict and fills in resolvable
# fields, so eval_envelope doesn't need to know the trial duration.
proc mp_sim::_resolve_envelope_durations {env duration} {
    set kind [dict get $env kind]
    switch -- $kind {
        sum_gaussians {
            if {![dict exists $env centers] && [dict exists $env n_pulses]} {
                set centers [mp_sim::evenly_spaced_pulse_centers \
                                 [dict get $env n_pulses] $duration]
                dict set env centers $centers
            }
            return $env
        }
        trapezoid_train {
            if {![dict exists $env centers] && [dict exists $env n_pulses]} {
                set centers [mp_sim::evenly_spaced_pulse_centers \
                                 [dict get $env n_pulses] $duration]
                dict set env centers $centers
            }
            return $env
        }
        product - sum {
            set new_parts [list]
            foreach p [dict get $env parts] {
                lappend new_parts [mp_sim::_resolve_envelope_durations $p $duration]
            }
            dict set env parts $new_parts
            return $env
        }
        default { return $env }
    }
}

# ============================================================
# Layer A.1: trajectory dispatcher
# ============================================================
#
# Trajectory kinds:
#   {kind static    ?x X? ?y Y?}
#   {kind sweep     x0 X0 x1 X1 ?y Y?}
#   {kind callback  proc procname}             -- user proc takes $t, returns {x y}
#   {kind step_sequence positions {{x y} ...}  -- discrete location switches
#                       step_times {t0 t1 ...}}    holds position from step_times[i]
#                                                  to step_times[i+1]; len must be
#                                                  positions+1
#
proc mp_sim::eval_trajectory {traj ts duration} {
    set kind [dict get $traj kind]
    set n [dl_length $ts]
    switch -- $kind {
        static {
            set sx [expr {[dict exists $traj x] ? [dict get $traj x] : 0.0}]
            set sy [expr {[dict exists $traj y] ? [dict get $traj y] : 0.0}]
            dl_local mox [dl_mult $sx [dl_ones $n]]
            dl_local moy [dl_mult $sy [dl_ones $n]]
            dl_return [dl_llist $mox $moy]
        }
        sweep {
            set x0 [dict get $traj x0]
            set x1 [dict get $traj x1]
            set y  [expr {[dict exists $traj y] ? [dict get $traj y] : 0.0}]
            dl_local mox [dl_add $x0 [dl_mult $ts [expr {($x1 - $x0) / $duration}]]]
            dl_local moy [dl_mult $y [dl_ones $n]]
            dl_return [dl_llist $mox $moy]
        }
        callback {
            # The user proc is called once per frame as {*}$cb $t and
            # may return either {x y} (position only) or {x y vx vy}
            # (position + velocity in patch-local units per second).
            # When velocity is supplied it is carried through as a
            # third/fourth llist element so compile_spec can use the
            # exact per-frame speed/direction instead of finite-
            # differencing the position channel. A trajectory with a
            # velocity discontinuity (e.g. a bounce) should return the
            # 4-tuple so the speed at the bounce frame is exact.
            set cb [dict get $traj proc]
            set xs [list]; set ys [list]
            set vxs [list]; set vys [list]
            set have_vel 1
            foreach t [dl_tcllist $ts] {
                set r [{*}$cb $t]
                lappend xs [lindex $r 0]
                lappend ys [lindex $r 1]
                if {[llength $r] >= 4} {
                    lappend vxs [lindex $r 2]
                    lappend vys [lindex $r 3]
                } else {
                    set have_vel 0
                }
            }
            dl_local mox [dl_flist {*}$xs]
            dl_local moy [dl_flist {*}$ys]
            if {$have_vel} {
                dl_local mvx [dl_flist {*}$vxs]
                dl_local mvy [dl_flist {*}$vys]
                dl_return [dl_llist $mox $moy $mvx $mvy]
            } else {
                dl_return [dl_llist $mox $moy]
            }
        }
        step_sequence {
            set positions  [dict get $traj positions]
            set step_times [dict get $traj step_times]
            if {[llength $step_times] != [llength $positions] + 1} {
                error "mp_sim::eval_trajectory step_sequence: step_times must have length(positions)+1"
            }
            # Build per-frame (mox, moy) by walking ts and looking up
            # which interval each t falls into. O(n_frames) with a
            # single pass; positions are typically << n_frames.
            set xs [list]; set ys [list]
            set npos [llength $positions]
            foreach t [dl_tcllist $ts] {
                set idx 0
                for {set i 0} {$i < $npos} {incr i} {
                    set t_lo [lindex $step_times $i]
                    set t_hi [lindex $step_times [expr {$i + 1}]]
                    if {$t >= $t_lo && $t < $t_hi} { set idx $i; break }
                }
                # If t is past all intervals, hold last position.
                if {$idx >= $npos} { set idx [expr {$npos - 1}] }
                lassign [lindex $positions $idx] x y
                lappend xs $x
                lappend ys $y
            }
            dl_local mox [dl_flist {*}$xs]
            dl_local moy [dl_flist {*}$ys]
            dl_return [dl_llist $mox $moy]
        }
        default { error "mp_sim::eval_trajectory: unknown kind '$kind'" }
    }
}

# mp_sim::trajectory_kinematics traj_pair ts
#   Given the llist returned by eval_trajectory ({mox moy} or
#   {mox moy mvx mvy}) and the time grid ts, return a 2-element llist
#   {speed dir}: per-frame speed (patch-local units/sec) and direction
#   (radians). When eval_trajectory supplied velocity (the 4-element
#   form) the kinematics are exact. Otherwise speed/direction are
#   recovered by finite-differencing the position channels.
#
#   Finite-difference note: a forward difference is used for the first
#   frame and a centred difference elsewhere, giving a smooth estimate
#   for continuous paths. Across a velocity discontinuity (e.g. a
#   bounce) the one straddling frame's speed is an average of the pre-
#   and post-discontinuity speeds -- callbacks that contain a bounce
#   should return the exact 4-tuple to avoid this. dt is taken from ts.
proc mp_sim::trajectory_kinematics {traj_pair ts} {
    set npair [dl_length $traj_pair]
    set n [dl_length $ts]
    if {$npair >= 4} {
        # Exact: velocity supplied.
        dl_local vx $traj_pair:2
        dl_local vy $traj_pair:3
        dl_local speed [dl_sqrt [dl_add [dl_mult $vx $vx] [dl_mult $vy $vy]]]
        dl_local dir   [dl_atan2 $vy $vx]
        dl_return [dl_llist $speed $dir]
    }
    # Finite-difference fallback from position channels.
    dl_local mox $traj_pair:0
    dl_local moy $traj_pair:1
    if {$n < 2} {
        dl_return [dl_llist [dl_zeros $n] [dl_zeros $n]]
    }
    # dt grid (assume uniform; ts[1]-ts[0]).
    set dt [expr {[dl_get $ts 1] - [dl_get $ts 0]}]
    if {$dt <= 0.0} { set dt 1.0 }
    # Centred difference. Build "next" and "prev" index lists explicitly
    # (clamped at the ends) rather than relying on a shift-direction
    # convention -- frame i uses indices min(i+1,n-1) and max(i-1,0).
    # Where the clamp collapses the pair (the two endpoints) the step
    # is 1 frame, elsewhere 2; we divide by the actual index gap so the
    # estimate is correct one-sided at the ends and centred inside.
    set idx_next [list]
    set idx_prev [list]
    set gap      [list]
    for {set i 0} {$i < $n} {incr i} {
        set inx [expr {$i + 1}] ; if {$inx > $n - 1} { set inx [expr {$n - 1}] }
        set ipv [expr {$i - 1}] ; if {$ipv < 0}       { set ipv 0 }
        lappend idx_next $inx
        lappend idx_prev $ipv
        lappend gap [expr {($inx - $ipv) * $dt}]
    }
    dl_local inx [dl_ilist {*}$idx_next]
    dl_local ipv [dl_ilist {*}$idx_prev]
    dl_local gap [dl_flist {*}$gap]
    dl_local dx [dl_div [dl_sub [dl_choose $mox $inx] [dl_choose $mox $ipv]] $gap]
    dl_local dy [dl_div [dl_sub [dl_choose $moy $inx] [dl_choose $moy $ipv]] $gap]
    dl_local speed [dl_sqrt [dl_add [dl_mult $dx $dx] [dl_mult $dy $dy]]]
    dl_local dir   [dl_atan2 $dy $dx]
    dl_return [dl_llist $speed $dir]
}

# ============================================================
# Layer A.2: callback / threshold-crossing pre-computation
# ============================================================
#
# A spec may include a `callbacks` field, a list of dicts each describing
# a threshold to monitor. compile_spec scans the per-frame envelope and
# pre-computes the frame indices at which each crossing occurs, storing
# them in the timeline dg under per-callback columns. Both headless
# run_trial and the live prescript can then dispatch by frame index
# without re-scanning.
#
# Callback dict format:
#   {name N threshold T direction (rising|falling|both) proc P}
# At dispatch, callback proc is called as:
#   {*}$P $name $frame_idx $t $value
proc mp_sim::compile_callbacks {timeline_dg coh_per_frame ts_per_frame callbacks} {
    set count 0
    foreach cb $callbacks {
        set name [dict get $cb name]
        set thr  [dict get $cb threshold]
        set dir  [expr {[dict exists $cb direction] ? [dict get $cb direction] : "rising"}]
        set p    [dict get $cb proc]
        set frames [list]
        set n [dl_length $coh_per_frame]
        # Find sign-change crossings of (coh - thr).
        # For rising: prev < thr AND curr >= thr.
        # For falling: prev >= thr AND curr < thr.
        for {set i 1} {$i < $n} {incr i} {
            set prev [dl_get $coh_per_frame [expr {$i - 1}]]
            set curr [dl_get $coh_per_frame $i]
            switch -- $dir {
                rising  { if {$prev <  $thr && $curr >= $thr} { lappend frames $i } }
                falling { if {$prev >= $thr && $curr <  $thr} { lappend frames $i } }
                both    {
                    if {$prev <  $thr && $curr >= $thr} { lappend frames $i }
                    if {$prev >= $thr && $curr <  $thr} { lappend frames $i }
                }
            }
        }
        # Persist into the timeline dg.
        dl_set $timeline_dg:callback_${count}_name      [dl_slist $name]
        dl_set $timeline_dg:callback_${count}_proc      [dl_slist $p]
        dl_set $timeline_dg:callback_${count}_threshold [dl_flist $thr]
        dl_set $timeline_dg:callback_${count}_direction [dl_slist $dir]
        if {[llength $frames] > 0} {
            dl_set $timeline_dg:callback_${count}_frames [dl_ilist {*}$frames]
        } else {
            dl_set $timeline_dg:callback_${count}_frames [dl_ilist]
        }
        incr count
    }
    dl_set $timeline_dg:callbacks_count [dl_ilist $count]
}

# mp_sim::dispatch_callbacks_at timeline frame_idx
#   For every callback registered in the timeline, if frame_idx is in
#   its frames list, fire the registered proc as
#       {*}$proc $name $frame_idx $t $value
#   Returns the list of fired callback names (useful for tests).
proc mp_sim::dispatch_callbacks_at {timeline frame_idx} {
    set fired [list]
    if {![dl_exists $timeline:callbacks_count]} { return $fired }
    set count [dl_get $timeline:callbacks_count 0]
    set t     [dl_get $timeline:t $frame_idx]
    set v     [dl_get $timeline:coherence $frame_idx]
    for {set k 0} {$k < $count} {incr k} {
        set frames_dl $timeline:callback_${k}_frames
        if {[dl_length $frames_dl] == 0} continue
        # Linear search; n_frames per callback is small in practice.
        foreach f [dl_tcllist $frames_dl] {
            if {$f == $frame_idx} {
                set name [dl_get $timeline:callback_${k}_name 0]
                set p    [dl_get $timeline:callback_${k}_proc 0]
                catch {{*}$p $name $frame_idx $t $v}
                lappend fired $name
                break
            }
        }
    }
    return $fired
}

# ============================================================
# Layer A: design-spec -> state timeline
# ============================================================

# mp_sim::envelope_sum_gaussians ts centers sigma_s base_coh
#   Sample a sum-of-N-Gaussians envelope at every t in ts. Returns a
#   dl flist of envelope values clamped to [0, base_coh].
proc mp_sim::envelope_sum_gaussians {ts centers sigma_s base_coh} {
    set n [dl_length $ts]
    if {$sigma_s <= 0.0 || [llength $centers] == 0} {
        return [dl_mult $base_coh [dl_ones $n]]
    }
    dl_local sum [dl_zeros $n]
    foreach c $centers {
        dl_local z [dl_div [dl_sub $ts $c] $sigma_s]
        dl_local g [dl_exp [dl_mult -0.5 [dl_mult $z $z]]]
        dl_local sum [dl_add $sum $g]
    }
    dl_local v [dl_mult $base_coh $sum]
    # Clamp to [0, base_coh]. Cap above first using a mask captured
    # BEFORE we mutate the values.
    dl_local hi [dl_gte $v $base_coh]
    dl_local v  [dl_add [dl_mult [dl_sub 1.0 $hi] $v] [dl_mult $hi $base_coh]]
    dl_local lo [dl_lt $v 0.0]
    dl_local v  [dl_mult [dl_sub 1.0 $lo] $v]
    dl_return $v
}

# mp_sim::evenly_spaced_pulse_centers n_pulses duration
#   N evenly-spaced centers at mid-interval positions in [0, duration]:
#   t_k = T*(k+0.5)/N for k = 0..N-1. Mirrors mp_pulsed.tcl.
proc mp_sim::evenly_spaced_pulse_centers {n_pulses duration} {
    set out [list]
    if {$n_pulses <= 0} { return $out }
    for {set k 0} {$k < $n_pulses} {incr k} {
        lappend out [expr {$duration * ($k + 0.5) / $n_pulses}]
    }
    return $out
}

# mp_sim::compile_spec spec ?-gname name?
#   Compile a high-level spec dict into a state-timeline dg.
#
#   Spec dict shape:
#     meta       {duration <sec> dt <sec> ?patch_size_dva <dva>?}
#     endpoints  {target   {coh <0..1> speed <pu/s> dir <rad> life <s>}
#                 surround {coh <0..1> speed <pu/s> dir <rad> life <s>}}
#     envelope   {kind sum_gaussians n_pulses <int> sigma_ms <ms>
#                 ?centers <list>? ?base_coh <0..1>?}
#                {kind flat ?base_coh <0..1>?}
#     trajectory {kind static ?x <pu>? ?y <pu>?}
#                {kind sweep x0 <pu> x1 <pu> ?y <pu>?}
#                {kind callback proc <procname>}
#
#   The endpoints' coh/speed/dir/life are tweened in lockstep by the
#   normalized envelope frac = coh/base_coh -- the "single knob across
#   multiple parameters" property that makes the trough state
#   statistically identical to the surround.
proc mp_sim::compile_spec {spec args} {
    array set opts {-gname {}}
    array set opts $args

    set meta       [dict get $spec meta]
    set endpoints  [dict get $spec endpoints]
    set envelope   [dict get $spec envelope]
    set trajectory [dict get $spec trajectory]

    set duration       [dict get $meta duration]
    set dt             [dict get $meta dt]
    set patch_size_dva [expr {[dict exists $meta patch_size_dva] ? [dict get $meta patch_size_dva] : 1.0}]

    if {$duration <= 0.0 || $dt <= 0.0} {
        error "mp_sim::compile_spec: duration and dt must be positive"
    }
    set n_frames [expr {int(round($duration / $dt)) + 1}]

    set tgt [dict get $endpoints target]
    set sur [dict get $endpoints surround]
    set base_coh [expr {[dict exists $envelope base_coh] ? [dict get $envelope base_coh] : [dict get $tgt coh]}]

    # Bounce: optional direction-discontinuity overlay. When the spec
    # contains a 'bounce' block, the smooth direction tween is replaced
    # by a step function -- pre_dir before bounce_t, post_dir after --
    # that simulates a sudden trajectory turn for the peak-vs-trough
    # leakage manipulation.
    set has_bounce [dict exists $spec bounce]
    if {$has_bounce} {
        set bounce [dict get $spec bounce]
    }

    # Time grid -- closed interval [0, n_frames*dt).
    dl_local ts [dl_mult $dt [dl_fromto 0 $n_frames]]

    # Envelope -- coherence at each t. Pre-resolve sum_gaussians centers
    # against the spec's duration so eval_envelope doesn't have to look
    # at ts to derive them. Also prepares centers for bounce phase
    # resolution and for tile_times metadata.
    set envelope_resolved [mp_sim::_resolve_envelope_durations $envelope $duration]
    dl_local coh [mp_sim::eval_envelope $envelope_resolved $ts]
    set centers [mp_sim::collect_tile_times $envelope_resolved $duration]

    # Tween fraction.
    if {$base_coh > 0.0} {
        dl_local frac [dl_div $coh $base_coh]
    } else {
        dl_local frac [dl_zeros $n_frames]
    }
    dl_local one_minus_frac [dl_sub 1.0 $frac]

    # Trajectory -> mask_offset(t). eval_trajectory returns a 2-element
    # llist {mox moy} or a 4-element {mox moy mvx mvy} of per-frame
    # offsets (and optionally velocities). Evaluated BEFORE the tween
    # so the trajectory's own per-frame speed/direction can drive the
    # coherent ("target") endpoint -- essential for non-constant-speed
    # paths such as a falling ball, where a fixed endpoint speed would
    # mismatch the rendered translation.
    dl_local _traj_pair [mp_sim::eval_trajectory $trajectory $ts $duration]
    dl_local mox $_traj_pair:0
    dl_local moy $_traj_pair:1
    dl_local _kin [mp_sim::trajectory_kinematics $_traj_pair $ts]
    dl_local traj_speed $_kin:0
    dl_local traj_dir   $_kin:1

    # Whether the trajectory carries meaningful per-frame motion. A
    # 'static' trajectory has ~zero speed everywhere; in that case we
    # fall back to the endpoint speed/direction (preserves the original
    # constant-patch behaviour and every existing spec). A trajectory
    # with motion supplies its own coherent-state speed and direction.
    #
    # step_sequence is explicitly excluded: it is piecewise-constant
    # (the patch holds position within a tile and jumps between tiles),
    # so finite-differencing would produce spurious one-frame speed
    # spikes at the jumps. RF-mapping specs built on step_sequence must
    # keep the endpoint-scalar speed/direction behaviour, including the
    # per-tile direction overlay applied by compile_mapping_spec.
    set traj_kind [dict get $trajectory kind]
    if {$traj_kind eq "step_sequence"} {
        set traj_has_motion 0
    } else {
        set traj_has_motion [expr {[dl_max $traj_speed] > 1e-9}]
    }

    # Linear tween of (speed, lifetime, direction) between surround and
    # the coherent "target" state, gated by frac. The coherent target
    # speed/direction is the trajectory's own per-frame value when the
    # trajectory has motion, else the endpoint scalar (back-compat).
    if {$traj_has_motion} {
        dl_local tgt_speed $traj_speed
    } else {
        dl_local tgt_speed [dl_mult [dict get $tgt speed] [dl_ones $n_frames]]
    }
    dl_local speed [dl_add [dl_mult $frac           $tgt_speed] \
                            [dl_mult $one_minus_frac [dict get $sur speed]]]
    dl_local life  [dl_add [dl_mult $frac           [dict get $tgt life]] \
                            [dl_mult $one_minus_frac [dict get $sur life]]]

    # Direction. With a moving trajectory the coherent-state direction
    # is the trajectory tangent; the surround direction still applies
    # in the trough via the tween. Without trajectory motion we keep
    # the original endpoint-scalar tween (back-compat).
    if {$traj_has_motion} {
        dl_local dir $traj_dir
    } else {
        set tdir [dict get $tgt dir]
        set sdir [dict get $sur dir]
        if {abs($tdir - $sdir) < 1e-12} {
            dl_local dir [dl_mult $tdir [dl_ones $n_frames]]
        } else {
            dl_local dir [dl_add [dl_mult $frac           $tdir] \
                                  [dl_mult $one_minus_frac $sdir]]
        }
    }

    # Bounce overrides direction. Sets a clean step function based on
    # phase=peak/trough/custom + pulse_index. We use the just-built
    # tile_times list to resolve phase, then overwrite direction.
    set bounce_t 0.0
    if {$has_bounce} {
        set b_phase    [expr {[dict exists $bounce phase]    ? [dict get $bounce phase]    : "custom"}]
        set b_idx      [expr {[dict exists $bounce pulse_index] ? [dict get $bounce pulse_index] : 0}]
        set b_t_custom [expr {[dict exists $bounce t_custom] ? [dict get $bounce t_custom] : 0.0}]
        set pre_dir    [expr {[dict exists $bounce pre_dir]  ? [dict get $bounce pre_dir]  : 0.0}]
        set post_dir   [expr {[dict exists $bounce post_dir] ? [dict get $bounce post_dir] : 0.0}]

        switch -- $b_phase {
            peak {
                if {[llength $centers] == 0} {
                    error "mp_sim::compile_spec: bounce phase=peak needs an envelope with pulses"
                }
                set k $b_idx
                if {$k < 0} { set k 0 }
                if {$k >= [llength $centers]} { set k [expr {[llength $centers] - 1}] }
                set bounce_t [lindex $centers $k]
            }
            trough {
                if {[llength $centers] < 2} {
                    error "mp_sim::compile_spec: bounce phase=trough needs >=2 pulses"
                }
                set k $b_idx
                if {$k < 0} { set k 0 }
                if {$k > [expr {[llength $centers] - 2}]} {
                    set k [expr {[llength $centers] - 2}]
                }
                set ta [lindex $centers $k]
                set tb [lindex $centers [expr {$k + 1}]]
                set bounce_t [expr {0.5 * ($ta + $tb)}]
            }
            custom { set bounce_t $b_t_custom }
            default { error "mp_sim::compile_spec: unknown bounce phase '$b_phase'" }
        }

        # Build step direction: pre_dir for t < bounce_t, post_dir for
        # t >= bounce_t. Vectorized via mask. This overrides the
        # trajectory-tangent direction computed above -- the bounce
        # block is the authority on the direction discontinuity. Note
        # the mask_offset (position) path is NOT altered here; a
        # callback trajectory that contains a real bounce supplies the
        # bent position path itself, and pre_dir/post_dir must be kept
        # consistent with it by the caller.
        dl_local mask [dl_gte $ts $bounce_t]
        dl_local dir [dl_add [dl_mult [dl_sub 1.0 $mask] $pre_dir] \
                              [dl_mult $mask $post_dir]]
    }

    # Build the dg.
    set gname $opts(-gname)
    if {$gname eq ""} { set gname [dg_tempname] }
    if {[dg_exists $gname]} { dg_delete $gname }
    set g [dg_create $gname]

    dl_set $g:t              $ts
    dl_set $g:mask_offset_x  $mox
    dl_set $g:mask_offset_y  $moy
    dl_set $g:direction      $dir
    dl_set $g:coherence      $coh
    dl_set $g:speed          $speed
    dl_set $g:lifetime_s     $life

    # Group-level scalars (1-element lists).
    dl_set $g:dt              [dl_flist $dt]
    dl_set $g:n_frames        [dl_ilist $n_frames]
    dl_set $g:patch_size_dva  [dl_flist $patch_size_dva]
    dl_set $g:duration        [dl_flist $duration]
    dl_set $g:base_coh        [dl_flist $base_coh]
    if {[llength $centers] > 0} {
        dl_set $g:tile_times [dl_flist {*}$centers]
    } else {
        dl_set $g:tile_times [dl_flist]
    }
    if {$has_bounce} {
        dl_set $g:bounce_t        [dl_flist $bounce_t]
        dl_set $g:bounce_pre_dir  [dl_flist $pre_dir]
        dl_set $g:bounce_post_dir [dl_flist $post_dir]
        dl_set $g:bounce_phase    [dl_slist $b_phase]
    }
    # Pre-compute threshold-crossing frames for each registered callback.
    # The live prescript / headless run_trial can dispatch by frame
    # index without re-scanning the envelope.
    set callbacks [expr {[dict exists $spec callbacks] ? [dict get $spec callbacks] : [list]}]
    mp_sim::compile_callbacks $g $coh $ts $callbacks
    return $g
}

# mp_sim::compile_mapping_spec base_spec ?-positions L? ?-on_dur D? ?-off_dur D?
#                              ?-ease_dur D? ?-direction R? ?-on_callback P?
#                              ?-off_callback P? ?-base_coh V?
#                              ?-pre_dur D? ?-gname N?
#
#   Convenience wrapper that builds an envelope=trapezoid_train +
#   trajectory=step_sequence consistent with each other for a "rapid
#   RF mapping with persistent surround" experiment.
#
#   Per-location duty cycle:
#     [pre_dur][ease][on_dur][ease][off_dur] = "tile"
#   The pulse train has one tile per position; positions step in OFF
#   windows so position changes never overlap a coherent plateau.
#
#   Callback wiring:
#     -on_callback  : fires when envelope crosses 0.5*base_coh rising  (effective patch-on)
#     -off_callback : fires when envelope crosses 0.5*base_coh falling (effective patch-off)
#     Each receives {name frame_idx t value}; consumers can derive row
#     index from frame timing or position lookup.
#
#   Returns the timeline dg name.
proc mp_sim::compile_mapping_spec {base_spec args} {
    array set opts {
        -positions     {{0 0}}
        -on_dur        0.150
        -off_dur       0.050
        -ease_dur      0.050
        -direction     0.0
        -directions    {}
        -on_callback   {}
        -off_callback  {}
        -base_coh      1.0
        -pre_dur       0.100
        -gname         {}
    }
    array set opts $args

    set positions [list]
    foreach p $opts(-positions) {
        if {[llength $p] != 2} {
            error "mp_sim::compile_mapping_spec: each position must be {x y}"
        }
        lappend positions $p
    }
    set npos [llength $positions]
    if {$npos < 1} { error "mp_sim::compile_mapping_spec: need >= 1 position" }

    # -directions: per-tile target direction (radians). When provided,
    # length must match positions; the timeline's direction column is
    # overwritten with each tile's direction so the patch points the
    # configured way at each location. When omitted, the scalar
    # -direction applies uniformly.
    set per_tile_dirs $opts(-directions)
    if {[llength $per_tile_dirs] > 0 && [llength $per_tile_dirs] != $npos} {
        error "mp_sim::compile_mapping_spec: -directions length ([llength $per_tile_dirs]) must match -positions ($npos)"
    }

    set on   $opts(-on_dur)
    set off  $opts(-off_dur)
    set ease $opts(-ease_dur)
    set pre  $opts(-pre_dur)

    # Tile = ease + on + ease + off. Center of the plateau is at
    # offset (pre + ease + on/2) from t=0. Each subsequent tile starts
    # at the previous tile's end.
    set tile [expr {$on + 2.0 * $ease + $off}]
    set duration [expr {$pre + $tile * $npos}]

    set centers   [list]
    set step_times [list 0.0]   ;# n+1 boundaries
    for {set k 0} {$k < $npos} {incr k} {
        set tile_start [expr {$pre + $k * $tile}]
        set plateau_center [expr {$tile_start + $ease + $on / 2.0}]
        lappend centers $plateau_center
        # Position k spans the entire tile (its rise, plateau, and
        # fall). The position SWITCH (k -> k+1) happens at the start
        # of the next tile's pre-rise OFF window, which is during the
        # current tile's `off` segment.
        if {$k < $npos - 1} {
            lappend step_times [expr {$tile_start + 2.0 * $ease + $on + $off / 2.0}]
        } else {
            lappend step_times $duration
        }
    }

    # Synthesize the envelope and trajectory dicts.
    set envelope [dict create \
        kind        trapezoid_train \
        centers     $centers \
        plateau_dur $on \
        ease_dur    $ease \
        base_coh    $opts(-base_coh)]
    set trajectory [dict create \
        kind        step_sequence \
        positions   $positions \
        step_times  $step_times]

    # Endpoints: target carries the configured direction, surround is
    # whatever was in base_spec. If base_spec doesn't set direction, we
    # write the configured direction into target.
    if {[dict exists $base_spec endpoints]} {
        set endpoints [dict get $base_spec endpoints]
    } else {
        set endpoints [dict create \
            target   {coh 1.0 speed 0.6 dir 0.0 life 0.5} \
            surround {coh 0.0 speed 0.23 dir 0.0 life 0.08}]
    }
    set tgt [dict get $endpoints target]
    dict set tgt dir $opts(-direction)
    dict set endpoints target $tgt

    # Callbacks
    set callbacks [list]
    set thr [expr {0.5 * $opts(-base_coh)}]
    if {$opts(-on_callback) ne ""} {
        lappend callbacks [dict create \
            name patch_on threshold $thr direction rising \
            proc $opts(-on_callback)]
    }
    if {$opts(-off_callback) ne ""} {
        lappend callbacks [dict create \
            name patch_off threshold $thr direction falling \
            proc $opts(-off_callback)]
    }

    # Final spec: take meta from base_spec but override duration with
    # what we computed.
    set meta [dict create \
        duration       $duration \
        dt             [expr {[dict exists $base_spec meta] && [dict exists [dict get $base_spec meta] dt] ? [dict get $base_spec meta dt] : 0.0167}] \
        patch_size_dva [expr {[dict exists $base_spec meta] && [dict exists [dict get $base_spec meta] patch_size_dva] ? [dict get $base_spec meta patch_size_dva] : 1.0}]]

    set spec [dict create \
        meta       $meta \
        endpoints  $endpoints \
        envelope   $envelope \
        trajectory $trajectory \
        callbacks  $callbacks]

    set tl [mp_sim::compile_spec $spec -gname $opts(-gname)]

    # Per-tile direction overlay: write each tile's direction into the
    # timeline's `direction` column. step_times divides time into
    # npos+1 boundaries; tile k spans [step_times[k], step_times[k+1]).
    if {[llength $per_tile_dirs] > 0} {
        set ts [dl_tcllist $tl:t]
        set new_dir [list]
        foreach t $ts {
            # Find which tile this t belongs to. Linear scan is fine
            # for the modest position counts we use (< ~50).
            set k 0
            for {set i 0} {$i < $npos} {incr i} {
                set t_lo [lindex $step_times $i]
                set t_hi [lindex $step_times [expr {$i + 1}]]
                if {$t >= $t_lo && $t < $t_hi} { set k $i; break }
                if {$i == $npos - 1 && $t >= $t_hi} { set k $i }
            }
            lappend new_dir [lindex $per_tile_dirs $k]
        }
        dl_set $tl:direction [dl_flist {*}$new_dir]
        # Persist the per-tile direction list as a metadata column for
        # downstream consumers / analysis.
        dl_set $tl:tile_directions [dl_flist {*}$per_tile_dirs]
    }

    return $tl
}

# Stash the source path so ::mp_sim_reload can find it after the
# namespace gets blown away. This file is sourced (not loaded) by the
# pkgIndex, so $dir is the pkg directory at this point in evaluation
# -- typically /Users/sheinb/src/dlsh/vfs/lib/mp_sim during dev, or
# the zipfs root path during deployed runs.
set ::__mp_sim_pkg_dir [file dirname [info script]]

# ::mp_sim_reload  (global, NOT in mp_sim:: namespace -- needs to
# survive the namespace delete it triggers).
#   Force a fresh re-source of mp_sim.tcl by forgetting the package and
#   wiping its namespace, then re-requiring. Useful in a long-running
#   tkcon when iterating on the package on disk.
#
#   The auto_path is augmented with the parent of the captured pkg
#   directory so a fresh `package require mp_sim` finds the on-disk
#   pkgIndex.tcl. If mp_sim was loaded from inside the zipfs and the
#   user wants to override with a checkout, set ::__mp_sim_pkg_dir to
#   the dev location BEFORE calling mp_sim_reload.
proc ::mp_sim_reload {} {
    set pkg_dir $::__mp_sim_pkg_dir
    package forget mp_sim
    catch {namespace delete ::mp_sim}
    set parent [file dirname $pkg_dir]
    if {[lsearch -exact $::auto_path $parent] < 0} {
        set ::auto_path [linsert $::auto_path 0 $parent]
    }
    package require mp_sim
    return [package present mp_sim]
}

# ============================================================
# Visualization helpers
# ============================================================

# mp_sim::colorize_to_image values ?-cmap MAP? ?-vmin V? ?-vmax V?
#   Map a 1D dl of values onto an RGB image suitable for dlg_image.
#   Returns a packed char-list of length 3*N (R,G,B,R,G,B,...). Mirrors
#   the planko_trials.tcl idiom; works with any dlsh colormap name
#   (VIRIDIS, JET, BWR, etc., resolved via dlg_heatmap).
#
#   With -vmin / -vmax provided, values are mapped onto the colormap
#   linearly and clamped to [vmin, vmax]; otherwise the input's own
#   min/max are used. Use -vmin/-vmax when you want consistent coloring
#   across multiple heatmaps (e.g. side-by-side panels).
proc mp_sim::colorize_to_image {values args} {
    array set opts {-cmap VIRIDIS -vmin {} -vmax {}}
    array set opts $args
    dl_local heatmap [dlg_heatmap $opts(-cmap)]
    set nsteps [dl_length $heatmap:0]

    set vmin $opts(-vmin)
    set vmax $opts(-vmax)
    if {$vmin eq ""} { set vmin [dl_min $values] }
    if {$vmax eq ""} { set vmax [dl_max $values] }
    set vrange [expr {$vmax - $vmin}]
    set ndata [dl_length $values]

    if {$vrange < 1e-10} {
        dl_local indices [dl_replicate [expr {$nsteps/2}] $ndata]
    } else {
        dl_local clamped [dl_div [dl_sub $values $vmin] $vrange]
        # Clamp to [0, 1].
        dl_local clamped [dl_mult [dl_lt $clamped 1.0] $clamped]
        dl_local clamped [dl_add $clamped [dl_mult [dl_gte $clamped 1.0] 1.0]]
        dl_local clamped [dl_mult [dl_gte $clamped 0.0] $clamped]
        dl_local indices [dl_int [dl_mult $clamped [expr {$nsteps - 1}]]]
    }

    dl_local r [dl_choose $heatmap:0 $indices]
    dl_local g [dl_choose $heatmap:1 $indices]
    dl_local b [dl_choose $heatmap:2 $indices]
    # Interleave RGB triplets and pack as char.
    dl_return [dl_char [dl_collapse [dl_transpose [dl_llist $r $g $b]]]]
}

# mp_sim::draw_heatmap cx cy values width height nx ny ?-cmap M? ?-vmin V? ?-vmax V?
#   Render a 2D grid of values as a heatmap centered at (cx, cy) with
#   the given world-space width and height. values is a flat dl list of
#   length nx*ny in COLUMN-MAJOR order: cell at (col, row) = (xi, yi)
#   is at index xi*ny + yi. (This matches mp_sim::_grid_cells which
#   walks the first vary key fastest -- so a sweep over keys
#   {sigma_ms, n_dots} produces (sigma, n_dots) pairs in column-major
#   order with sigma indexing columns and n_dots indexing rows.)
#
#   Drawn as nx*ny filled rectangles via filledrect rather than
#   dlg_image, which gave us strange interpolation artefacts with very
#   small images. Filled rectangles are pixel-exact regardless of
#   resolution.
#
#   Caller is responsible for the surrounding axis/labels; this proc
#   only draws the colored cells.
proc mp_sim::draw_heatmap {cx cy values width height nx ny args} {
    array set opts {-cmap VIRIDIS -vmin {} -vmax {}}
    array set opts $args
    dl_local heatmap [dlg_heatmap $opts(-cmap)]
    set nsteps [dl_length $heatmap:0]

    set vmin $opts(-vmin)
    set vmax $opts(-vmax)
    if {$vmin eq ""} { set vmin [dl_min $values] }
    if {$vmax eq ""} { set vmax [dl_max $values] }
    set vrange [expr {$vmax - $vmin}]
    if {$vrange < 1e-10} { set vrange 1.0 }

    set cell_w [expr {$width  / double($nx)}]
    set cell_h [expr {$height / double($ny)}]
    set xL0    [expr {$cx - $width  / 2.0}]
    set yL0    [expr {$cy - $height / 2.0}]

    # Precompute color table for fast per-cell lookup.
    dl_local r_tab [dl_int $heatmap:0]
    dl_local g_tab [dl_int $heatmap:1]
    dl_local b_tab [dl_int $heatmap:2]

    set n [dl_length $values]
    for {set k 0} {$k < $n} {incr k} {
        set v [dl_get $values $k]
        set norm [expr {($v - $vmin) / $vrange}]
        if {$norm < 0.0} { set norm 0.0 }
        if {$norm > 1.0} { set norm 1.0 }
        set ci [expr {int($norm * ($nsteps - 1))}]
        set rr [dl_get $r_tab $ci]
        set gg [dl_get $g_tab $ci]
        set bb [dl_get $b_tab $ci]
        # Column-major: k = xi*ny + yi
        set xi [expr {$k / $ny}]
        set yi [expr {$k % $ny}]
        set xL [expr {$xL0 + $xi * $cell_w}]
        set yL [expr {$yL0 + $yi * $cell_h}]
        set xR [expr {$xL + $cell_w}]
        set yR [expr {$yL + $cell_h}]
        setcolor [dlg_rgbcolor $rr $gg $bb]
        filledrect $xL $yL $xR $yR
    }
}

# mp_sim::leakage_projection ens_dg post_dir
#   Given an ensemble dg and a post-bounce direction (radians), compute
#   per-frame "new-direction signal":
#     proj(t) = mean_dx(t) * cos(post_dir) + mean_dy(t) * sin(post_dir)
#   averaged across trials. Returns a dl name (kept persistent in the
#   ens_dg under the column 'proj_post_dir', so it survives this proc).
#
#   This is the natural metric for "how much motion energy in the new
#   direction is being delivered at frame t" -- the signal a downstream
#   direction-tuned cell tuned to post_dir would integrate. At a peak
#   bounce instant, we expect proj ~= speed * coh_recorded (full peak
#   delivery). At a trough bounce instant, we expect proj ~= speed *
#   c_trough -- the leakage.
proc mp_sim::leakage_projection {ens_dg post_dir} {
    dl_local mean_dx [dl_means [dl_transpose $ens_dg:dx_mean]]
    dl_local mean_dy [dl_means [dl_transpose $ens_dg:dy_mean]]
    set c [expr {cos($post_dir)}]
    set s [expr {sin($post_dir)}]
    dl_local proj [dl_add [dl_mult $mean_dx $c] [dl_mult $mean_dy $s]]
    dl_set $ens_dg:proj_post_dir $proj
    return $ens_dg:proj_post_dir
}

# mp_sim::validate_timeline gname
#   Check required state-timeline columns exist and are length-aligned.
#   Throws on failure with a descriptive message.
proc mp_sim::validate_timeline {gname} {
    set required {t mask_offset_x mask_offset_y direction coherence speed lifetime_s}
    foreach col $required {
        if {![dl_exists $gname:$col]} {
            error "mp_sim::validate_timeline: missing column '$col' in $gname"
        }
    }
    set n [dl_length $gname:t]
    foreach col $required {
        if {[dl_length $gname:$col] != $n} {
            error "mp_sim::validate_timeline: column '$col' length mismatch ($n expected)"
        }
    }
    return 1
}

# ============================================================
# Layer B: state timeline -> per-trial dot-field log
# ============================================================

# Toroidal wrap of a position dl to [-0.5, 0.5). Implemented via floor-
# based shift so it's correct for arbitrary FP values, including those
# that wrap multiple cells in a single frame after a large dt.
proc mp_sim::_wrap_pos {p} {
    dl_local p [dl_add $p 0.5]
    dl_local p [dl_sub $p [dl_floor $p]]
    dl_local p [dl_sub $p 0.5]
    dl_return $p
}

# Dot state is held in a persistent scratch dg with columns
# {x, y, theta, coherent}. Storing in a dg (rather than passing
# dl_local references between procs) is essential: dl_local dynlists
# are auto-deleted when their owning proc exits, so a dict of names
# returned from a helper proc would carry dangling references. The
# scratch dg lives until the trial ends and is deleted explicitly.

# mp_sim::_init_dot_state state_dg n_dots coherence direction direction_jitter
#   Populate state_dg with a fresh dot field consistent with
#   motionpatch.c initial conditions: positions uniform in [-0.5,
#   0.5]^2; floor(coherence*n_dots) random dots flagged coherent;
#   coherent thetas ~ N(0, jitter); incoherent thetas ~ U(0, 2pi).
proc mp_sim::_init_dot_state {state_dg n_dots coherence direction direction_jitter} {
    variable pi
    dl_local x [dl_sub [dl_urand $n_dots] 0.5]
    dl_local y [dl_sub [dl_urand $n_dots] 0.5]

    set n_coh [expr {int(floor($coherence * $n_dots + 0.5))}]
    if {$n_coh < 0} { set n_coh 0 }
    if {$n_coh > $n_dots} { set n_coh $n_dots }
    if {$n_coh == 0} {
        dl_local coh_flag [dl_zeros $n_dots]
    } elseif {$n_coh == $n_dots} {
        dl_local coh_flag [dl_ones $n_dots]
    } else {
        dl_local rkey [dl_urand $n_dots]
        dl_local sidx [dl_sortIndices $rkey]
        dl_local picks [dl_choose $sidx [dl_fromto 0 $n_coh]]
        dl_local coh_flag [dl_zeros $n_dots]
        dl_local coh_flag [dl_replaceByIndex $coh_flag $picks [dl_ones $n_coh]]
    }

    dl_local theta_coh [dl_mult $direction_jitter [dl_zrand $n_dots]]
    dl_local theta_inc [dl_mult [expr {2.0 * $pi}] [dl_urand $n_dots]]
    dl_local theta [dl_add [dl_mult $coh_flag $theta_coh] \
                            [dl_mult [dl_sub 1.0 $coh_flag] $theta_inc]]

    dl_set $state_dg:x        $x
    dl_set $state_dg:y        $y
    dl_set $state_dg:theta    $theta
    dl_set $state_dg:coherent $coh_flag
    return
}

# mp_sim::_step state_dg direction speed lifetime_s coherence dt jitter
#   Advance the dot field one frame, mutating state_dg's columns in
#   place. Order:
#     1. Coherence rebalance (stable membership: flip the minimum
#        number of flags; resample theta only on flipped dots)
#     2. Poisson respawn mask
#     3. Integrate all dots once (vectorized)
#     4. Toroidal wrap to [-0.5, 0.5)
#     5. Overwrite respawn slots with fresh positions / thetas
proc mp_sim::_step {state_dg direction speed lifetime_s coherence dt direction_jitter} {
    variable pi
    set n [dl_length $state_dg:x]

    # ---- 1. Coherence rebalance ----
    set target  [expr {int(floor($coherence * $n + 0.5))}]
    if {$target < 0} { set target 0 }
    if {$target > $n} { set target $n }
    set current [expr {int(round([dl_sum $state_dg:coherent]))}]
    set delta   [expr {$target - $current}]
    if {$delta != 0} {
        if {$delta > 0} {
            dl_local cand [dl_indices [dl_eq $state_dg:coherent 0]]
            set flip_count $delta
        } else {
            dl_local cand [dl_indices [dl_eq $state_dg:coherent 1]]
            set flip_count [expr {-$delta}]
        }
        set ncand [dl_length $cand]
        if {$ncand >= $flip_count && $flip_count > 0} {
            dl_local rkey [dl_urand $ncand]
            dl_local sidx [dl_sortIndices $rkey]
            dl_local picks [dl_choose $cand [dl_choose $sidx [dl_fromto 0 $flip_count]]]
            if {$delta > 0} {
                dl_local cf2 [dl_replaceByIndex $state_dg:coherent $picks [dl_ones $flip_count]]
                dl_local fresh_th [dl_mult $direction_jitter [dl_zrand $flip_count]]
                dl_local th2 [dl_replaceByIndex $state_dg:theta $picks $fresh_th]
            } else {
                dl_local cf2 [dl_replaceByIndex $state_dg:coherent $picks [dl_zeros $flip_count]]
                dl_local fresh_th [dl_mult [expr {2.0 * $pi}] [dl_urand $flip_count]]
                dl_local th2 [dl_replaceByIndex $state_dg:theta $picks $fresh_th]
            }
            dl_set $state_dg:coherent $cf2
            dl_set $state_dg:theta    $th2
        }
    }

    # ---- 2. Respawn mask ----
    set p_respawn 0.0
    if {$lifetime_s > 0.0} {
        set p_respawn [expr {$dt / $lifetime_s}]
        if {$p_respawn > 1.0} { set p_respawn 1.0 }
    }
    if {$p_respawn > 0.0} {
        dl_local respawn [dl_lt [dl_urand $n] $p_respawn]
    } else {
        dl_local respawn [dl_zeros $n]
    }

    # ---- 3. Integrate all dots ----
    # angle = (coherent ? direction + theta : theta). Direction is a
    # scalar; theta is per-dot. cf*direction adds direction only to
    # coherent dots.
    dl_local angle [dl_add $state_dg:theta \
                            [dl_mult $state_dg:coherent $direction]]
    set v [expr {$speed * $dt}]
    dl_local nx [dl_add $state_dg:x [dl_mult [dl_cos $angle] $v]]
    dl_local ny [dl_add $state_dg:y [dl_mult [dl_sin $angle] $v]]

    # ---- 4. Wrap ----
    dl_local nx [mp_sim::_wrap_pos $nx]
    dl_local ny [mp_sim::_wrap_pos $ny]

    # ---- 5. Respawn overwrites ----
    dl_local respawn_idx [dl_indices $respawn]
    set n_respawn [dl_length $respawn_idx]
    if {$n_respawn > 0} {
        dl_local rx [dl_sub [dl_urand $n_respawn] 0.5]
        dl_local ry [dl_sub [dl_urand $n_respawn] 0.5]
        dl_local nx [dl_replaceByIndex $nx $respawn_idx $rx]
        dl_local ny [dl_replaceByIndex $ny $respawn_idx $ry]
        dl_local cf_re [dl_choose $state_dg:coherent $respawn_idx]
        dl_local th_coh [dl_mult $direction_jitter [dl_zrand $n_respawn]]
        dl_local th_inc [dl_mult [expr {2.0 * $pi}] [dl_urand $n_respawn]]
        dl_local th_re [dl_add [dl_mult $cf_re $th_coh] \
                                [dl_mult [dl_sub 1.0 $cf_re] $th_inc]]
        dl_local th_new [dl_replaceByIndex $state_dg:theta $respawn_idx $th_re]
        dl_set $state_dg:theta $th_new
    }

    dl_set $state_dg:x $nx
    dl_set $state_dg:y $ny
    return
}

# mp_sim::_compute_frame_summary state_dg direction speed dt n
#   Compute per-frame summary stats from the current dot state. Returns
#   a Tcl dict with: coh_recorded, dir_mean_coh, dir_circ_var_coh,
#   dir_mean_inc, dx_mean, dy_mean, speed_mean, alive_count.
#
#   Math:
#     angle_i  = direction + theta_i   for coherent dots
#               theta_i                for incoherent dots
#     coh_recorded     = (#coherent) / N
#     dir_mean_coh     = atan2(sum sin angle_i over coherent,
#                              sum cos angle_i over coherent)
#     dir_circ_var_coh = 1 - sqrt(C^2 + S^2) / n_coh    (coherent only)
#     dx_mean / dy_mean = population mean per-dot velocity vector,
#                         (1/N) * sum [cos|sin](angle_i) * speed
#                         -- the "delivered motion vector" you'd get
#                         out of an ideal motion-energy pooling.
#
#   speed and dt are passed in scalar form so the kernel doesn't need
#   to look them up again.
proc mp_sim::_compute_frame_summary {state_dg direction speed dt n} {
    set n_coh_int [expr {int(round([dl_sum $state_dg:coherent]))}]
    set coh_recorded [expr {double($n_coh_int) / double($n)}]

    # angle = theta + coherent * direction (vectorized: incoherent dots
    # get +0, coherent get +direction).
    dl_local angle [dl_add $state_dg:theta \
                            [dl_mult $state_dg:coherent $direction]]
    dl_local ca [dl_cos $angle]
    dl_local sa [dl_sin $angle]

    set sum_cos_all [dl_sum $ca]
    set sum_sin_all [dl_sum $sa]

    # Coherent-only sums via mask multiplication.
    set sum_cos_coh [dl_sum [dl_mult $ca $state_dg:coherent]]
    set sum_sin_coh [dl_sum [dl_mult $sa $state_dg:coherent]]

    if {$n_coh_int > 0} {
        set dir_mean_coh     [expr {atan2($sum_sin_coh, $sum_cos_coh)}]
        set R_coh            [expr {hypot($sum_cos_coh, $sum_sin_coh) / double($n_coh_int)}]
        set dir_circ_var_coh [expr {1.0 - $R_coh}]
    } else {
        set dir_mean_coh     0.0
        set dir_circ_var_coh 0.0
    }

    # Incoherent dots: angle = theta only.
    set n_inc_int [expr {$n - $n_coh_int}]
    if {$n_inc_int > 0} {
        # incoherent mask = 1 - coherent
        dl_local inc_mask [dl_sub 1.0 $state_dg:coherent]
        # For incoherent dots, angle == theta, so we can just use
        # cos/sin of theta. Recompute to keep the mask multiplication
        # correct -- the +direction term added zero for incoherent dots
        # because of cf, so ca/sa already encode theta on those rows.
        set sum_cos_inc [dl_sum [dl_mult $ca $inc_mask]]
        set sum_sin_inc [dl_sum [dl_mult $sa $inc_mask]]
        set dir_mean_inc [expr {atan2($sum_sin_inc, $sum_cos_inc)}]
    } else {
        set dir_mean_inc 0.0
    }

    set dx_mean [expr {$sum_cos_all * $speed / double($n)}]
    set dy_mean [expr {$sum_sin_all * $speed / double($n)}]
    set speed_mean $speed
    set alive_count $n

    return [dict create \
        coh_recorded     $coh_recorded \
        dir_mean_coh     $dir_mean_coh \
        dir_circ_var_coh $dir_circ_var_coh \
        dir_mean_inc     $dir_mean_inc \
        dx_mean          $dx_mean \
        dy_mean          $dy_mean \
        speed_mean       $speed_mean \
        alive_count      $alive_count]
}

# mp_sim::_run_trial_compute timeline n_dots seed jitter record_dots
#   Internal: run one trial and return a Tcl dict with per-frame
#   accumulator lists (one entry per frame). When record_dots is true,
#   also accumulates 2-deep nested lists of dot positions/thetas/flags
#   (outer = frame, inner = dot).
#
#   Splits cleanly from any dg-building so the ensemble layer can call
#   this directly without paying per-trial dg create/delete overhead.
proc mp_sim::_run_trial_compute {timeline n_dots seed jitter record_dots} {
    if {$seed ne ""} { dl_srand $seed }

    set nframes [dl_length $timeline:t]
    set dt     [dl_get $timeline:dt 0]

    set state_dg [dg_tempname]
    catch {dg_delete $state_dg}
    dg_create $state_dg

    set coh0 [dl_get $timeline:coherence 0]
    set dir0 [dl_get $timeline:direction 0]

    # Per-frame accumulator lists.
    set acc_t                [list]
    set acc_coh_recorded     [list]
    set acc_dir_mean_coh     [list]
    set acc_dir_circ_var_coh [list]
    set acc_dir_mean_inc     [list]
    set acc_dx_mean          [list]
    set acc_dy_mean          [list]
    set acc_speed_mean       [list]
    set acc_alive_count      [list]
    # Per-frame dot fields (only populated when record_dots is true);
    # each entry is itself a list of n_dots values.
    set acc_dot_x  [list]
    set acc_dot_y  [list]
    set acc_dot_th [list]
    set acc_dot_cf [list]

    try {
        mp_sim::_init_dot_state $state_dg $n_dots $coh0 $dir0 $jitter

        for {set i 0} {$i < $nframes} {incr i} {
            set t   [dl_get $timeline:t $i]
            set dir [dl_get $timeline:direction $i]
            set sp  [dl_get $timeline:speed $i]
            set lf  [dl_get $timeline:lifetime_s $i]
            set ch  [dl_get $timeline:coherence $i]
            if {$i > 0} {
                mp_sim::_step $state_dg $dir $sp $lf $ch $dt $jitter
            }
            set s [mp_sim::_compute_frame_summary $state_dg $dir $sp $dt $n_dots]
            lappend acc_t                $t
            lappend acc_coh_recorded     [dict get $s coh_recorded]
            lappend acc_dir_mean_coh     [dict get $s dir_mean_coh]
            lappend acc_dir_circ_var_coh [dict get $s dir_circ_var_coh]
            lappend acc_dir_mean_inc     [dict get $s dir_mean_inc]
            lappend acc_dx_mean          [dict get $s dx_mean]
            lappend acc_dy_mean          [dict get $s dy_mean]
            lappend acc_speed_mean       [dict get $s speed_mean]
            lappend acc_alive_count      [dict get $s alive_count]
            if {$record_dots} {
                lappend acc_dot_x  [dl_tcllist $state_dg:x]
                lappend acc_dot_y  [dl_tcllist $state_dg:y]
                lappend acc_dot_th [dl_tcllist $state_dg:theta]
                lappend acc_dot_cf [dl_tcllist $state_dg:coherent]
            }
        }
    } finally {
        catch {dg_delete $state_dg}
    }

    return [dict create \
        t                $acc_t \
        coh_recorded     $acc_coh_recorded \
        dir_mean_coh     $acc_dir_mean_coh \
        dir_circ_var_coh $acc_dir_circ_var_coh \
        dir_mean_inc     $acc_dir_mean_inc \
        dx_mean          $acc_dx_mean \
        dy_mean          $acc_dy_mean \
        speed_mean       $acc_speed_mean \
        alive_count      $acc_alive_count \
        dot_x            $acc_dot_x \
        dot_y            $acc_dot_y \
        dot_theta        $acc_dot_th \
        dot_coherent     $acc_dot_cf]
}

# mp_sim::run_trial timeline ?-n_dots N? ?-seed S? ?-direction_jitter J?
#                            ?-gname name? ?-record_dots BOOL?
#   Run one trial. Returns a single-row ensemble dg containing per-frame
#   summary stats and (if -record_dots 1) per-frame raw dot positions
#   nested as frames-of-dots-lists.
proc mp_sim::run_trial {timeline args} {
    array set opts {-n_dots 1000 -seed {} -direction_jitter 0.0 \
                    -gname {} -record_dots 0}
    array set opts $args

    mp_sim::validate_timeline $timeline

    set data [mp_sim::_run_trial_compute $timeline $opts(-n_dots) \
                  $opts(-seed) $opts(-direction_jitter) $opts(-record_dots)]

    set gname $opts(-gname)
    if {$gname eq ""} { set gname [dg_tempname] }
    catch {dg_delete $gname}
    set g [dg_create $gname]

    # Per-trial scalars (single-element lists -- this is the row).
    set seed_val [expr {$opts(-seed) eq "" ? -1 : $opts(-seed)}]
    dl_set $g:trial_id [dl_ilist 0]
    dl_set $g:seed     [dl_ilist $seed_val]
    dl_set $g:n_dots   [dl_ilist $opts(-n_dots)]

    # Per-frame timecourses, one nested entry per row.
    foreach col {coh_recorded dir_mean_coh dir_circ_var_coh dir_mean_inc \
                 dx_mean dy_mean speed_mean} {
        dl_set $g:$col [dl_llist [dl_flist {*}[dict get $data $col]]]
    }
    dl_set $g:alive_count [dl_llist [dl_ilist {*}[dict get $data alive_count]]]

    if {$opts(-record_dots)} {
        # 3-deep: row -> frame -> dot. Build the per-frame lists first,
        # then wrap them in an outer 1-element llist.
        foreach {col srckey type} {
            dot_x        dot_x        flist
            dot_y        dot_y        flist
            dot_theta    dot_theta    flist
            dot_coherent dot_coherent ilist
        } {
            set frames [list]
            foreach row [dict get $data $srckey] {
                if {$type eq "ilist"} {
                    lappend frames [dl_ilist {*}$row]
                } else {
                    lappend frames [dl_flist {*}$row]
                }
            }
            dl_set $g:$col [dl_llist [dl_llist {*}$frames]]
        }
    }

    # Group-level: timeline columns + metadata. Stored unwrapped so they
    # can be plotted directly against any single-trial timecourse.
    foreach col {t mask_offset_x mask_offset_y direction coherence speed lifetime_s} {
        dl_set $g:${col}_design $timeline:$col
    }
    foreach col {patch_size_dva duration base_coh tile_times n_frames dt} {
        if {[dl_exists $timeline:$col]} {
            dl_set $g:$col $timeline:$col
        }
    }
    dl_set $g:n_trials [dl_ilist 1]
    return $g
}

# mp_sim::ensemble timeline ?-n_dots N? ?-n_trials T? ?-seed S?
#                           ?-direction_jitter J? ?-gname name?
#                           ?-record_dots BOOL?
#   Run T trials and accumulate them into one nested dg with T rows.
#   Schema: see top-of-file docs. With record_dots off (default), a
#   1000-trial ensemble is light enough to keep entirely in memory and
#   pass to plotting / dl_means / dl_stds for figure-1 panels.
#
#   Per-trial seed: if -seed S is provided, trial i uses seed S+i.
#   Otherwise the C-side RNG state carries across trials (still valid,
#   just non-reproducible).
proc mp_sim::ensemble {timeline args} {
    array set opts {-n_dots 1000 -n_trials 100 -seed {} \
                    -direction_jitter 0.0 -gname {} -record_dots 0}
    array set opts $args

    mp_sim::validate_timeline $timeline
    set T $opts(-n_trials)
    if {$T < 1} { error "mp_sim::ensemble: n_trials must be >= 1" }

    # Per-row column accumulators. Each element is a list (nested) or
    # scalar collected across trials.
    set rows_seed         [list]
    set rows_coh_recorded     [list]
    set rows_dir_mean_coh     [list]
    set rows_dir_circ_var_coh [list]
    set rows_dir_mean_inc     [list]
    set rows_dx_mean          [list]
    set rows_dy_mean          [list]
    set rows_speed_mean       [list]
    set rows_alive_count      [list]
    set rows_dot_x  [list]; set rows_dot_y  [list]
    set rows_dot_th [list]; set rows_dot_cf [list]

    for {set i 0} {$i < $T} {incr i} {
        set seed [expr {$opts(-seed) eq "" ? "" : $opts(-seed) + $i}]
        set data [mp_sim::_run_trial_compute $timeline $opts(-n_dots) \
                      $seed $opts(-direction_jitter) $opts(-record_dots)]
        lappend rows_seed [expr {$seed eq "" ? -1 : $seed}]
        foreach col {coh_recorded dir_mean_coh dir_circ_var_coh dir_mean_inc \
                     dx_mean dy_mean speed_mean alive_count} {
            lappend rows_$col [dict get $data $col]
        }
        if {$opts(-record_dots)} {
            lappend rows_dot_x  [dict get $data dot_x]
            lappend rows_dot_y  [dict get $data dot_y]
            lappend rows_dot_th [dict get $data dot_theta]
            lappend rows_dot_cf [dict get $data dot_coherent]
        }
    }

    set gname $opts(-gname)
    if {$gname eq ""} { set gname [dg_tempname] }
    catch {dg_delete $gname}
    set g [dg_create $gname]

    # Per-trial scalars.
    dl_set $g:trial_id [dl_ilist {*}[lseq 0 [expr {$T - 1}]]]
    dl_set $g:seed     [dl_ilist {*}$rows_seed]
    dl_set $g:n_dots   [dl_repeat [dl_ilist $opts(-n_dots)] $T]

    # Nested per-trial frame timecourses. Build inner flist for each
    # trial's frame list, then wrap T of them into an outer llist.
    foreach col {coh_recorded dir_mean_coh dir_circ_var_coh dir_mean_inc \
                 dx_mean dy_mean speed_mean} {
        set inner [list]
        foreach trial_frames [set rows_$col] {
            lappend inner [dl_flist {*}$trial_frames]
        }
        dl_set $g:$col [dl_llist {*}$inner]
    }
    set inner [list]
    foreach trial_frames $rows_alive_count {
        lappend inner [dl_ilist {*}$trial_frames]
    }
    dl_set $g:alive_count [dl_llist {*}$inner]

    if {$opts(-record_dots)} {
        foreach {col srcvar type} {
            dot_x        rows_dot_x  flist
            dot_y        rows_dot_y  flist
            dot_theta    rows_dot_th flist
            dot_coherent rows_dot_cf ilist
        } {
            set outer [list]
            foreach trial_frames [set $srcvar] {
                set frames [list]
                foreach row $trial_frames {
                    if {$type eq "ilist"} {
                        lappend frames [dl_ilist {*}$row]
                    } else {
                        lappend frames [dl_flist {*}$row]
                    }
                }
                lappend outer [dl_llist {*}$frames]
            }
            dl_set $g:$col [dl_llist {*}$outer]
        }
    }

    # Group-level / shared columns.
    foreach col {t mask_offset_x mask_offset_y direction coherence speed lifetime_s} {
        dl_set $g:${col}_design $timeline:$col
    }
    foreach col {patch_size_dva duration base_coh tile_times n_frames dt} {
        if {[dl_exists $timeline:$col]} {
            dl_set $g:$col $timeline:$col
        }
    }
    dl_set $g:n_trials [dl_ilist $T]
    return $g
}

# ============================================================
# Sweep: parameter-grid runner
# ============================================================

# mp_sim::_spec_set spec path value
#   Set a value in a spec dict using a dotted path.
#   Example: spec_set $spec endpoints.target.speed 12.0
#   Returns the modified spec dict (Tcl dicts are value-typed; the
#   caller assigns the result).
proc mp_sim::_spec_set {spec path value} {
    set keys [split $path .]
    dict set spec {*}$keys $value
    return $spec
}

# mp_sim::_grid_cells vary_dict
#   Cartesian product over a dict of {key -> value-list}. Returns a
#   Tcl list of dicts, one per cell, each with one value per key.
proc mp_sim::_grid_cells {vary_dict} {
    set keys [dict keys $vary_dict]
    if {[llength $keys] == 0} { return [list [dict create]] }
    set k    [lindex $keys 0]
    set rest_dict [dict remove $vary_dict $k]
    set rest_combos [mp_sim::_grid_cells $rest_dict]
    set out [list]
    foreach v [dict get $vary_dict $k] {
        foreach combo $rest_combos {
            dict set combo $k $v
            lappend out $combo
        }
    }
    return $out
}

# mp_sim::_summarize_ensemble ens_dg
#   Extract per-frame mean and std across trials from an ensemble dg.
#   Returns a Tcl dict mapping {col -> {mean_dl_name std_dl_name}} for
#   each summary column (coh_recorded, dir_mean_coh, ...). The dl names
#   are dl_local-scoped to the caller, so the caller must persist
#   them via dl_set to a long-lived dg before the proc returns.
proc mp_sim::_summarize_ensemble {ens_dg} {
    set out [dict create]
    foreach col {coh_recorded dir_mean_coh dir_circ_var_coh dir_mean_inc \
                 dx_mean dy_mean speed_mean} {
        # Each ens_dg:$col is a row-of-flists. dl_transpose makes it
        # frame-of-rows, then dl_means / dl_stds reduce over trials.
        dl_local m [dl_means [dl_transpose $ens_dg:$col]]
        dl_local s [dl_stds  [dl_transpose $ens_dg:$col]]
        dict set out $col [list $m $s]
    }
    return $out
}

# mp_sim::sweep base_spec ?-vary <dict>? ?-n_dots N? ?-n_trials T?
#                         ?-seed S? ?-direction_jitter J? ?-gname name?
#
#   Run an ensemble per cell in a parameter grid. Returns a grid dg
#   with one row per cell.
#
#   The -vary dict has keys that are dotted paths into the spec dict
#   ("envelope.sigma_ms", "endpoints.target.speed", etc.) or special
#   "runtime.X" keys ("runtime.n_dots", "runtime.direction_jitter")
#   that override the runtime args. Values are lists of points to
#   sweep through.
#
#   Output schema:
#     Per-cell scalars (one row each):
#       cell_id        ilist
#       <vary_path_1>  flist or ilist     -- one column per swept path
#       <vary_path_2>  ...
#       n_dots, n_trials                 -- effective values for the cell
#     Per-cell per-frame nested:
#       <stat>_mean, <stat>_std          -- mean/std across trials
#                                            for each of the 7 summary
#                                            stats; each entry is a
#                                            flist of n_frames floats
#     Group-level (single-element):
#       t_design, coherence_design, ... -- design columns from the
#                                            BASE spec; if the sweep
#                                            varies envelope or
#                                            trajectory params, these
#                                            differ per row, but
#                                            base_t etc are kept for
#                                            reference. Per-cell design
#                                            columns are NOT stored
#                                            here -- recompile the spec
#                                            for any cell to retrieve
#                                            them if needed.
proc mp_sim::sweep {base_spec args} {
    array set opts {-vary {} -n_dots 1000 -n_trials 100 -seed {} \
                    -direction_jitter 0.0 -gname {}}
    array set opts $args

    set vary $opts(-vary)
    set cells [mp_sim::_grid_cells $vary]
    set n_cells [llength $cells]
    if {$n_cells < 1} {
        error "mp_sim::sweep: vary dict produced 0 cells"
    }

    # Capture the path order from the vary dict so the output columns
    # appear in a stable order matching the user's input.
    set paths [dict keys $vary]

    # Per-cell accumulators.
    set rows_cell_id [list]
    set rows_n_dots  [list]
    set rows_n_trials [list]
    array set rows_path {}
    foreach p $paths { set rows_path($p) [list] }
    array set rows_mean {}
    array set rows_std  {}
    foreach col {coh_recorded dir_mean_coh dir_circ_var_coh dir_mean_inc \
                 dx_mean dy_mean speed_mean} {
        set rows_mean($col) [list]
        set rows_std($col)  [list]
    }

    # Build the base timeline once for group-level columns.
    set base_tl [mp_sim::compile_spec $base_spec]

    set cell_id 0
    foreach cell $cells {
        # Resolve the cell's spec and runtime overrides.
        set this_spec $base_spec
        set this_n_dots $opts(-n_dots)
        set this_jit    $opts(-direction_jitter)
        foreach p $paths {
            set v [dict get $cell $p]
            if {[string match "runtime.*" $p]} {
                set arg [string range $p [string length "runtime."] end]
                switch -- $arg {
                    n_dots           { set this_n_dots $v }
                    direction_jitter { set this_jit    $v }
                    default { error "mp_sim::sweep: unknown runtime path '$p'" }
                }
            } else {
                set this_spec [mp_sim::_spec_set $this_spec $p $v]
            }
        }

        # Compile + run ensemble for this cell. seed adjusts per cell so
        # every cell has independent (but reproducible) RNG history.
        set this_tl [mp_sim::compile_spec $this_spec]
        set cell_seed [expr {$opts(-seed) eq "" ? "" : $opts(-seed) + 1000 * $cell_id}]
        set ens [mp_sim::ensemble $this_tl -n_dots $this_n_dots \
                     -n_trials $opts(-n_trials) -seed $cell_seed \
                     -direction_jitter $this_jit]

        # Extract per-frame mean/std into dl_local refs, then COPY them
        # into proper named dls so the data survives ensemble cleanup.
        # (Saving the dl_local name directly into a list would dangle.)
        foreach col {coh_recorded dir_mean_coh dir_circ_var_coh dir_mean_inc \
                     dx_mean dy_mean speed_mean} {
            dl_local m [dl_means [dl_transpose $ens:$col]]
            dl_local s [dl_stds  [dl_transpose $ens:$col]]
            # Persist into a temp dg so we can lookup-by-name later.
            # Names must be unique per (cell, col).
            set tmpname  __mp_sim_sweep_tmp_${cell_id}_${col}_m
            set tmpname2 __mp_sim_sweep_tmp_${cell_id}_${col}_s
            dl_set $tmpname  $m
            dl_set $tmpname2 $s
            lappend rows_mean($col) $tmpname
            lappend rows_std($col)  $tmpname2
        }

        lappend rows_cell_id  $cell_id
        lappend rows_n_dots   $this_n_dots
        lappend rows_n_trials $opts(-n_trials)
        foreach p $paths {
            lappend rows_path($p) [dict get $cell $p]
        }

        catch {dg_delete $ens}
        catch {dg_delete $this_tl}
        incr cell_id
    }

    set gname $opts(-gname)
    if {$gname eq ""} { set gname [dg_tempname] }
    catch {dg_delete $gname}
    set g [dg_create $gname]

    dl_set $g:cell_id  [dl_ilist {*}$rows_cell_id]
    dl_set $g:n_dots   [dl_ilist {*}$rows_n_dots]
    dl_set $g:n_trials [dl_ilist {*}$rows_n_trials]
    foreach p $paths {
        # Heuristic: ints stay ints, anything else as flist. For paths
        # like envelope.n_pulses we want an ilist; for sigma_ms a
        # flist. Inspect the first value.
        set vals $rows_path($p)
        set v0 [lindex $vals 0]
        if {[string is integer -strict $v0]} {
            dl_set $g:$p [dl_ilist {*}$vals]
        } else {
            dl_set $g:$p [dl_flist {*}$vals]
        }
    }
    foreach col {coh_recorded dir_mean_coh dir_circ_var_coh dir_mean_inc \
                 dx_mean dy_mean speed_mean} {
        # Wrap each cell's per-frame flist into one llist column.
        set inner_m [list]
        foreach name $rows_mean($col) {
            lappend inner_m $name
        }
        set inner_s [list]
        foreach name $rows_std($col) {
            lappend inner_s $name
        }
        dl_set $g:${col}_mean [dl_llist {*}$inner_m]
        dl_set $g:${col}_std  [dl_llist {*}$inner_s]
        # Free the temp persistent dls now that they're inside g.
        foreach name $rows_mean($col) { catch {dl_delete $name} }
        foreach name $rows_std($col)  { catch {dl_delete $name} }
    }

    # Group-level reference: copy base spec's design columns.
    foreach col {t mask_offset_x mask_offset_y direction coherence speed lifetime_s} {
        dl_set $g:${col}_design $base_tl:$col
    }
    foreach col {patch_size_dva duration base_coh tile_times n_frames dt} {
        if {[dl_exists $base_tl:$col]} {
            dl_set $g:${col}_base $base_tl:$col
        }
    }
    dl_set $g:n_cells [dl_ilist $n_cells]

    catch {dg_delete $base_tl}
    return $g
}
