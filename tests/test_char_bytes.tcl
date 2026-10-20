#!/usr/bin/env dlsh
#
# test_char_bytes.tcl
#   A char dynlist (DF_CHAR) holds unsigned bytes, 0..255, on every
#   platform.  The C code used to read chars through plain `char`, which is
#   signed on macOS and x86 Linux but unsigned on ARM Linux, so the same
#   bytes read -1 on a Mac and 255 on a rig, and dl_mean/dl_gt/dl_sort of
#   byte data disagreed between the two.  Every expectation here is the
#   unsigned answer, so the test means the same thing on every platform.
#
#   Usage:  dlsh test_char_bytes.tcl        (exits non-zero on any failure)

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

# --- creation and element access ----------------------------------------
check "dl_clist reads unsigned"   [dl_tcllist [dl_clist 255 128 0]] {255 128 0}
check "dl_clist keeps low byte"   [dl_tcllist [dl_clist -1 256 -128 300]] {255 0 128 44}
set c [dl_clist 10 200 30]
check "dl_get"                    [dl_get $c 1] 200
dl_put $c 0 -1
dl_put $c 2 300
check "dl_put keeps low byte"     [dl_tcllist $c] {255 200 44}
dl_append $c 250; dl_prepend $c -2; dl_insert $c 1 129
check "append/prepend/insert"     [dl_tcllist $c] {254 129 255 200 44 250}
set fe {}
dl_foreach v [dl_clist 0 255] { lappend fe $v }
check "dl_foreach"                $fe {0 255}

# --- raw bytes in and out ------------------------------------------------
set all {}
for {set i 0} {$i < 256} {incr i} { lappend all $i }
set bytes [binary format c* $all]
set b [dl_create char]
dl_fromString $bytes $b
check "dl_fromString length"      [dl_length $b] 256
check "dl_fromString 0..255"      [dl_tcllist $b] $all
dl_toString $b out
check "dl_toString round trip"    [string equal $out $bytes] 1
check "dl_fromString + dl_uint"   [dl_tcllist [dl_uint $b]] $all

# --- reductions and comparisons see the unsigned values -----------------
set px [dl_clist 255 1]
check "dl_mean"                   [dl_mean $px] 128.0
check "dl_sum"                    [dl_sum $px] 256
check "dl_min / dl_max"           [list [dl_min $px] [dl_max $px]] {1 255}
check "dl_minIndex / dl_maxIndex" [list [dl_minIndex $px] [dl_maxIndex $px]] {1 0}
check "dl_gt char/char"           [dl_tcllist [dl_gt [dl_clist 200 10] [dl_clist 100]]] {1 0}
check "dl_lt char/char"           [dl_tcllist [dl_lt [dl_clist 200 10] [dl_clist 100]]] {0 1}
check "dl_gt char/float"          [dl_tcllist [dl_gt [dl_clist 200 10] 100.0]] {1 0}
check "dl_eq char/char"           [dl_tcllist [dl_eq [dl_clist 255 0] [dl_clist -1]]] {1 0}
check "dl_sort"                   [dl_tcllist [dl_sort [dl_clist 200 5 130 0]]] {0 5 130 200}
check "dl_sortIndices"            [dl_tcllist [dl_sortIndices [dl_clist 200 5 130 0]]] {3 1 2 0}
check "dl_unique"                 [dl_tcllist [dl_unique [dl_clist 200 5 200 0]]] {0 5 200}
check "dl_find"                   [dl_find [dl_clist 5 200 7] [dl_clist 200]] 1
check "dl_find -1 is 255"         [dl_find [dl_clist 5 255 7] [dl_clist -1]] 1
check "dl_meanList"               [dl_tcllist [dl_meanList [dl_llist [dl_clist 200 10] [dl_clist 100 20]]]] {150.0 15.0}
check "dl_sumList"                [dl_tcllist [dl_sumList [dl_llist [dl_clist 200 10] [dl_clist 100 20]]]] {300 30}
check "dl_select char 0/1 mask"   [dl_tcllist [dl_select [dl_ilist 1 2 3] [dl_clist 0 1 1]]] {2 3}
# any value > 1 makes a char selection list a list of indices, and an
# index above 127 used to read as negative (an out-of-range read)
check "dl_select char indices"    [dl_tcllist [dl_select [dl_series 0 199] [dl_clist 150 2]]] {150 2}
check "dl_abs is identity"        [dl_tcllist [dl_abs [dl_clist 200 3]]] {200 3}

