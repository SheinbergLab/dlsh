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
    # `default` is the ACTIVE model used when no -display is given. It starts as
    # sRGB ("about right") and monitor.tcl overrides it via set_default with the
    # measured primaries, so demos with no -display arg get the calibrated
    # display on a calibrated box and sRGB elsewhere.
    set displays(default) {eotf srgb weights {0.2126 0.7152 0.0722}}
    variable default_calibrated 0

    namespace export lin enc luminance isolum_pair max_chroma in_gamut
}

# ------------------------------------------------------------------
# Display model
# ------------------------------------------------------------------

# Define / override a display model. weights are RELATIVE primary luminances
# (need not sum to 1; ratios are what matter for equiluminance).
proc ::colorcal::display_define {name args} {
    variable displays
    # -chromaticities {xr yr xg yg xb yb} enables the cone/DKL backend;
    # -white {xw yw} is the white point. Both optional (weights-only otherwise).
    set m {eotf srgb weights {0.2126 0.7152 0.0722}}
    foreach {k v} $args {
        switch -- $k {
            -eotf           { dict set m eotf $v }
            -weights        { dict set m weights $v }
            -chromaticities { dict set m chroma $v }
            -white          { dict set m white $v }
            default         { error "display_define: unknown option $k" }
        }
    }
    set displays($name) $m
    return $name
}

# Does this display carry the colorimetric data (primary chromaticities) needed
# for the cone/DKL backend? If not, only the weights-only backend is available.
proc ::colorcal::_has_cone {display} {
    expr {[dict exists [_model $display] chroma]}
}

# Set the ACTIVE default display model (what every proc uses when no -display
# is given). monitor.tcl calls this once at startup with the measured display.
# -label is optional provenance. Marks the default calibrated so consumers can
# tell measured from the sRGB fallback.
proc ::colorcal::set_default {args} {
    variable default_calibrated
    variable default_label
    set label ""
    set fwd {}
    foreach {k v} $args {
        if {$k eq "-label"} { set label $v } else { lappend fwd $k $v }
    }
    display_define default {*}$fwd
    set default_calibrated 1
    set default_label $label
    return default
}

# 1 if monitor.tcl has installed a measured display; 0 if still sRGB fallback.
proc ::colorcal::default_calibrated {} {
    variable default_calibrated
    return $default_calibrated
}
proc ::colorcal::default_label {} {
    variable default_label
    return [expr {[info exists default_label] ? $default_label : ""}]
}

proc ::colorcal::_model {display} {
    variable displays
    if {![info exists displays($display)]} {
        error "colorcal: unknown display model '$display'"
    }
    return $displays($display)
}

