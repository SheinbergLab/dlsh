# traj --
#   Motion models for pursuit / trajectory-prediction stimuli. ONE definition
#   of "where is the target at time t, how fast is it going, and where are its
#   landmarks" -- shared by the three Tcl consumers that until now each carried
#   their own copy of the ballistic formula:
#
#       traj  (this package: pos / vel / landmarks, per motion_type)
#         <- launch_sim   (scene geometry, goals, occluders; ballistic case)
#         <- ess loaders  (stimdg construction)
#         <- stim2 replay (per-frame target position)
#
#   Pure Tcl math -- no dlsh, stim2, dserv or box2d -- so it loads anywhere.
#
# THE CONTRACT
#   A trajectory is ONE self-describing dict. `motion_type` selects the model;
#   every other key is that model's parameters. motion_type is OPTIONAL and
#   defaults to `ballistic`, so every dict that launch_sim already emits and
#   every stimdg row that already exists keeps working untouched.
#
#       traj::pos       {params t}  -> {x y}       position at time t
#       traj::vel       {params t}  -> {vx vy}     velocity at time t
#       traj::landmarks {params}    -> dict name -> LIST of times (s)
#       traj::duration  {params}    -> total duration (= the `end` landmark)
#       traj::extent    {params}    -> max |x|,|y| reached (fit / field sizing)
#       traj::sample    {params dt} -> dict {t {..} x {..} y {..}}
#
#   pos/vel are the PURE MODEL: they are NOT clamped to the flight interval.
#   Clamping and sample-interpolation are policy and stay in launch_sim.
#
# LANDMARKS
#   name -> list of times, uniformly, so consumers can anchor to "landmark k"
#   instead of hardcoding a flight fraction. Every model reports `start` and
#   `end`; ballistic adds `apex` (vertical-velocity zero-crossing). An
#   oscillatory model (pendulum) will report `turn` with one entry per turning
#   point -- which is what a coherence dip should anchor to.
#
# ADDING A MOTION
#   Write pos/vel/landmarks (+ optional extent) procs, then:
#       traj::register <type> -pos <proc> -vel <proc> -landmarks <proc>
#   Nothing else in the stack needs to learn about it. Keep the contract thin:
#   add fields only when a real motion needs them.

package provide traj 1.0

namespace eval traj {
    variable models {}                ;# type -> {pos .. vel .. landmarks .. extent ..}
    variable default_type ballistic   ;# motion_type absent => ballistic (back-compat)
}

# ------------------------------------------------------------------
# Registry
# ------------------------------------------------------------------
proc traj::register { type args } {
    variable models
    set spec [dict create pos {} vel {} landmarks {} extent {}]
    foreach { k v } $args {
        switch -- $k {
            -pos       { dict set spec pos $v }
            -vel       { dict set spec vel $v }
            -landmarks { dict set spec landmarks $v }
            -extent    { dict set spec extent $v }
            default    { error "traj::register: unknown option '$k'" }
        }
    }
    foreach k { pos vel landmarks } {
        if { [dict get $spec $k] eq "" } {
            error "traj::register: model '$type' is missing -$k"
        }
    }
    dict set models $type $spec
    return $type
}

proc traj::types {}       { variable models ; return [lsort [dict keys $models]] }
proc traj::has { type }   { variable models ; return [dict exists $models $type] }

proc traj::type_of { params } {
    variable default_type
    if { [dict exists $params motion_type] } { return [dict get $params motion_type] }
    return $default_type
}

proc traj::_call { params which args } {
    variable models
    set type [traj::type_of $params]
    if { ![dict exists $models $type] } {
        error "traj: unknown motion_type '$type' (registered: [traj::types])"
    }
    set p [dict get $models $type $which]
    if { $p eq "" } { error "traj: model '$type' provides no $which" }
    return [$p $params {*}$args]
}

# ------------------------------------------------------------------
# Dispatch
# ------------------------------------------------------------------
proc traj::pos       { params t } { return [traj::_call $params pos $t] }
proc traj::vel       { params t } { return [traj::_call $params vel $t] }
proc traj::landmarks { params }   { return [traj::_call $params landmarks] }

# |v(t)| -- the single number most per-frame drivers actually want (internal
# dot speed, coherence-tween endpoints, ...). A thin convenience over vel, not
# a new model hook.
proc traj::speed { params t } {
    lassign [traj::vel $params $t] vx vy
    return [expr {hypot($vx,$vy)}]
}

proc traj::duration { params } {
    set m [traj::landmarks $params]
    if { ![dict exists $m end] } { error "traj: model reports no 'end' landmark" }
    return [lindex [dict get $m end] 0]
}

# max |x| / |y| reached over the trajectory (drives fit checks and patch-field
# sizing). A model may supply an exact -extent; otherwise fall back to samples.
proc traj::extent { params { dt 0.005 } } {
    variable models
    set type [traj::type_of $params]
    set p [dict get $models $type extent]
    if { $p ne "" } { return [$p $params] }
    set s [traj::sample $params $dt]
    set mx 0.0
    foreach x [dict get $s x] y [dict get $s y] {
        set m [expr {max(abs($x), abs($y))}]
        if { $m > $mx } { set mx $m }
    }
    return $mx
}

