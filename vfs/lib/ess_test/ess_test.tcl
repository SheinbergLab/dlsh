# ess_test --
#   A headless test harness for ESS loaders / stim files / (partial) protocols.
#
#   Validates the DETERMINISTIC LOGIC and DATA OUTPUT of ESS scripts WITHOUT
#   dserv, OpenGL, the rig, or any hardware. Only dlsh (dl_*/dg_*) and the pure
#   dlsh packages a script requires (launch_sim, mp_sim, ...) are real; dserv /
#   GL / hardware are absent or stubbed. Turns the ad-hoc harnesses we keep
#   hand-rolling into `package require ess_test` primitives so writing a new
#   loader/stim comes with a fast, repeatable inner test loop.
#
#   Two things it does:
#     1. LOADER HARNESS (Tier 1) -- source a <proto>_loaders.tcl faithfully
#        (inside `namespace eval ::ess`, exactly as ESS does, so absolute
#        `::ess::sys::proto::helper` calls resolve), run a loader body with
#        args, and hand back the `stimdg` for column/timing assertions. Pure
#        dlsh; runs under a bare `dlsh -e`.
#     2. STIM HARNESS (Tier 2) -- install capturing stubs for the stim2 GL/
#        motionpatch commands, source a <proto>_stim.tcl, and drive its
#        per-frame prescripts with a synthetic StimTimeF clock. You assert on
#        WHAT THE STIM DECIDED TO DRAW each frame (position, direction,
#        coherence, speed, lifetime, color, events) -- the arithmetic the stim
#        hands to stim2 -- never on pixels.
#
#   HONEST BOUNDARY (say it loudly): this is a LOGIC/DATA harness, not a
#   renderer, a vsync clock, or a state-machine emulator.
#     * No rendering (GL) -- stubs capture INTENT, not pixels. Whether the disc
#       is actually invisible, how the mask/sampler looks, real dot motion:
#       those need real stim2 and your eyes.
#     * No real vsync/timing -- YOU drive a synthetic StimTimeF.
#     * No hardware (eye/joystick/juicer/sound) -- stubbed.
#     * No full state-machine run against the real dserv event loop.
#   Use ess_test to catch math/data/timing regressions fast and headless; use
#   real stim2 to see how it looks. They are complementary.
#
#   See README.md for a full walkthrough; test_ess_test.tcl for worked examples.

package provide ess_test 0.1
package require dlsh

namespace eval ess_test {
    # --- config -------------------------------------------------------------
    variable systems_root [file join $::env(HOME) systems ess]

    # --- loader-harness state ----------------------------------------------
    variable loaders   {}   ;# dict: loader_name -> {params <list> body <str>}
    variable last_loader ""  ;# name of the most recently registered loader
    variable defaults  {}   ;# dict: loader_name -> default arg dict (optional)

    # --- fake_system state --------------------------------------------------
    variable syscalls {}    ;# dict: fake_system cmd -> list of {subcmd args}

    # --- stim-harness state -------------------------------------------------
    variable handle_seq 0    ;# monotonic fake object-handle counter
    variable h2n {}          ;# dict: handle  -> registered objName
    variable n2h {}          ;# dict: objName -> handle
    variable prescripts {}   ;# list of {obj script} registered via addPreScript
    variable thisframe  {}   ;# this-frame (post-flip) script queue
    variable history    {}   ;# list of capture records (dicts)
    variable evts       {}   ;# list of event records (dicts)

    # --- assertion state ----------------------------------------------------
    variable npass 0
    variable nfail 0
    variable cur_test ""
}

# ==========================================================================
# 0. Config
# ==========================================================================
# ess_test::config -systems_root <path>   -- set/override the systems root
# ess_test::config                        -- return the current config dict
proc ess_test::config {args} {
    variable systems_root
    if {[llength $args] == 0} {
        return [dict create systems_root $systems_root]
    }
    foreach {k v} $args {
        switch -- $k {
            -systems_root { set systems_root $v }
            default { error "ess_test::config: unknown option $k" }
        }
    }
    return
}

# Resolve the on-disk path for a (system, protocol, type) triple. `type` is one
# of loaders/stim/variants/protocol/system (the file-name suffix convention).
proc ess_test::script_path {system protocol type} {
    variable systems_root
    switch -- $type {
        system   { return [file join $systems_root $system ${system}.tcl] }
        protocol { return [file join $systems_root $system $protocol ${protocol}.tcl] }
        default  { return [file join $systems_root $system $protocol ${protocol}_${type}.tcl] }
    }
}

