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

# --- arithmetic through the generic kernels (dlwide.c) -----------------
# Values chosen so that a float32 or int32 path would give a different
# answer: sums beyond 2^32, products beyond 2^53, fractions beyond 7 digits.
set W  [dl_wlist 5000000000 -1 7]
set D  [dl_dlist 0.1 0.2 0.3]
set I  [dl_ilist 1 2 3]
set F  [dl_flist 0.5 0.25 0.125]

check "int64+int64"        [dl_tcllist [dl_add $W $W]] {10000000000 -2 14}
check "int64+int64 type"   [dl_datatype [dl_add $W $W]] int64
check "int64+long"         [dl_tcllist [dl_add $W $I]] {5000000001 1 10}
check "int64+long type"    [dl_datatype [dl_add $W $I]] int64
check "int64+scalar"       [dl_tcllist [dl_add $W 1]] {5000000001 0 8}
check "scalar+int64"       [dl_tcllist [dl_sub 1 $W]] {-4999999999 2 -6}
check "int64*long exact"   [dl_tcllist [dl_mult $W 3]] {15000000000 -3 21}
check "int64*int64 wraps like int32 does" [dl_datatype [dl_mult $W $W]] int64
check "int64 div by zero"  [dl_tcllist [dl_div $W [dl_wlist 2 0 7]]] {2500000000 0 1}
check "int64+float -> double" [dl_datatype [dl_add $W $F]] double
check "int64+float value"  [dl_tcllist [dl_add [dl_wlist 5000000000] [dl_flist 0.5]]] 5000000000.5
check "double+double"      [dl_tcllist [dl_add $D $D]] {0.2 0.4 0.6}
check "double+long"        [dl_tcllist [dl_add $D $I]] {1.1 2.2 3.3}
check "double type"        [dl_datatype [dl_add $D $I]] double
check "double*scalar"      [dl_tcllist [dl_mult $D 10]] {1.0 2.0 3.0}
check "double pow"         [dl_tcllist [dl_pow [dl_dlist 2] [dl_dlist 0.5]]] [expr {pow(2,0.5)}]
check "int64 pow"          [dl_tcllist [dl_pow [dl_wlist 2] [dl_wlist 40]]] [expr {2**40}]
check "double fmod"        [dl_tcllist [dl_fmod [dl_dlist 5.5] 2]] 1.5
check "nested int64 arith" [dl_tcllist [dl_add [dl_llist $W $W] 1]] {{5000000001 0 8} {5000000001 0 8}}
check "empty wide arith"   [dl_datatype [dl_add [dl_wlist] $W]] int64
check "empty wide arith 2" [dl_datatype [dl_add [dl_dlist] $I]] double

