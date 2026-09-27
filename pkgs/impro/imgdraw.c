/*
 * imgdraw.c
 * draw utilities
 * Nikos K. Logothetis, 06 Apr 1996
 * Modified, updated by DLS, 30-MAY-1996/2002-2008 for inclusion into Tcl/Tk
 * And updated again and again...
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "img.h"

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

/*
 * This needs to be hooked into the system better so it can be
 * adjusted for a given setup...
 */
static float PixelsPerDegree = 40.0;
static float cf[8] = {1.0, 0.923879, 0.707106, 0.461748,
			   1.0, 0.461748, 0.707106, 0.923879};

/*
 ****************************************************************************
 *                       Image Creation/Deletion
 ****************************************************************************
 */
IMG_IMAGE *initimage (int width, int height, int depth, char *name)
{
  IMG_IMAGE *Image = (IMG_IMAGE *)calloc (1, sizeof(IMG_IMAGE));
  Image->width = width;
  Image->height = height;
  Image->depth = depth;
  Image->size = Image->width * Image->height * Image->depth;
  Image->data = (byte *)calloc (Image->size, sizeof(byte));
  strncpy(Image->name, name, 127);
  return ((IMG_IMAGE *)Image);
}

IMG_IMAGE *initimage_withvals (int width, int height, int depth, char *name, 
			       unsigned char *vals)
{
  IMG_IMAGE *Image = (IMG_IMAGE *)calloc (1, sizeof(IMG_IMAGE));
  Image->width = width;
  Image->height = height;
  Image->depth = depth;
  Image->size = Image->width * Image->height * Image->depth;
  Image->data = (byte *)vals;
  strncpy(Image->name, name, 127);
  return ((IMG_IMAGE *)Image);
}

IMG_IMAGE *img_copy(IMG_IMAGE *img, char *name)
{
  IMG_IMAGE *newimg = initimage (img->width, img->height, img->depth, name);
  memcpy(newimg->data, img->data, img->size);
  return newimg;
}

IMG_IMAGE *img_subsample(IMG_IMAGE *img, int subsample, char *name)
{
  int i, j, k, l, ox, oy, width, height;
  IMG_IMAGE *newimg;
  int newval;
  float subsample_2 = subsample*subsample;

  width = img->width/subsample;
  height = img->height/subsample;
  newimg = initimage (width, height, img->depth, name);

  for (i = 0, oy = 0; i < img->height; i+=subsample, oy++) {
    for (j = 0, ox = 0; j < img->width; j+=subsample, ox++) {
      newval = 0;
      for (k = 0; k < subsample; k++) {
	for (l = 0; l < subsample; l++) {
	  newval += readpoint(img, j+l, i+k);
	}
      }
      newval = (int) (newval / subsample_2);
      writepoint(newimg, ox, oy, newval);
    }
  }

  return newimg;
}

IMG_IMAGE *img_blend(IMG_IMAGE *img1, IMG_IMAGE *img2, IMG_IMAGE *alpha,
		     char *name)
{
  IMG_IMAGE *newimg;
  int i;
  int n;
  float p;

  if (!img1 || !img2 || !alpha) return NULL;

  if (img1->width  != img2->width ||
      img1->height != img2->height || 
      img1->depth  != img2->depth ||
      img1->width  != alpha->width ||
      img1->height != alpha->height ||
      img1->depth  != alpha->depth) {
    return NULL;
  }
  newimg = initimage (img1->width, img1->height, img1->depth, name);

  n = img1->width*img1->height*img1->depth;
  if (img1->depth == 4) {
    for (i = 0; i < n; i+=4) {
      p = alpha->data[i+3]/255.0;
      newimg->data[i+0] = img1->data[i+0]*p + img2->data[i+0]*(1.0-p);
      newimg->data[i+1] = img1->data[i+1]*p + img2->data[i+1]*(1.0-p);
      newimg->data[i+2] = img1->data[i+2]*p + img2->data[i+2]*(1.0-p);
    }
  }
  else {
    for (i = 0; i < img1->size; i++) {
      p = alpha->data[i]/255.0;
      newimg->data[i] = img1->data[i]*p + img2->data[i]*(1.0-p);
    }
  }

  return newimg;
}

