/*
 * runnerphys.c -- the Door Runner press-window solver in C
 *
 * The compiled twin of runner::window_for / ctx_sim / _fsim in
 * ~/systems/ess/lib/runner-1.0.tm (grant Aim 3). runner-1.0.tm calls these
 * when the package loads and falls back to its own Tcl otherwise; the two
 * must give bit-identical doubles (the Tcl is pinned to the browser
 * prototype door_runner.html, and stimdg windows are re-simulated offline).
 *
 * EXACTNESS: every expression below is the Tcl one, in the same order, on
 * IEEE doubles. Build with -ffp-contract=off and never -ffast-math / -Ofast
 * (see CMakeLists.txt): a fused multiply-add (the default on ARM gcc, and
 * within an expression on Apple clang) rounds once instead of twice and
 * moves the last bits, which can flip a borderline window edge. Do not
 * "simplify" an expression here without changing the Tcl and re-running
 * ess/runner/tests/test_runnerphys.tcl.
 *
 * The model knows nothing of runner dicts: ground is a flat list of solid
 * segments {x0 x1 h ...}, the avatar a box of half-width aw2 moving right
 * at a two-piece speed profile (v0, switching to v1 at tStep), with one
 * fixed ballistic jump (vy = j, vy -= gdt per step of dt).
 *
 *   runnerphys::window solid v0 tStep v1 dt aw2 j gdt floor x y t xGoal t0 t1
 *       the press window that clears the obstacle ahead, from a grounded
 *       start (x y t, on the solver's step grid), scanning press times
 *       t0, t0 + 0.02, ... <= t1. Returns {a b landXmax}, or {} if the
 *       obstacle can be walked or cannot be cleared.
 *       (runner::window_for; the caller derives c w pXmin.)
 *
 *   runnerphys::sim solid v0 tStep v1 dt aw2 j gdt floor x y t xGoal tPress
 *       one press at tPress from the same start: {ok survived landX}
 *       (runner::ctx_sim; landX {} when it did not survive).
 *
 * Author: DLS / Claude, Oct 2026
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>

#if defined(_WIN32)
#define EXPORT(a, b) __declspec(dllexport) a b
#else
#define EXPORT(a, b) a b
#endif

#ifndef TCL_SIZE_MAX
typedef int Tcl_Size;
#endif

#define MAX_STEPS 5000

/* one obstacle's context: runner::feature_ctx */
typedef struct {
  double *sol;   /* x0 x1 h triples */
  Tcl_Size nsol; /* number of triples */
  double v0, tStep, v1, dt, aw2, j, gdt, floor_, xGoal;
} Ctx;

/* state before a step: one entry of feature_ctx's snaps */
typedef struct {
  double x, y, vy, t, landX;
  int g, hasLand;
} Snap;

typedef struct {
  int ok, survived, hasLand;
  double landX;
} Res;

/*
 * runner::_fsim: from step k (state given), jumping at step pressK if
 * grounded there. With rec, stores the state before each step.
 */
static Res fsim(const Ctx *c, double x, double y, double vy, int g, double t,
                double landX, int hasLand, int jumped, int k, int pressK,
                Snap *rec, int *nrec) {
  Res fail = {0, 0, 0, 0.0};
  Tcl_Size i;
  for (; k < MAX_STEPS; k++) {
    if (rec) {
      Snap *s = &rec[(*nrec)++];
      s->x = x; s->y = y; s->vy = vy; s->g = g; s->t = t;
      s->landX = landX; s->hasLand = hasLand;
    }
    double nx = x + (t < c->tStep ? c->v0 : c->v1) * c->dt;
    double xa = nx - c->aw2;
    double xb = nx + c->aw2;
    double hN = -INFINITY;
    for (i = 0; i < c->nsol; i++) {
      double s0 = c->sol[3 * i], s1 = c->sol[3 * i + 1], h = c->sol[3 * i + 2];
      if (s1 > xa && s0 < xb && h > hN) hN = h;
    }
    if (hN > y + 1e-6) return fail;
    x = nx;
    t = t + c->dt;
    if (g) {
      if (k == pressK) {
        vy = c->j; g = 0; jumped = 1;
      } else if (hN < y - 1e-6) {
        g = 0; vy = 0.0;
      }
    }
    if (!g) {
      vy = vy - c->gdt;
      y = y + vy * c->dt;
      if (vy < 0 && y <= hN) {
        y = hN; g = 1; vy = 0.0; landX = x; hasLand = 1;
      }
      if (y < c->floor_) return fail;
    }
    if (x >= c->xGoal && g) {
      Res r;
      if (!hasLand) { landX = x; hasLand = 1; }
      r.ok = jumped; r.survived = 1; r.landX = landX; r.hasLand = 1;
      return r;
    }
  }
  return fail;
}

