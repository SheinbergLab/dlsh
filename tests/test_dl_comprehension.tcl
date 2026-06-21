#!/usr/bin/env dlsh
#
# test_dl_comprehension.tcl
#   Correctness test for the embedded comprehension layer (dl_map / dl_filter /
#   dl_reduce / dl_comp). These procs are built into libdlsh from
#   src/dl_comprehension.tcl and Tcl_Eval'd in Dl_Init, so they must be present
#   immediately after the package loads -- this test does NOT source the .tcl.
#
#   Usage:  dlsh test_dl_comprehension.tcl        (exits non-zero on failure)

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
    if {$got eq $want} { puts "OK   $label" } \
    else { puts "FAIL $label -> got {$got} want {$want}"; incr ::fail }
}

# the verbs must exist from load alone (embedded, not sourced)
foreach c {dl_map dl_filter dl_reduce dl_comp} {
    check "embedded: $c present" [llength [info commands $c]] 1
}

set xs   [dl_fromto 0 10]
set rows [dl_llist [dl_ilist 1 2 3] [dl_ilist 10 20] [dl_ilist 5]]

# dl_map -- type inference + per-row mapping
check "map int (auto)"    [dl_tcllist [dl_map x $xs {expr {$x*$x+1}}]] {1 2 5 10 17 26 37 50 65 82}
check "map float (auto)"  [dl_tcllist [dl_map x $xs {expr {$x/2.0}}]] {0.0 0.5 1.0 1.5 2.0 2.5 3.0 3.5 4.0 4.5}
check "map string (auto)" [dl_tcllist [dl_map x [dl_ilist 1 2 3] {format v%d $x}]] {v1 v2 v3}
check "map over rows"     [dl_tcllist [dl_map r $rows {dl_sum $r}]] {6 30 5}
check "map type override"  [dl_datatype [dl_map x $xs {expr {$x*$x}} float]] float
check "map empty"         [dl_tcllist [dl_map x [dl_ilist] {expr {$x+1}}]] {}

# dl_filter -- flat and per-row
check "filter flat" [dl_tcllist [dl_filter x $xs {expr {$x>5 && $x%3==0}}]] {6 9}
check "filter rows" [dl_tcllist [dl_filter r $rows {expr {[dl_length $r]>=2}}]] {{1 2 3} {10 20}}

# dl_reduce -- left fold to a scalar
check "reduce sum" [dl_reduce a x $xs {expr {$a+$x}} 0] 45
check "reduce max" [dl_reduce a x $xs {expr {max($a,$x)}} -1] 9

# dl_comp -- where / map / both / errors
check "comp where+map" [dl_tcllist [dl_comp x $xs -where {expr {$x>5}} -map {expr {$x*$x}}]] {36 49 64 81}
check "comp map only"  [dl_tcllist [dl_comp x $xs -map {expr {$x+100}}]] {100 101 102 103 104 105 106 107 108 109}
check "comp where only" [dl_tcllist [dl_comp x $xs -where {expr {$x%2}}]] {1 3 5 7 9}
check "comp bad opt errors" [catch { dl_comp x $xs -nope {1} }] 1

# lifetime: usable in caller scope, and re-returnable through a proc
set r [dl_map x [dl_ilist 1 2 3] {expr {$x*10}}]
check "result usable in caller" [dl_tcllist $r] {10 20 30}
proc _mk {} { return [dl_return [dl_map x [dl_ilist 1 2 3] {expr {$x*10}}]] }
check "re-returned through proc" [dl_tcllist [_mk]] {10 20 30}

if {$::fail} { puts "=== $::fail FAILURE(S) ==="; exit 1 }
puts "=== ALL PASS ==="
