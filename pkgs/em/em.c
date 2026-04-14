/*
 * NAME
 *      em.c
 *
 * PURPOSE
 *      Compiled event-detection primitives for the em (eye movement)
 *      package.  Exposes saccade, post-saccadic oscillation, pursuit,
 *      and fixation detection on nested DYN_LISTs of calibrated eye
 *      position (degrees) and time (seconds) per trial.
 *
 *      This file is the compiled half of the em package.  The high
 *      level Tcl facade lives in vfs/lib/em/em.tcl.
 *
 * AUTHOR
 *      DLS
 *
 * DATE
 *      APR-2026
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <tcl.h>
#include <df.h>
#include <dfana.h>
#include <tcl_dl.h>

#define EM_VERSION "1.0"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Per-sample em_state encoding (stored as DF_CHAR) */
#define EM_STATE_FIX   0
#define EM_STATE_SAC   1
#define EM_STATE_PSO   2
#define EM_STATE_PUR   3
#define EM_STATE_BLINK 4

/*****************************************************************************
 *                                                                           *
 *                              Options                                      *
 *                                                                           *
 *****************************************************************************/

typedef struct {
  double vel_thresh;       /* deg/s - enter-a-movement-blob threshold          */
  double vel_thresh_low;   /* deg/s - leave-a-movement-blob (coasted) threshold*/
  double coast_dur;        /* s     - speed must stay below vtl for this long  */
                           /*         to close a blob (absorbs wobble dips)    */
  double min_dur;          /* s     - minimum primary saccade duration         */
  double min_isi;          /* s     - minimum inter-saccade interval (merge)   */
  double min_amp;          /* deg   - minimum saccade amplitude                */
  double max_blob_dur;     /* s     - loose outer cap on saccade+PSO extent    */
  double pso_max_dur;      /* s     - hard cap on PSO duration (post prim end) */
  double post_pso_delay;   /* s     - delay into stable window before sampling */
  int    post_pso_samples; /* n     - samples to median for landing estimates  */
  int    runlength;        /* n     - consecutive samples above vel_thresh     */
} em_opts_t;

static void em_opts_defaults(em_opts_t *o)
{
  o->vel_thresh       = 30.0;   /* deg/s */
  o->vel_thresh_low   = 10.0;
  o->coast_dur        = 0.012;  /* 12 ms — absorbs brief wobble zero-crossings */
  o->min_dur          = 0.006;  /* 6 ms minimum primary saccade */
  o->min_isi          = 0.020;  /* 20 ms */
  o->min_amp          = 0.2;    /* deg */
  o->max_blob_dur     = 0.300;  /* outer cap on saccade+PSO extent            */
  o->pso_max_dur      = 0.060;  /* 60 ms — literature max for DPI lens wobble */
  o->post_pso_delay   = 0.010;
  o->post_pso_samples = 8;
  o->runlength        = 2;
}

static int get_double_from_dict(Tcl_Interp *interp, Tcl_Obj *dict,
                                const char *key, double *out)
{
  Tcl_Obj *k = Tcl_NewStringObj(key, -1);
  Tcl_Obj *v = NULL;
  Tcl_IncrRefCount(k);
  if (Tcl_DictObjGet(interp, dict, k, &v) != TCL_OK) {
    Tcl_DecrRefCount(k);
    return TCL_ERROR;
  }
  Tcl_DecrRefCount(k);
  if (v == NULL) return TCL_CONTINUE;  /* key absent */
  return Tcl_GetDoubleFromObj(interp, v, out);
}

static int get_int_from_dict(Tcl_Interp *interp, Tcl_Obj *dict,
                             const char *key, int *out)
{
  Tcl_Obj *k = Tcl_NewStringObj(key, -1);
  Tcl_Obj *v = NULL;
  Tcl_IncrRefCount(k);
  if (Tcl_DictObjGet(interp, dict, k, &v) != TCL_OK) {
    Tcl_DecrRefCount(k);
    return TCL_ERROR;
  }
  Tcl_DecrRefCount(k);
  if (v == NULL) return TCL_CONTINUE;
  return Tcl_GetIntFromObj(interp, v, out);
}

static int em_opts_from_dict(Tcl_Interp *interp, Tcl_Obj *dict, em_opts_t *o)
{
  int rc;
#define GETD(k, f) do {                                                 \
    rc = get_double_from_dict(interp, dict, k, &o->f);                  \
    if (rc == TCL_ERROR) return TCL_ERROR;                              \
  } while (0)
#define GETI(k, f) do {                                                 \
    rc = get_int_from_dict(interp, dict, k, &o->f);                     \
    if (rc == TCL_ERROR) return TCL_ERROR;                              \
  } while (0)

  GETD("vel_thresh",       vel_thresh);
  GETD("vel_thresh_low",   vel_thresh_low);
  GETD("coast_dur",        coast_dur);
  GETD("min_dur",          min_dur);
  GETD("min_isi",          min_isi);
  GETD("min_amp",          min_amp);
  GETD("max_blob_dur",     max_blob_dur);
  GETD("pso_max_dur",      pso_max_dur);
  GETD("post_pso_delay",   post_pso_delay);
  GETI("post_pso_samples", post_pso_samples);
  GETI("runlength",        runlength);

#undef GETD
#undef GETI
  return TCL_OK;
}

/*****************************************************************************
 *                                                                           *
 *                            Speed computation                              *
 *                                                                           *
 *****************************************************************************/

/*
 * Compute per-sample speed (deg/s) via central differences on h,v,t.
 * Endpoints use one-sided differences.  Output speed[] has length n.
 */
static void compute_speed(int n, const float *h, const float *v, const float *t,
                          float *speed)
{
  int i;
  if (n <= 0) return;
  if (n == 1) { speed[0] = 0.0f; return; }

  for (i = 1; i < n - 1; i++) {
    float dh = h[i+1] - h[i-1];
    float dv = v[i+1] - v[i-1];
    float dt = t[i+1] - t[i-1];
    if (dt <= 0) { speed[i] = 0.0f; continue; }
    speed[i] = sqrtf(dh*dh + dv*dv) / dt;
  }

  /* endpoints — one-sided */
  {
    float dh = h[1] - h[0];
    float dv = v[1] - v[0];
    float dt = t[1] - t[0];
    speed[0] = (dt > 0) ? sqrtf(dh*dh + dv*dv) / dt : 0.0f;
  }
  {
    float dh = h[n-1] - h[n-2];
    float dv = v[n-1] - v[n-2];
    float dt = t[n-1] - t[n-2];
    speed[n-1] = (dt > 0) ? sqrtf(dh*dh + dv*dv) / dt : 0.0f;
  }
}

/*****************************************************************************
 *                                                                           *
 *                       Saccade / PSO detection                             *
 *                                                                           *
 *****************************************************************************/

typedef struct {
  int    start_idx;     /* first sample above threshold (primary saccade on)   */
  int    stop_idx;      /* last sample above threshold   (primary saccade off) */
  int    pso_stop_idx;  /* end of post-saccadic oscillation (>= stop_idx)      */
  float  peak_vel;      /* deg/s over [start_idx, stop_idx]                    */
  float  pso_peak_vel;  /* deg/s over (stop_idx, pso_stop_idx]                 */
  float  from_h, from_v;         /* pre-saccade stable position (deg)          */
  float  to_h_raw, to_v_raw;     /* position at primary end (pre-PSO)          */
  float  to_h, to_v;             /* post-PSO stable position (deg)             */
  float  amp;                    /* from->to (post-PSO) amplitude (deg)        */
  float  dir;                    /* from->to direction (deg, atan2)            */
  int    has_pso;
} em_sac_t;

/*
 * Sample median of up to w values from src starting at idx, clamped to [0,n).
 * Returns 1 if at least one sample was available.
 */
