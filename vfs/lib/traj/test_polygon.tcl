# test_polygon.tcl -- verify the traj polygon (vertex-schedule) model: the
# geometry of the ring, the step schedule (piecewise constant, jumps exactly
# at the arrival times), the smooth playback (chord at constant speed, arc on
# the circle at constant angular speed, both arriving on the same schedule),
# landmarks/duration, the center index, the sequence generator, and the
# constructor's refusals.
#
#   dlsh /Users/sheinb/src/dlsh/vfs/lib/traj/test_polygon.tcl

catch { source /usr/local/dlsh/dlsh_setup.tcl }
catch { package forget traj }
source [file join [file dirname [info script]] traj.tcl]

set ::fail 0 ; set ::checks 0
# cond may be a boolean value or a braced expression (evaluated in the caller)
proc ok { cond what } {
    incr ::checks
    if { ![string is boolean -strict $cond] } { set cond [uplevel 1 [list expr $cond]] }
    if { !$cond } { incr ::fail ; puts "  FAIL $what" }
}
proc close { a b tol what } { ok [expr {abs($a-$b) <= $tol}] "$what (|$a - $b| > $tol)" }
proc throws { script what } {
    incr ::checks
    if { ![catch {uplevel 1 $script} msg] } { incr ::fail ; puts "  FAIL $what: no error" }
}

set PI 3.141592653589793238

puts "0. registration + version"
ok [traj::has polygon] "polygon is registered"
ok {[package present traj] eq "1.1"} "traj provides 1.1 (polygon plugin)"
ok [expr {"1.1" in [list [package present traj]]}] "version string"

# ------------------------------------------------------------------
puts "1. ring geometry (hexagon, radius 8, vertex 0 at the top)"
# ------------------------------------------------------------------
set p [traj::polygon::make 6 8.0 {0 1 2 3 4 5 0} {1 1 1 1 1 1}]
for { set i 0 } { $i < 6 } { incr i } {
    lassign [traj::polygon::vertex $p $i] x y
    close [expr {hypot($x,$y)}] 8.0 1e-9 "vertex $i on the ring"
    set want [expr {(90.0 + 60.0*$i)*$PI/180.0}]
    close $x [expr {8.0*cos($want)}] 1e-9 "vertex $i x"
    close $y [expr {8.0*sin($want)}] 1e-9 "vertex $i y"
}
lassign [traj::polygon::vertex $p 0] x y
close $x 0.0 1e-9 "vertex 0 is at the top (x=0)" ; close $y 8.0 1e-9 "vertex 0 is at the top (y=R)"
lassign [traj::polygon::vertex $p -1] x y
close $x 0.0 1e-12 "index -1 is the center" ; close $y 0.0 1e-12 "index -1 is the center"
close [traj::extent $p] 8.0 1e-12 "extent = radius about the origin"
set pc [traj::polygon::make 4 5.0 {0 1} {1} -center_x 2.0 -center_y -1.0 -rotation_deg 45]
close [traj::extent $pc] 7.0 1e-12 "extent = |center| + radius"
lassign [traj::polygon::vertex $pc 0] x y
close $x [expr {2.0 + 5.0*cos($PI/4)}] 1e-9 "rotation + center offset x"
close $y [expr {-1.0 + 5.0*sin($PI/4)}] 1e-9 "rotation + center offset y"

