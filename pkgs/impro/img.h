/****************************************************************************
 *
 * NAME
 *   img.h
 *
 * DESCRIPTION
 *   Header file for basic IMage PROcessing functions
 *
 ****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#define IMG_MAX_COLOR 255
#define IMG_PI 3.14159265
#define IMG_TWOPI (2.0*IMG_PI)
#define TAN1 0.017455064

typedef unsigned int   Dimension; /* for memory allocations		*/
typedef unsigned char  byte;      /* byte type				*/

typedef struct {                  /* image structure			*/
  char name[128];		  /* name of image for accessing        */
  Dimension width;            	  /* width of image in pixels		*/
  Dimension height;           	  /* height of image in pixels		*/
  Dimension size;	          /* height of image in bytes		*/
  Dimension depth;                /* depth of image in bytes            */
  byte *data;             	  /* data in bytes			*/
} IMG_IMAGE;

/*
 ****************************************************************************
 *  Function Prototypes
 ****************************************************************************
 */

IMG_IMAGE *initimage (int width, int height, int depth, char *name);
IMG_IMAGE *initimage_withvals (int width, int height, int depth, char *name, 
			       unsigned char *vals);
void freeimage (IMG_IMAGE *p);

IMG_IMAGE *img_copy(IMG_IMAGE *img, char *name);
IMG_IMAGE *img_load (char *filename, char *name, int width, int height);
IMG_IMAGE *img_blend(IMG_IMAGE *img1, IMG_IMAGE *img2, IMG_IMAGE *alpha,
		     char *name);
IMG_IMAGE *img_mix(IMG_IMAGE *img1, IMG_IMAGE *img2, float mixprop,
		   char *name);
IMG_IMAGE *img_subsample(IMG_IMAGE *img, int subsample, char *name);
IMG_IMAGE *img_grayscale(IMG_IMAGE *img, char *name);
IMG_IMAGE *img_recolor(IMG_IMAGE *src, float *m, int msize, char *name);
IMG_IMAGE *img_bias(IMG_IMAGE *src, int *m, int msize, char *name);


float img_alphaArea(IMG_IMAGE *img);
int   img_alphaBB(IMG_IMAGE *img, int *, int *, int *, int *, 
		    float *, float *);

void img_invert(IMG_IMAGE *img);
void img_copyarea(IMG_IMAGE *img_source, IMG_IMAGE *img_dest,
		  int sx, int sy, int width, int height,
		  int dx, int dy);
void img_blendarea(IMG_IMAGE *img_source, IMG_IMAGE *img_dest,
		     int sx, int sy, int width, int height,
		   int dx, int dy);
int img_gamma(IMG_IMAGE *img, float gamma);

void writepoint(IMG_IMAGE *img, int x, int y, byte value);
  byte readpoint(IMG_IMAGE *img, int x, int y);
  int readRGBA(IMG_IMAGE *img, int x, int y, byte *r, byte *g, byte *b, byte *a);
int writeRGBA(IMG_IMAGE *img, int x, int y, byte r, byte g, byte b, byte a);
void writeline(IMG_IMAGE *img, int x, int y, int width, byte *p);
void readline(IMG_IMAGE *img, int x, int y, int width, byte *p);
void writecolumn(IMG_IMAGE *img, int x, int y, int height, byte *p);
void readcolumn(IMG_IMAGE *img, int x, int y, int height, byte *p);
void readrect(IMG_IMAGE *img, int x, int y, int dx, int dy, byte *p);
void writecirc(IMG_IMAGE *img, int x, int y, int radius, int v, float tr);
void writerect(IMG_IMAGE *img, int x, int y, int dx, int dy, int v, float tr);
void writeregion(IMG_IMAGE *img, int x, int y, int *element, int dx, int dy);
int  blt(IMG_IMAGE *dest, IMG_IMAGE *src, int x, int y, int dx, int dy);
void writemask(IMG_IMAGE *img, int x, int y, int *element, int dx, int dy);


/*
 ****************************************************************************
 * Pattern Making Prototypes
 ****************************************************************************
 */
void mkgauss (IMG_IMAGE *img, float radius, int bkgvalue, int maxvalue);
void mksine (IMG_IMAGE *img, int width, int height, int orientation,
	     int wavelen, int bkgvalue, int amplitude, float phase);
void mksquare (IMG_IMAGE *img, int width, int height, int orientation,
	       int wavelen, int bkgvalue, int amplitude);
void mkchecker (IMG_IMAGE *img, int width, int height, int wavelen,
		int minvalue, int maxvalue);
void mkramp (IMG_IMAGE *img, int width, int height, int orientation,
	     int wavelen, int bkgvalue, int amplitude);
void mkconcentricsine(IMG_IMAGE *img, float sf, float phase);
void mkradialsine(IMG_IMAGE *img, float af);



#define BYTE_IMAGE

#ifdef BYTE_IMAGE
typedef unsigned char kz_pixel_t;        /* for 8 bit-per-pixel images */
#define uiNR_OF_GREY (256)
#else
typedef unsigned short kz_pixel_t;       /* for 12 bit-per-pixel images (default) */
# define uiNR_OF_GREY (4096)
#endif

/******** Prototype of CLAHE function. Put this in a separate include file. *****/
int CLAHE(kz_pixel_t* pImage, unsigned int uiXRes, unsigned int uiYRes, kz_pixel_t Min,
          kz_pixel_t Max, unsigned int uiNrX, unsigned int uiNrY,
          unsigned int uiNrBins, float fCliplimit);



#ifdef __cplusplus
}
#endif
