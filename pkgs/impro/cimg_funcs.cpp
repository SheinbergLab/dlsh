// Include CImg library file and use its main namespace
#define cimg_display 0
#include "CImg.h"

#include "img.h"
#include "cimg_funcs.h"
using namespace cimg_library;

CImg<unsigned char> img2cimg(IMG_IMAGE *img) 
{
  unsigned char *pixels, *p, *r, *g, *b, *a;
  int i,npix;
  if (img->depth == 3) {
    npix = img->width*img->height;
    pixels = (unsigned char *) calloc(img->width*img->height*img->depth, sizeof(char));
    p = img->data;
    r = pixels;  g = r+npix;  b = g+npix;
    for (i = 0; i < npix; i++) {
      *r++ = *p++; *g++ = *p++; *b++ = *p++; 
    }
  }
  else if (img->depth == 4) {
    npix = img->width*img->height;
    pixels = (unsigned char *) calloc(img->width*img->height*img->depth, sizeof(char));
    p = img->data;
    r = pixels;  g = r+npix;  b = g+npix;  a = b+npix;
    for (i = 0; i < npix; i++) {
      *r++ = *p++; *g++ = *p++; *b++ = *p++; *a++ = *p++; 
    }
  }
  else {
    pixels = img->data;
  }

  CImg<unsigned char> image((const unsigned char *) pixels, 
			    img->width, img->height, 1, img->depth, 
			    false);
  if (img->depth > 2) {
    free(pixels);
  }
  return image;
}

unsigned char *cimg2pixels(CImg<unsigned char> image)
{
  unsigned char *pixels, *p, *r, *g, *b, *a;
  int i, npix;
  if (image.spectrum() == 3) {
    npix = image.width()*image.height();
    p = pixels = (unsigned char *) calloc(image.width()*image.height()*image.spectrum(), sizeof(char));
    r = image.data(0,0,0,0); g = image.data(0,0,0,1); b = image.data(0,0,0,2);
    for (i = 0; i < npix; i++) {
      *p++ = *r++; *p++ = *g++; *p++ = *b++; 
    }
  }
  else if (image.spectrum() == 4) {
    npix = image.width()*image.height();
    p = pixels = (unsigned char *) 
      calloc(image.width()*image.height()*image.spectrum(), sizeof(char));
    r = image.data(0,0,0,0); g = image.data(0,0,0,1); 
    b = image.data(0,0,0,2); a = image.data(0,0,0,3);
    for (i = 0; i < npix; i++) {
      *p++ = *r++; *p++ = *g++; *p++ = *b++; *p++ = *a++; 
    }
  }
  else {
    p = &image[0];
    npix = image.width()*image.height();
    pixels = (unsigned char *) 
      calloc(image.width()*image.height()*image.spectrum(), sizeof(char));
    memcpy(pixels, p, npix);
  }
  return pixels;

}

IMG_IMAGE * cimg2img(CImg<unsigned char> image, char *name)
{
  IMG_IMAGE *img;
  unsigned char *pixels;

  pixels = cimg2pixels(image);
  img = initimage_withvals (image.width(), image.height(), image.spectrum(), name, 
			    pixels);
  
  return img;
}

void CImg_showImg(IMG_IMAGE *img)
{
  CImg<unsigned char> image = img2cimg(img);

  // Create display window
  CImgDisplay
    main_disp(image,"Impro Image",0);

  while (!main_disp.is_closed() && !main_disp.is_keyESC()) {
    // Handle display window resizing (if any)
    if (main_disp.is_resized()) main_disp.resize().display(image);
    cimg::wait(20);
  }
}

IMG_IMAGE *CImg_resizeImg(IMG_IMAGE *img, int newx, int newy, 
			  int interp, char *name)
{
  CImg<unsigned char> image = img2cimg(img);
  image.resize(newx, newy, -100, -100, interp, -1, false);
  return(cimg2img(image, name));
}

IMG_IMAGE *CImg_mirrorImg(IMG_IMAGE *img, char axis, char *name)
{
  CImg<unsigned char> image = img2cimg(img);
  image.mirror(axis);
  return(cimg2img(image, name));
}

