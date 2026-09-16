# b2world.tcl -- a headless Box2D world of TAGGED BODIES, given as data.
#
# The generic engine under paradigm physics libraries (sling_sim in
# ~/systems/ess/lib is the first). It knows nothing about slingshots,
# buckets or planks: a world is a dict of bodies, each with a name, a shape,
# a type and a list of ROLE tags; a run launches one body and steps until a
# STOP RULE fires -- a contact between two roles, a tracked body leaving
# the bounds, or the time cap. Paradigm content (what the bodies are, which
# contact means what, how a pull becomes a velocity) stays with the
# paradigm; adding a new obstacle kind, a new target shape or a moving body
# is data here, not a module change.
#
# Same code everywhere it runs (loader sweep, stim preview, release-time
# pre-simulation, replay, off-line analysis), so a preview IS the flight.
#
# World spec (a dict):
#   gravity   dva/s^2, negative down (default -10, the box2d world's own;
#             any other value is honoured by a per-step force on every
#             dynamic body, F = m*(g - g_world), m from the fixture's
#             default density 1: pi r^2 for a circle, w*h for a box)
#   dt        step (s), default 1/120
#   max_t     cap (s), default 6
#   bounds    {x_lo x_hi y_lo y_hi}; a TRACKED body outside -> "out"
#   bodies    list of body dicts:
#               name      unique; contacts are reported by name
#               shape     box | circle
#               type      static | kinematic | dynamic
#               x y       centre; w h (box) or r (circle); angle (deg, box)
#               restitution (default 0.3)   sensor 0|1 (default 0)
#               roles     list of tags, e.g. {obstacle} {target hit}
#               path      kinematic only: how it moves (see below)
#
# Kinematic paths (velocity-driven, so contacts see the body's motion):
#   {kind linear vx vy}                       constant velocity
#   {kind oscillate ax ay period phase_deg}   x(t) = x0 + ax*sin(w t + ph)
#
# Stop rules (-stop, a list; first match wins, evaluated in order):
#   {contact ROLE_A ROLE_B OUTCOME}   a begin-contact between a body carrying
#                                     ROLE_A and one carrying ROLE_B
#   plus, always: a tracked body leaving `bounds` -> out; max_t -> timeout.
#   -stop_proc NAME  an optional Tcl proc called every step as
#                    NAME world t positions (positions: name -> {x y angle}
#                    for tracked bodies); a non-empty return is the outcome.
#
# simulate returns a dict:
#   outcome t_end n_steps
#   contacts   list of {t name_a name_b} (every begin-contact, all bodies)
#   paths      dict name -> {t {..} x {..} y {..}} for each -record body
#   final      dict name -> {x y angle} for each tracked body
#
# Units are whatever the caller's are (dva here); box2d treats them as
# meters, which is fine at these scales.

package require box2d
package provide b2world 1.0

namespace eval b2world {
    variable pi 3.14159265358979
    variable g_world -10.0          ;# fixed in the box2d package's createWorld
    variable type_code {static 0 kinematic 1 dynamic 2}
}

proc b2world::default_spec {} {
    return [dict create gravity -10.0 dt [expr {1.0/120.0}] max_t 6.0 \
                bounds {-1e9 1e9 -1e9 1e9} bodies {}]
}

# Body dict helpers: b2world::body name shape type x y ?key val ...?
proc b2world::body { name shape type x y args } {
    set b [dict create name $name shape $shape type $type x $x y $y \
               angle 0.0 restitution 0.3 sensor 0 roles {} path {}]
    foreach { k v } $args { dict set b $k $v }
    if { $shape eq "box" && !([dict exists $b w] && [dict exists $b h]) } {
        error "b2world::body $name: a box needs w and h"
    }
    if { $shape eq "circle" && ![dict exists $b r] } {
        error "b2world::body $name: a circle needs r"
    }
    if { $type ni {static kinematic dynamic} } {
        error "b2world::body $name: type must be static|kinematic|dynamic"
    }
    return $b
}

proc b2world::add_body { specvar bodydict } {
    upvar 1 $specvar spec
    dict lappend spec bodies $bodydict
    return
}

proc b2world::find_body { spec name } {
    foreach b [dict get $spec bodies] {
        if { [dict get $b name] eq $name } { return $b }
    }
    return {}
}