# Inverse EOTF: gamma-encoded code value [0,1] -> linear light [0,1].
proc ::colorcal::lin {c {display default}} {
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
proc ::colorcal::enc {c {display default}} {
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
proc ::colorcal::luminance {rgb {display default}} {
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
    set display default
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
    set display default
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

# ------------------------------------------------------------------
# Generalized picker:  color = pedestal + chromatic DIRECTION + magnitude
#
# Direction lives in the display's ISOLUMINANT PLANE. Red/green is no longer
# special -- it's one axis. Two tiers, chosen by what the display model carries:
#
#   weights-only (primary luminances): axes are DISPLAY-RGB luminance-neutral
#     directions -- honest and useful, but NOT cone-cardinal.
#       -axis rg : red up / green down, blue fixed         (dir = {1, -wR/wG, 0})
#       -axis by : blue up / yellow down, luminance-neutral (dir = {m, m, 1})
#       -azimuth <deg> : cos*rg + sin*by  (display-space angle; documented approx)
#
#   cone/DKL (needs -chromaticities): true L-M / S-(L+M) cardinal axes and
#     cone-contrast magnitude -- the rig/observer-PORTABLE representation. STUB.
#
# `contrast` is the magnitude (the portable "delta"): code-value chroma along
# the axis in weights-only mode; cone contrast in cone mode.
# ------------------------------------------------------------------

# Luminance-neutral direction {dr dg db} in code space for an axis/azimuth.
proc ::colorcal::_direction {display axis {azimuth ""}} {
    lassign [dict get [_model $display] weights] wr wg wb
    set k  [expr {double($wr)/double($wg)}]
    set e1 [list 1.0 [expr {-$k}] 0.0]                        ;# red-green
    set m  [expr {-double($wb)/(double($wr)+double($wg))}]
    set e2 [list $m $m 1.0]                                   ;# blue-yellow (display)
    if {$azimuth ne ""} {
        set th [expr {$azimuth * 3.14159265358979 / 180.0}]
        return [lmap a $e1 b $e2 {expr {cos($th)*$a + sin($th)*$b}}]
    }
    switch -- $axis {
        rg      { return $e1 }
        by - s  { return $e2 }
        default { error "colorcal: unknown axis '$axis' (use rg|by or -azimuth)" }
    }
}

proc ::colorcal::_parse_dir_opts {argsVar} {
    upvar 1 $argsVar a
    set o {display default axis rg azimuth {} contrast 0.0 pole + balance 0.0 space auto}
    foreach {k v} $a {
        switch -- $k {
            -display  { dict set o display $v }
            -axis     { dict set o axis $v }
            -azimuth  { dict set o azimuth $v }
            -contrast { dict set o contrast $v }
            -pole     { dict set o pole $v }
            -balance  { dict set o balance $v }
            -space    { dict set o space $v }
            default   { error "colorcal: unknown option $k" }
        }
    }
    return $o
}

# One equiluminant color (code-value {r g b}). -pole +|a (default) or -|b picks
# the sign of the excursion (and of the balance tilt).
proc ::colorcal::color {pedestal args} {
    set o [_parse_dir_opts args]
    set display [dict get $o display]
    set s [expr {([dict get $o pole] in {- b}) ? -1.0 : 1.0}]
    set space [dict get $o space]
    # cone/DKL is opt-in ONLY (-space cone) until the backend lands, so a
    # display that merely carries chromaticities never routes to the stub and
    # errors. When cone is implemented, `auto` can prefer it on such displays.
    if {$space eq "cone"} {
        return [_cone_color $pedestal $o $s]
    }
    set y0  [lin $pedestal $display]
    set dir [_direction $display [dict get $o axis] [dict get $o azimuth]]
    set c   [dict get $o contrast]
    set off [expr {$s * [dict get $o balance] * $y0}]
    return [lmap d $dir {enc [expr {$y0 + $s*$c*$d + $off}] $display}]
}

# The +/- pole pair about the pedestal (generalizes isolum_pair to any axis).
proc ::colorcal::pair {pedestal args} {
    return [dict create a [color $pedestal {*}$args -pole +] \
                        b [color $pedestal {*}$args -pole -]]
}

# Largest contrast before either pole clips (breaking equiluminance) for the
# chosen axis/azimuth. Generalizes max_chroma.
proc ::colorcal::max_contrast {pedestal args} {
    set o [_parse_dir_opts args]
    set display [dict get $o display]
    set y0  [lin $pedestal $display]
    set dir [_direction $display [dict get $o axis] [dict get $o azimuth]]
    set b   [dict get $o balance]
    set best Inf
    foreach s {1.0 -1.0} {
        set base [expr {$y0 + $s*$b*$y0}]
        foreach d $dir {
            set sd [expr {$s*$d}]
            if {abs($sd) < 1e-12} continue
            set lim [expr {$sd > 0 ? (1.0-$base)/$sd : (0.0-$base)/$sd}]
            if {$lim < $best} { set best $lim }
        }
    }
    return [expr {$best < 0 ? 0.0 : $best}]
}

# ---- cone/DKL backend (STUB) --------------------------------------
# From primary chromaticities + luminances -> RGB->XYZ; then XYZ->LMS via cone
# fundamentals -> RGB->LMS. A DKL azimuth becomes an LMS-contrast direction;
# `contrast` is cone contrast. This is the rig/observer-portable path.
proc ::colorcal::_rgb2lms {display} {
    error "colorcal cone/DKL backend: TODO (build RGB->LMS from -chromaticities + cone fundamentals)"
}
proc ::colorcal::_cone_color {pedestal o s} {
    error "colorcal cone/DKL backend: TODO (azimuth/contrast in cone space)"
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

# Persist / restore a profile. A profile is a plain Tcl dict, so v0.1 writes it
# verbatim (also valid as a dserv string / JSON-ish). dg/dgz + dserv
# publish/subscribe are a later transport upgrade, not required for the
# monitor.tcl -> set_default -> generate path.
proc ::colorcal::profile_save {profile file} {
    set fp [open $file w]; puts $fp $profile; close $fp
    return $file
}
proc ::colorcal::profile_load {file} {
    set fp [open $file r]; set d [read $fp]; close $fp
    return [string trim $d]
}

package provide colorcal $::colorcal::version
