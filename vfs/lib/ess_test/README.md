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
| `config ?-systems_root path?` | get/set the systems root (default `~/systems/ess`) |
| `script_path sys proto type` | resolve a file path (`type`: loaders/stim/variants/protocol/system) |

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
| `dg_summary g ?nrows?` | columns + lengths + sample rows |

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

`test_systems.tcl` runs Tier-1 `load_loaders` against **every**
`<sys>/<proto>/*_loaders.tcl` under the systems root and categorizes each:

```sh
dlsh vfs/lib/ess_test/test_systems.tcl
```

As of this writing, 17/22 systems load clean (loaders from 1–31 params), and 5
are flagged **DEP** — they `package require` a package not in `dlsh.zip`
(`planko`, `haptic`, `blob`). Those aren't harness failures; the script simply
can't be sourced headless until its dependency is available. All 23 `*_stim.tcl`
files **source** clean under `stub_stim2` (GL/motionpatch/dserv all stubbed).

A script is testable headless only if the pure-dlsh packages it requires load
headless (`launch_sim`, `mp_sim`, `sqlite3` do). Stim files that wire a live
dserv subscription at load (`qpcs::dsSet`/`dsGet`/`dsStimAddMatch`,
`$::dservhost`) are kept headless by `stub_dserv` (folded into `stub_stim2`):
reads return empty, writes go nowhere, nothing connects.

## Non-goals

No rendering (stubs capture intent, not pixels). No real vsync/timing (you drive
a synthetic `StimTimeF`). No hardware. No full state-machine run against the live
dserv event loop. It is a logic/data harness — pair it with real stim2 for the
appearance check.
