#!/usr/bin/env dlsh
#
# test_dslog_tables.tcl
#   The per-datapoint readers of the dslog package: dslog::read with
#   -match / -absolute, dslog::unpack and dslog::jsonTable, against a
#   fixture written with dslog::open / dslog::put (tests/dslog_fixture.tcl).
#   Also pins the no-option dslog::read output for that fixture to a golden
#   transcript taken from the implementation before these readers existed.
#
#   Usage:  dlsh test_dslog_tables.tcl        (exits non-zero on any failure)
#
#   With the lab's real file present it also prints (never asserts) the
#   wall time of each reader on it.

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
# 1.1 is the version with unpack/jsonTable; requiring it also makes the
# test fail at once if it was pointed at an old installed dlsh.zip
package require dslog 1.1

source [file join [file dirname [info script]] dslog_fixture.tcl]

set ::fail 0
proc check {label got want} {
    if {$got eq $want} {
        puts "OK   $label"
    } else {
        puts "FAIL $label -> got {$got} want {$want}"
        incr ::fail
    }
}
proc errors {label script {pattern *}} {
    set rc [catch {uplevel 1 $script} msg]
    if {$rc && [string match $pattern $msg]} {
        puts "OK   $label (error: [string range $msg 0 90])"
    } elseif {$rc} {
        puts "FAIL $label -> error {$msg} does not match {$pattern}"
        incr ::fail
    } else {
        puts "FAIL $label -> succeeded with {$msg}"
        incr ::fail
    }
}
# doubles: compare to 1e-9 so the test does not depend on print formatting
proc near {label got want} {
    set ok [expr {[llength $got] == [llength $want]}]
    if {$ok} {
        foreach g $got w $want {
            if {abs($g - $w) > 1e-9} { set ok 0; break }
        }
    }
    if {$ok} { puts "OK   $label" } else {
        puts "FAIL $label -> got {$got} want {$want}"; incr ::fail
    }
}
proc isnan {v} { expr {[string tolower $v] eq "nan"} }

set INT64_MIN -9223372036854775808
set T $::dslog_fixture_t0

set chan [file tempfile fix dslog_tables]
close $chan
dslog_fixture_write $fix
set put_names {}; set put_ts {}
foreach rec [dslog_fixture_records] {
    lappend put_names [lindex $rec 0]
    lappend put_ts [lindex $rec 1]
}

# ---------------------------------------------------------------------
# 1. The no-option read is unchanged: golden transcript from the previous
#    implementation (wideTypes off, the default).
# ---------------------------------------------------------------------
check "wideTypes default" [dslog::wideTypes] 0
set g [dslog::read $fix]
check "read: names" [dl_tcllist $g:varname] \
    {logger:open evt:3:1 ess/status eyetracking/raw stim/frame stim/cmd stim/frame stim/cmd stim/cmd stim/frame misc/json misc/json str/json bad/json bad/json mixed/json mixed/json short/frame short/frame dbl/arr sh/arr int/arr arr/json mix/arr mix/arr}
check "read: timestamp type" [dl_datatype $g:timestamp] float
check "read: timestamps" [dl_tcllist $g:timestamp] \
    {0.0 0.0 0.0010000000474974513 0.0020000000949949026 0.003000000026077032 0.0034000000450760126 0.019700000062584877 0.019999999552965164 0.035999998450279236 0.036400001496076584 0.03999999910593033 0.04100000113248825 0.041999999433755875 0.0430000014603138 0.04399999976158142 0.04500000178813934 0.04600000008940697 0.04699999839067459 0.04800000041723251 0.04899999871850014 0.05000000074505806 0.050999999046325684 0.052000001072883606 0.05299999937415123 0.05400000140070915}
check "read: vals" [dl_tcllist $g:vals] \
    [list 3 42 running {1.5 -2.0 3.25 4.0} {} [list {{"seq":1,"cmd":"setBackground 0","t_send":1761436800003100,"t_ret":1761436800003400,"dq":0.5,"done":1,"swap":100,"flip":1761436800003300,"status":"ok"}}] {} [list {{"seq":2,"cmd":"!show 1","t_send":1761436800019800,"t_ret":1761436800020000,"dq":null,"done":null,"swap":null,"flip":null,"status":"nohdr"}}] [list {{"seq":3,"hash":"abc123","len":5000,"t_send":1761436800035000,"t_ret":1761436800036000,"dq":2,"done":3.5,"swap":102,"flip":1761436800035900,"status":"ok"}}] {} [list {{"a":1,"b":"x","z":null}}] [list {{"b":"y","c":true,"z":null,"nested":{"k":1},"arr":[1,2]}}] [list {{"v":2.5}}] [list {{"ok":1}}] [list {{not json}}] [list {{"k":1}}] [list {{"k":"s"}}] {} {} {1.5 2.5} {1 -2 3} {7 -8} [list {[1,2]}] {} {1.0 2.0}]
