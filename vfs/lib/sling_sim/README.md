# sling_sim

The slingshot world, defined once. A ball seated at an anchor is drawn back
by a pull displacement `(dx, dy)` and released; it flies under gravity, may
bounce off static obstacles, and either lands in an open-topped target
bucket (**hit**) or does not (**miss** / **out** / **timeout**).

Pure Tcl over the headless `box2d` dlsh package, so the **same code** runs in
the ESS loader (reachability sweep), the stim (live preview and the
release-time pre-simulation that is then replayed), the viz, and off-line
analysis. The preview at the instant of release *is* the flight.

```tcl
package require sling_sim
set spec [sling_sim::default_spec]                 ;# a dict; every key documented there
lassign [sling_sim::velocity $spec -2.0 -1.0] vx vy ;# the ONE pull -> launch mapping
set r   [sling_sim::simulate $spec $vx $vy]         ;# outcome t_end land_x/y contacts t x y
set r   [sling_sim::preview  $spec -2.0 -1.0]       ;# same, from the pull
set sw  [sling_sim::sweep $spec -n_frac 8 -n_angle 36]   ;# hit set over the pull space
```

## Spec keys

| key | meaning |
|---|---|
| `anchor_x/y` | the seat (dva) |
| `reach`, `v_max`, `min_frac` | pull at full draw (dva), speed at full draw (dva/s), release below this fraction is an abort |
| `ball_r`, `gravity` | ball radius; gravity (dva/s², negative down) |
| `ground_y`, `field_hx/hy` | the floor; the field extent (leaving it = `out`) |
| `target_x/y/w/h`, `wall_t` | bucket floor centre, inner width, wall height, wall/floor thickness |
| `*_restitution` | ball / ground / target |
| `obstacles` | list of `{x y w h angle_deg restitution}` static boxes |
| `dt`, `max_t` | step (s) and cap (s) |

`spec_from_stimdg g row` / `specs_to_stimdg g specs` round-trip a spec through
stimdg: scalar keys one float column each, obstacles as six nested columns
`obs_x obs_y obs_w obs_h obs_angle obs_restitution`.

## Notes

- Box2D's world gravity is fixed at −10; the spec's `gravity` is honoured by a
  per-step force correction on the ball (`gravity_correction`).
- The integrator lands a few percent under the analytic apex. That bias is
  identical everywhere this package runs, which is what matters.
- `sling_sim::velocity` is duplicated in `dserv/lib/ess_sling-1.0.tm` (which
  cannot depend on dlsh.zip); `dserv/tests/test_ess_sling.tcl` pins the two.
- Moving obstacles are a later extension: kinematic bodies with a deterministic
  path and a phase in the spec, stepped identically in preview, presim and replay.

Test: `dlsh test_sling_sim.tcl` (sources the on-disk copy).
