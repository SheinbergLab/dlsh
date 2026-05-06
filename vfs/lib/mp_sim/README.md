# mp_sim

Headless simulator and design-spec compiler for cryptic-motion / pulsed-coherence random-dot kinematograms. Mirrors the dot-update semantics of stim2's `motionpatch.c` without a display, so trial ensembles can be run in bulk for figure generation, parameter sweeps, and statistical verification of the manipulation. Also provides a stable on-disk schema (the "state timeline" dg) that lets stim, analysis, and live experimental control all consume the same description of what should happen at each frame.

## Quick start

```tcl
package require mp_sim

# Compile a 5-pulse Gaussian envelope on a static patch:
set spec [dict create \
    meta {duration 2.0 dt 0.0167 patch_size_dva 13.0} \
    endpoints {target   {coh 1.0 speed 0.6 dir 0.0 life 0.5} \
               surround {coh 0.0 speed 0.23 dir 0.0 life 0.08}} \
    envelope {kind sum_gaussians n_pulses 5 sigma_ms 50.0 base_coh 1.0} \
    trajectory {kind static}]

set tl [mp_sim::compile_spec $spec]
# tl is a dg name with per-frame columns: t, mask_offset_x/y, direction,
# coherence, speed, lifetime_s. Plus group-level scalars: dt, n_frames,
# duration, base_coh, tile_times, patch_size_dva.

# Run one trial, returns a 1-row ensemble dg:
set log [mp_sim::run_trial $tl -n_dots 2000 -seed 42]

# Run 200 trials, returns an n-row nested dg:
set ens [mp_sim::ensemble $tl -n_dots 2000 -n_trials 200 -seed 1]

# Per-frame mean across trials (uses dlsh's recursive vectorized math):
dl_local mean_coh [dl_means [dl_transpose $ens:coh_recorded]]
```

## Two layers

**Layer A — Design spec → state timeline.** A high-level Tcl dict describes the trial; `compile_spec` evaluates it onto a per-frame timeline dg.

**Layer B — Timeline → simulated trial.** Given a timeline plus `(n_dots, dt, seed)`, `run_trial` runs a Tcl reimplementation of the C dot-update kernel and returns per-frame summary stats (and optionally raw per-dot positions). `ensemble` runs N trials and accumulates rows into a nested dg.

## Spec dict shape

```
{meta       {duration <sec> dt <sec> ?patch_size_dva <dva>?}
 endpoints  {target   {coh <0..1> speed <pu/s> dir <rad> life <s>}
             surround {coh <0..1> speed <pu/s> dir <rad> life <s>}}
 envelope   <envelope-dict>
 trajectory <trajectory-dict>
 ?bounce    <bounce-dict>?
 ?callbacks <callback-list>?}
```

The endpoints' coh/speed/dir/life are tweened in lockstep by the normalized envelope frac = coh/base_coh — the "single knob across multiple parameters" property that makes the trough state statistically identical to the surround.

## Envelope kinds

Atomic:
- `{kind flat              base_coh V}`
- `{kind sum_gaussians     n_pulses N sigma_ms S ?centers C? base_coh V}`
- `{kind cosine_ramp       t0 T0 t1 T1 base_coh V}`
- `{kind gate              t0 T0 t1 T1 base_coh V}`
- `{kind sigmoid           t0 T0 tau TAU base_coh V}`
- `{kind trapezoid_train   centers L plateau_dur P ease_dur E base_coh V}`
- `{kind callback          proc P ?args A?}` — user proc taking `$ts $args`

Compositors:
- `{kind product   parts {<env1> <env2> ...}}`
- `{kind sum       parts {<env1> <env2> ...} base_coh V}`

Add a new kind = one switch case in `eval_envelope` + a small private proc.

## Trajectory kinds

- `{kind static    ?x X? ?y Y?}`
- `{kind sweep     x0 X0 x1 X1 ?y Y?}`
- `{kind callback  proc P}` — user proc takes `$t`, returns `{x y}`
- `{kind step_sequence positions {{x y} ...} step_times {t0 t1 ...}}` — discrete location switches

## Callbacks

Spec can include a `callbacks` list. compile_spec pre-computes threshold-crossing frames; both headless `run_trial` and live stim prescripts can dispatch via `mp_sim::dispatch_callbacks_at $timeline $frame_idx`.

```tcl
set spec [dict merge $base_spec [dict create \
    callbacks {
        {name patch_on  threshold 0.5 direction rising  proc emit_event_on}
        {name patch_off threshold 0.5 direction falling proc emit_event_off}
    }]]
```

Callback signature: `{*}$proc $name $frame_idx $t $envelope_value`.

## Convenience: `compile_mapping_spec`

Builds a trapezoid_train envelope synchronized with a step_sequence trajectory for rapid RF mapping. Position switches happen during OFF windows by construction; per-tile directions are optional.

```tcl
set tl [mp_sim::compile_mapping_spec $base_spec \
            -positions {{-3 -3} {0 -3} {3 -3} ... } \
            -on_dur 0.150 -off_dur 0.050 -ease_dur 0.050 \
            -direction 0.0 \
            -directions {1.0 2.0 3.0 ...}        ;# optional per-tile dirs
            -on_callback emit_on -off_callback emit_off]
```

## Visualization

- `mp_sim::colorize_to_image $values -cmap VIRIDIS -vmin V -vmax V` — RGB-packed char list for `dlg_image` (image-blit pattern).
- `mp_sim::draw_heatmap cx cy $values w h nx ny -cmap M -vmin V -vmax V` — render a 2D grid as filled rectangles (more reliable than dlg_image for small grids).
- `mp_sim::leakage_projection $ens_dg $post_dir` — project ensemble-mean (dx, dy) onto an arbitrary direction; useful for bounce-leakage analyses.

## Threshold-crossing events for live experiments

The same callback system that fires in `dispatch_callbacks_at` is what the prf::motionpatch_continuous protocol uses to emit frame-locked events to dserv. The stim-side prescript reads the timeline column `callback_K_frames`, fires the registered proc when the current frame matches, and the proc then calls `dserv_send` to deliver a binary event payload to the host's dserv. The protocol subscribes via `dservAddExactMatch` + `dpointSetScript` and logs via `::ess::evt_put`. mp_sim itself stays headless and ignorant of dserv — callbacks are just Tcl procs.

## Tests

`tclsh testmp_sim.tcl` — 12 tests covering schema, kernel bounds, ensemble means, sweeps, bounce leakage, mapping with callbacks, per-tile directions, and resource cleanup.

## Play scripts (for live dlshell sessions)

| script | shows |
|---|---|
| `dev.tcl` | dev-mode loader (front-of-path on auto_path + `mp_sim_reload`) |
| `play_single.tcl` | one trial, design vs realized coherence overlay |
| `play_ensemble.tcl` | 200-trial mean ± std band on dx_mean (1/√N noise floor visible) |
| `play_sweep.tcl` | 9-cell σ × n_dots grid summary table |
| `play_bounce.tcl` | peak vs trough bounce leakage projections |
| `play_leakage_heatmap.tcl` | 2D heatmap of trough-bounce leakage across (σ, N) |
| `play_mapping.tcl` | 9-position rapid RF mapping with callback firing log |

## Versions

- 0.1 — initial release: kernel, run_trial, ensemble, sweep, bounce, leakage_projection.
- 0.2 — envelope dispatcher with composable kinds, step_sequence trajectory, threshold-crossing callbacks, compile_mapping_spec, per-tile directions, viridis heatmap helpers.
