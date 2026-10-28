#!/usr/bin/env dlsh
#
# test_em_wide.tcl
#   The em package's C detectors take the eye signal as double as well as
#   float.  Under dslog::wideTypes 1 the h/v/t columns arrive as doubles:
#   em::c::detect_saccades and em::c::velocity refused them ("expected float
#   sublists") and em::c::classify read their bytes as floats, unchecked.
#   Two synthetic trials (a fixation, a 10 deg saccade, a slow drift) go
#   through all three as float and as double columns; every output column
#   must be identical, since the detectors work in float either way, as a
#   narrow read narrows doubles to float before they ever see them.
#
#   Time-base insurance: the same trials with times on a session scale
#   (+3000 s, where a float is good to ~240 us) as double input are
#   rebased before narrowing, so they find the same saccades with the same
#   durations and speeds, and their absolute output times come back offset
#   by 3000 s.
#
#   Usage:  dlsh test_em_wide.tcl        (exits non-zero on any failure)

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
package require em

set ::fail 0
proc check {label got want} {
    if {$got eq $want} { puts "ok   $label" } \
    else { puts "FAIL $label\n       got:  [string range $got 0 300]\n       want: [string range $want 0 300]"; incr ::fail }
}

# one trial at 500 Hz: 300 ms fixating at (0,0), a 10 deg rightward saccade
# over 30 ms (raised cosine), then a 2 deg/s drift up for the rest
proc trial {n shift} {
    set h {}; set v {}; set t {}
    for {set i 0} {$i < $n} {incr i} {
        set s [expr {$i / 500.0}]
        if {$s < 0.3} { set x 0.0 } \
        elseif {$s < 0.33} { set x [expr {5.0 * (1 - cos(3.14159265 * ($s - 0.3) / 0.03))}] } \
        else { set x 10.0 }
        set y [expr {$s < 0.33 ? 0.0 : 2.0 * ($s - 0.33)}]
        lappend h [expr {$x + $shift}]; lappend v $y; lappend t $s
    }
    list $h $v $t
}

# dl_flist / dl_dlist build float / double lists from values (dl_float and
# dl_double convert a list's type)
array set make {float dl_flist double dl_dlist}
foreach type {float double} {
    set g($type) [dg_create]
    foreach col {h v t} { dl_set $g($type):$col [dl_llist] }
    foreach {n shift} {400 0.0 500 -3.0} {
        lassign [trial $n $shift] h v t
        dl_append $g($type):h [$make($type) {*}$h]
        dl_append $g($type):v [$make($type) {*}$v]
        dl_append $g($type):t [$make($type) {*}$t]
    }
}
check "inputs: 400 and 500 samples" [dl_tcllist [dl_lengths $g(double):h]] {400 500}
check "inputs: double sublists on the wide side" [dl_datatype $g(double):h:0] double

# velocity
set vf [dl_tcllist [em::c::velocity $g(float):h $g(float):v $g(float):t]]
set vd [dl_tcllist [em::c::velocity $g(double):h $g(double):v $g(double):t]]
check "velocity: double input accepted, same speeds" $vd $vf

# detect_saccades
foreach type {float double} {
    set sac($type) [em::c::detect_saccades $g($type):h $g($type):v $g($type):t ""]
}
check "detect_saccades: one saccade found in each trial" \
    [dl_tcllist [dl_lengths $sac(float):sac_starts]] {1 1}
check "detect_saccades: same columns" \
    [lsort [dg_tclListnames $sac(double)]] [lsort [dg_tclListnames $sac(float)]]
foreach col [lsort [dg_tclListnames $sac(float)]] {
    check "detect_saccades $col identical" \
        "[dl_datatype $sac(double):$col] [dl_tcllist $sac(double):$col]" \
        "[dl_datatype $sac(float):$col] [dl_tcllist $sac(float):$col]"
}

