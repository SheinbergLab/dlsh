# test_sling_sim.tcl -- headless checks for the sling_sim package.
#
#   dlsh test_sling_sim.tcl        (or tclsh9.0 after dlsh_setup)
#
# Sources the ON-DISK sling_sim.tcl so it tests what was just edited, not a
# copy baked into dlsh.zip.

if {![info exists ::__sling_sim_test_loaded]} {
    catch { source /usr/local/dlsh/dlsh_setup.tcl }
    package require dlsh
    catch {package forget sling_sim}
    catch {namespace delete ::sling_sim}
    source [file join [file dirname [info script]] sling_sim.tcl]
    set ::__sling_sim_test_loaded 1
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

set spec [sling_sim::default_spec]

puts "velocity mapping:"
lassign [sling_sim::velocity $spec -3.0 0.0] vx vy
approx "full pull -x launches +x at v_max" $vx 16.0 1e-6
approx "...with no y"                      $vy 0.0  1e-6
lassign [sling_sim::velocity $spec -1.5 -1.5] vx vy
approx "half-draw diagonal: |v| = v_max/2 * sqrt(2)/sqrt(2)" [expr {hypot($vx,$vy)}] 11.3137 1e-3
check  "diagonal launches up-right" [expr {$vx > 0 && $vy > 0}]
lassign [sling_sim::velocity $spec -30.0 0.0] vx vy
approx "over-draw clamps at v_max" $vx 16.0 1e-6
check  "zero pull -> zero velocity" {[sling_sim::velocity $spec 0 0] eq {0.0 0.0}}
approx "pull_frac clamps"  [sling_sim::pull_frac $spec 9 0] 1.0 1e-9
approx "pull_frac half"    [sling_sim::pull_frac $spec 1.5 0] 0.5 1e-9
lassign [sling_sim::pull_from_polar $spec 1.0 180.0] dx dy
approx "polar 180 deg = pull -x" $dx -3.0 1e-6

puts "\nfree flight matches the parabola (no obstacle in the way):"
# launch straight up from a high anchor: apex = v^2/(2g)
set s2 [dict merge $spec {anchor_x 0.0 anchor_y 0.0 target_x 12.0 target_y -6.0 field_hy 40 max_t 3.0}]
set r [sling_sim::simulate $s2 0.0 10.0]
set ymax -1e9
foreach y [dict get $r y] { if { $y > $ymax } { set ymax $y } }
# Box2D's soft-step integrator lands a few percent under the analytic apex;
# that bias is identical in the preview, the replay and the loader sweep,
# which is the property that matters. Bound it rather than pin it.
approx "apex ~ v^2/2g = 5.10 dva (within 5%)" $ymax 5.10 0.26
check  "outcome is a miss (hits the ground)" {[dict get $r outcome] eq "miss"}
check  "records t/x/y of equal length" {
    [llength [dict get $r t]] == [llength [dict get $r x]] &&
    [llength [dict get $r x]] == [llength [dict get $r y]]
}
check  "first sample is the anchor" {[lindex [dict get $r x] 0] == 0.0 && [lindex [dict get $r y] 0] == 0.0}

puts "\nspec gravity is honoured (per-step correction):"
set g0 [dict merge $s2 {gravity -4.9}]
set r0 [sling_sim::simulate $g0 0.0 10.0]
set ymax0 -1e9
foreach y [dict get $r0 y] { if { $y > $ymax0 } { set ymax0 $y } }
approx "half gravity -> twice the apex (ratio)" [expr {$ymax0/$ymax}] 2.0 0.1

puts "\nthe default target is reachable, and the sweep finds it:"
set sw [sling_sim::sweep $spec -n_frac 8 -n_angle 36]
check "some pulls hit" {[dict get $sw n_hits] > 0}
check "best_frac reported" {[dict get $sw best_frac] ne ""}
lassign [sling_sim::pull_from_polar $spec [dict get $sw best_frac] [dict get $sw best_angle]] dx dy
set rb [sling_sim::preview $spec $dx $dy]
check "the best pull replays as a hit" {[dict get $rb outcome] eq "hit"}
check "the hit contact is the bucket floor" {[dict get $rb first_hit] eq "target_b" || [lsearch -index 1 [dict get $rb contacts] target_b] >= 0}
puts "   [dict get $sw n_hits]/[dict get $sw n_total] pulls hit; best frac [dict get $sw best_frac] angle [dict get $sw best_angle]"

puts "\nthe bucket cannot be 'hit' from the outside:"
# geometry: floor inset between the walls, base under the floor
lassign [sling_sim::target_geometry $spec] fl wl wr bs
lassign $fl fx fy fw fh; lassign $wl lx ly lw lh; lassign $bs bx by bw bh
set t [dict get $spec wall_t]
approx "floor is inset by one wall thickness each side" $fw [expr {[dict get $spec target_w] - 2*$t}] 1e-9
check  "walls reach below the floor to the bottom of the base" \
    {abs(($ly - $lh/2.0) - ($by - $bh/2.0)) < 1e-9}
check  "base spans the full width under the floor" {$bw == [dict get $spec target_w] && $by < $fy}
# a ball rising into the underside of the bucket touches the base, not the floor
set cx [dict get $spec target_x]; set cy [dict get $spec target_y]
set under [dict merge $spec [list anchor_x $cx anchor_y [expr {$cy - $t/2.0 - [dict get $spec base_h] - [dict get $spec ball_r] - 0.05}] gravity -0.0001 max_t 1.0]]
set ru [sling_sim::simulate $under 0.0 3.0]
check  "from below: first contact is the base"  {[dict get $ru first_hit] eq "target_base"}
check  "from below: not a hit"                  {[dict get $ru outcome] ne "hit"}
# a ball sliding in from the side at floor height touches a wall, not the floor
set side [dict merge $spec [list anchor_x [expr {$cx + [dict get $spec target_w]/2.0 + [dict get $spec ball_r] + 0.5}] anchor_y $cy gravity -0.0001 max_t 1.0]]
set rs [sling_sim::simulate $side -3.0 0.0]
# the base's end and the wall's outer face are flush, so either may be
# reported first; what matters is that the floor is not
check  "from the side: first contact is the shell (wall or base)" \
    {[dict get $rs first_hit] in {target_r target_base}}
check  "from the side: the floor is never touched" \
    {[lsearch -index 1 [dict get $rs contacts] target_b] < 0}
check  "from the side: not a hit"               {[dict get $rs outcome] ne "hit"}
# ...and a ball dropped into the mouth is a hit
set drop [dict merge $spec [list anchor_x $cx anchor_y [expr {$cy + 3.0}] max_t 3.0]]
set rd [sling_sim::simulate $drop 0.0 0.0]
check  "dropped into the mouth: hit"            {[dict get $rd outcome] eq "hit"}
lassign [sling_sim::target_mouth $spec] mlo mhi
check  "...with its centre inside the mouth"    {[dict get $rd land_x] > $mlo && [dict get $rd land_x] < $mhi}

puts "\ndeterminism: the same release twice gives the same path:"
set ra [sling_sim::preview $spec $dx $dy]
check "identical x" {[dict get $ra x] eq [dict get $rb x]}
check "identical y" {[dict get $ra y] eq [dict get $rb y]}

puts "\nan obstacle in the path changes the outcome:"
# a wall between anchor and target, from the ground to the top of the field
# so no lob clears it
set sx [expr {([dict get $spec anchor_x] + [dict get $spec target_x])/2.0}]
set so [dict merge $spec [list obstacles [list [list $sx 0.0 0.3 [expr {2.0*[dict get $spec field_hy]}] 0.0 0.3]]]]
set ro [sling_sim::preview $so $dx $dy]
check "blocked flight is no longer a hit" {[dict get $ro outcome] ne "hit"}
check "the obstacle was contacted" {[lsearch -index 1 [dict get $ro contacts] obs_0] >= 0}

puts "\nstimdg round trip:"
set g [dg_create]
sling_sim::specs_to_stimdg $g [list $spec $so]
check "scalar column present" {[dl_exists $g:target_x]}
check "two rows" {[dl_length $g:target_x] == 2}
check "obstacle columns nested" {[dl_length $g:obs_x] == 2 && [dl_length [dl_get $g:obs_x 0]] == 0 && [dl_length [dl_get $g:obs_x 1]] == 1}
set back [sling_sim::spec_from_stimdg $g 1]
approx "target_x survives" [dict get $back target_x] [dict get $spec target_x] 1e-5
check  "obstacle survives" {[llength [dict get $back obstacles]] == 1}
lassign [lindex [dict get $back obstacles] 0] ox oy ow oh oa orr
approx "obstacle x" $ox $sx 1e-4
approx "obstacle restitution" $orr 0.3 1e-5
set rr [sling_sim::preview $back $dx $dy]
check "rebuilt spec reproduces the blocked outcome" {[dict get $rr outcome] eq [dict get $ro outcome]}
dg_delete $g

puts ""
if { $::nfail } { puts "FAILURES: $::nfail"; exit 1 }
puts "all checks passed"
