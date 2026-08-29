# ess_test — a headless test harness for ESS loaders / stim / protocols

Status: SPEC (to be implemented in a fresh conversation). Author of the ad-hoc
patterns this generalizes: the pursuit/ballistic build session (2026-07-01).

## Goal

A dlsh-loadable Tcl package that validates the **deterministic logic and data
output** of ESS loaders, stim files, and (partially) protocol methods —
WITHOUT dserv, OpenGL, the rig, or any hardware. Turn the ad-hoc harnesses we
keep hand-rolling into `package require ess_test` primitives, so writing a new
loader/stim comes with a fast, repeatable inner test loop.

Every real bug caught before the rig this session came from one of these
patterns: the `::ess::` namespace-resolution abort, the flight-fraction→ms
timing, the parabola/tangent driver math, the `button_init {*}$args` blowup,
and the coh=0 invisibility (target==surround) check.

## Location & packaging

- Source: `~/src/dlsh/vfs/lib/ess_test/ess_test.tcl` (mirrors `launch_sim`).
- Header: `package provide ess_test 0.1` + `package require dlsh`.
- Bundled into `dlsh.zip` on the next VFS rebuild → available to `dlsh`,
  `dlsh -e '...'`, and `tclsh` (after `source /usr/local/dlsh/dlsh_setup.tcl`).
- Namespace: `::ess_test`. All state in that namespace; nothing leaks to `::`
  except the stubs it deliberately installs.

## Design principles

1. **Zero rig deps.** Only dlsh (`dl_*`/`dg_*`) is real. dserv/GL/hardware are
   absent or stubbed. Nothing here should ever need the rig to run.
2. **Faithful execution context where it matters.** Source loader/protocol
   files INSIDE `namespace eval ::ess { ... }` — ESS does this, so
   `namespace eval sys::proto {...}` lands at `::ess::sys::proto::` and helper
   procs resolve at their absolute `::ess::...` names. (This is what surfaced
   the `sample_fit` bug; a global-scope source hid it.) Run loader bodies via
   `apply` from a NON-`sys::proto` namespace, matching the real oo-method
   context (which is also not that namespace).
3. **Capturing stubs.** Stim2 GL commands become recording no-ops so you can
   assert on what the stim *would* draw/write (position, coherence, direction,
   speed, color, events) each frame — never on pixels.
4. **Return plain data; assert in the caller.** Loaders return the `stimdg`
   handle; the frame stepper returns per-frame records. Provide light assert
   helpers, not a heavy framework.
5. **Honest boundary.** This is a logic/data harness, not a renderer, a vsync
   clock, or a state-machine emulator. Say so loudly (see "Non-goals").

## Components / API

### 0. Bootstrap
- `package require ess_test` → pulls dlsh. Ensures `mp_sim` is loadable (real
  dlsh package) so stim files' `package require mp_sim` succeeds.
- Reads an optional systems root (default `~/systems/ess`) for locating files;
  overridable via `ess_test::config -systems_root <path>`.

### 1. ESS environment shim
- `ess_test::fake_ess` — `package provide ess <ver>` so `package require ess`
  in loader/protocol files succeeds. Installs a stub `::ess` namespace whose
  methods are inert or capturing (see fake_system). At file *source* time
  loaders/protocols mostly only define procs, so few ess calls fire then; the
  calls happen when bodies/methods run, where we control the context.

### 2. Capturing "system object" (shared by loader + protocol harnesses)
- `ess_test::fake_system ?name?` — returns a command that captures EVERY
  `$sys <subcmd> <args...>` into a structured registry:
  `add_param`, `add_variable`, `add_state`, `add_action`, `add_transition`,
  `add_method`, `set_start`, `set_end`, `set_*_callback`, `set_protocol`,
  `add_loader`, `set_viz_config`, ... Implement as an `oo::class` with
  `method unknown {sub args}` or a proc dispatcher.
- Query: `ess_test::sys_params $s`, `ess_test::sys_methods $s`,
  `ess_test::sys_loaders $s`, `ess_test::sys_callbacks $s`, etc.

### 3. Loader harness
- `ess_test::load_loaders {system protocol}` — find `<sys>/<proto>/<proto>_loaders.tcl`,
  `fake_ess`, `namespace eval ::ess { source <file> }`, then call
  `::ess::<sys>::<proto>::loaders_init [fake_system]`. Returns the captured
  loader name(s) + param lists.
