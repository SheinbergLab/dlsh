# play_sweep.tcl -- 3x3 sweep over (sigma_ms × n_dots), 50 trials per
# cell. Prints a small text table of "trough floor" -- mean coherent
# fraction at the deepest trough -- vs the two parameters. Shows how
# parameter choices shape the manipulation's empirical noise floor.

set spec [dict create \
    meta {duration 2.0 dt 0.0167 patch_size_dva 13.0} \
    endpoints {target   {coh 1.0 speed 0.6 dir 0.0 life 0.5} \
               surround {coh 0.0 speed 0.23 dir 0.0 life 0.08}} \
    envelope {kind sum_gaussians n_pulses 5 sigma_ms 50.0 base_coh 1.0} \
    trajectory {kind static}]

set t0 [clock milliseconds]
catch {dg_delete play_sw}
set sw [mp_sim::sweep $spec \
            -vary {envelope.sigma_ms {30 60 90}
                   runtime.n_dots    {500 2000 8000}} \
            -n_trials 50 -seed 9999 -gname play_sw]
set elapsed_ms [expr {[clock milliseconds] - $t0}]
puts "sweep: 9 cells × 50 trials = 450 trials in $elapsed_ms ms"

# For each cell, find the per-frame mean of coh_recorded and report
# the value at the design's trough frame (worst case).
set trough_idx [dl_minIndex $sw:coherence_design]
set ncells [dl_length $sw:cell_id]

puts "\nsigma_ms  n_dots   peak_mean   trough_mean   trough_std"
puts [string repeat "-" 56]
for {set i 0} {$i < $ncells} {incr i} {
    set sigma_ms [dl_get $sw:envelope.sigma_ms $i]
    set ndots    [dl_get $sw:runtime.n_dots    $i]
    dl_local m   [dl_get $sw:coh_recorded_mean $i]
    dl_local s   [dl_get $sw:coh_recorded_std  $i]
    set peak_idx [dl_maxIndex $sw:coherence_design]
    set pm [dl_get $m $peak_idx]
    set tm [dl_get $m $trough_idx]
    set ts [dl_get $s $trough_idx]
    puts [format "%-9s %-8s %-11.4f %-13.4f %.4f" \
              $sigma_ms $ndots $pm $tm $ts]
}
