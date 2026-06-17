# launch_sim --
#   Headless generator for ballistic "launcher" trajectories: a projectile
#   fired from a launcher under gravity, optionally passing behind an
#   occluder, landing in one of two goals. Pure dlsh/Tcl -- NO stim2, dserv,
#   or box2d -- so trials can be generated, verified, and drawn in any
#   environment with the dlsh libs (plain tclsh9, dlshell, or the ess_control
#   virtual display). The SAME code produces trials on the rig and ground
#   truth off-rig.
#
#   Scope: the GENERATION layer only -- trajectory math + scene-layout
#   geometry + a lightweight dlg_ trial overview. The stim side consumes the
#   resulting ball_t/x/y + side contract and owns all GL rendering. (When a
#   variant needs real bounce physics through a blocker field, that one
#   generation backend delegates to box2d on the dserv/stim side; this
#   package stays box2d-free and covers the closed-form ballistic case.)
#
#   Contract: sample_trajectory returns a dict (one trial):
#     side           0 = left goal, 1 = right goal
#     launcher_x/y   launch point (dva)
#     angle_rad      launch elevation (radians)
#     speed vx vy gravity
#     land_time      s until landing
#     land_x         landing x (dva) at floor_y
#     ball_t/x/y     sampled trajectory as Tcl lists (fixed dt + final land pt)
#   plus the board geometry it was generated against (floor_y, catchers, ...).
#
#   Occlusion is a SEPARATE, decoupled overlay (it never affects trajectories):
#   run a trajectory through `occlude {tr regions}` to add occlusion_intervals
#   / occlusion_enter_time / occlusion_exit_time / occlusion_duration.
#
#   Conventions: angles in radians; distances in dva; time in seconds.
#   See test_launch_sim.tcl for usage.

package provide launch_sim 0.1
package require dlsh

namespace eval launch_sim {
    variable pi 3.14159265358979323846
}

# ------------------------------------------------------------------
# Parameters
# ------------------------------------------------------------------
# Default board + sampling parameters. Override any subset by passing a
# dict to sample_trajectory / build_trials; missing keys fall back here.
# (Occluders are a separate overlay -- see occlude/occlusion_intervals.)
proc launch_sim::default_params {} {
    return [dict create \
        boundary       floor \
        launcher_x_min -15.0 launcher_x_max  -2.0 \
        launcher_y_min  -7.0 launcher_y_max  -1.0 \
        floor_y         -8.5 \
        lcatcher_x      11.5 rcatcher_x      14.5 goal_halfwidth 1.0 \
        ball_area      0.126 \
        speed_min        5.0 speed_max       30.0 \
        angle_min       10.0 angle_max       80.0 \
        gravity_min      0.0 gravity_max    19.62 \
        min_visible      0.3 max_sim_time     8.0 \
        dt        0.01666667 vx_cap          30.0 \
        max_attempts    5000 \
        circle_cx        0.0 circle_cy        0.0 circle_r        9.0 \
        launch_margin    1.0 \
        arc_radius       8.0 arc_span_deg   140.0 \
        arc_cx           0.0 arc_cy           0.0 launcher_jitter   2.0]
}

# ------------------------------------------------------------------
# Shape geometry (equal-area projectile shapes). Returns
# {shape sx sy angle_rad}; angle in radians. Kept here so the stim and
# any analysis agree on shape extents.
# ------------------------------------------------------------------
proc launch_sim::shape_geometry { shape ball_area } {
    variable pi
    switch -- $shape {
        Circle {
            set r [expr {sqrt($ball_area/$pi)}]
            return [list Circle $r $r 0.0]
        }
        RectThin {
            set sy [expr {sqrt($ball_area*5.6)}]
            set sx [expr {$ball_area/$sy}]
            return [list Box $sx $sy 0.0]
        }
        DiamondSq {
            set side [expr {sqrt($ball_area)}]
            return [list Box $side $side [expr {22.5*$pi/180.0}]]
        }
        default { error "launch_sim::shape_geometry: unknown shape $shape" }
    }
}

