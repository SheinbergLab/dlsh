# play_ensemble.tcl -- 200 trials. Two plots:
#   (1) coh_recorded mean (overlays the design exactly -- structurally
#       zero variance because the rebalance uses round(coh*N)/N, the
#       same as motionpatch.c's stable-membership update).
#   (2) dx_mean ensemble band -- the genuinely stochastic stat. The
#       coherent contribution is locked, the incoherent contribution is
#       (1/N) sum cos(uniform) which scales as 1/sqrt(N).
#
# Source after dev.tcl:
#   source /Users/sheinb/src/dlsh/pkgs/mp_sim/dev.tcl
#   source /Users/sheinb/src/dlsh/pkgs/mp_sim/play_ensemble.tcl

set spec [dict create \
    meta {duration 2.0 dt 0.0167 patch_size_dva 13.0} \
    endpoints {target   {coh 1.0 speed 0.6 dir 0.0 life 0.5} \
               surround {coh 0.0 speed 0.23 dir 0.0 life 0.08}} \
    envelope {kind sum_gaussians n_pulses 5 sigma_ms 50.0 base_coh 1.0} \
    trajectory {kind static}]

catch {dg_delete play_tl_e}
set tl [mp_sim::compile_spec $spec -gname play_tl_e]

set t0 [clock milliseconds]
catch {dg_delete play_ens}
set ens [mp_sim::ensemble $tl -n_dots 2000 -n_trials 200 -seed 1 \
             -gname play_ens]
set elapsed_ms [expr {[clock milliseconds] - $t0}]
puts "ensemble: 200 trials × 2000 dots × [dl_length $tl:t] frames in $elapsed_ms ms"

# Per-frame stats across trials. dl_transpose flips rows×frames ->
# frames×rows; dl_means/dl_stds reduce over the inner (trials) dim.
dl_local mean_coh [dl_means [dl_transpose $ens:coh_recorded]]
dl_local mean_dx  [dl_means [dl_transpose $ens:dx_mean]]
dl_local std_dx   [dl_stds  [dl_transpose $ens:dx_mean]]
dl_local design   $ens:coherence_design
dl_local t        $ens:t_design

# ---- Panel 1: coh_recorded mean lying on the design line ----
set p1 [dlp_newplot]
dlp_addXData $p1 $t
dlp_addYData $p1 $design     ;# y idx 0
dlp_addYData $p1 $mean_coh   ;# y idx 1
dlp_draw $p1 lines 0 -linecolor $colors(light_gray) -lwidth 300
dlp_draw $p1 lines 1 -linecolor $colors(blue)       -lwidth 100
dlp_set $p1 title  "coh_recorded mean (zero variance: rebalance is deterministic)"
dlp_set $p1 xtitle "time (s)"
dlp_set $p1 ytitle "coherent fraction"
dlp_setyrange $p1 -0.05 1.05

# ---- Panel 2: dx_mean ensemble (the stochastic dimension) ----
# The incoherent regime (troughs) shows visible 1/sqrt(N) noise; the
# coherent regime (peaks) locks onto cos(target_dir) = 1 because
# coherent dots all share the global direction with no jitter.
set p2 [dlp_newplot]
dlp_addXData $p2 $t
dlp_addYData $p2 $design    ;# normalized 0..1, plotted for reference
dlp_addYData $p2 $mean_dx
dlp_draw $p2 lines 0 -linecolor $colors(light_gray) -lwidth 300
dlp_draw $p2 lines 1 -linecolor $colors(red)        -lwidth 150
dlp_addErrbars $p2 1 $std_dx -vert -largs "-linecolor $colors(red)"
dlp_set $p2 title  "dx_mean ensemble: mean ± std across 200 trials"
dlp_set $p2 xtitle "time (s)"
dlp_set $p2 ytitle "population dx (cos(angle) avg)"

# Render both: stack vertically.
clearwin
dlp_setpanels 2 1
dlp_subplot $p1 0
dlp_subplot $p2 1
flushwin

puts ""
puts "coh_recorded:  max(mean - design) = [dl_max [dl_abs [dl_sub $mean_coh $design]]]"
puts "dx_mean:       max per-frame std  = [dl_max $std_dx]"
puts "               (compare 1/sqrt(2000) = 0.0224 -- expected trough sd)"
puts ""
puts "(to save: dumpwin postscript /tmp/play_ensemble.ps)"
