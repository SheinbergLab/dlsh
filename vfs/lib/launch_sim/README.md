# launch_sim

Headless generator for ballistic **launcher** trajectories — a projectile
fired from a launcher under gravity that lands at a boundary, optionally hidden
by an occluder. Pure `dlsh`/Tcl: **no stim2, dserv, or box2d**, so trials can be
generated, verified, and drawn in any environment with the dlsh libs (plain
`tclsh9`, `dlshell`, or the `ess_control` web virtual display). The same code
produces trials on the rig and ground truth off-rig.

It is the **generation** layer only (trajectory math + scene geometry + a
lightweight `dlg_` overview). The stim side consumes the `ball_t/x/y` + exit
contract and owns all GL rendering.

```tcl
package require launch_sim
set tr [launch_sim::sample_trajectory]        ;# one trial (a dict)
set g  [launch_sim::build_trials 100]         ;# a dynamic group of trials
```

## Boundary modes (`boundary` param)

| mode     | landing                                   | report |
|----------|-------------------------------------------|--------|
| `floor`  | parabola to a floor line, two goals       | `land_x` / `side` |
| `circle` | interior launcher → first rim crossing    | `exit_angle` |
| `arc`    | launcher-centered circle, downrange arc   | `deviation` from heading |

`arc` is the experiment geometry: the circle is centered on the launcher and
only a downrange arc (`arc_span_deg` wide, centered on the launch heading) is
valid; the report is the **signed deviation** from the heading (0 = straight
ahead). Linear motion lands at deviation ≈ 0; ballistic is offset by the
gravity-induced curve — so the deviation *is* the curvature-extrapolation
measure. Curvature (hence difficulty) is maximal when heading ⟂ gravity, ~0
when heading ∥ gravity. `catcher_pose {tr dev}` gives the tangent-rectangle
"catcher" pose at any swipe position.

Gravity of 0 yields exact straight-line (constant-velocity) motion.

## Kinematics source

- `ball_pos_at_time {tr t mode}` — position at any t (`analytic` exact parabola,
  `interp` linear over samples, `auto`). Decouples smoothness from the sample
  grid → smooth playback at any display refresh.
- `ball_vel_at_time {tr t mode}` — velocity companion (for SPEM eye-velocity
  scoring / pursuit-window lead).

## Occluder (decoupled overlay — never affects trajectories)

- `point_occluded {x y regions}`, `occlusion_intervals {bt bx by regions}`,
  `occlude {tr regions}`.
- `regions` is a union of primitives: `{type rect x0 y0 x1 y1}`,
  `{type circle cx cy r}`, `{type arc cx cy r0 r1 a0 a1}` (a0/a1 radians).
- `occlude` adds `occlusion_intervals` / `occlusion_enter_time` /
  `occlusion_exit_time` / `occlusion_duration` (the SPEM IV).
- `build_trials` accepts an `occluder` plus selection knobs: `occl_dur_min/max`
  (hidden-duration window) and `require_exit_occluded` (the exit is hidden, so
  the report is genuinely predictive — "occluded to the edge").

## Visualization

`draw_trial {tr}` renders a `dlg_` overview (ring/arc/floor, occluder, the
trajectory dimmed where occluded, launcher, exit/catcher) — identical in
`dlshell` and the `ess_control` virtual display.

## Develop / test

```sh
tclsh9.0 test_launch_sim.tcl        # headless test suite
```
```tcl
source dev.tcl                       # front-loads this checkout; gives demos
demo | demo_dots | demo_circle | demo_arc
```

`dlg_*`/`setwindow`/`clearwin` build the gbuf headless; only `flushwin` (the
display push) is supplied by the host — guarded with `[info commands flushwin]`.