# ------------------------------------------------------------------
# sample_trajectory -- rejection-sample ONE valid ballistic trajectory.
#
#   params : a dict overriding default_params (or {} for all defaults)
#   side   : 0=left, 1=right, or -1 to choose randomly
#
# Constraints (same as the original launch loaders, unified):
#   (a) airborne >= min_visible and <= max_sim_time
#   (b) horizontal-speed cap |vx| <= vx_cap
#   (c) if an occluder is present, >= min_visible of pre-occlusion flight
#   (d) lands in the inner half of the chosen goal (the half nearest the
#       other goal), so trials split evenly left/right by construction.
# ------------------------------------------------------------------
proc launch_sim::sample_trajectory { {params {}} {side -1} } {
    variable pi
    set p [dict merge [default_params] $params]
    if { $side < 0 } { set side [expr {int(rand()*2)}] }
    switch -- [dict get $p boundary] {
        circle { return [sample_trajectory_circle $p $side] }
        arc    { return [sample_trajectory_arc    $p $side] }
    }
    dict with p {}   ;# (floor mode) import keys as locals (launcher_x_min, ...)

    if { $side == 0 } {
        set target_lo [expr {$lcatcher_x + $goal_halfwidth/2.0}]
        set target_hi [expr {$lcatcher_x + $goal_halfwidth}]
    } else {
        set target_lo [expr {$rcatcher_x - $goal_halfwidth}]
        set target_hi [expr {$rcatcher_x - $goal_halfwidth/2.0}]
    }

    for { set attempt 0 } { $attempt < $max_attempts } { incr attempt } {
        set angle_deg [expr {$angle_min + rand()*($angle_max-$angle_min)}]
        set rad [expr {$angle_deg*$pi/180.0}]

        set cap  [expr {$vx_cap/cos($rad)}]
        set smax [expr {min($speed_max,$cap)}]
        if { $smax < $speed_min } continue
        set speed [expr {$speed_min + rand()*($smax-$speed_min)}]

        set vx [expr {$speed*cos($rad)}]
        set vy [expr {$speed*sin($rad)}]
        set gravity [expr {$gravity_min + rand()*($gravity_max-$gravity_min)}]

        set launcher_x [expr {$launcher_x_min + rand()*($launcher_x_max-$launcher_x_min)}]
        set launcher_y [expr {$launcher_y_min + rand()*($launcher_y_max-$launcher_y_min)}]

        set dy [expr {$launcher_y - $floor_y}]
        if { $gravity > 0.0 } {
            # ballistic: lands at the positive root of the parabola
            set disc [expr {$vy*$vy + 2.0*$gravity*$dy}]
            if { $disc < 0.0 } continue
            set land_time [expr {($vy + sqrt($disc))/$gravity}]
        } elseif { $gravity == 0.0 } {
            # linear (constant-velocity) motion: a straight line reaches the
            # floor only if aimed DOWNWARD (vy < 0). land_time = dy/|vy|.
            if { $vy >= 0.0 } continue
            set land_time [expr {-$dy/$vy}]
        } else {
            continue   ;# negative gravity unsupported
        }
        if { $land_time < $min_visible || $land_time > $max_sim_time } continue

        set land_x [expr {$launcher_x + $vx*$land_time}]
        if { $land_x < $target_lo || $land_x > $target_hi } continue

        # success -- sample the parabola at fixed dt, then the exact land point
        set tlist {}; set xlist {}; set ylist {}
        for { set t 0.0 } { $t < $land_time } { set t [expr {$t+$dt}] } {
            lappend tlist $t
            lappend xlist [expr {$launcher_x + $vx*$t}]
            lappend ylist [expr {$launcher_y + $vy*$t - 0.5*$gravity*$t*$t}]
        }
        lappend tlist $land_time; lappend xlist $land_x; lappend ylist $floor_y

        return [dict create \
            boundary floor side $side launcher_x $launcher_x launcher_y $launcher_y \
            angle_rad $rad speed $speed vx $vx vy $vy gravity $gravity \
            land_time $land_time land_x $land_x \
            floor_y $floor_y lcatcher_x $lcatcher_x rcatcher_x $rcatcher_x \
            goal_halfwidth $goal_halfwidth \
            ball_t $tlist ball_x $xlist ball_y $ylist]
    }
    error "launch_sim::sample_trajectory: no valid trajectory in $max_attempts attempts"
}

# True if angle a (rad) lies in the CCW arc from a0 to a1 (rad), with wrap.
proc launch_sim::angle_in_arc { a a0 a1 } {
    variable pi
    set two [expr {2.0*$pi}]
    set d [expr {fmod(($a-$a0)+10.0*$two, $two)}]
    set w [expr {fmod(($a1-$a0)+10.0*$two, $two)}]
    return [expr {$d <= $w}]
}

