# play_single.tcl -- one trial, overlay design coherence and the
# realized coherent fraction. Sanity-check that a single realization
# tracks the Gaussian-pulse train.
#
# Source after dev.tcl in a dlshell session:
#   source /Users/sheinb/src/dlsh/pkgs/mp_sim/dev.tcl
#   source /Users/sheinb/src/dlsh/pkgs/mp_sim/play_single.tcl

set spec [dict create \
    meta {duration 2.0 dt 0.0167 patch_size_dva 13.0} \
    endpoints {target   {coh 1.0 speed 0.6 dir 0.0 life 0.5} \
               surround {coh 0.0 speed 0.23 dir 0.0 life 0.08}} \
    envelope {kind sum_gaussians n_pulses 5 sigma_ms 50.0 base_coh 1.0} \
    trajectory {kind static}]

catch {dg_delete play_tl}
set tl [mp_sim::compile_spec $spec -gname play_tl]

catch {dg_delete play_log}
set log [mp_sim::run_trial $tl -n_dots 2000 -seed 42 -gname play_log]

# log:coh_recorded is a 1-element nested column; index 0 gives the
# n_frames flist.
dl_local rec    [dl_get $log:coh_recorded 0]
dl_local design $log:coherence_design
dl_local t      $log:t_design

# Multi-trace plot. dlp_addXData / dlp_addYData push lists; the
# returned indices wire dlp_draw to the right datasets. Single x
# dataset (time), two y datasets.
set p [dlp_newplot]
dlp_addXData $p $t
dlp_addYData $p $design     ;# y dataset 0
dlp_addYData $p $rec        ;# y dataset 1
dlp_draw $p lines 0 -linecolor $colors(blue)   -lwidth 200
dlp_draw $p lines 1 -linecolor $colors(red)    -lwidth 100

dlp_set $p title  "design (blue) vs single trial coherent fraction (red)"
dlp_set $p xtitle "time (s)"
dlp_set $p ytitle "coherence"
dlp_setyrange $p -0.05 1.05
clearwin
dlp_plot $p
flushwin

puts "frame count: [dl_length $t]"
puts "design peak: [dl_max $design]   recorded peak: [dl_max $rec]"
puts "design trough: [dl_min $design]   recorded trough: [dl_min $rec]"
puts ""
puts "(to save: dlp_postscript $p /tmp/play_single.ps)"
