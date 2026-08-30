#!/usr/bin/env dlsh
#
# test_reduction_accuracy.tcl
#   dl_sum / dl_mean / dl_std must stay accurate on long lists.
#
#   These reductions used narrow accumulators: an `int` for integer sums and
#   a `float` for everything else -- including the integer path in
#   dynListMeanList. Both failed silently and only on large inputs, which is
#   the worst shape for a bug in analysis code:
#
#     dl_sum  [dl_fromto 0 10000000]  ->  -2014260032   (int32 wraparound)
#     dl_mean [dl_fromto 0 10000000]  ->  4871488.0     (2.57% low; float32
#                                                        stops absorbing
#                                                        addends past ~2^24)
#
#   Sizes here are chosen to straddle those limits: 1e5 already exceeds int32
#   for a sum of 0..n, and 1e7 is well past float32's mantissa.
#
#   Usage: dlsh test_reduction_accuracy.tcl
#   Exits non-zero on any inaccuracy.

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
proc close_enough {name got want tol} {
    if {$want == 0} { set rel [expr {abs($got)}] } \
    else { set rel [expr {abs(double($got)-$want)/abs($want)}] }
    if {$rel <= $tol} {
        puts "OK   $name"
    } else {
        puts [format "FAIL %s -> got %s want %s (rel err %.3g > %g)" \
                  $name $got $want $rel $tol]
        incr ::failures
    }
}

# --- integer sums must be EXACT, at any length ---------------------------
# Tcl carries 64-bit integers, so there is no reason to lose digits here.
proc int_sum_check {n} {
    set s [dl_fromto 0 $n]
    set got [dl_sum $s]
    set want [expr {$n * ($n - 1) / 2}]
    return [list $got $want]
}
foreach n {1000 100000 1000000 10000000} {
    lassign [int_sum_check $n] got want
    ok "dl_sum exact for n=$n" $got $want
}

# A negative result is the specific signature of the old int32 wraparound.
proc no_wrap {} {
    set s [dl_fromto 0 10000000]
    return [expr {[dl_sum $s] > 0}]
}
ok "dl_sum does not wrap negative" [no_wrap] 1

# --- means stay accurate on long lists -----------------------------------
proc mean_check {n} {
    set s [dl_fromto 0 $n]
    return [list [dl_mean $s] [expr {($n - 1) / 2.0}]]
}
foreach n {1000 100000 1000000 10000000} {
    lassign [mean_check $n] got want
    close_enough "dl_mean accurate for n=$n" $got $want 1e-9
}

# The same list as floats: the accumulator, not the storage, was the problem.
proc float_mean {n} {
    set s [dl_float [dl_fromto 0 $n]]
    return [list [dl_mean $s] [expr {($n - 1) / 2.0}]]
}
lassign [float_mean 10000000] got want
close_enough "dl_mean accurate on a float list" $got $want 1e-6

proc float_sum {n} {
    set s [dl_float [dl_fromto 0 $n]]
    return [list [dl_sum $s] [expr {double($n) * ($n - 1) / 2}]]
}
lassign [float_sum 10000000] got want
close_enough "dl_sum accurate on a float list" $got $want 1e-9

# --- std/var ride on the mean, so check one long case --------------------
# A uniform ramp 0..n-1 has population variance (n^2-1)/12; with the n-1
# divisor the expected sd is sqrt(n(n+1)/12).
proc std_check {n} {
    set s [dl_fromto 0 $n]
    return [list [dl_std $s] [expr {sqrt(double($n) * ($n + 1) / 12.0)}]]
}
lassign [std_check 1000000] got want
close_enough "dl_std accurate for n=1e6" $got $want 1e-4

# --- small inputs must be untouched --------------------------------------
ok "dl_sum small ints"   [dl_sum [dl_ilist 1 2 3]] 6
ok "dl_mean small"       [dl_mean [dl_flist 1.5 2.5]] 2.0
# Empty input returns an empty result, as it always has -- the exact-int
# path declines empty lists and falls through to the original code.
ok "dl_sum empty unchanged" [dl_sum [dl_ilist]] {}

if {$::failures} {
    puts "FAILURES: $::failures"
    exit 1
}
puts "all reduction accuracy tests passed"
exit 0