# ------------------------------------------------------------------
# sample_trajectory_circle -- circle-boundary variant. The launcher is an
# INTERIOR point; the ball flies until it first re-crosses the circle, and the
# report is the EXIT ANGLE on the rim (a 1D answer space: swipe/compass, exact
# for both linear and ballistic). `angle_min/angle_max` here are the launch
# HEADING range in degrees (math convention), not elevation; a circle variant
# typically sets them 0..360. (Occlusion is a separate overlay -- see occlude.)
# ------------------------------------------------------------------
proc launch_sim::sample_trajectory_circle { p side } {
    variable pi
    dict with p {}

    for { set attempt 0 } { $attempt < $max_attempts } { incr attempt } {
        set rad   [expr {($angle_min + rand()*($angle_max-$angle_min))*$pi/180.0}]
        set speed [expr {$speed_min + rand()*($speed_max-$speed_min)}]
        set vx [expr {$speed*cos($rad)}]
        set vy [expr {$speed*sin($rad)}]
        set gravity [expr {$gravity_min + rand()*($gravity_max-$gravity_min)}]

        # interior launcher: uniform over the disk of radius (r - margin)
        set lr  [expr {($circle_r-$launch_margin)*sqrt(rand())}]
        set lth [expr {rand()*2.0*$pi}]
        set launcher_x [expr {$circle_cx + $lr*cos($lth)}]
        set launcher_y [expr {$circle_cy + $lr*sin($lth)}]

        # step until the ball first reaches the rim, then bisect for the exact
        # crossing -- works for line and parabola alike, no quartic algebra
        set tlist {}; set xlist {}; set ylist {}
        set exit_t {}
        for { set t 0.0 } { $t <= $max_sim_time } { set t [expr {$t+$dt}] } {
            set x [expr {$launcher_x + $vx*$t}]
            set y [expr {$launcher_y + $vy*$t - 0.5*$gravity*$t*$t}]
            if { $t > 0.0 && hypot($x-$circle_cx,$y-$circle_cy) >= $circle_r } {
                set ta [expr {$t-$dt}]; set tb $t
                for { set b 0 } { $b < 40 } { incr b } {
                    set tm [expr {0.5*($ta+$tb)}]
                    set xm [expr {$launcher_x+$vx*$tm}]
                    set ym [expr {$launcher_y+$vy*$tm-0.5*$gravity*$tm*$tm}]
                    if { hypot($xm-$circle_cx,$ym-$circle_cy) >= $circle_r } { set tb $tm } else { set ta $tm }
                }
                set exit_t $tb
                break
            }
            lappend tlist $t; lappend xlist $x; lappend ylist $y
        }
        if { $exit_t eq {} || $exit_t < $min_visible } continue

        set land_x [expr {$launcher_x + $vx*$exit_t}]
        set land_y [expr {$launcher_y + $vy*$exit_t - 0.5*$gravity*$exit_t*$exit_t}]
        # left/right balance + a discrete `side` for the 2AFC scaffolding
        if { $side == 0 && $land_x > $circle_cx } continue
        if { $side == 1 && $land_x < $circle_cx } continue

        lappend tlist $exit_t; lappend xlist $land_x; lappend ylist $land_y
        set exit_angle [expr {atan2($land_y-$circle_cy, $land_x-$circle_cx)}]

        return [dict create \
            boundary circle side $side \
            launcher_x $launcher_x launcher_y $launcher_y \
            angle_rad $rad speed $speed vx $vx vy $vy gravity $gravity \
            land_time $exit_t land_x $land_x land_y $land_y exit_angle $exit_angle \
            circle_cx $circle_cx circle_cy $circle_cy circle_r $circle_r \
            ball_t $tlist ball_x $xlist ball_y $ylist]
    }
    error "launch_sim::sample_trajectory_circle: no valid trajectory in $max_attempts attempts"
}

# ==================================================================
# Occluder (decoupled visibility overlay -- does NOT affect trajectories)
# ==================================================================
# An occluder is a list of region dicts (a union); a point is occluded if it
# falls in ANY region. Region primitives:
#   {type rect   x0 y0 x1 y1}
#   {type circle cx cy r}
#   {type arc    cx cy r0 r1 a0 a1}   ;# annular sector, a0..a1 radians (CCW)
# Arbitrary occluder shapes compose as unions of these. Because the occluder
# never touches the physics, occlusion is just a scan of the trajectory.

proc launch_sim::point_occluded { x y regions } {
    foreach reg $regions {
        switch -- [dict get $reg type] {
            rect {
                if { $x >= [dict get $reg x0] && $x <= [dict get $reg x1] &&
                     $y >= [dict get $reg y0] && $y <= [dict get $reg y1] } { return 1 }
            }
            circle {
                if { hypot($x-[dict get $reg cx],$y-[dict get $reg cy]) <= [dict get $reg r] } { return 1 }
            }
            arc {
                set rr [expr {hypot($x-[dict get $reg cx],$y-[dict get $reg cy])}]
                if { $rr >= [dict get $reg r0] && $rr <= [dict get $reg r1] &&
                     [angle_in_arc [expr {atan2($y-[dict get $reg cy],$x-[dict get $reg cx])}] \
                                   [dict get $reg a0] [dict get $reg a1]] } { return 1 }
            }
        }
    }
    return 0
}

# Scan a sampled trajectory against an occluder; return a list of {enter exit}
# time intervals during which the ball is hidden.
proc launch_sim::occlusion_intervals { bt bx by regions } {
    set intervals {}; set in 0; set enter {}
    foreach t $bt x $bx y $by {
        set occ [point_occluded $x $y $regions]
        if { $occ && !$in } { set in 1; set enter $t }
        if { !$occ && $in } { set in 0; lappend intervals [list $enter $t] }
    }
    if { $in } { lappend intervals [list $enter [lindex $bt end]] }
    return $intervals
}

# Augment a trajectory dict with occlusion fields for the given occluder:
# occlusion_intervals, first enter / last exit times, and total hidden
# duration (the SPEM IV). The occluder is carried along for rendering.
proc launch_sim::occlude { tr regions } {
    set ints [occlusion_intervals [dict get $tr ball_t] [dict get $tr ball_x] \
                  [dict get $tr ball_y] $regions]
    set dur 0.0
    foreach iv $ints { set dur [expr {$dur + [lindex $iv 1]-[lindex $iv 0]}] }
    set enter {}; set exit {}
    if { [llength $ints] } {
        set enter [lindex [lindex $ints 0] 0]
        set exit  [lindex [lindex $ints end] 1]
    }
    dict set tr occlusion_intervals  $ints
    dict set tr occlusion_enter_time $enter
    dict set tr occlusion_exit_time  $exit
    dict set tr occlusion_duration   $dur
    dict set tr occluder_regions     $regions
    return $tr
}

