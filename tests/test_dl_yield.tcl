#!/usr/bin/env dlsh
#
# test_dl_yield.tcl
#   Correctness test for dl_yield (src/tcl_dl.c). dl_yield is what dl_return
#   is almost always meant to be: it returns from the enclosing proc, promotes
#   an existing >#< instead of deep copying it, and never consumes a list the
#   caller named.
#
#   Usage:  dlsh test_dl_yield.tcl        (exits non-zero on failure)

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

check "dl_yield present" [llength [info commands dl_yield]] 1

###############################
# 1. it actually returns
###############################

proc mk {} { dl_yield [dl_ilist 1 2 3] }
check "returns without an explicit return" [dl_tcllist [mk]] {1 2 3}

# the dl_return footgun -- code after it -- is unreachable, not silently lossy
set ::reached 0
proc after_yield {} { dl_yield [dl_ilist 4 5 6]; set ::reached 1; return }
check "value survives trailing code" [dl_tcllist [after_yield]] {4 5 6}
check "trailing code is unreachable" $::reached 0

# for contrast: this is what dl_return does with the same shape
proc lossy {} { dl_return [dl_ilist 4 5 6]; return }
check "dl_return + bare return still loses it" [lossy] {}

###############################
# 2. lifetime survives arbitrary nesting
###############################

proc l1 {} { dl_yield [dl_fromto 0 5] }
proc l2 {} { dl_yield [l1] }
proc l3 {} { dl_yield [l2] }
proc l4 {} { dl_yield [l3] }
check "4-deep pass-through" [dl_tcllist [l4]] {0 1 2 3 4}

# the same shape dangles under dl_return, which is why the re-wrap was needed
proc d1 {} { return [dl_return [dl_fromto 0 5]] }
proc d2 {} { return [d1] }
check "dl_return dangles one frame up" [catch {dl_tcllist [d2]}] 1

proc slot {} { dl_yield [dl_add ysrc 0] }
proc s2 {} { dl_yield [slot] }
proc s3 {} { dl_yield [s2] }
proc s4 {} { dl_yield [s3] }
proc rslot {} { return [dl_return [dl_add ysrc 0]] }
proc r2 {} { return [dl_return [rslot]] }
proc r3 {} { return [dl_return [r2]] }
proc r4 {} { return [dl_return [r3]] }

# A hand-off must promote, not deep copy: the marginal cost of the extra
# frames must not scale with list length. dl_return copies per frame and grows
# ~linearly; the bound here is deliberately loose (a copying implementation is
# off by ~1000x at these sizes, so this cannot trip on timing noise alone).
proc marginal {deep shallow reps} {
    proc __drain {p} { set v [$p]; dl_length $v }
    __drain $deep ; __drain $shallow
    set d [lindex [time {__drain $deep} $reps] 0]
    set s [lindex [time {__drain $shallow} $reps] 0]
    return [expr {($d - $s) / 3.0}]
}
dl_set ysrc [dl_fromto 0 1000]
set y_small [marginal s4 slot 2000]
set r_small [marginal r4 rslot 2000]
dl_delete ysrc
dl_set ysrc [dl_fromto 0 2000000]
set y_big [marginal s4 slot 50]
set r_big [marginal r4 rslot 50]
dl_delete ysrc

# Compare the two implementations AT THE SAME SIZE rather than one
# implementation against itself at two sizes. Both are measured under
# identical conditions, so a slow or throttled machine scales them together
# and cancels out; an absolute bound on y_big/y_small does not, and failed
# intermittently on CI (15x observed on a throttled runner against a 10x
# bound) with no code change to explain it.
check "promoting beats copying at 2e6 elements" \
    [expr {$r_big > 5 * $y_big}] 1
# Still assert yield does not scale, but loosely enough that only a real
# copying regression trips it: the list grew 2000x between the two
# measurements, and a copying implementation shows up as ~1000x here.
check "dl_yield hand-off does not scale with length" \
    [expr {$y_big < 100 * $y_small}] 1
check "dl_return hand-off is O(length) (control)" \
    [expr {$r_big > 20 * $r_small}] 1
puts [format "     (per frame: yield %.1f -> %.1f us, return %.1f -> %.1f us, 1e3 -> 2e6 elements)" \
          $y_small $y_big $r_small $r_big]

###############################
# 3. calling contexts
###############################

proc early {x} { if {$x} { dl_yield [dl_ilist 9 9] } ; dl_yield [dl_ilist 0] }
check "early return (non-tail)"  [dl_tcllist [early 1]] {9 9}
check "fall-through return"      [dl_tcllist [early 0]] {0}

proc inloop {} {
    foreach x {1 2 3} { if {$x == 2} { dl_yield [dl_ilist 22] } }
    dl_yield [dl_ilist 0]
}
check "returns out of a loop body" [dl_tcllist [inloop]] {22}