# ==========================================================================
# 1. ESS environment shim
# ==========================================================================
# Make `package require ess` succeed and ensure a stub ::ess namespace exists,
# so loader/protocol files source cleanly. At SOURCE time these files mostly
# only DEFINE procs (few ::ess calls fire), and the loader body's ess-side
# calls are captured by the fake_system, so the stub namespace stays minimal.
proc ess_test::fake_ess {{version 2.0}} {
    catch {package forget ess}
    package provide ess $version
    namespace eval ::ess {}
    return
}

# ==========================================================================
# 2. Capturing "system object" (shared by loader + protocol harnesses)
# ==========================================================================
# A fake `$sys` whose EVERY `$sys <subcmd> <args...>` is recorded. add_loader/
# add_param/add_variable/add_method/add_state/add_action/add_transition/
# set_*/set_*_callback/... all funnel through one capturing dispatcher, so we
# need no per-subcommand knowledge. Introspect with the sys_* helpers below.
# (Implemented as a plain proc rather than TclOO: dead simple, and it dodges
# TclOO `unknown`-dispatch quirks across Tcl versions.)
proc ess_test::_sys_record {cmd sub arglist} {
    variable syscalls
    dict lappend syscalls $cmd [list $sub $arglist]
    return {}
}

# Create a fresh capturing system object; returns its command name (use it
# wherever the real ESS hands a `$sys` to loaders_init/protocol_init).
proc ess_test::fake_system {{name fake_sys}} {
    variable handle_seq
    variable syscalls
    set cmd ::ess_test::_fakesys[incr handle_seq]
    dict set syscalls $cmd {}
    proc $cmd {sub args} [format {ess_test::_sys_record %s $sub $args} [list $cmd]]
    return $cmd
}

# Queries over a fake_system's captured calls.
proc ess_test::sys_calls {s} { variable syscalls; return [dict get $syscalls $s] }
proc ess_test::sys_subcall {s sub} {
    variable syscalls
    set out {}
    foreach c [dict get $syscalls $s] { if {[lindex $c 0] eq $sub} { lappend out [lindex $c 1] } }
    return $out
}
proc ess_test::sys_params {s} {
    set out {}
    foreach a [ess_test::sys_subcall $s add_param] { lappend out [lindex $a 0] }
    return $out
}
proc ess_test::sys_variables {s} {
    set out {}
    foreach a [ess_test::sys_subcall $s add_variable] { lappend out [lindex $a 0] }
    return $out
}
proc ess_test::sys_methods {s} {
    set out {}
    foreach a [ess_test::sys_subcall $s add_method] { lappend out [lindex $a 0] }
    return $out
}
proc ess_test::sys_loaders {s} {
    set out {}
    foreach a [ess_test::sys_subcall $s add_loader] { lappend out [lindex $a 0] }
    return $out
}

# ==========================================================================
# 3. Loader harness
# ==========================================================================
# Every fully-qualified ::ess::...::loaders_init command currently defined
# (walks the ::ess namespace tree).
proc ess_test::_all_loaders_init {} {
    set out {}
    ess_test::_scan_loaders_init ::ess out
    return $out
}
proc ess_test::_scan_loaders_init {ns outvar} {
    upvar 1 $outvar out
    if {[info commands ${ns}::loaders_init] ne ""} { lappend out ${ns}::loaders_init }
    foreach child [namespace children $ns] { ess_test::_scan_loaders_init $child out }
}