# classify, fed each side's own saccades
foreach type {float double} {
    set cls($type) [em::c::classify $g($type):h $g($type):v $g($type):t "" \
                        $sac($type):sac_starts $sac($type):sac_stops \
                        $sac($type):pso_starts $sac($type):pso_stops]
}
check "classify: same columns" \
    [lsort [dg_tclListnames $cls(double)]] [lsort [dg_tclListnames $cls(float)]]
foreach col [lsort [dg_tclListnames $cls(float)]] {
    check "classify $col identical" \
        "[dl_datatype $cls(double):$col] [dl_tcllist $cls(double):$col]" \
        "[dl_datatype $cls(float):$col] [dl_tcllist $cls(float):$col]"
}

# time-base insurance: the double trials again with t + 3000 s
proc near {label got want tol} {
    set bad {}
    foreach a $got b $want { if {abs($a - $b) > $tol} { lappend bad "$a vs $b" } }
    if {[llength $got] != [llength $want]} { set bad "lengths [llength $got] vs [llength $want]" }
    if {$bad eq ""} { puts "ok   $label" } else { puts "FAIL $label: [lrange $bad 0 3]"; incr ::fail }
}
set late [dg_create]
dl_set $late:h $g(double):h
dl_set $late:v $g(double):v
dl_set $late:t [dl_add $g(double):t 3000.0]
check "session-scale input is double" [dl_datatype $late:t:0] double
set sacl [em::c::detect_saccades $late:h $late:v $late:t ""]
foreach col {sac_starts sac_stops pso_starts pso_stops} {
    check "session scale: $col the same samples" [dl_tcllist $sacl:$col] [dl_tcllist $sac(double):$col]
}
near "session scale: sac_durs as trial-relative" \
    [dl_tcllist [dl_unpack $sacl:sac_durs]] [dl_tcllist [dl_unpack $sac(double):sac_durs]] 1e-6
near "session scale: sac_peakvels as trial-relative (deg/s)" \
    [dl_tcllist [dl_unpack $sacl:sac_peakvels]] [dl_tcllist [dl_unpack $sac(double):sac_peakvels]] 1e-3
near "session scale: sac_t_start = trial-relative + 3000 s (to float at 3000 s)" \
    [dl_tcllist [dl_unpack $sacl:sac_t_start]] \
    [dl_tcllist [dl_add [dl_unpack $sac(double):sac_t_start] 3000.0]] 5e-4
set clsl [em::c::classify $late:h $late:v $late:t "" \
              $sacl:sac_starts $sacl:sac_stops $sacl:pso_starts $sacl:pso_stops]
check "session scale: classify em_state the same" \
    [dl_tcllist $clsl:em_state] [dl_tcllist $cls(double):em_state]
near "session scale: fix_t_start = trial-relative + 3000 s" \
    [dl_tcllist [dl_unpack $clsl:fix_t_start]] \
    [dl_tcllist [dl_add [dl_unpack $cls(double):fix_t_start] 3000.0]] 5e-4
near "session scale: velocity as trial-relative" \
    [dl_tcllist [dl_unpack [em::c::velocity $late:h $late:v $late:t]]] \
    [dl_tcllist [dl_unpack [em::c::velocity $g(double):h $g(double):v $g(double):t]]] 1e-2

# a non-numeric sublist is still refused
set bad [dg_create]
dl_set $bad:h [dl_llist [dl_slist a b]]
dl_set $bad:v [dl_llist [dl_flist 1 2]]
dl_set $bad:t [dl_llist [dl_flist 0 1]]
check "a string sublist is refused" \
    [catch {em::c::detect_saccades $bad:h $bad:v $bad:t ""}] 1
check "... by classify too" \
    [catch {em::c::classify $bad:h $bad:v $bad:t "" $sac(float):sac_starts $sac(float):sac_stops $sac(float):pso_starts $sac(float):pso_stops}] 1

if {$::fail} { puts "$::fail FAILED"; exit 1 }
puts "all passed"
exit 0