set types {}; dl_foreach v $g:vals { lappend types [dl_datatype $v] }
check "read: val types" $types \
    {long long string float long string long string string long string string string string string string string long long float short long string long float}
set full_names [dl_tcllist $g:varname]
set full_vals [dl_tcllist $g:vals]
set full_ts [dl_tcllist $g:timestamp]
dg_delete $g

# ---------------------------------------------------------------------
# 2. -match: only the matching names, same values as the full read
# ---------------------------------------------------------------------
set f [dslog::read $fix -match stim/frame -match stim/cmd]
check "match: names" [dl_tcllist $f:varname] \
    {stim/frame stim/cmd stim/frame stim/cmd stim/cmd stim/frame}
set want_vals {}; set want_ts {}
foreach n $full_names v $full_vals t $full_ts {
    if {$n in {stim/frame stim/cmd}} { lappend want_vals $v; lappend want_ts $t }
}
check "match: vals" [dl_tcllist $f:vals] $want_vals
check "match: timestamps (still from the file's first record)" \
    [dl_tcllist $f:timestamp] $want_ts
check "match: timestamp type" [dl_datatype $f:timestamp] float
dg_delete $f

set e [dslog::read $fix -match evt:*]
check "match evt:*: names" [dl_tcllist $e:varname] {evt:3:1}
check "match evt:*: vals"  [dl_tcllist $e:vals] {42}
dg_delete $e

set e [dslog::read $fix -match logger:*]
check "match logger:*: the synthetic row is filtered like any other" \
    [dl_tcllist $e:varname] {logger:open}
dg_delete $e

set e [dslog::read $fix -match nothing/*]
check "match with no hits: empty columns" \
    [list [dl_length $e:varname] [dl_length $e:timestamp] [dl_length $e:vals]] {0 0 0}
dg_delete $e

set e [dslog::read $fix -match {*/arr}]
check "match glob: names" [dl_tcllist $e:varname] {dbl/arr sh/arr int/arr mix/arr mix/arr}
dg_delete $e

errors "match: missing pattern" { dslog::read $fix -match } "*-match requires a pattern*"
errors "read: bad option" { dslog::read $fix -bogus } "*bad option*"

dslog::wideTypes 1
set f [dslog::read $fix -match stim/frame]
check "match + wideTypes: int64 payloads decoded" [dl_tcllist $f:vals] \
    [list [list 1 100 [expr {$T+2950}] 5000 0 1 2] \
          [list 2 101 [expr {$T+19600}] 5100 0 0 0] \
          [list 3 102 [expr {$T+36300}] 5200 1 2 1]]
check "match + wideTypes: timestamp type" [dl_datatype $f:timestamp] double
dg_delete $f
dslog::wideTypes 0

# ---------------------------------------------------------------------
# 3. -absolute: epoch microseconds, exactly as put
# ---------------------------------------------------------------------
set a [dslog::read $fix -absolute]
check "absolute: timestamp type" [dl_datatype $a:timestamp] int64
check "absolute: names unchanged" [dl_tcllist $a:varname] $full_names
check "absolute: vals unchanged"  [dl_tcllist $a:vals] $full_vals
check "absolute: record timestamps equal the put timestamps" \
    [lrange [dl_tcllist $a:timestamp] 1 end] $put_ts
check "absolute: logger:open sits on the first record" \
    [dl_get $a:timestamp 0] $T
dg_delete $a

set a [dslog::read $fix -match stim/frame -absolute]
check "absolute + match: timestamps" [dl_tcllist $a:timestamp] \
    [list [expr {$T+3000}] [expr {$T+19700}] [expr {$T+36400}]]
check "absolute + match: names" [dl_tcllist $a:varname] {stim/frame stim/frame stim/frame}
dg_delete $a

