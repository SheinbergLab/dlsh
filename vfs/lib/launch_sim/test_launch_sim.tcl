# test_launch_sim.tcl -- headless sanity tests for the launch_sim package.
#
# Run from any dlsh-capable shell:
#   tclsh9.0 test_launch_sim.tcl
#
# Mounts the system dlsh zipvfs, then front-loads this checkout's parent
# (vfs/lib) on auto_path so the ON-DISK launch_sim shadows any copy already
# baked into dlsh.zip (so tests exercise what we just edited).

if {![info exists ::__launch_sim_test_loaded]} {
    source /usr/local/dlsh/dlsh_setup.tcl
    package require dlsh
    # launch_sim now depends on `traj` (the shared trajectory model). Source the
    # ON-DISK traj too -- until dlsh.zip is rebuilt it isn't in the VFS at all,
    # and even after, we want to test the copy we just edited. Sourcing runs its
    # `package provide traj 1.0`, satisfying launch_sim's `package require`.
    catch {package forget traj}
    catch {namespace delete ::traj}
    source [file join [file dirname [file dirname [info script]]] traj traj.tcl]
    # Source the ON-DISK launch_sim directly. auto_path front-loading does NOT
    # reliably shadow a copy already in dlsh.zip (dlsh's package scan caches the
    # zip's `package ifneeded` first), so source the sibling file to be sure we
    # test what we just edited, not the deployed copy.
    catch {package forget launch_sim}
    catch {namespace delete ::launch_sim}
    source [file join [file dirname [info script]] launch_sim.tcl]
    set ::__launch_sim_test_loaded 1
}

set ::nfail 0
proc assert {cond msg} {
    if {![uplevel 1 [list expr $cond]]} {
        puts stderr "FAIL: $msg"; incr ::nfail
    } else {
        puts "  ok: $msg"
    }
}

set p [launch_sim::default_params]

# ---- 1. occlusion board: many trials, both sides ----
puts "occlusion board (200 trials)..."
for {set i 0} {$i < 200} {incr i} {
    set want [expr {$i % 2}]
    set tr [launch_sim::sample_trajectory $p $want]

    set side [dict get $tr side]
    set ghw  [dict get $tr goal_halfwidth]
    set lx   [dict get $tr land_x]
    if {$side == 0} {
        set lo [expr {[dict get $tr lcatcher_x] + $ghw/2.0}]
        set hi [expr {[dict get $tr lcatcher_x] + $ghw}]
    } else {
        set lo [expr {[dict get $tr rcatcher_x] - $ghw}]
        set hi [expr {[dict get $tr rcatcher_x] - $ghw/2.0}]
    }

    if {$side != $want} { puts stderr "FAIL: side not honored"; incr ::nfail; break }
    if {!($lx >= $lo-1e-6 && $lx <= $hi+1e-6)} {
        puts stderr "FAIL: land_x $lx not in target half \[$lo $hi\]"; incr ::nfail; break }

    set bt [dict get $tr ball_t]; set bx [dict get $tr ball_x]; set by [dict get $tr ball_y]
    if {[llength $bt] != [llength $bx] || [llength $bx] != [llength $by]} {
        puts stderr "FAIL: trajectory column lengths differ"; incr ::nfail; break }
    if {abs([lindex $bx 0] - [dict get $tr launcher_x]) > 1e-6} {
        puts stderr "FAIL: trajectory does not start at launcher_x"; incr ::nfail; break }
    if {abs([lindex $by end] - [dict get $tr floor_y]) > 1e-6} {
        puts stderr "FAIL: trajectory does not end at floor_y"; incr ::nfail; break }
    if {abs([lindex $bx end] - $lx) > 1e-6} {
        puts stderr "FAIL: trajectory does not end at land_x"; incr ::nfail; break }
}
assert {$::nfail == 0} "200 floor trials satisfied all landing constraints"