- `ess_test::loader_params ?name?` — the captured param name list.
- `ess_test::run_loader ?name? <args>` — run the loader body with args. Support:
  - **named dict** `{nr 2 gravities {9.8 -9.8} ...}` → mapped onto the loader's
    param order (harness knows params); missing keys → error listing them (or a
    per-protocol defaults dict the caller can register).
  - **positional list** matching params exactly.
  Execute via `apply [list $params $body] {*}$vals` from `::` (NOT the loader
  namespace). Auto-`dg_delete stimdg` first. Returns the `stimdg` handle.
- `ess_test::dg_summary $g` — columns + lengths + a couple of sample rows.

### 4. Stim harness
- `ess_test::stub_stim2` — install stubs, in three classes:
  - **factory** (unique ids): `polygon metagroup motionpatch shaderImageCreate
    shaderImageID img_create img_drawPolygon img_drawPolygonFast img_imgtolist`.
  - **inert**: `glistInit resetObjList shaderImageReset polycirc polycolor
    scaleObj metagroupAdd glistAddObject glistSetDynamic glistSetCurGroup
    glistSetVisible redraw load_Impro img_delete masksoftness ...`.
  - **capturing** (record per target): `motionpatch_coherence/_speed/_lifetime/
    _direction/_color/_maskoffset/_maskscale/_pointsize/_masktype/_setSampler/
    _samplermaskmode translateObj setVisible objName dserv_send_evt
    addPreScript addThisFrameScript`.
  - `objName` builds a **name↔handle map**; captures are keyed by the registered
    name (e.g. `dots_target`) with handle canonicalization. (Name-vs-handle was
    the bug in the first invisibility audit — bake the resolution in.)
- Globals: `::StimTimeF` (float ms), `::StimTicksF`, `::StimTime` (int ms),
  `::SwapCount`. Settable; default 0.
- `ess_test::stim_source {system protocol}` — source `<proto>_stim.tcl` with
  stubs installed and mp_sim available.
- Frame stepping (emulates the stim2 per-frame loop):
  - `ess_test::set_time $ms`.
  - `ess_test::step ?-dt 0.016?` — advance StimTimeF by dt, run the captured
    `addPreScript` proc(s), then any `addThisFrameScript` (post-flip), and
    return this frame's captured writes (a dict keyed by target).
  - `ess_test::play -dur $s -dt $dt` — loop `step`, return a list of frame
    records (or transposed columns for easy assertion).
  - Decide prescript model: single vs multiple; whether to emulate metagroup
    member framescripts (see reference_stim2_metagroup_traversal) — probably
    start with the top-level prescript only (what pursuit uses).
- `ess_test::captured <cmd> <target>` — history; `ess_test::last <cmd> <target>`.
- `ess_test::events` — the `dserv_send_evt` calls (name + payload), for
  asserting motion_on/probe_on/coh_event timing.

### 5. Assertions / runner
- `ess_test::assert {expr} msg` (count + print ok/FAIL, like test_launch_sim.tcl).
- `ess_test::approx a b ?tol?`, `ess_test::in_range v lo hi`.
- `ess_test::test name body` blocks; `ess_test::summary` → pass/fail + exit code.

### 6. Data (dgz) helpers
- Thin conveniences over dl_/dg_ (`ess_test::col`, min/max, per-row). The richer
  recorded-dot-log analysis stays in Python (`dgread` + numpy) — reference the
  existing `~/systems/ess/pursuit/pursuit_dotlog_verify.py` as the pattern;
  don't reimplement it in Tcl.

## Example usage (target ergonomics)

