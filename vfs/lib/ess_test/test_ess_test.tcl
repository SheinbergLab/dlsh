# test_ess_test.tcl --
#   Self-test AND worked example for the ess_test harness. Uses the real
#   pursuit/ballistic system (under ess_test's systems_root, default
#   ~/systems/ess) as the fixture, re-expressing this session's ad-hoc
#   harnesses as short ess_test tests. This is the pattern to copy when you
#   write tests for a NEW loader/stim.
#
#   Run headless from any dlsh shell:
#       dlsh /Users/sheinb/src/dlsh/vfs/lib/ess_test/test_ess_test.tcl
#   or, once ess_test is baked into dlsh.zip:
#       dlsh -e 'package require ess_test; source .../test_ess_test.tcl'
#
#   Bootstrap: source the ON-DISK ess_test.tcl directly. auto_path front-load
#   does NOT reliably shadow a copy already in dlsh.zip (the zip's package
#   ifneeded is scanned first), so we source the sibling file to test what we
#   just edited -- same lesson as test_launch_sim.tcl.

if {![info exists ::__ess_test_loaded]} {
    catch { source /usr/local/dlsh/dlsh_setup.tcl }
    package require dlsh
    catch {package forget ess_test}
    catch {namespace delete ::ess_test}
    source [file join [file dirname [info script]] ess_test.tcl]
    set ::__ess_test_loaded 1
}

namespace import ess_test::assert

# The SPEC's ergonomics example, as one defaults dict so each test varies only
# what it needs. Every one of the loader's 24 params is present here.
set BALLISTIC_DEFAULTS {
    nr 2  gravities {9.8 0 -9.8}  launch_params {}  fit_halfextent 10.0
    fix_r 0.2  target_type pulsed_patch  spot_color {0.9 0.9 0.9}
    patch_size 16 dot_density 8 pointsize 2.5 target_speed_ratio 1.0
    target_lifetime 0.15 surround_speed_dva_sec 3 bg_lifetime 0.1
    target_diameter 2.5 surround_alpha 1 target_color {0.6 0.6 0.6}
    surround_color {0.6 0.6 0.6} coherence 1 surround_coherence 0
    cue_color none internal_rotation_deg 0
    coh_params {coh_profile gate coh_lo 0 coh_onset_frac 0.30 coh_dur_frac 0.40}
    probe_params {probe_type color probe_time_frac {0.45 0.55 0.65} catch_frac 0.2
                  probe_frames 2 probe_color_a {0.5 0.8 0.5} probe_color_b {0.8 0.5 0.5}}
}

# ==========================================================================
# TIER 1 -- loader / stimdg validation (pure dlsh, no stim2)
# ==========================================================================
ess_test::test "loader: build stimdg" {
    # Sourcing INSIDE ::ess + calling loaders_init would surface the
    # sample_fit namespace-resolution bug (a global-scope source hid it).
    set names [ess_test::load_loaders pursuit ballistic]
    assert {[lsearch $names setup_ballistic] >= 0} "registered setup_ballistic loader"

    ess_test::loader_defaults setup_ballistic $::BALLISTIC_DEFAULTS
    # vary nothing -> full defaults; run the body (this is where sample_fit is
    # called by its absolute ::ess::pursuit::ballistic::sample_fit name).
    set ::g [ess_test::run_loader {}]

    # nr(2) x gravities(3) x coh_depth(1) x coh_dur(1) = 6 trials
    assert {[dl_length $::g:stimtype] == 6} "6 trials (2 x {9.8 0 -9.8})"
    assert {[dl_length $::g:launcher_x] == 6} "launcher_x column present, 6 rows"
    assert {[dl_exists $::g:vx] && [dl_exists $::g:vy] && [dl_exists $::g:gravity]} \
        "analytic scalars (vx/vy/gravity) present for the stim replay"
}

ess_test::test "loader: flight-fraction -> ms timing" {
    # coh_on_ms must equal coh_onset_frac(0.30) * flight_ms for every row --
    # the timing bug class (a fixed ms onset would not scale with land_time).
    set bad 0
    for {set i 0} {$i < [dl_length $::g:coh_on_ms]} {incr i} {
        set flight_ms [expr {[dl_get $::g:land_time $i]*1000.0}]
        set want [expr {0.30*$flight_ms}]
        if {![ess_test::approx [dl_get $::g:coh_on_ms $i] $want 1.0]} { incr bad }
    }
    assert {$bad == 0} "coh_on_ms == 0.30 * flight_ms for all rows"

    # the coherence dip width is coh_dur_frac(0.40) of flight
    set bad 0
    for {set i 0} {$i < [dl_length $::g:coh_off_ms]} {incr i} {
        set width [expr {[dl_get $::g:coh_off_ms $i]-[dl_get $::g:coh_on_ms $i]}]
        set want  [expr {0.40*[dl_get $::g:land_time $i]*1000.0}]
        if {![ess_test::approx $width $want 1.0]} { incr bad }
    }
    assert {$bad == 0} "coh dip width == 0.40 * flight_ms for all rows"
}

ess_test::test "loader: probe lands inside the coherence dip" {
    # for probe-present rows, probe_time_ms must fall within [coh_on, coh_off]
    set checked 0
    for {set i 0} {$i < [dl_length $::g:probe_present]} {incr i} {
        if {![dl_get $::g:probe_present $i]} continue
        incr checked
        assert {[ess_test::in_range [dl_get $::g:probe_time_ms $i] \
                    [dl_get $::g:coh_on_ms $i] [dl_get $::g:coh_off_ms $i]]} \
            "row $i probe inside dip"
    }
    assert {$checked > 0} "at least one probe-present row existed to check"
}

