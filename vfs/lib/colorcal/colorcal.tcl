# colorcal.tcl -- frontend-independent color/luminance calibration core.
#
# Pure Tcl (+ dlsh where noted). NO GL, NO stim2 commands -- so it is
# headless-testable and reusable by stim, control, and analysis alike. The
# stim2/graphics frontend (HFP calibration, meter patches, workspace UI) lives
# in colorcal_stim.tcl.
#
# See README.md for the design note. Key invariant: equiluminant construction
# is done in LINEAR light using a DISPLAY MODEL (EOTF + luminance weights); the
# sRGB/Rec709 default is "about right" for a nominal display but must be
# replaced by a measured/hardware profile to be exact on a real monitor.

package require Tcl 8.6-

namespace eval ::colorcal {
    variable version 0.1

    # Display models, keyed by name. Each: {eotf <spec> weights {wr wg wb}}.
    #   eotf:  srgb | gamma:<g> | lut:<dg>   (lut = TODO, measured 1D LUT)
    # Default: sRGB EOTF + Rec709 luminance weights (a NOMINAL standard display).
    variable displays
    array set displays {}
    set displays(srgb) {eotf srgb weights {0.2126 0.7152 0.0722}}
    set displays(default) {eotf srgb weights {0.2126 0.7152 0.0722}}

    namespace export lin enc luminance isolum_pair max_chroma in_gamut
}

# ------------------------------------------------------------------
# Display model
# ------------------------------------------------------------------

# Define / override a display model. weights are RELATIVE primary luminances
# (need not sum to 1; ratios are what matter for equiluminance).
proc ::colorcal::display_define {name args} {
    variable displays
    set m {eotf srgb weights {0.2126 0.7152 0.0722}}
    foreach {k v} $args {
        switch -- $k {
            -eotf    { dict set m eotf $v }
            -weights { dict set m weights $v }
            default  { error "display_define: unknown option $k" }
        }
    }
    set displays($name) $m
    return $name
}

proc ::colorcal::_model {display} {
    variable displays
    if {![info exists displays($display)]} {
        error "colorcal: unknown display model '$display'"
    }
    return $displays($display)
}

# Inverse EOTF: gamma-encoded code value [0,1] -> linear light [0,1].
proc ::colorcal::lin {c {display srgb}} {
    set eotf [dict get [_model $display] eotf]
    if {$eotf eq "srgb"} {
        return [expr {$c <= 0.04045 ? $c/12.92 : pow(($c+0.055)/1.055, 2.4)}]
    } elseif {[regexp {^gamma:(.+)$} $eotf -> g]} {
        return [expr {pow((($c) < 0 ? 0 : $c), double($g))}]
    }
    # TODO: lut:<dg> -- interpolate a measured 1D LUT.
    error "colorcal::lin: unsupported eotf '$eotf'"
}

# EOTF: linear [0,1] -> code value [0,1]. Clamps out-of-gamut input.
proc ::colorcal::enc {c {display srgb}} {
    if {$c < 0.0} { set c 0.0 } elseif {$c > 1.0} { set c 1.0 }
    set eotf [dict get [_model $display] eotf]
    if {$eotf eq "srgb"} {
        return [expr {$c <= 0.0031308 ? 12.92*$c : 1.055*pow($c, 1.0/2.4) - 0.055}]
    } elseif {[regexp {^gamma:(.+)$} $eotf -> g]} {
        return [expr {pow($c, 1.0/double($g))}]
    }
    error "colorcal::enc: unsupported eotf '$eotf'"
}

# Relative luminance Y of a code-value RGB triplet on the given display.
proc ::colorcal::luminance {rgb {display srgb}} {
    lassign $rgb r g b
    lassign [dict get [_model $display] weights] wr wg wb
    expr {$wr*[lin $r $display] + $wg*[lin $g $display] + $wb*[lin $b $display]}
}

# ------------------------------------------------------------------
# Equiluminant construction (red/green, L-M cardinal axis)
# ------------------------------------------------------------------

