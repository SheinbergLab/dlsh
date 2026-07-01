# ess_test

A **headless test harness** for ESS loaders, stim files, and (partially)
protocol methods. It validates the *deterministic logic and data output* of ESS
scripts **without dserv, OpenGL, the rig, or any hardware** — so writing a new
loader/stim comes with a fast, repeatable inner loop you can run from a plain
`dlsh -e`.

Only `dlsh` (`dl_*`/`dg_*`) and the pure-dlsh packages a script actually
requires (`launch_sim`, `mp_sim`, …) are real. dserv, GL, and hardware are
absent or stubbed.

```tcl
package require ess_test

# ---- Tier 1: loader -> stimdg (pure dlsh) ----
ess_test::load_loaders pursuit ballistic
set g [ess_test::run_loader { nr 2  gravities {9.8 0 -9.8}  ... }]
ess_test::assert {[dl_length $g:stimtype] == 6} "6 trials"

# ---- Tier 2: per-frame stim driver (capturing stubs) ----
ess_test::stub_stim2
ess_test::stim_source pursuit ballistic
nexttrial 0 [dl_get $g:fix_x 0] [dl_get $g:fix_y 0] [dl_get $g:fix_r 0]
target_on ; pursuit_start
ess_test::play -dur [dl_get $g:land_time 0] -dt 0.016
ess_test::assert {[tcl::mathfunc::min {*}[ess_test::values motionpatch_coherence dots_target]] < 0.05} \
    "coherence hits ~0 in the gate"
ess_test::summary
```

See **`test_ess_test.tcl`** for a complete, passing worked example against the
real `pursuit/ballistic` system — copy it as the template for a new system.

---

## What it can and cannot check (read this)

The stubs capture the **numbers the stim hands stim2** each frame — the
arithmetic the driver computes — never pixels.

**CAN check (deterministic, machine-checkable):**
- loader → `stimdg`: column presence, lengths, factorial counts, per-row values
- timing math: flight-fraction → ms conversions, coherence dip placement, probe
  onset inside the dip
- per-frame driver: target position along the trajectory, internal direction
  tracking the tangent, internal speed scaling with `|v(t)|`, coherence dipping
  to its floor (via the **real** `mp_sim` timeline), probe firing for N frames
- event timing: `motion_on` at t0, `coh_event` at landmarks, `probe_on` in the dip
- that a file **sources** headless (catches syntax / `package require` / typo'd
  `objName` errors before you ever open stim2)

**CANNOT check (needs real stim2 + your eyes):**
- whether the disc is actually *invisible*, mask/sampler appearance, real dot
  rendering and motion
- real vsync / frame timing (you drive a synthetic `StimTimeF`)
- hardware (eye/joystick/juicer/sound), and the full state machine against the
  live dserv event loop

Use `ess_test` to catch math/data/timing regressions fast and headless; use real
stim2 to see how it looks. They are complementary, not redundant.

---

## Running

```sh
# a test file directly
dlsh vfs/lib/ess_test/test_ess_test.tcl

# or the spec's inline form (once ess_test is in dlsh.zip)
dlsh -e 'package require ess_test; source my_tests.tcl'
```

`ess_test` lives in `dlsh.zip`, so after the next VFS rebuild `package require
ess_test` works from `dlsh`, `dlsh -e '...'`, and any `tclsh` that has sourced
`/usr/local/dlsh/dlsh_setup.tcl`. **Before** the rebuild (i.e. while editing the
package itself), source the on-disk copy — see the bootstrap block at the top of
`test_ess_test.tcl`.

By default it looks for systems under `~/systems/ess`. Override with:

```tcl
ess_test::config -systems_root /path/to/systems/ess
```

---

## Writing tests for a NEW loader / stim

The workflow that catches the most bugs, in value order:

### 1. Loader → stimdg (do this first)

```tcl
package require ess_test
ess_test::load_loaders <system> <protocol>        ;# sources *_loaders.tcl inside ::ess
# register a defaults dict once so each test varies only what it needs:
ess_test::loader_defaults <loader_name> { <all params with sane defaults> }
set g [ess_test::run_loader { <keys you want to override> }]

ess_test::assert {[dl_length $g:stimtype] == <expected>} "trial count"
ess_test::assert {[dl_exists $g:my_new_col]} "new column present"
puts [ess_test::dg_summary $g]                    ;# eyeball all columns + samples
```