static int window_median(const float *src, int n, int idx, int w, float *out)
{
  float buf[64];
  int i, count = 0;
  int end = idx + w;
  if (w > 64) w = 64;
  if (idx < 0) idx = 0;
  if (end > n) end = n;
  for (i = idx; i < end && count < 64; i++) buf[count++] = src[i];
  if (count == 0) return 0;
  /* simple insertion sort */
  for (i = 1; i < count; i++) {
    float x = buf[i];
    int j = i - 1;
    while (j >= 0 && buf[j] > x) { buf[j+1] = buf[j]; j--; }
    buf[j+1] = x;
  }
  *out = (count & 1) ? buf[count/2]
                     : 0.5f * (buf[count/2 - 1] + buf[count/2]);
  return 1;
}

static int window_median_back(const float *src, int n, int end_idx, int w, float *out)
{
  /* median of w samples ending at end_idx (inclusive) */
  int start = end_idx - w + 1;
  return window_median(src, n, start, w, out);
}

/*
 * Detect saccades and PSOs for a single trial.
 *
 * Two-phase algorithm:
 *   1. Find movement BLOBS via coasted velocity thresholding.  A blob
 *      starts when speed crosses vel_thresh and only closes when speed
 *      has stayed below vel_thresh_low for coast_dur.  This absorbs the
 *      brief velocity dips that occur during DPI lens oscillation
 *      (where the signal momentarily hits zero as it reverses).
 *   2. Inside each blob, split primary saccade from PSO by finding the
 *      first zero-crossing of SIGNED velocity along the saccade axis,
 *      after the speed peak.  That is the moment the eye first starts
 *      moving opposite the saccade direction — i.e., lens overshoot
 *      reversing — which marks the end of globe rotation.
 *
 * Returns number of events; *events_out is malloc'd (caller frees).
 */
static int detect_saccades_trial(int n,
                                 const float *h, const float *v, const float *t,
                                 const unsigned char *blink, /* 0/1, may be NULL */
                                 const em_opts_t *opts,
                                 const float *speed,
                                 em_sac_t **events_out)
{
  int cap = 16, count = 0;
  em_sac_t *ev = (em_sac_t *) malloc(cap * sizeof(em_sac_t));
  int i, k;

  float vt  = (float) opts->vel_thresh;
  float vtl = (float) opts->vel_thresh_low;

  /* estimate sampling period from first valid dt */
  float dt_est = 0.004f;
  if (n >= 2 && t[1] > t[0]) dt_est = t[1] - t[0];

  int min_run          = opts->runlength > 0 ? opts->runlength : 1;
  int min_dur_samps    = (int) ceilf((float)opts->min_dur / dt_est);
  if (min_dur_samps < 1) min_dur_samps = 1;
  int min_isi_samps    = (int) ceilf((float)opts->min_isi / dt_est);
  int coast_samps      = (int) ceilf((float)opts->coast_dur / dt_est);
  if (coast_samps < 1) coast_samps = 1;
  int max_blob_samps   = (int) ceilf((float)opts->max_blob_dur / dt_est);
  int pso_max_samps    = (int) ceilf((float)opts->pso_max_dur / dt_est);
  int post_delay_samps = (int) ceilf((float)opts->post_pso_delay / dt_est);

  i = 0;
  while (i < n) {
    int i_scan = i;   /* remember where we started; always advance past it */
    if (blink && blink[i]) { i++; continue; }

    /* ------ phase 1a: need runlength consecutive samples >= vt ------ */
    int run;
    if (speed[i] < vt) { i++; continue; }
    for (run = 0; run < min_run && i + run < n; run++) {
      if (blink && blink[i+run]) break;
      if (speed[i+run] < vt) break;
    }
    if (run < min_run) { i++; continue; }

    /* blob-start: walk back through samples with speed >= vtl */
    int blob_start = i;
    while (blob_start > 0 &&
           speed[blob_start-1] >= vtl &&
           !(blink && blink[blob_start-1])) blob_start--;

    /* ------ phase 1b: walk forward with coast-based closure ------ */
    int blob_end = blob_start;
    int quiet = 0;
    for (k = blob_start; k < n; k++) {
      if (blink && blink[k]) break;
      if (speed[k] >= vtl) {
        quiet = 0;
        blob_end = k;
      } else {
        quiet++;
        if (quiet >= coast_samps) break;
      }
      if (k - blob_start > max_blob_samps) break;
    }

    int blob_dur = blob_end - blob_start + 1;
    if (blob_dur > max_blob_samps) {
      /* not a saccade — skip past it */
      i = (blob_end + 1 > i_scan + 1) ? blob_end + 1 : i_scan + 1;
      continue;
    }

    /* ------ phase 2: landing windows and saccade direction ------ */
    float fh, fv, th, tv;
    if (!window_median_back(h, n, blob_start - 1, opts->post_pso_samples, &fh))
      fh = h[blob_start];
    if (!window_median_back(v, n, blob_start - 1, opts->post_pso_samples, &fv))
      fv = v[blob_start];
    if (!window_median(h, n, blob_end + 1 + post_delay_samps,
                       opts->post_pso_samples, &th))
      th = h[blob_end];
    if (!window_median(v, n, blob_end + 1 + post_delay_samps,
                       opts->post_pso_samples, &tv))
      tv = v[blob_end];

    float dh = th - fh;
    float dv = tv - fv;
    float amp_norm = sqrtf(dh*dh + dv*dv);
    if (amp_norm < (float) opts->min_amp) {
      i = (blob_end + 1 > i_scan + 1) ? blob_end + 1 : i_scan + 1;
      continue;
    }

    float ux = (amp_norm > 1e-6f) ? dh / amp_norm : 1.0f;
    float uy = (amp_norm > 1e-6f) ? dv / amp_norm : 0.0f;

    /* ------ phase 2a: peak sample within blob ------ */
    int peak_idx = blob_start;
    for (k = blob_start; k <= blob_end; k++)
      if (speed[k] > speed[peak_idx]) peak_idx = k;

    /* ------ phase 2b: find primary end by signed-velocity zero crossing.
       Compute signed velocity = (dh/dt, dv/dt) . (ux, uy) at each sample
       from peak forward.  The primary saccade ends at the first sample
       where signed_v becomes <= 0 (eye is now moving opposite the saccade
       direction — lens reversal). ------ */
    int prim_end = blob_end;
    for (k = peak_idx; k < blob_end; k++) {
      int kp = (k + 1 < n) ? k + 1 : k;
      int km = (k - 1 >= 0) ? k - 1 : k;
      float dtk = t[kp] - t[km];
      if (dtk <= 0) continue;
      float vxk = (h[kp] - h[km]) / dtk;
      float vyk = (v[kp] - v[km]) / dtk;
      float sv  = vxk * ux + vyk * uy;
      if (sv <= 0.0f) { prim_end = k; break; }
    }

    /* Primary peak velocity over [blob_start, prim_end] */
    float prim_peak = 0.0f;
    for (k = blob_start; k <= prim_end; k++)
      if (speed[k] > prim_peak) prim_peak = speed[k];

    /* ------ hard cap on PSO duration. ------
       The coast-based blob_end can extend far past the real PSO when a
       saccade lands on a moving target (pursuit keeps speed above
       vel_thresh_low).  Clamp the PSO tail so pursuit isn't absorbed. */
    int pso_end_capped = blob_end;
    if (pso_end_capped > prim_end + pso_max_samps) {
      pso_end_capped = prim_end + pso_max_samps;
    }
    if (pso_end_capped >= n) pso_end_capped = n - 1;

    /* PSO occupies (prim_end, pso_end_capped] if non-empty */
    int has_pso = (pso_end_capped > prim_end) ? 1 : 0;
    float pso_peak = 0.0f;
    if (has_pso) {
      for (k = prim_end + 1; k <= pso_end_capped; k++)
        if (speed[k] > pso_peak) pso_peak = speed[k];
    }
    blob_end = pso_end_capped;

    int prim_dur = prim_end - blob_start + 1;
    if (prim_dur < min_dur_samps) {
      i = (blob_end + 1 > i_scan + 1) ? blob_end + 1 : i_scan + 1;
      continue;
    }

    /* "raw" pre-PSO landing: small median just after prim_end */
    float trh, trv;
    if (!window_median(h, n, prim_end + 1, 3, &trh)) trh = h[prim_end];
    if (!window_median(v, n, prim_end + 1, 3, &trv)) trv = v[prim_end];

    em_sac_t e;
    memset(&e, 0, sizeof(e));
    e.start_idx    = blob_start;
    e.stop_idx     = prim_end;
    e.pso_stop_idx = blob_end;
    e.peak_vel     = prim_peak;
    e.pso_peak_vel = pso_peak;
    e.has_pso      = has_pso;
    e.from_h = fh;    e.from_v = fv;
    e.to_h   = th;    e.to_v   = tv;
    e.to_h_raw = trh; e.to_v_raw = trv;
    e.amp    = amp_norm;
    e.dir    = (float) (atan2f(dv, dh) * 180.0 / M_PI);

    /* min-ISI merge with previous event */
    if (count > 0) {
      em_sac_t *prev = &ev[count-1];
      int gap = blob_start - prev->pso_stop_idx;
      if (gap < min_isi_samps) {
        if (e.peak_vel > prev->peak_vel) prev->peak_vel = e.peak_vel;
        if (e.pso_peak_vel > prev->pso_peak_vel) prev->pso_peak_vel = e.pso_peak_vel;
        prev->stop_idx     = e.stop_idx;
        prev->pso_stop_idx = e.pso_stop_idx;
        prev->has_pso      = prev->has_pso || e.has_pso;
        prev->to_h_raw     = e.to_h_raw;
        prev->to_v_raw     = e.to_v_raw;
        prev->to_h         = e.to_h;
        prev->to_v         = e.to_v;
        {
          float ddh = prev->to_h - prev->from_h;
          float ddv = prev->to_v - prev->from_v;
          prev->amp = sqrtf(ddh*ddh + ddv*ddv);
          prev->dir = (float)(atan2f(ddv, ddh) * 180.0 / M_PI);
        }
        i = (blob_end + 1 > i_scan + 1) ? blob_end + 1 : i_scan + 1;
        continue;
      }
    }

    if (count >= cap) {
      cap *= 2;
      ev = (em_sac_t *) realloc(ev, cap * sizeof(em_sac_t));
    }
    ev[count++] = e;

    i = (blob_end + 1 > i_scan + 1) ? blob_end + 1 : i_scan + 1;
  }

  *events_out = ev;
  return count;
}

