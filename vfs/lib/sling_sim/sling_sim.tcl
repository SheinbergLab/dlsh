# sling_sim.tcl -- the slingshot world, ONE definition.
#
# A ball seated in a slingshot at (anchor_x, anchor_y) is drawn back by a
# pull displacement (dx, dy) and released; it flies under gravity, may
# bounce off static obstacles, and either lands in an open-topped target
# bucket (a HIT) or does not (a MISS). Everything here runs on the headless
# `box2d` dlsh package, so the SAME code produces:
#
#   * the loader's reachability sweep (which targets can be hit at all),
#   * the stim's live preview ("training wheels": where a release RIGHT NOW
#     would send the ball),
#   * the stim's deferred playback (the flight is pre-simulated once at the
#     instant of release, then replayed kinematically -- outcome decided by
#     the simulation, never by frame timing),
#   * off-line reconstruction from the logged release vector.
#
# Because the preview and the flight are the same simulation from the same
# world, the dotted line at release IS the trajectory. Keep it that way: no
# caller should re-implement any piece of this.
#
# Units: degrees of visual angle (dva) as Box2D meters, seconds, dva/s. Pull
# displacement is where the hand has drawn the seat TO, relative to the
# anchor -- so a pull down-left launches up-right, exactly as a real
# slingshot does. The one mapping from pull to launch velocity is
# `sling_sim::velocity`; ess_sling-1.0.tm carries a copy of the formula
# (it cannot depend on dlsh.zip) and tests/test_ess_sling.tcl pins the two
# together.
#
# Obstacles are static boxes {x y w h angle_deg restitution}. Kinematic
# (moving) obstacles are a later extension: they belong in the spec with a
# deterministic path and a phase, so `simulate` can step them identically
# in the preview, the release presim, and the replay.
#
# A "spec" is a plain dict; `default_spec` lists every key. Column-per-key
# is how it rides in stimdg (`spec_from_stimdg` / `spec_to_columns`).

package require box2d
package provide sling_sim 1.0

namespace eval sling_sim {
    variable pi 3.14159265358979

    # Names used for bodies -- contacts are reported by these.
    variable name_ball    ball
    variable name_ground  ground
    variable name_floor   target_b
    variable name_wall_l  target_l
    variable name_wall_r  target_r
    variable name_base    target_base
    variable name_obs     obs        ;# obs_0, obs_1, ...
}

proc sling_sim::default_spec {} {
    return [dict create \
        anchor_x       -9.0  anchor_y      -3.0 \
        ball_r          0.4 \
        gravity        -9.8 \
        reach           3.0  v_max        16.0  min_frac 0.15 \
        ground_y       -8.0 \
        field_hx       16.0  field_hy      9.0 \
        target_x        7.0  target_y     -6.0 \
        target_w        2.4  target_h      1.6  wall_t 0.15  base_h 0.4 \
        ball_restitution   0.30 \
        ground_restitution 0.20 \
        target_restitution 0.10 \
        obstacles      {} \
        dt             [expr {1.0/120.0}] \
        max_t           6.0]
}

# The ONE pull -> launch-velocity mapping. dx,dy is the pull displacement
# (dva) from the anchor; the ball leaves opposite to it, at v_max when the
# pull reaches `reach`. Clamped: a pull past reach launches at v_max.
proc sling_sim::velocity { spec dx dy } {
    set reach [dict get $spec reach]
    set vmax  [dict get $spec v_max]
    if { $reach <= 0.0 } { return {0.0 0.0} }
    set mag [expr {hypot($dx, $dy)}]
    if { $mag <= 0.0 } { return {0.0 0.0} }
    set frac [expr {$mag/$reach}]
    if { $frac > 1.0 } { set frac 1.0 }
    set speed [expr {$vmax*$frac}]
    return [list [expr {-$dx/$mag*$speed}] [expr {-$dy/$mag*$speed}]]
}

# Pull fraction of full draw, clamped to [0,1].
proc sling_sim::pull_frac { spec dx dy } {
    set reach [dict get $spec reach]
    if { $reach <= 0.0 } { return 0.0 }
    set f [expr {hypot($dx, $dy)/$reach}]
    return [expr {$f > 1.0 ? 1.0 : $f}]
}

# A pull from its polar description: frac of full draw, pull angle in
# degrees (0 = the hand pulled straight +x, i.e. the ball launches -x).
proc sling_sim::pull_from_polar { spec frac angle_deg } {
    variable pi
    set r [expr {[dict get $spec reach]*$frac}]
    set a [expr {$angle_deg*$pi/180.0}]
    return [list [expr {$r*cos($a)}] [expr {$r*sin($a)}]]
}

