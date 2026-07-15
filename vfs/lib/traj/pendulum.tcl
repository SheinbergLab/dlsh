# pendulum.tcl -- traj motion plugin: the EXACT (large-amplitude) simple
# pendulum. A point bob on a massless rod of length L swinging under gravity g
# about a pivot. NOT the small-angle approximation: the whole point of a
# pendulum stimulus is the gravity-driven timing (slow at the turning points,
# fast through the bottom) and the vertical bob excursion, and both only become
# pronounced at large amplitude, where SHM is wrong. So we use the exact closed
# form in Jacobi elliptic functions.
#
# DYNAMICS   theta'' = -(g/L) sin(theta)          theta = angle from straight-down
# SOLUTION   released from rest at +theta0:
#              theta(t) = 2*asin( k * sn(K(k) - w0*t, k) )
#            with modulus k = sin(theta0/2), w0 = sqrt(g/L), K = quarter period
#            in elliptic-argument units. Full period = 4K/w0.
#            theta'(t) = -2*k*w0*cn(K-w0*t, k)      (exact; = 0 at the turns)
#   phase_frac f in [0,1) shifts the start by f of a full period:
#     f=0    -> starts at the +extreme, at rest (a natural "release")
#     f=0.25 -> starts at the bottom, moving (fastest point)
#
# GEOMETRY (hanging pendulum, pivot above the bob):
#     x = pivot_x + L*sin(theta)
#     y = pivot_y - L*cos(theta)     (theta=0 => bob straight below pivot)
#
# Pendulum dicts MUST come from traj::pendulum::make -- it caches w0, k and the
# quarter period K so pos/vel are O(1) per frame (no AGM in the hot path). This
# differs from ballistic (whose pos works off raw scalars); a model is allowed
# to require a constructor.
#
# The INVERTED pendulum (unstable, theta'' = +(g/L) sin theta) is a separate
# plugin; it shares no elliptic machinery (its near-top motion is hyperbolic),
# so the helpers below stay local to this file for now.

namespace eval traj::pendulum {}

# ------------------------------------------------------------------
# Complete elliptic integral K(k) via the arithmetic-geometric mean.
#   K(k) = pi / (2 * AGM(1, sqrt(1-k^2)))
# ------------------------------------------------------------------
proc traj::pendulum::ellipK { k } {
    set a 1.0
    set b [expr {sqrt(1.0 - $k*$k)}]
    for { set i 0 } { $i < 60 } { incr i } {
        if { abs($a-$b) < 1e-16 } break
        set a2 [expr {0.5*($a+$b)}]
        set b  [expr {sqrt($a*$b)}]
        set a  $a2
    }
    return [expr {3.141592653589793238 / (2.0*$a)}]
}

# ------------------------------------------------------------------
# Jacobi elliptic sn, cn, dn at argument u, modulus k. Descending-Landen / AGM
# method (Abramowitz & Stegun 16.4). Returns {sn cn dn}.
# ------------------------------------------------------------------
proc traj::pendulum::ellipj { u k } {
    set m [expr {$k*$k}]
    if { $m < 1e-18 } { return [list [expr {sin($u)}] [expr {cos($u)}] 1.0] }
    set a 1.0
    set b [expr {sqrt(1.0 - $m)}]
    set c $k
    set av [list $a] ; set cv [list $c]
    set n 0
    while { abs($c) > 1e-15 && $n < 40 } {
        set a2 [expr {0.5*($a+$b)}]
        set c  [expr {0.5*($a-$b)}]
        set b  [expr {sqrt($a*$b)}]
        set a  $a2
        incr n
        lappend av $a ; lappend cv $c
    }
    set phi [expr {pow(2.0,$n)*$a*$u}]
    for { set i $n } { $i > 0 } { incr i -1 } {
        set ci [lindex $cv $i] ; set ai [lindex $av $i]
        set phi [expr {0.5*($phi + asin(($ci/$ai)*sin($phi)))}]
    }
    set sn [expr {sin($phi)}]
    set cn [expr {cos($phi)}]
    set dn [expr {sqrt(1.0 - $m*$sn*$sn)}]
    return [list $sn $cn $dn]
}