oo::class create YieldTest {
    method make  {} { dl_yield [dl_flist 1.5 2.5] }
    method chain {} { dl_yield [my make] }
}
YieldTest create yt
check "oo:: method"          [dl_tcllist [yt make]]  {1.5 2.5}
check "oo:: method chained"  [dl_tcllist [yt chain]] {1.5 2.5}

check "apply/lambda" [dl_tcllist [apply {{} { dl_yield [dl_ilist 7 7] }}]] {7 7}

proc caught {} { dl_yield [dl_ilist 77] }
check "ok under catch" [catch {caught} m] 0
check "value under catch" [dl_tcllist $m] 77

###############################
# 4. it never eats a list it did not make
###############################

dl_set kept [dl_ilist 1 1 1]
proc from_named {} { dl_yield kept }
check "named list yielded"  [dl_tcllist [from_named]] {1 1 1}
check "named list survives" [dl_tcllist kept]         {1 1 1}

# dl_return, by contrast, renames it out from under you
dl_set doomed [dl_ilist 1 1 1]
proc steal {} { return [dl_return doomed] }
steal
check "dl_return consumes a named list" [dl_exists doomed] 0

dg_create yg ; dl_set yg:col [dl_ilist 2 2 2]
proc from_col {} { dl_yield yg:col }
check "group column yielded"  [dl_tcllist [from_col]] {2 2 2}
check "group column survives" [dl_tcllist yg:col]     {2 2 2}

# this frame's own dl_local IS ours to hand up -- it dies with the frame anyway
proc from_local {} { dl_local v [dl_ilist 8 8]; dl_yield $v }
check "own dl_local yielded" [dl_tcllist [from_local]] {8 8}

# ...but a dl_local belonging to a CALLER is not, even when the parameter
# happens to be spelled the same as the caller's variable (the trace, not the
# name, decides ownership)
proc relay_same_name {v} { dl_yield $v }
proc owner_same_name {} {
    dl_local v [dl_ilist 6 6]
    set got [relay_same_name $v]
    return [list [dl_tcllist $v] [dl_tcllist $got]]
}
check "caller's dl_local not stolen (name alias)" [owner_same_name] {{6 6} {6 6}}

proc relay_diff_name {l} { dl_yield $l }
proc owner_diff_name {} {
    dl_local w [dl_ilist 4 4]
    set got [relay_diff_name $w]
    return [list [dl_tcllist $w] [dl_tcllist $got]]
}
check "caller's dl_local not stolen" [owner_diff_name] {{4 4} {4 4}}

# a dl_local yielded up several frames stays O(1) and stays correct
proc lo1 {} { dl_local v [dl_fromto 0 4]; dl_yield $v }
proc lo2 {} { dl_yield [lo1] }
proc lo3 {} { dl_yield [lo2] }
check "dl_local promoted through 3 frames" [dl_tcllist [lo3]] {0 1 2 3}

# underscores in the variable name must still round-trip
proc from_local_us {} { dl_local my_long_v [dl_ilist 3 3]; dl_yield $my_long_v }
check "dl_local with underscores" [dl_tcllist [from_local_us]] {3 3}

# a >#< reached from a frame that does not own it is copied, not stolen
proc stash {} { dl_yield [dl_ilist 5 5] }
set foreign [stash]
proc relay {n} { dl_yield $n }
check "foreign >#< copied"   [dl_tcllist [relay $foreign]] {5 5}
check "foreign >#< survives" [dl_tcllist $foreign]         {5 5}

###############################
# 5. errors
###############################

proc bad_arg {} { dl_yield nosuchlist }
check "unknown list errors"  [catch {bad_arg}] 1
proc no_arg {} { dl_yield }
check "missing arg errors"   [catch {no_arg} m] 1
check "missing arg message"  $m {usage: dl_yield dynlist}

###############################
# 6. no leaks on either path
###############################

dl_set ysrc [dl_ilist 1 2 3]
proc consume_yield  {} { set v [s4]; dl_length $v }
proc consume_return {} { set v [r4]; dl_length $v }
proc consume_named  {} { set v [from_named]; dl_length $v }

set base [llength [dl_dir]]
for {set i 0} {$i < 500} {incr i} { consume_yield }
check "no leak: 500x promote path" [llength [dl_dir]] $base
for {set i 0} {$i < 500} {incr i} { consume_named }
check "no leak: 500x copy path"    [llength [dl_dir]] $base
for {set i 0} {$i < 500} {incr i} { consume_return }
check "no leak: 500x dl_return"    [llength [dl_dir]] $base

###############################

if {$::fail} { puts "\nFAILURES: $::fail"; exit 1 }
puts "\nall dl_yield tests passed"
exit 0
