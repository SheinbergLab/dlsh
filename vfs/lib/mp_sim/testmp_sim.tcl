# testmp_sim.tcl -- basic sanity tests for the mp_sim package.
#
# Run from a Tcl shell with the system dlsh zip mounted, then this
# package's pkgs directory on auto_path:
#   tclsh testmp_sim.tcl

# Mount the system dlsh zipvfs (mirrors /usr/local/bin/dlshell setup
# minus Tk/tkcon). When run inside an existing dlsh-enabled shell, the
# zipfs mount is a no-op.
if {![info exists ::__mp_sim_test_loaded]} {
    set dlshlib [file join /usr/local dlsh dlsh.zip]
    set base [file join [zipfs root] dlsh]
    catch {zipfs mount $dlshlib $base}
    set auto_path [linsert $auto_path [set auto_path 0] $base/lib]
    package require dlsh
    # Add this checkout's pkgs directory if not already present.
    set pkgs_dir [file normalize [file join [file dirname [info script]] ..]]
    if {[lsearch -exact $auto_path $pkgs_dir] < 0} {
        lappend auto_path $pkgs_dir
    }
    set ::__mp_sim_test_loaded 1
}

package require mp_sim

proc assert {cond msg} {
    # Evaluate cond in the CALLER's scope so the test-script's local
    # variables (e.g. $n, $tl) are visible.
    if {![uplevel 1 [list expr $cond]]} {
        puts stderr "FAIL: $msg"; exit 1
    }
    puts "  ok: $msg"
}

# ------------------------------------------------------------
# 1. compile_spec produces a valid timeline with the right shape.
# ------------------------------------------------------------
puts "Test 1: compile_spec basic shape"

set spec [dict create \
    meta {duration 2.0 dt 0.0167 patch_size_dva 13.0} \
    endpoints {target   {coh 1.0 speed 0.6 dir 0.0 life 0.5} \
               surround {coh 0.0 speed 0.23 dir 0.0 life 0.08}} \
    envelope {kind sum_gaussians n_pulses 5 sigma_ms 50.0 base_coh 1.0} \
    trajectory {kind static}]

set tl [mp_sim::compile_spec $spec -gname mp_sim_tl_test]
mp_sim::validate_timeline $tl

set n [dl_length $tl:t]
assert {$n > 100}                                    "timeline has frames ($n)"
assert {[dl_length $tl:coherence] == $n}             "coherence length matches"
assert {[dl_length $tl:speed] == $n}                 "speed length matches"
assert {[dl_length $tl:lifetime_s] == $n}            "lifetime length matches"
assert {[dl_length $tl:mask_offset_x] == $n}         "mask_offset_x length matches"

# Pulse peaks should reach base_coh; troughs should be ~zero with
# default sigma=50ms, N=5 pulses across 2s (Δ=400ms, Δ/σ=8 → trough ~0).
set max_coh [dl_max $tl:coherence]
set min_coh [dl_min $tl:coherence]
assert {$max_coh > 0.99}                             "peak coherence near base"
assert {$min_coh < 0.01}                             "trough coherence near zero"

# Speed should sit at surround between pulses (0.23) and target at peaks (0.6).
set max_speed [dl_max $tl:speed]
set min_speed [dl_min $tl:speed]
assert {abs($max_speed - 0.6) < 1e-3}                "peak speed = target"
assert {abs($min_speed - 0.23) < 1e-3}               "trough speed = surround"

# ------------------------------------------------------------
# 2. run_trial: single-row schema, summary stats present.
# ------------------------------------------------------------
puts "\nTest 2: run_trial single-trial schema"

set log [mp_sim::run_trial $tl -n_dots 200 -seed 42 -gname mp_sim_log_test]
assert {[dl_get $log:n_trials 0] == 1}               "single-trial dg has n_trials=1"
assert {[dl_get $log:n_dots 0] == 200}               "n_dots scalar matches"

# Each timecourse column is a 1-element nested list -- one row, n_frames
# values. Inner length should match the design.
set inner_len [dl_length [dl_get $log:coh_recorded 0]]
assert {$inner_len == $n}                            "coh_recorded inner len = n_frames ($inner_len)"
assert {[dl_length $log:t_design] == $n}             "design columns copied as flat (t_design len $n)"