# ---- 2. wider launcher board still lands ----
puts "wider launcher board..."
set p2 [dict merge $p {launcher_x_max 8.0}]
set tr2 [launch_sim::sample_trajectory $p2]
assert {[dict get $tr2 land_time] > 0} "trial lands"

# ---- 3. build_trials dg ----
puts "build_trials..."
set g [launch_sim::build_trials 10 $p]
assert {[dl_length $g:side] == 10} "build_trials produced 10 trials"
assert {[dl_length $g:ball_x:0] > 2} "trial 0 has a sampled trajectory"
assert {[dl_sum $g:side] == 5} "build_trials split 5 left / 5 right"

# ---- 4. shape_geometry equal-area ----
puts "shape_geometry..."
foreach s {Circle RectThin DiamondSq} {
    lassign [launch_sim::shape_geometry $s 0.126] shp sx sy ang
    assert {abs($sx*$sy*([string equal $shp Circle] ? 3.14159265 : 1.0) - 0.126) < 1e-3 || [string equal $shp Box]} "$s geometry returns sane extents"
}

# ---- 5. draw_trial runs headless (writes the gbuf; no display needed) ----
puts "draw_trial (headless gbuf)..."
set ok [expr {![catch {launch_sim::draw_trial $tr2} err]}]
assert {$ok} "draw_trial executes without error ($err)"

# ---- 6. ball_pos_at_time: analytic exactness + interp accuracy ----
puts "ball_pos_at_time..."
set tr [launch_sim::sample_trajectory $p 0]
set lt [dict get $tr land_time]

lassign [launch_sim::ball_pos_at_time $tr 0.0] x0 y0
assert {abs($x0-[dict get $tr launcher_x])<1e-9 && abs($y0-[dict get $tr launcher_y])<1e-9} \
    "t<=0 returns the launch point"
lassign [launch_sim::ball_pos_at_time $tr [expr {$lt+1.0}]] xe ye
assert {abs($xe-[dict get $tr land_x])<1e-9 && abs($ye-[dict get $tr floor_y])<1e-9} \
    "t>=land_time returns the landing point"

# analytic must equal the closed form exactly; interp must track it closely,
# both at OFF-GRID (sub-dt) times -- the smooth-playback guarantee.
set vx [dict get $tr vx]; set vy [dict get $tr vy]; set g [dict get $tr gravity]
set lx [dict get $tr launcher_x]; set ly [dict get $tr launcher_y]
set maxerr_a 0.0; set maxerr_i 0.0
for {set k 1} {$k < 50} {incr k} {
    set t [expr {$lt*$k/50.0 + 0.0007}]
    if {$t >= $lt} continue
    set ex [expr {$lx+$vx*$t}]; set ey [expr {$ly+$vy*$t-0.5*$g*$t*$t}]
    lassign [launch_sim::ball_pos_at_time $tr $t analytic] ax ay
    lassign [launch_sim::ball_pos_at_time $tr $t interp]   ix iy
    set ea [expr {hypot($ax-$ex,$ay-$ey)}]; if {$ea>$maxerr_a} {set maxerr_a $ea}
    set ei [expr {hypot($ix-$ex,$iy-$ey)}]; if {$ei>$maxerr_i} {set maxerr_i $ei}
}
assert {$maxerr_a < 1e-9}  "analytic == closed form (max err [format %.2e $maxerr_a])"
assert {$maxerr_i < 0.02}  "interp within 0.02 dva of closed form (max err [format %.4f $maxerr_i])"

# auto picks analytic when params present; falls back to interp when absent
lassign [launch_sim::ball_pos_at_time $tr [expr {$lt*0.5}] auto]     aax aay
lassign [launch_sim::ball_pos_at_time $tr [expr {$lt*0.5}] analytic] anx any
assert {abs($aax-$anx)<1e-12 && abs($aay-$any)<1e-12} "auto uses analytic when vx present"
set tr_nopar [dict remove $tr vx]
lassign [launch_sim::ball_pos_at_time $tr_nopar [expr {$lt*0.5}] auto] px py
assert {[string is double -strict $px] && [string is double -strict $py]} \
    "auto falls back to interp when vx absent"