# ------------------------------------------------------------------
# sample_trajectory_arc -- launcher-aligned landing arc. The circle is centered
# on the LAUNCHER and only a downrange arc (arc_span_deg wide, centered on the
# launch heading) is valid/visible. The report is the SIGNED deviation from the
# heading (0 = straight ahead), so for linear motion it's ~0 and for ballistic
# it's offset by the gravity-induced curve -- the deviation IS the curvature-
# extrapolation DV. Also returns the catcher pose (tangent rectangle) at the
# true exit. `angle_min/angle_max` are the launch HEADING range (deg).
# ------------------------------------------------------------------
proc launch_sim::sample_trajectory_arc { p side } {
    variable pi
    dict with p {}
    set R $arc_radius
    set halfspan [expr {($arc_span_deg/2.0)*$pi/180.0}]

    for { set attempt 0 } { $attempt < $max_attempts } { incr attempt } {
        set heading [expr {($angle_min + rand()*($angle_max-$angle_min))*$pi/180.0}]
        set speed   [expr {$speed_min + rand()*($speed_max-$speed_min)}]
        set vx [expr {$speed*cos($heading)}]
        set vy [expr {$speed*sin($heading)}]
        set gravity [expr {$gravity_min + rand()*($gravity_max-$gravity_min)}]

        # launcher = arc center (+ optional jitter); the circle is centered here
        set lr  [expr {$launcher_jitter*sqrt(rand())}]
        set lth [expr {rand()*2.0*$pi}]
        set launcher_x [expr {$arc_cx + $lr*cos($lth)}]
        set launcher_y [expr {$arc_cy + $lr*sin($lth)}]

        # fly until first crossing of radius R from the launcher; bisect to refine
        set tlist {}; set xlist {}; set ylist {}; set exit_t {}
        for { set t 0.0 } { $t <= $max_sim_time } { set t [expr {$t+$dt}] } {
            set x [expr {$launcher_x + $vx*$t}]
            set y [expr {$launcher_y + $vy*$t - 0.5*$gravity*$t*$t}]
            if { $t > 0.0 && hypot($x-$launcher_x,$y-$launcher_y) >= $R } {
                set ta [expr {$t-$dt}]; set tb $t
                for { set b 0 } { $b < 40 } { incr b } {
                    set tm [expr {0.5*($ta+$tb)}]
                    set xm [expr {$launcher_x+$vx*$tm}]
                    set ym [expr {$launcher_y+$vy*$tm-0.5*$gravity*$tm*$tm}]
                    if { hypot($xm-$launcher_x,$ym-$launcher_y) >= $R } { set tb $tm } else { set ta $tm }
                }
                set exit_t $tb
                break
            }
            lappend tlist $t; lappend xlist $x; lappend ylist $y
        }
        if { $exit_t eq {} || $exit_t < $min_visible } continue

        set land_x [expr {$launcher_x + $vx*$exit_t}]
        set land_y [expr {$launcher_y + $vy*$exit_t - 0.5*$gravity*$exit_t*$exit_t}]
        set exit_angle [expr {atan2($land_y-$launcher_y, $land_x-$launcher_x)}]
        # signed deviation from heading, wrapped to (-pi,pi]
        set dev [expr {atan2(sin($exit_angle-$heading), cos($exit_angle-$heading))}]
        if { abs($dev) > $halfspan } continue   ;# outside the valid/visible arc
        # discrete side = which side of the heading it landed (for 2AFC compat)
        set s [expr {$dev < 0 ? 0 : 1}]
        if { $side == 0 && $dev >= 0 } continue
        if { $side == 1 && $dev <  0 } continue

        lappend tlist $exit_t; lappend xlist $land_x; lappend ylist $land_y
        set catcher_angle [expr {$exit_angle + $pi/2.0}]   ;# tangent to the arc

        return [dict create \
            boundary arc side $s \
            launcher_x $launcher_x launcher_y $launcher_y \
            angle_rad $heading heading $heading speed $speed vx $vx vy $vy gravity $gravity \
            land_time $exit_t land_x $land_x land_y $land_y \
            exit_angle $exit_angle deviation $dev \
            arc_cx $launcher_x arc_cy $launcher_y arc_radius $R arc_span_deg $arc_span_deg \
            catcher_x $land_x catcher_y $land_y catcher_angle $catcher_angle \
            ball_t $tlist ball_x $xlist ball_y $ylist]
    }
    error "launch_sim::sample_trajectory_arc: no valid trajectory in $max_attempts attempts"
}

# Catcher pose (x y tangent_angle) at a given signed deviation (radians) from
# the heading -- lets the stim slide/"spin" the catcher along the arc.
proc launch_sim::catcher_pose { tr dev } {
    variable pi
    set a [expr {[dict get $tr heading] + $dev}]
    set x [expr {[dict get $tr arc_cx] + [dict get $tr arc_radius]*cos($a)}]
    set y [expr {[dict get $tr arc_cy] + [dict get $tr arc_radius]*sin($a)}]
    return [list $x $y [expr {$a + $pi/2.0}]]
}

