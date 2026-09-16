# test_b2world.tcl -- headless checks for the b2world engine.
#
#   dlsh test_b2world.tcl
#
# Sources the ON-DISK b2world.tcl so it tests what was just edited.

if {![info exists ::__b2world_test_loaded]} {
    catch { source /usr/local/dlsh/dlsh_setup.tcl }
    package require dlsh
    catch {package forget b2world}
    catch {namespace delete ::b2world}
    source [file join [file dirname [info script]] b2world.tcl]
    set ::__b2world_test_loaded 1
}

set ::nfail 0
proc check { label cond } {
    set ok [uplevel 1 [list expr $cond]]
    if { $ok } { puts "  ok   $label" } else { puts "  FAIL $label"; incr ::nfail }
}
proc approx { label got want { tol 0.05 } } {
    if { abs($got - $want) <= $tol } { puts "  ok   $label ($got)" } else {
        puts "  FAIL $label: got $got want ~$want"; incr ::nfail
    }
}

# a floor, a ball above it
proc floor_world {} {
    set s [b2world::default_spec]
    dict set s bounds {-20 20 -20 40}
    b2world::add_body s [b2world::body ground box static 0.0 -8.5 w 40 h 1 roles {ground}]
    b2world::add_body s [b2world::body ball circle dynamic 0.0 0.0 r 0.4 roles {projectile}]
    return $s
}

puts "bodies as data:"
set s [floor_world]
check "two bodies" {[llength [dict get $s bodies]] == 2}
check "find_body" {[dict get [b2world::find_body $s ball] r] == 0.4}
if { [catch { b2world::body bad box static 0 0 } e] } { puts "  ok   a box without w/h is refused" } else { puts "  FAIL box refused"; incr ::nfail }

puts "\na launch, a contact stop rule, recorded path:"
set r [b2world::simulate $s -launch {ball 3.0 6.0} \
           -stop {{contact projectile ground miss}} -record ball]
check  "outcome from the rule"           {[dict get $r outcome] eq "miss"}
check  "contact logged with both names"  {[llength [dict get $r contacts]] >= 1 && [lsearch [lindex [dict get $r contacts] 0] ball] >= 0}
check  "contacts_of gives {t other}"     {[lindex [lindex [b2world::contacts_of $r ball] 0] 1] eq "ground"}
set p [dict get $r paths ball]
check  "path recorded"                   {[llength [dict get $p x]] == [dict get $r n_steps] + 1}
check  "first sample at the start"       {[lindex [dict get $p x] 0] == 0.0 && [lindex [dict get $p y] 0] == 0.0}
check  "final position reported"         {[dict exists [dict get $r final] ball]}
set ymax -1e9
foreach y [dict get $p y] { if { $y > $ymax } { set ymax $y } }
approx "apex ~ v^2/2g at g=-10 (within 5%)" $ymax 1.8 0.1

puts "\ngravity is honoured on every dynamic body:"
set s2 [floor_world]; dict set s2 gravity -5.0
set r2 [b2world::simulate $s2 -launch {ball 3.0 6.0} -stop {{contact projectile ground miss}} -record ball]
set ymax2 -1e9
foreach y [dict get [dict get $r2 paths ball] y] { if { $y > $ymax2 } { set ymax2 $y } }
approx "half gravity -> twice the apex (ratio)" [expr {$ymax2/$ymax}] 2.0 0.1

puts "\nbounds -> out; no rule -> timeout; stride thins the record:"
set s3 [floor_world]; dict set s3 bounds {-2 2 -20 40}
set r3 [b2world::simulate $s3 -launch {ball 10.0 0.0} -record ball]
check "left the bounds"      {[dict get $r3 outcome] eq "out"}
set s4 [floor_world]; dict set s4 max_t 0.5
set r4 [b2world::simulate $s4 -launch {ball 0.0 20.0} -record ball]
check "time cap -> timeout"  {[dict get $r4 outcome] eq "timeout"}
set r5 [b2world::simulate $s -launch {ball 3.0 6.0} -stop {{contact projectile ground miss}} -record ball -stride 4]
check "stride 4 records ~1/4 of the steps" {abs([llength [dict get [dict get $r5 paths ball] x]] - [dict get $r5 n_steps]/4) <= 2}