`load_loaders` sources the file **inside `namespace eval ::ess`**, exactly as the
rig does, and `run_loader` executes the loader body via `apply` from the global
namespace `::` — matching the real oo-method context. This is deliberate: it
reproduces the execution context in which absolute-name helper calls
(`::ess::sys::proto::my_helper`) must resolve, so a namespace-resolution bug
surfaces *here* instead of on the rig.

### 2. Stim per-frame driver

```tcl
ess_test::stub_stim2                               ;# install capturing stubs
ess_test::stim_source <system> <protocol>          ;# sources *_stim.tcl headless

ess_test::set_time 0
nexttrial 0 [dl_get $g:fix_x 0] [dl_get $g:fix_y 0] [dl_get $g:fix_r 0]
ess_test::clear_captures                           ;# ignore build-time writes
target_on ; pursuit_start                          ;# whatever your protocol calls
ess_test::play -dur [dl_get $g:land_time 0] -dt 0.016

# assert on what the driver wrote each frame:
ess_test::values  motionpatch_direction dots_target   ;# list of per-frame values
ess_test::last    motionpatch_coherence dots_target   ;# most recent record
ess_test::events  pursuit/probe_on                    ;# {t name payload} records
```

If your stim uses different object names or motionpatch setters, they are
already captured — `stub_stim2` records **every** `motionpatch_*` setter plus
`translateObj`, `polycolor`, `setVisible`, and `dserv_send_evt`, keyed by the
`objName` you registered (handles are canonicalized to their names).

---

## API reference

### Config
| Command | Purpose |
|---|---|
| `config` | return the current config (`systems_root`, `ambient`, `dserv_data`) |
| `config -systems_root path` | set the systems root (default `~/systems/ess`) |
| `config -screen_halfx v` / `-screen_halfy v` | screen geometry injected into loader scope |
| `config -ambient dict` | merge more ambient vars injected as loader locals |
| `config -dserv {key val …}` | seed values returned by `dservGet` |
| `config -tm_path dir` | extra Tcl module (.tm) dir on the `package require` path |
| `script_path sys proto type` | resolve a file path (`type`: loaders/stim/variants/protocol/system) |

**ESS execution context.** On the rig a loader runs as an oo method with ESS
ambient state around it. The harness reproduces enough of that to run real
loaders headless:
- injects **ambient variables** (`screen_halfx/halfy`, extend via `config -ambient`) as locals;
- harvests the **system/protocol params + variables** (from `add_param`/`add_variable`, by best-effort running the protocol's `protocol_init`) and injects those too, so a loader that reads e.g. `$n_choices` works;
- puts **`<systems_root>/lib`** (and `config -tm_path` dirs) on the Tcl module path, so `package require planko`/`haptic`/`blob`/… resolve;
- **stubs dserv** (`dservGet` returns seeded values, default `ess/screen_refresh_rate 60`; writes/subscriptions go nowhere) and seeds a few ESS globals (`::ess::system_path`, `::ess::current(...)`, `::ess::rmt_host`, …);
- binds **`my`** so a delegating variant loader (`my setup_trials …`) runs the sibling loader;
- best-effort **sources the system and protocol files** so shared helpers (e.g. `::ess::prf::generate_positions`) resolve.

Disable the last two with `load_loaders sys proto -with_system 0 -with_protocol 0`.

### Tier 1 — loader harness
| Command | Purpose |
|---|---|
| `fake_ess ?ver?` | satisfy `package require ess` + stub `::ess` namespace |
| `fake_system ?name?` | a capturing `$sys`; every `$sys sub …` is recorded |
| `sys_calls $s` / `sys_subcall $s sub` | raw captured calls |
| `sys_params`/`sys_variables`/`sys_methods`/`sys_loaders $s` | captured names by kind |
| `load_loaders sys proto ?-file path?` | source `*_loaders.tcl` in `::ess`, run `loaders_init`, return loader names |
| `loader_params ?name?` | captured parameter-name list |
| `loader_defaults name dict` | register a default arg dict for `run_loader` |
| `run_loader ?name? <dict\|list>` | run a loader body, return the `stimdg` handle |
| `load_variants sys proto` | source `*_variants.tcl`, return the normalized variants dict |
| `variant_args sys proto variant` | `{loader_proc arg-values}` for a variant's defaults |
| `run_variant sys proto variant` | run the loader as ESS would for that variant → `stimdg` |
| `dg_summary g ?nrows?` | columns + lengths + sample rows |

`run_variant` reproduces ESS's own variant→args resolution (comment-stripping
via the borrowed `normalize_variants`, preset-var substitution, first-choice of
each `loader_option` normalized to `{name value}`), so you can dry-run any
variant without hand-writing its args:

