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
#   A DG datapoint (stimdg) sent twice: every occurrence kept serialized in
#   <dgbuf>name with its time in <dgbuft>name, the first alone decoded into
#   <name>column columns (no duplicate names).  Under wide types, the
#   anchor: file_t0_us (the first record on the dserv clock) and
#   obs_start_us (each BEGINOBS from it, whole us); a narrow group has
#   neither.
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

# two versions of a stimdg, serialized as dserv carries them (DSERV_DG 6)
proc dgbytes {vals} {
    catch { dg_delete stimdg }
    dg_create stimdg
    dl_set stimdg:x [dl_ilist {*}$vals]
    dg_toString64 stimdg b64
    dg_delete stimdg
    binary decode base64 $b64
}
set dg1 [dgbytes {1 2 3}]
set dg2 [dgbytes {7 8}]

set T 1761436800000000
set recs [list \
    [list ess/status        $T              1 boot] \
    [list stimdg            [expr {$T+500}]  6 $dg1] \
    [list eyetracking/raw   [expr {$T+1000}] 2 [binary format f* {1 2 3 4}]] \
    [list eventlog/events   [expr {$T+2000}] [evt 19] [binary format n 0]] \
    [list ess/status        [expr {$T+3000}] 1 in1] \
    [list eventlog/events   [expr {$T+4500}] [evt 20] [binary format n 0]] \
    [list ess/status        [expr {$T+5250}] 1 iti] \
    [list eyetracking/raw   [expr {$T+6000}] 2 [binary format f* {5 6}]] \
    [list stimdg            [expr {$T+6500}] 6 $dg2] \
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

    # DG datapoints: every occurrence, serialized and timed; decoded once
    check "$w: <dgbuf>stimdg keeps both occurrences" [dl_length $g:<dgbuf>stimdg:0] 2
    set back {}
    foreach k {0 1} {
        set bytes [binary format c* [dl_tcllist $g:<dgbuf>stimdg:0:$k]]
        set h2 [dg_fromString64 [binary encode base64 $bytes]]
        lappend back [dl_tcllist $h2:x]
        dg_delete $h2
    }
    check "$w: each buffer decodes back to its version" $back {{1 2 3} {7 8}}
    check "$w: <dgbuft>stimdg times on the first-record axis" \
        [lmap t [dl_tcllist $g:<dgbuft>stimdg:0] {format %.1f $t}] \
        [expr {$wide ? {0.5 6.5} : {0.0 6.0}}]
    check "$w: decoded once: one <stimdg>x column, the first version" \
        [list [llength [lsearch -all [dg_tclListnames $g] <stimdg>x]] [dl_tcllist $g:<stimdg>x]] \
        {1 {1 2 3}}

    # the anchor, wide only
    if {$wide} {
        check "wide: file_t0_us is the first record's dserv time" [dl_tcllist $g:file_t0_us] $T
        check "wide: obs_start_us in whole us from it" [dl_tcllist $g:obs_start_us] {2000 7000}
        check "wide: both int64" [list [dl_datatype $g:file_t0_us] [dl_datatype $g:obs_start_us]] {int64 int64}
    } else {
        check "narrow: no int64 anchor columns" \
            [list [dl_exists $g:file_t0_us] [dl_exists $g:obs_start_us]] {0 0}
    }
    dg_delete $g
}
dslog::wideTypes 0
file delete $path

if {$::fail} { puts "$::fail FAILED"; exit 1 }
puts "all passed"
exit 0
