#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*
 * ANSI C code from the article
 * "Centroid of a Polygon"
 * by Gerard Bashein and Paul R. Detmer,
	(gb@locke.hs.washington.edu, pdetmer@u.washington.edu)
 * in "Graphics Gems IV", Academic Press, 1994
 */

/*********************************************************************
polyCentroid: Calculates the centroid (xCentroid, yCentroid) and area
of a polygon, given its vertices (x[0], y[0]) ... (x[n-1], y[n-1]). It
is assumed that the contour is closed, i.e., that the vertex following
(x[n-1], y[n-1]) is (x[0], y[0]).  The algebraic sign of the area is
positive for counterclockwise ordering of vertices in x-y plane;
otherwise negative.

Returned values:  0 for normal execution;  1 if the polygon is
degenerate (number of vertices < 3);  and 2 if area = 0 (and the
centroid is undefined).
**********************************************************************/
int polygon_centroid(float x[], float y[], int n,
		     float *xCentroid, float *yCentroid, float *area)
{
  register int i, j;
  double ai, atmp = 0, xtmp = 0, ytmp = 0;
  if (n < 3) return 1;

  for (i = n-1, j = 0; j < n; i = j, j++)
    {
      ai = x[i] * y[j] - x[j] * y[i];
      atmp += ai;
      xtmp += (x[j] + x[i]) * ai;
      ytmp += (y[j] + y[i]) * ai;
    }
  *area = (float) (atmp / 2);
  if (atmp != 0)
    {
      *xCentroid =	(float) (xtmp / (3 * atmp));
      *yCentroid =	(float) (ytmp / (3 * atmp));
      return 0;
    }
  return 2;
}

/*
 * Compute distance from a point to a segment.
 */
float dist_segment(float x, float y, float x1, float y1, 
		   float x2, float y2) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  float long_segment = (float) sqrt(dx*dx + dy*dy);
  float ddx, ddy;
  float unitx, unity, vx, vy, long_proy, proyx, proyy;

  if (long_segment == 0) { 
    ddx = x - x1;
    ddy = y - y1; 
    return (float) sqrt(ddx*ddx + ddy*ddy); 
  }
  
  unitx = dx/long_segment;
  unity = dy/long_segment;
  vx = x - x1;
  vy = y - y1;
  long_proy = vx*unitx + vy*unity;
  proyx = x1 + long_proy*unitx;
  proyy = y1 + long_proy*unity;
  
  if (long_proy > long_segment) { 
    ddx = x - x2;
    ddy = y - y2; 
    return (float) sqrt(ddx*ddx+ddy*ddy); 
  }
  else if (long_proy < 0) { 
    ddx = x - x1; 
    ddy = y - y1; 
    return (float) sqrt(ddx*ddx+ddy*ddy); 
  }
  ddx = x - proyx;
  ddy = y - proyy;
  return (float) sqrt(ddx*ddx+ddy*ddy);
}


/* 
 * Find closest point in list to new x,y
 */
int closest_point(int npts, float *xvals, float *yvals, float x, float y)
{
  int i;
  float xp, yp, xnext, ynext;
  float d_pt, d_seg, dmin_pt, dmin_seg;
  int p_pt = 0, p_seg = 0;

  for (i = 0; i < npts; i++) {
    xp = xvals[i];
    yp = yvals[i];
    xnext = xvals[(i+1)%npts];
    ynext = yvals[(i+1)%npts];
    d_pt  = (xp-x)*(xp-x) + (yp-y)*(yp-y);
    d_seg = dist_segment(x,y,xp,yp,xnext,ynext);
    if (!i || d_pt<dmin_pt)   { 
      dmin_pt = d_pt; 
      p_pt = i; 
    }
    if (!i || d_seg<dmin_seg) { 
      dmin_seg = d_seg; 
      p_seg = i; 
    }
  }
  return p_seg;
}

/*
 * Calculate tangents from set of coords
 */
static int 
tangents(int npts, float *xvals, float *yvals, float *uvals, float *vvals)
{
  int i;
  float x, y, xprev, yprev, xnext, ynext;
  float u0, v0, u1, v1, n1, n0, n, u, v, fact;
  for (i = 0; i < npts; i++) {
    xprev = xvals[(i+(npts-1))%npts];
    yprev = yvals[(i+(npts-1))%npts];
    x = xvals[i];
    y = yvals[i];
    xnext = xvals[(i+1)%npts];
    ynext = yvals[(i+1)%npts];
    
    u0 = x-xprev;
    v0 = y-yprev;
    n0 = 1e-8f + (float) sqrt(u0*u0 + v0*v0);
    u1 = xnext-x;
    v1 = ynext-y;
    n1 = 1e-8f + (float) sqrt(u1*u1 + v1*v1);
    u = u0/n0 + u1/n1;
    v = v0/n0 + v1/n1;
    n = 1e-8f + (float) sqrt(u*u + v*v);
    fact = 0.5f*(n0 + n1);
      uvals[i] = fact*u/n;
      vvals[i] = fact*v/n;
  }
  return 0;
}


