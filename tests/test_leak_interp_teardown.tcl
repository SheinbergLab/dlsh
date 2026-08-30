#!/usr/bin/env dlsh
#
# test_leak_interp_teardown.tcl
#   Destroying an interpreter must reclaim the lists and dyngroups it still
#   holds.
#
#   Dl_Init registered its per-interp state (DLSHINFO, the dlTable and the
#   dgTable) with a NULL delete proc, so deleteDlFunc -- which existed all
#   along -- was never called. Every interpreter that went away abandoned
#   everything still registered in it. dserv gives each subprocess its own
#   interpreter and a dyngroup like stimdg is not small, so this was worth
#   real memory.
#
#   Only untraced things leaked. Tcl dismantles the global namespace before
#   clearing assoc data, so ordinary temps were already freed by their delete
#   traces; what survived was exactly what never had one -- lists bound by
#   name with dl_set, and every dyngroup. So this test builds those, not
#   temps, or it would measure nothing.
#
#   Usage: dlsh test_leak_interp_teardown.tcl ?blocks? ?interps? ?size?
#   Exits non-zero if RSS climbs across blocks.

if {[catch {package require dlsh}]} {
    foreach path {/usr/local/dlsh/dlsh.zip /usr/local/lib/dlsh.zip} {
        if {[file exists $path]} {
            catch {zipfs mount $path /dlsh}
            set base [file join [zipfs root] dlsh]
            set ::auto_path [linsert $::auto_path 0 ${base}/lib]
            break
        }
    }
    package require dlsh
}

if {[catch {package require Thread}]} {
    puts "SKIP: no Thread package, cannot create interpreters to tear down"
    exit 0
}

set BLOCKS  [expr {$argc >= 1 ? [lindex $argv 0] : 4}]
set NINTERP [expr {$argc >= 2 ? [lindex $argv 1] : 6}]
set SIZE    [expr {$argc >= 3 ? [lindex $argv 2] : 20000}]

proc rss_kb {} {
    if {[file readable /proc/self/status]} {
        set f [open /proc/self/status r]; set d [read $f]; close $f
        if {[regexp {VmRSS:\s+(\d+)\s+kB} $d -> r]} { return $r }
    }
    if {![catch {exec ps -o rss= -p [pid]} r]} { return [string trim $r] }
    return -1
}

if {[rss_kb] < 0} {
    puts "SKIP: no RSS available on this platform"
    exit 0
}

# Named lists and a dyngroup: the two things that carry no delete trace.
set BODY [string map [list %S% $SIZE] {
    for {set k 0} {$k < 10} {incr k} { dl_set named_$k [dl_fromto 0 %S%] }
    dg_create bigdg
    for {set k 0} {$k < 10} {incr k} { dl_set bigdg:col_$k [dl_fromto 0 %S%] }
    set _ done
}]
set init "set ::auto_path [list $::auto_path]\npackage require dlsh\n"

proc block {n init body} {
    set tids {}
    for {set i 0} {$i < $n} {incr i} {
        lappend tids [thread::create -joinable "$init$body"]
    }
    foreach t $tids { thread::join $t }
}

block $NINTERP $init $BODY          ;# warm-up, not measured

set growth {}
for {set b 0} {$b < $BLOCKS} {incr b} {
    set r0 [rss_kb]
    block $NINTERP $init $BODY
    lappend growth [expr {[rss_kb] - $r0}]
}

# Each block leaked ~13 MB before the fix and well under 1 MB after, so the
# signal is enormous relative to allocator noise; compare the last two blocks
# against a threshold far below the old leak but above ordinary jitter.
set tail [expr {[lindex $growth end] + [lindex $growth end-1]}]
puts "per-block RSS growth: $growth kB (last two: $tail kB)"

if {$tail > 4096} {
    puts "FAIL interp teardown leaks -- lists/dyngroups not reclaimed"
    exit 1
}
puts "OK   interp teardown reclaims lists and dyngroups"
exit 0
