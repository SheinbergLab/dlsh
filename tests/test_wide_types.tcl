#!/usr/bin/env dlsh
#
# test_wide_types.tcl
#   Storage-level support for the two 8-byte element types, DF_INT64 and
#   DF_DOUBLE (dg tags 11 and 12): creation, element access, conversion,
#   every serialization path, and -- just as important -- that everything
#   NOT yet taught about them fails with an error rather than a wrong answer.
#
#   Usage:  dlsh test_wide_types.tcl        (exits non-zero on any failure)

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

set ::fail 0
proc check {label got want} {
    if {$got eq $want} {
        puts "OK   $label"
    } else {
        puts "FAIL $label -> got {$got} want {$want}"
        incr ::fail
    }
}
proc errors {label script} {
    set rc [catch {uplevel 1 $script} msg]
    if {$rc} { puts "OK   $label (error: [string range $msg 0 60])" } \
    else     { puts "FAIL $label -> succeeded with {$msg}"; incr ::fail }
}

# Values that do not survive int32 or float32.
set BIG   9007199254740993     ;# 2^53 + 1: exact in int64, not in double
set US    1757606400123456     ;# a microsecond epoch timestamp
set PI    3.141592653589793

# --- creation ---------------------------------------------------------
set w [dl_wlist $BIG -1 $US]
set d [dl_dlist $PI 1e300 -0.5]
check "dl_wlist datatype" [dl_datatype $w] int64
check "dl_dlist datatype" [dl_datatype $d] double
check "dl_create int64"   [dl_datatype [dl_create int64 1 2]] int64
check "dl_create wide"    [dl_datatype [dl_create wide 1 2]] int64
check "dl_create double"  [dl_datatype [dl_create double 1.5]] double
check "int64 exact"       [dl_tcllist $w] [list $BIG -1 $US]
check "double exact"      [dl_tcllist $d] [list $PI 1e+300 -0.5]
check "length"            [list [dl_length $w] [dl_length $d]] {3 3}

# --- element access ---------------------------------------------------
dl_append $w 7; dl_prepend $w -$BIG; dl_insert $w 2 42
check "int64 append/prepend/insert" [dl_tcllist $w] [list -$BIG $BIG 42 -1 $US 7]
dl_append $d 2.5; dl_prepend $d -1e-300; dl_insert $d 1 0.25
check "double append/prepend/insert" [dl_tcllist $d] [list -1e-300 0.25 $PI 1e+300 -0.5 2.5]
check "dl_get int64"  [dl_get $w 1] $BIG
check "dl_get double" [dl_get $d 2] $PI
dl_put $w 0 $US; dl_put $d 0 $PI
check "dl_put int64"  [dl_get $w 0] $US
check "dl_put double" [dl_get $d 0] $PI
errors "int64 rejects non-integer" { dl_append $w 1.5 }
errors "int64 rejects overflow"    { dl_append $w 99999999999999999999 }
set a {}; dl_foreach x $w { lappend a $x }; check "dl_foreach int64" $a [dl_tcllist $w]
set a {}; dl_foreach x $d { lappend a $x }; check "dl_foreach double" $a [dl_tcllist $d]

# --- copy / reverse / select ------------------------------------------
set wc [dl_copy $w]; dl_append $wc 1
check "dl_copy independent"  [expr {[dl_length $wc] - [dl_length $w]}] 1
check "dl_copy contents"     [lrange [dl_tcllist $wc] 0 end-1] [dl_tcllist $w]
check "dl_reverse int64"     [dl_tcllist [dl_reverse $w]] [lreverse [dl_tcllist $w]]
check "dl_reverse double"    [dl_tcllist [dl_reverse $d]] [lreverse [dl_tcllist $d]]
check "dl_select int64"      [dl_tcllist [dl_select $w [dl_ilist 1 0 0 0 0 1]]] [list $US 7]
check "dl_choose double"     [dl_tcllist [dl_choose $d [dl_ilist 2 0]]] [list $PI $PI]
check "nested in llist"      [dl_tcllist [dl_llist $w $d]] [list [dl_tcllist $w] [dl_tcllist $d]]

# --- conversion --------------------------------------------------------
check "dl_int64 of ilist"     [dl_tcllist [dl_int64 [dl_ilist 1 -2 3]]] {1 -2 3}
check "dl_int64 datatype"     [dl_datatype [dl_int64 [dl_ilist 1]]] int64
check "dl_double of flist"    [dl_datatype [dl_double [dl_flist 1.5]]] double
check "dl_double of ilist"    [dl_tcllist [dl_double [dl_ilist 1 2]]] {1.0 2.0}
check "dl_int of wlist"       [dl_tcllist [dl_int [dl_wlist 5 -6]]] {5 -6}
check "dl_float of dlist"     [dl_tcllist [dl_float [dl_dlist 0.5]]] 0.5
check "dl_int64 of double"    [dl_tcllist [dl_int64 [dl_dlist 2.9 -2.9]]] {2 -2}
check "dl_double of slist"    [dl_tcllist [dl_double [dl_slist 1.25 -3]]] {1.25 -3.0}
check "dl_int64 of slist"     [dl_tcllist [dl_int64 [dl_slist $BIG]]] $BIG
# chars print as signed through dl_tcllist; match the existing int path
check "dl_uchar of int64"     [dl_tcllist [dl_uchar [dl_wlist 255 256]]] [dl_tcllist [dl_uchar [dl_ilist 255 256]]]
check "dl_int64 of uchar"     [dl_tcllist [dl_uint64 [dl_char -1]]] 255
check "empty conversion"      [dl_datatype [dl_double [dl_ilist]]] double

