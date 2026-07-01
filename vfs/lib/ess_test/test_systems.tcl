# test_systems.tcl --
#   Generalization probe: run Tier-1 load_loaders against EVERY system/protocol
#   under the systems root and categorize the outcome. Proves the loader harness
#   (fake_ess + namespace-eval sourcing + fake_system capture + loaders_init
#   discovery) works beyond pursuit/ballistic, and flags scripts whose pure-dlsh
#   deps aren't available headless.
#
#   Run:  dlsh vfs/lib/ess_test/test_systems.tcl
#
#   Categories:
#     OK    -- sourced, loaders_init ran, >=1 loader captured
#     DEP   -- needs a package not loadable headless (planko/haptic/blob/...)
#     FAIL  -- an unexpected error (a real harness or script bug) -> exit 1

if {![info exists ::__ess_test_loaded]} {
    catch { source /usr/local/dlsh/dlsh_setup.tcl }
    package require dlsh
    catch {package forget ess_test}
    catch {namespace delete ::ess_test}
    source [file join [file dirname [info script]] ess_test.tcl]
    set ::__ess_test_loaded 1
}

# discover every <sys>/<proto>/<proto>_loaders.tcl under the systems root
set root [dict get [ess_test::config] systems_root]
set pairs {}
foreach f [lsort [glob -nocomplain -directory $root */*/*_loaders.tcl]] {
    if {[string match */overlays/* $f]} continue
    set proto [file tail [file dirname $f]]
    set sys   [file tail [file dirname [file dirname $f]]]
    lappend pairs [list $sys $proto]
}

puts "systems root: $root"
puts "found [llength $pairs] loader files\n"
puts [format "  %-14s %-18s %-6s %s" SYSTEM PROTOCOL STATUS DETAIL]
puts "  [string repeat - 74]"

set nok 0; set ndep 0; set nfail 0
foreach p $pairs {
    lassign $p sys proto
    if {[catch {ess_test::load_loaders $sys $proto} names]} {
        # classify: a missing pure-dlsh dependency is expected/allowed
        if {[string match "*can't find package*" $names] ||
            [string match "*can't find a usable*" $names]} {
            set pkg [regsub {.*package ([^\n:]+).*} $names {\1}]
            puts [format "  %-14s %-18s %-6s needs %s" $sys $proto DEP [string trim $pkg]]
            incr ndep
        } else {
            puts [format "  %-14s %-18s %-6s %s" $sys $proto FAIL \
                      [string range [lindex [split $names \n] 0] 0 44]]
            incr nfail
        }
        continue
    }
    # sourced + ran: report the loaders and the first one's param count
    set first [lindex $names 0]
    set np [llength [ess_test::loader_params $first]]
    puts [format "  %-14s %-18s %-6s %d loader(s): %s  (%s: %d params)" \
              $sys $proto OK [llength $names] $names $first $np]
    incr nok
}

puts "\nOK=$nok  DEP=$ndep  FAIL=$nfail  (total [llength $pairs])"
if {$nfail > 0} {
    puts "FAIL: $nfail system(s) errored unexpectedly"
    exit 1
}
puts "PASS: every headless-capable system loaded"
exit 0