/*
 * Estimate 3th-order polynomial interpolation
 */
int third_order_poly(int npts, float *xvals, float *yvals, int res,
		     float *xinterp, float *yinterp)
{
  float x0, y0, x1, y1;
  float ax = 0, bx = 0, cx = 0, dx = 0, ay = 0, by = 0, cy = 0, dy = 0;
  float u0, v0, u1, v1;
  int i, j;

  float *uvals, *vvals;

  float t;
  int s;

  uvals = (float *) calloc(npts, sizeof(float));
  vvals = (float *) calloc(npts, sizeof(float));

  /* get tangents */
  tangents(npts, xvals, yvals, uvals, vvals);

  for (i = 0, j = 0; i < npts; i++) {
    x0 = xvals[i];
    y0 = yvals[i];
    x1 = xvals[(i+1)%npts];
    y1 = yvals[(i+1)%npts];
  
    u0 = uvals[i];
    v0 = vvals[i];
    u1 = uvals[(i+1)%npts];
    v1 = vvals[(i+1)%npts];
 
    ax = 2*(x0 - x1) + u0 + u1;
    bx = 3*(x1 - x0) - 2*u0 - u1;
    cx = u0;
    dx = x0;
    ay = 2*(y0 - y1) + v0 + v1;
    by = 3*(y1 - y0) - 2*v0 - v1;
    cy = v0;
    dy = y0;
    
    /* Compute t from the loop index rather than accumulating it.  Accumulating
       t += 1.0f/res drifts in float, so the emitted point count differed from
       the (res+1)*npts the caller allocated whenever 1/res was not exact in
       binary -- res of 40, 100 or 200 produced the wrong count and the caller
       reported "unable to interpolate data".  Indexing is exact for every res. */
    for (s = 0; s <= res; s++) {
      t = (float) s / (float) res;
      xinterp[j] = ax*t*t*t + bx*t*t + cx*t + dx;
      yinterp[j] = ay*t*t*t + by*t*t + cy*t + dy;
      j++;
    }
  }

  /* Make the first and last points exactly equal */
  xinterp[j-1] = xinterp[0];
  yinterp[j-1] = yinterp[0];

  free(uvals);
  free(vvals);
  return j;
}




/*
 * CREDIT: The bezier functions were taken from the Graphics Gems V
 *         chapter on quick and simple bezier curves
 *   DLS / MAR-98
 */

static void bezierForm(int nctrlpnts, float *x, float *y, float *bx, float *by)
/*   Setup Bezier coefficient array once for each control polygon. */
{
  int i, n, choose;
  n = nctrlpnts-1;
  for(i = 0; i <= n; i++) {
    if (i == 0) choose = 1;
    else if (i == 1) choose = n;
    else choose = choose *(n-i+1)/i;
    bx[i] = x[i]*choose;
    by[i] = y[i]*choose;
  }
}

static void bezierCurve(int nctrlpnts, float *bx, float *by, 
			float *wbx, float *wby,
			float *x, float *y, float t)
/*  Return point (x,y), t <= 0 <= 1 from C, given the number
    of Points in control polygon. BezierForm must be called
    once for any given control polygon. */
{   
  int k, n;
  float t1, tt, u;

  n = nctrlpnts - 1;  
  u = t;
  wbx[0] = bx[0];
  wby[0] = by[0];
  for(k =1; k <=n; k++) {
    wbx[k] = bx[k]*u;
    wby[k] = by[k]*u;
    u =u*t;
  }
  
  *x = wbx[n];  
  *y = wby[n];
  t1 = 1-t;
  tt = t1;
  for(k = n-1; k >=0; k--) {
    *x += wbx[k]*tt;
    *y += wby[k]*tt;
    tt =tt*t1;
  }
}

/*
 * NAME
 *  bezier_interpolatate
 *
 * DESCRIPTION
 *  Given series of 4x2 points, first with bezier splines with resolution
 * specfified by res.  Function assumes xinterp and yinterp have been 
 * allocated with enough space to hold npts*(res+1) floats.
 *
 */
