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

# An escaped list whose last reference goes away is freed promptly -- the
# drop arrives while its frame is still unwinding, so dlRefDrop frees it
# rather than handing it upward. Retention therefore matches master exactly:
# plain temps accumulate until the frame exits (long-standing dlsh behavior,
# and the reason test_leak_dl_foreach exists), escaped+dropped lists do not
# accumulate at all. Measured on master: 100 and 0, identical to here.
proc churn_escape {} { set x [dl_fromto 0 50]; return $x }

proc retention_inside {} {
    set b [nlists]
    for {set i 0} {$i < 100} {incr i} { dl_ilist 1 2 3 }   ;# plain temps
    set plain [expr {[nlists] - $b}]
    set b [nlists]
    for {set i 0} {$i < 100} {incr i} { set t [churn_escape]; unset t }
    set escaped [expr {[nlists] - $b}]
    return "$plain $escaped"
}
set before [nlists]
ok "retention matches master" [retention_inside] {100 0}
ok "  and everything is reclaimed at frame exit" [expr {[nlists] - $before}] 0

# Overwriting the variable drops the old reference just as unset does.
proc churn_overwrite {} {
    set b [nlists]
    set slot [churn_escape]
    for {set i 0} {$i < 100} {incr i} { set slot [churn_escape] }
    return [expr {[nlists] - $b}]
}
set before [nlists]
churn_overwrite
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
proc build_acc {n} {
    set acc {}
    for {set i 0} {$i < $n} {incr i} { lappend acc [dl_fromto 0 10] }
    return $acc
}
proc acc_lifecycle {} {
    set b [nlists]
    set acc [build_acc 50]
    set held [expr {[nlists] - $b}]
    set sample [dl_tcllist [lindex $acc 17]]
    unset acc
    return [list $held $sample]
}
set before [nlists]
lassign [acc_lifecycle] held sample
ok "accumulator keeps its lists alive" $held 50
ok "  and they are all usable" $sample {0 1 2 3 4 5 6 7 8 9}
ok "accumulator release frees them all" [expr {[nlists] - $before}] 0

###########################################################################
# 4. dl_delete is authoritative
#
# dl_delete means "this list is gone now", not "release my claim on it", and
# that holds however many references are outstanding -- same as master:
#
#     set x [dl_ilist 1 2 3]; dl_delete $x; dl_tcllist $x   ;# -> error
#
# It is the one place a script can outrank the refcount. Outstanding
# references stay safe: tclDeleteDynList detaches their handles before the
# free, so they hold an inert handle and report a missing list rather than
# reading freed memory. The handle keeps the list's name so the error names
# the right list even if that object never generated a string rep.
###########################################################################

proc make_escaped {} { set x [dl_ilist 1 2 3]; return $x }

proc delete_referenced {} {
    set x [dl_ilist 1 2 3]
    set name ""
    append name [string range $x 0 end]      ;# remember which list it was
    dl_delete $x
    if {![catch {dl_tcllist $x} e]} { return "still alive" }
    # The error must name the deleted list, not "" -- that is what the name
    # kept in the detached handle is for.
    if {[string match "*\"$name\"*not found*" $e]} { return "errored, named" }
    return "errored, but unhelpfully: $e"
}
ok "dl_delete beats a live reference" [delete_referenced] {errored, named}

# Deleting through one of two aliases takes the list out from under both.
proc delete_aliased {} {
    set x [dl_ilist 1 2 3]
    set y $x
    dl_delete $x
    return "[catch {dl_tcllist $x}] [catch {dl_tcllist $y}]"
}
ok "  and out from under an alias too" [delete_aliased] {1 1}

# An escaped list -- no frame claim left, only a reference -- is deletable.
proc delete_escaped {} {
    set k [make_escaped]
    dl_delete $k
    if {[catch {dl_tcllist $k}]} { return "errored" }
    return "still alive"
}
ok "  and an escaped list as well" [delete_escaped] {errored}

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
# 5. Shimmering must not lose the list
#
# A Tcl_Obj carries one internal rep at a time. Converting a dynlist handle to
# another type -- [llength], [lindex], [string length], [string range] all do
# this -- makes Tcl generate the name into the string rep and then discard our
# internal rep, which drops the reference. If that was the last reference, a
# naive refcount frees the list right there, while the variable still spells
# its name.
#
# dlRefDrop handles this by not freeing an unreferenced-but-registered list at
# all: it hands it to the current frame as an ordinary temp (dlReclaimInFrame),
# so the name stays resolvable for the rest of that frame. Safe even when the
# drop happens during frame teardown, because Tcl_PopCallFrame unlinks the
# frame before deleting its locals, so the claim lands in the caller's frame.
###########################################################################

proc shimmer_escape {} { set x [dl_ilist 1 2 3]; return $x }
proc shimmer_probe {script} {
    set keep [shimmer_escape]      ;# escaped, so no frame claim remains
    eval $script
    if {[catch {dl_tcllist $keep}]} { return "lost" }
    return "survives"
}