```tcl
ess_test::load_loaders pursuit ballistic
set g [ess_test::run_variant pursuit ballistic ballistic_detect]
ess_test::assert {[dl_length $g:stimtype] == 40} "detect variant = 40 trials"
```

`run_loader` accepts a **named dict** (`{nr 2 gravities {…} …}`, mapped onto the
loader's param order; missing keys error unless supplied by registered
defaults) or a **positional list** matching the params exactly.

### Tier 2 — stim harness
| Command | Purpose |
|---|---|
| `stub_stim2` | install factory/inert/capturing stubs; resets stim state |
| `stim_source sys proto ?-file path?` | source `*_stim.tcl` with stubs installed |
| `set_time ms` | set `StimTimeF` (and `StimTime`) |
| `step ?-dt 0.016?` | advance one frame: run prescripts, then this-frame scripts; return `{t writes events}` |
| `play -dur s -dt dt` | loop `step`; return list of per-frame dicts |
| `captured cmd ?target?` | matching records (`{t cmd target args}`) |
| `last cmd ?target?` | most recent matching record |
| `series cmd target ?argidx?` | `{t val}` time series of one arg |
| `values cmd target ?argidx?` | just the values |
| `events ?name?` | event records (`{t name payload}`) |
| `event_time name` | `StimTimeF` when a named event first fired |
| `clear_captures` | drop the capture/event log (keep object maps + prescripts) |
| `reset_stim` | full reset of stim-harness state |

**Frame model:** `stub_stim2` records `addPreScript` registrations (the pre-flip
per-frame drivers) and `addThisFrameScript` queues (one-shot post-flip pushes,
used for events). `step` advances `StimTimeF` by `dt`, runs every prescript in
registration order, then runs the this-frame scripts they queued. `resetObjList`
(called by `nexttrial`) clears prescripts and the object-name map, so each trial
re-registers cleanly.

### Assertions
| Command | Purpose |
|---|---|
| `assert {expr} msg` | eval `expr` in the caller's scope; print ok/FAIL; count |
| `approx a b ?tol?` / `in_range v lo hi` | numeric predicates |
| `test name {body}` | group assertions; a body error is itself a failure |
| `summary` | print tally; return 0 if all passed (use `exit [ess_test::summary]`) |
| `reset_counts` | zero the pass/fail tally |

---

## Coverage across systems

Two probes sweep the whole collection:

```sh
dlsh vfs/lib/ess_test/test_systems.tcl    # Tier-1: does every loader LOAD?
dlsh vfs/lib/ess_test/test_variants.tcl   # does every variant's loader RUN + produce a clean stimdg?
```

As of this writing: **all 22 systems load**, all **23 `*_stim.tcl` source** clean,
and **87/95 variants run and produce a well-formed `stimdg`** (0 ragged). Systems
whose libs live in `<systems_root>/lib` (`planko`, `haptic`, `blob`, …) load
because the harness puts that dir on the Tcl module path. The 8 non-running
variants are genuine: 2 are a **real loader bug** the sweep surfaced
(`hapticvis/transfer`'s `setup_visual_transfer`/`setup_haptic_transfer` call
`setup_trials` with 10 args, but it takes 13), 4 need a value computed at trial
time (`n_choices`), and 2 (`phd`) need a real grasp sqlite DB. Finding those is
the point — `dl_choose`/arg-count/`dynlist` errors in a sweep are exactly what
you want a dry test to flag.

A script is testable headless only if the packages it requires load headless
(pure-dlsh ones plus anything in `<systems_root>/lib`). The ESS-context shims
described under **Config** (ambient vars, harvested system/protocol
params+variables, `dservGet` seed, `my`, module path, system/protocol sourcing)
are what let real loaders run without the rig; a live dserv subscription in a
stim file (`qpcs::dsSet`/`dsGet`, `$::dservhost`) is kept headless by `stub_dserv`.

## Non-goals

No rendering (stubs capture intent, not pixels). No real vsync/timing (you drive
a synthetic `StimTimeF`). No hardware. No full state-machine run against the live
dserv event loop. It is a logic/data harness — pair it with real stim2 for the
appearance check.