proc b2world::mass { b } {
    variable pi
    if { [dict get $b shape] eq "circle" } {
        set r [dict get $b r]
        return [expr {$pi*$r*$r}]
    }
    return [expr {[dict get $b w]*[dict get $b h]}]
}

# Build the world. Returns {world handles} where handles is name -> body id.
# Static and kinematic bodies first, dynamic last, in spec order within each.
proc b2world::build { spec } {
    variable pi
    variable type_code
    set w [box2d::createWorld]
    set handles [dict create]
    foreach pass {static kinematic dynamic} {
        foreach b [dict get $spec bodies] {
            if { [dict get $b type] ne $pass } continue
            set name [dict get $b name]
            set tc   [dict get $type_code $pass]
            set sens [dict get $b sensor]
            if { [dict get $b shape] eq "box" } {
                set h [box2d::createBox $w $name $tc [dict get $b x] [dict get $b y] \
                           [dict get $b w] [dict get $b h] \
                           [expr {[dict get $b angle]*$pi/180.0}] $sens]
            } else {
                set h [box2d::createCircle $w $name $tc [dict get $b x] [dict get $b y] \
                           [dict get $b r] $sens]
            }
            box2d::setRestitution $w $h [dict get $b restitution]
            dict set handles $name $h
        }
    }
    return [list $w $handles]
}

# Velocity of a kinematic path at time t (s).
proc b2world::path_velocity { path t } {
    variable pi
    switch -- [dict get $path kind] {
        linear {
            return [list [dict get $path vx] [dict get $path vy]]
        }
        oscillate {
            set ax [dict get $path ax]; set ay [dict get $path ay]
            set T  [dict get $path period]
            set ph [expr {[dict get $path phase_deg]*$pi/180.0}]
            set om [expr {2.0*$pi/$T}]
            set c  [expr {$om*cos($om*$t + $ph)}]
            return [list [expr {$ax*$c}] [expr {$ay*$c}]]
        }
        default { error "b2world: unknown path kind '[dict get $path kind]'" }
    }
}

# Position of a kinematic path at time t, from its start (x0,y0) -- for a
# renderer that wants to draw a moving body without stepping a world.
proc b2world::path_position { path x0 y0 t } {
    variable pi
    switch -- [dict get $path kind] {
        linear {
            return [list [expr {$x0 + [dict get $path vx]*$t}] \
                         [expr {$y0 + [dict get $path vy]*$t}]]
        }
        oscillate {
            set ax [dict get $path ax]; set ay [dict get $path ay]
            set T  [dict get $path period]
            set ph [expr {[dict get $path phase_deg]*$pi/180.0}]
            set om [expr {2.0*$pi/$T}]
            set s  [expr {sin($om*$t + $ph) - sin($ph)}]
            return [list [expr {$x0 + $ax*$s}] [expr {$y0 + $ay*$s}]]
        }
        default { error "b2world: unknown path kind '[dict get $path kind]'" }
    }
}

proc b2world::has_role { b role } {
    return [expr {[lsearch -exact [dict get $b roles] $role] >= 0}]
}

