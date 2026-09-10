# polygon.tcl -- traj motion plugin: a target visiting the vertices of a
# regular polygon on a schedule. ONE model with two playback modes, so the
# same trial design (vertex sequence + timing) can be run as a SACCADE task
# (mode step: the target sits at a vertex and jumps to the next on schedule)
# or as a PURSUIT task (mode smooth: it travels continuously between the same
# vertices, arriving on the same schedule). That is the point of putting the
# schedule here rather than in a stim file: the spatiotemporal-prediction
# question (predictable vs random ordering, fixed vs random timing) is asked
# of saccades and pursuit with identical geometry and identical event times.
#
# GEOMETRY
#   vertex i (0..N-1) sits at angle rotation_deg + i*360/N (degrees, CCW
#   positive, so increasing index is CCW on screen) on a circle of `radius`
#   about (center_x, center_y). Index -1 is the CENTER itself, so a schedule
#   may begin at fixation and step out to the ring.
#
# SCHEDULE
#   seq        = the K+1 vertex indices visited, in order (seq[0] is where the
#                target is at t = 0)
#   intervals  = K durations (s); intervals[k] separates the arrival at seq[k]
#                from the arrival at seq[k+1]
#   times      = cumulative arrival times T_0 = 0, T_1, ... T_K (derived)
#   hold_last  = how long the target stays at seq[K] after arriving (default:
#                the last interval, so the final step is observable)
#   land_time  = T_K + hold_last = the `end` landmark
#
# MODES
#   step    pos(t) = vertex(seq[k]) for T_k <= t < T_{k+1}: piecewise constant,
#           the jump to seq[k+1] happens exactly at T_{k+1}. vel is 0 (the
#           jumps are discontinuities, not velocities).
#   smooth  the target moves from seq[k] to seq[k+1] over [T_k, T_{k+1}] and
#           is AT seq[k+1] at T_{k+1}; path chord = straight line at constant
#           speed, path arc = along the circle at constant angular speed (only
#           between ADJACENT vertices, never from the center).
#
# LANDMARKS
#   start {0}, vertex {T_1 .. T_K} (the target reaches seq[k]: the saccade
#   trigger in step mode, the vertex passage in smooth mode), end {land_time}.
#
# Dicts MUST come from traj::polygon::make (it validates and caches `times`).

namespace eval traj::polygon {}

# ------------------------------------------------------------------
# Vertex-sequence generator -- the ONE definition of "predictable" (cw / ccw)
# and "random" orderings, shared by the loader and its tests.
#   n_vertices  N            n_steps  K (sequence length is K+1)
#   kind        cw | ccw | random
#   start       first vertex (0..N-1)
#   offsets     random only: K values in 1..N-1 (each step moves by that many
#               vertices), supplied by the caller so the loader's randomness
#               stays under dl_srand. cw/ccw ignore it.
# cw = decreasing index (angles increase CCW, so index-1 is clockwise on
# screen); ccw = increasing index. Every step lands on a DIFFERENT vertex.
# ------------------------------------------------------------------
proc traj::polygon::sequence { n_vertices n_steps kind start { offsets {} } } {
    if { $n_vertices < 2 } { error "polygon::sequence: n_vertices must be >= 2" }
    if { $n_steps < 0 }    { error "polygon::sequence: n_steps must be >= 0" }
    if { $start < 0 || $start >= $n_vertices } {
        error "polygon::sequence: start $start out of range 0..[expr {$n_vertices-1}]"
    }
    switch -- $kind {
        cw     { set offs [lrepeat $n_steps [expr {$n_vertices-1}]] }
        ccw    { set offs [lrepeat $n_steps 1] }
        random {
            if { [llength $offsets] < $n_steps } {
                error "polygon::sequence: random needs $n_steps offsets, got [llength $offsets]"
            }
            set offs [lrange $offsets 0 [expr {$n_steps-1}]]
            foreach o $offs {
                if { $o < 1 || $o >= $n_vertices } {
                    error "polygon::sequence: offset $o must be in 1..[expr {$n_vertices-1}]"
                }
            }
        }
        default { error "polygon::sequence: kind must be cw, ccw or random, got '$kind'" }
    }
    set seq [list $start]
    set v $start
    foreach o $offs {
        set v [expr {($v + $o) % $n_vertices}]
        lappend seq $v
    }
    return $seq
}

