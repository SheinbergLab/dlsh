# test_pendulum.tcl -- verify the exact-elliptic pendulum against INDEPENDENT
# references: a straight RK4 integration of theta''=-(g/L)sin(theta) (which owes
# nothing to elliptic functions), the physical energy invariant, the rod-length
# geometry, and known Jacobi/elliptic special values.
#
#   dlsh /Users/sheinb/src/dlsh/vfs/lib/traj/test_pendulum.tcl

catch { source /usr/local/dlsh/dlsh_setup.tcl }
source [file join [file dirname [info script]] traj.tcl]

set ::fail 0 ; set ::checks 0
proc ok { cond what } {
    incr ::checks
    if { !$cond } { incr ::fail ; puts "  FAIL $what" }
}
proc close { a b tol what } { ok [expr {abs($a-$b) <= $tol}] "$what (|$a - $b| > $tol)" }

set PI 3.141592653589793238

# ------------------------------------------------------------------
# 0. elliptic special values
# ------------------------------------------------------------------
puts "0. elliptic special values"
close [traj::pendulum::ellipK 0.0] [expr {$PI/2.0}] 1e-12 "K(0)=pi/2"
lassign [traj::pendulum::ellipj 0.0 0.6] sn cn dn
close $sn 0.0 1e-12 "sn(0)=0" ; close $cn 1.0 1e-12 "cn(0)=1" ; close $dn 1.0 1e-12 "dn(0)=1"
set K [traj::pendulum::ellipK 0.6]
lassign [traj::pendulum::ellipj $K 0.6] sn cn dn
close $sn 1.0 1e-9 "sn(K)=1" ; close $cn 0.0 1e-9 "cn(K)=0"
# k->0 : sn/cn/dn -> sin/cos/1
lassign [traj::pendulum::ellipj 0.7 1e-6] sn cn dn
close $sn [expr {sin(0.7)}] 1e-6 "sn(u,0)=sin u" ; close $cn [expr {cos(0.7)}] 1e-6 "cn(u,0)=cos u"

# ------------------------------------------------------------------
# 1. geometry + release conditions, across amplitudes incl. LARGE
# ------------------------------------------------------------------
puts "1. geometry (bob on the rod) + release-from-rest conditions"
foreach th0 { 0.2 0.8 1.5 2.5 } {
    foreach L { 4.0 8.0 } {
        set p [traj::pendulum::make $th0 $L 9.8 3.0 -pivot_x 1.0 -pivot_y 6.0]
        # bob always at distance L from the pivot
        for { set i 0 } { $i <= 40 } { incr i } {
            set t [expr {3.0*$i/40.0}]
            lassign [traj::pendulum::pos $p $t] x y
            set r [expr {hypot($x-1.0, $y-6.0)}]
            ok [expr {abs($r-$L) < 1e-9}] "bob on rod |r-L| (th0=$th0 L=$L t=$t)"
        }
        # released at +theta0, at rest
        lassign [traj::pendulum::pos $p 0.0] x0 y0
        set th_start [expr {atan2($x0-1.0, -($y0-6.0))}]   ;# angle from straight down
        close $th_start $th0 1e-7 "theta(0)=theta0 (th0=$th0 L=$L)"
        lassign [traj::pendulum::vel $p 0.0] vx0 vy0
        close [expr {hypot($vx0,$vy0)}] 0.0 1e-6 "released at rest (th0=$th0 L=$L)"
    }
}

# ------------------------------------------------------------------
# 2. THE key check: exact-elliptic theta(t) == RK4 integration of the ODE
#    (large amplitude, where SHM would be visibly wrong)
# ------------------------------------------------------------------
puts "2. exact solution vs independent RK4 integration (theta''=-(g/L)sin th)"
proc angle_of { p t } {  ;# recover theta(t) from the bob geometry
    lassign [traj::pendulum::pos $p $t] x y
    set px [dict get $p pivot_x] ; set py [dict get $p pivot_y]
    return [expr {atan2($x-$px, -($y-$py))}]
}
foreach th0 { 0.5 1.5 2.5 } {
    set L 6.0 ; set g 9.8
    set p [traj::pendulum::make $th0 $L $g 4.0]
    set w2 [expr {$g/$L}]
    # RK4 from (theta0, 0)
    set th $th0 ; set om 0.0 ; set h 0.00005 ; set t 0.0
    set worst 0.0
    set nextcheck 0.1
    while { $t < 3.5 } {
        # rk4 step for th'=om, om'=-w2 sin th
        set k1t $om ;                    set k1o [expr {-$w2*sin($th)}]
        set k2t [expr {$om+0.5*$h*$k1o}]; set k2o [expr {-$w2*sin($th+0.5*$h*$k1t)}]
        set k3t [expr {$om+0.5*$h*$k2o}]; set k3o [expr {-$w2*sin($th+0.5*$h*$k2t)}]
        set k4t [expr {$om+$h*$k3o}] ;    set k4o [expr {-$w2*sin($th+$h*$k3t)}]
        set th [expr {$th + $h/6.0*($k1t+2*$k2t+2*$k3t+$k4t)}]
        set om [expr {$om + $h/6.0*($k1o+2*$k2o+2*$k3o+$k4o)}]
        set t  [expr {$t + $h}]
        if { $t >= $nextcheck } {
            set d [expr {abs([angle_of $p $t] - $th)}]
            if { $d > $worst } { set worst $d }
            set nextcheck [expr {$nextcheck + 0.1}]
        }
    }
    ok [expr {$worst < 5e-4}] "elliptic==RK4 over 3.5s, th0=$th0 (worst dtheta=[format %.2e $worst] rad)"
}