# ------------------------------------------------------------------
# ball_pos_at_time -- ball {x y} at an ARBITRARY time t, decoupled from the
# stored sample grid, so playback stays smooth at any display refresh (no
# snap-to-sample stair-stepping). Intended as the SINGLE position source for
# both the stim replay and the moving pursuit window.
#
#   tr   : a trajectory dict from sample_trajectory
#   t    : seconds since launch
#   mode : analytic -- exact closed-form parabola (needs vx/vy/gravity;
#                      ballistic paths only)
#          interp   -- linear interpolation of stored ball_t/x/y; works for
#                      ANY path, including future non-closed-form blocker paths
#          auto     -- analytic if generative params are present, else interp
#
# Clamps to the launch point at t<=0 and the landing point at t>=land_time.
# (The occluder is irrelevant here -- position is defined by physics; whether
# the ball is *drawn* is a separate visibility concern owned by the replay.)
# ------------------------------------------------------------------
proc launch_sim::ball_pos_at_time { tr t {mode auto} } {
    set lt [dict get $tr land_time]
    if { $t <= 0.0 } {
        return [list [dict get $tr launcher_x] [dict get $tr launcher_y]]
    }
    if { $t >= $lt } {
        set ey [expr {[dict exists $tr land_y] ? [dict get $tr land_y] : [dict get $tr floor_y]}]
        return [list [dict get $tr land_x] $ey]
    }
    if { $mode eq "auto" } {
        set mode [expr {[dict exists $tr vx] ? "analytic" : "interp"}]
    }
    if { $mode eq "analytic" } {
        set x [expr {[dict get $tr launcher_x] + [dict get $tr vx]*$t}]
        set y [expr {[dict get $tr launcher_y] + [dict get $tr vy]*$t \
                     - 0.5*[dict get $tr gravity]*$t*$t}]
        return [list $x $y]
    }
    # interp: linear between the samples bracketing t (lists are short, so a
    # forward scan is fine; switch to bisection if trajectories get long)
    set bt [dict get $tr ball_t]
    set bx [dict get $tr ball_x]
    set by [dict get $tr ball_y]
    set n [llength $bt]
    set i 0
    while { $i < $n-2 && [lindex $bt [expr {$i+1}]] <= $t } { incr i }
    set t0 [lindex $bt $i]; set t1 [lindex $bt [expr {$i+1}]]
    set d  [expr {$t1-$t0}]
    if { $d <= 0.0 } { return [list [lindex $bx $i] [lindex $by $i]] }
    set f  [expr {($t-$t0)/$d}]
    return [list [expr {[lindex $bx $i]+$f*([lindex $bx [expr {$i+1}]]-[lindex $bx $i])}] \
                 [expr {[lindex $by $i]+$f*([lindex $by [expr {$i+1}]]-[lindex $by $i])}]]
}

# ------------------------------------------------------------------
# ball_vel_at_time -- ball velocity {vx vy} at time t (companion to
# ball_pos_at_time). For the SPEM measure (eye velocity vs target velocity
# through occlusion) and pursuit-window lead. analytic = exact (vx, vy-g*t);
# interp = central finite difference of the stored samples. t clamped to the
# flight [0, land_time].
# ------------------------------------------------------------------
proc launch_sim::ball_vel_at_time { tr t {mode auto} } {
    set lt [dict get $tr land_time]
    if { $t < 0.0 } { set t 0.0 }
    if { $t > $lt } { set t $lt }
    if { $mode eq "auto" } {
        set mode [expr {[dict exists $tr vx] ? "analytic" : "interp"}]
    }
    if { $mode eq "analytic" } {
        return [list [dict get $tr vx] [expr {[dict get $tr vy] - [dict get $tr gravity]*$t}]]
    }
    set h  [expr {$lt*0.01 + 1e-4}]
    set ta [expr {$t-$h < 0.0 ? 0.0 : $t-$h}]
    set tb [expr {$t+$h > $lt ? $lt : $t+$h}]
    lassign [ball_pos_at_time $tr $ta interp] x0 y0
    lassign [ball_pos_at_time $tr $tb interp] x1 y1
    set d [expr {$tb-$ta}]
    if { $d <= 0.0 } { return [list 0.0 0.0] }
    return [list [expr {($x1-$x0)/$d}] [expr {($y1-$y0)/$d}]]
}

