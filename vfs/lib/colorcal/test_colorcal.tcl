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
set prof [colorcal::profile_new -observer test -display srgb]
colorcal::profile_set prof probe_a {kind isolum pole a pedestal 0.5 chroma 0.16 balance 0.0 display srgb}
ok "profile_rgb matches construction" \
    [expr {[colorcal::profile_rgb $prof probe_a] eq [dict get $pr a]}]
set boosted [colorcal::profile_rgb $prof probe_a -saturation 0.4]
ok "profile boosted saturation equilum" \
    [approx [colorcal::luminance $boosted] [colorcal::lin 0.5]]

# 8. Generalized picker: general pair(-axis rg) == legacy isolum_pair.
set gp [colorcal::pair 0.5 -axis rg -contrast 0.16]
ok "general rg == isolum_pair a" [expr {[dict get $gp a] eq [dict get $pr a]}]
ok "general rg == isolum_pair b" [expr {[dict get $gp b] eq [dict get $pr b]}]

# 9. Blue/yellow axis is equiluminant too.
set by [colorcal::pair 0.5 -axis by -contrast 0.15]
ok "by axis equal Y" \
    [approx [colorcal::luminance [dict get $by a]] [colorcal::luminance [dict get $by b]]]
ok "by axis Y == pedestal" \
    [approx [colorcal::luminance [dict get $by a]] [colorcal::lin 0.5]]

# 10. Arbitrary azimuth stays equiluminant (weights-only display plane).
foreach az {30 90 135} {
    set p [colorcal::pair 0.5 -azimuth $az -contrast 0.08]
    ok "azimuth $az equiluminant" \
        [approx [colorcal::luminance [dict get $p a]] [colorcal::luminance [dict get $p b]]]
}

# 11. max_contrast(-axis rg) == max_chroma; a single color respects the pole.
ok "max_contrast rg == max_chroma" \
    [approx [colorcal::max_contrast 0.5 -axis rg] [colorcal::max_chroma 0.5]]
ok "single color pole +" [expr {[colorcal::color 0.5 -axis rg -contrast 0.16 -pole +] eq [dict get $pr a]}]

# 12. A chromaticity-bearing display is detected, but -space auto stays
#     weights-only (no surprise error); cone is opt-in only and errors clearly.
colorcal::display_define conemon -eotf gamma:1.0 -weights {22.4 61.4 7.3} \
    -chromaticities {0.64 0.33 0.30 0.60 0.15 0.06}
ok "cone display detected"     [colorcal::_has_cone conemon]
ok "auto stays weights-only"   [expr {![catch {colorcal::color 0.5 -axis rg -contrast 0.1 -display conemon}]}]
ok "explicit cone is stubbed"  [catch {colorcal::color 0.5 -axis rg -contrast 0.1 -display conemon -space cone}]

# 13. Profile save/load round-trips and still regenerates the same triplet.
set tmp [file join [pwd] .colorcal_test_profile]
colorcal::profile_save $prof $tmp
set p2 [colorcal::profile_load $tmp]
file delete $tmp
ok "profile save/load regenerates" \
    [expr {[colorcal::profile_rgb $p2 probe_a] eq [colorcal::profile_rgb $prof probe_a]}]

puts "colorcal: $pass passed, $fail failed"
exit [expr {$fail ? 1 : 0}]