# build_trials now stores vx/vy for the analytic path source
set g2 [launch_sim::build_trials 4 $p g2dg]
assert {[dl_exists g2dg:vx] && [dl_exists g2dg:vy]} "build_trials stores vx/vy"

# (occlusion is now a DECOUPLED overlay -- tested in section 11, not baked
#  into the samplers.)

# ---- 8. linear motion: gravity 0 + downward angles => straight path ----
puts "linear (zero-gravity) motion..."
set plin [dict merge $p {gravity_min 0 gravity_max 0 angle_min -60 angle_max -10}]
set trl [launch_sim::sample_trajectory $plin 1]
assert {[dict get $trl gravity] == 0} "linear trial has zero gravity"
assert {[dict get $trl vy] < 0} "linear shot is aimed downward (vy<0)"
# every sample lies exactly on the constant-velocity line y = ly + vy*t
set vyl [dict get $trl vy]; set lyl [dict get $trl launcher_y]
set maxerr 0.0
foreach t [dict get $trl ball_t] y [dict get $trl ball_y] {
    set e [expr {abs($y - ($lyl + $vyl*$t))}]
    if {$e > $maxerr} {set maxerr $e}
}
assert {$maxerr < 1e-9} "linear: y == launcher_y + vy*t at every sample (max err [format %.2e $maxerr])"
# ball_pos_at_time analytic stays exact with g=0
lassign [launch_sim::ball_pos_at_time $trl [expr {[dict get $trl land_time]*0.5}] analytic] mlx mly
assert {abs($mly - ($lyl + $vyl*[expr {[dict get $trl land_time]*0.5}])) < 1e-9} \
    "ball_pos_at_time analytic exact under zero gravity"

# ---- 8b. negative gravity: inverse parabola == vertical mirror of |g| arc ----
puts "negative gravity (inverse parabola)..."
# pin every range so +g and -g are the SAME launch -> a matched mirror pair.
# wide goals (side 1 window [0,50]) so the deterministic land_x is accepted.
set ppin {launcher_x_min -5 launcher_x_max -5 launcher_y_min -2 launcher_y_max -2 \
          floor_y -6 lcatcher_x -100 rcatcher_x 100 goal_halfwidth 100 \
          speed_min 12 speed_max 12 angle_min 55 angle_max 55 \
          min_visible 0.3 max_sim_time 6.0 max_attempts 200}
set pos [launch_sim::sample_trajectory [dict merge $p $ppin {gravity_min 9.8 gravity_max 9.8}] 1]
set neg [launch_sim::sample_trajectory [dict merge $p $ppin {gravity_min -9.8 gravity_max -9.8}] 1]
set lyp [dict get $pos launcher_y]
assert {abs([dict get $neg vy] + [dict get $pos vy]) < 1e-9} \
    "inverted vy == -normal vy (launched the other way)"
assert {[dict get $neg gravity] < 0 && abs([dict get $neg gravity]+[dict get $pos gravity]) < 1e-9} \
    "inverted gravity == -normal gravity"
assert {abs([dict get $neg land_time]-[dict get $pos land_time]) < 1e-9} \
    "inverted land_time == normal (matched duration)"
assert {abs([dict get $neg land_x]-[dict get $pos land_x]) < 1e-9} \
    "inverted land_x == normal (x unchanged)"
set merr 0.0
foreach a [dict get $pos ball_y] b [dict get $neg ball_y] {
    set e [expr {abs((2.0*$lyp-$a)-$b)}]; if {$e > $merr} {set merr $e}
}
assert {$merr < 1e-9} "inverted ball_y == 2*launcher_y - normal ball_y at every sample (max err [format %.2e $merr])"
assert {[dict exists $neg land_y] && \
        abs([dict get $neg land_y]-(2.0*$lyp-[dict get $neg floor_y])) < 1e-9} \
    "inverted dict carries land_y = 2*launcher_y - floor_y"
