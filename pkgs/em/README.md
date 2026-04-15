# em — eye movement event detection

Compiled primitives for saccade, post-saccadic oscillation (PSO),
pursuit, and fixation detection on calibrated eye-tracking data.
Designed for clean 250 Hz DPI (dual-Purkinje) recordings but
paradigm-agnostic.

## Package layout

    pkgs/em/
      CMakeLists.txt    build libem against tclstub + dlsh
      em.c              compiled primitives (em::c::* commands)
      README.md         this file

    vfs/lib/em/
      pkgIndex.tcl      loads libem then sources em.tcl
      em.tcl            high-level facade (em::extract_events, calibration)
      biquadratic.tcl   biquadratic calibration fitting + evaluation

The C extension installs to `vfs/lib/em/<OS>/<MACHINE>/libem<ext>` via
`install_tcl_package(NAME em TARGET em)` in `pkgs/CMakeLists.txt`.
`package require em` loads the C library and sources the Tcl facade
as a single unified package.

## Inputs

The facade `em::extract_events $g` expects these columns to already
exist on the dg (produced by `em::process_raw_streams` during file
conversion):

    em_h_deg     nested float   horizontal eye position in degrees
    em_v_deg     nested float   vertical eye position in degrees
    em_seconds   nested float   per-sample time in seconds from trial start
    in_blink     nested char    blink flag from the realtime tracker (optional)
    pupil_r      nested float   pupil radius, used for blink-mask extension (optional)

## Outputs

One call to `em::extract_events $g` adds these per-trial nested
columns to the dg:

    Saccade event lists
      sac_starts     int32    sample index of saccade onset
      sac_stops      int32    sample index of primary saccade end
      sac_t_start    float    onset time (s)
      sac_t_stop     float    primary end time (s)
      sac_amps       float    amplitude, from->post-PSO landing (deg)
      sac_dirs       float    direction, from->post-PSO landing (deg, atan2)
      sac_peakvels   float    peak speed within primary saccade (deg/s)
      sac_durs       float    primary saccade duration (s)
      sac_from_h     float    pre-saccade fixation position (deg)
      sac_from_v     float
      sac_to_h       float    post-PSO settled landing position (deg)
      sac_to_v       float
      sac_to_h_raw   float    pre-PSO landing position (for tuning/diagnosis)
      sac_to_v_raw   float

    PSO (post-saccadic oscillation) event lists
      pso_starts     int32
      pso_stops      int32
      pso_t_start    float
      pso_t_stop     float
      pso_peakvels   float    peak speed within PSO (deg/s)
      pso_amps       float    distance between pre-PSO raw and post-PSO settled landing

    Pursuit event lists
      pur_starts     int32
      pur_stops      int32
      pur_t_start    float
      pur_t_stop     float
      pur_durs       float
      pur_mean_vel   float    magnitude of mean velocity vector (deg/s)
      pur_mean_dir   float    direction of mean velocity vector (deg)
      pur_coherence  float    resultant length of unit velocity vectors (0..1)
      pur_dispersion float    spatial extent within the event (deg)

    Fixation event lists (derived from em_state run-length encoding)
      fix_starts     int32
      fix_stops      int32
      fix_t_start    float
      fix_t_stop     float
      fix_h          float    mean position over fixation (deg)
      fix_v          float
      fix_durs       float

    Per-sample classification
      em_state       char     0=fix 1=saccade 2=PSO 3=pursuit 4=blink

## Algorithms

### 1. Saccade + PSO — blob-then-split

Most video-based saccade detectors use a velocity threshold with a
runlength criterion and call it a day.  DPI recordings break that
approach because the crystalline lens continues to move inside the
globe after eye rotation stops — a real optical signal faithfully
reported by the tracker, but not eye rotation — producing
post-saccadic oscillations that are too large (up to several degrees)
and too fast (peaks of 100-1500+ deg/s) for a naive threshold to
separate from the saccade itself.