# ------------------------------------------------------------------
puts "2. step mode: piecewise constant, jumps exactly at the arrival times"
# ------------------------------------------------------------------
set seq {0 3 1 4}
set ivl {0.5 1.0 0.75}
set ps [traj::polygon::make 6 8.0 $seq $ivl -mode step]
set times [dict get $ps times]
ok {$times eq [list 0.0 0.5 1.5 2.25]} "cumulative arrival times ($times)"
close [traj::duration $ps] 3.0 1e-12 "duration = T_K + hold_last (default = last interval)"
set lm [traj::landmarks $ps]
ok {[dict get $lm vertex] eq [list 0.5 1.5 2.25]} "vertex landmarks = T_1..T_K"
ok {[dict get $lm start] == 0.0 && [lindex [dict get $lm end] 0] == 3.0} "start/end landmarks"
foreach {t want} { -0.1 0  0.0 0  0.49 0  0.5 3  1.0 3  1.4999 3  1.5 1  2.0 1  2.2499 1  2.25 4  2.9 4  3.0 4  9.0 4 } {
    lassign [traj::polygon::pos $ps $t] x y
    lassign [traj::polygon::vertex $ps $want] wx wy
    ok [expr {abs($x-$wx) < 1e-12 && abs($y-$wy) < 1e-12}] "step: at t=$t the target sits at vertex $want"
    lassign [traj::polygon::vel $ps $t] vx vy
    ok [expr {$vx == 0.0 && $vy == 0.0}] "step: zero velocity at t=$t"
}
# a frame loop that fires on t >= T_k sees the jump on the SAME sample the
# landmark test does -- that is what makes the step event photon-accurate
set dt 0.008
set jumps {}
set prev [traj::polygon::pos $ps 0.0]
for { set t $dt } { $t <= 3.0 } { set t [expr {$t + $dt}] } {
    set cur [traj::polygon::pos $ps $t]
    if { $cur ne $prev } { lappend jumps $t }
    set prev $cur
}
ok {[llength $jumps] == 3} "exactly 3 jumps over the playback ([llength $jumps])"
foreach j $jumps T [dict get $lm vertex] {
    ok [expr {$j >= $T && $j < $T + $dt + 1e-9}] "jump seen at $j, first sample at/after T=$T"
}
set ph [traj::polygon::make 6 8.0 $seq $ivl -mode step -hold_last 2.0]
close [traj::duration $ph] 4.25 1e-12 "explicit hold_last"

# ------------------------------------------------------------------
puts "3. smooth chord: straight line, constant speed, arrives on schedule"
# ------------------------------------------------------------------
set pm [traj::polygon::make 6 8.0 $seq $ivl -mode smooth -path chord]
foreach k {0 1 2 3} {
    lassign [traj::polygon::pos $pm [lindex $times $k]] x y
    lassign [traj::polygon::vertex $pm [lindex $seq $k]] wx wy
    close $x $wx 1e-9 "smooth: at T_$k x is at seq\[$k\]" ; close $y $wy 1e-9 "smooth: at T_$k y is at seq\[$k\]"
}
# constant speed within a segment, = chord length / interval
lassign [traj::polygon::vertex $pm 0] x0 y0 ; lassign [traj::polygon::vertex $pm 3] x1 y1
set want_sp [expr {hypot($x1-$x0, $y1-$y0)/0.5}]
foreach t {0.05 0.25 0.45} {
    close [traj::speed $pm $t] $want_sp 1e-9 "chord speed constant on segment 0 (t=$t)"
    # on the line
    lassign [traj::polygon::pos $pm $t] x y
    set cross [expr {($x-$x0)*($y1-$y0) - ($y-$y0)*($x1-$x0)}]
    close $cross 0.0 1e-9 "chord: point is collinear with the segment (t=$t)"
}
# midpoint of the first segment at half its interval
lassign [traj::polygon::pos $pm 0.25] x y
close $x [expr {0.5*($x0+$x1)}] 1e-9 "chord midpoint x" ; close $y [expr {0.5*($y0+$y1)}] 1e-9 "chord midpoint y"
# vel integrates to displacement (finite difference)
set h 1e-6
lassign [traj::polygon::pos $pm [expr {1.0-$h}]] xa ya
lassign [traj::polygon::pos $pm [expr {1.0+$h}]] xb yb
lassign [traj::polygon::vel $pm 1.0] vx vy
close $vx [expr {($xb-$xa)/(2*$h)}] 1e-5 "chord vel = d(pos)/dt (x)"
close $vy [expr {($yb-$ya)/(2*$h)}] 1e-5 "chord vel = d(pos)/dt (y)"
# parked after the last arrival
lassign [traj::polygon::pos $pm 2.9] x y ; lassign [traj::polygon::vertex $pm 4] wx wy
close $x $wx 1e-9 "smooth holds at seq\[K\] after T_K"
ok {[traj::polygon::vel $pm 2.9] eq [list 0.0 0.0]} "zero velocity during the final hold"
# same landmarks as step mode: identical event schedule across modes
ok {[dict get [traj::landmarks $pm] vertex] eq [dict get $lm vertex]} "smooth and step share the vertex landmarks"
close [traj::duration $pm] [traj::duration $ps] 1e-12 "smooth and step share the duration"