# Geometry of the target bucket, as {x y w h} boxes in this order:
#   floor   the HIT body (target_b), INSET between the walls
#   wall_l  outer shell, from the bottom of the base up to the rim
#   wall_r
#   base    a plinth under the floor, full width (target_base)
#
# The floor is enclosed on every side but its top: its ends are behind the
# walls and its underside is on the base, so the only way to touch it is
# from above, between the walls -- i.e. with the ball's centre inside the
# bucket. A graze along the outside of the bucket (its underside, an end)
# lands on the base or a wall, which are contacts but not a hit. That
# closes the false hit a full-width floor with walls sitting on top of it
# produced when the ball clipped the floor's outer corner.
#
# Shared by the physics build and every renderer so they cannot drift.
proc sling_sim::target_geometry { spec } {
    set cx [dict get $spec target_x]
    set cy [dict get $spec target_y]
    set w  [dict get $spec target_w]
    set h  [dict get $spec target_h]
    set t  [dict get $spec wall_t]
    set b  [dict get $spec base_h]
    set half [expr {$w/2.0}]
    set floor_top [expr {$cy + $t/2.0}]
    set base_bot  [expr {$cy - $t/2.0 - $b}]
    set wall_h    [expr {$h + $t + $b}]
    set wall_cy   [expr {$base_bot + $wall_h/2.0}]
    return [list \
        [list $cx $cy [expr {$w - 2.0*$t}] $t] \
        [list [expr {$cx - $half + $t/2.0}] $wall_cy $t $wall_h] \
        [list [expr {$cx + $half - $t/2.0}] $wall_cy $t $wall_h] \
        [list $cx [expr {$cy - $t/2.0 - $b/2.0}] $w $b]]
}

# The bucket's interior opening: {x_lo x_hi} between the inner wall faces.
proc sling_sim::target_mouth { spec } {
    set cx [dict get $spec target_x]
    set w  [dict get $spec target_w]
    set t  [dict get $spec wall_t]
    return [list [expr {$cx - $w/2.0 + $t}] [expr {$cx + $w/2.0 - $t}]]
}

# Build the world. Returns {world ball}. Static bodies first so they exist
# when the ball is stepped. The ball is created DYNAMIC at the anchor; the
# caller sets its velocity.
proc sling_sim::build_world { spec } {
    variable pi
    variable name_ball; variable name_ground
    variable name_floor; variable name_wall_l; variable name_wall_r
    variable name_base
    variable name_obs

    set w [box2d::createWorld]

    # gravity: the package's world is (0,-10) by default; box2d::createWorld
    # takes no gravity argument, so gravity is applied as a per-step force
    # correction? No -- it is simplest to accept the world's gravity and
    # scale the spec's if they differ. Verified below in `gravity_world`.
    set g [gravity_world $w]
    dict set spec _g_world $g

    set hx [dict get $spec field_hx]
    set gy [dict get $spec ground_y]
    set ground [box2d::createBox $w $name_ground 0 0.0 [expr {$gy - 0.5}] \
                    [expr {2.0*$hx + 4.0}] 1.0 0]
    box2d::setRestitution $w $ground [dict get $spec ground_restitution]

    lassign [target_geometry $spec] fl wl wr bs
    foreach { name geom } [list $name_floor $fl $name_wall_l $wl $name_wall_r $wr \
                               $name_base $bs] {
        lassign $geom x y bw bh
        set b [box2d::createBox $w $name 0 $x $y $bw $bh 0]
        box2d::setRestitution $w $b [dict get $spec target_restitution]
    }

    set i 0
    foreach o [dict get $spec obstacles] {
        lassign $o ox oy ow oh oang orest
        if { $oang eq "" }  { set oang 0.0 }
        if { $orest eq "" } { set orest 0.4 }
        set b [box2d::createBox $w ${name_obs}_$i 0 $ox $oy $ow $oh \
                   [expr {$oang*$pi/180.0}]]
        box2d::setRestitution $w $b $orest
        incr i
    }

    set ball [box2d::createCircle $w $name_ball 2 \
                  [dict get $spec anchor_x] [dict get $spec anchor_y] \
                  [dict get $spec ball_r]]
    box2d::setRestitution $w $ball [dict get $spec ball_restitution]
    return [list $w $ball]
}

