# colorcal_stim.tcl -- stim2/graphics frontend for colorcal.  *** STUB ***
#
# Depends on stim2 (draws), so only load where a display exists. The
# frontend-independent math lives in colorcal.tcl. These procs are to be
# generalized out of examples/motionpatch/mp_postcue.tcl:
#   - the counterphase red/green flicker bar field  -> hfp_run
#   - the two big equiluminant meter squares        -> patches
#   - the float/8-bit/predY reporter                -> report
#
# Kept as signatures + TODOs so the core can land and be tested first.

package require colorcal

namespace eval ::colorcal::stim {
    namespace export hfp_run patches report
}

# Heterochromatic flicker photometry. Present a counterphase red/green field at
# `pedestal` on `display`, reversing at `hz`; the observer adjusts the balance
# tilt until flicker is minimized. Returns the nulled balance value.
#   TODO: port mp_pc_update's flicker branch + a bar-field builder; drive the
#   balance via a workspace adjuster; return on accept.
proc ::colorcal::stim::hfp_run {display pedestal args} {
    error "colorcal::stim::hfp_run: TODO (generalize mp_postcue Flicker Calibration)"
}

# Static verification patches: two big, well-separated squares of the two poles
# for luminance-meter readings. Uses a BRIGHT pedestal for gamut headroom so the
# meter chroma can be strongly saturated while staying equiluminant (see
# colorcal::max_chroma / the mp_postcue clipping footgun).
#   TODO: port mp_pc_photometer_setup/_update (two polygons + live recolor).
proc ::colorcal::stim::patches {spec args} {
    error "colorcal::stim::patches: TODO (generalize mp_postcue Photometer Patches)"
}

# Print a pole pair (float / 8-bit / predicted luminance) for console
# verification against a meter. Pure -- can move to core if no drawing is added.
proc ::colorcal::stim::report {pedestal chroma args} {
    set pair [::colorcal::isolum_pair $pedestal $chroma {*}$args]
    foreach pole {a b} label {red green} {
        set rgb [dict get $pair $pole]
        puts [format "  %s (%-5s) float %s   8bit %s   predY %.4f" \
                  $pole $label [lmap c $rgb {format %.4f $c}] \
                  [::colorcal::rgb8 $rgb] [::colorcal::luminance $rgb]]
    }
    return $pair
}

package provide colorcal_stim 0.1
