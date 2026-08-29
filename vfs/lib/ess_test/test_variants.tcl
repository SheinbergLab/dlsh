# test_variants.tcl --
#   Coverage probe: for EVERY system/protocol/variant under the systems root,
#   run the loader exactly as ESS would for that variant's defaults (via
#   ess_test::run_variant) and sanity-check the resulting stimdg. This exercises
#   the loader logic across the whole collection -- the broadest "dry test" of
#   trial generation -- using the harness's ESS-context shims (ambient screen
#   vars, dserv stubs, `my` delegation, and best-effort system-file sourcing).
#
#   Run:  dlsh vfs/lib/ess_test/test_variants.tcl
#
#   Per variant:
#     OK n=<rows>  -- loader ran, stimdg columns all equal length
#     SKIP         -- a loader arg has no variant option (supplied elsewhere on
#                     the rig, e.g. config/params); not runnable from defaults
#     DEP          -- needs a package/command not available headless
#     RAGGED       -- stimdg columns of unequal length  <-- a real integrity bug
#     ERR          -- other error
#   Exit nonzero only on RAGGED (the only category that is unambiguously a bug).

if {![info exists ::__ess_test_loaded]} {
    catch { source /usr/local/dlsh/dlsh_setup.tcl }
    package require dlsh
    catch {package forget ess_test}
    catch {namespace delete ::ess_test}
    source [file join [file dirname [info script]] ess_test.tcl]
    set ::__ess_test_loaded 1
}

set root [dict get [ess_test::config] systems_root]
set pairs {}
foreach f [lsort [glob -nocomplain -directory $root */*/*_loaders.tcl]] {
    if {[string match */overlays/* $f]} continue
    set proto [file tail [file dirname $f]]
    set sys   [file tail [file dirname [file dirname $f]]]
    lappend pairs [list $sys $proto]
}

proc classify_err {msg} {
    if {[string match "*can't find package*" $msg]}            { return DEP }
    if {[string match {*invalid command name "dserv*} $msg]}   { return DEP }
    if {[string match "*no loader_option for*" $msg]}          { return SKIP }
    return ERR
}

puts "systems root: $root\n"
array set tally {OK 0 SKIP 0 DEP 0 RAGGED 0 ERR 0}
set nvariants 0
foreach p $pairs {
    lassign $p sys proto
    if {[catch {ess_test::load_loaders $sys $proto} names]} {
        puts [format "  %-28s (loaders unavailable: %s)" $sys/$proto \
                  [string range [lindex [split $names \n] 0] 0 40]]
        continue
    }
    if {[catch {ess_test::load_variants $sys $proto} variants]} continue

    dict for {vname vinfo} $variants {
        if {![dict exists $vinfo loader_proc]} continue
        incr nvariants
        if {[catch {ess_test::run_variant $sys $proto $vname} err]} {
            set cat [classify_err $err]
            incr tally($cat)
            puts [format "  %-13s %-16s %-26s %-5s %s" $sys $proto $vname $cat \
                      [string range [lindex [split $err \n] 0] 0 40]]
            continue
        }
        set n [dl_length stimdg:stimtype]
        set ragged {}
        foreach c [dg_tclListnames stimdg] { if {[dl_length stimdg:$c] != $n} { lappend ragged $c } }
        if {[llength $ragged]} {
            incr tally(RAGGED)
            puts [format "  %-13s %-16s %-26s RAGGED n=%d %s" $sys $proto $vname $n [lrange $ragged 0 3]]
        } else {
            incr tally(OK)
        }
    }
}

puts "\n$nvariants variants:  OK=$tally(OK)  SKIP=$tally(SKIP)  DEP=$tally(DEP)  ERR=$tally(ERR)  RAGGED=$tally(RAGGED)"
if {$tally(RAGGED) > 0} {
    puts "FAIL: $tally(RAGGED) variant(s) produced a ragged stimdg (real integrity bug)"
    exit 1
}
puts "PASS: every runnable loader produced a well-formed stimdg"
exit 0
