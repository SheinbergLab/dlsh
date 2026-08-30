# dl_soak.tcl -- leak soak for dynlist lifetimes.
#
#   dlsh tests/tools/dl_soak.tcl <mode> ?blocks? ?threads? ?iters? ?-global?
#
# Modes: single | pooled | fresh | bare | load    (see README.md)
#
# Growth is reported PER BLOCK. The first block always includes allocator
# warm-up, so judge the tail, not the total.

fconfigure stdout -buffering none
package require dlsh

set MODE    [lindex $argv 0]
set BLOCKS  [expr {$argc >= 2 ? [lindex $argv 1] : 6}]
set THREADS [expr {$argc >= 3 ? [lindex $argv 2] : 8}]
set ITERS   [expr {$argc >= 4 ? [lindex $argv 3] : 1500}]
set GLOBAL  [expr {[lsearch -exact $argv -global] >= 0}]

if {$MODE ni {single pooled fresh bare load}} {
    puts "usage: dl_soak.tcl single|pooled|fresh|bare|load ?blocks? ?threads? ?iters? ?-global?"
    exit 2
}

proc rss_kb {} {
    if {[file readable /proc/self/status]} {
        set f [open /proc/self/status r]; set d [read $f]; close $f
        if {[regexp {VmRSS:\s+(\d+)\s+kB} $d -> r]} { return $r }
    }
    if {![catch {exec ps -o rss= -p [pid]} r]} { return [string trim $r] }
    return -1
}

# Every lifetime path the refcounted-handle work touches: escape by return,
# escape into containers, shimmering (which drops the last reference), an
# authoritative delete under a live reference, dyngroup absorption, and some
# real library procs that lean on dl_local internally.
set WORK {
    for {set i 0} {$i < %I%} {incr i} {
        set a [dl_fromto 0 40]
        set b [dl_mult $a 2]
        set acc {}
        lappend acc $a $b
        dict set d k $b
        llength $a
        lindex $b 0
        set g [dl_add $a 1]
        set v [dl_ilist 1 2 3]
        set alias $v
        dl_delete $v
        catch {dl_tcllist $alias}
        set grp [dg_create]
        dl_set $grp:col [dl_ilist 1 2 3]
        dg_delete $grp
        catch {dl_tcllist [dl_paste [dl_ilist 1 2] [dl_ilist 3 4]]}
        catch {dl_tcllist [dl_multisort [dl_ilist 3 1 2]]}
        unset acc d a b g v alias grp
    }
}

# Frame scope by default: that is how real dlsh code is written, and temps are
# reclaimed at frame exit. -global reproduces top-level accumulation, which is
# by design (dl_clean's job), not a leak.
set body [string map [list %I% $ITERS] $WORK]
if {!$GLOBAL} { set body "proc _soak_work {} {\n$body\n}\n_soak_work" }

set init "set ::auto_path [list $::auto_path]\npackage require dlsh\n"

proc report {mode growth residual} {
    set sum 0
    foreach g $growth { incr sum $g }
    puts ""
    puts "mode=$mode per-block growth: $growth kB   mean [expr {$sum/[llength $growth]}] kB"
    if {$residual ne ""} { puts "residual lists: $residual" }
    if {[llength $growth] < 2} { return 0 }
    set tail [expr {[lindex $growth end] + [lindex $growth end-1]}]
    if {$tail > 4096} { puts "LEAKING: $tail kB over the last two blocks"; return 1 }
    puts "CLEAN"
    return 0
}

puts "soak mode=$MODE blocks=$BLOCKS threads=$THREADS iters=$ITERS global=$GLOBAL"
if {$GLOBAL} {
    puts "NOTE: -global runs the work at the top level, where every temp keeps a"
    puts "      hidden global claim and so is retained until dl_clean. Growth here"
    puts "      is expected -- it is what dl_clean exists for, not a leak."
}
puts [format "%-8s %-12s %-10s" block "rss(kB)" "d-rss"]

set growth {}
set residual ""

switch -- $MODE {
    single {
        eval $body                                  ;# warm-up
        for {set _blk 0} {$_blk < $BLOCKS} {incr _blk} {
            set _r0 [rss_kb]; eval $body; set _r1 [rss_kb]
            lappend growth [expr {$_r1-$_r0}]
            puts [format "%-8s %-12s %-10s" $_blk $_r1 [expr {$_r1-$_r0}]]
        }
        set residual [llength [dl_dir]]
    }
    pooled - fresh - bare - load {
        package require Thread
        switch -- $MODE {
            bare { set tbody {set x 1} ; set init "" }
            load { set tbody "" }
            default { set tbody $body }
        }
        if {$MODE eq "pooled"} {
            # Create the workers ONCE and drive work into them, the way dserv
            # reuses interpreters. Growth here is real accumulation rather
            # than per-interpreter-creation cost.
            set tids {}
            for {set t 0} {$t < $THREADS} {incr t} {
                lappend tids [thread::create "$init\nthread::wait"]
            }
            foreach t $tids { thread::send -async $t "$tbody\nset _ ok" res($t) }
            foreach t $tids { if {![info exists res($t)]} { vwait res($t) } ; unset res($t) }
            for {set _blk 0} {$_blk < $BLOCKS} {incr _blk} {
                set _r0 [rss_kb]
                foreach t $tids { thread::send -async $t "$tbody\nllength \[dl_dir\]" res($t) }
                set counts {}
                foreach t $tids {
                    if {![info exists res($t)]} { vwait res($t) }
                    lappend counts $res($t); unset res($t)
                }
                set _r1 [rss_kb]
                lappend growth [expr {$_r1-$_r0}]
                puts [format "%-8s %-12s %-10s worker lists %s" $_blk $_r1 [expr {$_r1-$_r0}] [lsort -unique $counts]]
            }
            foreach t $tids { thread::release $t }
            set residual [llength [dl_dir]]
        } else {
            proc spawn {n init tbody} {
                set tids {}
                for {set t 0} {$t < $n} {incr t} {
                    lappend tids [thread::create -joinable "$init$tbody"]
                }
                foreach t $tids { thread::join $t }
            }
            spawn $THREADS $init $tbody             ;# warm-up
            for {set _blk 0} {$_blk < $BLOCKS} {incr _blk} {
                set _r0 [rss_kb]; spawn $THREADS $init $tbody; set _r1 [rss_kb]
                lappend growth [expr {$_r1-$_r0}]
                puts [format "%-8s %-12s %-10s" $_blk $_r1 [expr {$_r1-$_r0}]]
            }
            set residual [llength [dl_dir]]
        }
    }
}

exit [report $MODE $growth $residual]
