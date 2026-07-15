# inverted_pendulum.tcl -- traj motion plugin: the INVERTED pendulum, the
# unstable anti-restoring twin of pendulum.tcl. A bob on a rigid rod ABOVE the
# pivot, near the (unstable) upright, falling over under gravity.
#
# DYNAMICS   theta'' = +(g/L) sin(theta)          theta = angle from straight UP
#   The sign flip vs the ordinary pendulum (theta''=-(g/L)sin) makes theta=0
#   (upright) a potential MAXIMUM: the bob accelerates AWAY from it. Released
#   near vertical it speeds up as it falls -- the exact opposite of a pendulum,
#   which DECELERATES toward its turning point. This is the harmonic-regime
#   analogue of the ballistic "valley": gravity-driven but gravity-INCONSISTENT
#   in its dynamics (no restoring, no anticipatable reversal).
#
# WHY SAMPLED, not closed form: the interesting motion sits near the separatrix
# (E ~ the unstable-equilibrium energy), where the elliptic-modulus k -> 1 and
# the closed form gets numerically delicate; and we want arbitrary launch state
# (an actively-toppled bob, not only release-from-rest). So make{} integrates
# theta''=+(g/L)sin with RK4 once and stores a fixed-dt (x,y,vx,vy) table; pos/
# vel are O(1) linear interpolation of it. traj is built for this (the sampled
# path is a first-class representation).
#
# GEOMETRY (bob ABOVE pivot):
#     x = pivot_x + L*sin(theta)
#     y = pivot_y + L*cos(theta)     (theta=0 => bob straight ABOVE the pivot)
#
# Dicts MUST come from traj::inverted_pendulum::make.

namespace eval traj::inverted_pendulum {}

# Constructor: integrate the fall and cache the sampled path.
#   theta_top  initial angle from vertical (rad; sign = which way it topples)
#   length L, g, duration; opts -pivot_x -pivot_y -theta_dot0 -int_dt
proc traj::inverted_pendulum::make { theta_top length g duration args } {
    set pivot_x 0.0 ; set pivot_y 0.0 ; set td0 0.0 ; set dt 0.002
    foreach { o v } $args {
        switch -- $o {
            -pivot_x   { set pivot_x $v }
            -pivot_y   { set pivot_y $v }
            -theta_dot0 { set td0 $v }
            -int_dt    { set dt $v }
            default    { error "traj::inverted_pendulum::make: unknown option '$o'" }
        }
    }
    if { $length <= 0.0 } { error "inverted_pendulum: length must be > 0" }
    if { $g <= 0.0 }      { error "inverted_pendulum: g must be > 0" }
    if { $dt <= 0.0 }     { error "inverted_pendulum: int_dt must be > 0" }
    set w2 [expr {$g/$length}]
    set L  $length

    # RK4 on state (theta, omega) with omega'=+w2 sin(theta); sample at fixed dt
    # from t=0 through >= duration (one guard sample past the end for interp).
    set th $theta_top ; set om $td0 ; set t 0.0
    set st {} ; set sx {} ; set sy {} ; set svx {} ; set svy {}
    set nsteps [expr {int(ceil($duration/$dt))+1}]
    for { set i 0 } { $i <= $nsteps } { incr i } {
        set x  [expr {$pivot_x + $L*sin($th)}]
        set y  [expr {$pivot_y + $L*cos($th)}]
        set vx [expr {$L*cos($th)*$om}]
        set vy [expr {-$L*sin($th)*$om}]
        lappend st $t ; lappend sx $x ; lappend sy $y ; lappend svx $vx ; lappend svy $vy
        # advance one RK4 step
        set k1t $om ;                     set k1o [expr {$w2*sin($th)}]
        set k2t [expr {$om+0.5*$dt*$k1o}]; set k2o [expr {$w2*sin($th+0.5*$dt*$k1t)}]
        set k3t [expr {$om+0.5*$dt*$k2o}]; set k3o [expr {$w2*sin($th+0.5*$dt*$k2t)}]
        set k4t [expr {$om+$dt*$k3o}] ;    set k4o [expr {$w2*sin($th+$dt*$k3t)}]
        set th [expr {$th + $dt/6.0*($k1t+2*$k2t+2*$k3t+$k4t)}]
        set om [expr {$om + $dt/6.0*($k1o+2*$k2o+2*$k3o+$k4o)}]
        set t  [expr {$t + $dt}]
    }
    set p [dict create \
        motion_type inverted_pendulum \
        pivot_x $pivot_x pivot_y $pivot_y length $L gravity $g \
        theta_top $theta_top theta_dot0 $td0 land_time $duration \
        sample_dt $dt nsamp [llength $st] \
        s_x $sx s_y $sy s_vx $svx s_vy $svy]
    lassign [traj::inverted_pendulum::pos $p $duration] lx ly
    dict set p land_x $lx ; dict set p land_y $ly
    # on-screen half-extent, from the ALREADY-SAMPLED path (a single pass over
    # sx/sy here is cheaper than traj::extent's generic fallback, which would
    # resample the whole path a second time via pos()).
    set mx 0.0
    foreach x $sx y $sy {
        set m [expr {max(abs($x), abs($y))}]
        if { $m > $mx } { set mx $m }
    }
    dict set p maxext $mx
    return $p
}