# ------------------------------------------------------------------
# build_trials -- assemble `nr` trajectories into a dynamic group, evenly
# split left/right. Nested ball_t/x/y columns (one list per trial) follow
# the same convention the stim replay + viz consume. Returns the dg name.
# ------------------------------------------------------------------
proc launch_sim::build_trials { nr {params {}} {name launch_trials} } {
    set p [dict merge [default_params] $params]
    if { [dg_exists $name] } { dg_delete $name }

    # optional DECOUPLED occluder (a list of regions) + selection. When
    # `occluder` is supplied each trial is occluded post-hoc and resampled to
    # satisfy: occl_dur_min/max (hidden-duration window -- the SPEM IV) and/or
    # require_exit_occluded (the exit point is hidden, so the report is genuinely
    # predictive -- "occluded to the edge"; the ball does NOT reappear).
    set occluder {}; set dmin {}; set dmax {}; set exit_occ 0
    if { [dict exists $p occluder]     } { set occluder [dict get $p occluder] }
    if { [dict exists $p occl_dur_min] } { set dmin     [dict get $p occl_dur_min] }
    if { [dict exists $p occl_dur_max] } { set dmax     [dict get $p occl_dur_max] }
    if { [dict exists $p require_exit_occluded] } { set exit_occ [dict get $p require_exit_occluded] }

    # scalar columns; mode-specific ones (floor's land_x/lcatcher_*, circle's
    # exit_angle/circle_*, arc's deviation/catcher_*) are stored only when
    # present, so one dg holds whichever boundary + occluder was used.
    set common {launcher_x launcher_y vx vy angle_rad speed gravity land_time}
    set modecols {land_x land_y exit_angle deviation heading floor_y \
                  lcatcher_x rcatcher_x goal_halfwidth \
                  circle_cx circle_cy circle_r \
                  arc_cx arc_cy arc_radius arc_span_deg catcher_x catcher_y catcher_angle \
                  occlusion_enter_time occlusion_exit_time occlusion_duration}

    for { set i 0 } { $i < $nr } { incr i } {
        set s [expr {$i % 2}]
        set tr [sample_trajectory $p $s]
        if { [llength $occluder] } {
            set tr [occlude $tr $occluder]
            if { $dmin ne {} || $dmax ne {} || $exit_occ } {
                for { set k 0 } { $k < 200 } { incr k } {
                    set d [dict get $tr occlusion_duration]
                    set ok 1
                    if { $dmin ne {} && $d < $dmin } { set ok 0 }
                    if { $dmax ne {} && $d > $dmax } { set ok 0 }
                    set ey [expr {[dict exists $tr land_y] ? [dict get $tr land_y] : [dict get $tr floor_y]}]
                    if { $exit_occ && ![point_occluded [dict get $tr land_x] $ey $occluder] } { set ok 0 }
                    if { $ok } break
                    set tr [occlude [sample_trajectory $p $s] $occluder]
                }
            }
        }
        set tg [dg_create]
        dl_set $tg:side     [dl_ilist [dict get $tr side]]
        dl_set $tg:boundary [dl_slist [dict get $tr boundary]]
        foreach c $common { dl_set $tg:$c [dl_flist [dict get $tr $c]] }
        foreach c $modecols {
            if { [dict exists $tr $c] } {
                set v [dict get $tr $c]
                if { $v eq {} } { set v -1 }
                dl_set $tg:$c [dl_flist $v]
            }
        }

        dl_set $tg:ball_t [dl_flist {*}[dict get $tr ball_t]]
        dl_set $tg:ball_x [dl_flist {*}[dict get $tr ball_x]]
        dl_set $tg:ball_y [dl_flist {*}[dict get $tr ball_y]]
        foreach c {ball_t ball_x ball_y} { dl_set $tg:$c [dl_llist $tg:$c] }

        if { ![dg_exists $name] } { dg_copy $tg $name } else { dg_append $name $tg }
        dg_delete $tg
    }

    set n [dl_length $name:side]
    dl_set $name:id [dl_fromto 0 $n]
    return $name
}

# ------------------------------------------------------------------
# draw_trial -- lightweight dlg_ overview of one trajectory. Uses only the
# generic dlg_/cgraph commands, so it renders identically in dlshell (Tk
# canvas) and the ess_control web virtual display. Pass a dict from
# sample_trajectory.
#
#   options: -window {llx lly urx ury}  (default -16 -9 16 9)
#            -clear 0|1     (clear + set window/background first; default 1)
#            -flush 0|1     (flush the buffer at the end; default 1)
# Occlusion (if the trial was passed through occlude) dims the hidden samples.
# ------------------------------------------------------------------
proc launch_sim::draw_trial { tr args } {
    if { [dict exists $tr boundary] } {
        switch -- [dict get $tr boundary] {
            circle { return [draw_trial_circle $tr {*}$args] }
            arc    { return [draw_trial_arc    $tr {*}$args] }
        }
    }
    array set opt {-window {-16 -9 16 9} -clear 1 -flush 1}
    array set opt $args

    if { $opt(-clear) } {
        clearwin
        setbackground [dlg_rgbcolor 10 10 10]
        setwindow {*}$opt(-window)
    }
    lassign $opt(-window) wllx wlly wurx wury

    set floor_y    [dict get $tr floor_y]
    set lcatcher_x [dict get $tr lcatcher_x]
    set rcatcher_x [dict get $tr rcatcher_x]
    set ghw        [dict get $tr goal_halfwidth]
    set lx         [dict get $tr launcher_x]
    set ly         [dict get $tr launcher_y]

    # floor
    dlg_lines [dl_flist $wllx $wurx] [dl_flist $floor_y $floor_y] \
        -linecolor [dlg_rgbcolor 80 80 80]
    # goals (bright = the target side)
    set tside [dict get $tr side]
    foreach gx [list $lcatcher_x $rcatcher_x] s {0 1} {
        if { $s == $tside } { set c [dlg_rgbcolor 60 220 90] } else { set c [dlg_rgbcolor 110 150 110] }
        dlg_lines [dl_flist [expr {$gx-$ghw}] [expr {$gx+$ghw}]] \
                  [dl_flist $floor_y $floor_y] -linecolor $c
    }
    # occluder regions (decoupled; drawn if the trial was passed through occlude)
    set regions {}
    if { [dict exists $tr occluder_regions] } { set regions [dict get $tr occluder_regions] }
    if { [llength $regions] } { draw_occluder_regions $regions }
    # launcher
    dlg_markers [dl_flist $lx] [dl_flist $ly] -marker fcircle -size 0.8x \
        -color [dlg_rgbcolor 150 150 150]

    # trajectory: dim where occluded (per regions), else bright
    set vx_ {}; set vy_ {}; set occ_xs {}; set occ_ys {}
    foreach x [dict get $tr ball_x] y [dict get $tr ball_y] {
        if { [llength $regions] && [point_occluded $x $y $regions] } {
            lappend occ_xs $x; lappend occ_ys $y
        } else {
            lappend vx_ $x; lappend vy_ $y
        }
    }
    if { [llength $vx_] > 1 } {
        dlg_lines [dl_flist {*}$vx_] [dl_flist {*}$vy_] -linecolor [dlg_rgbcolor 0 255 255]
    }
    if { [llength $occ_xs] > 1 } {
        dlg_markers [dl_flist {*}$occ_xs] [dl_flist {*}$occ_ys] \
            -marker fcircle -size 0.12x -color [dlg_rgbcolor 70 90 90]
    }
    # landing point
    dlg_markers [dl_flist [dict get $tr land_x]] [dl_flist $floor_y] \
        -marker fcircle -size 0.3x -color [dlg_rgbcolor 255 220 120]

    # flushwin is the display-push hook the host environment supplies
    # (dlshell canvas, ess_control virtual display); absent in a headless
    # tclsh, where the gbuf is built but not rendered.
    if { $opt(-flush) && [llength [info commands flushwin]] } { flushwin }
}

