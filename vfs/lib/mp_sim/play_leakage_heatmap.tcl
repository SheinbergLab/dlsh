# play_leakage_heatmap.tcl -- 2D parameter-grid sweep over (sigma_ms,
# n_dots) for a TROUGH-aligned bounce, rendering the at-bounce leakage
# signal as a colored heatmap. The figure-4 deliverable: principled
# choice of (sigma, N) for the experiment.
#
# What you should see:
#   * Going DOWN  (smaller sigma_ms): trough deepens -> leakage shrinks
#   * Going LEFT  (smaller n_dots):   round() snaps trough count to 0
#                                     more often -> leakage shrinks too
#   * Top-right corner: shallow trough + many dots -> visible signal
#   * Bottom-left corner: deep trough + few dots -> below-threshold
#
# Source after dev.tcl:
#   source /Users/sheinb/src/dlsh/pkgs/mp_sim/dev.tcl
#   source /Users/sheinb/src/dlsh/pkgs/mp_sim/play_leakage_heatmap.tcl

set pre_dir  0.0
set post_dir [expr {3.14159265 / 3.0}]

set base_spec [dict create \
    meta {duration 2.0 dt 0.0167 patch_size_dva 13.0} \
    endpoints [dict create \
        target   [dict create coh 1.0 speed 0.6 dir $pre_dir life 0.5] \
        surround [dict create coh 0.0 speed 0.23 dir 0.0 life 0.08]] \
    envelope {kind sum_gaussians n_pulses 5 sigma_ms 80.0 base_coh 1.0} \
    trajectory {kind static} \
    bounce [dict create phase trough pulse_index 2 \
                pre_dir $pre_dir post_dir $post_dir]]

# Grid axes. sigma_ms varies the trough depth; n_dots controls the
# discretization noise floor.
set sigmas  {60 80 100 120 140 160}
set ndots_l {500 1000 2000 4000 8000}
set nx [llength $sigmas]
set ny [llength $ndots_l]
puts "running [expr {$nx * $ny}]-cell sweep ($nx sigmas x $ny n_dots), 30 trials each"

set t0 [clock milliseconds]
catch {dg_delete play_lk_sw}
set sw [mp_sim::sweep $base_spec \
            -vary [dict create envelope.sigma_ms $sigmas runtime.n_dots $ndots_l] \
            -n_trials 30 -seed 12345 -gname play_lk_sw]
puts "sweep done in [expr {[clock milliseconds] - $t0}] ms"

# For each cell, the "leakage at bounce_t" is dx_mean*cos(post_dir) +
# dy_mean*sin(post_dir) at the frame nearest bounce_t. Note the bounce
# is phase=trough/pulse_index=2 in EVERY cell; bounce_t depends on
# n_pulses+T (constant here) so it's the same frame for all cells.

# Compile a representative timeline to grab the bounce frame index.
set tl_ref [mp_sim::compile_spec $base_spec -gname _lk_ref_tl]
set bt [dl_get $tl_ref:bounce_t 0]
dl_local diffs [dl_abs [dl_sub $tl_ref:t $bt]]
set bidx [dl_minIndex $diffs]
puts "trough bounce_t = ${bt}s   (frame index $bidx)"
dg_delete $tl_ref

# Pull per-cell leakage values into a flat list. The sweep iterates
# cells in cartesian-product order with the FIRST vary key (sigma_ms)
# walking SLOWEST and the SECOND key (n_dots) walking FASTEST. So:
#   cell 0: sigma=60,  n_dots=500
#   cell 1: sigma=60,  n_dots=1000
#   ...
#   cell ny-1: sigma=60, n_dots=8000
#   cell ny:   sigma=80, n_dots=500
# i.e. column-major in (sigma, n_dots) where sigma is the column index.
# leakages[xi*ny + yi] = leakage at (sigma_xi, n_dots_yi).
set leakages [list]
set ncells [dl_get $sw:n_cells 0]
set c_post [expr {cos($post_dir)}]
set s_post [expr {sin($post_dir)}]
for {set i 0} {$i < $ncells} {incr i} {
    dl_local dxm [dl_get $sw:dx_mean_mean $i]
    dl_local dym [dl_get $sw:dy_mean_mean $i]
    set dx_at [dl_get $dxm $bidx]
    set dy_at [dl_get $dym $bidx]
    set proj [expr {$dx_at * $c_post + $dy_at * $s_post}]
    lappend leakages $proj
}