# ------------------------------------------------------------------
# 3. energy conservation: 0.5*thetadot^2 - w0^2 cos(theta) == const
# ------------------------------------------------------------------
puts "3. energy conservation along the exact trajectory"
foreach th0 { 0.8 2.0 } {
    set L 5.0 ; set g 9.8 ; set w2 [expr {$g/$L}]
    set p [traj::pendulum::make $th0 $L $g 4.0 -phase_frac 0.15]
    set E0 [expr {-$w2*cos($th0)}]        ;# at rest at theta0
    set worst 0.0
    for { set i 0 } { $i <= 200 } { incr i } {
        set t [expr {4.0*$i/200.0}]
        set th [angle_of $p $t]
        lassign [traj::pendulum::vel $p $t] vx vy
        set thd [expr {hypot($vx,$vy)/$L}]     ;# |v| = L|thetadot|
        set E [expr {0.5*$thd*$thd - $w2*cos($th)}]
        set d [expr {abs($E-$E0)}]
        if { $d > $worst } { set worst $d }
    }
    ok [expr {$worst < 1e-6}] "energy conserved, th0=$th0 (worst dE=[format %.2e $worst])"
}

# ------------------------------------------------------------------
# 4. landmarks: turns have min speed (~0), bottoms have max speed; period 4K/w0
# ------------------------------------------------------------------
puts "4. landmarks -- turning points slow, bottom crossings fast"
set p [traj::pendulum::make 1.2 6.0 9.8 4.0]
set m [traj::pendulum::landmarks $p]
ok [dict exists $m turn]   "reports turn landmarks"
ok [dict exists $m bottom] "reports bottom landmarks"
proc speed { p t } { lassign [traj::pendulum::vel $p $t] vx vy ; return [expr {hypot($vx,$vy)}] }
foreach tt [dict get $m turn] {
    ok [expr {[speed $p $tt] < 0.05}] "turn @ [format %.3f $tt]s is a rest point (speed [format %.3f [speed $p $tt]])"
}
# a bottom crossing should be much faster than a turn
set vbot [speed $p [lindex [dict get $m bottom] 0]]
ok [expr {$vbot > 1.0}] "bottom crossing is fast (speed [format %.2f $vbot])"
# full-period spacing between consecutive turns == 2K/w0 (half period)
set turns [dict get $m turn]
if { [llength $turns] >= 2 } {
    set gap [expr {[lindex $turns 1]-[lindex $turns 0]}]
    set halfP [expr {2.0*[dict get $p quarterK]/[dict get $p omega0]}]
    close $gap $halfP 1e-6 "turn spacing == half period"
}

# ------------------------------------------------------------------
# 5. registered + reachable through the generic dispatch
# ------------------------------------------------------------------
puts "5. dispatch: traj::pos/vel/landmarks route to the pendulum model"
ok [expr {[traj::has pendulum]}] "pendulum registered"
set p [traj::pendulum::make 1.0 6.0 9.8 2.0]
lassign [traj::pos $p 0.37] gx gy
lassign [traj::pendulum::pos $p 0.37] dx dy
ok [expr {$gx==$dx && $gy==$dy}] "traj::pos dispatches to pendulum"
close [traj::duration $p] 2.0 1e-12 "traj::duration == land_time"

puts ""
if { $::fail } { puts "FAILED: $::fail / $::checks" ; exit 1 }
puts "PASS: all $::checks checks (exact pendulum == RK4, energy-conserving, geometry + landmarks correct)"