# ------------------------------------------------------------------
# draw_trial_circle -- dlg_ overview for a circle-boundary trial: the ring,
# any (decoupled) occluder regions, launcher, trajectory (visible/occluded),
# and the exit point on the rim. Dispatched to by draw_trial. Options match
# draw_trial (-clear/-flush; -window defaults to fit the circle).
# ------------------------------------------------------------------
proc launch_sim::draw_trial_circle { tr args } {
    variable pi
    set cx [dict get $tr circle_cx]; set cy [dict get $tr circle_cy]
    set R  [dict get $tr circle_r]
    array set opt [list -window [list [expr {$cx-$R-1}] [expr {$cy-$R-1}] \
                                      [expr {$cx+$R+1}] [expr {$cy+$R+1}]] \
                        -clear 1 -flush 1]
    array set opt $args

    if { $opt(-clear) } {
        clearwin
        setbackground [dlg_rgbcolor 10 10 10]
        setwindow {*}$opt(-window)
    }

    # the ring
    set rx {}; set ry {}
    for { set k 0 } { $k <= 72 } { incr k } {
        set a [expr {$k/72.0*2.0*$pi}]
        lappend rx [expr {$cx+$R*cos($a)}]; lappend ry [expr {$cy+$R*sin($a)}]
    }
    dlg_lines [dl_flist {*}$rx] [dl_flist {*}$ry] -linecolor [dlg_rgbcolor 80 80 80]

    # occluder regions (decoupled) drawn under the path
    set regions {}
    if { [dict exists $tr occluder_regions] } { set regions [dict get $tr occluder_regions] }
    if { [llength $regions] } { draw_occluder_regions $regions }

    # launcher
    dlg_markers [dl_flist [dict get $tr launcher_x]] [dl_flist [dict get $tr launcher_y]] \
        -marker fcircle -size 0.6x -color [dlg_rgbcolor 150 150 150]

    # trajectory: dim where occluded (per regions), else bright
    set vx_ {}; set vy_ {}; set occ_xs {}; set occ_ys {}
    foreach x [dict get $tr ball_x] y [dict get $tr ball_y] {
        if { [llength $regions] && [point_occluded $x $y $regions] } {
            lappend occ_xs $x; lappend occ_ys $y
        } else {
            lappend vx_ $x; lappend vy_ $y
        }
    }
    if { [llength $vx_] > 1 } {
        dlg_lines [dl_flist {*}$vx_] [dl_flist {*}$vy_] -linecolor [dlg_rgbcolor 0 255 255]
    }
    if { [llength $occ_xs] > 1 } {
        dlg_markers [dl_flist {*}$occ_xs] [dl_flist {*}$occ_ys] \
            -marker fcircle -size 0.12x -color [dlg_rgbcolor 70 90 90]
    }

    # exit point on the rim
    dlg_markers [dl_flist [dict get $tr land_x]] [dl_flist [dict get $tr land_y]] \
        -marker fcircle -size 0.35x -color [dlg_rgbcolor 255 220 120]

    if { $opt(-flush) && [llength [info commands flushwin]] } { flushwin }
}

# Draw occluder regions (a union of rect/circle/arc primitives) as filled
# shapes -- generic, used by any boundary mode's draw.
proc launch_sim::draw_occluder_regions { regions {fill {}} } {
    variable pi
    if { $fill eq {} } { set fill [dlg_rgbcolor 45 45 60] }
    foreach reg $regions {
        switch -- [dict get $reg type] {
            rect {
                set x0 [dict get $reg x0]; set y0 [dict get $reg y0]
                set x1 [dict get $reg x1]; set y1 [dict get $reg y1]
                dlg_lines [dl_flist $x0 $x1 $x1 $x0 $x0] [dl_flist $y0 $y0 $y1 $y1 $y0] \
                    -fillcolor $fill -linecolor $fill
            }
            circle {
                set cx [dict get $reg cx]; set cy [dict get $reg cy]; set r [dict get $reg r]
                dlg_markers [dl_flist $cx] [dl_flist $cy] -marker fcircle \
                    -size [expr {2.0*$r}]x -color $fill
            }
            arc {
                set cx [dict get $reg cx]; set cy [dict get $reg cy]
                set r0 [dict get $reg r0]; set r1 [dict get $reg r1]
                set a0 [dict get $reg a0]; set a1 [dict get $reg a1]
                set n 24
                for { set k 0 } { $k < $n } { incr k } {
                    set aa [expr {$a0 + ($k/double($n))*($a1-$a0)}]
                    set ab [expr {$a0 + (($k+1)/double($n))*($a1-$a0)}]
                    dlg_lines \
                        [dl_flist [expr {$cx+$r1*cos($aa)}] [expr {$cx+$r1*cos($ab)}] \
                                  [expr {$cx+$r0*cos($ab)}] [expr {$cx+$r0*cos($aa)}] [expr {$cx+$r1*cos($aa)}]] \
                        [dl_flist [expr {$cy+$r1*sin($aa)}] [expr {$cy+$r1*sin($ab)}] \
                                  [expr {$cy+$r0*sin($ab)}] [expr {$cy+$r0*sin($aa)}] [expr {$cy+$r1*sin($aa)}]] \
                        -fillcolor $fill -linecolor $fill
                }
            }
        }
    }
}