# Source a <proto>_loaders.tcl the way ESS does -- INSIDE `namespace eval ::ess`
# -- so `namespace eval sys::proto {...}` lands at ::ess::sys::proto:: and the
# helper procs it defines resolve at their absolute ::ess::... names. Then call
# the protocol's loaders_init on a fresh fake_system and capture every loader it
# registers. Returns the list of loader names found.
proc ess_test::load_loaders {system protocol args} {
    variable loaders
    variable last_loader

    set path [ess_test::script_path $system $protocol loaders]
    if {[dict exists $args -file]} { set path [dict get $args -file] }
    if {![file exists $path]} { error "ess_test::load_loaders: no file $path" }

    ess_test::fake_ess

    # Source inside ::ess so absolute-name helper resolution matches the rig.
    # (A global-scope source hides ::ess::sys::proto::helper resolution bugs --
    # exactly the class of bug this harness exists to catch.)
    set before [ess_test::_all_loaders_init]
    namespace eval ::ess [list source $path]
    set after [ess_test::_all_loaders_init]

    # Find the loaders_init this file defined. The protocol's namespace tail
    # need NOT match the directory name (e.g. prf/drifting-gratings defines
    # ::ess::prf::drifting), so prefer the conventional name but fall back to
    # whichever loaders_init is newly defined.
    set init "::ess::${system}::${protocol}::loaders_init"
    if {[info commands $init] eq ""} {
        set new {}
        foreach c $after { if {[lsearch -exact $before $c] < 0} { lappend new $c } }
        set match {}
        foreach c $new { if {[string match "::ess::${system}::*" $c]} { lappend match $c } }
        if {[llength $match]} {
            set init [lindex $match 0]
        } elseif {[llength $new]} {
            set init [lindex $new 0]
        } else {
            error "ess_test::load_loaders: no loaders_init defined after sourcing $path"
        }
    }

    set s [ess_test::fake_system "${system}_${protocol}_sys"]
    $init $s

    # Harvest every add_loader {name params body} into our registry.
    set loaders {}
    foreach a [ess_test::sys_subcall $s add_loader] {
        lassign $a name params body
        dict set loaders $name [dict create params $params body $body]
        set last_loader $name
    }
    if {[dict size $loaders] == 0} {
        error "ess_test::load_loaders: $init registered no loaders"
    }
    return [dict keys $loaders]
}

# The captured parameter-name list for a loader (defaults to the last one).
proc ess_test::loader_params {{name ""}} {
    variable loaders
    variable last_loader
    if {$name eq ""} { set name $last_loader }
    if {![dict exists $loaders $name]} { error "ess_test: no loader '$name'" }
    return [dict get $loaders $name params]
}

# Register a per-loader default arg dict so tests can pass only what they vary.
proc ess_test::loader_defaults {name dict} {
    variable defaults
    dict set defaults $name $dict
    return
}

# Bind a caller spec (named dict OR positional list) to the loader's param
# order. Named mode: every spec key must be a param; any missing param is an
# error (unless supplied by registered defaults). Positional mode: length must
# equal the param count.
proc ess_test::_bind_args {name spec} {
    variable defaults
    set params [ess_test::loader_params $name]

    # Named-dict path: even length AND every key is a known param.
    set named [expr {[llength $spec] % 2 == 0}]
    if {$named} {
        foreach {k v} $spec { if {[lsearch -exact $params $k] < 0} { set named 0; break } }
    }
    if {$named && ([llength $spec] > 0 || [dict exists $defaults $name])} {
        if {[dict exists $defaults $name]} {
            set spec [dict merge [dict get $defaults $name] $spec]
        }
        set missing {}; set vals {}
        foreach p $params {
            if {[dict exists $spec $p]} { lappend vals [dict get $spec $p] } else { lappend missing $p }
        }
        if {[llength $missing]} { error "ess_test::run_loader: missing params: $missing" }
        return $vals
    }

    # Positional path.
    if {[llength $spec] != [llength $params]} {
        error "ess_test::run_loader: got [llength $spec] positional args, loader '$name' takes\
               [llength $params] ($params)"
    }
    return $spec
}

# Run a loader body with args and return the stimdg handle.
#   run_loader <spec>          -- default (last-registered) loader
#   run_loader <name> <spec>   -- a named loader
# <spec> is a named dict {nr 2 gravities {9.8 0 -9.8} ...} or a positional list.
# The body runs via `apply` from the GLOBAL namespace :: (NOT the loader's
# sys::proto namespace) to match the real oo-method execution context.
proc ess_test::run_loader {args} {
    variable loaders
    variable last_loader
    if {[llength $args] == 1} {
        set name $last_loader; set spec [lindex $args 0]
    } elseif {[llength $args] == 2} {
        lassign $args name spec
    } else {
        error "usage: ess_test::run_loader ?name? <dict-or-list>"
    }
    if {![dict exists $loaders $name]} { error "ess_test: no loader '$name'" }

    set params [dict get $loaders $name params]
    set body   [dict get $loaders $name body]
    set vals   [ess_test::_bind_args $name $spec]

    if {[dg_exists stimdg]} { dg_delete stimdg }
    return [apply [list $params $body] {*}$vals]
}