# ==========================================================================
# TIER 2 -- headless per-frame stim-driver validation (capturing stubs)
# ==========================================================================
# We install the stubs once and source the stim file; each test then rebuilds a
# trial with nexttrial and drives frames. NOTE the honest boundary: these check
# the NUMBERS the stim hands stim2 (position/direction/coherence/events), never
# how it looks. Invisibility/masking/rendering still need real stim2 + eyes.
ess_test::stub_stim2
ess_test::stim_source pursuit ballistic

# helper: (re)build trial $id's scene, then start playback from t=0
proc play_trial {id} {
    ess_test::set_time 0
    set fx [dl_get stimdg:fix_x $id]
    set fy [dl_get stimdg:fix_y $id]
    set fr [dl_get stimdg:fix_r $id]
    nexttrial $id $fx $fy $fr    ;# resetObjList clears prior prescripts/maps
    ess_test::clear_captures
    target_on
    pursuit_start
    return [dl_get stimdg:land_time $id]
}

ess_test::test "stim(patch): parabola position via mask offset" {
    set dur [play_trial 0]       ;# trial 0 is a pulsed_patch, gravity 9.8
    ess_test::play -dur $dur -dt 0.016

    # patch position is written as mask offset = pos / patch_size
    set patch [dl_get stimdg:patch_size 0]
    set oxs [ess_test::values motionpatch_maskoffset dots_target 0]
    assert {[llength $oxs] > 5} "target mask offset written every frame"
    set x_end [expr {[lindex $oxs end]*$patch}]
    assert {[ess_test::approx $x_end [dl_get stimdg:land_x 0] 0.1]} \
        "final target x == land_x (parabola replayed to the endpoint)"
}

ess_test::test "stim(patch): motion_on fires once at playback start" {
    play_trial 0
    ess_test::play -dur [dl_get stimdg:land_time 0] -dt 0.016
    set on [ess_test::events pursuit/motion_on]
    assert {[llength $on] == 1} "exactly one pursuit/motion_on event"
    assert {[ess_test::approx [ess_test::event_time pursuit/motion_on] 16.7 2.0]} \
        "motion_on at the first driver frame"
}

ess_test::test "stim(patch): internal direction tracks the tangent" {
    play_trial 0                 ;# gravity 9.8 -> vy-g*t changes sign
    ess_test::play -dur [dl_get stimdg:land_time 0] -dt 0.016
    set dirs [ess_test::values motionpatch_direction dots_target 0]
    assert {[llength $dirs] > 5} "direction written each frame"
    set lo [tcl::mathfunc::min {*}$dirs]
    set hi [tcl::mathfunc::max {*}$dirs]
    assert {[expr {$hi-$lo}] > 0.1} "tangent heading rotates over the arc (spread [format %.2f [expr {$hi-$lo}]] rad)"
}

ess_test::test "stim(patch): coherence dips to ~0 mid-gate" {
    play_trial 0                 ;# coh_profile gate, coh_lo 0, peak 1
    ess_test::play -dur [dl_get stimdg:land_time 0] -dt 0.016
    set cohs [ess_test::values motionpatch_coherence dots_target 0]
    assert {[llength $cohs] > 5} "coherence written each frame"
    assert {[tcl::mathfunc::max {*}$cohs] > 0.9} "coherence peaks near 1 outside the dip"
    assert {[tcl::mathfunc::min {*}$cohs] < 0.05} "coherence hits ~0 in the gate (the invisibility window)"
    # and the coherence landmark events fired
    assert {[llength [ess_test::events pursuit/coh_event]] > 0} "coh_event landmark(s) fired"
}

ess_test::test "stim(patch): probe fires inside the dip, at its scheduled time" {
    # find a probe-present patch row
    set prow -1
    for {set i 0} {$i < [dl_length stimdg:probe_present]} {incr i} {
        if {[dl_get stimdg:probe_present $i] && [dl_get stimdg:probe_type $i] ne "none"} { set prow $i; break }
    }
    assert {$prow >= 0} "found a probe-present row (row $prow)"

    play_trial $prow
    ess_test::play -dur [dl_get stimdg:land_time $prow] -dt 0.016
    set pon [ess_test::events pursuit/probe_on]
    assert {[llength $pon] == 1} "exactly one pursuit/probe_on"
    # probe_on relative to motion_on == the scheduled probe_time_ms (+- 1 frame)
    set rel [expr {[ess_test::event_time pursuit/probe_on]-[ess_test::event_time pursuit/motion_on]}]
    assert {[ess_test::approx $rel [dl_get stimdg:probe_time_ms $prow] 20.0]} \
        "probe_on at probe_time_ms after motion_on (rel [format %.0f $rel] ms)"
}

ess_test::test "stim(spot): polygon translated along the parabola" {
    # rebuild the design with a simple-spot target to exercise translateObj
    set ::gs [ess_test::run_loader [dict merge $::BALLISTIC_DEFAULTS {target_type spot}]]
    set dur [play_trial 0]
    ess_test::play -dur $dur -dt 0.016
    set xs {}
    foreach r [ess_test::captured translateObj target] { lappend xs [lindex [dict get $r args] 0] }
    assert {[llength $xs] > 5} "spot translated every frame"
    assert {[ess_test::approx [lindex $xs end] [dl_get stimdg:land_x 0] 0.1]} \
        "final spot x == land_x"
    assert {[llength [ess_test::events pursuit/motion_on]] == 1} "spot also fires motion_on once"
}

exit [ess_test::summary]