IMG_IMAGE *img_mix(IMG_IMAGE *img1, IMG_IMAGE *img2, float p,
		   char *name)
{
  IMG_IMAGE *newimg;
  int i, size;
  
  if (!img1 || !img2) return NULL;

  if (img1->width  != img2->width ||
      img1->height != img2->height || 
      img1->depth  != img2->depth) {
    return NULL;
  }
  newimg = initimage (img1->width, img1->height, img1->depth, name);
  
  size = img1->width*img1->height*img1->depth;
  for (i = 0; i < size; i++) {
    newimg->data[i] = (byte) (img1->data[i]*p + img2->data[i]*(1.0-p));
  }
  return newimg;
}



IMG_IMAGE *img_grayscale(IMG_IMAGE *src, char *name)
{
  int i;
  IMG_IMAGE *dest = initimage (src->width, src->height, src->depth, name);
  unsigned char *d, *s, g;
  int npix = src->width*src->height;

  if (!dest) return NULL;

  if (src->depth != 3 && src->depth != 4) {
    memcpy(dest->data, src->data, npix*src->depth);
  }
  else if (src->depth == 3) {
    s = src->data;
    d = dest->data;
    for (i = 0; i < npix; i++, s+=3, d+=3) {
      g = (unsigned char ) (s[0] * .299 + s[1] * .587 + s[2] * .114);
      d[0] = d[1] = d[2] = g;
    }
  }
  else if (src->depth == 4) {
    s = src->data;
    d = dest->data;
    for (i = 0; i < npix; i++, s+=4, d+=4) {
      g = (unsigned char ) (s[0] * .299 + s[1] * .587 + s[2] * .114);
      d[0] = d[1] = d[2] = g;
      d[3] = s[3];
    }
  }
  return(dest);
}


IMG_IMAGE *img_recolor(IMG_IMAGE *src, float *m, int msize, char *name)
{
  int i;
  IMG_IMAGE *dest = initimage (src->width, src->height, src->depth, name);
  unsigned char *d, *s, g;
  int npix = src->width*src->height;

  if (!dest) return NULL;

  if (src->depth != 3 && src->depth != 4) {
    memcpy(dest->data, src->data, npix*src->depth);
  }
  else if (src->depth == 3) {
    static float m9[9];	    /* 3x3 to use for computation */

    if (msize != 9 && msize != 16) return NULL;
    if (msize == 9) memcpy(m9, m, 9*sizeof(float));
    else {			/* turn 4x4 to 3x3 */
      m9[0] = m[0]; m9[1] = m[1]; m9[2] = m[2];
      m9[3] = m[4]; m9[4] = m[5]; m9[5] = m[6];
      m9[6] = m[8]; m9[7] = m[9]; m9[8] = m[10];
    }
    s = src->data;
    d = dest->data;
    for (i = 0; i < npix; i++, s+=3, d+=3) {
      d[0] = (unsigned char ) (s[0] * m9[0] + s[1] * m9[1] + s[2] * m9[2]);
      d[1] = (unsigned char ) (s[0] * m9[3] + s[1] * m9[4] + s[2] * m9[5]);
      d[2] = (unsigned char ) (s[0] * m9[6] + s[1] * m9[7] + s[2] * m9[8]);
    }
  }
  else if (src->depth == 4) {
    static float m16[9];	    /* 4x4 to use for computation */
    if (msize != 16 && msize != 9) return NULL;
    if (msize == 16) memcpy(m16, m, 16*sizeof(float));
    else {			/* turn 3x3 to 4x4 */
      m16[0] = m[0]; m16[1] = m[1]; m16[2] = m[2];  m16[3] = 0.0;
      m16[4] = m[3]; m16[5] = m[4]; m16[6] = m[5];  m16[7] = 0.0;
      m16[8] = m[6]; m16[9] = m[7]; m16[10] = m[8]; m16[11] = 0.0;
      m16[12] = 0.0; m16[13] = 0.0; m16[14] = 0.0;  m16[15] = 1.0;
    }
    s = src->data;
    d = dest->data;
    for (i = 0; i < npix; i++, s+=4, d+=4) {
      d[0] = (unsigned char ) (s[0] * m16[0] +  s[1] * m16[1]  + s[2] * m16[2]  + s[3] * m16[3]);
      d[1] = (unsigned char ) (s[0] * m16[4] +  s[1] * m16[5]  + s[2] * m16[6]  + s[3] * m16[7]);
      d[2] = (unsigned char ) (s[0] * m16[8] +  s[1] * m16[9]  + s[2] * m16[10] + s[3] * m16[11]);
      d[3] = (unsigned char ) (s[0] * m16[12] + s[1] * m16[13] + s[2] * m16[14] + s[3] * m16[15]);
    }
  }
  return(dest);
}

