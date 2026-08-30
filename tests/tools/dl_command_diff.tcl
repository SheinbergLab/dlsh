# dl_command_diff.tcl -- exercise the whole dl_*/dm_* command surface with canned
# argument patterns and print a normalized transcript. Run against two builds
# and diff: any behavioral difference shows up as a diff line.
#
#   dlsh tests/tools/dl_command_diff.tcl <outfile>
#
# See README.md in this directory -- in particular why fixtures must not be
# built inside a proc.
#
# Normalization matters. Generated names (%list37%, >4<, &x_2&) embed
# allocation counters, so they are replaced with placeholders and results are
# resolved to values wherever they name a list. Otherwise the diff would be
# pure noise.

package require dlsh

# Pull in the analysis packages under lib/local -- corrgram, graphing,
# selectivity et al are where the heaviest dl_local use lives, and none of it
# is touched by the existing test suite. Ones with compiled parts simply will
# not load here; that is fine and is recorded in the transcript so a package
# silently dropping out shows up as a diff.
set loaded {}
foreach p {corrgram graphing selectivity roc spikes traj hex colorcal wav
           mtspec spec rasts robust em dtw grasp points} {
    if {![catch {package require $p}]} { lappend loaded $p }
}

set outfile [lindex $argv 0]
set out [open $outfile w]

# Deterministic RNG so dl_urand and friends are comparable.
catch {dl_srand 12345}

# Commands that would destroy the fixture state, do I/O, or hijack control
# flow. dl_local/dl_return/dl_yield are excluded because they return from the
# probe proc itself; they have dedicated tests.
set SKIP {
    dl_clean dl_cleanReturns dl_delete dl_local dl_return dl_yield
    dl_srand dl_pushTemps dl_popTemps dl_reset
}
set SKIP_RE {read|write|dump|open|close|file|save|load|send|exit|quit|source|eval|exec|print|log$}

proc norm {r} {
    # Resolve list handles to values so we compare data, not names.
    if {![catch {dl_tcllist $r} v]} { set r $v }
    regsub -all {%[a-zA-Z]*[0-9]+%} $r {<tmp>} r
    regsub -all {>[0-9]+<} $r {<ret>} r
    regsub -all {&[^& ]*_[0-9]+&} $r {<loc>} r
    if {[string length $r] > 200} { set r "[string range $r 0 199]...(trunc)" }
    return [string map {\n " "} $r]
}

# Fresh fixtures per call: many dl_ commands mutate in place, so reusing them
# would make each result depend on every call before it.
#
# CRITICAL: this returns the SCRIPT that builds the fixture, not the fixture.
# Building it inside a proc and returning it is exactly the pattern this branch
# fixes -- on master the list dies with the proc frame, so every fixture would
# arrive dead and the whole transcript would read "dynlist not found". The
# script is evaluated in the caller's (global) frame so both builds behave the
# same and the diff measures the commands, not the harness.
proc fixture_script {tag} {
    switch -- $tag {
        I1  { return {dl_ilist 3 1 4 1 5} }
        I2  { return {dl_ilist 2 7 1 8 2} }
        F1  { return {dl_flist 1.5 2.5 3.5 4.5 5.5} }
        S1  { return {dl_slist a b c d e} }
        L1  { return {dl_llist [dl_ilist 1 2 3] [dl_ilist 4 5]} }
        L2  { return {dl_llist [dl_ilist 1 0 1] [dl_ilist 0 1]} }
        N   { return {set _ 2} }
        N1  { return {set _ 1} }
        F   { return {set _ 2.0} }
        default { return [list set _ $tag] }
    }
}

set PATTERNS {
    {}
    {I1}
    {I1 I2}
    {I1 N}
    {F1}
    {F1 F}
    {S1}
    {L1}
    {L1 L2}
    {I1 I2 N}
    {L1 N}
}

set cmds [lsort -unique [concat [info commands dl_*] [info commands dm_*] \
                                [info commands dlp_*] [info procs dl_*] \
                                [info procs dlp_*]]]
puts $out "# packages loaded: [lsort $loaded]"
puts $out "# commands: [llength $cmds]"

foreach c $cmds {
    if {[lsearch -exact $SKIP $c] >= 0} continue
    if {[regexp -nocase $SKIP_RE $c]} continue
    foreach p $PATTERNS {
        # Evaluated here, in the global frame, so fixtures outlive the call on
        # both builds (see fixture_script).
        set args {}
        foreach tag $p { lappend args [eval [fixture_script $tag]] }
        if {[catch {$c {*}$args} r]} {
            puts $out "$c \[$p\] ERR [norm $r]"
        } else {
            puts $out "$c \[$p\] OK  [norm $r]"
        }
    }
}
flush $out
close $out
puts "wrote [llength $cmds] commands to $outfile"
