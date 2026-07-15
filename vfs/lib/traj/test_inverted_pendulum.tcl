# test_inverted_pendulum.tcl -- verify the sampled inverted pendulum against
# independent invariants: the equation of motion theta''=+(g/L)sin(theta), the
# energy 0.5*thetadot^2 + w0^2 cos(theta) = const, rod-length geometry, the
# qualitative "accelerates away from the top" signature, and interp round-trip.
#
#   dlsh /Users/sheinb/src/dlsh/vfs/lib/traj/test_inverted_pendulum.tcl

catch { source /usr/local/dlsh/dlsh_setup.tcl }
source [file join [file dirname [info script]] traj.tcl]

set ::fail 0 ; set ::checks 0
proc ok { cond what } { incr ::checks ; if { !$cond } { incr ::fail ; puts "  FAIL $what" } }
proc close { a b tol what } { ok [expr {abs($a-$b) <= $tol}] "$what (|$a-$b|=[format %.2e [expr {abs($a-$b)}]] > $tol)" }

# angle from straight UP, recovered from the bob geometry
proc up_angle { p t } {
    lassign [traj::inverted_pendulum::pos $p $t] x y
    return [expr {atan2($x-[dict get $p pivot_x], $y-[dict get $p pivot_y])}]
}
proc speed { p t } { lassign [traj::inverted_pendulum::vel $p $t] vx vy ; return [expr {hypot($vx,$vy)}] }

# ------------------------------------------------------------------
# 1. geometry: bob always at distance L from the pivot, ABOVE it at t=0
# ------------------------------------------------------------------
puts "1. rod-length geometry + starts above the pivot near vertical"
foreach th0 { 0.1 0.4 0.9 } {
    foreach L { 5.0 9.0 } {
        set p [traj::inverted_pendulum::make $th0 $L 9.8 1.6 -pivot_x 2.0 -pivot_y -3.0]
        for { set i 0 } { $i <= 40 } { incr i } {
            set t [expr {1.6*$i/40.0}]
            lassign [traj::inverted_pendulum::pos $p $t] x y
            ok [expr {abs(hypot($x-2.0,$y+3.0)-$L) < 1e-9}] "bob on rod (th0=$th0 L=$L t=$t)"
        }
        close [up_angle $p 0.0] $th0 1e-6 "starts at theta_top (th0=$th0 L=$L)"
    }
}

# ------------------------------------------------------------------
# 2. equation of motion holds along the path: theta'' == +(g/L) sin(theta).
#    Second-difference the STORED RK4 samples (NOT the interpolated angle) with
#    step == sample_dt -- interpolation would inject piecewise-linear kinks, and
#    a step below the sample spacing would measure those, not the dynamics.
# ------------------------------------------------------------------
puts "2. stored path obeys theta'' = +(g/L) sin(theta)"
foreach th0 { 0.2 0.6 } {
    set L 6.0 ; set g 9.8 ; set w2 [expr {$g/$L}]
    set p [traj::inverted_pendulum::make $th0 $L $g 1.4]
    set dt [dict get $p sample_dt] ; set n [dict get $p nsamp]
    set sx [dict get $p s_x] ; set sy [dict get $p s_y]
    set px [dict get $p pivot_x] ; set py [dict get $p pivot_y]
    # theta from up at each stored sample
    set th {}
    for { set i 0 } { $i < $n } { incr i } {
        lappend th [expr {atan2([lindex $sx $i]-$px, [lindex $sy $i]-$py)}]
    }
    set worst 0.0
    for { set i 1 } { $i < $n-1 } { incr i } {
        set acc [expr {([lindex $th [expr {$i+1}]]-2*[lindex $th $i]+[lindex $th [expr {$i-1}]])/($dt*$dt)}]
        set rhs [expr {$w2*sin([lindex $th $i])}]
        set d [expr {abs($acc-$rhs)}]
        if { $d > $worst } { set worst $d }
    }
    ok [expr {$worst < 1e-3}] "theta''==+(g/L)sin at stored samples, th0=$th0 (worst [format %.2e $worst])"
}