puts "\nrules match by ROLE, first rule wins:"
set s6 [floor_world]
b2world::add_body s6 [b2world::body pad box static 0.0 -5.0 w 2 h 0.3 roles {target hit}]
set r6 [b2world::simulate $s6 -launch {ball 0.0 0.0} \
            -stop {{contact projectile hit hit} {contact projectile ground miss}} -record ball]
check "dropped onto the pad: hit"        {[dict get $r6 outcome] eq "hit"}
set r7 [b2world::simulate $s6 -launch {ball 4.0 0.0} \
            -stop {{contact projectile hit hit} {contact projectile ground miss}} -record ball]
check "thrown past the pad: miss"        {[dict get $r7 outcome] eq "miss"}

puts "\na custom stop proc sees tracked positions:"
proc stop_when_high { w t positions } {
    lassign [dict get $positions ball] x y
    if { $y > 1.0 } { return high }
    return ""
}
set r8 [b2world::simulate $s -launch {ball 0.0 8.0} -stop_proc stop_when_high -record ball]
check "custom outcome"                   {[dict get $r8 outcome] eq "high"}
check "stopped early"                    {[dict get $r8 t_end] < 0.5}

puts "\nkinematic bodies move along their path and deflect the ball:"
set s9 [floor_world]
# a paddle sweeping left-to-right through the ball's fall line
b2world::add_body s9 [b2world::body paddle box kinematic -3.0 -3.0 w 1.5 h 0.3 roles {obstacle} \
                          path {kind linear vx 4.0 vy 0.0}]
set r9 [b2world::simulate $s9 -launch {ball 0.0 0.0} \
            -stop {{contact projectile ground miss}} -record {ball paddle}]
check "the paddle was contacted"         {[lsearch -index 1 [b2world::contacts_of $r9 ball] paddle] >= 0}
lassign [dict get $r9 final paddle] px py
check "the paddle moved right"           {$px > -2.0}
lassign [dict get $r9 final ball] bx by
check "the ball was knocked sideways"    {abs($bx) > 0.3}
check "the paddle's path was recorded"   {[llength [dict get [dict get $r9 paths paddle] x]] > 10}

puts "\n... an oscillating body returns to its start after one period:"
set path {kind oscillate ax 2.0 ay 0.0 period 1.0 phase_deg 0.0}
lassign [b2world::path_position $path 5.0 -3.0 0.25] x q
approx "quarter period: +ax"  $x 7.0 1e-6
lassign [b2world::path_position $path 5.0 -3.0 1.0] x q
approx "full period: back"    $x 5.0 1e-6
set s10 [floor_world]
b2world::add_body s10 [b2world::body bob box kinematic 5.0 -3.0 w 0.5 h 0.5 roles {obstacle} path $path]
dict set s10 max_t 1.0
set r10 [b2world::simulate $s10 -record {bob}]
lassign [dict get $r10 final bob] bx by
approx "simulated oscillation returns to its start (velocity-driven)" $bx 5.0 0.15

puts "\ndeterminism:"
set ra [b2world::simulate $s -launch {ball 3.0 6.0} -stop {{contact projectile ground miss}} -record ball]
set rb [b2world::simulate $s -launch {ball 3.0 6.0} -stop {{contact projectile ground miss}} -record ball]
check "identical paths" {[dict get $ra paths] eq [dict get $rb paths]}

puts "\nsweep:"
proc launch_case { c } { return [list ball [lindex $c 0] [lindex $c 1]] }
set sw [b2world::sweep $s6 {{0 0} {4 0} {0 5}} launch_case \
            -stop {{contact projectile hit hit} {contact projectile ground miss}} -track ball]
check "one result per case" {[llength $sw] == 3}
check "outcomes per case"   {[lindex [lindex $sw 0] 1] eq "hit" && [lindex [lindex $sw 1] 1] eq "miss"}

puts ""
if { $::nfail } { puts "FAILURES: $::nfail"; exit 1 }
puts "all checks passed"