# These four convert the object to another type and drop the reference.
ok "shimmer: llength"       [shimmer_probe {llength $keep}]       {survives}
ok "shimmer: lindex"        [shimmer_probe {lindex $keep 0}]      {survives}
ok "shimmer: string length" [shimmer_probe {string length $keep}] {survives}
ok "shimmer: string range"  [shimmer_probe {string range $keep 0 end}] {survives}
# These only need the string rep, so the handle is never disturbed.
ok "no shimmer: comparison" [shimmer_probe {if {$keep eq ""} {}}]  {survives}
ok "no shimmer: append"     [shimmer_probe {set s ""; append s $keep}] {survives}
ok "no shimmer: dl_* cmds"  [shimmer_probe {dl_length $keep}]      {survives}

# A shimmered list is still reclaimed -- it becomes a temp of the frame that
# dropped it, and dies with that frame like any other temp.
proc shimmer_churn {} {
    set b [nlists]
    for {set i 0} {$i < 100} {incr i} {
        set keep [shimmer_escape]
        llength $keep
        unset keep
    }
    return [expr {[nlists] - $b}]
}
set before [nlists]
ok "shimmered lists retain in-frame" [shimmer_churn] 100
ok "  and are reclaimed at frame exit" [expr {[nlists] - $before}] 0

# The name survives a full round trip out through a bare string and back.
proc shimmer_roundtrip {} {
    set keep [shimmer_escape]
    set name ""
    append name [string range $keep 0 end]   ;# shimmers, then copies the name
    unset keep                               ;# only the bare string is left
    return [dl_tcllist $name]
}
ok "name still resolves after round trip" [shimmer_roundtrip] {1 2 3}

###########################################################################
# 6. REMAINING CORNER, documented
#
# Telling "this drop is a frame unwinding" from "this drop is a shimmer"
# relies on seeing the list used by a live frame -- either its name being
# materialized (dlRefUpdateString) or a dl_* command resolving it
# (dlRefNoteUse). Both are invisible in one specific sequence:
#
#   the list's name was already materialized in the CALLEE (so the caller's
#   shimmer finds the string rep cached and never calls updateString), AND
#   the caller's very first touch of it is a shimmering op, with no dl_*
#   command in between.
#
# Then the unwinding mark from the callee's exit is still set and the list is
# freed. Closing it would need a per-command execution hook, which is a global
# cost on every command in the interpreter -- not worth it for this.
#
# This was already broken on master (the list was freed at the callee's exit
# regardless), so nothing regresses; and the failure is a clean "not found".
###########################################################################

proc corner_callee {} { set x [dl_ilist 1 2 3]; dl_length $x; return $x }

proc corner_hits {} {
    set k [corner_callee]
    llength $k                     ;# first touch is a shimmer -> mark still set
    if {[catch {dl_tcllist $k}]} { return "lost" }
    return "survives"
}
ok "corner: name taken in callee, then shimmered" [corner_hits] {lost}

proc corner_avoided {} {
    set k [corner_callee]
    dl_length $k                   ;# any dl_* use first re-establishes it
    llength $k
    if {[catch {dl_tcllist $k}]} { return "lost" }
    return "survives"
}
ok "corner: a dl_* use first avoids it" [corner_avoided] {survives}

###########################################################################
# 7. Memory soak: RSS must plateau, not climb
#
# The census above proves lists leave dlTable; this proves the memory goes
# with them. Growth is measured per block, because the first block includes
# allocator warm-up -- an absolute threshold would flag that as a leak.
###########################################################################

proc get_rss_kb {} {
    if {[file readable /proc/self/status]} {
        set f [open /proc/self/status r]; set d [read $f]; close $f
        if {[regexp {VmRSS:\s+(\d+)\s+kB} $d -> rss]} { return $rss }
    }
    if {![catch {exec ps -o rss= -p [pid]} rss]} { return [string trim $rss] }
    return -1
}

proc soak_block {n} {
    for {set i 0} {$i < $n} {incr i} {
        set a [churn_escape]
        set b [dl_mult $a 2]
        set acc {}
        lappend acc $a $b
        dict set d k $b
        llength $a                 ;# exercise the shimmer path too
        unset acc d a b
    }
}

# Run the blocks inside a proc, the way real dlsh code works: temps and any
# promoted lists belong to that frame and go when it returns.
proc soak_run {} {
    soak_block 2000                            ;# warm up the allocator
    set growth {}
    for {set r 0} {$r < 4} {incr r} {
        set b [get_rss_kb]
        soak_block 2000
        lappend growth [expr {[get_rss_kb] - $b}]
    }
    return $growth
}

if {[get_rss_kb] < 0} {
    puts "SKIP soak (no RSS available on this platform)"
} else {
    set before [nlists]
    set growth [soak_run]
    # The last two blocks are steady state; allow page-granularity noise only.
    set tail [expr {[lindex $growth 2] + [lindex $growth 3]}]
    if {$tail > 512} {
        puts "FAIL soak: RSS still climbing, per-block growth {$growth} kB"
        incr ::failures
    } else {
        puts "OK   soak: RSS plateaus (per-block growth {$growth} kB)"
    }
    ok "soak leaves no lists behind" [expr {[nlists] - $before}] 0
}

###########################################################################

if {$::failures} {
    puts "FAILURES: $::failures"
    exit 1
}
puts "all refcount tests passed"
exit 0