# ------------------------------------------------------------------
# Constructor
# ------------------------------------------------------------------
proc traj::polygon::make { n_vertices radius seq intervals args } {
    set center_x 0.0 ; set center_y 0.0 ; set rotation_deg 90.0
    set mode step ; set path chord ; set hold_last -1
    foreach { o v } $args {
        switch -- $o {
            -center_x     { set center_x $v }
            -center_y     { set center_y $v }
            -rotation_deg { set rotation_deg $v }
            -mode         { set mode $v }
            -path         { set path $v }
            -hold_last    { set hold_last $v }
            default       { error "traj::polygon::make: unknown option '$o'" }
        }
    }
    set N [expr {int($n_vertices)}]
    if { $N < 2 }        { error "polygon: n_vertices must be >= 2, got $n_vertices" }
    if { $radius <= 0 }  { error "polygon: radius must be > 0, got $radius" }
    if { $mode ni {step smooth} } { error "polygon: mode must be step or smooth, got '$mode'" }
    if { $path ni {chord arc} }   { error "polygon: path must be chord or arc, got '$path'" }
    set K [expr {[llength $seq] - 1}]
    if { $K < 1 } { error "polygon: seq needs at least 2 entries (one step), got [llength $seq]" }
    if { [llength $intervals] != $K } {
        error "polygon: [llength $seq] vertices need $K intervals, got [llength $intervals]"
    }
    foreach v $seq {
        if { $v < -1 || $v >= $N } {
            error "polygon: seq entry $v out of range (-1 = center, 0..[expr {$N-1}])"
        }
    }
    set times [list 0.0]
    set T 0.0
    foreach dt $intervals {
        if { $dt <= 0 } { error "polygon: every interval must be > 0, got $dt" }
        set T [expr {$T + double($dt)}]
        lappend times $T
    }
    if { $path eq "arc" } {
        if { $N < 3 } { error "polygon: path arc needs n_vertices >= 3" }
        for { set k 0 } { $k < $K } { incr k } {
            set a [lindex $seq $k] ; set b [lindex $seq [expr {$k+1}]]
            if { $a < 0 || $b < 0 } {
                error "polygon: path arc cannot start or end at the center (seq entry -1); use path chord"
            }
            set d [expr {(($b - $a) % $N + $N) % $N}]
            if { $d != 1 && $d != $N-1 } {
                error "polygon: path arc needs ADJACENT vertices at every step; step $k goes $a -> $b"
            }
        }
    } else {
        for { set k 0 } { $k < $K } { incr k } {
            if { [lindex $seq $k] == [lindex $seq [expr {$k+1}]] } {
                error "polygon: step $k does not move (seq [lindex $seq $k] -> [lindex $seq $k]); every step must change vertex"
            }
        }
    }
    if { $hold_last < 0 } { set hold_last [lindex $intervals end] }
    set land [expr {$T + double($hold_last)}]

    set p [dict create \
        motion_type polygon \
        n_vertices $N radius [expr {double($radius)}] \
        center_x [expr {double($center_x)}] center_y [expr {double($center_y)}] \
        rotation_deg [expr {double($rotation_deg)}] \
        mode $mode path $path \
        seq $seq intervals $intervals times $times n_steps $K \
        hold_last [expr {double($hold_last)}] land_time $land \
        maxext [expr {max(abs($center_x) + $radius, abs($center_y) + $radius)}]]
    lassign [traj::polygon::pos $p $land] lx ly
    dict set p land_x $lx ; dict set p land_y $ly
    return $p
}

# angle (rad) of vertex i; the center has no angle
proc traj::polygon::_angle { p i } {
    set N [dict get $p n_vertices]
    return [expr {([dict get $p rotation_deg] + $i*360.0/$N) * 3.141592653589793/180.0}]
}

