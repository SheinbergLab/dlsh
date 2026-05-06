# play_bounce.tcl -- peak vs trough direction-discontinuity ("bounce")
# leakage demo. Builds two ensembles with identical envelope but
# different bounce phase, projects the population velocity vector onto
# the post-bounce direction, and overlays the two traces.
#
# What you should see:
#   PEAK bounce:    proj jumps to ~target_speed at bounce_t (full motion
#                   energy in the new direction is delivered)
#   TROUGH bounce:  proj barely budges -- only the few coherent dots at
#                   trough depth carry the new direction signal.
#                   Magnitude = speed * c_trough  (what we predict).
#
# Source after dev.tcl:
#   source /Users/sheinb/src/dlsh/pkgs/mp_sim/dev.tcl
#   source /Users/sheinb/src/dlsh/pkgs/mp_sim/play_bounce.tcl

# Bounce: 60 deg deflection (pre = 0, post = pi/3).
set pre_dir  0.0
set post_dir [expr {3.14159265 / 3.0}]

set base_spec [dict create \
    meta {duration 2.0 dt 0.0167 patch_size_dva 13.0} \
    endpoints [dict create \
        target   [dict create coh 1.0 speed 0.6 dir $pre_dir life 0.5] \
        surround [dict create coh 0.0 speed 0.23 dir 0.0 life 0.08]] \
    envelope {kind sum_gaussians n_pulses 5 sigma_ms 50.0 base_coh 1.0} \
    trajectory {kind static}]

# Two specs that differ only in bounce phase. Same pulse_index so the
# two bounces happen at comparable times (peak vs midpoint between
# pulses 2 and 3 ~ frame 60).
set spec_peak [dict merge $base_spec [dict create \
    bounce [dict create phase peak  pulse_index 2 \
                pre_dir $pre_dir post_dir $post_dir]]]
set spec_trough [dict merge $base_spec [dict create \
    bounce [dict create phase trough pulse_index 2 \
                pre_dir $pre_dir post_dir $post_dir]]]

catch {dg_delete play_tl_peak}
catch {dg_delete play_tl_trough}
set tl_peak   [mp_sim::compile_spec $spec_peak   -gname play_tl_peak]
set tl_trough [mp_sim::compile_spec $spec_trough -gname play_tl_trough]

set bt_peak   [dl_get $tl_peak:bounce_t   0]
set bt_trough [dl_get $tl_trough:bounce_t 0]
puts "bounce times:  peak @ ${bt_peak}s   trough @ ${bt_trough}s"
puts "post_dir = [format %.3f $post_dir] rad   (pre=0)"

# Run ensembles. Modest n_dots so the leakage shows up clearly.
set t0 [clock milliseconds]
catch {dg_delete play_ens_peak}
catch {dg_delete play_ens_trough}
set ens_p [mp_sim::ensemble $tl_peak   -n_dots 2000 -n_trials 100 -seed 1 \
              -gname play_ens_peak]
set ens_t [mp_sim::ensemble $tl_trough -n_dots 2000 -n_trials 100 -seed 1 \
              -gname play_ens_trough]
puts "two ensembles in [expr {[clock milliseconds] - $t0}] ms"

# Project population velocity onto the post-bounce direction.
mp_sim::leakage_projection $ens_p $post_dir
mp_sim::leakage_projection $ens_t $post_dir

dl_local proj_p $ens_p:proj_post_dir
dl_local proj_t $ens_t:proj_post_dir
dl_local t      $ens_p:t_design

# Quantify "post-bounce signal at the bounce instant" for each.
dl_local diff_p [dl_abs [dl_sub $t $bt_peak]]
dl_local diff_t [dl_abs [dl_sub $t $bt_trough]]
set bidx_p [dl_minIndex $diff_p]
set bidx_t [dl_minIndex $diff_t]
set sig_p [dl_get $proj_p $bidx_p]
set sig_t [dl_get $proj_t $bidx_t]

puts ""
puts "post-bounce direction signal AT bounce_t:"
puts [format "  peak   bounce: proj = %.4f  (full delivery; expected ~%.4f)" \
          $sig_p [expr {0.6 * cos($post_dir - $post_dir)}]]
puts [format "  trough bounce: proj = %.4f  (leakage; predicted = base_coh * c_trough * speed * cos(0))" \
          $sig_t]

# Overlay plot. proj_p (blue, peak) and proj_t (red, trough).
set p [dlp_newplot]
dlp_addXData $p $t
dlp_addYData $p $proj_p
dlp_addYData $p $proj_t
dlp_draw $p lines 0 -linecolor $colors(blue) -lwidth 200
dlp_draw $p lines 1 -linecolor $colors(red)  -lwidth 200
dlp_set $p title  "post-bounce direction signal: PEAK (blue) vs TROUGH (red)"
dlp_set $p xtitle "time (s)"
dlp_set $p ytitle "proj onto post_dir = dx cos + dy sin"
clearwin
dlp_plot $p
flushwin

puts ""
puts "(to save: dlp_postscript $p /tmp/play_bounce.ps)"
