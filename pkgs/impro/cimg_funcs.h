#define cimg_display 0

#ifdef __cplusplus
extern "C" {
#endif

  //  void CImg_pushCImg(IMG_IMAGE *);
  //  void CImg_popCImg(IMG_IMAGE *);
  void CImg_showImg(IMG_IMAGE *);
  
  IMG_IMAGE *CImg_resizeImg(IMG_IMAGE *img, int newx, int newy, 
			    int interp, char *name);
  IMG_IMAGE *CImg_rotateImg(IMG_IMAGE *img, double angle, int cond, char *name);
  IMG_IMAGE *CImg_mirrorImg(IMG_IMAGE *img, char axis, char *name);
  IMG_IMAGE *CImg_cropImg(IMG_IMAGE *img, int x0, int y0, int x1, int y1, char *name);
  IMG_IMAGE *CImg_drawFillImg(IMG_IMAGE *img, int x, int y, 
			      unsigned char *fillcolor, char *name);
  IMG_IMAGE *CImg_edgeDetectImg(IMG_IMAGE *img, int mode, double thresL, double thresH, double alpha, char *name);
  int CImg_fillDoubleDetect(IMG_IMAGE *img, char *name);
  IMG_IMAGE *CImg_drawPolygon(IMG_IMAGE *img, int n, float *x, float *y,
			      unsigned char *color, unsigned int pattern, char *name);
  IMG_IMAGE *CImg_drawPolygonAlt(IMG_IMAGE *img, int n, float *x, float *y,
				 unsigned char *color, unsigned int pattern, char *name);
  IMG_IMAGE *CImg_drawPolygonChannelMask(IMG_IMAGE *img, int n, float *x, float *y,
					 unsigned char *color, int channel_mask,
					 char *name);
  IMG_IMAGE *CImg_fillPolygonOutside(IMG_IMAGE *img, int n, float *x, float *y,
				     unsigned char *color, unsigned int pattern, char *name);
  IMG_IMAGE *CImg_fftImg(IMG_IMAGE *img, int rmin, int rmax, char *name);
  IMG_IMAGE *CImg_blurImg(IMG_IMAGE *image, float sigma, int boundary_conditions, int is_gaussian, char *name);
  
#ifdef __cplusplus
}
#endif
