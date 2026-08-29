#!/usr/bin/env dlsh
#
# test_mp_sim_kinematics.tcl
#   Regression test for mp_sim::trajectory_kinematics.
#
#   Both early exits in this proc were written with dl_return, which does not
#   transfer control. The npair>=4 branch therefore computed the exact
#   kinematics and then fell through into the finite-difference fallback,
#   whose result overwrote it -- so a callback returning the exact
#   {x y vx vy} 4-tuple had its supplied velocity silently discarded, which
#   is precisely the case the proc's own comment tells you to use to avoid
#   finite-difference smearing across a bounce. The n<2 guard fell through
#   into "dl_get $ts 1" and errored. Both now use dl_yield.
#
#   Usage:  dlsh test_mp_sim_kinematics.tcl     (exits non-zero on failure)

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
package require mp_sim

set ::fail 0
proc check {label got want} {
    if {$got eq $want} { puts "OK   $label" } \
    else { puts "FAIL $label -> got {$got} want {$want}"; incr ::fail }
}

dl_local ts [dl_flist 0.0 0.1 0.2 0.3]

# positions imply speed 10; supplied velocity says 100. The supplied value wins.
dl_local exact [dl_llist \
    [dl_flist 0.0 1.0 2.0 3.0] [dl_flist 0.0 0.0 0.0 0.0] \
    [dl_flist 100.0 100.0 100.0 100.0] [dl_flist 0.0 0.0 0.0 0.0]]
dl_local k [mp_sim::trajectory_kinematics $exact $ts]
check "supplied velocity is used, not re-derived" [dl_tcllist $k:0] {100.0 100.0 100.0 100.0}
check "supplied direction is used"                [dl_tcllist $k:1] {0.0 0.0 0.0 0.0}

# with only position channels, finite-differencing is still correct
dl_local pos [dl_llist [dl_flist 0.0 1.0 2.0 3.0] [dl_flist 0.0 0.0 0.0 0.0]]
check "finite-difference fallback" \
    [dl_tcllist [dl_get [mp_sim::trajectory_kinematics $pos $ts] 0]] {10.0 10.0 10.0 10.0}

# a single frame has no gradient: zeros, not an error from dl_get $ts 1
dl_local ts1  [dl_flist 0.0]
dl_local pos1 [dl_llist [dl_flist 0.0] [dl_flist 0.0]]
check "n<2 returns zeros without erroring" \
    [expr {![catch {mp_sim::trajectory_kinematics $pos1 $ts1} r] && [dl_tcllist $r:0] eq "0"}] 1

if {$::fail} { puts "\nFAILURES: $::fail"; exit 1 }
puts "\nall mp_sim kinematics tests passed"
exit 0
