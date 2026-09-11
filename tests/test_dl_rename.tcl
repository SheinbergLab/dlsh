#!/usr/bin/env dlsh
#
# test_dl_rename.tcl
#   dl_rename / dl_set onto names that look like Tcl literals.
#
#   `dl_set 2 $x` used to crash the interpreter.  After removing the target's
#   old hash entry, tclSetDynList re-resolved the target name with
#   tclFindDynList to decide whether it was an existing sublist -- and on a
#   colon-free name that lookup can only succeed by coercing a literal, so
#   "2" manufactured a temp list.  tclFindDynListParent then "succeeded" on
#   the same coercion path without setting parent/index, and the sublist
#   replacement freed whatever garbage those held (SIGBUS).  dl_rename is
#   dl_set + dl_delete, so it crashed the same way.  The behavioral
#   differential in tests/tools/ hits this on its first dl_rename pattern,
#   which meant nothing after dl_rename was ever compared.
#
#   Usage:  dlsh test_dl_rename.tcl
#   Exits non-zero on any failure.

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

set ::failures 0
proc check {label got want} {
    if {$got eq $want} {
        puts "OK   $label"
    } else {
        puts "FAIL $label -> got {$got} want {$want}"
        incr ::failures
    }
}
proc nlists {} { llength [dl_dir] }

###########################################################################
# 1. Literal-looking target names: the crash
###########################################################################

# The exact sequence the differential harness runs: rename a fixture onto
# another fixture's temp name, then rename a fresh fixture onto "2".
set a [dl_ilist 3 1 4 1 5]
set b [dl_ilist 2 7 1 8 2]
check "rename onto a live temp name" [catch {dl_rename $a $b}] 0
check "  ...source is gone" [dl_exists $a] 0
check "  ...target carries the source's data" [dl_tcllist $b] {3 1 4 1 5}

set c [dl_ilist 3 1 4 1 5]
check "rename onto numeric name \"2\"" [catch {dl_rename $c 2} r] 0
check "  ...list named 2 exists" [dl_exists 2] 1
check "  ...has the data" [dl_tcllist 2] {3 1 4 1 5}
check "  ...source handle is stale" [dl_exists $c] 0
dl_delete 2

# dl_set alone, with a shared handle (copy path) and an inline temp (move path).
set base [nlists]
set x [dl_ilist 1 2]
dl_set 2 $x
check "dl_set 2 \$x copies" [dl_tcllist 2] {1 2}
dl_append $x 9
check "  ...copy is independent" [list [dl_tcllist 2] [dl_tcllist $x]] {{1 2} {1 2 9}}
dl_delete 2 $x
check "  ...nothing left behind" [nlists] $base

dl_set 2 [dl_ilist 1 2]
check "dl_set 2 \[inline\] moves" [dl_tcllist 2] {1 2}
check "  ...exactly one list registered" [expr {[nlists] - $base}] 1
dl_delete 2
check "  ...freed" [nlists] $base

# Names that parse as multi-element or float Tcl lists are just names too.
dl_set {1 2} [dl_ilist 5 6]
check "name with a space" [dl_tcllist {1 2}] {5 6}
dl_delete {1 2}
dl_set 2.5 [dl_ilist 7]
check "float-looking name" [dl_tcllist 2.5] 7
dl_delete 2.5
check "  ...freed" [nlists] $base

###########################################################################
# 2. Colon targets still behave: real sublists are replaced in place,
#    selection temps are refused without losing the source.
###########################################################################

dl_set L [dl_llist [dl_ilist 1 2 3] [dl_ilist 4 5]]
set s [dl_ilist 7 8]
dl_set L:0 $s
check "dl_set onto sublist (copy)" [dl_tcllist L] {{7 8} {4 5}}
dl_set L:1 [dl_ilist 9]
check "dl_set onto sublist (move)" [dl_tcllist L] {{7 8} 9}
check "  ...shared source untouched" [dl_tcllist $s] {7 8}

set before [nlists]
set t [dl_ilist 0]
check "dl_set onto a selection is an error" \
    [catch {dl_set L:0:1 $t} r] 1
check "  ...message" $r {dl_set: temporary lists cannot be dl_set}
check "  ...source survives the error" [dl_tcllist $t] 0
check "  ...target untouched" [dl_tcllist L] {{7 8} 9}
dl_delete $t
# Inline temp on the same error path, inside a proc so frame exit is what
# reclaims it: the source used to be pulled out of the table before the
# check, so nothing owned it any more.
proc inline_onto_selection {} {
    return [catch {dl_set L:0:1 [dl_ilist 0]} r]
}
# The first run of ANY proc whose body errors on an inline temp leaves one
# list registered, on every build back to the installed one; the second and
# later runs are clean.  Not this bug, so measure a warmed-up call.
inline_onto_selection
set before [nlists]
check "inline source onto a selection is an error" [inline_onto_selection] 1
check "  ...frame exit reclaims the source" [nlists] $before
check "  ...target untouched" [dl_tcllist L] {{7 8} 9}
dl_delete L $s

###########################################################################

if {$::failures} {
    puts "FAILED: $::failures"
    exit 1
}
puts "ALL PASSED"
exit 0