check "int64 eq"           [dl_tcllist [dl_eq $W 5000000000]] {1 0 0}
check "int64 lt long"      [dl_tcllist [dl_lt $W 0]] {0 1 0}
check "int64 gte"          [dl_tcllist [dl_gte $W $I]] {1 0 1}
check "double eq exact"    [dl_tcllist [dl_eq [dl_dlist 0.1] 0.1]] 1
# literals parse at the operand's precision only when a wide list is present
check "int64 eq wide literal"   [dl_tcllist [dl_eq $W 5000000001]] {0 0 0}
check "int64 + wide literal"    [dl_tcllist [dl_add [dl_wlist 1] 5000000001]] 5000000002
check "double + literal exact"  [dl_tcllist [dl_sub [dl_dlist 0.3] 0.1]] [expr {0.3-0.1}]
check "legacy float + literal stays float" [dl_datatype [dl_add [dl_flist 1] 0.5]] float
check "legacy int + big literal stays float" [dl_datatype [dl_add [dl_ilist 1] 5000000000]] float
check "literal list in wide context" [dl_tcllist [dl_add $W {1 2 3}]] {5000000001 1 10}
check "double lt"          [dl_tcllist [dl_lt $D 0.25]] {1 1 0}
check "int64 eqIndex"      [dl_tcllist [dl_eqIndex $W 7]] 2
check "int64 gtIndex none" [dl_tcllist [dl_gtIndex $W 5000000000]] {}
check "int64 and/or"       [dl_tcllist [dl_or [dl_wlist 0 0 5] [dl_wlist 0 1 0]]] {0 1 1}
check "int64 mod exact"    [dl_tcllist [dl_mod [dl_wlist 5000000001] 1000000]] 1
check "int64 mod type"     [dl_datatype [dl_mod $W 3]] int64
check "double mod"         [dl_tcllist [dl_mod [dl_dlist 5.5] 2]] 1.5
check "int64 oneof"        [dl_tcllist [dl_oneof $W [dl_wlist 7 -1]]] {0 1 1}
check "int64 not"          [dl_tcllist [dl_not [dl_wlist 0 5]]] {1 0}
check "double where"       [dl_tcllist [dl_where [dl_ilist 1 0 1] $D 9]] {0.1 9.0 0.3}
check "where int64/long -> int64" [dl_datatype [dl_where [dl_ilist 1 0 1] $W $I]] int64
check "where int64/long value"    [dl_tcllist [dl_where [dl_ilist 1 0 1] $W $I]] {5000000000 2 7}

# --- reductions ---------------------------------------------------------
check "sum int64 exact"    [dl_sum $W] 5000000006
check "sum double"         [dl_sum $D] [expr {0.1+0.2+0.3}]
check "prod int64"         [dl_prod [dl_wlist 3000000000 3]] 9000000000
check "prods int64 overflow -> double" [dl_datatype [dl_prods [dl_llist [dl_wlist 5000000000 5000000000]]]] double
check "sums int64"         [dl_tcllist [dl_sums [dl_llist $W $I]]] {5000000006 6}
check "sums int64 type"    [dl_datatype [dl_sums [dl_llist $W $I]]] int64
check "sums mixed double"  [dl_datatype [dl_sums [dl_llist $D $W]]] double
check "cumsum int64"       [dl_tcllist [dl_cumsum $W]] {5000000000 4999999999 5000000006}
check "cumsum double"      [dl_tcllist [dl_cumsum [dl_dlist 0.5 0.25]]] {0.5 0.75}
check "mean int64"         [dl_mean $W] [expr {5000000006/3.0}]
check "mean double"        [dl_mean $D] [expr {(0.1+0.2+0.3)/3.0}]
check "means"              [dl_tcllist [dl_means [dl_llist $D $D]]] [list [expr {(0.1+0.2+0.3)/3.0}] [expr {(0.1+0.2+0.3)/3.0}]]
check "var double"         [expr {abs([dl_var [dl_dlist 1 2 3 4]] - 5.0/3) < 1e-12}] 1
check "std int64"          [expr {abs([dl_std [dl_wlist 1 2 3 4]] - sqrt(5.0/3)) < 1e-12}] 1
check "min int64"          [dl_min $W] -1
check "max int64"          [dl_max $W] 5000000000
check "max nested mixed"   [dl_max [dl_llist $I $W]] 5000000000
check "min double"         [dl_min $D] 0.1
check "mins"               [dl_tcllist [dl_mins [dl_llist $W $D]]] {-1.0 0.1}
check "maxs int64 type"    [dl_datatype [dl_maxs [dl_llist $W $I]]] int64
check "minIndex/maxIndex"  [list [dl_minIndex $W] [dl_maxIndex $W]] {1 0}
check "maxPositions"       [dl_tcllist [dl_maxPositions [dl_llist $W $D]]] {0 2}
check "any/all int64"      [list [dl_any [dl_wlist 0 0 1]] [dl_all [dl_wlist 1 0]]] {1 0}
check "anys double"        [dl_tcllist [dl_anys [dl_llist [dl_dlist 0 0] [dl_dlist 0 0.5]]]] {0 1}