# Sampled path -- the universal escape hatch for models with no elementary
# closed form (e.g. a large-angle pendulum), and what a dumb replayer consumes.
proc traj::sample { params dt { t0 0.0 } { t1 {} } } {
    if { $t1 eq "" } { set t1 [traj::duration $params] }
    set ts {} ; set xs {} ; set ys {}
    for { set t $t0 } { $t < $t1 } { set t [expr {$t + $dt}] } {
        lassign [traj::pos $params $t] x y
        lappend ts $t ; lappend xs $x ; lappend ys $y
    }
    lassign [traj::pos $params $t1] x y      ;# always land the endpoint exactly
    lappend ts $t1 ; lappend xs $x ; lappend ys $y
    return [dict create t $ts x $xs y $ys]
}

# ==================================================================
#  MODEL: ballistic -- constant acceleration (the free-projectile case)
#     x(t) = launcher_x + vx*t
#     y(t) = launcher_y + vy*t - 0.5*gravity*t^2
#  gravity > 0 = "hill" (concave down), < 0 = "valley" (the vertical mirror:
#  gravity-inconsistent), == 0 = straight line. Expressions are transcribed
#  verbatim from the three implementations they replace (launch_sim
#  ball_pos/vel_at_time, ess::pursuit::ballistic::symmetric_arc, and the
#  per-frame formula in ballistic_stim.tcl) so the port is bit-faithful.
# ==================================================================
namespace eval traj::ballistic {}

proc traj::ballistic::pos { p t } {
    set x [expr {[dict get $p launcher_x] + [dict get $p vx]*$t}]
    set y [expr {[dict get $p launcher_y] + [dict get $p vy]*$t \
                 - 0.5*[dict get $p gravity]*$t*$t}]
    return [list $x $y]
}

proc traj::ballistic::vel { p t } {
    return [list [dict get $p vx] \
                 [expr {[dict get $p vy] - [dict get $p gravity]*$t}]]
}

# start / end always; apex = the vertical-velocity zero-crossing (vy/g), when
# it falls strictly inside the flight. A coherence dip anchored "post-apex"
# should reference THIS, not a hardcoded flight fraction.
proc traj::ballistic::landmarks { p } {
    set T  [dict get $p land_time]
    set g  [dict get $p gravity]
    set vy [dict get $p vy]
    set m [dict create start [list 0.0] end [list $T]]
    if { $g != 0.0 } {
        set ta [expr {double($vy)/double($g)}]
        if { $ta > 0.0 && $ta < $T } { dict set m apex [list $ta] }
    }
    return $m
}

# Exact extent: x is monotonic so its max |.| is at an endpoint; y is a
# parabola so its extremum is the apex (when in range). For the symmetric arc
# this reduces to max(ecc, |launch_y|, |apex|) -- symmetric_arc's maxext.
proc traj::ballistic::extent { p } {
    set T [dict get $p land_time]
    lassign [traj::ballistic::pos $p 0.0] x0 y0
    lassign [traj::ballistic::pos $p $T]  x1 y1
    set mx [expr {max(abs($x0), abs($x1), abs($y0), abs($y1))}]
    set m [traj::ballistic::landmarks $p]
    if { [dict exists $m apex] } {
        lassign [traj::ballistic::pos $p [lindex [dict get $m apex] 0]] xa ya
        set mx [expr {max($mx, abs($ya))}]
    }
    return $mx
}

# Constructor: the SYMMETRIC fixed-endpoint arc used by pursuit/ballistic --
# launch from -/+ecc, fly for T, return to launch height (vy = g*T/2, peak =
# g*T^2/8). Transcribed from ess::pursuit::ballistic::symmetric_arc; emits the
# same keys plus motion_type, so it is a drop-in for that proc.
proc traj::ballistic::symmetric { ecc T g side { launch_y 0.0 } } {
    set peak [expr {$g * $T * $T / 8.0}]
    set apex [expr {$launch_y + $peak}]
    if { $side == 1 } {
        set lx [expr { $ecc}] ; set vx [expr {-2.0*$ecc/$T}]
    } else {
        set lx [expr {-$ecc}] ; set vx [expr { 2.0*$ecc/$T}]
    }
    return [dict create \
        motion_type ballistic \
        launcher_x $lx  launcher_y $launch_y \
        vx $vx  vy [expr {0.5*$g*$T}]  gravity $g \
        land_time $T  land_x [expr {-$lx}]  floor_y $launch_y  side $side \
        maxext [expr {max($ecc, abs($launch_y), abs($apex))}]]
}

traj::register ballistic \
    -pos       traj::ballistic::pos \
    -vel       traj::ballistic::vel \
    -landmarks traj::ballistic::landmarks \
    -extent    traj::ballistic::extent

# ==================================================================
#  Built-in motion plugins (each self-registers). Sourced last so the core
#  registry/dispatch above is fully defined first.
# ==================================================================
source [file join [file dirname [info script]] pendulum.tcl]
source [file join [file dirname [info script]] inverted_pendulum.tcl]