```tcl
package require ess_test

# ---- loader ----
ess_test::load_loaders pursuit ballistic
set g [ess_test::run_loader {
    nr 2  gravities {9.8 0 -9.8}  launch_params {}  fit_halfextent 10.0
    fix_r 0.2  target_type pulsed_patch  spot_color {0.9 0.9 0.9}
    patch_size 16 dot_density 8 pointsize 2.5 target_speed_ratio 1.0
    target_lifetime 0.15 surround_speed_dva_sec 3 bg_lifetime 0.1
    target_diameter 2.5 surround_alpha 1 target_color {0.6 0.6 0.6}
    surround_color {0.6 0.6 0.6} coherence 1 surround_coherence 0
    cue_color none internal_rotation_deg 0
    coh_params {coh_profile gate coh_lo 0 coh_onset_frac 0.30 coh_dur_frac 0.40}
    probe_params {probe_type color probe_time_frac {0.45 0.55 0.65} catch_frac 0.2
                  probe_frames 2 probe_color_a {0.5 0.8 0.5} probe_color_b {0.8 0.5 0.5}}
}]
ess_test::assert {[dl_length $g:stimtype] == 6} "6 trials (2 x {9.8 0 -9.8})"
ess_test::assert {[ess_test::in_range [dl_get $g:probe_time_ms 0] \
                     [dl_get $g:coh_on_ms 0] [dl_get $g:coh_off_ms 0]]} "probe inside dip"

# ---- stim ----
ess_test::stub_stim2
ess_test::stim_source pursuit ballistic
nexttrial 0 [dl_get $g:fix_x 0] [dl_get $g:fix_y 0] [dl_get $g:fix_r 0]
pursuit_start
set F [ess_test::play -dur [dl_get $g:land_time 0] -dt 0.016]
# assert tangent direction rotates, coherence hits 0 mid-dip, probe = N frames,
# and events fired: ess_test::events -> motion_on at t0, probe_on in the dip
ess_test::summary
```

## Non-goals (say these in the package header too)
- No rendering (GL) — stubs capture intent, not pixels.
- No real vsync/timing — you drive a synthetic StimTimeF.
- No hardware (eye/joystick/juicer/sound) — protocol harness stubs those methods.
- No full state-machine run against the real dserv event loop (tier 4 can
  *partially* unit-test protocol methods with mocked ess + fake variables).

## Open questions for the build conversation
1. File location: hardcode `~/systems/ess/<sys>/<proto>/...`, accept explicit
   paths, or use ess_paths if it loads headless? (Start: convention + override.)
2. Named-arg→positional mapping + per-protocol default arg dicts (so tests are
   short). Register defaults where — in the test, or a sidecar?
3. fake_system implementation: oo::class `unknown` vs proc dispatcher.
   RESOLVED: proc dispatcher for the system object; but `my` MUST be a
   namespace ensemble, not a proc — a proc frame breaks `dl_return` out of a
   helper loader (`dynlist ">0<" not found`).
4. Prescript execution model + metagroup member framescripts + ThisFrameScript
   ordering.
5. Which `::ess::*` methods to stub for the tier-4 protocol harness.
6. Int vs float clock exposure (StimTime vs StimTimeF; see reference).

## Build tiers (value/effort order)
- **Tier 1 (MVP):** fake_ess + fake_system + load_loaders + run_loader + assert.
  → loader/stimdg validation. Covers most bugs. Ship this first.
- **Tier 2:** stub_stim2 + stim_source + step/play + captures + events.
  → per-frame stim driver validation.
- **Tier 3:** dg conveniences; port this session's pursuit checks to ~10-line
  ess_test tests as the first "customer"; document the pattern in the
  ess-development skill.
- **Tier 4 (stretch):** fake_system-driven protocol-method unit tests
  (`responded`, reward/scoring, `nexttrial`) with mocked ess + variables.

### Added after the fact: the ESS harness (`real_ess`)

Tiers 1-2 *fake* the ess package, which leaves the load pipeline itself
(`load_system`, `protocol_init`, `variant_init`, loader-arg plumbing, the
datapoints ESS publishes, its error handling) testable only against a running
dserv. `ess_test::real_ess` loads the genuine `ess-2.0.tm` and stubs only the
dserv/hardware C commands; `load_system` returns `{ok trials error}` without
rethrowing, and `datapoint`/`datapoints`/`dserv_log`/`dserv_history` assert on
what was published and in what order.

Verified: all 12 systems under `~/systems/ess` load headless this way. Scope is
LOADING, not RUNNING — no state machine, no real timing, no stim2.

Motivating bug: a loader that threw on a missing sqlite database left
`ess/status` at `loading` forever, which disables every control in
`ess_control.html`. `test_ess_harness.tcl` is the regression suite for that
contract, and it fails against the pre-fix `ess-2.0.tm`.

## Success criterion
Re-express this session's ad-hoc harnesses (loader stimdg checks; the
parabola/tangent/|v|-speed driver check; the coh=0 target==surround invisibility
check; syntax-source checks) as short `ess_test` tests runnable via
`dlsh -e 'package require ess_test; source pursuit_tests.tcl'`.
