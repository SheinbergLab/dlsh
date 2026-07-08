# test_colorcal.tcl -- headless sanity tests for the colorcal core.
# Run:  tclsh test_colorcal.tcl   (adds this dir to auto_path; needs Tcl only)

set here [file normalize [file dirname [info script]]]
set auto_path [linsert $auto_path 0 [file dirname $here]]
package require colorcal

set pass 0; set fail 0
proc ok {name cond} {
    if {$cond} { incr ::pass } else { incr ::fail; puts "FAIL: $name" }
}
proc approx {a b {tol 1e-4}} { expr {abs($a-$b) < $tol} }

# 1. EOTF round-trips.
foreach c {0.0 0.04 0.2 0.5 0.9 1.0} {
    ok "srgb roundtrip $c" [approx [colorcal::enc [colorcal::lin $c]] $c]
}

# 2. sRGB gray luminance == lin(gray) (weights sum to 1).
ok "gray luminance" [approx [colorcal::luminance {0.5 0.5 0.5}] [colorcal::lin 0.5]]

# 3. Equiluminant pair: both poles equal luminance at balance 0.
set pr [colorcal::isolum_pair 0.5 0.16]
set Ya [colorcal::luminance [dict get $pr a]]
set Yb [colorcal::luminance [dict get $pr b]]
ok "isolum equal Y" [approx $Ya $Yb]
ok "isolum Y == pedestal" [approx $Ya [colorcal::lin 0.5]]

# 4. Balance tilt splits luminance by 2*b*y0 (a brighter, b dimmer).
set b 0.2
set pr2 [colorcal::isolum_pair 0.5 0.16 -balance $b]
set Ya2 [colorcal::luminance [dict get $pr2 a]]
set Yb2 [colorcal::luminance [dict get $pr2 b]]
set y0 [colorcal::lin 0.5]
ok "balance split" [approx [expr {$Ya2 - $Yb2}] [expr {2.0*$b*$y0}]]

# 5. max_chroma predicts the clipping threshold; at/below it the pair stays
#    equiluminant, just above it it does not.
set mc [colorcal::max_chroma 0.5]
set below [colorcal::isolum_pair 0.5 [expr {$mc*0.98}]]
ok "equilum below max_chroma" \
    [approx [colorcal::luminance [dict get $below a]] [colorcal::luminance [dict get $below b]]]
set above [colorcal::isolum_pair 0.5 [expr {$mc + 0.1}]]
ok "clip breaks equilum above max_chroma" \
    [expr {![approx [colorcal::luminance [dict get $above a]] [colorcal::luminance [dict get $above b]]]}]

# 6. Alternate display weights change the R/G balance ratio k.
colorcal::display_define testmon -eotf gamma:2.2 -weights {0.30 0.60 0.10}
set prm [colorcal::isolum_pair 0.5 0.15 -display testmon]
ok "equilum on custom display" \
    [approx [colorcal::luminance [dict get $prm a] testmon] \
            [colorcal::luminance [dict get $prm b] testmon]]

# 7. Profile round-trips and regenerates at a boosted saturation.
set p [colorcal::profile_new -observer test -display srgb]
colorcal::profile_set p probe_a {kind isolum pole a pedestal 0.5 chroma 0.16 balance 0.0 display srgb}
ok "profile_rgb matches construction" \
    [expr {[colorcal::profile_rgb $p probe_a] eq [dict get $pr a]}]
set boosted [colorcal::profile_rgb $p probe_a -saturation 0.4]
ok "profile boosted saturation equilum" \
    [approx [colorcal::luminance $boosted] [colorcal::lin 0.5]]

puts "colorcal: $pass passed, $fail failed"
exit [expr {$fail ? 1 : 0}]