# ------------------------------------------------------------------
# Constructor: build a ready-to-run pendulum trajectory dict.
#   theta0   amplitude (rad, 0<theta0<pi)      length   rod length L (dva)
#   g        gravity (>0)                        duration playback length (s)
#   opts: -pivot_x -pivot_y -phase_frac
# Caches omega0/kmod/quarterK so pos/vel need no AGM per call.
# ------------------------------------------------------------------
proc traj::pendulum::make { theta0 length g duration args } {
    set pivot_x 0.0 ; set pivot_y 0.0 ; set phase_frac 0.0 ; set invert 0
    foreach { o v } $args {
        switch -- $o {
            -pivot_x    { set pivot_x $v }
            -pivot_y    { set pivot_y $v }
            -phase_frac { set phase_frac $v }
            -invert     { set invert $v }
            default     { error "traj::pendulum::make: unknown option '$o'" }
        }
    }
    if { $length <= 0.0 } { error "pendulum: length must be > 0" }
    if { $g <= 0.0 }      { error "pendulum: g must be > 0" }
    if { $theta0 <= 0.0 || $theta0 >= 3.141592653589793 } {
        error "pendulum: theta0 must be in (0, pi)"
    }
    # invert = the GRAVITY-INVERTED twin: identical theta(t) swing, but the bob
    # hangs ABOVE the pivot (stable about the TOP under upward gravity). It is
    # the exact vertical MIRROR of the hanging pendulum -- same amplitude,
    # period and speed profile -- so (pendulum, invert-pendulum) is a matched
    # gravity-consistent/inconsistent pair, exactly like (hill, valley). This is
    # NOT the unstable inverted_pendulum plugin (which topples).
    set k  [expr {sin($theta0/2.0)}]
    set w0 [expr {sqrt($g/$length)}]
    set qK [traj::pendulum::ellipK $k]
    set p [dict create \
        motion_type pendulum \
        pivot_x $pivot_x pivot_y $pivot_y length $length invert $invert \
        theta0 $theta0 gravity $g phase_frac $phase_frac \
        land_time $duration omega0 $w0 kmod $k quarterK $qK]
    # endpoint + on-screen extent (from the geometry, exact)
    lassign [traj::pendulum::pos $p $duration] lx ly
    dict set p land_x $lx ; dict set p land_y $ly
    set xext [expr {abs($pivot_x) + $length*sin($theta0)}]
    set ynear [expr {$pivot_y + ($invert ? $length : -$length)}]           ;# straight up/down
    set yturn [expr {$pivot_y + ($invert ? 1 : -1)*$length*cos($theta0)}]   ;# at the turns
    dict set p maxext [expr {max($xext, abs($ynear), abs($yturn))}]
    return $p
}

# angle + angular velocity at time t (the shared core of pos/vel)
proc traj::pendulum::_state { p t } {
    set qK [dict get $p quarterK]
    set w0 [dict get $p omega0]
    set k  [dict get $p kmod]
    set f  [dict get $p phase_frac]
    set u  [expr {$qK - $w0*$t - 4.0*$qK*$f}]
    lassign [traj::pendulum::ellipj $u $k] sn cn dn
    set ksn [expr {$k*$sn}]
    if { $ksn > 1.0 }  { set ksn 1.0 }
    if { $ksn < -1.0 } { set ksn -1.0 }
    set th  [expr {2.0*asin($ksn)}]
    set thd [expr {-2.0*$k*$w0*$cn}]        ;# exact: theta'(t) = -2 k w0 cn(u)
    return [list $th $thd]
}

proc traj::pendulum::pos { p t } {
    lassign [traj::pendulum::_state $p $t] th thd
    set L [dict get $p length]
    set s [expr {[dict exists $p invert] && [dict get $p invert] ? 1.0 : -1.0}]
    return [list [expr {[dict get $p pivot_x] + $L*sin($th)}] \
                 [expr {[dict get $p pivot_y] + $s*$L*cos($th)}]]
}

proc traj::pendulum::vel { p t } {
    lassign [traj::pendulum::_state $p $t] th thd
    set L [dict get $p length]
    set s [expr {[dict exists $p invert] && [dict get $p invert] ? 1.0 : -1.0}]
    return [list [expr {$L*cos($th)*$thd}] [expr {-$s*$L*sin($th)*$thd}]]
}

# Landmarks: turning points (theta'=0 => cn=0 => u=(2j+1)K) and bottom
# crossings (theta=0 => sn=0 => u=2jK), both enumerated over [0, land_time].
# These are what a coherence dip should anchor to (predict the reversal).
proc traj::pendulum::landmarks { p } {
    set qK [dict get $p quarterK]
    set w0 [dict get $p omega0]
    set f  [dict get $p phase_frac]
    set T  [dict get $p land_time]
    set eps [expr {1e-9*$T + 1e-12}]
    set turns {} ; set bottoms {}
    # turn: t = -2K(2f+j)/w0 ; bottom: t = K(1-4f-2j)/w0  -- scan j wide enough
    set half [expr {2.0*$qK/$w0}]            ;# half period (turn spacing)
    set jmax [expr {int(ceil($T/$half))+2}]
    for { set j [expr {-$jmax}] } { $j <= $jmax } { incr j } {
        set tt [expr {-2.0*$qK*(2.0*$f+$j)/$w0}]
        if { $tt >= -$eps && $tt <= $T+$eps } { lappend turns [expr {$tt<0?0.0:$tt}] }
        set tb [expr {$qK*(1.0-4.0*$f-2.0*$j)/$w0}]
        if { $tb >= -$eps && $tb <= $T+$eps } { lappend bottoms [expr {$tb<0?0.0:$tb}] }
    }
    set m [dict create start [list 0.0] end [list $T]]
    if { [llength $turns] }   { dict set m turn   [lsort -real -unique $turns] }
    if { [llength $bottoms] } { dict set m bottom [lsort -real -unique $bottoms] }
    return $m
}

traj::register pendulum \
    -pos       traj::pendulum::pos \
    -vel       traj::pendulum::vel \
    -landmarks traj::pendulum::landmarks