# With record_dots off (default), no per-dot columns.
assert {![dl_exists $log:dot_x]}                     "dot_x absent when record_dots=0"

# ------------------------------------------------------------
# 3. Coherent fraction at peak/trough -- uses per-frame summary.
# ------------------------------------------------------------
puts "\nTest 3: peak/trough coherent fraction (single trial, n=1000)"

set peak_idx [dl_maxIndex $tl:coherence]
set trough_idx [dl_minIndex $tl:coherence]

set log2 [mp_sim::run_trial $tl -n_dots 1000 -seed 7 -gname mp_sim_log_test2]
# coh_recorded[0] is the single row; frame index gives realized fraction.
dl_local coh_rec [dl_get $log2:coh_recorded 0]
set peak_frac    [dl_get $coh_rec $peak_idx]
set trough_frac  [dl_get $coh_rec $trough_idx]
puts "  peak_idx=$peak_idx coh_recorded = $peak_frac (target 1.0)"
puts "  trough_idx=$trough_idx coh_recorded = $trough_frac (target 0.0)"
# Initialization uses round(coh*N) so single-trial = exact at base_coh=1.
assert {abs($peak_frac - 1.0) < 0.05}                "peak coh_recorded matches design"
assert {abs($trough_frac - 0.0) < 0.05}              "trough coh_recorded matches design"

# ------------------------------------------------------------
# 4. Direction bias at peak: dir_mean_coh should match target dir.
# ------------------------------------------------------------
puts "\nTest 4: coherent direction matches target"

set spec2 [dict create \
    meta {duration 2.0 dt 0.0167 patch_size_dva 13.0} \
    endpoints {target   {coh 1.0 speed 0.6 dir 1.5707963 life 0.5} \
               surround {coh 0.0 speed 0.23 dir 0.0 life 0.08}} \
    envelope {kind sum_gaussians n_pulses 5 sigma_ms 50.0 base_coh 1.0} \
    trajectory {kind static}]
set tl2 [mp_sim::compile_spec $spec2 -gname mp_sim_tl_test2]
set log3 [mp_sim::run_trial $tl2 -n_dots 1000 -seed 11 -gname mp_sim_log_test3]

set peak_idx2 [dl_maxIndex $tl2:coherence]
dl_local dir_coh [dl_get $log3:dir_mean_coh 0]
dl_local cvar    [dl_get $log3:dir_circ_var_coh 0]
set d_at_peak [dl_get $dir_coh $peak_idx2]
set v_at_peak [dl_get $cvar    $peak_idx2]
puts "  dir_mean_coh @ peak = $d_at_peak (target ~ 1.5708); circ_var = $v_at_peak"
# Allow small atan2 noise.
assert {abs($d_at_peak - 1.5707963) < 0.01}          "coherent direction matches pi/2 at peak"
assert {$v_at_peak < 0.001}                          "circ variance ~0 at peak (jitter=0)"

# ------------------------------------------------------------
# 5. Scratch state dg cleanup: run_trial must not leak its scratch dg
#    even if many trials are run in succession.
# ------------------------------------------------------------
puts "\nTest 5: scratch state dg cleanup"
set dgs_before [dg_dir]
for {set i 0} {$i < 5} {incr i} {
    set glog [mp_sim::run_trial $tl -n_dots 100 -seed [expr {100 + $i}] \
                  -gname mp_sim_cleanup_$i -record_dots 0]
}
set dgs_after  [dg_dir]
# Allow exactly the 5 logs we asked for; no leaked scratch dgs.
set added [llength $dgs_after]
set baseline [llength $dgs_before]
set delta [expr {$added - $baseline}]
puts "  before: $baseline dgs   after: $added dgs   delta: $delta"
assert {$delta == 5}                                 "no scratch dg leaks (delta=$delta, expected 5)"
foreach name {mp_sim_cleanup_0 mp_sim_cleanup_1 mp_sim_cleanup_2 mp_sim_cleanup_3 mp_sim_cleanup_4} {
    catch {dg_delete $name}
}