/* runner::ctx_sim: a press at tPress, restarting from its step's snapshot */
static Res ctx_sim(const Ctx *c, const Snap *S, int nS, Res walk,
                   double tPress) {
  /* first step whose state t satisfies t + dt >= tPress (t ascends) */
  int lo = 0, hi = nS;
  while (lo < hi) {
    int mid = (lo + hi) / 2;
    if (S[mid].t + c->dt >= tPress) hi = mid;
    else lo = mid + 1;
  }
  if (lo >= nS) return walk;
  const Snap *s = &S[lo];
  return fsim(c, s->x, s->y, s->vy, s->g, s->t, s->landX, s->hasLand, 0, lo,
              lo, NULL, NULL);
}

/* runner::refine: bisect from a failing press time tf toward a clearing one */
static double refine(const Ctx *c, const Snap *S, int nS, Res walk, double tf,
                     double to) {
  int it;
  for (it = 0; it < 7; it++) {
    double m = (tf + to) / 2.0;
    if (ctx_sim(c, S, nS, walk, m).ok) to = m;
    else tf = m;
  }
  return to;
}

/*
 * Parse the shared arguments: solid v0 tStep v1 dt aw2 j gdt floor x y t
 * xGoal (objv[1..13]) into c and the start state. c->sol is allocated.
 */
static int get_ctx(Tcl_Interp *interp, Tcl_Obj *const objv[], Ctx *c,
                   double *x, double *y, double *t) {
  Tcl_Size n, i;
  Tcl_Obj **elts;
  double *d[] = {&c->v0, &c->tStep, &c->v1, &c->dt, &c->aw2, &c->j,
                 &c->gdt, &c->floor_, x, y, t, &c->xGoal};
  if (Tcl_ListObjGetElements(interp, objv[1], &n, &elts) != TCL_OK)
    return TCL_ERROR;
  if (n % 3) {
    Tcl_SetObjResult(interp,
                     Tcl_NewStringObj("solid must be {x0 x1 h ...}", -1));
    return TCL_ERROR;
  }
  for (i = 0; i < 12; i++) {
    if (Tcl_GetDoubleFromObj(interp, objv[i + 2], d[i]) != TCL_OK)
      return TCL_ERROR;
  }
  c->nsol = n / 3;
  c->sol = (double *)ckalloc(sizeof(double) * (n ? n : 1));
  for (i = 0; i < n; i++) {
    if (Tcl_GetDoubleFromObj(interp, elts[i], &c->sol[i]) != TCL_OK) {
      ckfree((char *)c->sol);
      return TCL_ERROR;
    }
  }
  return TCL_OK;
}

/* runner::feature_ctx: the press-free walk, keeping the state before every
   step. S must hold MAX_STEPS entries. */
static Res walk_ctx(const Ctx *c, double x, double y, double t, Snap *S,
                    int *nS) {
  *nS = 0;
  return fsim(c, x, y, 0.0, 1, t, 0.0, 0, 0, 0, -1, S, nS);
}