# --- dg round trips ----------------------------------------------------
set g [dg_create]
dl_set $g:w [dl_copy $w]
dl_set $g:d [dl_copy $d]
dl_set $g:nested [dl_llist [dl_wlist $BIG] [dl_dlist $PI]]
dl_set $g:i [dl_ilist 1 2 3]

set tmp [file join [expr {[info exists ::env(TMPDIR)] ? $::env(TMPDIR) : "/tmp"}] wide_types_[pid]]
dg_write $g $tmp.dgz
set r [dg_read $tmp.dgz]
check "dgz int64"        [dl_tcllist $r:w] [dl_tcllist $w]
check "dgz double"       [dl_tcllist $r:d] [dl_tcllist $d]
check "dgz nested"       [dl_tcllist $r:nested] [list $BIG $PI]
check "dgz types"        [list [dl_datatype $r:w] [dl_datatype $r:d]] {int64 double}
check "dgz untouched int" [dl_tcllist $r:i] {1 2 3}

dg_write $g $tmp.dg
set r2 [dg_read $tmp.dg]
check "uncompressed dg int64" [dl_tcllist $r2:w] [dl_tcllist $w]
check "uncompressed dg double" [dl_tcllist $r2:d] [dl_tcllist $d]
file delete $tmp.dgz $tmp.dg

dg_toString $g bytes
set r3 [dg_fromString $bytes]
check "dg_toString/fromString"   [dl_tcllist $r3:w] [dl_tcllist $w]
dg_toString64 $g b64
set r4 [dg_fromString64 $b64]
check "dg_toString64/fromString64" [dl_tcllist $r4:d] [dl_tcllist $d]

set n [dl_toString $w raw]
check "dl_toString bytes" $n [expr {8*[dl_length $w]}]
set back [dl_wlist]; dl_fromString $raw $back
check "dl_fromString int64" [dl_tcllist $back] [dl_tcllist $w]
dl_toString64 $d raw64
set backd [dl_dlist]; dl_fromString64 $raw64 $backd
check "dl_fromString64 double" [dl_tcllist $backd] [dl_tcllist $d]

# --- other serializers -------------------------------------------------
set j [dg_toJSON $g]
check "json has int64"  [expr {[string first $BIG $j] >= 0}] 1
check "json has double" [expr {[string first 3.14159265358979 $j] >= 0}] 1

if {[llength [info commands dg_toMsgpackData]]} {
    dg_toMsgpackData $g mp
    check "msgpack produces bytes" [expr {[string length $mp] > 0}] 1
}
if {[llength [info commands dg_toArrow]] && [llength [info commands dg_fromArrow]]} {
    # Arrow is a table: every column the same number of rows, and a nested
    # column's children all one type.  So build a group to those rules
    # rather than reusing the mixed nested list above.
    set ga [dg_create]
    dl_set $ga:w [dl_copy $w]
    dl_set $ga:d [dl_copy $d]
    dl_set $ga:n [dl_llist [dl_wlist $BIG] [dl_wlist 1 2] [dl_wlist 3] \
		      [dl_wlist 4] [dl_wlist 5] [dl_wlist 6]]
    dg_toArrow $ga arrow
    dg_fromArrow $arrow wideArrow
    check "arrow int64 round trip"  [dl_tcllist wideArrow:w] [dl_tcllist $w]
    check "arrow double round trip" [dl_tcllist wideArrow:d] [dl_tcllist $d]
    check "arrow nested int64"      [dl_tcllist wideArrow:n] [list $BIG {1 2} 3 4 5 6]
    check "arrow types" [list [dl_datatype wideArrow:w] [dl_datatype wideArrow:d]] {int64 double}
}

# --- not-yet-supported operations must error, never answer wrong -----
foreach {label script} {
    "dl_sum int64"      { dl_sum $w }
    "dl_add int64"      { dl_add $w $w }
    "dl_sort int64"     { dl_sort $w }
    "dl_mean double"    { dl_mean $d }
    "dl_min double"     { dl_min $d }
    "dl_add double"     { dl_add $d 1 }
    "dl_eq int64"       { dl_eq $w 7 }
    "dl_unique double"  { dl_unique $d }
    "dl_cumsum int64"   { dl_cumsum $w }
} {
    errors $label $script
}

# The guard must see through every way of naming a list.
set nested [dl_llist [dl_ilist 1 2 3] $w]
set gg [dg_create]
dl_set $gg:rows [dl_llist [dl_dlist 0.5] [dl_ilist 4]]
errors "guard: nested list"        { dl_sums $nested }
errors "guard: list:index"         { dl_median $nested:1 }
errors "guard: group:list"         { dl_means $gg:rows }
errors "guard: group:list:index"   { dl_sum $gg:rows:0 }
errors "guard: dl_medians (Tcl proc over list:index)" { dl_medians $nested }
check  "guard leaves int sublist usable" [dl_sum $nested:0] 6
check  "guard leaves int row usable"     [dl_sum $gg:rows:1] 4

# Comprehensions infer int64 for values that do not fit in 32 bits.
check "dl_map over int64" [dl_tcllist [dl_map x $w {expr {$x * 2}}]] \
    [lmap x [dl_tcllist $w] {expr {$x * 2}}]
check "dl_map result type" [dl_datatype [dl_map x $w {expr {$x * 2}}]] int64
check "dl_map small ints stay int" [dl_datatype [dl_map x [dl_ilist 1 2] {expr {$x * 2}}]] long

if {$::fail} {
    puts "=== $::fail FAILURE(S) ==="
    exit 1
}
puts "=== ALL PASS ==="