#define CLAMP(v) ((v)<0?0:(((v)>255)?255:(v)))
IMG_IMAGE *img_bias(IMG_IMAGE *src, int *m, int msize, char *name)
{
  int i;
  IMG_IMAGE *dest = initimage (src->width, src->height, src->depth, name);
  unsigned char *d, *s;
  int npix = src->width*src->height;
  unsigned char v;
  
  if (!dest) return NULL;

  if (src->depth != 3 && src->depth != 4) {
    memcpy(dest->data, src->data, npix*src->depth);
  }
  else if (src->depth == 3) {
    if (msize != 3) return NULL;
    
    s = src->data;
    d = dest->data;
    for (i = 0; i < npix; i++, s+=3, d+=3) {
      v = (unsigned char) (s[0] + m[0]);
      v = CLAMP(v);
      d[0] = v;
      v = (unsigned char) (s[1] + m[1]);
      v = CLAMP(v);
      d[1] = v;
      v = (unsigned char) (s[2] + m[2]);
      v = CLAMP(v);
      d[2] = v;
    }
  }
  else if (src->depth == 4) {
    if (msize != 4) return NULL;

    s = src->data;
    d = dest->data;
    for (i = 0; i < npix; i++, s+=4, d+=4) {
      v = (unsigned char ) (s[0] + m[0]);
      v = CLAMP(v);
      d[0] = v;
      v = (unsigned char ) (s[1] + m[1]);
      v = CLAMP(v);
      d[1] = v;
      v = (unsigned char ) (s[2] + m[2]);
      v = CLAMP(v);
      d[2] = v;
      v = (unsigned char ) (s[3] + m[3]);
      v = CLAMP(v);
      d[3] = v;
    }
  }
  return(dest);
}


float img_alphaArea(IMG_IMAGE *img)
{
  float sum = 0.0;
  int i, npix;
  unsigned char *p;
  if (img->depth == 1) {
    npix = img->width*img->height;
    p = img->data;
    for (i = 0; i < npix; i++, p++) sum += p[0];
    return (sum/(255.*npix));
  }
  else if (img->depth == 2) {
    npix = img->width*img->height;
    p = img->data;
    for (i = 0; i < npix; i++, p+=2) sum += p[1];
    return (sum/(255.*npix));
  }
  else if (img->depth == 4) {
    npix = img->width*img->height;
    p = img->data;
    for (i = 0; i < npix; i++, p+=4) sum += p[3];
    return (sum/(255.*npix));
  }
  return(1.);
}

