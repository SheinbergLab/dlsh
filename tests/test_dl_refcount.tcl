#!/usr/bin/env dlsh
#
# test_dl_refcount.tcl
#   Refcounted dynlist handles: a list that escapes the frame that made it
#   stays alive, and is freed once nothing refers to it any more.
#
#   Historically `set x [dl_add $a $b]` copied only the *name* "%list7%";
#   ownership stayed with a hidden variable in the creating frame, so the list
#   died at frame exit and any escaped copy of the name dangled. That is what
#   dl_local / dl_yield existed to work around. With handles, plain `set` and
#   plain `return` keep the list alive on their own.
#
#   The lifetime rule is additive: a list is freed when BOTH the frame claim
#   and the last object reference are gone. So this file has to prove two
#   things that pull in opposite directions -- escaped lists survive, and
#   nothing leaks -- which is why every case checks the dl_dir census rather
#   than just the value.
#
#   Usage:  dlsh test_dl_refcount.tcl ?rounds?
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
proc ok {name got want} {
    if {$got eq $want} {
        puts "OK   $name"
    } else {
        puts "FAIL $name -> got {$got} want {$want}"
        incr ::failures
    }
}

# Number of dynlists currently registered. Every case runs its work inside a
# proc and compares this before and after, so a lost free shows up exactly.
proc nlists {} { llength [dl_dir] }

###########################################################################
# 1. Escape paths that used to dangle
###########################################################################

proc esc_return {} {
    set x [dl_add [dl_ilist 1 2 3] 10]
    return $x
}
ok "escape by return" [dl_tcllist [esc_return]] {11 12 13}

proc esc_global {} {
    global g_escaped
    set g_escaped [dl_mult [dl_ilist 1 2 3] 3]
}
esc_global
ok "escape into a global" [dl_tcllist $g_escaped] {3 6 9}

proc esc_maker {n} {
    set x [dl_mult [dl_ilist 1 2 3] $n]
    return $x
}
proc esc_accumulate {} {
    set acc {}
    foreach n {1 2 3} { lappend acc [esc_maker $n] }
    set out {}
    foreach l $acc { lappend out [dl_tcllist $l] }
    return $out
}
ok "escape into an accumulator" [esc_accumulate] {{1 2 3} {2 4 6} {3 6 9}}

proc esc_dict {} {
    dict set d a [dl_add [dl_ilist 1 2] 1]
    dict set d b [dl_add [dl_ilist 1 2] 2]
    return "[dl_tcllist [dict get $d a]] / [dl_tcllist [dict get $d b]]"
}
ok "escape into a dict" [esc_dict] {2 3 / 3 4}

# Nested frames: the value is handed up three levels by plain `return`.
proc lvl3 {} { set x [dl_fromto 0 4]; return $x }
proc lvl2 {} { return [lvl3] }
proc lvl1 {} { return [lvl2] }
ok "escape up three frames" [dl_tcllist [lvl1]] {0 1 2 3}

###########################################################################
# 2. Existing idioms must behave exactly as before
###########################################################################

proc blessed {} {
    dl_local x [dl_add [dl_ilist 1 2 3] 10]
    dl_yield $x
}
ok "dl_local + dl_yield still work" [dl_tcllist [blessed]] {11 12 13}

proc yielded_local {} {
    dl_local out [dl_ilist 5 6]
    dl_yield $out
}
ok "dl_yield of a dl_local" [dl_tcllist [yielded_local]] {5 6}

# Dynlists are objects, not values: two names for one list alias, and a
# mutator is visible through both. This is pre-existing behavior and the
# refcount must not silently change it into copy-on-write.
proc alias_mutate {} {
    dl_local a [dl_ilist 1 2 3]
    set b $a
    dl_append $b 4
    return "[dl_tcllist $a] | [dl_tcllist $b]"
}
ok "aliases still share (reference semantics)" [alias_mutate] {1 2 3 4 | 1 2 3 4}

###########################################################################
# 3. Nothing leaks
###########################################################################

# A list built and dropped inside a proc is gone when the proc returns.
proc churn_local {} {
    set x [dl_add [dl_ilist 1 2 3] 1]
    set y [dl_mult $x 2]
    return [dl_sum $y]
}
set before [nlists]
for {set i 0} {$i < 200} {incr i} { churn_local }
ok "in-frame temps are reclaimed" [expr {[nlists] - $before}] 0

# A list that escapes, then loses its last reference, is reclaimed too.
proc churn_escape {} { set x [dl_fromto 0 50]; return $x }
set before [nlists]
for {set i 0} {$i < 200} {incr i} {
    set tmp [churn_escape]
    unset tmp
}
ok "escaped lists are reclaimed on unset" [expr {[nlists] - $before}] 0

# Overwriting the variable drops the old reference just as unset does.
set before [nlists]
set slot [churn_escape]
for {set i 0} {$i < 200} {incr i} { set slot [churn_escape] }
unset slot
ok "overwriting a variable frees the old list" [expr {[nlists] - $before}] 0