# A compact human summary of a dg: columns, lengths, and a couple sample rows.
proc ess_test::dg_summary {g {nrows 2}} {
    set cols [dg_tclListnames $g]
    set lines [list "dg '$g': [llength $cols] columns"]
    foreach c $cols {
        set n [dl_length $g:$c]
        set samp {}
        for {set i 0} {$i < $nrows && $i < $n} {incr i} {
            lappend samp [dl_get $g:$c $i]
        }
        lappend lines [format "  %-24s len=%-5d  %s" $c $n $samp]
    }
    return [join $lines \n]
}

# ==========================================================================
# 4. Stim harness -- capturing stubs
# ==========================================================================
# Reset ALL stim-harness state (handles, name maps, prescripts, captures,
# events). stub_stim2 calls this; call it yourself between independent trials.
proc ess_test::reset_stim {} {
    variable handle_seq 0
    variable h2n {}
    variable n2h {}
    variable prescripts {}
    variable thisframe {}
    variable history {}
    variable evts {}
    set ::StimTimeF 0.0
    set ::StimTicksF 0.0
    set ::StimTime 0
    set ::SwapCount 0
    return
}

# Drop just the capture/event LOG (keep object maps + prescripts). Handy to
# zero the log right before a `play` so assertions see only that window.
proc ess_test::clear_captures {} {
    variable history {}
    variable evts {}
    return
}

# Canonicalize a target: a registered handle -> its objName; a registered name
# stays; anything else is returned as-is. (Bakes in the name-vs-handle
# resolution that bit the first invisibility audit.)
proc ess_test::_canon {t} {
    variable h2n
    variable n2h
    if {[dict exists $h2n $t]} { return [dict get $h2n $t] }
    if {[dict exists $n2h $t]} { return $t }
    return $t
}

proc ess_test::_record {cmd rawtarget arglist} {
    variable history
    set rec [dict create t $::StimTimeF cmd $cmd target [ess_test::_canon $rawtarget] args $arglist]
    lappend history $rec
    return
}

proc ess_test::_event {name payload} {
    variable evts
    lappend evts [dict create t $::StimTimeF name $name payload $payload]
    return
}

# Install the stim2 command surface as stubs. Three classes:
#   factory   -- return a unique handle (object/texture/image ids)
#   inert     -- structural no-ops (glist/scale/mask setup, redraw, ...)
#   capturing -- record per (command,target) so you can assert on frame writes
proc ess_test::stub_stim2 {} {
    ess_test::reset_stim

    # -- factory: unique handles ------------------------------------------
    foreach c {polygon metagroup shaderImageCreate shaderImageID \
               img_create img_drawPolygon img_drawPolygonFast img_imgtolist} {
        proc ::$c {args} { return [incr ::ess_test::handle_seq] }
    }
    # motionpatch is a factory too (returns the patch handle)
    proc ::motionpatch {args} { return [incr ::ess_test::handle_seq] }

    # -- inert: structural no-ops -----------------------------------------
    foreach c {glistInit resetObjList shaderImageReset shaderSetPath polycirc \
               scaleObj metagroupAdd glistAddObject glistSetDynamic \
               glistSetCurGroup glistSetVisible redraw load_Impro img_delete \
               masksoftness motionpatch_logBegin motionpatch_logEnd \
               motionpatch_logExport} {
        proc ::$c {args} {}
    }
    # resetObjList clears the object list + its scripts on the real side; model
    # that so a fresh nexttrial re-registers cleanly (keeps the capture log).
    proc ::resetObjList {args} {
        set ::ess_test::prescripts {}
        set ::ess_test::thisframe {}
        set ::ess_test::h2n {}
        set ::ess_test::n2h {}
    }

    # -- name <-> handle map ----------------------------------------------
    proc ::objName {obj name} {
        dict set ::ess_test::h2n $obj $name
        dict set ::ess_test::n2h $name $obj
        return $name
    }

    # -- capturing: object transforms / color / visibility ----------------
    proc ::translateObj {obj x y} { ess_test::_record translateObj $obj [list $x $y] }
    proc ::polycolor    {obj args} { ess_test::_record polycolor $obj $args }
    proc ::setVisible   {obj v}    { ess_test::_record setVisible $obj [list $v] }

    # -- capturing: every motionpatch_* setter (target = 1st arg) ---------
    foreach c {motionpatch_coherence motionpatch_speed motionpatch_lifetime \
               motionpatch_direction motionpatch_color motionpatch_maskoffset \
               motionpatch_maskscale motionpatch_pointsize motionpatch_masktype \
               motionpatch_setSampler motionpatch_samplermaskmode} {
        proc ::$c {target args} [format {ess_test::_record %s $target $args} $c]
    }

    # -- per-frame script registration ------------------------------------
    proc ::addPreScript {obj script} {
        lappend ::ess_test::prescripts [list $obj $script]
    }
    proc ::addThisFrameScript {obj script} {
        lappend ::ess_test::thisframe $script
    }

    # -- events ------------------------------------------------------------
    proc ::dserv_send_evt {name {payload {}}} { ess_test::_event $name $payload }

    # -- dserv comm boundary (kept headless) ------------------------------
    ess_test::stub_dserv
    return
}

