# test_traj.tcl --
#   Proves the `traj` ballistic model is IDENTICAL to the three production
#   implementations it is meant to replace, before any of them is migrated:
#
#     1. ess::pursuit::ballistic::symmetric_arc  (the ESS loader constructor)
#        -- loaded from the REAL system via ess_test, not transcribed here.
#     2. launch_sim::ball_pos_at_time / ball_vel_at_time  (analytic branch).
#     3. the per-frame formula inlined in ballistic_stim.tcl (transcribed --
#        it lives inside an update proc, so there is nothing callable to hit).
#
#   Compared with EXACT float equality (the expressions are the same, so the
#   results must be bit-identical) over the real parameter grid the pursuit
#   variants actually use, plus off-grid cases (g=0 line, offset launch_y).
#
#   Run headless:  dlsh /Users/sheinb/src/dlsh/vfs/lib/traj/test_traj.tcl
#
#   Bootstrap: source the ON-DISK traj.tcl -- auto_path does NOT reliably
#   shadow a copy already baked into dlsh.zip (same lesson as test_ess_test).

catch { source /usr/local/dlsh/dlsh_setup.tcl }
source [file join [file dirname [info script]] traj.tcl]
package require launch_sim
package require ess_test
ess_test::config -systems_root /Users/sheinb/systems/ess

set ::fail 0 ; set ::checks 0
proc same { a b what } {
    incr ::checks
    if { $a != $b } {
        incr ::fail
        puts "  FAIL $what:  traj=$a  ref=$b  (delta [expr {$a-$b}])"
    }
}

# ------------------------------------------------------------------
# 1. constructor vs the FROZEN pre-migration symmetric_arc (golden model)
#
#    This carries a verbatim copy of the arc geometry as it stood in
#    ess::pursuit::ballistic::symmetric_arc BEFORE that proc was migrated to
#    delegate here. It is deliberately duplicated: comparing against the live
#    symmetric_arc would now be CIRCULAR (it calls traj), so the golden model
#    is what keeps this a real regression guard.
#    DO NOT "simplify" this to call traj::ballistic::symmetric.
# ------------------------------------------------------------------
proc golden_symmetric_arc { ecc T g side {launch_y 0.0} } {
    set peak [expr {$g * $T * $T / 8.0}]
    set apex [expr {$launch_y + $peak}]
    if { $side == 1 } {
        set lx [expr { $ecc}] ; set vx [expr {-2.0*$ecc/$T}]
    } else {
        set lx [expr {-$ecc}] ; set vx [expr { 2.0*$ecc/$T}]
    }
    return [dict create \
        launcher_x $lx  launcher_y $launch_y \
        vx $vx  vy [expr {0.5*$g*$T}]  gravity $g \
        land_time $T  land_x [expr {-$lx}]  floor_y $launch_y  side $side \
        maxext [expr {max($ecc, abs($launch_y), abs($apex))}]]
}

puts "1. constructor vs FROZEN pre-migration symmetric_arc (golden model)"
set trajs {}
foreach g { -9.8 9.8 0.0 } {
    foreach ecc { 6 9 12 } {
        foreach T { 1.5 2.0 } {
            foreach side { 0 1 } {
                set ref [golden_symmetric_arc $ecc $T $g $side 0.0]
                set new [traj::ballistic::symmetric $ecc $T $g $side 0.0]
                dict for { k v } $ref {
                    same [dict get $new $k] $v "symmetric(g=$g ecc=$ecc T=$T side=$side).$k"
                }
                lappend trajs $new
            }
        }
    }
}
puts "   [llength $trajs] arcs, all golden keys exact"

# ------------------------------------------------------------------
# 2/3. pos & vel, on a dense grid over the OPEN flight interval (launch_sim
#      CLAMPS at the endpoints -- policy; traj is the pure unclamped model).
#
#      NOTE on what each check is worth, post-migration:
#      (2) launch_sim now DELEGATES to traj, so this is no longer an independent
#          check of the MATH -- it is an INTEGRATION check that the delegation is
#          wired right (correct dict keys, clamping boundaries still honoured).
#          Kept for that reason; do not mistake it for a second opinion.
#      (3) is the independent math guard: a literal transcription of the formula
#          as it stands in ballistic_stim.tcl, which does NOT call traj. When the
#          stim is migrated too (step 4), keep this literal FROZEN -- it becomes
#          the golden model for pos/vel, exactly as golden_symmetric_arc did for
#          the constructor.
# ------------------------------------------------------------------
puts "2. pos/vel vs launch_sim (integration: delegation wired correctly)"
puts "3. pos/vel vs the literal ballistic_stim.tcl formula (independent guard)"
# extra off-grid params to stress the model beyond the symmetric constructor
lappend trajs \
    [dict create motion_type ballistic launcher_x -4.0 launcher_y 1.5 \
         vx 7.0 vy -3.0 gravity 0.0 land_time 2.0 land_x 10.0 floor_y 1.5] \
    [dict create motion_type ballistic launcher_x 3.0 launcher_y -2.0 \
         vx -5.5 vy 6.25 gravity 12.0 land_time 1.1 land_x -3.05 floor_y -2.0]