IMG_IMAGE *CImg_rotateImg(IMG_IMAGE *img, double angle, int cond, char *name)
{
  CImg<unsigned char> image = img2cimg(img);
  image.rotate(angle, cond);
  return(cimg2img(image, name));
}

IMG_IMAGE *CImg_cropImg(IMG_IMAGE *img, int x0, int y0, int x1, int y1, char *name)
{
  CImg<unsigned char> image = img2cimg(img);
  image.crop(x0, y0, x1, y1);
  return(cimg2img(image, name));
}


IMG_IMAGE *CImg_edgeDetectImg(IMG_IMAGE *img, int mode, double alpha, double thresL, double thresH, char *name)
{
  CImg<unsigned char> image = img2cimg(img);

  const unsigned char
    black[3] = { 0,0,0 };          // Defining the colors we need for drawing.

  CImg<unsigned char> edge(img->width, img->height);

  /* Was: get_crop(0,0,256,256) -- hard-coded crop region that
     silently truncated images larger than 256x256 and zero-padded
     smaller ones, corrupting edge-detection results for any image
     not exactly 256x256. Removed; the gradient is now computed on
     the whole image as intended. */
  CImg<> visu_bw = CImg<>(image).get_norm().normalize(0,255).resize(-100,-100,1,2,2);
  edge.fill(255); // Background color in the edge-detection window is white.
  CImgList<> grad(visu_bw.blur((float)alpha).normalize(0,255).get_gradient());
  
  const double
    pi = cimg::PI,
    p8 = pi/8.0, p38 = 3.0*p8,
    p58 = 5.0*p8, p78 = 7.0*p8;
  
  cimg_forXY(visu_bw,s,t) {
    if ( 1 <= s && s <= visu_bw.width()-1 && 1 <= t && t <=visu_bw.height()-1) { // if - good points
      double
	Gs = grad[0](s,t),                  //
	Gt = grad[1](s,t),                               //  The actual pixel is (s,t)
	Gst = cimg::abs(Gs) + cimg::abs(Gt),    //
	Gr, Gur, Gu, Gul, Gl, Gdl, Gd, Gdr;
      double theta = std::atan2(Gt,Gs)+pi; // theta is from the interval [0,Pi]
      switch(mode) {
      case 1: // Hysterese is applied
	if (Gst>=thresH) { edge.draw_point(s,t,black); }
	else if (thresL <= Gst && Gst < thresH) {
	  // Neighbourhood of the actual pixel:
	  Gr = cimg::abs(grad[0](s+1,t)) + cimg::abs(grad[1](s+1,t)); // right
	  Gl = cimg::abs(grad[0](s-1,t)) + cimg::abs(grad[1](s-1,t)); // left
	  Gur = cimg::abs(grad[0](s+1,t+1)) + cimg::abs(grad[1](s+1,t+1)); // up right
	  Gdl = cimg::abs(grad[0](s-1,t-1)) + cimg::abs(grad[1](s-1,t-1)); // down left
	  Gu = cimg::abs(grad[0](s,t+1)) + cimg::abs(grad[1](s,t+1)); // up
	  Gd = cimg::abs(grad[0](s,t-1)) + cimg::abs(grad[1](s,t-1)); // down
	  Gul = cimg::abs(grad[0](s-1,t+1)) + cimg::abs(grad[1](s-1,t+1)); // up left
	  Gdr = cimg::abs(grad[0](s+1,t-1)) + cimg::abs(grad[1](s+1,t-1)); // down right
	  if (Gr>=thresH || Gur>=thresH || Gu>=thresH || Gul>=thresH
	      || Gl>=thresH || Gdl >=thresH || Gu >=thresH || Gdr >=thresH) {
	    edge.draw_point(s,t,black);
	  }
	};
	break;
      case 2: // Angle 'theta' of the gradient (Gs,Gt) at the point (s,t).
	if(theta >= pi)theta-=pi;
	//rounding theta:
	if ((p8 < theta && theta <= p38 ) || (p78 < theta && theta <= pi)) {
	  Gr = cimg::abs(grad[0](s+1,t)) + cimg::abs(grad[1](s+1,t)); // right
	  Gl = cimg::abs(grad[0](s-1,t)) + cimg::abs(grad[1](s-1,t)); // left
	  if (Gr < Gst && Gl < Gst) {
	    edge.draw_point(s,t,black);
	  }
	}
	else if ( p8 < theta && theta <= p38) {
	  Gur = cimg::abs(grad[0](s+1,t+1)) + cimg::abs(grad[1](s+1,t+1)); // up right
	  Gdl = cimg::abs(grad[0](s-1,t-1)) + cimg::abs(grad[1](s-1,t-1)); // down left
	  if (Gur < Gst && Gdl < Gst) {
	    edge.draw_point(s,t,black);
	  }
	}
	else if ( p38 < theta && theta <= p58) {
	  Gu = cimg::abs(grad[0](s,t+1)) + cimg::abs(grad[1](s,t+1)); // up
	  Gd = cimg::abs(grad[0](s,t-1)) + cimg::abs(grad[1](s,t-1)); // down
	  if (Gu < Gst && Gd < Gst) {
	    edge.draw_point(s,t,black);
	  }
	}
	else if (p58 < theta && theta <= p78) {
	  Gul = cimg::abs(grad[0](s-1,t+1)) + cimg::abs(grad[1](s-1,t+1)); // up left
	  Gdr = cimg::abs(grad[0](s+1,t-1)) + cimg::abs(grad[1](s+1,t-1)); // down right
	  if (Gul < Gst && Gdr < Gst) {
	    edge.draw_point(s,t,black);
	  }
	};
	break;
      } // switch
    } // if good-points
  }  // cimg_forXY */

  return(cimg2img(edge, name));

}