# b2world::simulate spec ?options?
#   -launch {name vx vy}   set a body's velocity at t = 0
#   -stop rules            see the header
#   -stop_proc name        see the header
#   -record names          bodies whose per-step path is returned
#   -track names           bodies tested against bounds (default: -record)
#   -dt -max_t             override the spec
#   -stride n              record every n-th step (default 1)
proc b2world::simulate { spec args } {
    variable g_world
    set dt       [dict get $spec dt]
    set max_t    [dict get $spec max_t]
    set launch   {}
    set rules    {}
    set stop_proc ""
    set record   {}
    set track    ""
    set stride   1
    foreach { k v } $args {
        switch -- $k {
            -launch    { set launch $v }
            -stop      { set rules $v }
            -stop_proc { set stop_proc $v }
            -record    { set record $v }
            -track     { set track $v }
            -dt        { set dt $v }
            -max_t     { set max_t $v }
            -stride    { set stride $v }
            default    { error "b2world::simulate: unknown option $k" }
        }
    }
    if { $track eq "" } { set track $record }

    lassign [build $spec] w handles

    # roles by name, kinematic paths, gravity correction per dynamic body
    set roles [dict create]
    set kin   {}
    set corr  {}
    set dg [expr {[dict get $spec gravity] - $g_world}]
    foreach b [dict get $spec bodies] {
        set name [dict get $b name]
        dict set roles $name [dict get $b roles]
        if { [dict get $b type] eq "kinematic" && [dict get $b path] ne "" } {
            lappend kin [list $name [dict get $handles $name] [dict get $b path]]
        }
        if { [dict get $b type] eq "dynamic" && $dg != 0.0 } {
            lappend corr [list [dict get $handles $name] [expr {[mass $b]*$dg}]]
        }
    }

    if { [llength $launch] } {
        lassign $launch lname lvx lvy
        box2d::setLinearVelocity $w [dict get $handles $lname] $lvx $lvy
    }
    foreach k $kin {
        lassign $k name h path
        lassign [path_velocity $path 0.0] vx vy
        box2d::setLinearVelocity $w $h $vx $vy
    }

    lassign [dict get $spec bounds] x_lo x_hi y_lo y_hi

    set paths [dict create]
    foreach name $record { dict set paths $name [dict create t {} x {} y {}] }
    set positions [dict create]
    proc_record positions paths $w $handles $track $record 0.0 1

    set contacts {}
    set outcome timeout
    set t 0.0
    set n 0
    set nmax [expr {int(ceil($max_t/$dt))}]

    while { $n < $nmax } {
        foreach c $corr { lassign $c h fy; box2d::applyForce $w $h 0.0 $fy }
        foreach k $kin {
            lassign $k name h path
            lassign [path_velocity $path $t] vx vy
            box2d::setLinearVelocity $w $h $vx $vy
        }
        box2d::step $w $dt
        incr n
        set t [expr {$n*$dt}]
        proc_record positions paths $w $handles $track $record $t \
            [expr {$n % $stride == 0}]

        if { [box2d::getContactBeginEventCount $w] > 0 } {
            foreach c [box2d::getContactBeginEvents $w] {
                lassign $c a b
                lappend contacts [list $t $a $b]
                if { $outcome ne "timeout" } continue
                set ra [dict get $roles $a]; set rb [dict get $roles $b]
                foreach rule $rules {
                    lassign $rule kind r1 r2 oc
                    if { $kind ne "contact" } continue
                    if { ($r1 in $ra && $r2 in $rb) || ($r1 in $rb && $r2 in $ra) } {
                        set outcome $oc
                        break
                    }
                }
            }
            if { $outcome ne "timeout" } break
        }
        set out 0
        foreach name $track {
            lassign [dict get $positions $name] px py
            if { $px < $x_lo || $px > $x_hi || $py < $y_lo || $py > $y_hi } { set out 1 }
        }
        if { $out } { set outcome out; break }
        if { $stop_proc ne "" } {
            set r [$stop_proc $w $t $positions]
            if { $r ne "" } { set outcome $r; break }
        }
    }
    box2d::destroy $w

    return [dict create outcome $outcome t_end $t n_steps $n \
                contacts $contacts paths $paths final $positions]
}

# (internal) refresh tracked positions; append to recorded paths when due
proc b2world::proc_record { posvar pathsvar w handles track record t due } {
    upvar 1 $posvar positions
    upvar 1 $pathsvar paths
    foreach name $track {
        dict set positions $name [box2d::getBodyInfo $w [dict get $handles $name]]
    }
    if { !$due } return
    foreach name $record {
        if { ![dict exists $positions $name] } {
            dict set positions $name [box2d::getBodyInfo $w [dict get $handles $name]]
        }
        lassign [dict get $positions $name] px py
        # explicit get/set rather than `dict with`: that would unpack the
        # path's t/x/y over this proc's own t and append the list to itself
        set p [dict get $paths $name]
        dict lappend p t $t
        dict lappend p x $px
        dict lappend p y $py
        dict set paths $name $p
    }
}

# Contacts involving one body, as {t other}.
proc b2world::contacts_of { result name } {
    set out {}
    foreach c [dict get $result contacts] {
        lassign $c t a b
        if { $a eq $name } { lappend out [list $t $b] } \
        elseif { $b eq $name } { lappend out [list $t $a] }
    }
    return $out
}

# Sweep: run simulate once per case. `launch_proc` is called with the case
# and must return {name vx vy}. Returns a list of {case outcome result}
# (result without paths unless -record is given, to keep it light).
proc b2world::sweep { spec cases launch_proc args } {
    set out {}
    foreach case $cases {
        set launch [$launch_proc $case]
        set r [simulate $spec -launch $launch {*}$args]
        lappend out [list $case [dict get $r outcome] $r]
    }
    return $out
}