int img_alphaBB(IMG_IMAGE *img,
		  int *x0ptr, int *y0ptr, int *x1ptr, int *y1ptr,
		  float *cxptr, float *cyptr)
{
  int x0, y0, x1, y1;
  float sum = 0.0, *xvals, *yvals, cx, cy;
  int i, j, npix;
  unsigned char *p;
  if (img->depth != 1 && img->depth != 3 && img->depth != 4) return 0;

  xvals = (float *) calloc(img->width, sizeof(float));
  yvals = (float *) calloc(img->height, sizeof(float));

  npix = img->width*img->height;
  p = img->data+(img->depth-1);
  for (i = 0; i < img->height; i++) {
    for (j = 0; j < img->width; j++, p+=img->depth) {
      xvals[j] += *p;
      yvals[i] += *p;
    }
  }

  /* Find first col with data */
  for (i = 0, x0 = 0; i < img->width; i++) {
    if (xvals[i] > 0.0) { x0 = i; break; }
  }

  /* Find last col with data */
  for (i = img->width-1, x1 = img->width-1; i >= 0; i--) {
    if (xvals[i] > 0.0) { x1 = i; break; }
  }
  
  /* Find first row with data */
  for (j = 0, y0 = 0; j < img->height; j++) {
    if (yvals[j] > 0.0) { y0 = j; break; }
  }

  /* Find last col with data */
  for (j = img->height-1, y1 = img->height-1; j >= 0; j--) {
    if (yvals[j] > 0.0) { y1 = j; break; }
  }
  
  for (i = 0, cx = 0., sum = 0.; i < img->width; i++) {
    cx+=i*xvals[i];
    sum+=xvals[i];
  }
  cx /= sum;
  
  for (j = 0, cy = 0., sum = 0; j < img->height; j++) {
    cy+=j*yvals[j];
    sum+=yvals[j];
  }
  cy /= sum;
  
  if (x0ptr) *x0ptr = x0;
  if (y0ptr) *y0ptr = y0;
  if (x1ptr) *x1ptr = x1;
  if (y1ptr) *y1ptr = y1;
  if (cxptr) *cxptr = cx;
  if (cyptr) *cyptr = cy;

  return 1;
}


void freeimage (IMG_IMAGE *p)
{
  free((byte *)p->data);
  free((IMG_IMAGE *)p);
}


/*************************************************************************/
/*                         Image Manipulations                           */
/*************************************************************************/

void img_invert(IMG_IMAGE *img)
{
  int i;
  for (i = 0; i < img->size; i++) 
    img->data[i] = 255-(img->data)[i];
}

int img_gamma(IMG_IMAGE *img, float gamma)
{
  int i, size;
  byte *p;
  double g;
  byte lut[256];

  if (!img) return 0;
  p = img->data;
  size = img->size;
  
  if (gamma < 1.0) return 0;
  g = 1.0 / gamma;
  
  /* Make a lookup table */
  for (i = 0; i < 256; i++) lut[i] = 255.0 * pow (i/255.0, g);
  while (size--) *p = lut[*p++];
  return 1;
}

void img_copyarea(IMG_IMAGE *img_source, IMG_IMAGE *img_dest,
		  int sx, int sy, int width, int height,
		  int dx, int dy)
{
  int i, j, v;
  int ox, oy;			/* output x,y */
  int mx = sx+width;
  int my = sy+height;

  if (img_source->depth == 1 && img_dest->depth == 1) {
    for (j = sy, oy = dy; j < my; j++, oy++) {
      if (j < img_source->height) {
	for (i = sx, ox = dx; i < mx; i++, ox++) {
	  if (i >= 0 && i < img_source->width) {
	    v = readpoint(img_source, i, j);
	    if (ox < img_dest->width && oy < img_dest->height) {
	      writepoint(img_dest, ox, oy, v);
	    }
	  }
	}
      }
    }
  }
  else if (img_source->depth >= 3 && img_dest->depth >= 3) {
    byte r, g, b, a;
    for (j = sy, oy = dy; j < my; j++, oy++) {
      for (i = sx, ox = dx; i < mx; i++, ox++) {
	if (i >= 0 && i < img_source->width) {
	  if (readRGBA(img_source, i, j, &r, &g, &b, &a))
	    writeRGBA(img_dest, ox, oy, r, g, b, a);
	}
      }
    }
  }
}