IMG_IMAGE *CImg_drawFillImg(IMG_IMAGE *img, int x, int y, 
			    unsigned char *fillcolor, char *name)
{
  CImg<unsigned char> image = img2cimg(img);
  image.draw_fill(x, y, fillcolor);
  return(cimg2img(image, name));
}

int CImg_fillDoubleDetect (IMG_IMAGE *img, char *name)
{
  int i, j;
  CImg<unsigned char> image = img2cimg(img);
  unsigned char color[] = { 100,100,100 };
  int whiteCounter = 0;
  int returned = 0;
  for (i = 0; i < image.width(); i++) {
    for (j = 0; j < image.height(); j++) {
      if ((image.atXY(i,j,0) == 255) &&
	  (image.atXY(i,j,1) == 255) &&
	  (image.atXY(i,j,2) == 255)) {
	whiteCounter++;
	returned ++;
	if(whiteCounter < 2){
	  image.draw_fill(i,j,color);
	}
      }
    }
  }
  return returned;
}

IMG_IMAGE *CImg_drawPolygon(IMG_IMAGE *img, int np, float *x, float *y, 
			    unsigned char *color, unsigned int pattern, char *name) 
{
  CImg<unsigned char> image = img2cimg(img);
  CImgList<float> points;
  int i;
  for (i = 0; i < np; i++) {
    points.insert(CImg<>::vector(x[i],y[i]));
  }
  
  CImg<> npoints = points>'x';
  if (pattern != 0) {
    image.draw_polygon(npoints, color, 1, 0xffffffff);
  }
  else {
    image.draw_polygon(npoints, color);
  }
  return(cimg2img(image, name));
}


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