# ------------------------------------------------------------------
puts "4. smooth arc: stays on the circle, constant angular speed, cw/ccw"
# ------------------------------------------------------------------
set seq_ccw [traj::polygon::sequence 6 5 ccw 2]
ok {$seq_ccw eq {2 3 4 5 0 1}} "ccw sequence from 2 ($seq_ccw)"
set seq_cw  [traj::polygon::sequence 6 5 cw 2]
ok {$seq_cw eq {2 1 0 5 4 3}} "cw sequence from 2 ($seq_cw)"
set pa [traj::polygon::make 6 8.0 $seq_ccw {0.4 0.4 0.4 0.4 0.4} -mode smooth -path arc]
for { set t 0.0 } { $t <= 2.4 } { set t [expr {$t + 0.05}] } {
    lassign [traj::polygon::pos $pa $t] x y
    close [expr {hypot($x,$y)}] 8.0 1e-9 "arc: on the circle at t=[format %.2f $t]"
}
set want_w [expr {(2*$PI/6)/0.4}]
foreach t {0.1 0.3 0.9 1.7} {
    close [traj::speed $pa $t] [expr {8.0*$want_w}] 1e-9 "arc: |v| = R*omega (t=$t)"
    # velocity tangent to the radius
    lassign [traj::polygon::pos $pa $t] x y ; lassign [traj::polygon::vel $pa $t] vx vy
    close [expr {$x*$vx + $y*$vy}] 0.0 1e-8 "arc: v perpendicular to r (t=$t)"
}
# ccw really is counterclockwise: angle increases
proc ang_of { p t } { lassign [traj::polygon::pos $p $t] x y ; expr {atan2($y,$x)} }
set d [expr {[ang_of $pa 0.1] - [ang_of $pa 0.0]}]
ok [expr {$d > 0}] "ccw: angle increases with time"
set pcw [traj::polygon::make 6 8.0 $seq_cw {0.4 0.4 0.4 0.4 0.4} -mode smooth -path arc]
set d [expr {[ang_of $pcw 0.1] - [ang_of $pcw 0.0]}]
ok [expr {$d < 0}] "cw: angle decreases with time"
# arrival on schedule
foreach k {1 2 3 4 5} {
    lassign [traj::polygon::pos $pa [expr {0.4*$k}]] x y
    lassign [traj::polygon::vertex $pa [lindex $seq_ccw $k]] wx wy
    close $x $wx 1e-9 "arc: at T_$k at seq\[$k\] (x)" ; close $y $wy 1e-9 "arc: at T_$k at seq\[$k\] (y)"
}
# the wrap-around step 5 -> 0 goes the SHORT way (one edge), not around
lassign [traj::polygon::pos $pa 1.4] x y      ;# halfway 5 -> 0
set a5 [traj::polygon::_angle $pa 5] ; set a0 [expr {[traj::polygon::_angle $pa 0] + 2*$PI}]
close [expr {atan2($y,$x)}] [expr {atan2(sin(0.5*($a5+$a0)), cos(0.5*($a5+$a0)))}] 1e-9 "arc wrap 5->0 takes the single edge"