# fixed-dt linear interpolation of a stored per-sample array at time t
proc traj::inverted_pendulum::_interp { p arr t } {
    set dt [dict get $p sample_dt]
    set n  [dict get $p nsamp]
    set a  [dict get $p $arr]
    if { $t <= 0.0 }               { return [lindex $a 0] }
    set fi [expr {$t/$dt}]
    set i  [expr {int($fi)}]
    if { $i >= $n-1 } { return [lindex $a [expr {$n-1}]] }
    set f  [expr {$fi - $i}]
    set v0 [lindex $a $i] ; set v1 [lindex $a [expr {$i+1}]]
    return [expr {$v0 + $f*($v1-$v0)}]
}

proc traj::inverted_pendulum::pos { p t } {
    return [list [traj::inverted_pendulum::_interp $p s_x $t] \
                 [traj::inverted_pendulum::_interp $p s_y $t]]
}

proc traj::inverted_pendulum::vel { p t } {
    return [list [traj::inverted_pendulum::_interp $p s_vx $t] \
                 [traj::inverted_pendulum::_interp $p s_vy $t]]
}

# Landmarks by scanning the sampled speed: `top` = slowest sample (nearest the
# unstable upright, where prediction matters most), `bottom` = fastest sample
# (nearest straight-down, if the fall gets there). start/end always.
proc traj::inverted_pendulum::landmarks { p } {
    set dt [dict get $p sample_dt]
    set n  [dict get $p nsamp]
    set svx [dict get $p s_vx] ; set svy [dict get $p s_vy]
    set T  [dict get $p land_time]
    set imin 0 ; set imax 0 ; set smin 1e300 ; set smax -1.0
    for { set i 0 } { $i < $n } { incr i } {
        set sp [expr {hypot([lindex $svx $i],[lindex $svy $i])}]
        if { $sp < $smin } { set smin $sp ; set imin $i }
        if { $sp > $smax } { set smax $sp ; set imax $i }
    }
    set m [dict create start [list 0.0] end [list $T]]
    dict set m top    [list [expr {$imin*$dt}]]
    dict set m bottom [list [expr {$imax*$dt}]]
    return $m
}

proc traj::inverted_pendulum::extent { p } { return [dict get $p maxext] }

traj::register inverted_pendulum \
    -pos       traj::inverted_pendulum::pos \
    -vel       traj::inverted_pendulum::vel \
    -landmarks traj::inverted_pendulum::landmarks \
    -extent    traj::inverted_pendulum::extent