void img_blendarea(IMG_IMAGE *img_source, IMG_IMAGE *img_dest,
		  int sx, int sy, int width, int height,
		  int dx, int dy)
{
  int i, j, v;
  int ox, oy;			/* output x,y */
  int mx = sx+width;
  int my = sx+height;

  if (img_source->depth == 1 && img_dest->depth == 1) {
    for (j = sy, oy = dy; j < my; j++, oy++) {
      for (i = sx, ox = dx; i < mx; i++, ox++) {
	if (i >= 0 && i < img_source->width*img_source->depth) {
	  v = readpoint(img_source, i, j);
	  writepoint(img_dest, ox, oy, v);
	}
      }
    }
  }
  else if (img_source->depth == 3 && img_dest->depth >= 3) {
    byte r, g, b, a;
    for (j = sy, oy = dy; j < my; j++, oy++) {
      for (i = sx, ox = dx; i < mx; i++, ox++) {
	if (i >= 0 && i < img_source->width) {
	  if (readRGBA(img_source, i, j, &r, &g, &b, &a))
	    writeRGBA(img_dest, ox, oy, r, g, b, a);
	}
      }
    }
  }
  /* The only interesting case: should make more general */
  else if (img_source->depth == 4 && img_dest->depth >= 3) {
    byte dr, dg, db, da, sr, sg, sb, sa, r, g, b, a;
    float blend;
    for (j = sy, oy = dy; j < my; j++, oy++) {
      for (i = sx, ox = dx; i < mx; i++, ox++) {
	if (i >= 0 && i < img_source->width) {
	  if (readRGBA(img_source, i, j, &sr, &sg, &sb, &sa)) {
	    if (readRGBA(img_dest, ox, oy, &dr, &dg, &db, &da)) {
	      blend = sa/255.;
	      r = blend*sr+(1-blend)*dr;
	      g = blend*sg+(1-blend)*dg;
	      b = blend*sb+(1-blend)*db;
	      writeRGBA(img_dest, ox, oy, r, g, b, da);
	    }
	  }
	}
      }
    }
  }
}

/*
 ****************************************************************************
 *                  Pattern Creation Functions (mk-ers)
 ****************************************************************************
 */
void mkgauss (IMG_IMAGE *p, float sigma, int bkgvalue, int maxvalue)
{
  byte *pixval;
  register int i, j, k;
  register int xlo, xhi, dist, arcrad;

  float radius = 3.0*(double) sigma;
  int iDiameter = 2 * radius + 1;
  int r_squ = radius * radius;
  int ylo = p->height/2;
  int yhi = p->height/2;
  int hwidth = p->width/2;
  float denom = 2.0 * (float)sigma * sigma;

  pixval = (byte *)calloc (iDiameter, sizeof(byte));
  for (i = 0; i <= radius; i++)
    pixval[i] = maxvalue * exp(-i*(float)i/denom);

  for (j = 0; j <= radius; j++) {
    k = j;
    arcrad = dist = j * j;
    xlo = xhi = hwidth;
    for(i = 1; i <= iDiameter; i += 2) {
      if (dist > arcrad)
	arcrad += (k++ << 1) + 1;
      if (dist <= r_squ) {
	writepoint(p, xlo, ylo, pixval[k]);
	writepoint(p, xhi, ylo, pixval[k]);
	writepoint(p, xlo, yhi, pixval[k]);
	writepoint(p, xhi, yhi, pixval[k]);
	dist += i;
      } else {
	writepoint(p, xlo, ylo, bkgvalue);
	writepoint(p, xhi, ylo, bkgvalue);
	writepoint(p, xlo, yhi, bkgvalue);
	writepoint(p, xhi, yhi, bkgvalue);
      }
      --xlo; ++xhi;
    }
    --ylo; ++yhi;
  }
  free(pixval);
}

void mkchecker (IMG_IMAGE *p, int width, int height, int wavelen, int minvalue,
		int maxvalue)
{
  register int i, j, k;
  int idx = 0;
  int idy = 0;
  int hwavelen = wavelen >> 1;

  for (j = 0; j < height; j += hwavelen) {
    idy = 1 - idy;
    if (idy) idx = 0;
    else idx = 1;
    for (i = 0; i < width; i++) {
      if (!(i%hwavelen)) idx = 1 - idx;
      if (idx) {
	for (k = j; k < j+hwavelen; k++)
	  writepoint(p, i, k, maxvalue);
      } else {
	for (k = j; k < j+hwavelen; k++)
	  writepoint(p, i, k, minvalue);
      }
    }
  }
}