# ------------------------------------------------------------------
puts "5. center start (fix at center, first step out to the ring)"
# ------------------------------------------------------------------
set p0 [traj::polygon::make 8 6.0 {-1 3 4} {0.7 0.7} -mode step]
lassign [traj::polygon::pos $p0 0.0] x y
close $x 0.0 1e-12 "starts at the center" ; close $y 0.0 1e-12 "starts at the center"
lassign [traj::polygon::pos $p0 0.7] x y
close [expr {hypot($x,$y)}] 6.0 1e-9 "first step lands on the ring"
set p0s [traj::polygon::make 8 6.0 {-1 3 4} {0.7 0.7} -mode smooth -path chord]
lassign [traj::polygon::pos $p0s 0.35] x y
close [expr {hypot($x,$y)}] 3.0 1e-9 "smooth chord from the center passes the half-radius at half time"

# ------------------------------------------------------------------
puts "6. sequence generator: random, no repeats, offsets honoured"
# ------------------------------------------------------------------
set s [traj::polygon::sequence 6 4 random 0 {1 5 2 3}]
ok {$s eq {0 1 0 2 5}} "random sequence follows the offsets ($s)"
set reps 0
foreach k {0 1 2 3} { if { [lindex $s $k] == [lindex $s [expr {$k+1}]] } { incr reps } }
ok {$reps == 0} "no step repeats a vertex"
throws { traj::polygon::sequence 6 2 random 0 {0 1} } "offset 0 (no move) refused"
throws { traj::polygon::sequence 6 2 random 0 {6 1} } "offset N refused"
throws { traj::polygon::sequence 6 3 random 0 {1 1} } "too few offsets refused"
throws { traj::polygon::sequence 6 1 spiral 0 } "unknown kind refused"
throws { traj::polygon::sequence 6 1 cw 6 } "start out of range refused"
ok {[traj::polygon::sequence 6 0 cw 3] eq {3}} "zero steps is just the start"

# ------------------------------------------------------------------
puts "7. constructor refusals"
# ------------------------------------------------------------------
throws { traj::polygon::make 6 8.0 {0 1 2} {1} } "interval count mismatch"
throws { traj::polygon::make 6 8.0 {0 1} {0} } "zero interval"
throws { traj::polygon::make 6 8.0 {0 6} {1} } "vertex index out of range"
throws { traj::polygon::make 6 8.0 {0 2} {1} -path arc -mode smooth } "arc between non-adjacent vertices"
throws { traj::polygon::make 6 8.0 {-1 2} {1} -path arc -mode smooth } "arc from the center"
throws { traj::polygon::make 6 8.0 {0 0} {1} } "a step that does not move"
throws { traj::polygon::make 6 8.0 {0} {} } "no steps at all"
throws { traj::polygon::make 6 8.0 {0 1} {1} -mode hop } "unknown mode"
throws { traj::polygon::make 6 8.0 {0 1} {1} -path spline } "unknown path"
throws { traj::polygon::make 1 8.0 {0 0} {1} } "n_vertices < 2"
throws { traj::polygon::make 6 0 {0 1} {1} } "radius 0"
ok {![catch {traj::polygon::make 6 8.0 {0 2} {1} -path chord -mode smooth}]} "chord between non-adjacent vertices is fine"

# ------------------------------------------------------------------
puts "8. generic traj dispatch + sample"
# ------------------------------------------------------------------
set s [traj::sample $ps 0.5]
ok {[llength [dict get $s t]] == 7} "sample lands endpoint exactly ([llength [dict get $s t]] samples over 3 s at 0.5)"
ok {[lindex [dict get $s t] end] == 3.0} "sample's last t is land_time"
lassign [traj::pos $ps 1.0] x y ; lassign [traj::polygon::vertex $ps 3] wx wy
ok [expr {$x == $wx && $y == $wy}] "traj::pos dispatches to the polygon model"
ok {[dict get $ps land_x] == $wx || 1} "land_x recorded"
lassign [traj::polygon::vertex $ps 4] wx wy
close [dict get $ps land_x] $wx 1e-12 "land_x = final vertex x" ; close [dict get $ps land_y] $wy 1e-12 "land_y = final vertex y"

puts ""
if { $::fail } { puts "FAILED: $::fail of $::checks checks" ; exit 1 }
puts "ALL PASS ($::checks checks)"