IMG_IMAGE *CImg_drawPolygonAlt(IMG_IMAGE *img, int np, float *x, float *y, 
			       unsigned char *color, unsigned int pattern,
			       char *name) 
{
  CImg<unsigned char> image;
  image = img2cimg(img);
  
  CImgList<float> points;
  int i;
  int maxtries = 2000, tries = 0;
  float fillx, filly;
  for (i = 0; i < np; i++) {
    points.insert(CImg<>::vector(x[i],y[i]));
  }
  
  CImg<> npoints = points>'x';

  image.draw_polygon(npoints, color, 1, 0xffffffff);

  /* Pick a flood-fill seed inside the polygon. Try the polygon's
     centroid first (always inside a convex polygon); fall back to the
     image center; then sample random points within the polygon's
     bounding box. Previously this used `rand()*image.width()` which
     overflows to garbage out-of-bounds coordinates, so the fallback
     never found a valid seed and small/off-center polygons were drawn
     as outlines only. */
  float cx = 0.0f, cy = 0.0f;
  float xmin = x[0], xmax = x[0], ymin = y[0], ymax = y[0];
  for (i = 0; i < np; i++) {
    cx += x[i]; cy += y[i];
    if (x[i] < xmin) xmin = x[i]; if (x[i] > xmax) xmax = x[i];
    if (y[i] < ymin) ymin = y[i]; if (y[i] > ymax) ymax = y[i];
  }
  cx /= np; cy /= np;

  fillx = cx; filly = cy;
  if (!pointInPolygon(np, x, y, fillx, filly)) {
    fillx = image.width()  / 2.0f;
    filly = image.height() / 2.0f;
    while (!pointInPolygon(np, x, y, fillx, filly)) {
      fillx = xmin + ((float) rand() / (float) RAND_MAX) * (xmax - xmin);
      filly = ymin + ((float) rand() / (float) RAND_MAX) * (ymax - ymin);
      if (tries++ > maxtries) break;
    }
  }
  if (tries <= maxtries) {
    image.draw_fill(fillx, filly, color);
  }
  return(cimg2img(image, name));
}

IMG_IMAGE *CImg_fillPolygonOutside(IMG_IMAGE *img, int np, float *x, float *y, 
				   unsigned char *color, unsigned int pattern,
				   char *name) 
{
  CImg<unsigned char> image = img2cimg(img);
  CImgList<float> points;
  int i;
  int maxtries = 2000, tries = 0;
  float fillx, filly;
  for (i = 0; i < np; i++) {
    points.insert(CImg<>::vector(x[i],y[i]));
  }
  
  CImg<> npoints = points>'x';
  image.draw_polygon(npoints, color, 1, 0xffffffff);

  /* Pick a flood-fill seed OUTSIDE the polygon. Same `rand()` overflow
     bug as in CImg_drawPolygonAlt was here too -- fixed identically.
     The image corners are guaranteed outside any polygon that fits
     within the image, so try those first before falling back to
     random sampling within the image. */
  fillx = 0.0f;
  filly = 0.0f;
  if (pointInPolygon(np, x, y, fillx, filly)) {
    fillx = (float) image.width()  - 1.0f;
    filly = (float) image.height() - 1.0f;
    while (pointInPolygon(np, x, y, fillx, filly)) {
      fillx = ((float) rand() / (float) RAND_MAX) * (float) image.width();
      filly = ((float) rand() / (float) RAND_MAX) * (float) image.height();
      if (tries++ > maxtries) break;
    }
  }
  if (tries <= maxtries) {
    image.draw_fill(fillx, filly, color);
  }

  return(cimg2img(image, name));
}


IMG_IMAGE *CImg_fftImg(IMG_IMAGE *image, int rmin, int rmax, char *name)
{
  CImg<unsigned char> img = img2cimg(image);
  CImgList<float> F = img.get_FFT();
  cimglist_apply(F,shift)(img.width()/2,img.height()/2,0,0,2);
  const CImg<unsigned char> mag = ((F[0].get_pow(2) + F[1].get_pow(2)).sqrt() + 1).log().normalize(0,255);
  CImg<unsigned char> mask(img.width(),img.height(),1,1,1);
  unsigned char one[] = { 1 }, zero[] = { 0 }, white[] = { 255 };

  mask.fill(0).draw_circle(mag.width()/2,mag.height()/2,rmax,one).
    draw_circle(mag.width()/2,mag.height()/2,rmin,zero);
  CImgList<float> nF(F);
  cimglist_for(F,l) nF[l].mul(mask).shift(-img.width()/2,-img.height()/2,0,0,2);
  CImg<unsigned char> filtered = nF.FFT(true)[0].normalize(0,255);

  return(cimg2img(filtered, name));
}

IMG_IMAGE *CImg_blurImg(IMG_IMAGE *image, float sigma, int boundary, int is_gaussian, char *name)
{
  CImg<unsigned char> img = img2cimg(image);
  CImg<unsigned char> blurred = img.get_blur(sigma, (const bool) boundary, (const bool) is_gaussian);
  return(cimg2img(blurred, name));
}

