#!/usr/bin/env dlsh
#
# test_dslog_readess.tcl
#   dslog::readESS's session columns: a record outside the obs periods
#   (before the first, between two, after the last) is kept in
#   <session>name with its time in <sessiont>name (ms from the file's
#   first record, the obs_start_ms axis; whole ms, or the microsecond
#   fraction under wide types) and its value count in <sessionn>name.
#   The obs rows (<ds>/<dst>/<dsn>) are checked alongside, unchanged.
#
#   Usage:  dlsh test_dslog_readess.tcl        (exits non-zero on any failure)

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
package require dslog

set ::fail 0
proc check {label got want} {
    if {$got eq $want} { puts "ok   $label" } \
    else { puts "FAIL $label\n       got:  $got\n       want: $want"; incr ::fail }
}

# dserv type codes: STRING 1, FLOAT 2, EVT 9.  An event's type word is
# EVT | type<<8 | subtype<<16 | puttype<<24; BEGINOBS 19, ENDOBS 20.
proc evt {type} { expr {9 | ($type << 8) | (0 << 16) | (5 << 24)} }

set T 1761436800000000
set recs [list \
    [list ess/status        $T              1 boot] \
    [list eyetracking/raw   [expr {$T+1000}] 2 [binary format f* {1 2 3 4}]] \
    [list eventlog/events   [expr {$T+2000}] [evt 19] [binary format n 0]] \
    [list ess/status        [expr {$T+3000}] 1 in1] \
    [list eventlog/events   [expr {$T+4500}] [evt 20] [binary format n 0]] \
    [list ess/status        [expr {$T+5250}] 1 iti] \
    [list eyetracking/raw   [expr {$T+6000}] 2 [binary format f* {5 6}]] \
    [list eventlog/events   [expr {$T+7000}] [evt 19] [binary format n 1]] \
    [list ess/status        [expr {$T+8000}] 1 in2] \
    [list eventlog/events   [expr {$T+9000}] [evt 20] [binary format n 1]] \
    [list ess/status        [expr {$T+10500}] 1 end]]

set path [file join [expr {[info exists ::env(TMPDIR)] ? $::env(TMPDIR) : "/tmp"}] \
              test_dslog_readess_[pid].ess]
set h [dslog::open $path w $T]
foreach r $recs {
    lassign $r name ts type data
    dslog::put $h [dict create varname $name timestamp $ts flags 0 type $type data $data]
}
dslog::close $h

foreach wide {0 1} {
    dslog::wideTypes $wide
    set w [expr {$wide ? "wide" : "narrow"}]
    set g [dslog::readESS $path]

    check "$w: two obs periods" [dl_length $g:obs_start_ms] 2
    check "$w: obs_start_ms on the file's first record" [dl_tcllist $g:obs_start_ms] {2 7}
    check "$w: obs rows unchanged" [dl_tcllist $g:<ds>ess/status] {in1 in2}

    check "$w: <session> values, before / between / after" \
        [dl_tcllist $g:<session>ess/status:0] {boot iti end}
    check "$w: <sessionn> one value a record" [dl_tcllist $g:<sessionn>ess/status:0] {1 1 1}
    set times [dl_tcllist $g:<sessiont>ess/status:0]
    if {$wide} {
        check "wide: <sessiont> keeps the microseconds (ms from the first record)" \
            [lmap t $times {format %.3f $t}] {0.000 5.250 10.500}
        check "wide: <sessiont> is double" [dl_datatype $g:<sessiont>ess/status:0] double
    } else {
        check "narrow: <sessiont> in whole ms" $times {0 5 10}
        check "narrow: <sessiont> is long" [dl_datatype $g:<sessiont>ess/status:0] long
    }

    # a multi-value record: the counts split the concatenated values
    check "$w: float session values concatenated" \
        [dl_tcllist $g:<session>eyetracking/raw:0] {1.0 2.0 3.0 4.0 5.0 6.0}
    check "$w: <sessionn> splits them into records" \
        [dl_tcllist $g:<sessionn>eyetracking/raw:0] {4 2}
    check "$w: their record times" \
        [lmap t [dl_tcllist $g:<sessiont>eyetracking/raw:0] {expr {round($t)}}] {1 6}
    check "$w: one entry per session column, like <session>" \
        [list [dl_length $g:<sessiont>ess/status] [dl_length $g:<sessionn>ess/status]] {1 1}
    dg_delete $g
}
dslog::wideTypes 0
file delete $path

if {$::fail} { puts "$::fail FAILED"; exit 1 }
puts "all passed"
exit 0