The two-phase algorithm handles this:

**Phase 1: find a movement blob.**  A blob opens when speed crosses
`vel_thresh` (default 30 deg/s) for at least `runlength` consecutive
samples.  It walks back to where speed first rose above
`vel_thresh_low` (default 10 deg/s), then extends forward until speed
has stayed below `vel_thresh_low` for `coast_dur` (default 12 ms).
The coast requirement is critical for DPI: without it, the wobble's
zero-crossings would prematurely close the blob mid-saccade.  Blobs
longer than `max_blob_dur` (300 ms) are rejected as non-saccades.

**Phase 2a: find the peak.**  Locate the sample of maximum speed
within the blob.

**Phase 2b: split primary saccade from PSO.**  Compute the saccade
direction from pre-saccade median position to post-blob median
position.  Starting at the peak, walk forward computing the signed
velocity along the saccade axis.  The primary saccade ends at the
first sample where signed velocity becomes non-positive — i.e. the
eye first starts moving *opposite* the saccade direction.  This
corresponds physically to the lens oscillation reversing, which is
when globe rotation has in fact completed.

**PSO duration cap.**  The coast-based blob end can extend far past
the true PSO when a saccade lands on a moving target (pursuit keeps
speed above `vel_thresh_low`).  The PSO tail is hard-capped at
`pso_max_dur` (default 60 ms, matching Deubel & Bridgeman's upper
bound for crystalline-lens settling).  Samples beyond the cap are
left available for the pursuit detector.

**Landing position estimates.**
- `sac_from_h/v` — median of 8 samples ending just before saccade onset.
- `sac_to_h_raw` / `sac_to_v_raw` — median of 3 samples just after
  primary saccade end.  This is the lens position at the moment globe
  rotation stopped; it is a DPI artifact, not the settled eye
  position.  Kept only for tuning/diagnosis.
- `sac_to_h` / `sac_to_v` — median of 8 samples starting
  `post_pso_delay` (10 ms) after PSO end.  This is the true eye
  landing position and is what you want for saccade amplitudes,
  landing accuracy, fixation targets, etc.

**ISI merging.**  Consecutive saccades whose inter-event gap (from
PSO end of the previous to onset of the next) is smaller than
`min_isi` (20 ms) are fused and their features recomputed over the
merged span.

**Amplitude filter.**  Saccades with final amplitude below `min_amp`
(0.2 deg) are rejected.

**Reference:** The overall strategy follows Nyström & Holmqvist
(2010) "An adaptive algorithm for fixation, saccade, and glissade
detection in eyetracking data," but with the signed-velocity
zero-crossing swapped in for their secondary velocity threshold so
it behaves cleanly on DPI data.  See Deubel & Bridgeman (1995)
"Fourth Purkinje image signals reveal eye-lens deviations and
retinal image distortions during saccades," *Vision Research*, and
Tabernero & Artal (2014) for the biomechanics of why DPI trackers
produce large PSOs in the first place.

### 2. Pursuit — sliding-window I-VVT

Paradigm-agnostic pursuit detection, operating only on samples the
saccade stage has marked as fixation (`em_state == FIX`).

A sliding window of `window_dur` (default 60 ms = 15 samples at 250
Hz) steps through the FIX regions sample-by-sample.  For each window
position, the algorithm requires the entire window *plus a smoothing
halo of ±smooth_samps* to be FIX — that ensures per-sample velocity
estimates near the window edges are not contaminated by adjacent
saccades.

For each window, compute:

- **Mean velocity vector:** per-sample velocities are estimated via a
  wide symmetric difference of half-width `smooth_samps` (default 3,
  giving a ±12 ms baseline at 250 Hz — effectively a boxcar-smoothed
  first derivative).  Averaged across the window.
- **Direction coherence R:** the resultant length of unit velocity
  vectors.  R = |mean(v/|v|)|, in [0, 1].  R near 1 means all sample
  velocities point the same direction.