# The blessed idiom must not have regressed into a leak either.
#
# Note this has to be measured from inside a proc. dl_yield hands its result
# up by binding a ">N<" variable in the CALLER's frame; when the caller is the
# global scope that variable is never unset, so the list lives until dl_clean.
# That is long-standing dl_yield behavior -- master grows by exactly the same
# 200 lists here -- and is not something refcounting changes either way.
proc churn_yield {n} {
    for {set i 0} {$i < $n} {incr i} {
        set tmp [blessed]
        unset tmp
    }
}
set before [nlists]
churn_yield 200
ok "dl_yield results are reclaimed" [expr {[nlists] - $before}] 0

# An accumulator holds its lists for as long as it exists, and no longer.
set before [nlists]
proc build_acc {n} {
    set acc {}
    for {set i 0} {$i < $n} {incr i} { lappend acc [dl_fromto 0 10] }
    return $acc
}
set acc [build_acc 50]
ok "accumulator keeps its lists alive" [expr {[nlists] - $before}] 50
ok "  and they are all usable" [dl_tcllist [lindex $acc 17]] {0 1 2 3 4 5 6 7 8 9}
unset acc
ok "accumulator release frees them all" [expr {[nlists] - $before}] 0

###########################################################################
# 4. dl_delete
#
# SEMANTIC CHANGE, flagged for review. On master, dl_delete of a temp frees
# it immediately and any variable still holding the name dangles:
#
#     set x [dl_ilist 1 2 3]; dl_delete $x; dl_tcllist $x   ;# -> error
#
# dl_delete works by unsetting the list's hidden frame variable, i.e. by
# releasing the *frame's* claim. With handles, a live reference is a second
# claim, so the list now survives until that reference goes away too -- the
# delete is deferred rather than ignored. That is the safe direction (it
# cannot produce a use-after-free), and it is the only place the additive
# rule is visible from a script. The alternative -- make dl_delete
# authoritative and invalidate outstanding handles -- is a one-line change in
# tclDeleteDynList if that is preferred.
###########################################################################

proc delete_referenced {} {
    set x [dl_ilist 1 2 3]
    dl_delete $x
    if {[catch {dl_tcllist $x} e]} { return "errored" }
    return $e
}
ok "dl_delete defers while a reference lives" [delete_referenced] {1 2 3}

# With no live reference, dl_delete frees immediately as it always has.
# NB: [set name "$x"] would NOT do -- Tcl hands back the very same object, so
# `name` would still be a reference. Build a genuinely separate string.
proc delete_unreferenced {} {
    set x [dl_ilist 1 2 3]
    set name ""
    append name [string range $x 0 end]
    unset x
    dl_delete $name
    if {[catch {dl_tcllist $name} e]} { return "errored" }
    return "still alive: $e"
}
ok "dl_delete frees an unreferenced list" [delete_unreferenced] {errored}

# A named list has no hidden frame variable, so dl_delete is immediate.
proc delete_named {} {
    dl_set named_victim [dl_ilist 1 2 3]
    dl_delete named_victim
    if {[catch {dl_tcllist named_victim} e]} { return "errored" }
    return "still alive: $e"
}
ok "dl_delete of a named list is immediate" [delete_named] {errored}

###########################################################################
# 5. KNOWN LIMITATION: shimmering drops the last reference
#
# A Tcl_Obj holds one internal rep at a time. Converting a dynlist handle to
# another type -- [string length], [string range], [llength], [lindex] all do
# this -- frees the dynlist internal rep, which drops the reference. If that
# was the last one and the creating frame has already exited, the list is
# freed while the variable still holds its name.
#
# This is NOT a regression: on master the same variable was already dangling
# the moment its frame exited, so nothing that used to work stops working.
# It is an unfixed corner of the new guarantee, and it is why `set` cannot yet
# be advertised as unconditionally safe.
#
# The failure is a clean "not found" error, never a crash or a bad read.
#
# Fixing it properly means not freeing on refcount-zero at all, but handing
# the list back to the CURRENT frame as an ordinary temp, so the name stays
# resolvable for the rest of that frame. That needs a safe way to re-arm a
# frame claim from inside the Tcl_Obj free proc, which runs without an interp
# and sometimes during frame teardown -- deliberately left for review rather
# than rushed.
#
# These cases assert today's behavior so that a future fix trips them.
###########################################################################

proc shimmer_escape {} { set x [dl_ilist 1 2 3]; return $x }
proc shimmer_probe {script} {
    set keep [shimmer_escape]      ;# escaped, so no frame claim remains
    eval $script
    if {[catch {dl_tcllist $keep}]} { return "lost" }
    return "survives"
}

ok "shimmer: string length loses it" [shimmer_probe {string length $keep}] {lost}
ok "shimmer: llength loses it"       [shimmer_probe {llength $keep}]       {lost}
ok "shimmer: lindex loses it"        [shimmer_probe {lindex $keep 0}]      {lost}
# Operations that only need the string rep leave the handle intact.
ok "no shimmer: string compare"      [shimmer_probe {if {$keep eq ""} {}}] {survives}
ok "no shimmer: append elsewhere"    [shimmer_probe {set s ""; append s $keep}] {survives}
ok "no shimmer: dl_* commands"       [shimmer_probe {dl_length $keep}]     {survives}

###########################################################################

if {$::failures} {
    puts "FAILURES: $::failures"
    exit 1
}
puts "all refcount tests passed"
exit 0
