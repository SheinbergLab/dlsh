# test_ess_harness.tcl --
#   Worked example + regression suite for the ESS harness: the REAL
#   ess-2.0.tm driven headless against a stubbed dserv.
#
#   Run:  dlsh -e 'package require ess_test
#                  source <this dir>/test_ess_harness.tcl'
#
#   Part 1 uses the fixture system in ./fixtures (works / throws /
#   badloader) to pin down the load-failure contract:
#     - a load that throws must NOT leave ess/status at "loading"
#       (that disables every control in ess_control.html)
#     - it must publish ess/load_error with the stage and the original
#       message, and must preserve ess/last_good_system
#     - ess::load_last_good must recover
#
#   Part 2 runs the same checks against a REAL system in ~/systems, and
#   against a systems tree with the external data file removed, to show
#   the degraded ("loaded 0 trials") path end to end.
#
#   These fail against a pre-fix ess-2.0.tm -- that is the point.

package require ess_test

set here [file dirname [file normalize [info script]]]

# ── the ESS harness: real ess-2.0.tm, stubbed dserv ──────────────────
puts "ess [ess_test::real_ess -systems_root [file join $here fixtures ess]]\
 ([llength [ess_test::stubbed_commands]] commands stubbed)"

ess_test::test {a clean load} {
    set r [ess_test::load_system brokensys works basic]
    ess_test::assert {[dict get $r ok]}                              "load ok"
    ess_test::assert {[dict get $r trials] == 4}                     "4 trials"
    ess_test::assert {[ess_test::datapoint ess/status] eq "stopped"} "status stopped"
    ess_test::assert {[ess_test::datapoint ess/load_error] eq ""}    "load_error clear"
    ess_test::assert {[ess_test::datapoint ess/obs_total] == 4}      "obs_total published"
    ess_test::assert {[ess_test::datapoint ess/last_good_system] ne ""} \
        "last_good recorded"
}

set good [ess_test::datapoint ess/last_good_system]

ess_test::test {protocol_init throws} {
    ess_test::clear_dserv_log
    set r [ess_test::load_system brokensys throws basic]
    ess_test::assert {![dict get $r ok]} "load reported failure"
    ess_test::assert {[string match {*shape database*} [dict get $r error]]} \
        "original error preserved, not masked by the reporter"
    ess_test::assert {[ess_test::datapoint ess/status] eq "stopped"} \
        "status usable"
    # protocol_init runs BEFORE variant_init sets "loading", so this
    # particular failure never enters that state -- see badloader below.
    ess_test::assert {[lsearch [ess_test::dserv_history ess/status] loading] < 0} \
        "never entered loading (failed before variant_init)"
    set e [ess_test::datapoint ess/load_error]
    ess_test::assert {$e ne ""}                                 "load_error published"
    ess_test::assert {[string match {*"severity":"error"*} $e]} "severity=error"
    ess_test::assert {[string match {*protocol_init*} $e]}      "stage names protocol_init"
    ess_test::assert {[ess_test::datapoint ess/last_good_system] eq $::good} \
        "last_good survived the failure"
}

ess_test::test {the loader throws -- the path that wedged the GUI} {
    ess_test::clear_dserv_log
    set r [ess_test::load_system brokensys badloader basic]
    ess_test::assert {![dict get $r ok]} "load reported failure"
    ess_test::assert {[string match {*db file not found*} [dict get $r error]]} \
        "loader error preserved"
    ess_test::assert {[lsearch [ess_test::dserv_history ess/status] loading] >= 0} \
        "status entered loading"
    ess_test::assert {[ess_test::datapoint ess/status] eq "stopped"} \
        "status came back OUT of loading (the original bug)"
    ess_test::assert {[string match {*variant_execution*} \
                          [ess_test::datapoint ess/load_error]]} \
        "stage pinpoints the loader"
}

ess_test::test {a ragged stimdg is flagged at load, not mid-session} {
    set r [ess_test::load_system brokensys raggedloader basic]
    # it LOADS -- that is the danger: nothing throws, and nexttrial would
    # only blow up once the run reached the short column's end
    ess_test::assert {[dict get $r ok]}          "load itself succeeds"
    ess_test::assert {[dict get $r trials] == 4} "trials were built"
    ess_test::assert {[llength [ess_test::stimdg_problems]] > 0} \
        "harness sees the ragged column"
    set e [ess_test::datapoint ess/load_error]
    ess_test::assert {[string match {*"severity":"warning"*} $e]} "reported as a warning"
    ess_test::assert {[string match {*not a valid trial table*} $e]} "named as a table problem"
    ess_test::assert {[string match {*oops has 3 rows*} $e]} \
        "names the offending column and its length"
}

ess_test::test {recovery to the last good system} {
    ess_test::assert {![catch {::ess::load_last_good}]}              "load_last_good runs"
    ess_test::assert {[ess_test::datapoint ess/status] eq "stopped"} "status stopped"
    ess_test::assert {[ess_test::datapoint ess/load_error] eq ""}    "load_error cleared"
    ess_test::assert {[dl_length stimdg:stimtype] == 4}              "good trials restored"
}

# ── Part 2: a real system, with and without its external data ────────
#
# match_to_sample/phd needs a sqlite shape database that does not travel
# with the system tree. With it, 100 trials; without it, the loader must
# report 0 trials rather than throwing.

set real_root [file join $::env(HOME) systems ess]
if {![file isdirectory $real_root]} {
    puts "\n(skipping real-system checks: no $real_root)"
} else {
    ess_test::test {a real system loads} {
        ess_test::use_systems_root $real_root
        set r [ess_test::load_system match_to_sample phd VV]
        ess_test::assert {[dict get $r ok]} "match_to_sample/phd loads: [dict get $r error]"
        ess_test::assert {[dict get $r trials] > 0}  "built trials"
        ess_test::assert {[ess_test::datapoint ess/load_error] eq ""} "no report"
    }

    # Same tree, minus the data/ directory: the loader contract in action.
    set nodata [file join [ess_test::_tmpdir] ess_test_nodata]
    file delete -force $nodata
    file mkdir [file join $nodata ess match_to_sample phd]
    foreach f [glob -nocomplain [file join $real_root match_to_sample phd *.tcl]] {
        file copy $f [file join $nodata ess match_to_sample phd]
    }
    foreach f [glob -nocomplain [file join $real_root match_to_sample *.tcl]] {
        file copy $f [file join $nodata ess match_to_sample]
    }

    ess_test::test {a real system with its data file missing} {
        ess_test::use_systems_root [file join $nodata ess]
        set r [ess_test::load_system match_to_sample phd VV]
        # the contract: degrade, do not throw
        ess_test::assert {[dict get $r ok]} "load did NOT throw: [dict get $r error]"
        ess_test::assert {[dict get $r trials] == 0} "0 trials"
        ess_test::assert {[ess_test::datapoint ess/status] eq "stopped"} "status usable"
        set e [ess_test::datapoint ess/load_error]
        ess_test::assert {[string match {*"severity":"warning"*} $e]} \
            "reported as a warning, not an error"
        ess_test::assert {[string match {*shape database*} $e]} \
            "the reason reaches the datapoint"
    }
    file delete -force $nodata
}

ess_test::summary