- **Spatial dispersion:** the L2 diagonal of the window's position
  bounding box.
- **Max sample speed:** the largest single-sample speed in the
  window.

A window is classified as pursuit iff:

    min_speed      <= |mean_v| <= max_speed
    R              >= min_coherence
    min_dispersion <= dispersion <= max_dispersion
    max per-sample speed <= max_sample_speed

The max-dispersion and max-sample-speed checks reject windows that
contain an undetected saccade-like jump — important because
post-blink held-value recovery and other tracker artifacts can
otherwise slip through as "pursuit" at absurd dispersions (e.g., 42°
in 60 ms).

Per-window pursuit votes are ORed across overlapping positions.
Run-length encoding of the vote array yields candidate events, which
must each exceed `min_dur` (50 ms).

**Merge pass.**  Consecutive pursuit events separated by a gap of
`merge_gap` (default 60 ms) or less are fused if their mean direction
differs by at most `merge_angle_deg` (60°) *and* every sample in the
gap is currently FIX or PUR (never merging across a saccade, PSO, or
blink).  When merged, the bridging samples are re-labeled PUR and the
merged event's features are recomputed over the combined span.

**Reference:** Larsson, Nyström & Stridh (2013) "Detection of
saccades and postsaccadic oscillations in the presence of smooth
pursuit" describes the family of I-VVT / I-VMP algorithms this is
modeled on.  The particular feature set (velocity range + coherence +
dispersion + sample-speed sanity) was tuned on planko-bounce
DPI data.

### 3. Fixations

Once saccades, PSOs, pursuits, and blinks are labeled in `em_state`,
fixations fall out for free: a fixation event is any contiguous run
of samples still labeled `EM_STATE_FIX`.  Run-length encoding of the
state vector produces the `fix_*` event lists, with mean position
computed as the arithmetic mean of each run.

### 4. Blinks

The realtime tracker's `in_blink` column is read as a per-sample
boolean (supports `char`, `short`, `long`, or `float` storage).
Blink samples are treated as hard barriers throughout:

- Saccade detection cannot open a blob on a blink sample and cannot
  extend a blob across one.
- The classifier marks blink samples as `EM_STATE_BLINK` before any
  other labeling; saccade/PSO/pursuit passes cannot overwrite them.
- Pursuit windows require every sample (plus smoothing halo) to be
  FIX, so blinks naturally exclude their surrounding windows.

**Post-blink mask extension.**  Many trackers un-flag `in_blink`
before the pupil has fully re-stabilized — the signal then "holds"
its last-good position for several samples while `pupil_r` is still
ramping up, which can look like a giant instantaneous saccade at
trial start.  `em::c::extend_blink_mask` walks each `1->0` transition
in `in_blink` forward and keeps marking samples as blink until
`pupil_r` has plateaued (range over the trailing `extend_hold_samps`
window drops below `extend_stable_range`) or a `extend_max_samps` cap
is reached.  Enabled by default (`-extend_blink auto`) when `pupil_r`
is present on the dg.

## Tuning parameters

All parameters are overridable per call via a Tcl dict passed with
`-params`:

    em::extract_events $g -params [dict create pur_min_coherence 0.65 \
                                               pso_max_dur 0.080]

### Saccade + PSO

    vel_thresh             30.0   deg/s  blob entry threshold
    vel_thresh_low         10.0   deg/s  blob-coast threshold
    coast_dur              0.012  s      samples below vel_thresh_low
                                         required to close a blob
    runlength              2      n      samples >= vel_thresh to open a blob
    min_dur                0.006  s      minimum primary saccade duration
    min_isi                0.020  s      merge threshold
    min_amp                0.2    deg    minimum amplitude
    max_blob_dur           0.300  s      outer cap (blob is rejected beyond this)
    pso_max_dur            0.060  s      hard cap on PSO tail duration
    post_pso_delay         0.010  s      delay into stable window before sampling landing
    post_pso_samples       8      n      samples for landing medians

