# colorcal — design note & skeleton

Frontend-independent color/luminance calibration for the dlsh/stim2/ess
ecosystem. Constructs equiluminant (and, later, other cardinal-axis) colors,
runs/stores the observer or photometer calibration that pins them to a real
display, and lets experiments and loaders **carry calibrated colors between
systems** at runtime.

Status: **skeleton**. The pure-math core (display model + equiluminant
construction + gamut/limit math) is implemented and headless-testable. The
profile store, dserv/dgz transport, and the stim2 calibration frontend are
stubbed with signatures and TODOs.

---

## Why this needs a package (and can't be a helper)

We've now written the same isoluminant-color math three times (pursuit probe,
mp_postcue probe, mp_postcue meter patches) and hit its two footguns
(luminance-asymmetric naive red/green; chroma clipping silently breaking
equiluminance). More importantly, the *calibration* — which pins those colors
to a specific observer and display — needs to live somewhere durable and be
readable by stim, control, and analysis alike.

## The core reality: linear light + a display model are mandatory

Equiluminance is constructed by cancelling the R and G luminance changes:

    wR·Δlin_R + wG·Δlin_G = 0        (linear light only)

Two consequences drive the whole design:

1. **You must work in linear light.** The cancellation is only valid on linear
   channel values, so every construction linearizes (inverse EOTF), builds, and
   re-encodes. Doing it on gamma-encoded code values is simply wrong.

2. **The EOTF and the weights are display properties.** `sRGB EOTF` + `Rec709
   weights` (0.2126/0.7152/0.0722) describe a *nominal* display. A real
   uncalibrated monitor deviates in **both** its transfer curve and its primary
   luminances (so the true R↔G balance ratio `k = wR/wG` differs). The sRGB
   model therefore yields colors equiluminant *for the model*, off by a few
   percent *for the monitor*.

**Three calibration tiers — only two generalize without per-session testing:**

| tier | how | generalizes? |
|---|---|---|
| **measured** | photometer → real EOTF LUT + real primary luminances | ✅ physically correct for the standard observer |
| **hardware** | Display++ (or similar) in calibrated mode | ✅ known/linear pipeline |
| **model + observer null** | assume sRGB/Rec709, null residual by flicker (HFP) | ❌ "about right"; null absorbs display error + observer L:M variation at one operating point only |

So: **you cannot extrapolate a model-based construction to an uncharacterized
monitor and trust it.** colorcal makes the *display model* a first-class,
pluggable, measurable object, and treats the observer null as a residual on top.

## Architecture — two layers

- **`colorcal` (this file, `colorcal.tcl`)** — frontend-independent. Pure Tcl +
  dlsh. Display models, linearization, equiluminant construction, gamut/limit
  math, profile store, serialization. **No GL, no stim2 commands.** Headless-
  testable like `mp_sim`.
- **`colorcal_stim` (`colorcal_stim.tcl`)** — the stim2/graphics frontend:
  heterochromatic flicker photometry (HFP) harness, photometer verification
  patches, workspace integration. Depends on stim2; only sourced where a display
  exists. This is where mp_postcue's flicker/patch code gets generalized to.

## Core concepts / API (implemented)

**Display model** — EOTF + luminance weights, keyed by name.

    colorcal::display_define <name> -eotf srgb|gamma:<g>|lut:<dg> -weights {wr wg wb}
    colorcal::lin  <codeval> ?<display>?        ;# inverse EOTF: code -> linear
    colorcal::enc  <linear>  ?<display>?        ;# EOTF: linear -> code (clamped)
    colorcal::luminance <rgb> ?<display>?       ;# relative luminance Y

**Equiluminant construction** — red/green (L–M) cardinal axis for now.

    colorcal::isolum_pair <pedestal> <chroma> ?-display d? ?-balance b?
        -> {a {r g b}  b {r g b}}               ;# a=+axis(red), b=-axis(green)
    colorcal::max_chroma <pedestal> ?-display d? ?-balance b?
        -> largest chroma before a channel clips (clipping breaks equiluminance!)
    colorcal::in_gamut <linear_rgb> -> 0|1

`pedestal` and outputs are code values (0..1). `balance` is the observer-null
tilt (gray added to a / removed from b); its predicted luminance split is
`2·balance·lin(pedestal)`.

## Profiles & transport (stubbed)

A **profile** pins named colors to an (observer × display). It stores the
*generative spec* (pedestal, chroma, balance, display) rather than only final
triplets, so colors can be regenerated at any saturation on demand — and can
carry across systems.

    colorcal::profile_new -observer X -display Y            -> profile(dict)
    colorcal::profile_set  profileVar <name> <spec>
    colorcal::profile_rgb  <profile> <name> ?-saturation s? -> {r g b}
    colorcal::profile_to_dict / _from_dict                  ;# plain dict (dserv-friendly)
    colorcal::profile_save / _load  <file.dgz>              ;# TODO: dg serialization
    colorcal::publish / subscribe  ?-dpoint colorcal/active? ;# TODO: dserv live share

**Carrying colors between systems.** Recommended: a dserv datapoint holds the
*active* profile (one system calibrates → everyone subscribes/reads the same
colors at runtime); a `.dgz` is the persistence/portability layer. A loader that
needs isoluminant stimuli does:

    set p [colorcal::profile_load $path]         ;# or subscribe to dserv
    lassign [colorcal::profile_rgb $p probe_a] r g b

## Frontend: HFP + verification (stubbed → generalize from mp_postcue)

    colorcal::stim::hfp_run  <display> <pedestal> ?options?   -> balance
    colorcal::stim::patches  <profile|spec> ?options?         ;# static meter squares
    colorcal::stim::report   <spec>                           ;# print triplets + predY

Extract from `examples/motionpatch/mp_postcue.tcl`: the counterphase red/green
bar field (flicker null of `lum_balance`), the two big equiluminant meter
squares at a bright pedestal (note: meter patches use their own pedestal for
gamut headroom — see the clipping footgun), and the `print_triplets` reporter
(float / 8-bit / predY-vs-meter).

## Migration

1. Land `colorcal` core + `test_colorcal.tcl` (mirror `testmp_sim.tcl`).
2. Move the flicker/patch/report code out of mp_postcue into `colorcal_stim`.
3. Re-point mp_postcue at the package (first consumer). pursuit probe next.

## Open questions

- Canonical transport: dserv active-datapoint vs on-disk `.dgz` vs `stimdg`
  columns per trial (lean: dserv for active + dgz for storage).
- Profile keying / identity for (observer × display) and how loaders select one.
- Beyond red/green: S–(L+M) blue/yellow axis and arbitrary cone-contrast dirs.
- Measured-display ingestion: photometer LUT format + primary-luminance capture.