assert {[dict exists $pos land_y] && abs([dict get $pos land_y]-[dict get $pos floor_y]) < 1e-9} \
    "positive-gravity land_y == floor_y (now always present)"
# analytic replay needs no special-casing under negative gravity
set lt2 [dict get $neg land_time]
set vxn [dict get $neg vx]; set vyn [dict get $neg vy]; set gn [dict get $neg gravity]
set lxn [dict get $neg launcher_x]; set lyn [dict get $neg launcher_y]
set merr2 0.0
for {set k 1} {$k < 40} {incr k} {
    set t [expr {$lt2*$k/40.0}]
    set ey [expr {$lyn + $vyn*$t - 0.5*$gn*$t*$t}]
    lassign [launch_sim::ball_pos_at_time $neg $t analytic] ax ay
    set e [expr {abs($ay-$ey)}]; if {$e > $merr2} {set merr2 $e}
}
assert {$merr2 < 1e-9} "ball_pos_at_time analytic exact under negative gravity (max err [format %.2e $merr2])"
lassign [launch_sim::ball_vel_at_time $neg [expr {$lt2*0.5}] analytic] nvx nvy
assert {abs($nvx-$vxn) < 1e-9 && abs($nvy-($vyn-$gn*$lt2*0.5)) < 1e-9} \
    "ball_vel_at_time analytic == (vx, vy - g*t) with g<0"

# ---- 8c. g=0 straight line over a sampled duration (no-curvature control) ----
puts "linear control (g=0, duration-based)..."
set lin [launch_sim::sample_trajectory \
    [dict merge $p $ppin {gravity_min 0 gravity_max 0 linear_dur_min 1.5 linear_dur_max 1.5}] 1]
assert {[dict get $lin gravity] == 0} "linear trial has zero gravity"
assert {abs([dict get $lin land_time]-1.5) < 1e-9} "linear land_time == the sampled duration"
assert {[dict get $lin vy] > 0} "linear honors the (upward) launch elevation (vy>0, not floor-limited)"
set vxL [dict get $lin vx]; set vyL [dict get $lin vy]
set lxL [dict get $lin launcher_x]; set lyL [dict get $lin launcher_y]
set me 0.0
foreach t [dict get $lin ball_t] x [dict get $lin ball_x] y [dict get $lin ball_y] {
    set e [expr {hypot($x-($lxL+$vxL*$t), $y-($lyL+$vyL*$t))}]; if {$e > $me} {set me $e}
}
assert {$me < 1e-9} "linear: every sample on launcher + v*t (max err [format %.2e $me])"
assert {[dict exists $lin land_y] && \
        abs([dict get $lin land_y]-($lyL+$vyL*[dict get $lin land_time])) < 1e-9} \
    "linear land_y == launcher_y + vy*T (line endpoint)"

# ---- 9. circle boundary: interior launcher, exit on rim, exit angle ----
puts "circle boundary..."
set pc [dict merge $p {boundary circle angle_min 0 angle_max 360 gravity_min 0 gravity_max 10}]
set trc [launch_sim::sample_trajectory $pc]
assert {[dict get $trc boundary] eq "circle"} "boundary is circle"

set cxr [dict get $trc circle_cx]; set cyr [dict get $trc circle_cy]; set Rr [dict get $trc circle_r]
set rexit [expr {hypot([dict get $trc land_x]-$cxr,[dict get $trc land_y]-$cyr)}]
assert {abs($rexit-$Rr) < 1e-4} "exit lies on the circle (r=[format %.5f $rexit] vs R=$Rr)"
set ea [dict get $trc exit_angle]
assert {abs([dict get $trc land_x]-($cxr+$Rr*cos($ea))) < 1e-3 && \
        abs([dict get $trc land_y]-($cyr+$Rr*sin($ea))) < 1e-3} "exit_angle matches exit point"