dslog::wideTypes 1
set a [dslog::read $fix -absolute -match ess/*]
check "absolute ignores wideTypes" [dl_datatype $a:timestamp] int64
check "absolute + match: values" [dl_tcllist $a:vals] {running}
dg_delete $a
dslog::wideTypes 0

# ---------------------------------------------------------------------
# 4. unpack
# ---------------------------------------------------------------------
set fields {frame swapcount flip_us ticks_us missed cmds_applied ds_dispatched}
set fr [dslog::unpack $fix stim/frame $fields]
check "unpack: group name is a fresh handle" [string match "group*" $fr] 1
check "unpack: columns" [dg_tclListnames $fr] [concat timestamp seconds $fields]
set types {}
foreach c [dg_tclListnames $fr] { lappend types [dl_datatype $fr:$c] }
check "unpack: types" $types {int64 double int64 int64 int64 int64 int64 int64 int64}
check "unpack: rows" [dl_length $fr:frame] 3
check "unpack: timestamp" [dl_tcllist $fr:timestamp] \
    [list [expr {$T+3000}] [expr {$T+19700}] [expr {$T+36400}]]
near  "unpack: seconds from the first record" [dl_tcllist $fr:seconds] {0.003 0.0197 0.0364}
check "unpack: frame"      [dl_tcllist $fr:frame] {1 2 3}
check "unpack: swapcount"  [dl_tcllist $fr:swapcount] {100 101 102}
check "unpack: flip_us"    [dl_tcllist $fr:flip_us] \
    [list [expr {$T+2950}] [expr {$T+19600}] [expr {$T+36300}]]
check "unpack: ticks_us"   [dl_tcllist $fr:ticks_us] {5000 5100 5200}
check "unpack: missed"     [dl_tcllist $fr:missed] {0 0 1}
check "unpack: cmds_applied" [dl_tcllist $fr:cmds_applied] {1 0 2}
check "unpack: ds_dispatched" [dl_tcllist $fr:ds_dispatched] {2 0 1}
check "unpack: independent of wideTypes" [dslog::wideTypes] 0
dg_delete $fr

set d [dslog::unpack $fix dbl/arr {x y}]
check "unpack double: types" [list [dl_datatype $d:x] [dl_datatype $d:y]] {double double}
check "unpack double: values" [list [dl_tcllist $d:x] [dl_tcllist $d:y]] {1.5 2.5}
dg_delete $d
set d [dslog::unpack $fix sh/arr {a b c}]
check "unpack short: types" [dl_datatype $d:b] short
check "unpack short: values" [list [dl_tcllist $d:a] [dl_tcllist $d:b] [dl_tcllist $d:c]] {1 -2 3}
dg_delete $d
set d [dslog::unpack $fix int/arr {p q}]
check "unpack int: widened to int64" [dl_datatype $d:q] int64
check "unpack int: values" [list [dl_tcllist $d:p] [dl_tcllist $d:q]] {7 -8}
dg_delete $d
set d [dslog::unpack $fix eyetracking/raw {x1 y1 x2 y2}]
check "unpack float: types" [dl_datatype $d:x2] float
check "unpack float: values" \
    [list [dl_tcllist $d:x1] [dl_tcllist $d:y1] [dl_tcllist $d:x2] [dl_tcllist $d:y2]] \
    {1.5 -2.0 3.25 4.0}
dg_delete $d

errors "unpack: short record names the record and both counts" \
    { dslog::unpack $fix short/frame $fields } \
    "*short/frame record 1 (file record 17) has 5 values, expected 7*"
errors "unpack: wrong field count" \
    { dslog::unpack $fix stim/frame {a b} } \
    "*stim/frame record 0 (file record 3) has 7 values, expected 2*"
errors "unpack: payload type changes mid-file" \
    { dslog::unpack $fix mix/arr {a b} } \
    "*mix/arr record 1 (file record 23) is double, expected int64*"
errors "unpack: not an array payload" \
    { dslog::unpack $fix stim/cmd {a} } "*stim/cmd record 0 * is json, not a fixed-width array*"
errors "unpack: unknown datapoint" \
    { dslog::unpack $fix no/such {a} } {*no records named "no/such"*}
errors "unpack: empty field list" { dslog::unpack $fix stim/frame {} } "*field list is empty*"
errors "unpack: missing file" { dslog::unpack /no/such/file.ess stim/frame $fields } "*not found*"
errors "unpack: arity" { dslog::unpack $fix stim/frame } "*wrong # args*"

# ---------------------------------------------------------------------
# 5. jsonTable
# ---------------------------------------------------------------------
set c [dslog::jsonTable $fix stim/cmd]
check "jsonTable: group name is a fresh handle" [string match "group*" $c] 1
check "jsonTable: columns in first-seen order" [dg_tclListnames $c] \
    {timestamp seconds seq cmd t_send t_ret dq done swap flip status hash len}
set types {}
foreach col [dg_tclListnames $c] { lappend types [dl_datatype $c:$col] }
check "jsonTable: types" $types \
    {int64 double int64 string int64 int64 double double int64 int64 string string int64}
check "jsonTable: rows" [dl_length $c:seq] 3
check "jsonTable: timestamp" [dl_tcllist $c:timestamp] \
    [list [expr {$T+3400}] [expr {$T+20000}] [expr {$T+36000}]]
near  "jsonTable: seconds" [dl_tcllist $c:seconds] {0.0034 0.02 0.036}
check "jsonTable: seq" [dl_tcllist $c:seq] {1 2 3}
check "jsonTable: cmd (missing string -> \"\")" [dl_tcllist $c:cmd] {{setBackground 0} {!show 1} {}}
check "jsonTable: hash (missing string -> \"\")" [dl_tcllist $c:hash] {{} {} abc123}
check "jsonTable: len (missing int64 -> INT64_MIN)" [dl_tcllist $c:len] [list $INT64_MIN $INT64_MIN 5000]
check "jsonTable: swap (null int64 -> INT64_MIN)" [dl_tcllist $c:swap] [list 100 $INT64_MIN 102]
check "jsonTable: flip" [dl_tcllist $c:flip] [list [expr {$T+3300}] $INT64_MIN [expr {$T+35900}]]
check "jsonTable: t_send" [dl_tcllist $c:t_send] \
    [list [expr {$T+3100}] [expr {$T+19800}] [expr {$T+35000}]]
check "jsonTable: status" [dl_tcllist $c:status] {ok nohdr ok}
# dq: real first, then null, then integer -> double column, int widened
lassign [dl_tcllist $c:dq] dq0 dq1 dq2
check "jsonTable: dq real then int" [list $dq0 [isnan $dq1] $dq2] {0.5 1 2.0}
# done: integer first, then null, then real -> promoted to double
lassign [dl_tcllist $c:done] d0 d1 d2
check "jsonTable: done promoted int -> double" [list $d0 [isnan $d1] $d2] {1.0 1 3.5}
dg_delete $c

set m [dslog::jsonTable $fix misc/json]
check "jsonTable: differing key sets, nested/arr skipped" [dg_tclListnames $m] \
    {timestamp seconds a b z c}
check "jsonTable: a (absent in record 1)" [dl_tcllist $m:a] [list 1 $INT64_MIN]
check "jsonTable: b" [dl_tcllist $m:b] {x y}
check "jsonTable: c (absent in record 0, boolean -> 1)" [dl_tcllist $m:c] [list $INT64_MIN 1]
check "jsonTable: z (null everywhere -> int64 markers)" \
    [list [dl_datatype $m:z] [dl_tcllist $m:z]] [list int64 [list $INT64_MIN $INT64_MIN]]
dg_delete $m

set s [dslog::jsonTable $fix str/json]
check "jsonTable: string datapoint holding JSON" \
    [list [dl_datatype $s:v] [dl_tcllist $s:v]] {double 2.5}
dg_delete $s

errors "jsonTable: bad JSON names the record" \
    { dslog::jsonTable $fix bad/json } "*bad/json record 1 (file record 13):*"
errors "jsonTable: string/number conflict" \
    { dslog::jsonTable $fix mixed/json } \
    {*mixed/json record 1 (file record 15): key "k" is a string here but an integer before*}
errors "jsonTable: top-level array is not a row" \
    { dslog::jsonTable $fix arr/json } "*arr/json record 0 (file record 21) is not a JSON object*"
errors "jsonTable: not a JSON datapoint" \
    { dslog::jsonTable $fix stim/frame } "*stim/frame record 0 * is int64, not json*"
errors "jsonTable: unknown datapoint" \
    { dslog::jsonTable $fix no/such } {*no records named "no/such"*}
errors "jsonTable: missing file" { dslog::jsonTable /no/such/file.ess x } "*not found*"
errors "jsonTable: arity" { dslog::jsonTable $fix } "*wrong # args*"

# ---------------------------------------------------------------------
# 5b. Group naming: every reader hands back a FRESH group<N> by default, so
#     reading the same file or datapoint again just works; -name pins a
#     name and replaces an existing group of that name.
# ---------------------------------------------------------------------
set c1 [dslog::jsonTable $fix stim/cmd]
set c2 [dslog::jsonTable $fix stim/cmd]
check "jsonTable: second read gets its own group" [expr {$c1 ne $c2 && [dg_exists $c1] && [dg_exists $c2]}] 1
check "jsonTable: fresh names follow dg_create" [string match "group*" $c2] 1
check "jsonTable: same content" [dl_tcllist $c2:seq] [dl_tcllist $c1:seq]
dg_delete $c1; dg_delete $c2
set r1 [dslog::read $fix]
set r2 [dslog::read $fix -match stim/frame]
check "read: second read of one file gets its own group" [expr {$r1 ne $r2 && [dg_exists $r1] && [dg_exists $r2]}] 1
check "read: fresh name, not the path" [string match "group*" $r1] 1
dg_delete $r1; dg_delete $r2
set u1 [dslog::unpack $fix stim/frame $fields]
set u2 [dslog::unpack $fix stim/frame $fields]
check "unpack: second read gets its own group" [expr {$u1 ne $u2}] 1
dg_delete $u1; dg_delete $u2
set n1 [dslog::jsonTable $fix stim/cmd -name cmds]
check "jsonTable -name: pinned" $n1 cmds
set n2 [dslog::jsonTable $fix stim/cmd -name cmds]
check "jsonTable -name again: replaces, same handle" $n2 cmds
check "jsonTable -name: old group gone, new one present" [expr {[dg_exists cmds] && [llength [dg_tclListnames cmds]] > 2}] 1
check "read -name: pinned" [dslog::read $fix -absolute -name flat] flat
check "unpack -name: pinned" [dslog::unpack $fix stim/frame $fields -name frames] frames
check "unpack -name: replaces" [dslog::unpack $fix stim/frame $fields -name frames] frames
dg_delete cmds; dg_delete flat; dg_delete frames
errors "read: -name without a value" { dslog::read $fix -name } "*requires a group name*"
errors "unpack: bad trailing option" { dslog::unpack $fix stim/frame $fields -bogus x } "*wrong # args*"

check "wideTypes left at its default" [dslog::wideTypes] 0
file delete $fix

# ---------------------------------------------------------------------
# 6. Timing on the lab's real file, when present (printed, not asserted,
#    except the row count and the shape of the frame log)
# ---------------------------------------------------------------------
set real /Users/sheinb/data/ess/human_runner_door-preview6-go_261025200108.ess
if {[file exists $real]} {
    proc ms {script} {
        set us [lindex [time [list uplevel 1 $script]] 0]
        format %.1f [expr {$us / 1000.0}]
    }
    dslog::wideTypes 1
    puts "TIMING dslog::read                     [ms {set g [dslog::read $real]}] ms ([dl_length $g:varname] rows)"
    dg_delete $g
    puts "TIMING dslog::read -match stim/frame   [ms {set g [dslog::read $real -match stim/frame]}] ms ([dl_length $g:varname] rows)"
    dg_delete $g
    dslog::wideTypes 0
    puts "TIMING dslog::unpack stim/frame        [ms {set fr [dslog::unpack $real stim/frame $fields]}] ms ([dl_length $fr:frame] rows)"
    puts "TIMING dslog::jsonTable stim/cmd       [ms {set c [dslog::jsonTable $real stim/cmd]}] ms ([dl_length $c:seq] rows, columns: [dg_tclListnames $c])"
    dg_delete $c

    check "real: stim/frame rows" [dl_length $fr:frame] 11426
    # flip_us must climb strictly within each run of consecutive frames;
    # a run ends where the frame counter does not advance by one (a
    # stim2 restart / log gap)
    set dframe [dl_diff $fr:frame]
    set dflip  [dl_diff $fr:flip_us]
    set inrun  [dl_eq $dframe 1]
    set bad    [dl_sum [dl_and $inrun [dl_lte $dflip 0]]]
    puts "real: [dl_sum $inrun] consecutive-frame steps, [expr {[dl_length $dframe] - [dl_sum $inrun]}] run breaks"
    check "real: flip_us strictly increasing within runs" $bad 0
    dg_delete $fr
}

if {$::fail} {
    puts "FAILURES: $::fail"
    exit 1
}
puts "=== ALL PASS ==="