### Pursuit

    pur_window_dur         0.060  s      sliding window size
    pur_min_speed          1.0    deg/s  lower bound on mean window speed
    pur_max_speed          80.0   deg/s  upper bound on mean window speed
    pur_min_coherence      0.70   0..1   direction coherence threshold
    pur_min_dispersion     0.08   deg    lower bound on spatial extent
    pur_max_dispersion     8.0    deg    upper bound (reject saccade artefacts)
    pur_max_sample_speed   150.0  deg/s  reject window if any sample exceeds
    pur_min_dur            0.050  s      minimum event duration after RLE
    pur_smooth_samps       3      n      half-width of velocity smoothing
    pur_merge_gap          0.060  s      max gap to merge consecutive events
    pur_merge_angle_deg    60.0   deg    max direction difference to merge

### Blink extension

    extend_max_samps       25     n      upper bound on extension
    extend_hold_samps      3      n      stability window length
    extend_stable_range    1.0    float  pupil_r range to consider stable

## Tcl API

### em::extract_events

    em::extract_events g ?options?

Options:

    -params <dict>              parameter overrides (see tuning table above)
    -no_blink                   ignore in_blink even if present
    -extend_blink auto|on|off   post-blink mask extension (default auto)
    -save_extended_blink bool   keep the extended mask as g:in_blink_ext
                                (default false; otherwise ephemeral)
    -provenance bool            write <session>em/detection column with
                                parameters + version (default false, so
                                the dg stays strictly rectangular)

### C primitives (under em::c::*)

    em::c::version
    em::c::velocity h v t
                       -> nested float speed (deg/s), one sublist per trial

    em::c::detect_saccades h v t ?blink? ?options_dict?
                       -> dyngroup with sac_* and pso_* nested columns

    em::c::classify h v t blink sac_starts sac_stops pso_starts pso_stops ?options_dict?
                       -> dyngroup with em_state, pur_*, fix_* nested columns

    em::c::extend_blink_mask in_blink pupil_r ?options_dict?
                       -> nested char dl (extended mask)

These are the raw entry points; `em::extract_events` is usually the
only thing callers need.

## Integration

The extract pipeline calls `em::extract_events` automatically inside
each system's `<system>_extract.tcl` script, right after
`em::process_raw_streams` produces the calibrated columns:

    em::process_raw_streams $trials $em_streams -calibration $cal_coeffs
    if {[dl_exists $trials:em_h_deg]} {
        em::extract_events $trials
    }

Every trials dgz written by such a pipeline ships with all event
columns populated.  The `dg_viewer.html` tool in `dserv/www/`
recognizes the event columns and overlays saccade / PSO / pursuit
bands on the trace panel automatically (toggleable via the `events`
checkbox).

## References

- Deubel, H. & Bridgeman, B. (1995).  *Fourth Purkinje image signals
  reveal eye-lens deviations and retinal image distortions during
  saccades.*  Vision Research, 35(4), 529-538.
- Hooge, I., Holleman, G., Haukes, N., & Hessels, R. (2019).
  *Gaze tracking accuracy in humans: One eye is sometimes better
  than two.*  Behavior Research Methods, 51, 2712-2721.
- Larsson, L., Nyström, M., & Stridh, M. (2013).  *Detection of
  saccades and postsaccadic oscillations in the presence of smooth
  pursuit.*  IEEE Transactions on Biomedical Engineering, 60(9),
  2484-2493.
- Nyström, M. & Holmqvist, K. (2010).  *An adaptive algorithm for
  fixation, saccade, and glissade detection in eyetracking data.*
  Behavior Research Methods, 42(1), 188-204.
- Tabernero, J. & Artal, P. (2014).  *Lens oscillations in the human
  eye: implications for post-saccadic suppression of vision.*
  PLoS ONE, 9(4), e95764.