# --- elementwise math ---------------------------------------------------
check "abs int64 exact"    [dl_tcllist [dl_abs [dl_wlist -5000000000 3]]] {5000000000 3}
check "abs int64 type"     [dl_datatype [dl_abs $W]] int64
check "abs double"         [dl_tcllist [dl_abs [dl_dlist -0.1]]] 0.1
check "sqrt int64 -> double" [dl_tcllist [dl_sqrt [dl_wlist 4]]] 2.0
check "floor double -> int64" [dl_tcllist [dl_floor [dl_dlist 1e15 -2.5]]] {1000000000000000 -3}
check "round double type"  [dl_datatype [dl_round $D]] int64
check "sin double"         [dl_tcllist [dl_sin [dl_dlist 0]]] 0.0
check "negate int64"       [dl_tcllist [dl_negate $W]] {-5000000000 1 -7}
check "sign double"        [dl_tcllist [dl_sign [dl_dlist -0.5 0 2]]] {-1.0 0.0 1.0}
check "diff int64"         [dl_tcllist [dl_diff $W]] {-5000000001 8}
check "gradient double"    [dl_tcllist [dl_gradient [dl_dlist 0 1 4]]] {1.0 2.0 3.0}

# --- ordering -----------------------------------------------------------
check "sort int64"         [dl_tcllist [dl_sort $W]] {-1 7 5000000000}
check "sort double"        [dl_tcllist [dl_sort [dl_dlist 0.3 0.1 0.2]]] {0.1 0.2 0.3}
check "sortIndices int64"  [dl_tcllist [dl_sortIndices $W]] {1 2 0}
check "bsort nested"       [dl_tcllist [dl_bsort [dl_llist $W [dl_dlist 2 1]]]] {{-1 7 5000000000} {1.0 2.0}}
check "unique int64"       [dl_tcllist [dl_unique [dl_wlist 7 5000000000 7 -1]]] {-1 7 5000000000}
check "uniqueNoSort double" [dl_tcllist [dl_uniqueNoSort [dl_dlist 0.2 0.2 0.1]]] {0.2 0.1}
check "rank int64 (occurrence index, as for ints)" [dl_tcllist [dl_rank [dl_wlist 5 1 3 5]]] {0 0 0 1}
check "recode double"      [dl_tcllist [dl_recode [dl_dlist 0.5 0.1 0.5]]] {1 0 1}
check "find int64"         [dl_tcllist [dl_find $W [dl_wlist -1 7]]] 1
check "median (Tcl proc over sort/get)" [dl_median $W] 7

# --- still guarded: no verified implementation yet ----------------------
foreach {label script} {
    "dl_findIndices int64"     { dl_findIndices $W $W }
    "dl_countOccurences int64" { dl_countOccurences $W $W }
    "dl_hist double"           { dl_hist $D 0 1 2 }
    "dl_bmeans int64"          { dl_bmeans [dl_llist $W] }
} {
    errors $label $script
}

# The guard must see through every way of naming a list.
set nested [dl_llist [dl_ilist 1 2 3] $w]
set gg [dg_create]
dl_set $gg:rows [dl_llist [dl_dlist 0.5] [dl_ilist 4]]
errors "guard: nested list"        { dl_hist $nested 0 1 2 }
errors "guard: list:index"         { dl_findIndices $nested:1 $nested:1 }
errors "guard: group:list"         { dl_bmeans $gg:rows }
errors "guard: group:list:index"   { dl_countOccurences $gg:rows:0 $gg:rows:0 }
check  "guard leaves int sublist usable" [dl_sum $nested:0] 6
check  "guard leaves int row usable"     [dl_sum $gg:rows:1] 4
check  "medians over wide rows"          [dl_tcllist [dl_medians [dl_llist [dl_ilist 1 2 3] [dl_wlist 5 1 3]]]] {2.0 3.0}

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