void mkramp (IMG_IMAGE *p, int width, int height, int orientation, int wavelen,
	     int bkgvalue, int amplitude)
{
  int i, j;
  float incr = (2.0 * amplitude) / (double)wavelen;

  for (j = 0; j < height; j++)
    for (i = 0; i < width; i++)
      writepoint(p, i, j, bkgvalue + (i % wavelen) * incr);
}

void mkconcentricsine(IMG_IMAGE *p, float sf, float phase)
{
  register int pix, r, c;
  float rad;
  int xrad, yrad;

  xrad = p->width/2;
  yrad = p->height/2;

  pix = 0;
  for (r = -yrad; r < yrad; r++) {
    for (c = -xrad; c < xrad; c++, pix++) {
      rad = sqrt((double)r*r+(double)c*c)/PixelsPerDegree;
      p->data[pix]=(byte)(128+(128.0*sin((IMG_PI*phase)+(IMG_TWOPI*sf*rad))));
    }
  }
}
  

void mksine (IMG_IMAGE *p, int width, int height, int orientation, int wavelen,
	     int bkgvalue, int amplitude, float phase)
{
  byte *lut;
  int i, j;
  int incr, xincr, yincr;
  int period = (double)wavelen / cf[orientation] + 0.5;
  double step;

  switch (orientation) {
    case 1: xincr = 1; yincr = 1; break;
    case 2: xincr = 1; yincr = 0; break;
    case 3: xincr = 2; yincr = 0; break;
    case 5: xincr = period - 2; yincr = 0; break;
    case 6: xincr = period - 1; yincr = 0; break;
    case 7: xincr = period - 1; yincr = 1; break;
    case 0: case 4: default: xincr = yincr = 0; break;
    }
  step = IMG_TWOPI / period;
  lut = (byte *)calloc(2*width, sizeof(byte));

  for (i = 0; i < 2*width; i++)
    lut[i] = amplitude * sin(step*i+phase) + bkgvalue;
  
  if (orientation != 4) {
    for (incr = xincr, i = 0; i < height; i++) {
      j = incr % period;
      writeline(p, 0, i, width, &lut[j]);
      incr += xincr;
      if (yincr && (i & 1)) incr -= xincr;
    }
  } else {
    for (i = 0; i < width; i++)
      writecolumn(p, i, 0, height, lut);
  }
  free(lut);		       
}



void mkradialsine(IMG_IMAGE *p, float af)
{
  register int pix, r, c;
  float ang;
  int xrad, yrad;

  xrad = p->width/2;
  yrad = p->height/2;

  pix = 0;
  for (r = -yrad; r < yrad; r++) {
    for (c = -xrad; c < xrad; c++, pix++) {
      if (!c) 
	ang = atan2(r,0.00001);
      else if (!r)
	ang = atan2(0.00001, c);
      else 
	ang = atan2(r,c);
      p->data[pix] = (byte)(128+(128.0*cos(af*ang)));
    }
  }
}

/*
 ****************************************************************************
 *                           Read/Write Routines
 ****************************************************************************
 */

void writepoint(IMG_IMAGE *img, int x, int y, byte value)
{
  int offset;
  switch (img->depth) {
  case 1:
    if (x < img->width && y < img->height) {
      img->data[y*img->width+x] = value;
    }
    break;
  case 3:
  case 4:
    if (x < img->width && y < img->height) {
      offset = img->depth*(y*img->width+x);
      img->data[offset++] = value;
      img->data[offset++] = value;
      img->data[offset] = value;
    }
    break;
  }
}

byte readpoint(IMG_IMAGE *img, int x, int y)
{
  register int i = y * img->width + x;
  if (i < img->size)
    return(img->data[i]);
  else return (0);
}