/*****************************************************************************
 *                                                                           *
 *                           Pursuit detection                               *
 *                                                                           *
 *  Paradigm-agnostic I-VVT: slide a window over samples that are currently  *
 *  classified as fixation (em_state == FIX).  Classify window as pursuit if *
 *  all criteria are met:                                                    *
 *    - mean speed in [min_speed, max_speed]                                 *
 *    - direction coherence R >= min_coherence                               *
 *    - spatial dispersion >= min_dispersion                                 *
 *  Then run-length encode pursuit-marked samples and enforce min_dur.       *
 *                                                                           *
 *****************************************************************************/

typedef struct {
  double window_dur;       /* s     — sliding window size          */
  double min_speed;        /* deg/s — window mean speed must exceed */
  double max_speed;        /* deg/s — window mean speed must not exceed */
  double min_coherence;    /* 0..1  — resultant length R threshold  */
  double min_dispersion;   /* deg   — spatial extent threshold      */
  double max_dispersion;   /* deg   — upper bound on spatial extent
                              (reject undetected-saccade artefacts)  */
  double max_sample_speed; /* deg/s — reject window if any single
                              sample exceeds this                    */
  double min_dur;          /* s     — minimum pursuit event duration*/
  double merge_gap;        /* s     — merge consecutive events if
                              temporal gap is <= this                */
  double merge_angle_deg;  /* deg   — merge only if mean-direction
                              difference is <= this                  */
  int    smooth_samps;     /* half-width of symmetric difference for
                              velocity estimation (0 = 2-sample central,
                              3 = ±12 ms @ 250 Hz, good for pursuit)   */
} em_pursuit_opts_t;

static void em_pursuit_opts_defaults(em_pursuit_opts_t *o)
{
  o->window_dur       = 0.060;
  o->min_speed        = 1.0;     /* catch slow pursuits (planko scene) */
  o->max_speed        = 80.0;
  o->min_coherence    = 0.70;
  o->min_dispersion   = 0.08;    /* slower pursuits cover less ground  */
  o->max_dispersion   = 8.0;
  o->max_sample_speed = 150.0;
  o->min_dur          = 0.050;
  o->merge_gap        = 0.060;   /* merge events within 60 ms          */
  o->merge_angle_deg  = 60.0;    /* only if mean direction agrees      */
  o->smooth_samps     = 3;
}

/*
 * Wide symmetric difference: velocity at idx computed as
 *   (h[idx+w] - h[idx-w]) / (t[idx+w] - t[idx-w])
 * where w = smooth_samps.  Effectively a boxcar-smoothed first derivative.
 * Clamps at edges.  Returns 1 if valid, 0 if dt <= 0 or no span available.
 */
static int smoothed_velocity(int n, const float *h, const float *v,
                             const float *t, int idx, int w,
                             float *vx_out, float *vy_out)
{
  if (w < 1) w = 1;
  int lo = idx - w;
  int hi = idx + w;
  if (lo < 0)  lo = 0;
  if (hi >= n) hi = n - 1;
  if (hi <= lo) return 0;
  float dt = t[hi] - t[lo];
  if (dt <= 0) return 0;
  *vx_out = (h[hi] - h[lo]) / dt;
  *vy_out = (v[hi] - v[lo]) / dt;
  return 1;
}

static int em_pursuit_opts_from_dict(Tcl_Interp *interp, Tcl_Obj *dict,
                                     em_pursuit_opts_t *o)
{
  int rc;
#define PGETD(k, f) do {                                        \
    rc = get_double_from_dict(interp, dict, k, &o->f);          \
    if (rc == TCL_ERROR) return TCL_ERROR;                      \
  } while (0)
  PGETD("pur_window_dur",       window_dur);
  PGETD("pur_min_speed",        min_speed);
  PGETD("pur_max_speed",        max_speed);
  PGETD("pur_min_coherence",    min_coherence);
  PGETD("pur_min_dispersion",   min_dispersion);
  PGETD("pur_max_dispersion",   max_dispersion);
  PGETD("pur_max_sample_speed", max_sample_speed);
  PGETD("pur_min_dur",          min_dur);
  PGETD("pur_merge_gap",        merge_gap);
  PGETD("pur_merge_angle_deg",  merge_angle_deg);
#undef PGETD
  int rci = get_int_from_dict(interp, dict, "pur_smooth_samps", &o->smooth_samps);
  if (rci == TCL_ERROR) return TCL_ERROR;
  return TCL_OK;
}

typedef struct {
  int   start_idx;
  int   stop_idx;
  float mean_vel;
  float mean_dir;   /* deg */
  float coherence;  /* 0..1 */
  float dispersion; /* deg */
} em_pur_t;

/*
 * Detect pursuit events in a single trial.
 *
 * Inputs:
 *   n, h, v, t         — position and time arrays
 *   em_state           — per-sample state (may be modified: FIX -> PUR)
 *   opts               — pursuit detection options
 *
 * Output: newly malloc'd array of pursuit events (caller frees).
 */