set rl [expr {hypot([dict get $trc launcher_x]-$cxr,[dict get $trc launcher_y]-$cyr)}]
assert {$rl < $Rr} "launcher is interior (r=[format %.2f $rl] < R=$Rr)"

# every sampled point before the appended exit stays within the circle
set inside 1
set bx [dict get $trc ball_x]; set byy [dict get $trc ball_y]
for {set i 0} {$i < [llength $bx]-1} {incr i} {
    if {[expr {hypot([lindex $bx $i]-$cxr,[lindex $byy $i]-$cyr)}] > $Rr+1e-6} {set inside 0; break}
}
assert {$inside} "trajectory stays within the circle until exit"

# ball_pos_at_time clamps to the rim exit (uses land_y, not floor_y)
lassign [launch_sim::ball_pos_at_time $trc [expr {[dict get $trc land_time]+1}]] cex cey
assert {abs($cex-[dict get $trc land_x])<1e-9 && abs($cey-[dict get $trc land_y])<1e-9} \
    "ball_pos_at_time endpoint = rim exit"

# build_trials + draw_trial in circle mode
set gc [launch_sim::build_trials 4 $pc gcdg]
assert {[dl_exists gcdg:exit_angle] && [dl_exists gcdg:circle_r]} "build_trials stores circle columns"
assert {![catch {launch_sim::draw_trial $trc}]} "draw_trial circle runs"

# ---- 10. arc-landing mode: launcher-centered, deviation from heading ----
puts "arc landing mode..."
set pa [dict merge $p {boundary arc angle_min 0 angle_max 360 \
        gravity_min 3 gravity_max 14 arc_radius 8 arc_span_deg 140 launcher_jitter 1.5}]
set tra [launch_sim::sample_trajectory $pa]
assert {[dict get $tra boundary] eq "arc"} "boundary is arc"
set rr [expr {hypot([dict get $tra land_x]-[dict get $tra launcher_x], \
                    [dict get $tra land_y]-[dict get $tra launcher_y])}]
assert {abs($rr-[dict get $tra arc_radius]) < 1e-4} "exit lies on arc radius from launcher"
set halfspan [expr {[dict get $tra arc_span_deg]/2.0*3.14159265/180.0}]
assert {abs([dict get $tra deviation]) <= $halfspan+1e-9} "deviation within valid arc span"
# catcher pose at the true deviation matches the exit and is tangent (perp to radius)
lassign [launch_sim::catcher_pose $tra [dict get $tra deviation]] kx ky ka
assert {abs($kx-[dict get $tra land_x])<1e-6 && abs($ky-[dict get $tra land_y])<1e-6} \
    "catcher pose at landing == exit point"
assert {abs(cos($ka-[dict get $tra exit_angle])) < 1e-6} "catcher is tangent (perp to radius)"
# linear (g=0) arc: lands straight ahead (deviation ~ 0)
set tral [launch_sim::sample_trajectory [dict merge $pa {gravity_min 0 gravity_max 0}]]
assert {abs([dict get $tral deviation]) < 1e-6} "linear arc lands straight ahead (dev~0)"
# build_trials arc columns
set ga [launch_sim::build_trials 4 $pa gadg]
assert {[dl_exists gadg:deviation] && [dl_exists gadg:catcher_angle]} "build_trials stores arc columns"

# ---- 11. decoupled occluder: occlusion_intervals / occlude / point_occluded -
puts "occluder intervals (decoupled)..."
set big [list [dict create type rect x0 -100 y0 -100 x1 100 y1 100]]
set bt [dict get $tra ball_t]; set bbx [dict get $tra ball_x]; set bby [dict get $tra ball_y]
set ints [launch_sim::occlusion_intervals $bt $bbx $bby $big]
assert {[llength $ints]==1 && [lindex [lindex $ints 0] 0]==[lindex $bt 0]} \
    "all-covering rect => one interval from t0"
