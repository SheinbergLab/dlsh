#!/usr/bin/env dlsh
#
# test_dg_concat.tcl
#   Correctness test for dg_concat (concatenate groups/files into a fresh
#   dynGroup with a strict schema check) and a regression guard for dg_read,
#   whose format-detection was refactored into the shared dgReadFromFile helper
#   that dg_concat also uses. Exercises every read path (.dg / .dgz / .lz4 +
#   the extensionless .dg/.dgz fallback) so the refactor stays behaviour-
#   identical.
#
#   Usage:  dlsh test_dg_concat.tcl        (exits non-zero on failure)
#           dlsh --libdlsh ./libdlsh.dylib test_dg_concat.tcl   (dev build)

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
proc errcheck {label script} {
    if {[catch {uplevel 1 $script}]} { puts "OK   $label (errored as expected)" } \
    else { puts "FAIL $label -> expected an error, got none"; incr ::fail }
}

set tmp [file tempdir]

# ----- a small two-block fixture, written in every supported format -----
proc mkblock {name ids rts} {
    set g [dg_create]
    dl_set $g:id $ids
    dl_set $g:rt $rts
    dg_rename $g $name
    return $name
}
set A [mkblock blkA [dl_ilist 1 2 3]   [dl_flist 0.1 0.2 0.3]]
set B [mkblock blkB [dl_ilist 4 5]     [dl_flist 0.4 0.5]]

# ===== dg_concat: in-memory =====
set g [dg_concat $A $B]
check "concat mem: n"        [dl_length $g:id]   5
check "concat mem: ids"      [dl_tcllist $g:id]  {1 2 3 4 5}
check "concat mem: rt order" [format %.2f [lindex [dl_tcllist $g:rt] 3]] 0.40
check "concat leaves A"      [dl_length $A:id]   3
check "concat leaves B"      [dl_length $B:id]   2
check "concat is new group"  [expr {$g ne $A && $g ne $B}] 1

# ===== dg_read format round-trips (refactor guard) =====
dg_write $A [file join $tmp rt.dg]
dg_write $A [file join $tmp rt.dgz]
dg_write $A [file join $tmp rt.lz4]
foreach {label file} [list "read .dg" rt.dg "read .dgz" rt.dgz "read .lz4" rt.lz4] {
    set r [dg_read [file join $tmp $file]]
    check "$label: ids" [dl_tcllist $r:id] {1 2 3}
}
# extensionless name -> .dg fallback
check "read no-ext (.dg fallback)" [dl_tcllist [dg_read [file join $tmp rt]]:id] {1 2 3}

# ===== dg_concat: from files via glob =====
dg_write $A [file join $tmp part_1.dgz]
dg_write $B [file join $tmp part_2.dgz]
set gf [dg_concat {*}[lsort -dictionary [glob [file join $tmp part_*.dgz]]]]
check "concat files: ids" [dl_tcllist $gf:id] {1 2 3 4 5}

# ===== dg_concat: mixed group + file =====
set gm [dg_concat $A [file join $tmp part_2.dgz]]
check "concat mixed: n" [dl_length $gm:id] 5

# ===== single argument behaves like a copy =====
set gs [dg_concat $A]
check "concat single: ids"   [dl_tcllist $gs:id] {1 2 3}
check "concat single is copy" [expr {$gs ne $A}] 1

# ===== strict schema enforcement =====
set Cextra [dg_create]
dl_set $Cextra:id [dl_ilist 9]
dl_set $Cextra:rt [dl_flist 0.9]
dl_set $Cextra:extra [dl_ilist 9]
errcheck "reject extra column" { dg_concat $A $Cextra }

set Dtype [dg_create]
dl_set $Dtype:id [dl_flist 9.0]   ;# id is float here, long in A
dl_set $Dtype:rt [dl_flist 0.9]
errcheck "reject datatype mismatch" { dg_concat $A $Dtype }

errcheck "reject missing file" { dg_concat $A [file join $tmp no_such.dgz] }

if {$::fail} { puts "=== $::fail FAILURE(S) ==="; exit 1 }
puts "=== ALL PASS ==="