# ------------------------------------------------------------------
# draw_trial_arc -- dlg_ overview for an arc-landing trial: the launcher-
# centered landing arc (only the valid span), the heading reference, any
# occluder regions (from occlude), the trajectory (bright/dim by occlusion),
# the exit point, and the catcher (a short tangent bar) at the true landing.
# ------------------------------------------------------------------
proc launch_sim::draw_trial_arc { tr args } {
    variable pi
    set cx [dict get $tr arc_cx]; set cy [dict get $tr arc_cy]
    set R  [dict get $tr arc_radius]; set h [dict get $tr heading]
    set half [expr {([dict get $tr arc_span_deg]/2.0)*$pi/180.0}]
    set W [expr {$R+2}]
    array set opt [list -window [list [expr {$cx-$W}] [expr {$cy-$W}] \
                                      [expr {$cx+$W}] [expr {$cy+$W}]] -clear 1 -flush 1]
    array set opt $args
    if { $opt(-clear) } {
        clearwin; setbackground [dlg_rgbcolor 10 10 10]; setwindow {*}$opt(-window)
    }

    # occluder regions first (drawn under the path)
    if { [dict exists $tr occluder_regions] } {
        draw_occluder_regions [dict get $tr occluder_regions]
    }

    # the valid landing arc (heading +- half span)
    set ax {}; set ay {}; set n 48
    for { set k 0 } { $k <= $n } { incr k } {
        set a [expr {$h-$half + ($k/double($n))*2.0*$half}]
        lappend ax [expr {$cx+$R*cos($a)}]; lappend ay [expr {$cy+$R*sin($a)}]
    }
    dlg_lines [dl_flist {*}$ax] [dl_flist {*}$ay] -linecolor [dlg_rgbcolor 90 90 90]
    # heading reference (arc center direction) -- the straight-ahead "zero"
    dlg_lines [dl_flist $cx [expr {$cx+$R*cos($h)}]] [dl_flist $cy [expr {$cy+$R*sin($h)}]] \
        -linecolor [dlg_rgbcolor 50 50 60]

    # launcher
    dlg_markers [dl_flist $cx] [dl_flist $cy] -marker fcircle -size 0.5x \
        -color [dlg_rgbcolor 150 150 150]

    # trajectory: dim where occluded (per the occluder regions), else bright
    set regions {}
    if { [dict exists $tr occluder_regions] } { set regions [dict get $tr occluder_regions] }
    set vx_ {}; set vy_ {}; set ox {}; set oy {}
    foreach x [dict get $tr ball_x] y [dict get $tr ball_y] {
        if { [llength $regions] && [point_occluded $x $y $regions] } {
            lappend ox $x; lappend oy $y
        } else {
            lappend vx_ $x; lappend vy_ $y
        }
    }
    if { [llength $vx_] > 1 } {
        dlg_lines [dl_flist {*}$vx_] [dl_flist {*}$vy_] -linecolor [dlg_rgbcolor 0 255 255]
    }
    if { [llength $ox] > 1 } {
        dlg_markers [dl_flist {*}$ox] [dl_flist {*}$oy] -marker fcircle -size 0.12x \
            -color [dlg_rgbcolor 70 90 90]
    }

    # the catcher: a short bar tangent to the arc at the true landing
    lassign [catcher_pose $tr [dict get $tr deviation]] kx ky ka
    set hw 0.9
    dlg_lines [dl_flist [expr {$kx-$hw*cos($ka)}] [expr {$kx+$hw*cos($ka)}]] \
              [dl_flist [expr {$ky-$hw*sin($ka)}] [expr {$ky+$hw*sin($ka)}]] \
        -linecolor [dlg_rgbcolor 255 220 120]
    dlg_markers [dl_flist [dict get $tr land_x]] [dl_flist [dict get $tr land_y]] \
        -marker fcircle -size 0.25x -color [dlg_rgbcolor 255 220 120]

    if { $opt(-flush) && [llength [info commands flushwin]] } { flushwin }
}

namespace eval launch_sim {
    namespace export default_params shape_geometry \
        sample_trajectory sample_trajectory_circle sample_trajectory_arc \
        ball_pos_at_time ball_vel_at_time catcher_pose \
        point_occluded occlusion_intervals occlude \
        build_trials draw_trial draw_occluder_regions
}