assert {[llength [launch_sim::occlusion_intervals $bt $bbx $bby {}]]==0} "no regions => no occlusion"
set occd [launch_sim::occlude $tra $big]
assert {[dict get $occd occlusion_duration] > 0} "occlude computes positive hidden duration"
assert {[launch_sim::point_occluded 0 0 [list [dict create type circle cx 0 cy 0 r 1]]]} \
    "point inside circle region is occluded"
assert {![launch_sim::point_occluded 5 5 [list [dict create type circle cx 0 cy 0 r 1]]]} \
    "point outside circle region is not occluded"
# duration-targeted selection in build_trials (a rect spanning the field)
set pad [dict merge $pa [list occluder $big occl_dur_min 0.05]]
set gad [launch_sim::build_trials 3 $pad gaddg]
assert {[dl_exists gaddg:occlusion_duration] && [dl_min gaddg:occlusion_duration] >= 0.05} \
    "duration-targeted build keeps occl_dur >= min"

# ---- 12. ball_vel_at_time + require_exit_occluded ----
puts "ball_vel_at_time + exit-occluded gate..."
set trv [launch_sim::sample_trajectory $p 0]
set ltv [dict get $trv land_time]
set vx0 [dict get $trv vx]; set vy0 [dict get $trv vy]; set gg [dict get $trv gravity]
lassign [launch_sim::ball_vel_at_time $trv [expr {$ltv*0.5}] analytic] avx avy
assert {abs($avx-$vx0)<1e-9 && abs($avy-($vy0-$gg*$ltv*0.5))<1e-9} \
    "ball_vel_at_time analytic == (vx, vy - g*t)"
lassign [launch_sim::ball_vel_at_time $trv [expr {$ltv*0.5}] interp] ivx ivy
assert {hypot($ivx-$avx,$ivy-$avy) < 0.5} "ball_vel_at_time interp ~ analytic"

# require_exit_occluded: every built trial's exit lies inside the occluder
set regs [list [dict create type arc cx 0 cy 0 r0 6 r1 8 \
        a0 [expr {-3.14159265}] a1 3.14159265]]
set pae [dict merge $pa [list occluder $regs require_exit_occluded 1]]
set gae [launch_sim::build_trials 6 $pae gaedg]
set all_exit_occ 1
for {set i 0} {$i < [dl_length gaedg:land_x]} {incr i} {
    if {![launch_sim::point_occluded [dl_get gaedg:land_x $i] [dl_get gaedg:land_y $i] $regs]} {
        set all_exit_occ 0; break
    }
}
assert {$all_exit_occ} "require_exit_occluded => every built trial's exit is occluded"

# ---- 13. any-side (<0): deterministic arc launches never side-fight ----
puts "any-side deterministic launch..."
# a fixed-parameter launch (incl. the degenerate dev=0 heading) must succeed
# every time with side <0 -- regression for the old random-side rejection.
set anyside_ok 1
foreach hd {0 90 180 270} {
    set pd [dict merge $p [list boundary arc launcher_jitter 0 arc_radius 5.0 \
            arc_span_deg 360 max_sim_time 20.0 min_visible 0.05 \
            angle_min $hd angle_max $hd speed_min 2.5 speed_max 2.5 \
            gravity_min 0 gravity_max 0]]
    for {set i 0} {$i < 25} {incr i} {
        if {[catch {launch_sim::sample_trajectory $pd -1}]} { set anyside_ok 0; break }
    }
    if {!$anyside_ok} break
}
assert {$anyside_ok} "deterministic arc launches (incl. dev=0 headings) succeed with side<0"

if {$::nfail == 0} { puts "\nALL PASS" } else { puts "\n$::nfail FAILURE(S)"; exit 1 }