# Neuter the dserv-communication surface some stim files touch at source/run
# time (a live datapoint subscription or set). We FAKE the qpcs package so the
# stim's `package require qpcs` can't reload the real one over our stubs, give
# ::dservhost/::dservport harmless defaults, and make the ds-comm calls inert
# (dsGet returns "" -- there is no dserv to read). This is the spec's "stub
# dserv" boundary: reads/writes go nowhere, so nothing connects or hangs.
proc ess_test::stub_dserv {} {
    if {![info exists ::dservhost]} { set ::dservhost localhost }
    if {![info exists ::dservport]} { set ::dservport 4620 }
    catch {package forget qpcs}
    package provide qpcs 0.0
    namespace eval ::qpcs {}
    proc ::qpcs::dsSet {args} {}
    proc ::qpcs::dsGet {args} { return {} }
    proc ::qpcs::dsStimAddMatch {args} {}
    return
}

# Source a <proto>_stim.tcl with stubs installed (stub_stim2 must run first, so
# top-level calls like `load_Impro` exist). The stim's `package require mp_sim`
# etc. load for real.
proc ess_test::stim_source {system protocol args} {
    set path [ess_test::script_path $system $protocol stim]
    if {[dict exists $args -file]} { set path [dict get $args -file] }
    if {![file exists $path]} { error "ess_test::stim_source: no file $path" }
    uplevel #0 [list source $path]
    return
}

# ==========================================================================
# 4b. Stim harness -- synthetic frame clock
# ==========================================================================
proc ess_test::set_time {ms} {
    set ::StimTimeF [expr {double($ms)}]
    set ::StimTime  [expr {int($ms)}]
    return
}