static int detect_pursuit_trial(int n,
                                const float *h, const float *v, const float *t,
                                unsigned char *em_state,
                                const em_pursuit_opts_t *opts,
                                em_pur_t **events_out)
{
  int cap = 8, count = 0;
  em_pur_t *ev = (em_pur_t *) malloc(cap * sizeof(em_pur_t));
  int i, k;

  float dt_est = 0.004f;
  if (n >= 2 && t[1] > t[0]) dt_est = t[1] - t[0];

  int win_samps = (int) ceilf((float) opts->window_dur / dt_est);
  if (win_samps < 3) win_samps = 3;
  int min_dur_samps = (int) ceilf((float) opts->min_dur / dt_est);
  if (min_dur_samps < 1) min_dur_samps = 1;

  /* Per-sample pursuit vote (0 = not pursuit, 1 = pursuit) */
  unsigned char *vote = (unsigned char *) calloc(n > 0 ? n : 1, 1);

  /* Slide a window; classify each window and mark its samples.
     Require the window PLUS a smoothing halo of ±smooth_samps to all be
     FIX so the derivative estimate at every window sample is clean. */
  int halo = opts->smooth_samps > 0 ? opts->smooth_samps : 0;
  for (i = 0; i + win_samps <= n; i++) {
    int lo = i - halo;
    int hi = i + win_samps - 1 + halo;
    if (lo < 0 || hi >= n) continue;
    int ok = 1;
    int first_bad = -1;
    for (k = lo; k <= hi; k++) {
      if (em_state[k] != EM_STATE_FIX) { ok = 0; first_bad = k; break; }
    }
    if (!ok) {
      /* skip past the first contaminating non-FIX sample */
      int next = first_bad - halo;
      if (next <= i) next = i + 1;
      i = next - 1;   /* -1 because the for loop will increment */
      continue;
    }

    /* Compute per-sample velocity via central differences (already done
       once in compute_speed but we need signed components here). */
    double sum_vx = 0, sum_vy = 0;
    double sum_ux = 0, sum_uy = 0;
    float h_min = h[i], h_max = h[i], v_min = v[i], v_max = v[i];
    int valid = 0;

    int sample_speed_ok = 1;
    for (k = 0; k < win_samps; k++) {
      int idx = i + k;
      float vxk, vyk;
      if (!smoothed_velocity(n, h, v, t, idx, opts->smooth_samps, &vxk, &vyk))
        continue;
      float sp = sqrtf(vxk*vxk + vyk*vyk);
      if (sp > opts->max_sample_speed) { sample_speed_ok = 0; break; }
      sum_vx += vxk;
      sum_vy += vyk;
      if (sp > 1e-6f) {
        sum_ux += vxk / sp;
        sum_uy += vyk / sp;
      }
      if (h[idx] < h_min) h_min = h[idx];
      if (h[idx] > h_max) h_max = h[idx];
      if (v[idx] < v_min) v_min = v[idx];
      if (v[idx] > v_max) v_max = v[idx];
      valid++;
    }
    if (!sample_speed_ok || valid == 0) continue;

    double mean_vx = sum_vx / valid;
    double mean_vy = sum_vy / valid;
    double mean_speed = sqrt(mean_vx*mean_vx + mean_vy*mean_vy);
    double R = sqrt(sum_ux*sum_ux + sum_uy*sum_uy) / valid;

    float dh = h_max - h_min;
    float dv = v_max - v_min;
    double disp = sqrt(dh*dh + dv*dv);

    if (mean_speed >= opts->min_speed &&
        mean_speed <= opts->max_speed &&
        R           >= opts->min_coherence &&
        disp        >= opts->min_dispersion &&
        disp        <= opts->max_dispersion) {
      for (k = 0; k < win_samps; k++) vote[i+k] = 1;
    }
  }

  /* Run-length encode pursuit votes into events, applying min_dur filter */
  i = 0;
  while (i < n) {
    if (!vote[i]) { i++; continue; }
    int pstart = i;
    while (i < n && vote[i]) i++;
    int pstop = i - 1;
    if (pstop - pstart + 1 < min_dur_samps) continue;

    /* Recompute per-event features over the (potentially larger) span */
    double sum_vx = 0, sum_vy = 0;
    double sum_ux = 0, sum_uy = 0;
    float h_min = h[pstart], h_max = h[pstart];
    float v_min = v[pstart], v_max = v[pstart];
    int valid = 0;
    for (k = pstart; k <= pstop; k++) {
      float vxk, vyk;
      if (!smoothed_velocity(n, h, v, t, k, opts->smooth_samps, &vxk, &vyk))
        continue;
      sum_vx += vxk; sum_vy += vyk;
      float sp = sqrtf(vxk*vxk + vyk*vyk);
      if (sp > 1e-6f) { sum_ux += vxk/sp; sum_uy += vyk/sp; }
      if (h[k] < h_min) h_min = h[k];
      if (h[k] > h_max) h_max = h[k];
      if (v[k] < v_min) v_min = v[k];
      if (v[k] > v_max) v_max = v[k];
      valid++;
    }
    if (valid == 0) continue;

    double mvx = sum_vx / valid, mvy = sum_vy / valid;
    double msp = sqrt(mvx*mvx + mvy*mvy);
    double R   = sqrt(sum_ux*sum_ux + sum_uy*sum_uy) / valid;
    float  dhE = h_max - h_min, dvE = v_max - v_min;

    em_pur_t e;
    e.start_idx = pstart;
    e.stop_idx  = pstop;
    e.mean_vel  = (float) msp;
    e.mean_dir  = (float)(atan2(mvy, mvx) * 180.0 / M_PI);
    e.coherence = (float) R;
    e.dispersion = sqrtf(dhE*dhE + dvE*dvE);

    if (count >= cap) {
      cap *= 2;
      ev = (em_pur_t *) realloc(ev, cap * sizeof(em_pur_t));
    }
    ev[count++] = e;

    /* Mark pursuit in em_state */
    for (k = pstart; k <= pstop; k++) {
      if (em_state[k] == EM_STATE_FIX) em_state[k] = EM_STATE_PUR;
    }
  }

  free(vote);

  /* --------------- merge pass ---------------
   * Fuse consecutive pursuit events whose temporal gap (in samples)
   * is <= merge_gap_samps, provided their mean directions differ by
   * no more than merge_angle_deg.  Bridge the gap in em_state (so
   * samples in the gap become PUR) and recompute the merged event's
   * features over the combined span.
   */
  if (count >= 2 && opts->merge_gap > 0) {
    int merge_gap_samps = (int) ceilf((float) opts->merge_gap / dt_est);
    float max_ang = (float) opts->merge_angle_deg;

    int out = 0;
    for (int src = 1; src < count; src++) {
      em_pur_t *prev = &ev[out];
      em_pur_t *cur  = &ev[src];
      int gap = cur->start_idx - prev->stop_idx - 1;

      float dang = cur->mean_dir - prev->mean_dir;
      while (dang > 180.0f)  dang -= 360.0f;
      while (dang < -180.0f) dang += 360.0f;
      if (dang < 0) dang = -dang;

      /* Don't merge across non-FIX/non-PUR samples (saccade/PSO/blink
         sitting between the two events must not become pursuit). */
      int gap_ok = 1;
      for (k = prev->stop_idx + 1; k < cur->start_idx; k++) {
        unsigned char st = em_state[k];
        if (st != EM_STATE_FIX && st != EM_STATE_PUR) { gap_ok = 0; break; }
      }

      if (gap_ok && gap <= merge_gap_samps && dang <= max_ang) {
        /* bridge gap in em_state */
        for (k = prev->stop_idx + 1; k < cur->start_idx; k++) {
          em_state[k] = EM_STATE_PUR;
        }
        /* recompute merged features over [prev->start_idx, cur->stop_idx] */
        int a = prev->start_idx, b = cur->stop_idx;
        double sum_vx = 0, sum_vy = 0, sum_ux = 0, sum_uy = 0;
        float h_min = h[a], h_max = h[a], v_min = v[a], v_max = v[a];
        int valid = 0;
        for (k = a; k <= b; k++) {
          float vxk, vyk;
          if (!smoothed_velocity(n, h, v, t, k, opts->smooth_samps, &vxk, &vyk))
            continue;
          sum_vx += vxk; sum_vy += vyk;
          float sp = sqrtf(vxk*vxk + vyk*vyk);
          if (sp > 1e-6f) { sum_ux += vxk/sp; sum_uy += vyk/sp; }
          if (h[k] < h_min) h_min = h[k];
          if (h[k] > h_max) h_max = h[k];
          if (v[k] < v_min) v_min = v[k];
          if (v[k] > v_max) v_max = v[k];
          valid++;
        }
        if (valid > 0) {
          double mvx = sum_vx / valid, mvy = sum_vy / valid;
          prev->stop_idx  = b;
          prev->mean_vel  = (float) sqrt(mvx*mvx + mvy*mvy);
          prev->mean_dir  = (float)(atan2(mvy, mvx) * 180.0 / M_PI);
          prev->coherence = (float)(sqrt(sum_ux*sum_ux + sum_uy*sum_uy) / valid);
          float dhE = h_max - h_min, dvE = v_max - v_min;
          prev->dispersion = sqrtf(dhE*dhE + dvE*dvE);
        }
      } else {
        out++;
        if (out != src) ev[out] = ev[src];
      }
    }
    count = out + 1;
  }

  /* Mark all (possibly merged) events in em_state */
  for (int e = 0; e < count; e++) {
    for (k = ev[e].start_idx; k <= ev[e].stop_idx; k++) {
      if (em_state[k] == EM_STATE_FIX) em_state[k] = EM_STATE_PUR;
    }
  }

  *events_out = ev;
  return count;
}