int writeRGBA(IMG_IMAGE *img, int x, int y, byte r, byte g, byte b, byte a)
{
  int offset;
  int value;

  if (x >= img->width || y >= img->height) return 0;

  switch (img->depth) {
  case 1:
    value = (unsigned int) (r * .299 + g * .587 + b * .114);
    img->data[y*img->width+x] = value;
    break;
  case 3:
    offset = img->depth*(y*img->width+x);
    img->data[offset++] = r;
    img->data[offset++] = g;
    img->data[offset] = b;
    break;
  case 4:
    offset = img->depth*(y*img->width+x);
    img->data[offset++] = r;
    img->data[offset++] = g;
    img->data[offset++] = b;
    img->data[offset] = a;
    break;
  }
  return 1;
}

int readRGBA(IMG_IMAGE *img, int x, int y, byte *r, byte *g, byte *b, byte *a)
{
  int offset;

  if (x >= img->width || y >= img->height) { return 0; }

  switch (img->depth) {
  case 1:
    *r = *g = *b = img->data[y*img->width+x];
    *a = 255;
    break;
  case 3:
    offset = img->depth*(y*img->width+x);
    *r = img->data[offset++];
    *g = img->data[offset++];
    *b = img->data[offset];
    *a = 255;
    break;
  case 4:
    offset = img->depth*(y*img->width+x);
    *r = img->data[offset++];
    *g = img->data[offset++];
    *b = img->data[offset++];
    *a = img->data[offset];
    break;
  }
  return 1;
}

void writeline(IMG_IMAGE *img, int x, int y, int len, byte *p)
{
  register int i =y * img->width + x;
  if ((i+len) < img->size) {
    memcpy(&img->data[i], p, len);
  } else {
    fprintf(stderr, "Warning, trying to write line in unallocated space\n");
  }
}

void readline(IMG_IMAGE *img, int x, int y, int len, byte *p)
{
  register int i;
  register byte *pp = p;
  int offset = y * img->width + x;

  if ((offset+len) < img->size) {
    for (i = 0; i < len; i++)
      *pp++ = img->data[offset+i];
  }
}

void writecolumn(IMG_IMAGE *img, int x, int y, int len, byte *p)
{
  register int i;
  register byte *pp = p;
  for (i = y; i < y+len; i++)
    img->data[img->width*i+x] = *pp++;

}

void readcolumn(IMG_IMAGE *img, int x, int y, int len, byte *p)
{
  register int i;
  register byte *pp = p;
  for (i = y; i < y+len; i++)
    *pp++ = img->data[img->width*i+x];
}

void readrect(IMG_IMAGE *img, int x, int y, int dx, int dy, byte *p)
{
  register int i, j;
  register byte *pp = p;
  for (j = y; j < y + dy; j++)
    for (i = x; i < x + dx; i++)
      *pp++ = readpoint(img, i, j);
}

void writerect(IMG_IMAGE *img, int x, int y, int dx, int dy, int v, float tr)
{
  register int i, j;
  float f = 1.0 - tr;
  float fv = tr * v;
  int temp;

  for (j = y; j < y + dy; j++)
    for (i = x; i < x + dx; i++) {
      if (!tr)
	writepoint(img, i, j, v);
      else {
	temp = MIN(255,(int)(f*readpoint(img,i,j)+fv));
	writepoint(img, i, j, temp);
      }
    }
}

void writeregion(IMG_IMAGE *img, int x, int y, int *element, 
		 int dx, int dy)
{
  register int i, j, k;
  
  for (k = 0, j = y; j < y + dy; j++) {
    for (i = x; i < x + dx; i++) {
      writepoint(img, i, j, element[k++]);
    }
  }
}

void bltints(IMG_IMAGE *img, int x, int y, unsigned int *element, 
	      int dx, int dy)
{
  int i, j, k, end_x = x+dx, end_y = y+dy;
  unsigned int *dest = (unsigned int *) img->data;

  if (x == 0 && dx == img->width && y == 0 && dx == img->height) {
    memcpy(img->data, (unsigned char *) element, dx*dy*4);
    return;
  }

  for (j = y, k = 0; j < end_y; j++) {
    for (i = x; i < end_x; i++, k++) {
      if (i < img->width && j < img->height) {
	dest[j*img->width+i] = element[k];
      }
    }
  }
}