static int WindowCmd(ClientData cd, Tcl_Interp *interp, int objc,
                     Tcl_Obj *const objv[]) {
  Ctx c;
  double x, y, t, t0, t1, tt;
  Snap *S;
  int nS, n, k, cur, b0 = -1, b1 = -1;
  double *ts;
  Res walk, *rs;

  if (objc != 16) {
    Tcl_WrongNumArgs(interp, 1, objv,
                     "solid v0 tStep v1 dt aw2 j gdt floor x y t xGoal t0 t1");
    return TCL_ERROR;
  }
  if (Tcl_GetDoubleFromObj(interp, objv[14], &t0) != TCL_OK ||
      Tcl_GetDoubleFromObj(interp, objv[15], &t1) != TCL_OK)
    return TCL_ERROR;
  if (get_ctx(interp, objv, &c, &x, &y, &t) != TCL_OK) return TCL_ERROR;

  S = (Snap *)ckalloc(sizeof(Snap) * MAX_STEPS);
  walk = walk_ctx(&c, x, y, t, S, &nS);

  /* an obstacle must need a jump: a narrow gap down to a lower level can
     be walked */
  if (walk.survived) goto empty;

  /* the press-time grid, accumulated exactly as the Tcl for loop */
  n = 0;
  for (tt = t0; tt <= t1; tt = tt + 0.02) n++;
  ts = (double *)ckalloc(sizeof(double) * (n ? n : 1));
  rs = (Res *)ckalloc(sizeof(Res) * (n ? n : 1));
  k = 0;
  for (tt = t0; tt <= t1; tt = tt + 0.02) {
    ts[k] = tt;
    rs[k] = ctx_sim(&c, S, nS, walk, tt);
    k++;
  }

  /* the longest run of clearing presses (the first, on ties) */
  cur = -1;
  for (k = 0; k < n; k++) {
    if (rs[k].ok) {
      if (cur < 0) cur = k;
      if (b0 < 0 || k - cur > b1 - b0) { b0 = cur; b1 = k; }
    } else {
      cur = -1;
    }
  }

  if (b0 >= 0) {
    double a, b, landXmax = -INFINITY;
    Res rb;
    Tcl_Obj *out[3];
    a = b0 > 0 ? refine(&c, S, nS, walk, ts[b0 - 1], ts[b0]) : ts[b0];
    b = b1 < n - 1 ? refine(&c, S, nS, walk, ts[b1 + 1], ts[b1]) : ts[b1];
    for (k = b0; k <= b1; k++) landXmax = fmax(landXmax, rs[k].landX);
    rb = ctx_sim(&c, S, nS, walk, b);
    if (rb.ok) landXmax = fmax(landXmax, rb.landX);
    out[0] = Tcl_NewDoubleObj(a);
    out[1] = Tcl_NewDoubleObj(b);
    out[2] = Tcl_NewDoubleObj(landXmax);
    Tcl_SetObjResult(interp, Tcl_NewListObj(3, out));
  }
  ckfree((char *)ts);
  ckfree((char *)rs);
  ckfree((char *)S);
  ckfree((char *)c.sol);
  return TCL_OK;

empty:
  ckfree((char *)S);
  ckfree((char *)c.sol);
  return TCL_OK;
}

static int SimCmd(ClientData cd, Tcl_Interp *interp, int objc,
                  Tcl_Obj *const objv[]) {
  Ctx c;
  double x, y, t, tPress;
  Snap *S;
  int nS;
  Res walk, r;
  Tcl_Obj *out[3];

  if (objc != 15) {
    Tcl_WrongNumArgs(interp, 1, objv,
                     "solid v0 tStep v1 dt aw2 j gdt floor x y t xGoal tPress");
    return TCL_ERROR;
  }
  if (Tcl_GetDoubleFromObj(interp, objv[14], &tPress) != TCL_OK)
    return TCL_ERROR;
  if (get_ctx(interp, objv, &c, &x, &y, &t) != TCL_OK) return TCL_ERROR;
  S = (Snap *)ckalloc(sizeof(Snap) * MAX_STEPS);
  walk = walk_ctx(&c, x, y, t, S, &nS);
  r = ctx_sim(&c, S, nS, walk, tPress);
  out[0] = Tcl_NewIntObj(r.ok);
  out[1] = Tcl_NewIntObj(r.survived);
  out[2] = r.survived ? Tcl_NewDoubleObj(r.landX) : Tcl_NewObj();
  Tcl_SetObjResult(interp, Tcl_NewListObj(3, out));
  ckfree((char *)S);
  ckfree((char *)c.sol);
  return TCL_OK;
}

#ifdef __cplusplus
extern "C" {
#endif
EXPORT(int, Runnerphys_Init)(Tcl_Interp *interp) {
  if (
#ifdef USE_TCL_STUBS
      Tcl_InitStubs(interp, "8.6-", 0)
#else
      Tcl_PkgRequire(interp, "Tcl", "8.6-", 0)
#endif
      == NULL) {
    return TCL_ERROR;
  }
  if (Tcl_PkgProvide(interp, "runnerphys", "1.0") != TCL_OK) return TCL_ERROR;
  Tcl_Eval(interp, "namespace eval ::runnerphys {}");
  Tcl_CreateObjCommand(interp, "::runnerphys::window",
                       (Tcl_ObjCmdProc *)WindowCmd, NULL, NULL);
  Tcl_CreateObjCommand(interp, "::runnerphys::sim", (Tcl_ObjCmdProc *)SimCmd,
                       NULL, NULL);
  return TCL_OK;
}
#ifdef __cplusplus
}
#endif
