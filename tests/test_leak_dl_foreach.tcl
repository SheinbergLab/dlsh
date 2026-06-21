#!/usr/bin/env dlsh
#
# test_leak_dl_foreach.tcl
#   RSS leak test for dl_foreach, modeled on the other test_leak_*.tcl files.
#
#   The risk area is the DF_LIST (list-of-lists) path: dl_foreach copies each
#   sublist, registers it as a temporary dynlist, and must free it after the
#   body runs. dlsh's automatic temp-list cleanup only fires at proc exit, so
#   dl_foreach frees its per-element temp list EXPLICITLY (Tcl_UnsetVar2) every
#   iteration -- this test confirms that holds, including at global scope.
#
#   Inputs are built once; each round's work runs inside a proc so any temp
#   lists created by the harness itself are reclaimed and don't masquerade as
#   a dl_foreach leak.
#
#   Usage:  dlsh test_leak_dl_foreach.tcl ?rounds? ?rows? ?scalars?
#   Exits non-zero if RSS grows beyond a small threshold after warmup.

# --- dlsh bootstrap ---
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

proc get_rss_kb {} {
    if {[file readable /proc/self/status]} {
        set f [open /proc/self/status r]; set d [read $f]; close $f
        if {[regexp {VmRSS:\s+(\d+)\s+kB} $d -> rss]} { return $rss }
    }
    if {![catch {exec ps -o rss= -p [pid]} rss]} { return [string trim $rss] }
    return -1
}

set rounds  [expr {$argc >= 1 ? [lindex $argv 0] : 60}]
set n_rows  [expr {$argc >= 2 ? [lindex $argv 1] : 500}]
set n_scal  [expr {$argc >= 3 ? [lindex $argv 2] : 5000}]
set warmup  5
set thresh_kb 1024   ;# page-granularity noise allowed; a real per-iter leak is MB

# inputs built ONCE
set rows {}
for {set i 0} {$i < $n_rows} {incr i} {
    lappend rows [dl_ilist $i [expr {$i*2}] [expr {$i*3}]]
}
set ::big    [dl_llist {*}$rows]
set ::scalar [dl_fromto 0 $n_scal]

# one round of work, inside a proc so harness temps are auto-reclaimed
proc round_work {} {
    set s 0
    dl_foreach row $::big    { set s [expr {$s + [dl_sum $row]}] }   ;# DF_LIST path
    dl_foreach x   $::scalar { incr s }                              ;# scalar path
    return $s
}

puts "=== dl_foreach RSS leak test ==="
puts "  rounds:  $rounds   rows/round: $n_rows   scalars/round: $n_scal"
puts [format "%-8s %10s %10s" round RSS growth]

set rss0 [get_rss_kb]
set base $rss0
for {set r 1} {$r <= $rounds} {incr r} {
    round_work
    set rss [get_rss_kb]
    if {$r == $warmup} { set base $rss }
    if {$r <= $warmup || $r % 10 == 0 || $r == $rounds} {
        puts [format "%-8s %10d %10d" r$r $rss [expr {$rss - $base}]]
    }
}

set growth [expr {[get_rss_kb] - $base}]
puts "Growth after warmup (r$warmup): ${growth} kB  (threshold ${thresh_kb} kB)"
if {$growth > $thresh_kb} {
    puts "=== LEAK SUSPECTED ==="
    exit 1
}
puts "=== PASS ==="