/*****************************************************************************
 *                                                                           *
 *                        Tcl command: em::c::velocity                       *
 *                                                                           *
 *  em::c::velocity h_nested v_nested t_nested                               *
 *      -> nested dl of speed (deg/s), one sublist per trial                 *
 *                                                                           *
 *****************************************************************************/

static int em_velocity_cmd(ClientData data, Tcl_Interp *interp,
                           int argc, char *argv[])
{
  DYN_LIST *hall, *vall, *tall;
  DYN_LIST **hsub, **vsub, **tsub;
  DYN_LIST *result;
  int ntr, i;

  if (argc != 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " h v t", NULL);
    return TCL_ERROR;
  }
  if (tclFindDynList(interp, argv[1], &hall) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &vall) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[3], &tall) != TCL_OK) return TCL_ERROR;

  if (DYN_LIST_DATATYPE(hall) != DF_LIST ||
      DYN_LIST_DATATYPE(vall) != DF_LIST ||
      DYN_LIST_DATATYPE(tall) != DF_LIST) {
    Tcl_AppendResult(interp, argv[0],
                     ": expected nested dl (DF_LIST) for h, v, t", NULL);
    return TCL_ERROR;
  }

  ntr = DYN_LIST_N(hall);
  if (DYN_LIST_N(vall) != ntr || DYN_LIST_N(tall) != ntr) {
    Tcl_AppendResult(interp, argv[0], ": unequal trial counts", NULL);
    return TCL_ERROR;
  }

  hsub = (DYN_LIST **) DYN_LIST_VALS(hall);
  vsub = (DYN_LIST **) DYN_LIST_VALS(vall);
  tsub = (DYN_LIST **) DYN_LIST_VALS(tall);

  result = dfuCreateDynList(DF_LIST, ntr);

  for (i = 0; i < ntr; i++) {
    DYN_LIST *hl = hsub[i], *vl = vsub[i], *tl = tsub[i];
    int n;
    if (DYN_LIST_DATATYPE(hl) != DF_FLOAT ||
        DYN_LIST_DATATYPE(vl) != DF_FLOAT ||
        DYN_LIST_DATATYPE(tl) != DF_FLOAT) {
      dfuFreeDynList(result);
      Tcl_AppendResult(interp, argv[0], ": expected float sublists", NULL);
      return TCL_ERROR;
    }
    n = DYN_LIST_N(hl);
    if (DYN_LIST_N(vl) != n || DYN_LIST_N(tl) != n) {
      dfuFreeDynList(result);
      Tcl_AppendResult(interp, argv[0], ": unequal sublist lengths", NULL);
      return TCL_ERROR;
    }
    DYN_LIST *sp = dfuCreateDynList(DF_FLOAT, n > 0 ? n : 1);
    float *buf = (float *) calloc(n > 0 ? n : 1, sizeof(float));
    compute_speed(n,
                  (float *) DYN_LIST_VALS(hl),
                  (float *) DYN_LIST_VALS(vl),
                  (float *) DYN_LIST_VALS(tl),
                  buf);
    {
      int k;
      for (k = 0; k < n; k++) dfuAddDynListFloat(sp, buf[k]);
    }
    free(buf);
    dfuMoveDynListList(result, sp);
  }

  return tclPutList(interp, result);
}

/*****************************************************************************
 *                                                                           *
 *                    Tcl command: em::c::detect_saccades                    *
 *                                                                           *
 *  em::c::detect_saccades h v t ?blink? ?options_dict?                      *
 *      -> new dyn group with parallel nested columns                        *
 *                                                                           *
 *****************************************************************************/

static int em_detect_saccades_cmd(ClientData data, Tcl_Interp *interp,
                                  int objc, Tcl_Obj *const objv[])
{
  em_opts_t opts;
  DYN_LIST *hall, *vall, *tall, *ball = NULL;
  Tcl_Obj *opts_obj = NULL;
  DYN_GROUP *retgroup;
  int ntr, i;

  em_opts_defaults(&opts);

  if (objc < 4 || objc > 6) {
    Tcl_WrongNumArgs(interp, 1, objv, "h v t ?blink? ?options?");
    return TCL_ERROR;
  }

  if (tclFindDynList(interp, Tcl_GetString(objv[1]), &hall) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, Tcl_GetString(objv[2]), &vall) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, Tcl_GetString(objv[3]), &tall) != TCL_OK) return TCL_ERROR;

  if (objc >= 5) {
    const char *s = Tcl_GetString(objv[4]);
    if (s[0] != '\0') {
      if (tclFindDynList(interp, (char *) s, &ball) != TCL_OK) {
        /* treat non-list 5th arg as options dict */
        Tcl_ResetResult(interp);
        ball = NULL;
        opts_obj = objv[4];
      }
    }
  }
  if (objc == 6) opts_obj = objv[5];

  if (opts_obj) {
    if (em_opts_from_dict(interp, opts_obj, &opts) != TCL_OK) return TCL_ERROR;
  }

  if (DYN_LIST_DATATYPE(hall) != DF_LIST ||
      DYN_LIST_DATATYPE(vall) != DF_LIST ||
      DYN_LIST_DATATYPE(tall) != DF_LIST) {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
                     ": expected nested dl (DF_LIST) for h, v, t", NULL);
    return TCL_ERROR;
  }
  if (ball && DYN_LIST_DATATYPE(ball) != DF_LIST) {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
                     ": expected nested dl for blink", NULL);
    return TCL_ERROR;
  }

  ntr = DYN_LIST_N(hall);
  if (DYN_LIST_N(vall) != ntr || DYN_LIST_N(tall) != ntr ||
      (ball && DYN_LIST_N(ball) != ntr)) {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
                     ": unequal trial counts", NULL);
    return TCL_ERROR;
  }

  DYN_LIST **hsub = (DYN_LIST **) DYN_LIST_VALS(hall);
  DYN_LIST **vsub = (DYN_LIST **) DYN_LIST_VALS(vall);
  DYN_LIST **tsub = (DYN_LIST **) DYN_LIST_VALS(tall);
  DYN_LIST **bsub = ball ? (DYN_LIST **) DYN_LIST_VALS(ball) : NULL;

  /* Allocate output nested lists */
#define MKLIST(name) DYN_LIST *name = dfuCreateDynList(DF_LIST, ntr)
  MKLIST(sac_starts);   MKLIST(sac_stops);
  MKLIST(sac_t_start);  MKLIST(sac_t_stop);
  MKLIST(sac_amps);     MKLIST(sac_dirs);
  MKLIST(sac_peakvels); MKLIST(sac_durs);
  MKLIST(sac_from_h);   MKLIST(sac_from_v);
  MKLIST(sac_to_h);     MKLIST(sac_to_v);
  MKLIST(sac_to_h_raw); MKLIST(sac_to_v_raw);
  MKLIST(pso_starts);   MKLIST(pso_stops);
  MKLIST(pso_t_start);  MKLIST(pso_t_stop);
  MKLIST(pso_peakvels); MKLIST(pso_amps);