int bezier_interpolate(int npts, float *x, float *y, 
		       int res, float *xinterp, float *yinterp)
{
  int j, k, m, nout;
  float tstep = (float) 1./res;
  int nquads = npts/4;
  float bx[4], by[4];		/* For holding coefficients  */
  float wbx[4], wby[4];		/* Working buffers for above */
  float xpos, ypos, t;

  float *xp = xinterp, *yp = yinterp;

  for (k = 0, m = 0; k < nquads; k++, m+=4) {
    bezierForm(4, &x[m], &y[m], bx, by);
    *xp++ = x[m];
    *yp++ = y[m];
    /* t from the index, not accumulated: see the same fix in
       third_order_poly.  The point count here was already safe, but an
       accumulated t drifts in float and shifts the samples at high res. */
    for (j = 1; j <= res; j++) {
      t = (float) j / (float) res;
      bezierCurve(4, bx, by, wbx, wby, &xpos, &ypos, t);
      *xp++ = xpos;
      *yp++ = ypos;
    }
  }

  nout = nquads*(res+1);
  /* Make the first and last points exactly equal */
  xinterp[nout-1] = xinterp[0];
  yinterp[nout-1] = yinterp[0];

  return (nout);
}



/*
  Return the clockwise status of a curve, clockwise or counterclockwise
  n vertices making up curve p
  return 0 for incomputables eg: colinear points
  CLOCKWISE == 1
  COUNTERCLOCKWISE == -1
  It is assumed that
  - the polygon is closed
  - the last point is not repeated.
  - the polygon is simple (does not intersect itself or have holes)
*/

#define CLOCKWISE (1)
#define COUNTERCLOCKWISE (-1)

#define CONVEX (1)
#define CONCAVE (-1)

static int polygon_clockwise(float *x, float *y,int n)
{
  int i,j,k;
  int count = 0;
  double z;
  
  if (n < 3)
    return(0);
  
  for (i=0;i<n;i++) {
    j = (i + 1) % n;
    k = (i + 2) % n;
    z  = (x[j] - x[i]) * (y[k] - y[j]);
    z -= (y[j] - y[i]) * (x[k] - x[j]);
    if (z < 0)
      count--;
    else if (z > 0)
      count++;
  }
  if (count > 0)
    return(COUNTERCLOCKWISE);
  else if (count < 0)
    return(CLOCKWISE);
  else
    return(0);
}

/*
  Return whether a polygon in 2D is concave or convex
  return 0 for incomputables eg: colinear points
  CONVEX == 1
  CONCAVE == -1
  It is assumed that the polygon is simple
  (does not intersect itself or have holes)
*/

static int polygon_convex(float *x, float *y, int n)
{
  int i,j,k;
  int flag = 0;
  double z;
  
  if (n < 3)
    return(0);
  
  for (i=0;i<n;i++) {
    j = (i + 1) % n;
    k = (i + 2) % n;
    z  = (x[j] - x[i]) * (y[k] - y[j]);
    z -= (y[j] - y[i]) * (x[k] - x[j]);
    if (z < 0)
      flag |= 1;
    else if (z > 0)
      flag |= 2;
    if (flag == 3)
      return(CONCAVE);
  }
  if (flag != 0)
    return(CONVEX);
  else
    return(0);
}

int polygon_is_simple(int n, float *x, float *y, int *ccw)
{
  int i;
  float x3[3];
  float y3[3];

  for (i = 0; i < n-2; i++) {
    ccw[i] = polygon_clockwise(&x[i], &y[i], 3);
  }

  /* Test last 2 triplets that wrap around */
  x3[0] = x[n-2]; x3[1] = x[n-1]; x3[2] = x[0]; 
  y3[0] = y[n-2]; y3[1] = y[n-1]; y3[2] = y[0]; 
  ccw[i++] = polygon_clockwise(&x3[0], &y3[0], 3);

  x3[0] = x[n-1]; x3[1] = x[0]; x3[2] = x[1]; 
  y3[0] = y[n-1]; y3[1] = y[0]; y3[2] = y[1]; 
  ccw[i++] = polygon_clockwise(&x3[0], &y3[0], 3);

  return 1;
}

/*
  public domain function by Darel Rex Finley, 2006
  Determines the intersection point of the line segment defined by points A&B
  with the line segment defined by points C and D.
  
  Returns 1 if the intersection point was found, and stores that point in X,Y.
  Returns 0 if there is no determinable intersection point
*/