# The box2d package's world gravity is fixed at (0, -10) (no createWorld
# option). The spec's `gravity` is honoured by applying the DIFFERENCE as a
# per-step force on the ball: F = m*(g_spec - g_world). Mass of a unit-
# density circle is pi*r^2, which is what box2d gives a default fixture.
proc sling_sim::gravity_world { w } { return -10.0 }

proc sling_sim::gravity_correction { spec } {
    variable pi
    set r [dict get $spec ball_r]
    set m [expr {$pi*$r*$r}]
    set dg [expr {[dict get $spec gravity] - [gravity_world {}]}]
    return [expr {$m*$dg}]
}

# Simulate one release. Returns a dict:
#   outcome   hit | miss | out | timeout
#   t_end     time (s) at which the outcome was decided
#   n_steps   steps taken
#   land_x/y  ball position at t_end
#   first_hit name of the first body contacted, or ""
#   contacts  list of {t name} for every begin-contact (ball vs body)
#   t x y     per-step lists (when -record 1, the default)
#
# A HIT is a begin-contact between the ball and the target floor. A MISS is
# a contact with the ground, or the ball leaving the field (`out`), or
# max_t elapsing (`timeout`, e.g. the ball resting on an obstacle).
proc sling_sim::simulate { spec vx vy args } {
    variable name_ball; variable name_ground; variable name_floor

    set dt     [dict get $spec dt]
    set max_t  [dict get $spec max_t]
    set record 1
    foreach { k v } $args {
        switch -- $k {
            -dt     { set dt $v }
            -max_t  { set max_t $v }
            -record { set record $v }
            default { error "sling_sim::simulate: unknown option $k" }
        }
    }

    lassign [build_world $spec] w ball
    box2d::setLinearVelocity $w $ball $vx $vy
    set fy [gravity_correction $spec]

    set hx [dict get $spec field_hx]
    set hy [dict get $spec field_hy]
    set gy [dict get $spec ground_y]

    set ts {}; set xs {}; set ys {}
    set contacts {}
    set outcome timeout
    set first_hit ""
    set t 0.0
    set n 0
    set nmax [expr {int(ceil($max_t/$dt))}]
    lassign [box2d::getBodyInfo $w $ball] bx by _
    if { $record } { lappend ts 0.0; lappend xs $bx; lappend ys $by }

    while { $n < $nmax } {
        if { $fy != 0.0 } { box2d::applyForce $w $ball 0.0 $fy }
        box2d::step $w $dt
        incr n
        set t [expr {$n*$dt}]
        lassign [box2d::getBodyInfo $w $ball] bx by _
        if { $record } { lappend ts $t; lappend xs $bx; lappend ys $by }

        if { [box2d::getContactBeginEventCount $w] > 0 } {
            foreach c [box2d::getContactBeginEvents $w] {
                if { [lsearch -exact $c $name_ball] < 0 } continue
                set other [lindex $c [expr {[lindex $c 0] eq $name_ball ? 1 : 0}]]
                lappend contacts [list $t $other]
                if { $first_hit eq "" } { set first_hit $other }
                if { $other eq $name_floor } { set outcome hit }
                if { $other eq $name_ground } { set outcome miss }
            }
            if { $outcome ne "timeout" } break
        }
        if { $bx < -$hx || $bx > $hx || $by > 3.0*$hy || $by < $gy - 2.0 } {
            set outcome out
            break
        }
    }
    box2d::destroy $w

    set res [dict create outcome $outcome t_end $t n_steps $n \
                 land_x $bx land_y $by first_hit $first_hit contacts $contacts]
    if { $record } {
        dict set res t $ts
        dict set res x $xs
        dict set res y $ys
    }
    return $res
}

# The preview: the flight a release of pull (dx,dy) would produce right
# now. Same simulation, called from the pull rather than the velocity.
proc sling_sim::preview { spec dx dy args } {
    lassign [velocity $spec $dx $dy] vx vy
    return [simulate $spec $vx $vy {*}$args]
}