#undef MKLIST

  for (i = 0; i < ntr; i++) {
    DYN_LIST *hl = hsub[i], *vl = vsub[i], *tl = tsub[i];
    int n, nev, k;
    em_sac_t *evs = NULL;
    float *speed;
    unsigned char *bvals_buf = NULL;    /* built from DF_CHAR/LONG/SHORT */
    const unsigned char *bvals = NULL;

    if (DYN_LIST_DATATYPE(hl) != DF_FLOAT ||
        DYN_LIST_DATATYPE(vl) != DF_FLOAT ||
        DYN_LIST_DATATYPE(tl) != DF_FLOAT) {
      Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
                       ": expected float sublists", NULL);
      return TCL_ERROR;
    }
    n = DYN_LIST_N(hl);
    if (DYN_LIST_N(vl) != n || DYN_LIST_N(tl) != n) {
      Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
                       ": unequal sublist lengths", NULL);
      return TCL_ERROR;
    }
    if (bsub) {
      DYN_LIST *bl = bsub[i];
      if (DYN_LIST_N(bl) != n) {
        Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
                         ": blink length != signal length", NULL);
        return TCL_ERROR;
      }
      bvals_buf = (unsigned char *) calloc(n > 0 ? n : 1, sizeof(unsigned char));
      switch (DYN_LIST_DATATYPE(bl)) {
      case DF_CHAR: {
        unsigned char *s = (unsigned char *) DYN_LIST_VALS(bl);
        for (k = 0; k < n; k++) bvals_buf[k] = s[k] ? 1 : 0;
        break;
      }
      case DF_SHORT: {
        short *s = (short *) DYN_LIST_VALS(bl);
        for (k = 0; k < n; k++) bvals_buf[k] = s[k] ? 1 : 0;
        break;
      }
      case DF_LONG: {
        int *s = (int *) DYN_LIST_VALS(bl);
        for (k = 0; k < n; k++) bvals_buf[k] = s[k] ? 1 : 0;
        break;
      }
      case DF_FLOAT: {
        float *s = (float *) DYN_LIST_VALS(bl);
        for (k = 0; k < n; k++) bvals_buf[k] = (s[k] != 0.0f) ? 1 : 0;
        break;
      }
      default:
        free(bvals_buf);
        Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
                         ": blink sublist must be numeric", NULL);
        return TCL_ERROR;
      }
      bvals = bvals_buf;
    }

    speed = (float *) calloc(n > 0 ? n : 1, sizeof(float));
    compute_speed(n,
                  (float *) DYN_LIST_VALS(hl),
                  (float *) DYN_LIST_VALS(vl),
                  (float *) DYN_LIST_VALS(tl),
                  speed);

    nev = detect_saccades_trial(n,
                                (float *) DYN_LIST_VALS(hl),
                                (float *) DYN_LIST_VALS(vl),
                                (float *) DYN_LIST_VALS(tl),
                                bvals, &opts, speed, &evs);

    /* build per-trial sublists and append */
    DYN_LIST *ss  = dfuCreateDynList(DF_LONG,  nev > 0 ? nev : 1);
    DYN_LIST *sp  = dfuCreateDynList(DF_LONG,  nev > 0 ? nev : 1);
    DYN_LIST *sts = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *stp = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *sa  = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *sd  = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *spv = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *sdu = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *sfh = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *sfv = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *sth = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *stv = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *srh = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *srv = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *ps  = dfuCreateDynList(DF_LONG,  nev > 0 ? nev : 1);
    DYN_LIST *pp  = dfuCreateDynList(DF_LONG,  nev > 0 ? nev : 1);
    DYN_LIST *pts = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *ptp = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *ppv = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);
    DYN_LIST *pa  = dfuCreateDynList(DF_FLOAT, nev > 0 ? nev : 1);

    float *tvals = (float *) DYN_LIST_VALS(tl);

    for (k = 0; k < nev; k++) {
      em_sac_t *e = &evs[k];
      dfuAddDynListLong(ss, e->start_idx);
      dfuAddDynListLong(sp, e->stop_idx);
      dfuAddDynListFloat(sts, tvals[e->start_idx]);
      dfuAddDynListFloat(stp, tvals[e->stop_idx]);
      dfuAddDynListFloat(sa,  e->amp);
      dfuAddDynListFloat(sd,  e->dir);
      dfuAddDynListFloat(spv, e->peak_vel);
      dfuAddDynListFloat(sdu, (float)(tvals[e->stop_idx] - tvals[e->start_idx]));
      dfuAddDynListFloat(sfh, e->from_h);
      dfuAddDynListFloat(sfv, e->from_v);
      dfuAddDynListFloat(sth, e->to_h);
      dfuAddDynListFloat(stv, e->to_v);
      dfuAddDynListFloat(srh, e->to_h_raw);
      dfuAddDynListFloat(srv, e->to_v_raw);

      if (e->has_pso) {
        int pstart = e->stop_idx + 1;
        int pend   = e->pso_stop_idx;
        if (pstart > pend) pstart = pend;
        dfuAddDynListLong(ps, pstart);
        dfuAddDynListLong(pp, pend);
        dfuAddDynListFloat(pts, tvals[pstart]);
        dfuAddDynListFloat(ptp, tvals[pend]);
        dfuAddDynListFloat(ppv, e->pso_peak_vel);
        /* PSO "amplitude" = max excursion from pre-PSO position — simple proxy:
           distance from raw-landing to post-PSO-landing */
        {
          float dh = e->to_h - e->to_h_raw;
          float dv = e->to_v - e->to_v_raw;
          dfuAddDynListFloat(pa, sqrtf(dh*dh + dv*dv));
        }
      }
    }

    dfuMoveDynListList(sac_starts, ss);
    dfuMoveDynListList(sac_stops, sp);
    dfuMoveDynListList(sac_t_start, sts);
    dfuMoveDynListList(sac_t_stop, stp);
    dfuMoveDynListList(sac_amps, sa);
    dfuMoveDynListList(sac_dirs, sd);
    dfuMoveDynListList(sac_peakvels, spv);
    dfuMoveDynListList(sac_durs, sdu);
    dfuMoveDynListList(sac_from_h, sfh);
    dfuMoveDynListList(sac_from_v, sfv);
    dfuMoveDynListList(sac_to_h, sth);
    dfuMoveDynListList(sac_to_v, stv);
    dfuMoveDynListList(sac_to_h_raw, srh);
    dfuMoveDynListList(sac_to_v_raw, srv);
    dfuMoveDynListList(pso_starts, ps);
    dfuMoveDynListList(pso_stops,  pp);
    dfuMoveDynListList(pso_t_start, pts);
    dfuMoveDynListList(pso_t_stop,  ptp);
    dfuMoveDynListList(pso_peakvels, ppv);
    dfuMoveDynListList(pso_amps, pa);

    free(speed);
    free(evs);
    if (bvals_buf) free(bvals_buf);
  }

  retgroup = dfuCreateDynGroup(20);
  dfuAddDynGroupExistingList(retgroup, "sac_starts",   sac_starts);
  dfuAddDynGroupExistingList(retgroup, "sac_stops",    sac_stops);
  dfuAddDynGroupExistingList(retgroup, "sac_t_start",  sac_t_start);
  dfuAddDynGroupExistingList(retgroup, "sac_t_stop",   sac_t_stop);
  dfuAddDynGroupExistingList(retgroup, "sac_amps",     sac_amps);
  dfuAddDynGroupExistingList(retgroup, "sac_dirs",     sac_dirs);
  dfuAddDynGroupExistingList(retgroup, "sac_peakvels", sac_peakvels);
  dfuAddDynGroupExistingList(retgroup, "sac_durs",     sac_durs);
  dfuAddDynGroupExistingList(retgroup, "sac_from_h",   sac_from_h);
  dfuAddDynGroupExistingList(retgroup, "sac_from_v",   sac_from_v);
  dfuAddDynGroupExistingList(retgroup, "sac_to_h",     sac_to_h);
  dfuAddDynGroupExistingList(retgroup, "sac_to_v",     sac_to_v);
  dfuAddDynGroupExistingList(retgroup, "sac_to_h_raw", sac_to_h_raw);
  dfuAddDynGroupExistingList(retgroup, "sac_to_v_raw", sac_to_v_raw);
  dfuAddDynGroupExistingList(retgroup, "pso_starts",   pso_starts);
  dfuAddDynGroupExistingList(retgroup, "pso_stops",    pso_stops);
  dfuAddDynGroupExistingList(retgroup, "pso_t_start",  pso_t_start);
  dfuAddDynGroupExistingList(retgroup, "pso_t_stop",   pso_t_stop);
  dfuAddDynGroupExistingList(retgroup, "pso_peakvels", pso_peakvels);
  dfuAddDynGroupExistingList(retgroup, "pso_amps",     pso_amps);

  return tclPutGroup(interp, retgroup);
}