static int lineSegmentIntersection(
				   double Ax, double Ay,
				   double Bx, double By,
				   double Cx, double Cy,
				   double Dx, double Dy,
				   double *X, double *Y) {
  
  double  distAB, theCos, theSin, newX, ABpos ;
  
  //  Fail if either line segment is zero-length.
  if (Ax==Bx && Ay==By || Cx==Dx && Cy==Dy) return 0;
  
  //  Fail if the segments share an end-point.
  if (Ax==Cx && Ay==Cy || Bx==Cx && By==Cy
      ||  Ax==Dx && Ay==Dy || Bx==Dx && By==Dy) {
    return 0; }
  
  //  (1) Translate the system so that point A is on the origin.
  Bx-=Ax; By-=Ay;
  Cx-=Ax; Cy-=Ay;
  Dx-=Ax; Dy-=Ay;
  
  //  Discover the length of segment A-B.
  distAB=sqrt(Bx*Bx+By*By);
  
  //  (2) Rotate the system so that point B is on the positive X axis.
  theCos=Bx/distAB;
  theSin=By/distAB;
  newX=Cx*theCos+Cy*theSin;
  Cy  =Cy*theCos-Cx*theSin; Cx=newX;
  newX=Dx*theCos+Dy*theSin;
  Dy  =Dy*theCos-Dx*theSin; Dx=newX;
  
  //  Fail if segment C-D doesn't cross line A-B.
  if (Cy<0. && Dy<0. || Cy>=0. && Dy>=0.) return 0;
  
  //  (3) Discover the position of the intersection point along line A-B.
  ABpos=Dx+(Cx-Dx)*Dy/(Dy-Cy);
  
  //  Fail if segment C-D crosses line A-B outside of segment A-B.
  if (ABpos<0. || ABpos>distAB) return 0;
  
  //  (4) Apply the discovered position to line A-B in the original coordinate system.
  *X=Ax+ABpos*theCos;
  *Y=Ay+ABpos*theSin;
  
  //  Success.
  return 1; 
}


int polygon_self_intersects(int nverts, float *x, float *y)
{
  int i, j;
  int nv_1 = nverts-1;
  double xi, yi;

  for (i = 0; i < nv_1; i++) {
    for (j = i+1; j < nv_1; j++) {
      if (lineSegmentIntersection(x[i], y[i], x[i+1], y[i+1],
				  x[j], y[j], x[j+1], y[j+1],
				  &xi, &yi)) 
	return 1;
    }
  }

  /* check the segment that wraps around */
  for (j = 0; j < nv_1; j++) {
    if (lineSegmentIntersection(x[nv_1], y[nv_1], x[0], y[0],
				x[j], y[j], x[j+1], y[j+1],
				&xi, &yi)) 
      return 1;
  }
  return 0;
}

/* Taken from http://alienryderflex.com/polygon/ 

 Globals which should be set before calling this function:

 int    polySides  =  how many corners the polygon has
 float  polyX[]    =  horizontal coordinates of corners
 float  polyY[]    =  vertical coordinates of corners
 float  x, y       =  point to be tested

 (Globals are used in this example for purposes of speed.  Change as
 desired.)

 The function will return YES if the point x,y is inside the polygon, or
 NO if it is not.  If the point is exactly on the edge of the polygon,
 then the function may return YES or NO.

 Note that division by zero is avoided because the division is protected
 by the "if" clause which surrounds it.
*/

int pointInPolygon(int polySides, float polyX[], float polyY[], 
		   float x, float y) 
{
  int      i, j=polySides-1 ;
  int      oddNodes=0      ;

  for (i=0; i<polySides; i++) {
    if ((polyY[i]< y && polyY[j]>=y
    ||   polyY[j]< y && polyY[i]>=y)
    &&  (polyX[i]<=x || polyX[j]<=x)) {
      if (polyX[i]+(y-polyY[i])/(polyY[j]-polyY[i])*(polyX[j]-polyX[i])<x) {
        oddNodes=!oddNodes; }}
    j=i; }

  return oddNodes; 
}


#ifdef STAND_ALONE  
int main(int argc, char *argv[])
{
  float x[] = { 128, 226, 384, 325, 370, 384, 246 };
  float y[] = { 128, 175, 128, 263, 266, 384, 299 };
  int i, n = 7*21;
  float *xinterp, *yinterp;

  xinterp = (float *) calloc(n, sizeof(float));
  yinterp = (float *) calloc(n, sizeof(float));

  n = third_order_poly(7, x, y, 20, xinterp, yinterp);
  
  for (i = 0; i < n; i++) {
    printf("%.2f %.2f\n", xinterp[i], yinterp[i]);
  }

  free(xinterp);
  free(yinterp);

}
#endif