foreach tr $trajs {
    set T [dict get $tr land_time]
    set lyy [dict get $tr launcher_y] ; set lxx [dict get $tr launcher_x]
    set vxx [dict get $tr vx] ; set vyy [dict get $tr vy] ; set gg [dict get $tr gravity]
    for { set i 1 } { $i < 200 } { incr i } {
        set t [expr {$T * $i / 200.0}]
        lassign [traj::pos $tr $t] tx ty
        lassign [traj::vel $tr $t] tvx tvy
        # --- ref 2: launch_sim analytic ---
        lassign [launch_sim::ball_pos_at_time $tr $t analytic] lx ly
        lassign [launch_sim::ball_vel_at_time $tr $t analytic] lvx lvy
        same $tx $lx "pos.x vs launch_sim (t=$t)"
        same $ty $ly "pos.y vs launch_sim (t=$t)"
        same $tvx $lvx "vel.x vs launch_sim (t=$t)"
        same $tvy $lvy "vel.y vs launch_sim (t=$t)"
        # --- ref 3: the stim's inlined per-frame arithmetic ---
        set sx [expr {$lxx + $vxx*$t}]
        set sy [expr {$lyy + $vyy*$t - 0.5*$gg*$t*$t}]
        set svy [expr {$vyy - $gg*$t}]
        same $tx $sx  "pos.x vs stim formula (t=$t)"
        same $ty $sy  "pos.y vs stim formula (t=$t)"
        same $tvy $svy "vel.y vs stim formula (t=$t)"
    }
}
puts "   [llength $trajs] trajectories x 199 timepoints"

# ------------------------------------------------------------------
# 4. landmarks + extent
# ------------------------------------------------------------------
puts "4. landmarks (apex) and extent"
foreach g { -9.8 9.8 } {
    foreach ecc { 6 9 12 } {
        set T 1.5
        set tr [traj::ballistic::symmetric $ecc $T $g 0 0.0]
        set m [traj::landmarks $tr]
        # symmetric arc: vy = g*T/2, so apex = vy/g = T/2 exactly
        same [lindex [dict get $m apex] 0] [expr {$T/2.0}] "apex(g=$g ecc=$ecc)"
        same [traj::duration $tr] $T "duration(g=$g ecc=$ecc)"
        # extent must reproduce symmetric_arc's maxext exactly
        same [traj::extent $tr] [dict get $tr maxext] "extent(g=$g ecc=$ecc)"
    }
}
# g=0 => no apex, and the line's extent is endpoint-bounded
set lin [dict create motion_type ballistic launcher_x -4.0 launcher_y 1.5 \
             vx 7.0 vy -3.0 gravity 0.0 land_time 2.0 land_x 10.0 floor_y 1.5]
if { [dict exists [traj::landmarks $lin] apex] } {
    incr ::fail ; puts "  FAIL: g=0 line should have no apex landmark"
}
incr ::checks

# ------------------------------------------------------------------
# 5. back-compat: motion_type ABSENT must behave as ballistic
# ------------------------------------------------------------------
puts "5. back-compat: dicts with no motion_type default to ballistic"
set bare [dict remove [lindex $trajs 0] motion_type]
lassign [traj::pos $bare 0.7] bx by
lassign [traj::pos [lindex $trajs 0] 0.7] rx ry
same $bx $rx "bare-dict pos.x" ; same $by $ry "bare-dict pos.y"

puts ""
if { $::fail } {
    puts "FAILED: $::fail / $::checks checks"
    exit 1
}
puts "PASS: all $::checks checks exact (traj ballistic == golden symmetric_arc == launch_sim == stim formula)"