/*****************************************************************************
 *                                                                           *
 *                    Tcl command: em::c::classify                           *
 *                                                                           *
 *  em::c::classify h v t blink sac_starts sac_stops pso_starts pso_stops    *
 *                  ?options_dict?                                           *
 *                                                                           *
 *  Builds per-sample em_state from blink + saccade + PSO event lists,       *
 *  runs pursuit detection on the remaining FIX samples, then returns a     *
 *  dyngroup containing:                                                     *
 *      em_state   (nested char, one sublist per trial)                      *
 *      pur_starts / pur_stops                                               *
 *      pur_t_start / pur_t_stop                                             *
 *      pur_durs                                                             *
 *      pur_mean_vel / pur_mean_dir                                          *
 *      pur_coherence / pur_dispersion                                       *
 *      fix_starts / fix_stops / fix_t_start / fix_t_stop                    *
 *      fix_h / fix_v / fix_durs                                             *
 *                                                                           *
 *****************************************************************************/

static int em_classify_cmd(ClientData data, Tcl_Interp *interp,
                           int objc, Tcl_Obj *const objv[])
{
  em_pursuit_opts_t popts;
  DYN_LIST *hall, *vall, *tall, *ball;
  DYN_LIST *ssall, *spall, *psall, *ppall;
  Tcl_Obj *opts_obj = NULL;
  int ntr, i, k;
  int bok = 0;

  em_pursuit_opts_defaults(&popts);

  if (objc < 9 || objc > 10) {
    Tcl_WrongNumArgs(interp, 1, objv,
      "h v t blink sac_starts sac_stops pso_starts pso_stops ?options?");
    return TCL_ERROR;
  }
  if (tclFindDynList(interp, Tcl_GetString(objv[1]), &hall) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, Tcl_GetString(objv[2]), &vall) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, Tcl_GetString(objv[3]), &tall) != TCL_OK) return TCL_ERROR;

  const char *bs = Tcl_GetString(objv[4]);
  if (bs[0] != '\0') {
    if (tclFindDynList(interp, (char *) bs, &ball) == TCL_OK) bok = 1;
    else Tcl_ResetResult(interp);
  }
  if (tclFindDynList(interp, Tcl_GetString(objv[5]), &ssall) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, Tcl_GetString(objv[6]), &spall) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, Tcl_GetString(objv[7]), &psall) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, Tcl_GetString(objv[8]), &ppall) != TCL_OK) return TCL_ERROR;
  if (objc == 10) opts_obj = objv[9];
  if (opts_obj) {
    if (em_pursuit_opts_from_dict(interp, opts_obj, &popts) != TCL_OK)
      return TCL_ERROR;
  }

  if (DYN_LIST_DATATYPE(hall) != DF_LIST ||
      DYN_LIST_DATATYPE(vall) != DF_LIST ||
      DYN_LIST_DATATYPE(tall) != DF_LIST) {
    Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
                     ": expected nested dl for h,v,t", NULL);
    return TCL_ERROR;
  }
  ntr = DYN_LIST_N(hall);

  DYN_LIST **hsub = (DYN_LIST **) DYN_LIST_VALS(hall);
  DYN_LIST **vsub = (DYN_LIST **) DYN_LIST_VALS(vall);
  DYN_LIST **tsub = (DYN_LIST **) DYN_LIST_VALS(tall);
  DYN_LIST **bsub = bok ? (DYN_LIST **) DYN_LIST_VALS(ball) : NULL;
  DYN_LIST **ssub = (DYN_LIST **) DYN_LIST_VALS(ssall);
  DYN_LIST **psub = (DYN_LIST **) DYN_LIST_VALS(spall);
  DYN_LIST **qsub = (DYN_LIST **) DYN_LIST_VALS(psall);
  DYN_LIST **rsub = (DYN_LIST **) DYN_LIST_VALS(ppall);

#define MKL(x) DYN_LIST *x = dfuCreateDynList(DF_LIST, ntr)
  MKL(em_state);
  MKL(pur_starts);  MKL(pur_stops);
  MKL(pur_t_start); MKL(pur_t_stop);
  MKL(pur_durs);    MKL(pur_mean_vel);
  MKL(pur_mean_dir); MKL(pur_coherence); MKL(pur_dispersion);
  MKL(fix_starts);  MKL(fix_stops);
  MKL(fix_t_start); MKL(fix_t_stop);
  MKL(fix_h);       MKL(fix_v);   MKL(fix_durs);