# Two equiluminant poles about a gray pedestal, modulated along red<->green so
# the net luminance change cancels (dG_lin = -(wR/wG)*dR_lin). Returns
# {a {r g b} b {r g b}} with a = red-ward (+axis), b = green-ward (-axis).
#
# -balance b : observer-null tilt. Adds gray +b*y0 to a and -b*y0 to b, i.e. a
#   predicted luminance split of 2*b*y0. This is the residual correction the HFP
#   null produces; on a measured/hardware display it is ~0.
proc ::colorcal::isolum_pair {pedestal chroma args} {
    set display srgb
    set balance 0.0
    foreach {k v} $args {
        switch -- $k {
            -display { set display $v }
            -balance { set balance  $v }
            default  { error "isolum_pair: unknown option $k" }
        }
    }
    set y0 [lin $pedestal $display]
    lassign [dict get [_model $display] weights] wr wg wb
    set k   [expr {double($wr)/double($wg)}]
    set off [expr {$balance * $y0}]
    set a [list [enc [expr {$y0 + $chroma    + $off}] $display] \
                [enc [expr {$y0 - $k*$chroma + $off}] $display] \
                [enc [expr {$y0             + $off}] $display]]
    set b [list [enc [expr {$y0 - $chroma    - $off}] $display] \
                [enc [expr {$y0 + $k*$chroma - $off}] $display] \
                [enc [expr {$y0             - $off}] $display]]
    return [dict create a $a b $b]
}

# Largest chroma before ANY linear channel of either pole leaves [0,1]. Past
# this the enc() clamp silently destroys equiluminance (the mp_postcue footgun),
# so callers/UI should cap saturation here (or raise the pedestal for headroom).
proc ::colorcal::max_chroma {pedestal args} {
    set display srgb
    set balance 0.0
    foreach {k v} $args {
        switch -- $k {
            -display { set display $v }
            -balance { set balance  $v }
        }
    }
    set y0 [lin $pedestal $display]
    lassign [dict get [_model $display] weights] wr wg wb
    set k   [expr {double($wr)/double($wg)}]
    set off [expr {$balance * $y0}]
    # a.r<=1 ; a.g>=0 ; b.r>=0 ; b.g<=1
    set lims [list [expr {1.0 - $y0 - $off}] \
                   [expr {($y0 + $off)/$k}] \
                   [expr {$y0 - $off}] \
                   [expr {(1.0 - $y0 + $off)/$k}]]
    set m [lindex $lims 0]
    foreach l $lims { if {$l < $m} { set m $l } }
    return [expr {$m < 0 ? 0.0 : $m}]
}

proc ::colorcal::in_gamut {linear_rgb} {
    foreach c $linear_rgb { if {$c < 0.0 || $c > 1.0} { return 0 } }
    return 1
}

# 8-bit (0..255) form of a code-value triplet -- convenience for reports/meters.
proc ::colorcal::rgb8 {rgb} { lmap c $rgb {expr {int(round($c*255.0))}} }

# ==================================================================
# Profile store -- STUB. A profile pins named colors to (observer x display)
# and stores the GENERATIVE spec so colors regenerate at any saturation and
# travel between systems. Dict-backed for now; dg/dgz + dserv are TODO.
# ==================================================================

# spec forms a color can take:
#   {kind isolum pole a|b pedestal P chroma C balance B display D}
#   {kind rgb    value {r g b}}
proc ::colorcal::profile_new {args} {
    set p [dict create observer {} display srgb colors {}]
    foreach {k v} $args {
        switch -- $k {
            -observer { dict set p observer $v }
            -display  { dict set p display  $v }
            default   { error "profile_new: unknown option $k" }
        }
    }
    return $p
}

proc ::colorcal::profile_set {pVar name spec} {
    upvar 1 $pVar p
    dict set p colors $name $spec
    return $p
}

# Resolve a named color to a code-value {r g b}. -saturation overrides the spec
# chroma (e.g. boosted meter patches) without mutating the stored spec.
proc ::colorcal::profile_rgb {profile name args} {
    set sat {}
    foreach {k v} $args { if {$k eq "-saturation"} { set sat $v } }
    if {![dict exists $profile colors $name]} {
        error "colorcal: no color '$name' in profile"
    }
    set spec [dict get $profile colors $name]
    switch -- [dict get $spec kind] {
        rgb { return [dict get $spec value] }
        isolum {
            set chroma [expr {$sat ne "" ? $sat : [dict get $spec chroma]}]
            set pair [isolum_pair [dict get $spec pedestal] $chroma \
                        -display [dict get $spec display] \
                        -balance [dict get $spec balance]]
            return [dict get $pair [dict get $spec pole]]
        }
        default { error "colorcal: unknown color kind [dict get $spec kind]" }
    }
}

# Transport. Plain-dict form is dserv/JSON-friendly as-is.
proc ::colorcal::profile_to_dict {profile} { return $profile }
proc ::colorcal::profile_from_dict {d}      { return $d }

# TODO: dg-backed persistence (mirror mp_sim's dgz round-trip) and dserv
# publish/subscribe of the active profile.
proc ::colorcal::profile_save {profile file} {
    error "colorcal::profile_save: not yet implemented (TODO: dg/dgz)"
}
proc ::colorcal::profile_load {file} {
    error "colorcal::profile_load: not yet implemented (TODO: dg/dgz)"
}

package provide colorcal $::colorcal::version
