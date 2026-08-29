# play_mapping.tcl -- 9-position rapid RF mapping using
# compile_mapping_spec. Trapezoid_train envelope + step_sequence
# trajectory + on/off threshold callbacks. Plots:
#   (1) coherence trace -- shows the trapezoid pulse train
#   (2) mask_offset_x trace -- shows position switches happen in OFF
#   (3) prints per-row callback firing times
#
# Source after dev.tcl:
#   source /Users/sheinb/src/dlsh/pkgs/mp_sim/dev.tcl
#   source /Users/sheinb/src/dlsh/pkgs/mp_sim/play_mapping.tcl

# A 3x3 grid of positions in dva. compile_mapping_spec walks them in
# order; each occupies one tile of the timeline.
set positions {
    {-3 -3} {0 -3} {3 -3}
    {-3  0} {0  0} {3  0}
    {-3  3} {0  3} {3  3}
}

# Per-row callbacks log via Tcl variables (in a real prf integration
# these would call dserv_send to emit events to dserv).
set ::__mapping_log [list]
proc mapping_on  {name fi t v} {
    lappend ::__mapping_log [list ON  $fi [format %.3f $t]]
}
proc mapping_off {name fi t v} {
    lappend ::__mapping_log [list OFF $fi [format %.3f $t]]
}

set base_spec [dict create \
    meta {dt 0.0167 patch_size_dva 13.0} \
    endpoints {target   {coh 1.0 speed 0.6 dir 0.0 life 0.5} \
               surround {coh 0.0 speed 0.23 dir 0.0 life 0.08}}]

catch {dg_delete play_map_tl}
set tl [mp_sim::compile_mapping_spec $base_spec \
            -positions $positions \
            -on_dur 0.150 -off_dur 0.050 -ease_dur 0.050 \
            -direction 0.0 \
            -on_callback mapping_on -off_callback mapping_off \
            -gname play_map_tl]

set duration [dl_get $tl:duration 0]
set nf       [dl_length $tl:t]
set npos     [llength $positions]
puts "mapping spec: $npos positions, ${duration}s total, $nf frames"

# Walk the timeline frame-by-frame, dispatching callbacks. (In live use
# the prescript would do this every refresh.)
for {set i 0} {$i < $nf} {incr i} {
    mp_sim::dispatch_callbacks_at $tl $i
}

puts "\nfired callbacks (frame index, time):"
foreach evt $::__mapping_log {
    lassign $evt phase fi t
    puts "  $phase  frame=$fi  t=${t}s"
}

# Plot envelope and mask_offset_x.
dl_local t   $tl:t
dl_local coh $tl:coherence
dl_local mox $tl:mask_offset_x

set p1 [dlp_newplot]
dlp_addXData $p1 $t
dlp_addYData $p1 $coh
dlp_draw $p1 lines 0 -linecolor $colors(blue) -lwidth 150
dlp_set $p1 title  "trapezoid pulse train (9 positions, 150ms ON, 50ms ease)"
dlp_set $p1 xtitle "time (s)"
dlp_set $p1 ytitle "coherence"
dlp_setyrange $p1 -0.05 1.10

set p2 [dlp_newplot]
dlp_addXData $p2 $t
dlp_addYData $p2 $mox
dlp_draw $p2 lines 0 -linecolor $colors(red) -lwidth 150
dlp_set $p2 title  "mask_offset_x: step changes in OFF windows"
dlp_set $p2 xtitle "time (s)"
dlp_set $p2 ytitle "mask_offset_x (dva)"

clearwin
dlp_setpanels 2 1
dlp_subplot $p1 0
dlp_subplot $p2 1
flushwin

puts ""
puts "(to save: dumpwin postscript /tmp/play_mapping.ps)"