# ------------------------------------------------------------------
# 3. energy conservation: 0.5*thetadot^2 + w0^2 cos(theta) == const
# ------------------------------------------------------------------
puts "3. energy conserved along the fall"
foreach th0 { 0.15 0.5 } {
    set L 7.0 ; set g 9.8 ; set w2 [expr {$g/$L}]
    set p [traj::inverted_pendulum::make $th0 $L $g 1.6]
    set E0 [expr {$w2*cos($th0)}]                     ;# released ~at rest at theta_top
    set worst 0.0
    for { set i 0 } { $i <= 160 } { incr i } {
        set t [expr {1.6*$i/160.0}]
        set th [up_angle $p $t]
        set thd [expr {[speed $p $t]/$L}]
        set E [expr {0.5*$thd*$thd + $w2*cos($th)}]
        set d [expr {abs($E-$E0)}]
        if { $d > $worst } { set worst $d }
    }
    ok [expr {$worst < 1e-4}] "energy conserved, th0=$th0 (worst [format %.2e $worst])"
}

# ------------------------------------------------------------------
# 4. THE signature: accelerates AWAY from the top (speed rises monotonically,
#    |angle| grows) -- the opposite of a pendulum decelerating to a turn
# ------------------------------------------------------------------
puts "4. accelerates away from the upright (speeds up, tips further)"
set p [traj::inverted_pendulum::make 0.2 6.0 9.8 1.4]
set prev_sp -1.0 ; set prev_ang 0.0 ; set mono 1 ; set away 1
for { set i 1 } { $i <= 60 } { incr i } {
    set t [expr {1.4*$i/60.0}]
    set sp [speed $p $t] ; set ang [expr {abs([up_angle $p $t])}]
    if { $sp < $prev_sp - 1e-6 } { set mono 0 }
    if { $ang < $prev_ang - 1e-6 } { set away 0 }
    set prev_sp $sp ; set prev_ang $ang
}
ok $mono "speed increases monotonically during the fall"
ok $away "bob tips monotonically away from vertical"
ok [expr {[speed $p 0.02] < 0.5}]  "starts slow near the top"
ok [expr {[speed $p 1.35] > 3.0}]  "fast later in the fall"
# landmark 'top' is near t=0 (slowest), 'bottom' after it
set m [traj::inverted_pendulum::landmarks $p]
ok [expr {[lindex [dict get $m top] 0] < 0.1}]   "top landmark near t=0 (slowest, nearest upright)"
ok [expr {[lindex [dict get $m bottom] 0] > [lindex [dict get $m top] 0]}] "bottom landmark after top"

# ------------------------------------------------------------------
# 5. interp round-trip + generic dispatch
# ------------------------------------------------------------------
puts "5. interp at a sample time == the stored sample; generic dispatch"
set p [traj::inverted_pendulum::make 0.3 6.0 9.8 1.0 -int_dt 0.002]
set dt [dict get $p sample_dt]
set ti [expr {10*$dt}]                              ;# exactly a sample time
lassign [traj::inverted_pendulum::pos $p $ti] x y
close $x [lindex [dict get $p s_x] 10] 1e-12 "pos at sample time == stored x"
close $y [lindex [dict get $p s_y] 10] 1e-12 "pos at sample time == stored y"
ok [expr {[traj::has inverted_pendulum]}] "inverted_pendulum registered"
lassign [traj::pos $p 0.4] gx gy
lassign [traj::inverted_pendulum::pos $p 0.4] dx dy
ok [expr {$gx==$dx && $gy==$dy}] "traj::pos dispatches to inverted_pendulum"

puts ""
if { $::fail } { puts "FAILED: $::fail / $::checks" ; exit 1 }
puts "PASS: all $::checks checks (obeys theta''=+(g/L)sin, energy-conserving, accelerates off the top)"