# position of vertex i (-1 = center)
proc traj::polygon::vertex { p i } {
    set cx [dict get $p center_x] ; set cy [dict get $p center_y]
    if { $i < 0 } { return [list $cx $cy] }
    set a [traj::polygon::_angle $p $i]
    set R [dict get $p radius]
    return [list [expr {$cx + $R*cos($a)}] [expr {$cy + $R*sin($a)}]]
}

# the segment index k with T_k <= t < T_{k+1} (K when t >= T_K, 0 when t < 0)
proc traj::polygon::_segment { p t } {
    set times [dict get $p times]
    set K [dict get $p n_steps]
    if { $t < 0.0 } { return 0 }
    for { set k 0 } { $k < $K } { incr k } {
        if { $t < [lindex $times [expr {$k+1}]] } { return $k }
    }
    return $K
}

# signed angular travel from vertex a to vertex b along the arc (the short
# way; for adjacent vertices exactly +/- 2pi/N)
proc traj::polygon::_arc_delta { p a b } {
    set N [dict get $p n_vertices]
    set d [expr {(($b - $a) % $N + $N) % $N}]
    if { $d > $N/2.0 } { set d [expr {$d - $N}] }
    return [expr {$d * 2.0*3.141592653589793/$N}]
}

proc traj::polygon::pos { p t } {
    set seq [dict get $p seq]
    set k [traj::polygon::_segment $p $t]
    set K [dict get $p n_steps]
    if { [dict get $p mode] eq "step" || $k >= $K || $t < 0.0 } {
        return [traj::polygon::vertex $p [lindex $seq $k]]
    }
    set times [dict get $p times]
    set t0 [lindex $times $k] ; set t1 [lindex $times [expr {$k+1}]]
    set u [expr {($t - $t0)/($t1 - $t0)}]
    set a [lindex $seq $k] ; set b [lindex $seq [expr {$k+1}]]
    if { [dict get $p path] eq "arc" } {
        set ang [expr {[traj::polygon::_angle $p $a] + $u*[traj::polygon::_arc_delta $p $a $b]}]
        set R [dict get $p radius]
        return [list [expr {[dict get $p center_x] + $R*cos($ang)}] \
                     [expr {[dict get $p center_y] + $R*sin($ang)}]]
    }
    lassign [traj::polygon::vertex $p $a] x0 y0
    lassign [traj::polygon::vertex $p $b] x1 y1
    return [list [expr {$x0 + $u*($x1-$x0)}] [expr {$y0 + $u*($y1-$y0)}]]
}

proc traj::polygon::vel { p t } {
    set k [traj::polygon::_segment $p $t]
    set K [dict get $p n_steps]
    if { [dict get $p mode] eq "step" || $k >= $K || $t < 0.0 } { return [list 0.0 0.0] }
    set seq [dict get $p seq]
    set times [dict get $p times]
    set dur [expr {[lindex $times [expr {$k+1}]] - [lindex $times $k]}]
    set a [lindex $seq $k] ; set b [lindex $seq [expr {$k+1}]]
    if { [dict get $p path] eq "arc" } {
        set u [expr {($t - [lindex $times $k])/$dur}]
        set dth [traj::polygon::_arc_delta $p $a $b]
        set ang [expr {[traj::polygon::_angle $p $a] + $u*$dth}]
        set w [expr {$dth/$dur}]
        set R [dict get $p radius]
        return [list [expr {-$R*$w*sin($ang)}] [expr {$R*$w*cos($ang)}]]
    }
    lassign [traj::polygon::vertex $p $a] x0 y0
    lassign [traj::polygon::vertex $p $b] x1 y1
    return [list [expr {($x1-$x0)/$dur}] [expr {($y1-$y0)/$dur}]]
}

proc traj::polygon::landmarks { p } {
    set times [dict get $p times]
    return [dict create start [list 0.0] \
                vertex [lrange $times 1 end] \
                end [list [dict get $p land_time]]]
}

proc traj::polygon::extent { p } { return [dict get $p maxext] }

traj::register polygon \
    -pos       traj::polygon::pos \
    -vel       traj::polygon::vel \
    -landmarks traj::polygon::landmarks \
    -extent    traj::polygon::extent