#undef MKL

  for (i = 0; i < ntr; i++) {
    int n = DYN_LIST_N(hsub[i]);
    float *h = (float *) DYN_LIST_VALS(hsub[i]);
    float *v = (float *) DYN_LIST_VALS(vsub[i]);
    float *t = (float *) DYN_LIST_VALS(tsub[i]);

    unsigned char *state = (unsigned char *) calloc(n > 0 ? n : 1, 1);

    /* blink */
    if (bsub) {
      DYN_LIST *bl = bsub[i];
      switch (DYN_LIST_DATATYPE(bl)) {
      case DF_CHAR: {
        unsigned char *s = (unsigned char *) DYN_LIST_VALS(bl);
        for (k = 0; k < n; k++) if (s[k]) state[k] = EM_STATE_BLINK;
        break;
      }
      case DF_SHORT: {
        short *s = (short *) DYN_LIST_VALS(bl);
        for (k = 0; k < n; k++) if (s[k]) state[k] = EM_STATE_BLINK;
        break;
      }
      case DF_LONG: {
        int *s = (int *) DYN_LIST_VALS(bl);
        for (k = 0; k < n; k++) if (s[k]) state[k] = EM_STATE_BLINK;
        break;
      }
      case DF_FLOAT: {
        float *s = (float *) DYN_LIST_VALS(bl);
        for (k = 0; k < n; k++) if (s[k] != 0.0f) state[k] = EM_STATE_BLINK;
        break;
      }
      default: break;
      }
    }

    /* saccade spans [start..stop] */
    {
      int m = DYN_LIST_N(ssub[i]);
      int *sv = (int *) DYN_LIST_VALS(ssub[i]);
      int *ev = (int *) DYN_LIST_VALS(psub[i]);
      for (k = 0; k < m; k++) {
        int a = sv[k], b = ev[k];
        if (a < 0) a = 0;
        if (b >= n) b = n - 1;
        for (int q = a; q <= b; q++)
          if (state[q] != EM_STATE_BLINK) state[q] = EM_STATE_SAC;
      }
    }

    /* PSO spans [start..stop] */
    {
      int m = DYN_LIST_N(qsub[i]);
      int *sv = (int *) DYN_LIST_VALS(qsub[i]);
      int *ev = (int *) DYN_LIST_VALS(rsub[i]);
      for (k = 0; k < m; k++) {
        int a = sv[k], b = ev[k];
        if (a < 0) a = 0;
        if (b >= n) b = n - 1;
        for (int q = a; q <= b; q++)
          if (state[q] == EM_STATE_FIX) state[q] = EM_STATE_PSO;
      }
    }

    /* Pursuit detection on remaining FIX */
    em_pur_t *pev = NULL;
    int npur = detect_pursuit_trial(n, h, v, t, state, &popts, &pev);

    /* Build pursuit output sublists */
    DYN_LIST *ps1 = dfuCreateDynList(DF_LONG,  npur > 0 ? npur : 1);
    DYN_LIST *ps2 = dfuCreateDynList(DF_LONG,  npur > 0 ? npur : 1);
    DYN_LIST *pt1 = dfuCreateDynList(DF_FLOAT, npur > 0 ? npur : 1);
    DYN_LIST *pt2 = dfuCreateDynList(DF_FLOAT, npur > 0 ? npur : 1);
    DYN_LIST *pdu = dfuCreateDynList(DF_FLOAT, npur > 0 ? npur : 1);
    DYN_LIST *pmv = dfuCreateDynList(DF_FLOAT, npur > 0 ? npur : 1);
    DYN_LIST *pmd = dfuCreateDynList(DF_FLOAT, npur > 0 ? npur : 1);
    DYN_LIST *pco = dfuCreateDynList(DF_FLOAT, npur > 0 ? npur : 1);
    DYN_LIST *pdi = dfuCreateDynList(DF_FLOAT, npur > 0 ? npur : 1);
    for (k = 0; k < npur; k++) {
      em_pur_t *e = &pev[k];
      dfuAddDynListLong(ps1, e->start_idx);
      dfuAddDynListLong(ps2, e->stop_idx);
      dfuAddDynListFloat(pt1, t[e->start_idx]);
      dfuAddDynListFloat(pt2, t[e->stop_idx]);
      dfuAddDynListFloat(pdu, t[e->stop_idx] - t[e->start_idx]);
      dfuAddDynListFloat(pmv, e->mean_vel);
      dfuAddDynListFloat(pmd, e->mean_dir);
      dfuAddDynListFloat(pco, e->coherence);
      dfuAddDynListFloat(pdi, e->dispersion);
    }
    dfuMoveDynListList(pur_starts,   ps1);
    dfuMoveDynListList(pur_stops,    ps2);
    dfuMoveDynListList(pur_t_start,  pt1);
    dfuMoveDynListList(pur_t_stop,   pt2);
    dfuMoveDynListList(pur_durs,     pdu);
    dfuMoveDynListList(pur_mean_vel, pmv);
    dfuMoveDynListList(pur_mean_dir, pmd);
    dfuMoveDynListList(pur_coherence,  pco);
    dfuMoveDynListList(pur_dispersion, pdi);
    free(pev);

    /* Fixation events = run-length encode EM_STATE_FIX runs */
    DYN_LIST *fs1 = dfuCreateDynList(DF_LONG,  8);
    DYN_LIST *fs2 = dfuCreateDynList(DF_LONG,  8);
    DYN_LIST *ft1 = dfuCreateDynList(DF_FLOAT, 8);
    DYN_LIST *ft2 = dfuCreateDynList(DF_FLOAT, 8);
    DYN_LIST *fhv = dfuCreateDynList(DF_FLOAT, 8);
    DYN_LIST *fvv = dfuCreateDynList(DF_FLOAT, 8);
    DYN_LIST *fdu = dfuCreateDynList(DF_FLOAT, 8);
    {
      int q = 0;
      while (q < n) {
        if (state[q] != EM_STATE_FIX) { q++; continue; }
        int a = q;
        while (q < n && state[q] == EM_STATE_FIX) q++;
        int b = q - 1;
        /* fixation median position */
        double sh = 0, sv = 0;
        int m = 0;
        for (int r = a; r <= b; r++) { sh += h[r]; sv += v[r]; m++; }
        float mh = m ? (float)(sh / m) : 0.0f;
        float mv = m ? (float)(sv / m) : 0.0f;
        dfuAddDynListLong(fs1, a);
        dfuAddDynListLong(fs2, b);
        dfuAddDynListFloat(ft1, t[a]);
        dfuAddDynListFloat(ft2, t[b]);
        dfuAddDynListFloat(fhv, mh);
        dfuAddDynListFloat(fvv, mv);
        dfuAddDynListFloat(fdu, t[b] - t[a]);
      }
    }
    dfuMoveDynListList(fix_starts,  fs1);
    dfuMoveDynListList(fix_stops,   fs2);
    dfuMoveDynListList(fix_t_start, ft1);
    dfuMoveDynListList(fix_t_stop,  ft2);
    dfuMoveDynListList(fix_h,       fhv);
    dfuMoveDynListList(fix_v,       fvv);
    dfuMoveDynListList(fix_durs,    fdu);

    /* Emit per-sample state as DF_CHAR */
    DYN_LIST *st = dfuCreateDynList(DF_CHAR, n > 0 ? n : 1);
    for (k = 0; k < n; k++) dfuAddDynListChar(st, state[k]);
    dfuMoveDynListList(em_state, st);

    free(state);
  }

  DYN_GROUP *retgroup = dfuCreateDynGroup(17);
  dfuAddDynGroupExistingList(retgroup, "em_state",       em_state);
  dfuAddDynGroupExistingList(retgroup, "pur_starts",     pur_starts);
  dfuAddDynGroupExistingList(retgroup, "pur_stops",      pur_stops);
  dfuAddDynGroupExistingList(retgroup, "pur_t_start",    pur_t_start);
  dfuAddDynGroupExistingList(retgroup, "pur_t_stop",     pur_t_stop);
  dfuAddDynGroupExistingList(retgroup, "pur_durs",       pur_durs);
  dfuAddDynGroupExistingList(retgroup, "pur_mean_vel",   pur_mean_vel);
  dfuAddDynGroupExistingList(retgroup, "pur_mean_dir",   pur_mean_dir);
  dfuAddDynGroupExistingList(retgroup, "pur_coherence",  pur_coherence);
  dfuAddDynGroupExistingList(retgroup, "pur_dispersion", pur_dispersion);
  dfuAddDynGroupExistingList(retgroup, "fix_starts",     fix_starts);
  dfuAddDynGroupExistingList(retgroup, "fix_stops",      fix_stops);
  dfuAddDynGroupExistingList(retgroup, "fix_t_start",    fix_t_start);
  dfuAddDynGroupExistingList(retgroup, "fix_t_stop",     fix_t_stop);
  dfuAddDynGroupExistingList(retgroup, "fix_h",          fix_h);
  dfuAddDynGroupExistingList(retgroup, "fix_v",          fix_v);
  dfuAddDynGroupExistingList(retgroup, "fix_durs",       fix_durs);
  return tclPutGroup(interp, retgroup);
}

/*****************************************************************************
 *                                                                           *
 *                                Em_Init                                    *
 *                                                                           *
 *****************************************************************************/
static int em_version_cmd(ClientData data, Tcl_Interp *interp,
                          int argc, char *argv[])
{
  Tcl_SetResult(interp, EM_VERSION, TCL_STATIC);
  return TCL_OK;
}

#ifdef WIN32
__declspec(dllexport) int Em_Init(Tcl_Interp *interp)
#else
int Em_Init(Tcl_Interp *interp)
#endif
{
  if (
#ifdef USE_TCL_STUBS
      Tcl_InitStubs(interp, "8.5-", 0)
#else
      Tcl_PkgRequire(interp, "Tcl", "8.5-", 0)
#endif
      == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_PkgRequire(interp, "dlsh", "1.2", 0) == NULL) {
    return TCL_ERROR;
  }

  Tcl_Eval(interp, "namespace eval em {}");
  Tcl_Eval(interp, "namespace eval em::c {}");

  Tcl_CreateCommand(interp, "em::c::version",
                    (Tcl_CmdProc *) em_version_cmd,
                    (ClientData) NULL,
                    (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateCommand(interp, "em::c::velocity",
                    (Tcl_CmdProc *) em_velocity_cmd,
                    (ClientData) NULL,
                    (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateObjCommand(interp, "em::c::detect_saccades",
                       em_detect_saccades_cmd,
                       (ClientData) NULL,
                       (Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateObjCommand(interp, "em::c::classify",
                       em_classify_cmd,
                       (ClientData) NULL,
                       (Tcl_CmdDeleteProc *) NULL);

  return TCL_OK;
}

#ifdef WIN32
__declspec(dllexport) int Em_SafeInit(Tcl_Interp *interp)
#else
int Em_SafeInit(Tcl_Interp *interp)
#endif
{
  return Em_Init(interp);
}
