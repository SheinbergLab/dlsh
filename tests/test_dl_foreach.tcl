#!/usr/bin/env dlsh
#
# test_dl_foreach.tcl
#   Correctness + regression test for dl_foreach.
#
#   History: dl_foreach was registered but effectively untested -- it mutated
#   a single shared Tcl_Obj as the loop variable (panics modern Tcl:
#   "...called with shared object"), and its datatype switch was missing the
#   DF_CHAR and DF_LIST cases (uninitialized return / no iteration). This test
#   locks in the reworked behavior: fresh per-iteration value, all leaf types,
#   nested-list "map over rows", and proper break/continue/return handling.
#
#   Usage:  dlsh test_dl_foreach.tcl        (exits non-zero on any failure)

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

set ::fail 0
proc check {label got want} {
    if {$got eq $want} {
        puts "OK   $label"
    } else {
        puts "FAIL $label -> got {$got} want {$want}"
        incr ::fail
    }
}

# --- flat leaf datatypes (every one used to crash on the first iteration) ---
set a {}; dl_foreach x [dl_ilist 10 20 30]              { lappend a $x }; check "long"   $a {10 20 30}
set a {}; dl_foreach x [dl_short [dl_ilist 1 2 3]]      { lappend a $x }; check "short"  $a {1 2 3}
set a {}; dl_foreach x [dl_flist 1.5 2.5]               { lappend a $x }; check "float"  $a {1.5 2.5}
set a {}; dl_foreach x [dl_uchar [dl_ilist 65 66 67]]   { lappend a $x }; check "char"   $a {65 66 67}
set a {}; dl_foreach x [dl_slist p q r]                 { lappend a $x }; check "string" $a {p q r}

# --- negatives / signedness ---
set a {}; dl_foreach x [dl_ilist -3 -2 -1]          { lappend a $x }; check "negative long"  $a {-3 -2 -1}
set a {}; dl_foreach x [dl_flist -1.5 2.5 -3.5]     { lappend a $x }; check "negative float" $a {-1.5 2.5 -3.5}
set a {}; dl_foreach x [dl_short [dl_ilist -100 100]] { lappend a $x }; check "negative short" $a {-100 100}

# loop var may be an array element
set a {}; dl_foreach arr(k) [dl_ilist 1 2 3] { lappend a $arr(k) }; check "array-element var" $a {1 2 3}

# --- nested list-of-lists: loop var bound to each sublist as a real dynlist ---
set a {}
dl_foreach row [dl_llist [dl_ilist 1 2] [dl_ilist 3 4 5]] {
    lappend a [list [dl_length $row] [dl_sum $row] [dl_tcllist $row]]
}
check "nested rows (map)" $a {{2 3 {1 2}} {3 12 {3 4 5}}}

# deep nesting (depth 3): body gets a nested dynlist it can collapse
set a {}
dl_foreach blk [dl_llist [dl_llist [dl_ilist 1 2] [dl_ilist 3]] [dl_llist [dl_ilist 4 5 6]]] {
    lappend a [list [dl_length $blk] [dl_tcllist [dl_collapse $blk]]]
}
check "deep nesting (depth 3)" $a {{2 {1 2 3}} {1 {4 5 6}}}

# heterogeneous leaf types across rows
set a {}
dl_foreach row [dl_llist [dl_ilist 1 2] [dl_flist 1.5 2.5] [dl_slist x y]] { lappend a [dl_tcllist $row] }
check "mixed-type rows" $a {{1 2} {1.5 2.5} {x y}}

# empty sublist as a row
set a {}; dl_foreach row [dl_llist [dl_ilist] [dl_ilist 7]] { lappend a [dl_length $row] }
check "empty sublist row" $a {0 1}

# nested dl_foreach (reentrancy, distinct vars)
set pairs {}
dl_foreach r [dl_llist [dl_ilist 1 2] [dl_ilist 3 4]] { dl_foreach v $r { lappend pairs $v } }
check "nested dl_foreach" $pairs {1 2 3 4}

# the bound value is a real, mutable copy -- editing it must not touch the parent
set N [dl_llist [dl_ilist 1 2] [dl_ilist 3 4]]
dl_foreach row $N { dl_append $row 99 }
check "parent intact after body edits copy" [dl_tcllist $N] {{1 2} {3 4}}

# the per-row map expresses the N-way interleave broadcast cleanly
set out {}
dl_foreach row [dl_llist [dl_ilist 1 2] [dl_ilist 3 4]] {
    lappend out [dl_tcllist [dl_interleave $row [dl_ilist 10 20] [dl_ilist 100 200]]]
}
check "per-row interleave map" $out {{1 10 100 2 20 200} {3 10 100 4 20 200}}

# --- loop control ---
set a {}; dl_foreach x [dl_ilist 1 2 3 4 5] { if {$x==3} continue; lappend a $x }
check "continue" $a {1 2 4 5}
set a {}; dl_foreach x [dl_ilist 1 2 3 4 5] { if {$x==3} break; lappend a $x }
check "break" $a {1 2}
proc _usesret {} { dl_foreach x [dl_ilist 1 2 3] { if {$x==2} { return early } }; return late }
check "return propagates" [_usesret] early

# --- edge cases ---
set a done; dl_foreach x [dl_ilist] { set a ran }; check "empty list" $a done
check "error in body propagates" [catch { dl_foreach x [dl_ilist 1 2] { error boom } }] 1
dl_foreach x [dl_ilist 1 2 3] { }; check "loop var unset after" [info exists x] 0

# --- stress: per-element temp lists churn through create+free without crashing ---
set rows {}
for {set i 0} {$i < 2000} {incr i} { lappend rows [dl_ilist $i [expr {$i*2}]] }
set total 0
dl_foreach row [dl_llist {*}$rows] { set total [expr {$total + [dl_sum $row]}] }
check "stress 2000 nested rows" $total [expr {[dl_sum [dl_fromto 0 2000]]*3}]

if {$::fail} {
    puts "=== $::fail FAILURE(S) ==="
    exit 1
}
puts "=== ALL PASS ==="