# ------------------------------------------------------------
# 6. Ensemble: 100 trials, mean coherent fraction tracks design.
# ------------------------------------------------------------
puts "\nTest 6: ensemble mean tracks design"
set ens [mp_sim::ensemble $tl -n_dots 500 -n_trials 100 -seed 1000 \
             -gname mp_sim_ens_test]
set nf [dl_length $ens:t_design]
set ntrials [dl_get $ens:n_trials 0]
puts "  n_trials=$ntrials  n_frames=$nf"
assert {$ntrials == 100}                             "ensemble has 100 rows"
assert {[dl_length $ens:coh_recorded] == 100}        "coh_recorded has 100 rows"

# dl_means of a nested flist column reduces along the inner dimension
# by default (returns one mean per row); we want the OPPOSITE -- mean
# per frame across trials. Use dl_transpose first, or dl_means with
# the right axis. Try the simple approach: mean across rows of each
# nested entry's k-th element. dl_collapse + reshape, or dl_means on
# the transpose.
dl_local coh_means_per_frame [dl_means [dl_transpose $ens:coh_recorded]]
set peak_idx [dl_maxIndex $ens:coherence_design]
set trough_idx [dl_minIndex $ens:coherence_design]
set mean_peak [dl_get $coh_means_per_frame $peak_idx]
set mean_trough [dl_get $coh_means_per_frame $trough_idx]
puts "  mean coh @ peak frame $peak_idx = $mean_peak (target ~ 1.0)"
puts "  mean coh @ trough frame $trough_idx = $mean_trough (target ~ 0.0)"
assert {abs($mean_peak - 1.0) < 0.02}                "ensemble mean coh at peak"
assert {abs($mean_trough - 0.0) < 0.02}              "ensemble mean coh at trough"

# Direction at peak should match target dir (here 0); circular variance
# should be near zero with jitter=0.
dl_local dir_means_per_frame      [dl_means [dl_transpose $ens:dir_mean_coh]]
dl_local dir_circvar_per_frame    [dl_means [dl_transpose $ens:dir_circ_var_coh]]
set dir_at_peak  [dl_get $dir_means_per_frame $peak_idx]
set cvar_at_peak [dl_get $dir_circvar_per_frame $peak_idx]
puts "  mean coh-dir @ peak = $dir_at_peak (target 0.0); circ_var = $cvar_at_peak"
assert {abs($dir_at_peak) < 0.02}                    "coherent dot direction matches target at peak"
assert {$cvar_at_peak < 0.01}                        "circ variance ~0 at peak (jitter=0)"

dg_delete $ens

# ------------------------------------------------------------
# 7. Sweep: 2D grid (sigma_ms x n_dots), ~20 trials per cell.
# ------------------------------------------------------------
puts "\nTest 7: parameter sweep produces grid dg"
set sw [mp_sim::sweep $spec \
            -vary {envelope.sigma_ms {30 60} runtime.n_dots {200 800}} \
            -n_trials 20 -seed 5000 -gname mp_sim_sw_test]

set ncells [dl_get $sw:n_cells 0]
puts "  n_cells=$ncells"
assert {$ncells == 4}                                "2x2 grid -> 4 cells"
assert {[dl_length $sw:envelope.sigma_ms] == 4}      "sigma column has 4 entries"
assert {[dl_length $sw:runtime.n_dots] == 4}         "n_dots column has 4 entries"
assert {[dl_length $sw:coh_recorded_mean] == 4}      "coh_recorded_mean has 4 rows"

# Each row's mean column is a nested 121-element flist.
set inner_len [dl_length [dl_get $sw:coh_recorded_mean 0]]
assert {$inner_len == 121}                           "per-cell mean trace length = n_frames"