# --- mixed with float and the wide types --------------------------------
check "char + float"              [dl_tcllist [dl_add [dl_clist 200 1] 100.0]] {300.0 101.0}
check "char + int64"              [dl_tcllist [dl_add [dl_clist 200 1] [dl_wlist 1 1]]] {201 2}
check "char * double"             [dl_tcllist [dl_mult [dl_clist 200 1] [dl_dlist 0.5]]] {100.0 0.5}
check "char > double"             [dl_tcllist [dl_gt [dl_clist 200 1] [dl_dlist 100]]] {1 0}

# --- mixed with long, short and integer literals: promote to the wider
#     type (these used to fail with "unable to compare/combine") ---------
set px [dl_clist 200 10 255]
set ln [dl_ilist 100 100 300]
set sh [dl_short [dl_ilist 100 100 300]]
# the result is as wide as the wider operand, so char * short wraps in a
# short (255 * 300 = 76500 -> 10964), as short * short does
foreach {what other wtype prod} [list long $ln long {20000 1000 76500} \
                                     short $sh short {20000 1000 10964}] {
    check "char > $what"          [dl_tcllist [dl_gt $px $other]] {1 0 0}
    check "char < $what"          [dl_tcllist [dl_lt $px $other]] {0 1 1}
    check "$what < char"          [dl_tcllist [dl_lt $other $px]] {1 0 0}
    check "char == $what"         [dl_tcllist [dl_eq $px [dl_add $other [dl_ilist 100 -90 -45]]]] {1 1 1}
    check "char + $what"          [dl_tcllist [dl_add $px $other]] {300 110 555}
    check "char + $what type"     [dl_datatype [dl_add $px $other]] $wtype
    check "char - $what"          [dl_tcllist [dl_sub $px $other]] {100 -90 -45}
    check "$what - char"          [dl_tcllist [dl_sub $other $px]] {-100 90 45}
    check "char * $what"          [dl_tcllist [dl_mult $px $other]] $prod
    check "char / $what"          [dl_tcllist [dl_div $px $other]] {2 0 0}
    check "$what / char"          [dl_tcllist [dl_div $other $px]] {0 10 1}
}
check "char > literal"            [dl_tcllist [dl_gt $px 100]] {1 0 1}
check "char < literal"            [dl_tcllist [dl_lt $px 128]] {0 1 0}
check "char == literal"           [dl_tcllist [dl_eq $px 255]] {0 0 1}
check "literal > char"            [dl_tcllist [dl_gt 128 $px]] {0 1 0}
check "char + literal"            [dl_tcllist [dl_add $px 100]] {300 110 355}
check "char + literal type"       [dl_datatype [dl_add $px 100]] long
check "char - literal"            [dl_tcllist [dl_sub $px 128]] {72 -118 127}
check "char * literal"            [dl_tcllist [dl_mult $px 2]] {400 20 510}
check "char / literal"            [dl_tcllist [dl_div $px 5]] {40 2 51}
check "literal / char"            [dl_tcllist [dl_div 1000 $px]] {5 100 3}
check "char mod literal"          [dl_tcllist [dl_mod $px 7]] {4 3 3}
check "char gt indices"           [dl_tcllist [dl_indices [dl_gt $px 100]]] {0 2}
check "short + long is long"      [dl_tcllist [dl_add [dl_short [dl_ilist 1 2]] [dl_ilist 70000 1]]] {70001 3}
check "empty char + long is long" [dl_datatype [dl_add [dl_create char] [dl_create long]]] long