void bltrgb(IMG_IMAGE *img, int x, int y, unsigned char *data, 
	    int dx, int dy)
{
  int i, j, k, offset;
  unsigned char *p;
  
  p = data;
  for (k = 0, j = y; j < y + dy; j++) {
    for (i = x; i < x + dx; i++,p+=3) {
      if (i < img->width && j < img->height) {
	offset = 3*(j*img->width+i);
	img->data[offset++] = *p;
	img->data[offset++] = *(p+1);
	img->data[offset] = *(p+2);
      }
    }
  }
}

void bltbytes(IMG_IMAGE *img, int x, int y, unsigned char *element, 
	      int dx, int dy)
{
  register int i, j, k, end_x = x+dx, end_y = y+dy;
  
  for (k = 0, j = y; j < end_y; j++) {
    for (i = x; i < end_x; i++) {
      writepoint(img, i, j, element[k++]);
    }
  }
}


int blt(IMG_IMAGE *dest, IMG_IMAGE *src, 
	int x, int y, int dx, int dy)
{
  if (dest->depth == 1 && src->depth == 1) {
    bltbytes(dest, x, y, (unsigned char *) src->data, dx, dy);
  }
  else if (dest->depth == 3 && src->depth == 3) {
    bltrgb(dest, x, y, (unsigned char *) src->data, dx, dy);
  }
  else if (dest->depth == 4 && src->depth == 4) {
    bltints(dest, x, y, (unsigned int *) src->data, dx, dy);
  }
  else return 0;

  return 1;
}


void writemask(IMG_IMAGE *img, int x, int y, int *element, 
	       int dx, int dy)
{
  byte  v, oldv;
  int i, j, k;
  int stop_y = y+dy, stop_x = x+dx;

  for (k = 0, j = y; j < stop_y; j++) {
    for (i = x; i < stop_x; i++) {
      v = (byte) element[k++];
      oldv = readpoint(img, i, j);
      if (v) writepoint(img, i, j, MAX(v,oldv));
    }
  }
}


void writecirc(IMG_IMAGE *img, int x, int y, int radius, int v, float tr)
{
  register int i, j, k;
  int xlo, xhi, ylo, yhi, dist, arcrad;
  int temp, size, r_squ;
  float f = 1.0 - tr;
  float fv = tr * v;
  
  size = 2 * radius + 1;
  r_squ = radius * radius;
  ylo = yhi = y;
  for (j = 0; j <= radius; j++) {
    k = j;
    arcrad = dist = j * j;
    xlo = xhi = x;
    for(i = 1; i <= size; i += 2) {
      if (dist > arcrad)
	arcrad += (k++ << 1) + 1;
      if (dist <= r_squ) {
	if (tr < 0.01) {
	  writepoint(img, xlo, ylo, v);
	  writepoint(img, xhi, ylo, v);
	  writepoint(img, xlo, yhi, v);
	  writepoint(img, xhi, yhi, v);
	} else {
	  temp = MIN(255,(int)(f*readpoint(img,xlo,ylo)+fv));
	  writepoint(img, xlo, ylo, temp);
	  if (xlo != xhi) {
	    temp = MIN(255,(int)(f*readpoint(img,xhi,ylo)+fv));
	    writepoint(img, xhi, ylo, temp);
	  }
	  if (ylo != yhi) {
	    temp = MIN(255,(int)(f*readpoint(img,xlo,yhi)+fv));
	    writepoint(img, xlo, yhi, temp);
	    if (xlo != xhi) {
	      temp = MIN(255,(int)(f*readpoint(img,xhi,yhi)+fv));
	      writepoint(img, xhi, yhi, temp);
	    }
	  }
	}
	dist += i;
      }
      --xlo; ++xhi;
    }
    --ylo; ++yhi;
  }
}