# Advance ONE frame: bump StimTimeF by dt (seconds), run every registered
# prescript in registration order (the pre-flip drivers), then run any
# this-frame scripts they queued (the post-flip event pushes), and return this
# frame's writes+events as a dict {t writes events}. Captures also accumulate
# in the global log (see captured/last/series/events).
proc ess_test::step {args} {
    variable prescripts
    variable thisframe
    variable history
    variable evts

    set dt 0.0166667
    if {[dict exists $args -dt]} { set dt [dict get $args -dt] }

    set ::StimTimeF [expr {$::StimTimeF + $dt*1000.0}]
    set ::StimTime  [expr {int($::StimTimeF)}]
    incr ::SwapCount

    set h0 [llength $history]
    set e0 [llength $evts]

    set thisframe {}
    foreach ps $prescripts {
        lassign $ps obj script
        uplevel #0 $script
    }
    # post-flip: fire the one-shot scripts queued during this frame
    set q $thisframe
    set thisframe {}
    foreach script $q { uplevel #0 $script }

    set writes [lrange $history $h0 end]
    set fired  [lrange $evts    $e0 end]
    return [dict create t $::StimTimeF writes $writes events $fired]
}

# Drive frames for `-dur` seconds at `-dt` seconds/frame; return the list of
# per-frame dicts from step. (Assertions usually read the accumulated log via
# captured/series/events rather than this list.)
proc ess_test::play {args} {
    set dur 1.0
    set dt  0.0166667
    if {[dict exists $args -dur]} { set dur [dict get $args -dur] }
    if {[dict exists $args -dt]}  { set dt  [dict get $args -dt] }
    set nframes [expr {int(ceil($dur/$dt))}]
    set frames {}
    for {set i 0} {$i < $nframes} {incr i} {
        lappend frames [ess_test::step -dt $dt]
    }
    return $frames
}

# ==========================================================================
# 4c. Stim harness -- capture queries
# ==========================================================================
# All records matching a command (and optionally a target). Each record is a
# dict: t cmd target args.
proc ess_test::captured {cmd {target ""}} {
    variable history
    set out {}
    foreach r $history {
        if {[dict get $r cmd] ne $cmd} continue
        if {$target ne "" && [dict get $r target] ne $target} continue
        lappend out $r
    }
    return $out
}

# The most recent matching record (or {} if none).
proc ess_test::last {cmd {target ""}} {
    set all [ess_test::captured $cmd $target]
    if {[llength $all] == 0} { return {} }
    return [lindex $all end]
}

# Time series {t val} of one arg (default arg 0) of a (command,target).
proc ess_test::series {cmd target {argidx 0}} {
    set out {}
    foreach r [ess_test::captured $cmd $target] {
        lappend out [list [dict get $r t] [lindex [dict get $r args] $argidx]]
    }
    return $out
}

# Just the values (no times) of one arg of a (command,target).
proc ess_test::values {cmd target {argidx 0}} {
    set out {}
    foreach r [ess_test::captured $cmd $target] { lappend out [lindex [dict get $r args] $argidx] }
    return $out
}

# Event records (dict: t name payload), optionally filtered by name.
proc ess_test::events {{name ""}} {
    variable evts
    if {$name eq ""} { return $evts }
    set out {}
    foreach e $evts { if {[dict get $e name] eq $name} { lappend out $e } }
    return $out
}

# The StimTimeF at which a named event first fired (or {} if never).
proc ess_test::event_time {name} {
    set m [ess_test::events $name]
    if {[llength $m] == 0} { return {} }
    return [dict get [lindex $m 0] t]
}

# ==========================================================================
# 5. Assertions / runner
# ==========================================================================
# Evaluate a boolean expr in the CALLER's scope (so $g:col and locals resolve),
# print ok/FAIL, and count. Mirrors test_launch_sim.tcl's proven style.
proc ess_test::assert {cond msg} {
    variable npass
    variable nfail
    variable cur_test
    set pfx [expr {$cur_test ne "" ? "\[$cur_test\] " : ""}]
    if {[catch {uplevel 1 [list expr $cond]} v]} {
        puts stderr "FAIL: ${pfx}$msg  (error: $v)"; incr nfail; return 0
    }
    if {$v} {
        puts "  ok: ${pfx}$msg"; incr npass; return 1
    }
    puts stderr "FAIL: ${pfx}$msg"; incr nfail; return 0
}

proc ess_test::approx {a b {tol 1e-6}} { expr {abs($a-$b) <= $tol} }
proc ess_test::in_range {v lo hi} { expr {$v >= $lo && $v <= $hi} }

# Group a set of assertions under a name; a body error is itself a failure.
proc ess_test::test {name body} {
    variable cur_test
    variable nfail
    puts "== $name"
    set prev $cur_test
    set cur_test $name
    if {[catch {uplevel 1 $body} err opts]} {
        puts stderr "FAIL: \[$name\] body errored: $err"
        puts stderr [dict get $opts -errorinfo]
        incr nfail
    }
    set cur_test $prev
    return
}

# Print the pass/fail tally. Returns 0 if all passed, 1 otherwise -- feed it to
# `exit` in a script: `exit [ess_test::summary]`.
proc ess_test::summary {} {
    variable npass
    variable nfail
    puts ""
    if {$nfail == 0} {
        puts "ALL PASS ($npass assertions)"
        return 0
    }
    puts "$nfail FAILURE(S) / [expr {$npass+$nfail}] assertions"
    return 1
}

# Reset the assertion tally (e.g. between independent test files in one interp).
proc ess_test::reset_counts {} {
    variable npass 0
    variable nfail 0
    return
}

namespace eval ess_test {
    namespace export config script_path fake_ess fake_system \
        sys_calls sys_subcall sys_params sys_variables sys_methods sys_loaders \
        load_loaders loader_params loader_defaults run_loader dg_summary \
        stub_stim2 stub_dserv stim_source reset_stim clear_captures \
        set_time step play captured last series values events event_time \
        assert approx in_range test summary reset_counts
}
