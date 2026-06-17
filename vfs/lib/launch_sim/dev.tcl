# dev.tcl -- interactive dev/visual-check helper for launch_sim.
#
# In dlshell (Tk canvas) or any dlg_-capable shell:
#   source /Users/sheinb/src/dlsh/vfs/lib/launch_sim/dev.tcl
#   demo            ;# draw one random occlusion trial
#   demo 1          ;# force right-goal trial
#   demo_dots       ;# draw one no-occluder (dots-style) trial
#
# Front-loads this on-disk checkout ahead of any copy baked into dlsh.zip,
# and re-sources it fresh, so edits to launch_sim.tcl take effect on a plain
# `source dev.tcl` without repacking the zip.

# Load dlsh FIRST (mount the zip if we're in a bare tclsh; a no-op inside
# dlshell where it's already loaded). Must precede front-loading vfs/lib --
# otherwise the on-disk `dlsh` pkgIndex shadows the zip one and tries to load
# a compiled .dylib that only exists inside dlsh.zip.
catch { source /usr/local/dlsh/dlsh_setup.tcl }
package require dlsh

# Source the ON-DISK launch_sim directly (auto_path can't reliably shadow a
# copy already baked into dlsh.zip), so edits take effect without a repack.
catch {package forget launch_sim}
catch {namespace delete ::launch_sim}
source [file join [file dirname [info script]] launch_sim.tcl]

# Draw one occlusion-board trial (side: -1 random, 0 left, 1 right).
proc demo { {side -1} {params {}} } {
    set tr [launch_sim::sample_trajectory $params $side]
    launch_sim::draw_trial $tr
    return $tr
}

# Draw one no-occluder (dots-style) trial.
proc demo_dots { {side -1} } {
    set p [dict merge [launch_sim::default_params] {occluder_x {} launcher_x_max 8.0}]
    demo $side $p
}

# Draw one circle-boundary trial: interior launcher, exit on the rim, with a
# decoupled annular-sector occluder near the rim. side: -1 random, 0/1 half.
proc demo_circle { {side -1} } {
    set p [dict merge [launch_sim::default_params] \
        {boundary circle angle_min 0 angle_max 360 gravity_min 0 gravity_max 12 circle_r 9.0}]
    set tr [launch_sim::sample_trajectory $p $side]
    # an arc-sector occluder hugging the top of the rim (decoupled overlay)
    set occ [list [dict create type arc cx 0 cy 0 r0 6 r1 9 \
        a0 [expr {3.14159265*0.25}] a1 [expr {3.14159265*0.75}]]]
    set tr [launch_sim::occlude $tr $occ]
    launch_sim::draw_trial $tr
    return $tr
}

# Draw one arc-landing trial: launcher-centered landing arc (only the valid
# span shown), report = deviation from heading, catcher = tangent bar at the
# landing, plus a decoupled rectangular occluder across the mid-path.
proc demo_arc { {side -1} } {
    set p [dict merge [launch_sim::default_params] \
        {boundary arc angle_min 0 angle_max 360 gravity_min 3 gravity_max 14 \
         arc_radius 8 arc_span_deg 140 launcher_jitter 1.5}]
    set tr [launch_sim::sample_trajectory $p $side]
    # a rectangle occluding the mid-path (occluder is decoupled from physics)
    set mx [expr {0.5*([dict get $tr launcher_x]+[dict get $tr land_x])}]
    set my [expr {0.5*([dict get $tr launcher_y]+[dict get $tr land_y])}]
    set occ [list [dict create type rect \
        x0 [expr {$mx-2.0}] y0 [expr {$my-2.0}] x1 [expr {$mx+2.0}] y1 [expr {$my+2.0}]]]
    set tr [launch_sim::occlude $tr $occ]
    launch_sim::draw_trial $tr
    puts [format "  arc: heading=%.0f deg  deviation=%.1f deg  occl_dur=%.3f s" \
        [expr {[dict get $tr heading]*180.0/3.14159265}] \
        [expr {[dict get $tr deviation]*180.0/3.14159265}] \
        [dict get $tr occlusion_duration]]
    return $tr
}

puts "launch_sim [package present launch_sim] loaded from $_ls_dir"
puts "try: demo | demo 1 | demo_dots | demo_circle | demo_arc"