# Sanity: at peak frame the mean coh_recorded should be ~1.0 in every
# cell (regardless of sigma or n_dots).
set peak_idx [dl_maxIndex $sw:coherence_design]
for {set ci 0} {$ci < $ncells} {incr ci} {
    dl_local m [dl_get $sw:coh_recorded_mean $ci]
    set p [dl_get $m $peak_idx]
    set sigma_ms [dl_get $sw:envelope.sigma_ms $ci]
    set ndots    [dl_get $sw:runtime.n_dots    $ci]
    puts "  cell $ci  sigma=$sigma_ms ms  n_dots=$ndots  peak coh = $p"
    assert {abs($p - 1.0) < 0.05}                    "cell $ci peak coh ~ 1.0"
}

dg_delete $sw

# ------------------------------------------------------------
# 8. Bounce: timeline direction is a step function; leakage projection
#    differs by ~base_coh-fold between peak and trough bounces.
# ------------------------------------------------------------
puts "\nTest 8: bounce step function and leakage projection"

set pre_dir  0.0
set post_dir 1.5707963   ;# pi/2

set spec_p [dict create \
    meta {duration 2.0 dt 0.0167 patch_size_dva 13.0} \
    endpoints [dict create \
        target   [dict create coh 1.0 speed 0.6 dir $pre_dir life 0.5] \
        surround [dict create coh 0.0 speed 0.23 dir 0.0 life 0.08]] \
    envelope {kind sum_gaussians n_pulses 5 sigma_ms 50.0 base_coh 1.0} \
    trajectory {kind static} \
    bounce [dict create phase peak pulse_index 2 \
                pre_dir $pre_dir post_dir $post_dir]]

set tl_p [mp_sim::compile_spec $spec_p -gname mp_sim_tl_b_peak]

# Direction column should be pre_dir before bounce_t and post_dir after.
set bt [dl_get $tl_p:bounce_t 0]
puts "  bounce_t = $bt"
assert {abs($bt - 1.0) < 0.01}                       "peak bounce_t at pulse 2 center (~1.0s)"
dl_local diffs [dl_abs [dl_sub $tl_p:t $bt]]
set bidx [dl_minIndex $diffs]
set d_before [dl_get $tl_p:direction [expr {$bidx - 5}]]
set d_after  [dl_get $tl_p:direction [expr {$bidx + 5}]]
assert {abs($d_before - $pre_dir) < 1e-6}            "direction = pre_dir before bounce"
assert {abs($d_after - $post_dir) < 1e-6}            "direction = post_dir after bounce"

# Run a small ensemble and check leakage projection magnitudes.
set ens_p [mp_sim::ensemble $tl_p -n_dots 2000 -n_trials 30 -seed 100 \
               -gname mp_sim_ens_b_peak]
mp_sim::leakage_projection $ens_p $post_dir
set sig_peak [dl_get $ens_p:proj_post_dir $bidx]
puts "  peak bounce post-dir signal at bounce_t = $sig_peak  (expect ~0.6 = speed)"
assert {abs($sig_peak - 0.6) < 0.05}                 "peak bounce delivers full new-dir signal"

# Trough bounce: same spec but phase=trough between pulses 2 and 3.
set spec_t [dict replace $spec_p bounce \
    [dict create phase trough pulse_index 2 \
        pre_dir $pre_dir post_dir $post_dir]]
set tl_t [mp_sim::compile_spec $spec_t -gname mp_sim_tl_b_trough]
set bt_t [dl_get $tl_t:bounce_t 0]
puts "  trough bounce_t = $bt_t"

set ens_t [mp_sim::ensemble $tl_t -n_dots 2000 -n_trials 30 -seed 100 \
               -gname mp_sim_ens_b_trough]
mp_sim::leakage_projection $ens_t $post_dir
dl_local diffs_t [dl_abs [dl_sub $tl_t:t $bt_t]]
set bidx_t [dl_minIndex $diffs_t]
set sig_trough [dl_get $ens_t:proj_post_dir $bidx_t]
puts "  trough bounce post-dir signal at bounce_t = $sig_trough  (expect << 0.6)"
assert {abs($sig_trough) < 0.05}                     "trough bounce leaks weakly"

dg_delete $tl_p
dg_delete $tl_t
dg_delete $ens_p
dg_delete $ens_t

puts "\nAll tests passed."