# --- byte arithmetic stays a byte and wraps modulo 256 -------------------
check "char + char wraps"         [dl_tcllist [dl_add [dl_clist 200] [dl_clist 100]]] {44}
check "char / char unsigned"      [dl_tcllist [dl_div [dl_clist 200] [dl_clist 2]]] {100}
check "dl_diff wraps"             [dl_tcllist [dl_diff [dl_clist 10 5 7]]] {251 2}
check "dl_negate wraps"           [dl_tcllist [dl_negate [dl_clist 1 0 200]]] {255 0 56}
check "dl_int first for signed"   [dl_tcllist [dl_diff [dl_int [dl_clist 10 5]]]] {-5}

# --- conversions out of char --------------------------------------------
set hi [dl_clist 0 128 255]
foreach cmd {dl_int dl_short dl_int64 dl_uint dl_ushort dl_uint64} {
    check "$cmd of chars"         [dl_tcllist [$cmd $hi]] {0 128 255}
}
foreach cmd {dl_float dl_double dl_ufloat dl_udouble} {
    check "$cmd of chars"         [dl_tcllist [$cmd $hi]] {0.0 128.0 255.0}
}

# --- conversions into char: dl_char and dl_uchar are the same -----------
foreach cmd {dl_char dl_uchar} {
    check "$cmd of ints"          [dl_tcllist [$cmd [dl_ilist 0 255 256 -1 300]]] {0 255 0 255 44}
    check "$cmd of shorts"        [dl_tcllist [$cmd [dl_slist 255 -1]]] {255 255}
    check "$cmd of floats"        [dl_tcllist [$cmd [dl_flist 3.7 255.0 -1.0 256.0]]] {3 255 255 0}
    check "$cmd of doubles"       [dl_tcllist [$cmd [dl_dlist 200.9 -1 300]]] {200 255 44}
    check "$cmd of int64"         [dl_tcllist [$cmd [dl_wlist 255 256 -1]]] {255 0 255}
    check "$cmd of strings"       [dl_tcllist [$cmd [dl_create string 200 -1]]] {200 255}
    check "$cmd of chars"         [dl_tcllist [$cmd $hi]] {0 128 255}
}

# --- serialization: same bytes, read as unsigned ------------------------
set g [dg_create]
dl_set $g:px [dl_clist 0 127 128 255]
set f [file join [expr {[info exists ::env(TMPDIR)] ? $::env(TMPDIR) : "/tmp"}] \
           test_char_bytes_[pid].dg]
dg_write $g $f
set g2 [dg_read $f]
file delete $f
check "dg_write/dg_read"          [dl_tcllist $g2:px] {0 127 128 255}
check "dl_datatype after read"    [dl_datatype $g2:px] char
check "dl_toJSON"                 [string map {" " ""} [dl_toJSON [dl_clist 0 255]]] {[0,255]}
foreach x [lsort -unique [list $g $g2]] { dg_delete $x }

# --- an image from impro: its pixel buffers were always unsigned, so a
#     bright PNG decoded to a char list must read as bright here too -----
if {[catch {package require impro} msg]} {
    puts "SKIP impro PNG round trip (impro not loadable: [string range $msg 0 60])"
} else {
    set pngfile [file join [expr {[info exists ::env(TMPDIR)] ? $::env(TMPDIR) : "/tmp"}] \
                     test_char_bytes_[pid].png]
    # 4x2 RGB, every pixel (255, 200, 128)
    img_writePNG [dl_replicate [dl_clist 255 200 128] 8] 4 2 $pngfile
    set fh [open $pngfile rb]; set png [read $fh]; close $fh
    file delete $pngfile
    set img [img_listfromPNG $png]
    check "PNG dims"              [dl_tcllist $img:0] {4 2}
    check "PNG pixels are chars"  [dl_datatype $img:1] char
    check "PNG first pixel"       [dl_tcllist [dl_choose $img:1 [dl_ilist 0 1 2]]] {255 200 128}
    check "PNG dl_max"            [dl_max $img:1] 255
    check "PNG dl_sum"            [dl_sum $img:1] [expr {8 * (255 + 200 + 128)}]
}

if {$::fail} {
    puts "=== $::fail FAILURE(S) ==="
    exit 1
}
puts "=== ALL PASS ==="