# Sweep the pull space: n_frac fractions in [min_frac, 1] x n_angle pull
# angles over the full circle. Returns a dict:
#   n_total n_hits  hits {list of {frac angle_deg}}
#   best_frac best_angle  -- the hit closest to the centre of the hit set
#   (the "easiest" release); "" if none.
proc sling_sim::sweep { spec args } {
    set n_frac 12
    set n_angle 36
    set amin 0.0
    set amax 360.0
    foreach { k v } $args {
        switch -- $k {
            -n_frac    { set n_frac $v }
            -n_angle   { set n_angle $v }
            -angle_min { set amin $v }
            -angle_max { set amax $v }
            default { error "sling_sim::sweep: unknown option $k" }
        }
    }
    set fmin [dict get $spec min_frac]
    set hits {}
    set n 0
    for { set i 0 } { $i < $n_frac } { incr i } {
        set frac [expr {$n_frac == 1 ? 1.0 : $fmin + (1.0 - $fmin)*$i/double($n_frac - 1)}]
        for { set j 0 } { $j < $n_angle } { incr j } {
            set ang [expr {$amin + ($amax - $amin)*$j/double($n_angle)}]
            lassign [pull_from_polar $spec $frac $ang] dx dy
            set r [preview $spec $dx $dy -record 0]
            incr n
            if { [dict get $r outcome] eq "hit" } { lappend hits [list $frac $ang] }
        }
    }
    set best_frac ""; set best_angle ""
    if { [llength $hits] } {
        # centroid of the hit set (angles are close together for one target,
        # so a plain mean is fine), then the hit nearest to it
        set sf 0.0; set sa 0.0
        foreach h $hits { lassign $h f a; set sf [expr {$sf + $f}]; set sa [expr {$sa + $a}] }
        set mf [expr {$sf/[llength $hits]}]
        set ma [expr {$sa/[llength $hits]}]
        set bd 1e9
        foreach h $hits {
            lassign $h f a
            set d [expr {pow(($f - $mf)/1.0, 2) + pow(($a - $ma)/45.0, 2)}]
            if { $d < $bd } { set bd $d; set best_frac $f; set best_angle $a }
        }
    }
    return [dict create n_total $n n_hits [llength $hits] hits $hits \
                best_frac $best_frac best_angle $best_angle]
}

# --- stimdg round trip -----------------------------------------------------
#
# Scalar keys become one float column each; `obstacles` becomes six nested
# columns (one sublist per trial, possibly empty): obs_x obs_y obs_w obs_h
# obs_angle obs_restitution.

proc sling_sim::scalar_keys {} {
    return {anchor_x anchor_y ball_r gravity reach v_max min_frac ground_y
            field_hx field_hy target_x target_y target_w target_h wall_t base_h
            ball_restitution ground_restitution target_restitution dt max_t}
}

proc sling_sim::obstacle_keys {} {
    return {obs_x obs_y obs_w obs_h obs_angle obs_restitution}
}

# Rebuild a spec from row `row` of group `g`. Missing scalar columns fall
# back to the defaults, so an older file still yields a usable spec.
proc sling_sim::spec_from_stimdg { g row } {
    set spec [default_spec]
    foreach k [scalar_keys] {
        if { [dl_exists $g:$k] } { dict set spec $k [dl_get $g:$k $row] }
    }
    set obs {}
    if { [dl_exists $g:obs_x] } {
        set xs [dl_tcllist [dl_get $g:obs_x $row]]
        set ys [dl_tcllist [dl_get $g:obs_y $row]]
        set ws [dl_tcllist [dl_get $g:obs_w $row]]
        set hs [dl_tcllist [dl_get $g:obs_h $row]]
        set as [dl_tcllist [dl_get $g:obs_angle $row]]
        set rs [dl_tcllist [dl_get $g:obs_restitution $row]]
        foreach x $xs y $ys w $ws h $hs a $as r $rs {
            lappend obs [list $x $y $w $h $a $r]
        }
    }
    dict set spec obstacles $obs
    return $spec
}

# Write a LIST of specs (one per trial) into group `g` as columns.
proc sling_sim::specs_to_stimdg { g specs } {
    set n [llength $specs]
    foreach k [scalar_keys] {
        set vals {}
        foreach s $specs { lappend vals [dict get $s $k] }
        dl_set $g:$k [dl_flist {*}$vals]
    }
    set cols [dict create]
    foreach k [obstacle_keys] { dict set cols $k [dl_llist] }
    foreach s $specs {
        set per [dict create]
        foreach k [obstacle_keys] { dict set per $k {} }
        foreach o [dict get $s obstacles] {
            lassign $o ox oy ow oh oa orest
            if { $oa eq "" }    { set oa 0.0 }
            if { $orest eq "" } { set orest 0.4 }
            foreach k [obstacle_keys] v [list $ox $oy $ow $oh $oa $orest] {
                dict lappend per $k $v
            }
        }
        foreach k [obstacle_keys] {
            dl_append [dict get $cols $k] [dl_flist {*}[dict get $per $k]]
        }
    }
    foreach k [obstacle_keys] { dl_set $g:$k [dict get $cols $k] }
    return $g
}