# Print the grid (rows = n_dots, cols = sigma). With column-major
# storage, table cell (yi, xi) is leakages[xi*ny + yi].
puts ""
puts "leakage signal at trough bounce_t (rows = n_dots, cols = sigma_ms):"
puts -nonewline [format "%-12s" "n_dots\\sigma:"]
foreach sm $sigmas { puts -nonewline [format "%10d" $sm] }
puts ""
for {set yi 0} {$yi < $ny} {incr yi} {
    set nd [lindex $ndots_l $yi]
    puts -nonewline [format "%-12d" $nd]
    for {set xi 0} {$xi < $nx} {incr xi} {
        set k [expr {$xi * $ny + $yi}]
        puts -nonewline [format "%10.4f" [lindex $leakages $k]]
    }
    puts ""
}

# Analytical prediction for comparison. The trough delivers BOTH a
# small coherent fraction AND surround-speed (because compile_spec
# tweens speed alongside coherence -- the matched-statistics core of
# the cryptic-motion design):
#     leakage(c) = c * (c * tgt_speed + (1 - c) * surround_speed)
# c = trough envelope value at bounce_t.
puts ""
puts "analytical prediction (n_dots-independent), leakage = c*(c*tgt + (1-c)*sur):"
puts -nonewline [format "%-12s" "sigma_ms:"]
set tgt_speed 0.6
set sur_speed 0.23
set t_bt 1.2
set centers {0.2 0.6 1.0 1.4 1.8}
foreach sm $sigmas {
    set sigma_s [expr {$sm / 1000.0}]
    set sum 0.0
    foreach c_k $centers {
        set z [expr {($t_bt - $c_k) / $sigma_s}]
        set sum [expr {$sum + exp(-0.5 * $z * $z)}]
    }
    set c $sum
    if {$c > 1.0} { set c 1.0 }
    set lk [expr {$c * ($c * $tgt_speed + (1.0 - $c) * $sur_speed)}]
    puts -nonewline [format "%10.4f" $lk]
}
puts ""

# Heatmap render. Use fixed vmin/vmax so the color scale represents the
# physical scale (0 = no leakage, 0.6 = full target_speed).
dl_local lk [dl_flist {*}$leakages]
set vmax 0.6
set vmin 0.0

set p [dlp_newplot]
# Plot is just a frame for the heatmap -- no data lines, but we still
# need DUMMY x/y data so dlp_setxrange/setyrange can clamp to a known
# data window during dlp_axes.
dlp_addXData $p [dl_flist 0 [expr {$nx - 1}]]
dlp_addYData $p [dl_flist 0 [expr {$ny - 1}]]
dlp_set $p title  "trough-bounce leakage (proj on post_dir): rows=n_dots, cols=sigma_ms"
dlp_set $p xtitle "sigma_ms (idx)"
dlp_set $p ytitle "n_dots (idx)"
dlp_setxrange $p -0.5 [expr {$nx - 0.5}]
dlp_setyrange $p -0.5 [expr {$ny - 0.5}]
clearwin
dlp_plot $p
# After dlp_plot returns, the viewport/window state isn't the plot's
# any more. dlp_pushwindow $p restores the plot's saved
# viewport+window so direct drawing primitives (filledrect) land in
# the correct coordinate system.
dlp_pushwindow $p
mp_sim::draw_heatmap [expr {($nx - 1) / 2.0}] [expr {($ny - 1) / 2.0}] \
    $lk $nx $ny $nx $ny -cmap VIRIDIS -vmin $vmin -vmax $vmax
dlp_popwindow
flushwin

puts ""
puts "vmin = $vmin   vmax = $vmax (= target_speed)"
puts "(to save: dumpwin postscript /tmp/play_leakage.ps)"
