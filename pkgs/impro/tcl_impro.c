/*
 * NAME
 *      tcl_impro.c
 *
 * DESCRIPTION
 *      Image drawing utility package
 *
 * DETAILS
 *      This file contains image primitives for making pixmaps, etc.
 *
 * AUTHOR
 *      DLS
 *
 * DATE
 *      MAY-1996, JUNE-2011, NOV-2013, JULY-2018
 */

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#pragma warning (disable:4244)
#pragma warning (disable:4305)
#define DllEntryPoint DllMain
#define EXPORT(a,b) __declspec(dllexport) a b
#else
#define DllEntryPoint
#define EXPORT a b
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <tcl.h>
//#include <tk.h>
#include <math.h>

#include "img.h"

#include "df.h"
#include "dfana.h"
#include "tcl_dl.h"
#include "labtcl.h"
#include <rawapi.h>
#include "targa.h"
#include "lodepng.h"

#include "cimg_funcs.h"

/*
 * Local tables for holding imgs
 */
static Tcl_HashTable imgTable;
static int imgCount = 0;		/* increasing count of images  */
extern DYN_LIST *imageMakeDynList(IMG_IMAGE *image);

enum IMPRO_POLYDRAW    { IMPRO_POLY1, IMPRO_POLY2, IMPRO_POLY3 };
enum IMPRO_FROMSTRING   { IMPRO_FROM_BINARY, IMPRO_FROM_BASE64 };
enum IMPRO_PNGOUTPUT   { IMPRO_PNG_TO_IMG, IMPRO_PNG_TO_DYNLIST };

/****************************************************************************/
/*                        img Function Declarations                         */
/****************************************************************************/

static int imgHelpCmd                (ClientData, Tcl_Interp *, int, char **);
static int imgCreateCmd              (ClientData, Tcl_Interp *, int, char **);
static int imgLoadCmd                (ClientData, Tcl_Interp *, int, char **);
static int imgCopyCmd                (ClientData, Tcl_Interp *, int, char **);
static int imgSubsampleCmd           (ClientData, Tcl_Interp *, int, char **);
static int imgBlendCmd               (ClientData, Tcl_Interp *, int, char **);
static int imgMixCmd                 (ClientData, Tcl_Interp *, int, char **);
static int imgDeleteCmd              (ClientData, Tcl_Interp *, int, char **);
static int imgWriteCmd               (ClientData, Tcl_Interp *, int, char **);
static int imgWidthCmd               (ClientData, Tcl_Interp *, int, char **);
static int imgHeightCmd              (ClientData, Tcl_Interp *, int, char **);
static int imgDimsCmd                (ClientData, Tcl_Interp *, int, char **);
static int imgDepthCmd               (ClientData, Tcl_Interp *, int, char **);
static int imgMakeObjCmd             (ClientData, Tcl_Interp *, int, char **);
static int imgMkGaussCmd             (ClientData, Tcl_Interp *, int, char **);
static int imgMkSineCmd              (ClientData, Tcl_Interp *, int, char **);
static int imgWriteCirclesCmd        (ClientData, Tcl_Interp *, int, char **);
static int imgWriteRectsCmd          (ClientData, Tcl_Interp *, int, char **);
static int imgWriteElementsCmd       (ClientData, Tcl_Interp *, int, char **);
static int imgComposeElementsCmd     (ClientData, Tcl_Interp *, int, char **);
static int imgInvertCmd              (ClientData, Tcl_Interp *, int, char **);
static int imgGammaCmd               (ClientData, Tcl_Interp *, int, char **);
static int imgCopyAreaCmd            (ClientData, Tcl_Interp *, int, char **);
static int imgBlendAreaCmd           (ClientData, Tcl_Interp *, int, char **);
static int imgScaleAlphaCmd          (ClientData, Tcl_Interp *, int, char **);
static int imgAddAlphaCmd            (ClientData, Tcl_Interp *, int, char **);
static int imgAlphaAreaCmd           (ClientData, Tcl_Interp *, int, char **);
static int imgAlphaBBCmd             (ClientData, Tcl_Interp *, int, char **);
static int imgPremultiplyAlphaCmd    (ClientData, Tcl_Interp *, int, char **);
static int imgBkgFillCmd             (ClientData, Tcl_Interp *, int, char **);
static int imgImageToListCmd         (ClientData, Tcl_Interp *, int, char **);
static int imgImageFromListCmd       (ClientData, Tcl_Interp *, int, char **);
static int imgInfoCmd                (ClientData, Tcl_Interp *, int, char **);
static int imgAddHeaderCmd           (ClientData, Tcl_Interp *, int, char **);
static int imgReadCmd                (ClientData, Tcl_Interp *, int, char **);
static int imgReadRawCmd             (ClientData, Tcl_Interp *, int, char **);
static int imgReadRawFilesCmd        (ClientData, Tcl_Interp *, int, char **);
static int imgReadTargaCmd           (ClientData, Tcl_Interp *, int, char **);
static int imgLoadTargaCmd           (ClientData, Tcl_Interp *, int, char **);
static int imgWriteTargaCmd          (ClientData, Tcl_Interp *, int, char **);
static int imgLoadPNGCmd             (ClientData, Tcl_Interp *, int, char **);
static int imgToPNGCmd               (ClientData, Tcl_Interp *, int, char **);
static int imgReadPNGCmd             (ClientData, Tcl_Interp *, int, char **);
static int imgWritePNGCmd            (ClientData, Tcl_Interp *, int, char **);
static int imgDirCmd                 (ClientData, Tcl_Interp *, int, char **);

static int imgShowImgCmd             (ClientData, Tcl_Interp *, int, char **);

static int imgResizeImgCmd           (ClientData, Tcl_Interp *, int, char **);
static int imgRescaleImgCmd          (ClientData, Tcl_Interp *, int, char **);

static int imgDrawFillImgCmd         (ClientData, Tcl_Interp *, int, char **);
static int imgEdgeDetectImgCmd       (ClientData, Tcl_Interp *, int, char **);
static int imgFillDetectImgCmd       (ClientData, Tcl_Interp *, int, char **);

static int imgDrawPolygonCmd         (ClientData, Tcl_Interp *, int, char **);
static int imgDrawPolygonChannelCmd  (ClientData, Tcl_Interp *, int, char **);
static int imgDrawPolygonFastCmd     (ClientData, Tcl_Interp *, int, char **);
static int imgDrawPolylineCmd        (ClientData, Tcl_Interp *, int, char **);


static int imgFlipImgCmd            (ClientData, Tcl_Interp *, int, char **);
static int imgMirrorImgCmd           (ClientData, Tcl_Interp *, int, char **);
static int imgRotateImgCmd           (ClientData, Tcl_Interp *, int, char **);
static int imgGrayscaleImgCmd        (ClientData, Tcl_Interp *, int, char **);
static int imgRecolorImgCmd          (ClientData, Tcl_Interp *, int, char **);
static int imgBiasImgCmd             (ClientData, Tcl_Interp *, int, char **);

static int imgFilterImgCmd           (ClientData, Tcl_Interp *, int, char **);
static int imgBlurImgCmd             (ClientData, Tcl_Interp *, int, char **);

static int imgEqualizeCmd            (ClientData, Tcl_Interp *, int, char **);

static int imgImageFromPNGCmd(ClientData cdata, Tcl_Interp * interp, 
			      int objc, Tcl_Obj * const objv[]);

enum AREA_MODE  { COPY_AREA, BLEND_AREA };

static TCL_COMMANDS IMGcommands[] = {
  { "img_dir",              imgDirCmd,      NULL, 
      "return list of loaded images" },
  { "img_create",           imgCreateCmd,      NULL, 
      "create an image" },
  { "img_load",             imgLoadCmd,        NULL, 
      "load an image" },
  { "img_copy",             imgCopyCmd,        NULL, 
      "copy an image" },
  { "img_blend",            imgBlendCmd,       NULL, 
      "blend image1 with image2 using alpha image" },
  { "img_mix",              imgMixCmd,       NULL, 
      "mix image1 with image2 using using mixprop*i1+(1-mixprop)*i2" },
  { "img_subsample",        imgSubsampleCmd,   NULL, 
      "subsample an image" },
  { "img_delete",           imgDeleteCmd,      NULL, 
      "delete an image" },
  { "img_write",            imgWriteCmd,       NULL, 
      "write an image" },
  { "img_width",            imgWidthCmd,       NULL, 
      "return width an image" },
  { "img_height",           imgHeightCmd,      NULL, 
      "return height of image" },
  { "img_dims",             imgDimsCmd,        NULL, 
      "return dimensions (heightxwidth) an image" },
  { "img_depth",             imgDepthCmd,        NULL, 
      "return depth of image" },

  { "img_gamma",            imgGammaCmd,      NULL, 
      "gamma correct an image" },
  { "img_invert",           imgInvertCmd,      NULL, 
      "invert image" },
  { "img_copyarea",         imgCopyAreaCmd,      (ClientData) COPY_AREA, 
      "copy an area from a source image to a dest" },
  { "img_blendarea",        imgCopyAreaCmd,      (ClientData) BLEND_AREA, 
      "blend an area from a source image to a dest" },
  { "img_bkgfill",          imgBkgFillCmd,     NULL, 
      "fill background with uniform color" },
  
  { "img_circles",          imgWriteCirclesCmd,     NULL, 
      "draw circles onto an image" },
  { "img_rectangles",       imgWriteRectsCmd,       NULL, 
      "draw rectangles onto an image" },
  { "img_elements",         imgWriteElementsCmd,     NULL, 
      "draw elements onto an image" },

  { "img_compose",          imgComposeElementsCmd,     NULL, 
      "compose multiple elements into an image" },
  { "img_addAlpha",         imgAddAlphaCmd,     NULL, 
      "insert alpha vals from one image into another" },
  { "img_scaleAlpha",       imgScaleAlphaCmd,     NULL, 
      "multiply alpha vals by a constant" },
  { "img_premultiplyAlpha", imgPremultiplyAlphaCmd,     NULL, 
      "premultiply an RGBA image" },
  { "img_alphaArea",        imgAlphaAreaCmd,     NULL, 
      "proportion of image visisble through alpha mask" },
  { "img_alphaBB",          imgAlphaBBCmd,     NULL, 
      "compute bounding region of alpha channel" },

  { "img_mkgauss",          imgMkGaussCmd,     NULL, 
      "create 2D gaussian" },
  { "img_mksine",           imgMkSineCmd,      NULL, 
      "create sinusoidal grating" },
  { "img_makeobj",          imgMakeObjCmd,     NULL, 
      "make image available to system" },

  { "img_img2list",         imgImageToListCmd,     NULL, 
    "get image in dynlist form" },
  { "img_imgtolist",        imgImageToListCmd,     NULL, 
    "get image in dynlist form" },
  { "img_list2img",         imgImageFromListCmd,     NULL, 
    "get image in dynlist form" },
  { "img_imgfromlist",      imgImageFromListCmd,     NULL, 
    "convert dynlist to image" },

  { "img_info", imgInfoCmd, NULL, "get image info" },
  { "img_read", imgReadCmd, NULL, "read a raw or targa file" },
  { "img_readraw", imgReadRawCmd, NULL, "read a raw file into dynlist" },
  { "img_readtarga", imgReadTargaCmd, NULL, "read a targa file into dynlist" },
  { "img_readTarga", imgReadTargaCmd, NULL, "read a targa file into dynlist" },
  { "img_loadTarga", imgLoadTargaCmd, NULL, "read a targa file into image" },
  { "img_writeTarga", imgWriteTargaCmd, NULL, "write an image to a targa file" },
  { "img_readPNG",  imgReadPNGCmd, NULL, "read a PNG file into dynlist" },
  { "img_loadPNG",  imgLoadPNGCmd, NULL, "read a PNG file into image" },
  { "img_toPNG",    imgToPNGCmd, NULL, "encode an image into PNG byte array" },
  { "img_writePNG", imgWritePNGCmd, NULL, "write an image to a PNG file" },
  { "img_readraws", imgReadRawFilesCmd, NULL, "read multiple raw files" },
  
  { "img_addHeader", imgAddHeaderCmd, NULL, "add header to raw image" },


#if 0
  { "img_show",      imgShowImgCmd, NULL, "show img using cimg library" },
#endif

  { "img_resize",    imgResizeImgCmd, NULL, "resize img using cimg library" },
  { "img_rescale",   imgRescaleImgCmd, NULL, "rescale keeping original dims" },
  { "img_mirrorx",   imgMirrorImgCmd, (ClientData) 'x', "mirror around 'x' img using cimg library" },
  { "img_mirrory",   imgMirrorImgCmd, (ClientData) 'y', "mirror around 'y' img using cimg library" },
  { "img_rotate",    imgRotateImgCmd, NULL, "rotate img using cimg library" },
  { "img_grayscale", imgGrayscaleImgCmd, NULL, "convert image to grayscale" },
  { "img_recolor",   imgRecolorImgCmd, NULL, "recolor image by matrix multiplication" },
  { "img_bias",      imgBiasImgCmd, NULL, "bias image pixels" },

  { "img_drawFill",   imgDrawFillImgCmd, NULL, "draw filled 2D region starting at position x,y in color fillcolor" },
  { "img_drawPolygon", imgDrawPolygonCmd, (ClientData) IMPRO_POLY2, 
    "draw polygon in color fillcolor" },
  { "img_drawPolygonFast", imgDrawPolygonCmd, (ClientData) IMPRO_POLY1, 
    "draw polygon in color fillcolor (faster method)" },
  { "img_fillPolygonOutside", imgDrawPolygonCmd, (ClientData) IMPRO_POLY3,
    "fill around polygon in color fillcolor" },
  { "img_drawPolygonChannel", imgDrawPolygonChannelCmd, NULL,
    "draw polygon writing only channels selected by mask (1=R 2=G 4=B 8=A)" },
  { "img_drawPolyline", imgDrawPolylineCmd, NULL, "draw polygon in using patterned color" },

  { "img_edgeDetect",imgEdgeDetectImgCmd, NULL, "edge detect img using cimg library" },
  { "img_fillDoubleDetect",imgFillDetectImgCmd, NULL, "detect doubles using fill from the cimg library" },

  { "img_filter", imgFilterImgCmd, NULL, "filter using FFT" },
  { "img_blur", imgBlurImgCmd, NULL, "blur image" },

  { "img_equalize",     imgEqualizeCmd,        NULL, 
      "use contrast limited adpated histogram equalization on an image" },
  

  { "img_help",             imgHelpCmd,        (void *) IMGcommands, 
      "display help" },
  { NULL, NULL, NULL, NULL }
};


static int base64decode (char *in, unsigned int inLen, unsigned char *out, unsigned int *outLen); 
static int base64encode(const void* data_buf, int dataLength, char* result, int resultSize);

/************************************************************************/
/************************************************************************/
/*                            Impro_Init                                */
/************************************************************************/
/************************************************************************/


#ifdef WIN32
#define _ANSI_ARGS_(a) a
EXPORT(int,Impro_Init) _ANSI_ARGS_((Tcl_Interp *interp))
#else
int Impro_Init(Tcl_Interp *interp)
#endif
{
  int i;
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

  if (Tcl_PkgProvide(interp, "impro", "1.4") != TCL_OK) {
    return TCL_ERROR;
  }

  i = 0;
  while (IMGcommands[i].name) {
    Tcl_CreateCommand(interp, IMGcommands[i].name, 
		      (Tcl_CmdProc *) IMGcommands[i].func, 
		      (ClientData) IMGcommands[i].cd, 
		      (Tcl_CmdDeleteProc *) NULL);
    i++;
  }

  Tcl_CreateObjCommand(interp, "img_imgfromPNG",
		       (Tcl_ObjCmdProc *) imgImageFromPNGCmd,
		       (ClientData) IMPRO_FROM_BINARY, NULL);
  Tcl_CreateObjCommand(interp, "img_imgfromPNG64", imgImageFromPNGCmd,
		       (ClientData) IMPRO_FROM_BASE64, NULL);
  Tcl_CreateObjCommand(interp, "img_listfromPNG", imgImageFromPNGCmd,
		       (ClientData) IMPRO_FROM_BINARY, NULL);
  Tcl_CreateObjCommand(interp, "img_listfromPNG64", imgImageFromPNGCmd,
		       (ClientData) IMPRO_FROM_BASE64, NULL);


  Tcl_InitHashTable(&imgTable, TCL_STRING_KEYS);
  
  return TCL_OK;
}


#ifdef WIN32
EXPORT(int,Impro_SafeInit) _ANSI_ARGS_((Tcl_Interp *interp))
#else
int Impro_SafeInit(Tcl_Interp *interp)
#endif
{
  return Impro_Init(interp);
}


#ifdef WIN32
EXPORT(int,Impro_Unload) _ANSI_ARGS_((Tcl_Interp *interp))
#else
int Impro_Unload(Tcl_Interp *interp)
#endif
{
  return TCL_OK;
}


#ifdef WIN32
EXPORT(int,Impro_SafeUnload) _ANSI_ARGS_((Tcl_Interp *interp))
#else
int Impro_SafeUnload(Tcl_Interp *interp)
#endif
{
  return TCL_OK;
}

/************************************************************************/
/*                              TCL Code                                */
/************************************************************************/

static int imgFindImg(Tcl_Interp *interp, char *name, IMG_IMAGE **img)
{
  IMG_IMAGE *image;
  Tcl_HashEntry *entryPtr;

  if ((entryPtr = Tcl_FindHashEntry(&imgTable, name))) {
    image = Tcl_GetHashValue(entryPtr);
    if (!image) {
      Tcl_SetResult(interp, "bad image ptr in hash table", TCL_STATIC);
      return TCL_ERROR;
    }
    if (img) *img = image;
    return TCL_OK;
  }
  else {
    if (img) {			/* If img was null, then don't set error */
      Tcl_AppendResult(interp, "image \"", name, "\" not found", 
		       (char *) NULL);
    }
    return TCL_ERROR;
  }
}

static int imgAddImage(Tcl_Interp *interp, IMG_IMAGE *newimage, char *imgname)
{
  Tcl_HashEntry *entryPtr;
  int newentry;

  entryPtr = Tcl_CreateHashEntry(&imgTable, imgname, &newentry);
  Tcl_SetHashValue(entryPtr, newimage);
  Tcl_SetResult(interp, imgname, TCL_STATIC);
  return TCL_OK;
}

/****************************************************************************
 *                            Bound Functions
 ****************************************************************************/

static int imgHelpCmd (ClientData cd, Tcl_Interp *interp, 
                      int argc, char *argv[])
{
  TCL_COMMANDS *commands = (TCL_COMMANDS *) cd;
  int i = 0;

  while (commands[i].name) {
    printf("%-22s - %s\n", commands[i].name, commands[i].desc);
    i++;
  }
  return TCL_OK;
}

static int imgCreateCmd(ClientData clientData, Tcl_Interp *interp,
			int argc, char *argv[])
{
  int width = 256, height = 256, depth = 1;
  int i, j;
  IMG_IMAGE *newimage;
  static char imgname[128];

  /* This is a nasty command parsing loop, looking for option pairs */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i],"-width")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no width specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &width) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-height")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no width specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &height) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-depth")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no width specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &depth) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, argv[0], 
			 ": bad option ", argv[i], (char *) NULL);
	goto error;
      }
    }
  }
  
  if (argc > 1) strncpy(imgname, argv[1], 127);
  else sprintf(imgname, "img%d", imgCount++);
  
  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = initimage(width, height, depth, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to create image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  return imgAddImage(interp, newimage, imgname);
  
 error:
  return TCL_ERROR;
}


static int imgLoadCmd(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  int width = 0, height = 0;	/* default to guessing */
  int d, header_bytes;
  int i, j;
  IMG_IMAGE *newimage;
  static char imgname[128], filename[128];
  int size;
  FILE *infp;
  unsigned char *pixels;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " filename", NULL);
    return TCL_ERROR;
  }

  /* if Targa or PNG file, call those functions instead */
  if (strstr(argv[1], ".tga") || strstr(argv[1], ".TGA")) {
    return imgLoadTargaCmd(clientData, interp, argc, argv);
  }
  else if (strstr(argv[1], ".png") || strstr(argv[1], ".PNG")) {
    return imgLoadPNGCmd(clientData, interp, argc, argv);
  }

  /* 
   * Command parsing loop, looking for option pairs
   * replace me with good code!
   */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i],"-width")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no width specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &width) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-height")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no width specified", (char *) NULL);
	  goto error;
	}
	if (Tcl_GetInt(interp, argv[i+1], &height) != TCL_OK) goto error;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, argv[0], 
			 ": bad option ", argv[i], (char *) NULL);
	goto error;
      }
    }
  }
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " filename [imgname]", 
		     " [-width width] [-height height]", (char *) NULL);
    return TCL_ERROR;
  }
  else strncpy(filename, argv[1], 127);

  if (argc > 2) strncpy(imgname, argv[2], 127);
  else sprintf(imgname, "img%d", imgCount++);


  if (!raw_getImageDims(argv[1], &width, &height, &d, &header_bytes)) {
    Tcl_AppendResult(interp, argv[0], ": unable to determin dims for file \"",
		     argv[1], "\"", NULL);
    return(TCL_ERROR);
  }
  
  size = width*height*d;
  pixels = malloc(size);
  if (!pixels) {
    Tcl_AppendResult(interp, argv[0], ": error allocating memory", NULL);
    return TCL_ERROR;
  }

  infp = fopen(argv[1], "rb");
  if (header_bytes) fseek(infp, header_bytes, SEEK_SET);
  if (fread(pixels, 1, size, infp) != size) {
    Tcl_AppendResult(interp, argv[0], ": error reading raw file", NULL);
    fclose(infp);
    free(pixels);
    return TCL_ERROR;
  }
  fclose(infp);

  if (!(newimage = initimage_withvals(width, height, d, imgname, pixels))) {
    Tcl_AppendResult(interp, argv[0], ": unable to create image ", imgname, 
		     (char *) NULL);
    free(pixels);
    return TCL_ERROR;
  }

  return imgAddImage(interp, newimage, imgname);
  
 error:
  return TCL_ERROR;
}


static int imgCopyCmd(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  IMG_IMAGE *newimage, *image;
  static char imgname[128];

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image [newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  
  if (argc > 2) {
    strncpy(imgname, argv[2], 127);
  }
  else sprintf(imgname, "img%d", imgCount++);

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = img_copy(image, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to copy image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);
}


static int imgGrayscaleImgCmd(ClientData clientData, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  IMG_IMAGE *image, *newimage;
  static char imgname[128];
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image [newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (argc > 2) strncpy(imgname, argv[2], 127);
  else sprintf(imgname, "img%d", imgCount++);
  
  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = img_grayscale(image, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to convert image to grayscale", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);

  return TCL_OK;
}


static int imgRecolorImgCmd(ClientData clientData, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  IMG_IMAGE *image, *newimage;
  DYN_LIST *colormatrix;
  static char imgname[128];
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image matrix [newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &colormatrix) != TCL_OK) return TCL_ERROR;
  if (DYN_LIST_DATATYPE(colormatrix) != DF_FLOAT) {
    Tcl_AppendResult(interp, argv[0], ": invalid color matrix", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (argc > 3) strncpy(imgname, argv[3], 127);
  else sprintf(imgname, "img%d", imgCount++);
  
  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = img_recolor(image,
			       (float *) DYN_LIST_VALS(colormatrix), DYN_LIST_N(colormatrix),
			       imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to recolor image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);

  return TCL_OK;
}

static int imgBiasImgCmd(ClientData clientData, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  IMG_IMAGE *image, *newimage;
  DYN_LIST *bias;
  static char imgname[128];
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image biasvals [newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &bias) != TCL_OK) return TCL_ERROR;
  if (DYN_LIST_DATATYPE(bias) != DF_LONG) {
    Tcl_AppendResult(interp, argv[0], ": invalid bias vals (should be ints)", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (argc > 3) strncpy(imgname, argv[3], 127);
  else sprintf(imgname, "img%d", imgCount++);
  
  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = img_bias(image,
			    (int *) DYN_LIST_VALS(bias), DYN_LIST_N(bias),
			    imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to bias image pixels", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);

  return TCL_OK;
}


static int imgEqualizeCmd(ClientData clientData, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  IMG_IMAGE *newimage, *image;
  static char imgname[128];
  int i, npix;
  unsigned char *graypix;
  unsigned char *s, *d;		/* source/destination data pointers */
  unsigned char min = 0;
  unsigned char max = 0xff;
  int nrX = 1, nrY = 1;			/* number of x and y regions */
  int nrBins = 128;
  double clipLimit = 255.;
  float alpha;
  int min_override = -1, max_override = -1;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " image ?min? ?max? ?n_xregions? ?n_yregions? ?n_bins? ?clip_limit? ?newname?", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  
  if (argc > 8) {
    strncpy(imgname, argv[8], 127);
  }
  else sprintf(imgname, "img%d", imgCount++);

  if (argc > 2) 
    if (Tcl_GetInt(interp, argv[2], &min_override) != TCL_OK) return TCL_ERROR;
  if (argc > 3) 
    if (Tcl_GetInt(interp, argv[3], &max_override) != TCL_OK) return TCL_ERROR;
  if (argc > 4) 
    if (Tcl_GetInt(interp, argv[4], &nrX) != TCL_OK) return TCL_ERROR;
  if (argc > 5) 
    if (Tcl_GetInt(interp, argv[5], &nrY) != TCL_OK) return TCL_ERROR;
  if (argc > 6) 
    if (Tcl_GetInt(interp, argv[6], &nrBins) != TCL_OK) return TCL_ERROR;
  if (argc > 7) 
    if (Tcl_GetDouble(interp, argv[7], &clipLimit) != TCL_OK) 
      return TCL_ERROR;

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  s = image->data;
  npix = image->width*image->height;
  graypix = (unsigned char *) malloc(npix*sizeof(unsigned char));
  d = graypix;

  switch (image->depth) {
  case 4:
    for (i = 0; i < npix; i++, s+=4, d++) {
      d[0] = (unsigned char ) (s[0] * .299 + s[1] * .587 + s[2] * .114);
      alpha = s[3]/255.;
      d[0] = d[0]*alpha+128*(1.0-alpha);
      if (d[0] > max) max = d[0];
      if (d[0] < min) min = d[0];
    }
    break;
  case 3:
    for (i = 0; i < npix; i++, s+=3, d++) {
      d[0] = (unsigned char ) (s[0] * .299 + s[1] * .587 + s[2] * .114);
      if (d[0] > max) max = d[0];
      if (d[0] < min) min = d[0];
    }
    break;
  case 2:
    for (i = 0; i < npix; i++, s+=2, d++) {
      d[0] = s[0];
      alpha = s[1]/255.;
      d[0] = d[0]*alpha+128*(1.0-alpha);
      if (d[0] > max) max = d[0];
      if (d[0] < min) min = d[0];
    }
    break;    
  case 1:
    for (i = 0; i < npix; i++, s++, d++) {
      d[0] = s[0];
      if (d[0] > max) max = d[0];
      if (d[0] < min) min = d[0];
    }
    break;    
  default:
    Tcl_AppendResult(interp, argv[0], ": invalid image", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  /* Allow overrides of min and/or max */
  if (min_override >= 0 && min_override < 256) min = min_override;
  if (max_override >= 0 && max_override < 256) max = max_override;

  /* Now call CLAHE to equalize */
  if (CLAHE(graypix, newimage->width, newimage->height, 
	    min, max,
	    nrX, nrY, nrBins, clipLimit) < 0) {

  }

  if (image->depth == 1) {
    newimage = initimage_withvals (image->width, image->height, image->depth, 
				   imgname, graypix);
    if (!newimage) return -1;
    else return imgAddImage(interp, newimage, imgname);
  }

  newimage = initimage (image->width, image->height, image->depth, imgname);
  if (!(newimage)) {
    Tcl_AppendResult(interp, argv[0], ": error equalizing image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }

  d = newimage->data;
  s = image->data;
  switch (image->depth) {
  case 4:
    for (i = 0; i < npix; i++, d+=4, s+=4) {
      d[0] = d[1] = d[2] = graypix[i];
      d[3] = s[3];
    }
    break;
  case 3:
    for (i = 0; i < npix; i++, d+=3) {
      d[0] = d[1] = d[2] = graypix[i];
    }
    break;
  case 2:
    for (i = 0; i < npix; i++, s+=2, d+=2) {
      d[0] = graypix[i];
      d[1] = s[1];
    }
    break;    
  }

  free(graypix);

  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);
}

#if 0
static int imgShowImgCmd(ClientData clientData, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  IMG_IMAGE *image;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  
  CImg_showImg(image);

  return TCL_OK;
}
#endif

#if 0
static int imgPushCImgCmd(ClientData clientData, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  IMG_IMAGE *image;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  
  CImg_pushCImg(image);

  return TCL_OK;
}


static int imgPopCImgCmd(ClientData clientData, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  IMG_IMAGE *image;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  
  CImg_popCImg(image);

  return TCL_OK;
}
#endif

static int imgResizeImgCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  IMG_IMAGE *image, *newimage;
  int width, height;
  int interpolation = 3; /* see CImg doc for interp types - default to linear */
  static char imgname[128];
  
  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image w h [interp] [newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &width) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &height) != TCL_OK) return TCL_ERROR;

  if (argc > 4) {
    if (Tcl_GetInt(interp, argv[4], &interpolation) != TCL_OK) return TCL_ERROR;
  }

  if (argc > 5) strncpy(imgname, argv[5], 127);
  else sprintf(imgname, "img%d", imgCount++);

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = CImg_resizeImg(image, width, height, 
				  interpolation, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to resize image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);

  return TCL_OK;
}



static int imgRescaleImgCmd(ClientData clientData, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  IMG_IMAGE *image, *newimage, *dest;
  int width, height, x, y;
  int interpolation = 3;	/* default interpolation */
  static char imgname[128];
  double prop;
  int recenter = 0;
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", 
		     argv[0], " image (-)proportion [newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &prop) != TCL_OK) return TCL_ERROR;
  if (prop < 0) {
    recenter = 1;
    prop = -prop;
  }
  if (argc > 3) strncpy(imgname, argv[3], 127);
  else sprintf(imgname, "img%d", imgCount++);

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  width = prop*image->width;
  height = prop*image->height;

  if (!(newimage = CImg_resizeImg(image, width, height, interpolation, "temp"))) {
    Tcl_AppendResult(interp, argv[0], ": unable to rescaleimage ", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (!(dest = initimage(image->width, image->height, image->depth, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to create image ", imgname, 
		     (char *) NULL);
    freeimage(newimage);
    return TCL_ERROR;
  }


  if (recenter) {
    IMG_IMAGE *cropped;
    int result;
    int x0, y0, x1, y1;

    result = img_alphaBB(newimage, &x0, &y0, &x1, &y1, NULL, NULL);
    cropped = CImg_cropImg(newimage, x0, y0, x1, y1, imgname);
    freeimage(newimage);

    x = (dest->width-cropped->width)/2;
    y = (dest->height-cropped->height)/2;
    
    blt(dest, cropped, x, y, cropped->width, cropped->height);
    freeimage(cropped);
  }
  else {
    x = (dest->width-newimage->width)/2;
    y = (dest->height-newimage->height)/2;
    blt(dest, newimage, x, y, newimage->width, newimage->height);
    freeimage(newimage);
  }

  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, dest, imgname);

  return TCL_OK;
}

static int imgMirrorImgCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  char axis = (Tcl_Size) clientData;
  IMG_IMAGE *image, *newimage;
  static char imgname[128];
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image [newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (argc > 2) strncpy(imgname, argv[2], 127);
  else sprintf(imgname, "img%d", imgCount++);

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = CImg_mirrorImg(image, axis, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to rotate image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);

  return TCL_OK;
}

static int imgRotateImgCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  IMG_IMAGE *image, *newimage;
  double angle;
  int cond = 0;
  static char imgname[128];
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image angle [cond=0,1,2] [newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &angle) != TCL_OK) return TCL_ERROR;
  
  if (argc > 3) {
    if (Tcl_GetInt(interp, argv[3], &cond) != TCL_OK) return TCL_ERROR;
    if (cond < 0 || cond > 2) {
      Tcl_AppendResult(interp, argv[0], ": cond must be 0, 1, or 2", 
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  if (argc > 4) strncpy(imgname, argv[4], 127);
  else sprintf(imgname, "img%d", imgCount++);

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = CImg_rotateImg(image, angle, cond, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to rotate image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);

  return TCL_OK;
}


static int imgDrawFillImgCmd(ClientData clientData, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  IMG_IMAGE *image, *newimage;
  int r, g, b, a = 255, x, y;
  unsigned char fillcolor[4];
  static char imgname[128];
  
  if (argc < 7) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image x y r g b [alpha]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &y) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[4], &r) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[5], &g) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[6], &b) != TCL_OK) return TCL_ERROR;

  fillcolor[0] = r;
  fillcolor[1] = g;
  fillcolor[2] = b;

  if (argc > 7) { 
    if (Tcl_GetInt(interp, argv[7], &a) != TCL_OK) return TCL_ERROR;
  }
  fillcolor[3] = a;

  sprintf(imgname, "img%d", imgCount++);

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = CImg_drawFillImg(image, x, y, (unsigned char *) fillcolor, 
				    imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to fill image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);

  return TCL_OK;
}

static int imgDrawPolygonCmd (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{ 
  IMG_IMAGE *img, *newimage;
  DYN_LIST *x, *y;
  float *xvals, *yvals;
  static char imgname[32];
  int r, g, b, a = 255;
  unsigned char fillcolor[4];
  int op = (Tcl_Size) data;


  if (argc < 7) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image xs ys r g b [a]",
		     (char *) NULL);
    return TCL_ERROR;
  }
   
  if (imgFindImg(interp, argv[1], &img) != TCL_OK) return TCL_ERROR;
  sprintf(imgname, "img%d", imgCount++);

  if (tclFindDynList(interp, argv[2], &x) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[3], &y) != TCL_OK) return TCL_ERROR;

  if (Tcl_GetInt(interp, argv[4], &r) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[5], &g) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[6], &b) != TCL_OK) return TCL_ERROR;

  fillcolor[0] = r;
  fillcolor[1] = g;
  fillcolor[2] = b;

  if (argc > 7) { 
    if (Tcl_GetInt(interp, argv[7], &a) != TCL_OK) return TCL_ERROR;
  }
  fillcolor[3] = a;


  if (DYN_LIST_DATATYPE(x) != DF_FLOAT ||
      DYN_LIST_DATATYPE(y) != DF_FLOAT) {
    Tcl_AppendResult(interp, argv[0], 
		     ": x, y must be of type float",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(x) != DYN_LIST_N(y)) {
    Tcl_AppendResult(interp, argv[0], ": source lists must be same length",
		     (char *) NULL);
    return TCL_ERROR;
  }

  xvals = (float *) DYN_LIST_VALS(x);
  yvals = (float *) DYN_LIST_VALS(y);

  switch (op) {
  case IMPRO_POLY1:
    if (!(newimage = CImg_drawPolygon(img, DYN_LIST_N(x), xvals, yvals, 
				      fillcolor, 0, imgname))) {
      Tcl_AppendResult(interp, argv[0], ": unable to draw polygon ", imgname, 
		       (char *) NULL);
      return TCL_ERROR;
    }
    break;
  case IMPRO_POLY2:
    if (!(newimage = CImg_drawPolygonAlt(img, DYN_LIST_N(x), xvals, yvals, 
					 fillcolor, 0, imgname))) {
      Tcl_AppendResult(interp, argv[0], ": unable to draw polygon ", imgname, 
		       (char *) NULL);
      return TCL_ERROR;
    }
    break;
  case IMPRO_POLY3:
    if (!(newimage = CImg_fillPolygonOutside(img, DYN_LIST_N(x), xvals, yvals, 
					     fillcolor, 0, imgname))) {
      Tcl_AppendResult(interp, argv[0], ": unable to draw polygon ", imgname, 
		       (char *) NULL);
      return TCL_ERROR;
    }
    break;
  }
  /*
   * Add to hash table which contains list of open images
   */
  if (newimage != img) return imgAddImage(interp, newimage, imgname);
  else {
    Tcl_AppendResult(interp, img->name, (char *) NULL);
    return TCL_OK;
  }
}

/* img_drawPolygonChannel image xs ys r g b a channel_mask
 *
 * Filled-polygon scan-line draw that writes only the channels selected
 * by channel_mask (bit 0=R, 1=G, 2=B, 3=A; combine with OR).
 *
 * Use case: packing multiple semantic layers into the channels of a
 * single RGBA texture without later writes clobbering earlier ones.
 * Example: write planks to alpha (mask=8), zones to R (mask=1), and
 * a frame to B (mask=4), all on one image, in any order.
 */
static int imgDrawPolygonChannelCmd(ClientData data, Tcl_Interp *interp,
				    int argc, char *argv[])
{
  IMG_IMAGE *img, *newimage;
  DYN_LIST *x, *y;
  float *xvals, *yvals;
  static char imgname[32];
  int r, g, b, a;
  int channel_mask;
  unsigned char fillcolor[4];

  if (argc < 9) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " image xs ys r g b a channel_mask",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (imgFindImg(interp, argv[1], &img) != TCL_OK) return TCL_ERROR;
  sprintf(imgname, "img%d", imgCount++);

  if (tclFindDynList(interp, argv[2], &x) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[3], &y) != TCL_OK) return TCL_ERROR;

  if (Tcl_GetInt(interp, argv[4], &r) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[5], &g) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[6], &b) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[7], &a) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[8], &channel_mask) != TCL_OK) return TCL_ERROR;
  if (channel_mask < 0 || channel_mask > 15) {
    Tcl_AppendResult(interp, argv[0], ": channel_mask must be 0..15",
		     (char *) NULL);
    return TCL_ERROR;
  }

  fillcolor[0] = (unsigned char) r;
  fillcolor[1] = (unsigned char) g;
  fillcolor[2] = (unsigned char) b;
  fillcolor[3] = (unsigned char) a;

  if (DYN_LIST_DATATYPE(x) != DF_FLOAT ||
      DYN_LIST_DATATYPE(y) != DF_FLOAT) {
    Tcl_AppendResult(interp, argv[0],
		     ": x, y must be of type float",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(x) != DYN_LIST_N(y)) {
    Tcl_AppendResult(interp, argv[0],
		     ": source lists must be same length",
		     (char *) NULL);
    return TCL_ERROR;
  }

  xvals = (float *) DYN_LIST_VALS(x);
  yvals = (float *) DYN_LIST_VALS(y);

  if (!(newimage = CImg_drawPolygonChannelMask(img, DYN_LIST_N(x), xvals, yvals,
					       fillcolor, channel_mask, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to draw polygon ", imgname,
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (newimage != img) return imgAddImage(interp, newimage, imgname);
  else {
    Tcl_AppendResult(interp, img->name, (char *) NULL);
    return TCL_OK;
  }
}


static int imgDrawPolylineCmd (ClientData data, Tcl_Interp *interp,
			      int argc, char *argv[])
{ 
  IMG_IMAGE *img, *newimage;
  DYN_LIST *x, *y;
  float *xvals, *yvals;
  static char imgname[32];
  int r, g, b, a = 255;
  int pattern;
  unsigned char fillcolor[4];

  if (argc < 8) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image xs ys pattern r g b [a]",
		     (char *) NULL);
    return TCL_ERROR;
  }
   
  if (imgFindImg(interp, argv[1], &img) != TCL_OK) return TCL_ERROR;
  sprintf(imgname, "img%d", imgCount++);

  if (tclFindDynList(interp, argv[2], &x) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[3], &y) != TCL_OK) return TCL_ERROR;

  if (Tcl_GetInt(interp, argv[4], &pattern) != TCL_OK) return TCL_ERROR;

  if (Tcl_GetInt(interp, argv[5], &r) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[6], &g) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[7], &b) != TCL_OK) return TCL_ERROR;

  fillcolor[0] = r;
  fillcolor[1] = g;
  fillcolor[2] = b;

  if (argc > 8) { 
    if (Tcl_GetInt(interp, argv[8], &a) != TCL_OK) return TCL_ERROR;
  }
  fillcolor[3] = a;


  if (DYN_LIST_DATATYPE(x) != DF_FLOAT ||
      DYN_LIST_DATATYPE(y) != DF_FLOAT) {
    Tcl_AppendResult(interp, argv[0], 
		     ": x, y must be of type float",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(x) != DYN_LIST_N(y)) {
    Tcl_AppendResult(interp, argv[0], ": source lists must be same length",
		     (char *) NULL);
    return TCL_ERROR;
  }

  xvals = (float *) DYN_LIST_VALS(x);
  yvals = (float *) DYN_LIST_VALS(y);

  if (!(newimage = CImg_drawPolygon(img, DYN_LIST_N(x), xvals, yvals, 
				    fillcolor, pattern, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to draw polygon ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);
}


static int imgFillDetectImgCmd(ClientData clientData, Tcl_Interp *interp,
			       int argc, char *argv[])
{
  IMG_IMAGE *image;
  int ret = 0;

  static char imgname[128];

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }

  ret = CImg_fillDoubleDetect(image,imgname);

  Tcl_SetObjResult(interp, Tcl_NewIntObj(ret));

  return TCL_OK;
}

static int imgEdgeDetectImgCmd(ClientData clientData, Tcl_Interp *interp,
			       int argc, char *argv[])
{
  IMG_IMAGE *image, *newimage;
  int mode = 1; /* 1 or 2: with/without hysteresis */
  static char imgname[128];
  
  /* These should be args */
  double alpha = 1.0, thresL = 13.53, thresH = 13.58;
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image mode [threshL(13.5) threshH(13.6) newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &mode) != TCL_OK) return TCL_ERROR;

  if (argc > 3) {
    if (Tcl_GetDouble(interp, argv[3], &thresL) != TCL_OK) return TCL_ERROR;
  }

  if (argc > 4) {
    if (Tcl_GetDouble(interp, argv[4], &thresH) != TCL_OK) return TCL_ERROR;
  }

  if (argc > 5) {
    strncpy(imgname, argv[3], 127);
  }
  else sprintf(imgname, "img%d", imgCount++);

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = CImg_edgeDetectImg(image, mode, alpha, thresL, thresH, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to edge detect image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);

  return TCL_OK;
}




static int imgSubsampleCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  IMG_IMAGE *newimage, *image;
  static char imgname[128];
  int subsample = 2;
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image subsample [newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &subsample) != TCL_OK) return TCL_ERROR;

  if (argc > 3) strncpy(imgname, argv[3], 127);
  else sprintf(imgname, "img%d", imgCount++);

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = img_subsample(image, subsample, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to subsample image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);
}


static int imgBlendCmd(ClientData clientData, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  IMG_IMAGE *newimage, *i1, *i2, *alpha;
  static char imgname[128];

  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " i1 i2 alpha [newname]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &i1) != TCL_OK) return TCL_ERROR;
  if (imgFindImg(interp, argv[2], &i2) != TCL_OK) return TCL_ERROR;
  if (imgFindImg(interp, argv[3], &alpha) != TCL_OK) return TCL_ERROR;
  
  if (argc > 4) strncpy(imgname, argv[4], 127);
  else sprintf(imgname, "img%d", imgCount++);

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = img_blend(i1, i2, alpha, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to combine images ", 
		     i1->name, " and ", i2->name, (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);
}



static int imgMixCmd(ClientData clientData, Tcl_Interp *interp,
		     int argc, char *argv[])
{
  IMG_IMAGE *newimage, *i1, *i2;
  double mixprop;
  static char imgname[128];

  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " i1 i2 mixprop", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &i1) != TCL_OK) return TCL_ERROR;
  if (imgFindImg(interp, argv[2], &i2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &mixprop) != TCL_OK) return TCL_ERROR;

  if (argc > 4) strncpy(imgname, argv[4], 127);
  else sprintf(imgname, "img%d", imgCount++);

  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = img_mix(i1, i2, mixprop, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to combine images ", 
		     i1->name, " and ", i2->name, (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);
}


static int imgDirCmd (ClientData data, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  Tcl_HashEntry *entryPtr;
  Tcl_HashSearch searchEntry;
  Tcl_DString dirList;
  Tcl_DStringInit(&dirList);
  entryPtr = Tcl_FirstHashEntry(&imgTable, &searchEntry);
  if (entryPtr) {
    Tcl_DStringAppendElement(&dirList, Tcl_GetHashKey(&imgTable, entryPtr));
    while ((entryPtr = Tcl_NextHashEntry(&searchEntry))) {
      Tcl_DStringAppendElement(&dirList, Tcl_GetHashKey(&imgTable, entryPtr));
    }
  }
  Tcl_DStringResult(interp, &dirList);
  return TCL_OK;
}

static int imgDeleteCmd (ClientData data, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  IMG_IMAGE *image;
  Tcl_HashEntry *entryPtr;
  int i;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image(s)", (char *) NULL);
    return TCL_ERROR;
  }
  
  for (i = 1; i < argc; i++) {
    if ((entryPtr = Tcl_FindHashEntry(&imgTable, argv[i]))) {
      if ((image = Tcl_GetHashValue(entryPtr))) 
	freeimage(image);
      Tcl_DeleteHashEntry(entryPtr);
    }
    
    /* 
     * If user has specified "dg_delete ALL" iterate through the hash table
     * and recursively call this function to close all open dyngroups
     */
    else if (!strcasecmp(argv[i],"ALL")) {
      Tcl_HashSearch search;
      for (entryPtr = Tcl_FirstHashEntry(&imgTable, &search);
	   entryPtr != NULL;
	   entryPtr = Tcl_NextHashEntry(&search)) {
	Tcl_VarEval(interp, argv[0], " ", Tcl_GetHashKey(&imgTable, entryPtr),
		    (char *) NULL);
      }
      imgCount = 0;
    }
    else {
      Tcl_AppendResult(interp, argv[0], ": image \"", argv[i], 
		       "\" not found", (char *) NULL);
      return TCL_ERROR;
    }
  }
  return TCL_OK;
}

static int imgWidthCmd (ClientData data, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  IMG_IMAGE *image;
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;

  Tcl_SetObjResult(interp, Tcl_NewIntObj(image->width));

  return TCL_OK;
}

static int imgHeightCmd (ClientData data, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  IMG_IMAGE *image;
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;

  Tcl_SetObjResult(interp, Tcl_NewIntObj(image->height));

  return TCL_OK;
}

static int imgDimsCmd (ClientData data, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  IMG_IMAGE *image;
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(image->width));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(image->height));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(image->depth));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}

static int imgDepthCmd (ClientData data, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  IMG_IMAGE *image;
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;

  Tcl_SetObjResult(interp, Tcl_NewIntObj(image->depth));

  return TCL_OK;
}


static int imgInvertCmd (ClientData data, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  IMG_IMAGE *image;
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;

  img_invert(image);
  
  Tcl_SetResult(interp, image->name, TCL_STATIC);
  return TCL_OK;
}

static int imgGammaCmd (ClientData data, Tcl_Interp *interp,
			int argc, char *argv[])
{
  IMG_IMAGE *image;
  double gamma;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image gamma", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &gamma) != TCL_OK) return TCL_ERROR;

  img_gamma(image, gamma);
  
  Tcl_SetResult(interp, image->name, TCL_STATIC);
  return TCL_OK;
}

static int imgCopyAreaCmd (ClientData data, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  int mode = (Tcl_Size) data;

  IMG_IMAGE *source_img, *dest_img;
  int sx = 0;
  int sy = 0;
  int w = -1, h = -1;
  int dx, dy;

  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " source dest dx dy [sx sy w h]",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &source_img) != TCL_OK) return TCL_ERROR;
  if (imgFindImg(interp, argv[2], &dest_img) != TCL_OK) return TCL_ERROR;

  if (Tcl_GetInt(interp, argv[3], &dx) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[4], &dy) != TCL_OK) return TCL_ERROR;

  if (argc > 5)
    if (Tcl_GetInt(interp, argv[5], &sx) != TCL_OK) return TCL_ERROR;
  if (argc > 6)
    if (Tcl_GetInt(interp, argv[6], &sy) != TCL_OK) return TCL_ERROR;
    
  if (argc > 7)
    if (Tcl_GetInt(interp, argv[7], &w) != TCL_OK) return TCL_ERROR;
  if (argc > 8)
    if (Tcl_GetInt(interp, argv[8], &h) != TCL_OK) return TCL_ERROR;
    
  if (w == -1) 
    w = source_img->width - sx;
  if (h == -1) 
    h = source_img->height - sy;
  
  switch (mode) {
  case COPY_AREA:
    img_copyarea(source_img, dest_img, sx, sy, w, h, dx, dy);
    break;
  case BLEND_AREA:
    img_blendarea(source_img, dest_img, sx, sy, w, h, dx, dy);
    break;
  }
  Tcl_SetResult(interp, dest_img->name, TCL_STATIC);
  return TCL_OK;
}

static int imgMakeObjCmd (ClientData data, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  int i,j;
  IMG_IMAGE *image;
  IMG_IMAGE *imageR;
  IMG_IMAGE *imageG;
  IMG_IMAGE *imageB;

  if (argc < 2 && argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image_(R)", 
		     " image_G", " image_B", " new_name", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (argc == 5) {
    if (imgFindImg(interp, argv[1], &imageR) != TCL_OK) return TCL_ERROR;
    if (imgFindImg(interp, argv[2], &imageG) != TCL_OK) return TCL_ERROR;
    if (imgFindImg(interp, argv[3], &imageB) != TCL_OK) return TCL_ERROR;
    
    if((imageR->size != imageG->size) || 
       (imageR->size != imageB->size) || 
       (imageG->size != imageB->size)) {
      Tcl_AppendResult(interp, argv[0], ": unable to combine images ", 
		       imageR->name, imageG->name, imageB->name, 
		       (char *) NULL);
      return TCL_ERROR;
    }
    
    if (!(image = initimage(imageR->width, imageR->height, imageR->depth, 
			    argv[4]))) {
      Tcl_AppendResult(interp, argv[0], ": unable to create image ", 
		       imageR->name,(char *) NULL);
      return TCL_ERROR;
    }
    image->depth = 3;
    image->size = image->width*image->height*image->depth;
    image->data = (byte *)calloc(image->size, sizeof(byte));
    for (j = 0, i = 0; i < (int) imageR->size; i++, j+=3) {
      image->data[j]   = imageR->data[i];
      image->data[j+1] = imageG->data[i];
      image->data[j+2] = imageB->data[i];
    }
    imgAddImage(interp, image, argv[4]);
  } else {
    if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
    image->depth = 1;
  }    

  {
    DYN_LIST *newlist = imageMakeDynList(image);
    
    if (!newlist) {
      Tcl_AppendResult(interp, argv[0], ": error making image dynlist",
		       (char *) NULL);
      return TCL_ERROR;
    }
    else {
      return(tclPutList(interp, newlist));
    }
  }
}


static int imgFilterImgCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  IMG_IMAGE *image, *newimage;
  static char imgname[128];
  
  /* These should be args */
  int rmin, rmax;
  
  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image low high", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &rmin) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &rmax) != TCL_OK) return TCL_ERROR;


  if (!(image->width == image->height)) {
    Tcl_AppendResult(interp, argv[0], ": image \"", argv[1], 
		     "\" must be square to be filtered", (char *) NULL);
    return TCL_ERROR;
  }


  if ((image->width != 32) &&
      (image->width != 64) &&
      (image->width != 128) &&
      (image->width != 256) &&
      (image->width != 512) &&
      (image->width != 1024) &&
      (image->width != 2048) &&
      (image->width != 4096)) {
    Tcl_AppendResult(interp, argv[0], ": image \"", argv[1], 
		     "\" must be power of two", (char *) NULL);
    return TCL_ERROR;
  }
  
  snprintf(imgname, sizeof(imgname), "img%d", imgCount++);
  
  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = CImg_fftImg(image, rmin, rmax, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to filter image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);
}



static int imgBlurImgCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  IMG_IMAGE *image, *newimage;
  static char imgname[128];
  
  /* These should be args */
  double sigma;
  int boundary = 1, is_gaussian = 0;
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " image sigma [boundary] [is_gaussian]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &sigma) != TCL_OK) return TCL_ERROR;

  if (argc > 3)
    if (Tcl_GetInt(interp, argv[3], &boundary) != TCL_OK) return TCL_ERROR;

  if (argc > 4)
    if (Tcl_GetInt(interp, argv[4], &is_gaussian) != TCL_OK) return TCL_ERROR;
  
  sprintf(imgname, "img%d", imgCount++);
  
  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (!(newimage = CImg_blurImg(image, sigma,
				boundary, is_gaussian, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to blur image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /*
   * Add to hash table which contains list of open images
   */
  return imgAddImage(interp, newimage, imgname);
}



/*****************************************************************************
 *
 *                           DRAWING FUNCTIONS
 *
 *****************************************************************************/


static int imgBkgFillCmd (ClientData data, Tcl_Interp *interp,
			  int argc, char *argv[])
{ 
  IMG_IMAGE *img;
  int bgvalue = 0;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], "fillcolor", (char *) NULL);
    return TCL_ERROR;
  }
  if (imgFindImg(interp, argv[1], &img) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &bgvalue) != TCL_OK) return TCL_ERROR;
  
  writerect(img, 0, 0, img->width, img->height, bgvalue, 0);
  return TCL_OK;
}


static int imgWriteCirclesCmd (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{ 
  int i, j;
  IMG_IMAGE *img;
  DYN_LIST *x, *y, *radii, *vals = NULL, *tr = NULL;
  int value = 255;
  double trans = 0.0;

  int cx, cy, r, v;
  int *xvals, *yvals, *rvals, *vvals;
  float *tvals;
  float t;
    
  
  if (argc < 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image x(s) y(s) rad(s) ",
		     "[val(s) tr(s)] [-val val] [-trans tr]", 
		     (char *) NULL);
    return TCL_ERROR;
  }

  /* This is a nasty command parsing loop, looking for option pairs */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i],"-val")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no value(s) specified", (char *) NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[i+1], &value) != TCL_OK) return TCL_ERROR;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-trans")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no transparency specified", (char *) NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetDouble(interp, argv[i+1], &trans) != TCL_OK) 
	  return TCL_ERROR;
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, argv[0], 
			 ": bad option ", argv[i], (char *) NULL);
	return TCL_ERROR;
      }
    }
  }

  if (imgFindImg(interp, argv[1], &img) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &x) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[3], &y) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[4], &radii) != TCL_OK) return TCL_ERROR;
  if (argc > 5) {
    if (tclFindDynList(interp, argv[5], &vals) != TCL_OK) return TCL_ERROR;
  }
  if (argc > 6) {
    if (tclFindDynList(interp, argv[6], &tr) != TCL_OK) return TCL_ERROR;
  }

  if (DYN_LIST_DATATYPE(x) != DF_LONG ||
      DYN_LIST_DATATYPE(y) != DF_LONG ||
      DYN_LIST_DATATYPE(radii) != DF_LONG) {
    Tcl_AppendResult(interp, argv[0], 
		     ": x y positions and radii must be of type int",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(x) != DYN_LIST_N(y)) {
    Tcl_AppendResult(interp, argv[0], ": unequal x and y positions",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(radii) != 1 && DYN_LIST_N(radii) != DYN_LIST_N(x)) {
    Tcl_AppendResult(interp, argv[0], ": wrong number of radii specified",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (vals && DYN_LIST_DATATYPE(vals) != DF_LONG) {
    Tcl_AppendResult(interp, argv[0], ": vals must be of type int",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (tr && DYN_LIST_DATATYPE(tr) != DF_FLOAT) {
    Tcl_AppendResult(interp, argv[0], ": vals must be of type float",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (vals) {
    if (DYN_LIST_N(vals) != 1 && DYN_LIST_N(vals) != DYN_LIST_N(x)) {
      Tcl_AppendResult(interp, argv[0], ": wrong number of vals specified",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  if (tr) {
    if (DYN_LIST_N(tr) != 1 && DYN_LIST_N(tr) != DYN_LIST_N(x)) {
      Tcl_AppendResult(interp, argv[0], ": wrong number of vals specified",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }
  
  xvals = (int *) DYN_LIST_VALS(x);
  yvals = (int *) DYN_LIST_VALS(y);
  rvals = (int *) DYN_LIST_VALS(radii);
  if (vals) vvals = (int *) DYN_LIST_VALS(vals);
  if (tr) tvals = (float *) DYN_LIST_VALS(tr);

  for (i = 0; i < DYN_LIST_N(x); i++) {
    /* Set the params for this pass */
    cx = xvals[i];
    cy = yvals[i];

    if (DYN_LIST_N(radii) == 1) r = rvals[0];
    else r = rvals[i];

    if (!vals) v = value;
    else if (DYN_LIST_N(vals) == 1) v = vvals[0];
    else v = vvals[i];

    if (!tr) t = trans;
    else if (DYN_LIST_N(tr) == 1) t = tvals[0];
    else t = tvals[i];

    /* And *finally* do the work! */

    writecirc(img, cx, cy, r, v, t);
  }
  Tcl_SetResult(interp, argv[1], TCL_STATIC);
  return TCL_OK;
}

static int imgWriteRectsCmd (ClientData data, Tcl_Interp *interp,
			     int argc, char *argv[])
{ 
  int i;
  IMG_IMAGE *img;
  DYN_LIST *x, *y, *dx, *dy, *vals;

  int cx, cy, cdx, cdy, cv;
  int *xvals, *yvals, *dxvals, *dyvals, *vvals;

  double trans = 0.0;

  if (argc < 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " image x(s) y(s) dx(s) dy(s) val(s)",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (imgFindImg(interp, argv[1], &img) != TCL_OK) return TCL_ERROR;

  if (tclFindDynList(interp, argv[2], &x) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[3], &y) != TCL_OK) return TCL_ERROR;

  if (tclFindDynList(interp, argv[4], &dx) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[5], &dy) != TCL_OK) return TCL_ERROR;

  if (tclFindDynList(interp, argv[6], &vals) != TCL_OK) return TCL_ERROR;


  if (DYN_LIST_DATATYPE(x) != DF_LONG ||
      DYN_LIST_DATATYPE(y) != DF_LONG ||
      DYN_LIST_DATATYPE(dx) != DF_LONG ||
      DYN_LIST_DATATYPE(dy) != DF_LONG ||
      DYN_LIST_DATATYPE(vals) != DF_LONG) {
    Tcl_AppendResult(interp, argv[0], 
		     ": x, y, dx, dy and valsmust be of type int",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(x) != DYN_LIST_N(y)) {
    Tcl_AppendResult(interp, argv[0],
		     ": x and y position lists must be the same length",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (DYN_LIST_N(dx) != 1 && DYN_LIST_N(dx) != DYN_LIST_N(x)) {
    Tcl_AppendResult(interp, argv[0], ": wrong number of dx specified",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(dy) != 1 && DYN_LIST_N(dy) != DYN_LIST_N(x)) {
    Tcl_AppendResult(interp, argv[0], ": wrong number of dy specified",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(vals) != 1 && DYN_LIST_N(vals) != DYN_LIST_N(x)) {
    Tcl_AppendResult(interp, argv[0], ": wrong number of vals specified",
		     (char *) NULL);
    return TCL_ERROR;
  }    

  
  xvals = (int *) DYN_LIST_VALS(x);
  yvals = (int *) DYN_LIST_VALS(y);
  dxvals = (int *) DYN_LIST_VALS(dx);
  dyvals = (int *) DYN_LIST_VALS(dy);
  vvals = (int *) DYN_LIST_VALS(vals);

  for (i = 0; i < DYN_LIST_N(x); i++) {
    /* Set the params for this pass */
    cx = xvals[i];
    cy = yvals[i];

    if (DYN_LIST_N(dx) == 1) cdx = dxvals[0];
    else cdx = dxvals[i];
    if (DYN_LIST_N(dy) == 1) cdy = dyvals[0];
    else cdy = dyvals[i];
    if (DYN_LIST_N(vals) == 1) cv = vvals[0];
    else cv = vvals[i];

    /* And *finally* do the work! */


    writerect(img, cx, cy, cdx, cdy, cv, trans);
  }
  
  Tcl_SetResult(interp, argv[1], TCL_STATIC);
  return TCL_OK;
}


static int imgWriteElementsCmd (ClientData data, Tcl_Interp *interp,
			    int argc, char *argv[])
{ 
  int i;
  IMG_IMAGE *img;
  DYN_LIST *x, *y, *element;
  int cx, cy, dx, dy;
  int *xvals, *yvals, *evals;
    
  
  if (argc < 6) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image x y element dx dy",
		     (char *) NULL);
    return TCL_ERROR;
  }
   
  if (imgFindImg(interp, argv[1], &img) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[2], &x) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[3], &y) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[4], &element) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[5], &dx) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[6], &dy) != TCL_OK) return TCL_ERROR;

  if (DYN_LIST_DATATYPE(x) != DF_LONG ||
      DYN_LIST_DATATYPE(y) != DF_LONG ||
      DYN_LIST_DATATYPE(element) != DF_LONG) {
    Tcl_AppendResult(interp, argv[0], 
		     ": x y positions and element must be of type int",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(x) != DYN_LIST_N(y)) {
    Tcl_AppendResult(interp, argv[0], ": unequal x and y positions",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(element) != dx*dy) {
    Tcl_AppendResult(interp, argv[0], ": error in element dimensions",
		     (char *) NULL);
    return TCL_ERROR;
  }
  xvals = (int *) DYN_LIST_VALS(x);
  yvals = (int *) DYN_LIST_VALS(y);
  evals = (int *) DYN_LIST_VALS(element);

  for (i = 0; i < DYN_LIST_N(x); i++) {
    /* Set the params for this pass */
    cx = xvals[i];
    cy = yvals[i];
    
    /* And *finally* do the work! */
    
    writeregion(img, cx, cy, evals, dx, dy);
  }
  Tcl_SetResult(interp, argv[1], TCL_STATIC);
  return TCL_OK;
}

static int imgComposeElementsCmd (ClientData data, Tcl_Interp *interp,
				  int argc, char *argv[])
{ 
  int i;
  IMG_IMAGE *img;
  DYN_LIST *x, *y, *wl, *hl, *element, **elists;
  int cx, cy, dx, dy;
  int *xvals, *yvals, *widths, *heights, *evals;
  static char imgname[32];
    
  if (argc < 6) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image x y element dx dy",
		     (char *) NULL);
    return TCL_ERROR;
  }
   
  if (imgFindImg(interp, argv[1], &img) != TCL_OK) return TCL_ERROR;
  strncpy(imgname, argv[1], 31);

  if (tclFindDynList(interp, argv[2], &x) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[3], &y) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[4], &element) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[5], &wl) != TCL_OK) return TCL_ERROR;
  if (tclFindDynList(interp, argv[6], &hl) != TCL_OK) return TCL_ERROR;

  if (DYN_LIST_DATATYPE(x) != DF_LONG ||
      DYN_LIST_DATATYPE(y) != DF_LONG ||
      DYN_LIST_DATATYPE(wl) != DF_LONG ||
      DYN_LIST_DATATYPE(hl) != DF_LONG ||
      DYN_LIST_DATATYPE(element) != DF_LIST) {
    Tcl_AppendResult(interp, argv[0], 
		     ": x, y, widths, heights, must be of type int",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (DYN_LIST_N(x) != DYN_LIST_N(y) ||
      DYN_LIST_N(x) != DYN_LIST_N(wl) ||
      DYN_LIST_N(x) != DYN_LIST_N(hl) ||
      DYN_LIST_N(x) != DYN_LIST_N(element)) {
    Tcl_AppendResult(interp, argv[0], ": source lists must be same length",
		     (char *) NULL);
    return TCL_ERROR;
  }
  xvals = (int *) DYN_LIST_VALS(x);
  yvals = (int *) DYN_LIST_VALS(y);
  widths = (int *) DYN_LIST_VALS(wl);
  heights = (int *) DYN_LIST_VALS(hl);
  elists = (DYN_LIST **) DYN_LIST_VALS(element);

  for (i = 0; i < DYN_LIST_N(element); i++) {
    if (DYN_LIST_DATATYPE(elists[i]) != DF_LONG) {
      Tcl_AppendResult(interp, argv[0], 
		       ": source elements should be of type int",
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (DYN_LIST_N(elists[i]) != widths[i]*heights[i]) {
      Tcl_AppendResult(interp, argv[0], ": error in element dimensions",
		       (char *) NULL);
      return TCL_ERROR;
    }
  }

  for (i = 0; i < DYN_LIST_N(x); i++) {
    /* Set the params for this pass */
    dx = widths[i];
    dy = heights[i];
    cx = xvals[i];
    cy = yvals[i];
    evals = (int *) (DYN_LIST_VALS(elists[i]));

    /* And *finally* do the work! */
    
    writemask(img, cx, cy, evals, dx, dy);
  }
  Tcl_SetResult(interp, imgname, TCL_STATIC);
  return TCL_OK;
}


static int imgMkGaussCmd (ClientData data, Tcl_Interp *interp,
			int argc, char *argv[])
{
  IMG_IMAGE *image;
  int background = 0, maxval = 255;
  double radius;

  if (argc < 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " image radius background maxval", (char *) NULL);
    return TCL_ERROR;
  }
  
  if (imgFindImg(interp, argv[1], &image)        != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &radius)    != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &background)   != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[4], &maxval)       != TCL_OK) return TCL_ERROR;
  
  mkgauss(image, radius, background, maxval);

  Tcl_SetResult(interp, argv[1], TCL_VOLATILE);
  
  return(TCL_OK);
}



static int imgMkSineCmd (ClientData data, Tcl_Interp *interp,
			int argc, char *argv[])
{
  IMG_IMAGE *image;
  int h = -1, w = -1, o = -1, mn = -1, md = -1, wavelen = -1;
  double phase = 0.0;

  if (argc < 8) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image width height ",
		     "ori (0-7)", "wavelen", "meanval", "mod", "[phase=0]", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (imgFindImg(interp, argv[1], &image)   != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &w)       != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &h)       != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[4], &o)       != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[5], &wavelen) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[6], &mn)      != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[7], &md)      != TCL_OK) return TCL_ERROR;
  if (argc > 8) {
    if (Tcl_GetDouble(interp, argv[8], &phase) != TCL_OK) return TCL_ERROR;
  }
  
  mksine(image, w, h, o, wavelen, mn, md, phase);

  Tcl_SetResult(interp, argv[1], TCL_VOLATILE);

  return(TCL_OK);
}




/*****************************************************************************
 *
 * FUNCTION
 *    imgWriteCmd
 *
 * ARGS
 *    Tcl Args
 *
 * TCL FUNCTION
 *    img_write
 *
 * DESCRIPTION
 *    Call imgWrite with appropriate args
 *
 *****************************************************************************/

static int imgWriteCmd (ClientData data, Tcl_Interp *interp,
			int argc, char *argv[])
{
  int i, j;
  IMG_IMAGE *img;
  Tcl_Channel outChannel = Tcl_GetStdChannel(TCL_STDOUT);
  int channel_opened = 0;

  /* This is an ugly command parsing loop that searches for option pairs */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (!strcmp(argv[i],"-channel")) {
	int mode;
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no output channel specified", (char *) NULL);
	  goto error;
	}
	if (channel_opened) Tcl_Close(interp, outChannel);
	if ((outChannel = Tcl_GetChannel(interp, argv[i+1], &mode)) == NULL) {
	  goto error;
	}
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else if (!strcmp(argv[i],"-output") ||
	       !strcmp(argv[i],"-out") ||
	       !strcmp(argv[i],"-o")) {
	if (i+1 == argc) {
	  Tcl_AppendResult(interp, argv[0], 
			   ": no output filename specified", (char *) NULL);
	  goto error;
	}
	if (!channel_opened) {
	  outChannel = Tcl_OpenFileChannel(interp, argv[i+1], "w", 0664);
	  if (!outChannel) return TCL_ERROR;
	  else channel_opened = 1;
	}
	for (j = i+2; j < argc; j++) argv[j-2] = argv[j];
	argc-=2;
	i-=1;
      }
      else {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, argv[0], 
			 ": bad option ", argv[i], (char *) NULL);
	goto error;
      }
    }
  }
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " [-output name] [-channel channel_id] imgname(s)",
		     (char *) NULL);
    goto error;
  }

  for (i = 1; i < argc; i++) {
    if (imgFindImg(interp, argv[i], &img) != TCL_OK) goto error;
    if ((unsigned) Tcl_Write(outChannel,
			     (const char *) img->data, img->size) != img->size) {
      Tcl_AppendResult(interp, argv[0], ": write error", (char *) NULL);
      goto error;
    }
  }
  
  if (channel_opened) Tcl_Close(interp, outChannel);
  return TCL_OK;

 error:
  if (channel_opened) Tcl_Close(interp, outChannel);
  return TCL_ERROR;
}



static int tclFindImageData(Tcl_Interp *interp, char *name, 
			    int *w, int *h, int *d, unsigned char **data)
{
  DYN_LIST *dl;
  DYN_LIST **sublists;
  int temp;
  if (tclFindDynList(interp, name, &dl) != TCL_OK) return TCL_ERROR;

  if ((*w != 0) && (*h != 0)) {		/* user specified the dims */
    int size = (*w) * (*h);
    if (DYN_LIST_DATATYPE(dl) != DF_CHAR) {
      Tcl_AppendResult(interp, "dynlist images must be chars", NULL);
      return TCL_ERROR;
    }
    if (DYN_LIST_N(dl) % size) {
      Tcl_AppendResult(interp, "find image: invalid size specified", NULL);
      return TCL_ERROR;
    }
    else *d = DYN_LIST_N(dl)/size;
    *data = (unsigned char *) DYN_LIST_VALS(dl);
    return TCL_OK;
  }
  

  if (DYN_LIST_DATATYPE(dl) != DF_LIST || DYN_LIST_N(dl) != 2) {
    Tcl_AppendResult(interp, "invalid dynlist (specify dims?)", NULL);
    return TCL_ERROR;
  }

  sublists = (DYN_LIST **) DYN_LIST_VALS(dl);

  if (DYN_LIST_DATATYPE(sublists[0]) != DF_LONG ||
      DYN_LIST_N(sublists[0]) != 2) {
    Tcl_AppendResult(interp, "image header must include w & h (ints)", NULL);
    return TCL_ERROR;
  }
  else {
    int *header = (int *) DYN_LIST_VALS(sublists[0]);
    *w = header[0];
    *h = header[1];
  }

  if (DYN_LIST_DATATYPE(sublists[1]) != DF_CHAR) {
    Tcl_AppendResult(interp, "image data must be chars", NULL);
    return TCL_ERROR;
  }
  
  temp = DYN_LIST_N(sublists[1])/((*w)*(*h));

  switch (temp) {
  case 1:
  case 3:
  case 4:
    if ((temp*(*w)*(*h)) != DYN_LIST_N(sublists[1])) {
      char sizes[64];
      sprintf(sizes, "[%dx%dx%d != %d]", *w, *h, temp,DYN_LIST_N(sublists[1]));
      Tcl_AppendResult(interp, "image data size mismatch ", sizes, NULL);
      return TCL_ERROR;
    }
    *d = temp;
    *data = (unsigned char *) DYN_LIST_VALS(sublists[1]);
    return TCL_OK;
    break;
  default:
    {
      char sizes[64];
      sprintf(sizes, "[w=%d, h=%d, n=%d]", *w, *h, DYN_LIST_N(sublists[1]));
      Tcl_AppendResult(interp, "image data size mismatch ", sizes, NULL);
      return TCL_ERROR;
    }
  }
}


static int imgImageFromListCmd(ClientData clientData, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  int nbytes;
  IMG_IMAGE *newimage;
  static char imgname[128];
  unsigned char *imagedata, *pixels;
  int w = 0, h = 0, d = 0;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], 
		     " pixels [w [h]]", (char *) NULL);
    return TCL_ERROR;
  }

  sprintf(imgname, "img%d", imgCount++);
  if (imgFindImg(interp, imgname, NULL) == TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": image \"", imgname, 
		     "\" already exists", (char *) NULL);
    return TCL_ERROR;
  }
  if (argc > 2) {
    if (argc == 3) {
      Tcl_AppendResult(interp, argv[0], 
		       ": width and height must be specified", (char *) NULL);
      return TCL_ERROR;

    }
    if (Tcl_GetInt(interp, argv[2], &w) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[3], &h) != TCL_OK) return TCL_ERROR;
  }
  if (tclFindImageData(interp, argv[1], &w, &h, &d, &imagedata) != TCL_OK) 
    return TCL_ERROR;
  
  /* Copy pixel data into our image */
  nbytes = w*h*d;
  pixels = calloc(nbytes, sizeof(unsigned char));
  memcpy(pixels, imagedata, nbytes);
  
  if (!(newimage = initimage_withvals(w, h, d, imgname, pixels))) {
    Tcl_AppendResult(interp, argv[0], ": unable to create image ", imgname, 
		     (char *) NULL);
    free(pixels);
    return TCL_ERROR;
  }

  return imgAddImage(interp, newimage, imgname);
}




/*read the information from the header and store it in the LodePNG_Info. return value is error*/
static int PNG_GetInfo(const unsigned char* in, size_t inlength, int *w, int *h, int *bitDepth, int *colorType)
{
  extern unsigned LodePNG_read32bitInt(const unsigned char* buffer);

  if(inlength == 0 || in == 0) return -1;
  if(inlength < 29) return -1;

  if(in[0] != 137 || in[1] != 80 || in[2] != 78 || in[3] != 71
     || in[4] != 13 || in[5] != 10 || in[6] != 26 || in[7] != 10)
  {
    return -1;
  }
  if(in[12] != 'I' || in[13] != 'H' || in[14] != 'D' || in[15] != 'R')
  {
    return -1;
  }

  /*read the values given in the header*/
  if (w) *w = LodePNG_read32bitInt(&in[16]);
  if (h) *h = LodePNG_read32bitInt(&in[20]);
  if (bitDepth) *bitDepth = in[24];
  if (colorType) *colorType = in[25];
  return 0;
}

static int imgImageFromPNGCmd(ClientData cdata, Tcl_Interp * interp, 
			      int objc, Tcl_Obj * const objv[])
{
  IMG_IMAGE *newimage;
  static char imgname[32];
  unsigned char *pixeldata;	/* returned from decode */
  unsigned int w, h;
  int d = 4, size;
  int colorType;
  
  unsigned char *pngdata;
  Tcl_Size length;
  int outputType;
  int encode64 = 0;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "PNGdata [depth]");
    return TCL_ERROR;
  }
  

  /* called tcl proc name specifies output type */
  if (!strcmp(Tcl_GetString(objv[0]),"img_imgfromPNG")) {
    outputType =  IMPRO_PNG_TO_IMG;		/* impro img */
  }
  else {
    outputType = IMPRO_PNG_TO_DYNLIST;		/* [dl_llist "w h" pix] */
  }  

  if (((Tcl_Size) cdata) == IMPRO_FROM_BASE64) 
    encode64 = 1;

  if (!encode64)
    pngdata = (unsigned char *) Tcl_GetByteArrayFromObj(objv[1], &length);
  else
    pngdata = (unsigned char *) Tcl_GetStringFromObj(objv[1], &length);
  
  if (!pngdata) {
    Tcl_AppendResult(interp, "img_imgfromPNG: invalid png data", NULL);
    return TCL_ERROR;
  }

  /* Get depth info */
  PNG_GetInfo(pngdata, length, NULL, NULL, NULL, &colorType);
  switch(colorType)
    {
    case 0: d = 1; break; /*grey*/
    case 2: d = 3; break; /*RGB*/
    case 3: d = 1; break; /*palette*/
    case 4: d = 2; break; /*grey + alpha*/
    case 6: d = 4; break; /*RGBA*/
    }
  
  if (!encode64) {
    switch (d) {
    case 4:
      LodePNG_decode32(&pixeldata, &w, &h, pngdata, length);
      break;
    case 3:
      LodePNG_decode24(&pixeldata, &w, &h, pngdata, length);
      break;
    default:
      Tcl_AppendResult(interp, "img_imgfromPNG: png filetype not supported", NULL);
      return TCL_ERROR;
    }
  }
  else {
    unsigned char *decoded_data;
    unsigned int decoded_length = length;
    int result;

    decoded_data = calloc(decoded_length, sizeof(char));
    result = base64decode((char *) pngdata, length, decoded_data, &decoded_length);

    if (result) {
      free(decoded_data);
      char resultstr[64];
      snprintf(resultstr, sizeof(resultstr),
	       "img_imgfromPNG: error decoding data (%d/%d bytes)", 
	       (int) length, decoded_length);
      Tcl_SetResult(interp, resultstr, TCL_VOLATILE);
      return TCL_ERROR;
    }

    switch (d) {
    case 4:
      LodePNG_decode32(&pixeldata, &w, &h, decoded_data, decoded_length);
      break;
    case 3:
      LodePNG_decode24(&pixeldata, &w, &h, decoded_data, decoded_length);
      break;
    default:
      free(decoded_data);
      Tcl_AppendResult(interp, "img_imgfromPNG: png filetype not supported", NULL);
      return TCL_ERROR;
    }

    free(decoded_data);
  }

  if (outputType == IMPRO_PNG_TO_IMG) {
    sprintf(imgname, "img%d", imgCount++);
    if (!(newimage = initimage_withvals(w, h, d, imgname, pixeldata))) {
      Tcl_AppendResult(interp, Tcl_GetString(objv[0]), ": unable to create image ", imgname, 
		       (char *) NULL);
      free(pixeldata);
      return TCL_ERROR;
    }
    return imgAddImage(interp, newimage, imgname);
  }
  else {
    DYN_LIST *newlist = dfuCreateDynList(DF_LIST, 2); 
    DYN_LIST *pixlist = dfuCreateDynListWithVals(DF_CHAR, w*h*d, pixeldata);
    DYN_LIST *header = dfuCreateDynList(DF_LONG, 2); 
    dfuAddDynListLong(header, w);
    dfuAddDynListLong(header, h);
    dfuMoveDynListList(newlist, header);
    dfuMoveDynListList(newlist, pixlist);
    
    return(tclPutList(interp, newlist));
  }
}



/*************************************************************************
 *         This function takes as an argument the image and returns 
 *         a dynamic list of integers, for now you have to figure out the
 *         dimensions yourself.
 *************************************************************************/

static int imgImageToListCmd  (ClientData data, Tcl_Interp *interp,
			int argc, char *argv[])
{
  IMG_IMAGE *image;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " image ", (char *) NULL);
    return TCL_ERROR;
  }

  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  
  {
    DYN_LIST *newlist = imageMakeDynList(image);
    if (!newlist) {
      Tcl_AppendResult(interp, argv[0], ": error making image dynlist",
		       (char *) NULL);
      return TCL_ERROR;
    }
    else {
      return(tclPutList(interp, newlist));
    }
  }
}


static int imgInfoCmd(ClientData clientData, Tcl_Interp *interp,
			int argc, char *argv[])
{
  int w = 0, h = 0, d = 0, header_bytes;
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], "filename", NULL);
    return TCL_ERROR;
  }
  
  if (strstr(argv[1], ".tga")) {
    tga_image img;
    if (tga_read(&img, argv[1]) != TGA_NOERR) {
      Tcl_AppendResult(interp, argv[0],
		       ": unable to determine dims for \"", argv[1] ,"\"", NULL);
      return(TCL_ERROR);
    }


    Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(img.width));
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(img.height));
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(img.pixel_depth/8));
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(0));
    Tcl_SetObjResult(interp, listPtr);
    
    tga_free_buffers(&img);
    return(TCL_OK);
  }


  else if (strstr(argv[1], ".png")) {
    unsigned char* buffer;
    size_t buffersize;
    unsigned error;
    int bitDepth;
    int colorType;
    extern unsigned LodePNG_loadFile(unsigned char** out, size_t* outsize, const char* filename);

    error = LodePNG_loadFile(&buffer, &buffersize, argv[1]);
    if (error) {
    errorReturn:
      Tcl_AppendResult(interp, argv[0],
		       ": unable to determine dims for \"", argv[1] ,"\"", NULL);
      return(TCL_ERROR);
    }
    
    error = PNG_GetInfo(buffer, buffersize, &w, &h, &bitDepth, &colorType); 
    if (error) goto errorReturn;
    free(buffer);
    
    switch(colorType)
      {
      case 0: d = 1; /*grey*/
      case 2: d = 3; /*RGB*/
      case 3: d = 1; /*palette*/
      case 4: d = 2; /*grey + alpha*/
      case 6: d = 4; /*RGBA*/
      }

    Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(w));
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(h));
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(d));
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(0));
    Tcl_SetObjResult(interp, listPtr);
  
    return TCL_OK;
  }

  if (argc > 2) {
    if (Tcl_GetInt(interp, argv[2], &w) != TCL_OK) return TCL_ERROR;
  }
  if (argc > 3) {
    if (Tcl_GetInt(interp, argv[3], &h) != TCL_OK) return TCL_ERROR;
  }
  if (argc > 4) {
    if (Tcl_GetInt(interp, argv[4], &d) != TCL_OK) return TCL_ERROR;
  }

  if (!raw_getImageDims(argv[1], &w, &h, &d, &header_bytes)) {
    Tcl_AppendResult(interp, argv[0],
		     ": unable to determine dims for \"", argv[1] ,"\"", NULL);
    return(TCL_ERROR);
  }

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(w));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(h));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(d));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(header_bytes));
  Tcl_SetObjResult(interp, listPtr);

  return(TCL_OK);
}


static int imgReadCmd(ClientData clientData, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], "filename", NULL);
    return TCL_ERROR;
  }

  if (strstr(argv[1], ".tga")) {
    return imgReadTargaCmd(clientData, interp, argc, argv);
  }
  else if (strstr(argv[1], ".png")) {
    return imgReadPNGCmd(clientData, interp, argc, argv);
  }
  else if (strstr(argv[1], ".raw")) {
    return imgReadRawCmd(clientData, interp, argc, argv);
  }
  else {
    Tcl_SetResult(interp, "img_read: unsupported file format", TCL_STATIC);
    return TCL_ERROR;
  }
}

static int imgReadRawCmd(ClientData clientData, Tcl_Interp *interp,
			 int argc, char *argv[])
{
  FILE *infp;
  unsigned char *pixels;
  DYN_LIST *newlist;
  int w = 0, h = 0, d = 0, header_bytes;
  unsigned int size;
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], "filename [w] [h] [d]", NULL);
    return TCL_ERROR;
  }

  if (argc > 2) {
    if (Tcl_GetInt(interp, argv[2], &w) != TCL_OK) return TCL_ERROR;
  }
  if (argc > 3) {
    if (Tcl_GetInt(interp, argv[3], &h) != TCL_OK) return TCL_ERROR;
  }
  if (argc > 4) {
    if (Tcl_GetInt(interp, argv[4], &d) != TCL_OK) return TCL_ERROR;
  }

  if (!raw_getImageDims(argv[1], &w, &h, &d, &header_bytes)) {
    Tcl_AppendResult(interp, argv[0],
		     ": unable to determine dims for \"", argv[1] ,"\"", NULL);
    return(TCL_ERROR);
  }
  
  size = w*h*d;
  pixels = malloc(size);
  if (!pixels) {
    Tcl_AppendResult(interp, argv[0], ": error allocating memory", NULL);
    return TCL_ERROR;
  }

  infp = fopen(argv[1], "rb");
  if (header_bytes) fseek(infp, header_bytes, SEEK_SET);
  if (fread(pixels, 1, size, infp) != size) {
    Tcl_AppendResult(interp, argv[0], ": error reading raw file", NULL);
    fclose(infp);
    free(pixels);
    return TCL_ERROR;
  }
  fclose(infp);

  newlist = dfuCreateDynListWithVals(DF_CHAR, size, pixels);
  return(tclPutList(interp, newlist));
}

static int imgReadTargaCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  DYN_LIST *newlist;
  int w = 0, h = 0, d = 0;
  unsigned int size;

  tga_image img;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], "filename", NULL);
    return TCL_ERROR;
  }

  if (tga_read(&img, argv[1]) != TGA_NOERR) {
    Tcl_AppendResult(interp, argv[0],
		     ": error loading file \"", argv[1] ,"\"", NULL);
    return(TCL_ERROR);
  }
  
  d = img.pixel_depth/8;
  w = img.width;
  h = img.height;
  size = d*w*h;

  if (!tga_is_top_to_bottom(&img)) tga_flip_vert(&img);
  //  if (tga_is_right_to_left(&img)) tga_flip_horiz(&img);
  tga_swap_red_blue(&img);

  newlist = dfuCreateDynListWithVals(DF_CHAR, size, img.image_data);

  if (img.image_id) free(img.image_id);
  if (img.color_map_data) free(img.color_map_data);

  return(tclPutList(interp, newlist));
}

static int imgReadPNGCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  unsigned char *pixeldata;	/* returned from decode */
  unsigned int w, h;
  int d, size;
  DYN_LIST *newlist;

  unsigned char* buffer;
  size_t buffersize;
  unsigned error;
  int bitDepth;
  int colorType;
  extern unsigned
    LodePNG_loadFile(unsigned char** out, size_t* outsize,
		     const char* filename);

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], "filename", NULL);
    return TCL_ERROR;
  }

  error = LodePNG_loadFile(&buffer, &buffersize, argv[1]);
  if (error) {
  errorReturn:
    Tcl_AppendResult(interp, argv[0],
		     ": unable to load file \"", argv[1] ,"\"", NULL);
    return(TCL_ERROR);
  }
  
  /* Get depth info */
  PNG_GetInfo(buffer, buffersize, NULL, NULL, &bitDepth, &colorType);
  switch(colorType)
    {
    case 0: d = 1; /*grey*/
    case 2: d = 3; /*RGB*/
    case 3: d = 1; /*palette*/
    case 4: d = 2; /*grey + alpha*/
    case 6: d = 4; /*RGBA*/
    }
  

  if (colorType != 0 && colorType != 2 && colorType != 6)  {
    Tcl_AppendResult(interp, argv[0],
		     ": invalid PNG file type \"", argv[1] ,"\"", NULL);
    return(TCL_ERROR);
  }
  
  error = LodePNG_decode(&pixeldata, &w, &h, buffer,
			 buffersize, colorType, bitDepth);
  free(buffer);
  if (error) {
    Tcl_AppendResult(interp, argv[0],
		     ": error reading file \"", argv[1] ,"\"", NULL);
    return(TCL_ERROR);
  }
  
  size = d*w*h;
  newlist = dfuCreateDynListWithVals(DF_CHAR, size, pixeldata);
  return(tclPutList(interp, newlist));
}


static int imgLoadTargaCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  IMG_IMAGE *newimage;
  int w = 0, h = 0, d = 0;
  unsigned int size;
  static char imgname[32];

  tga_image img;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " filename", NULL);
    return TCL_ERROR;
  }

  if (tga_read(&img, argv[1]) != TGA_NOERR) {
    Tcl_AppendResult(interp, argv[0], ": error loading file \"", argv[1],
		     "\"", NULL);
    return(TCL_ERROR);
  }
  
  d = img.pixel_depth/8;
  w = img.width;
  h = img.height;
  size = d*w*h;

  if (!tga_is_top_to_bottom(&img)) tga_flip_vert(&img);
  tga_swap_red_blue(&img);

  if (img.image_id) free(img.image_id);
  if (img.color_map_data) free(img.color_map_data);

  sprintf(imgname, "img%d", imgCount++);
  if (!(newimage = initimage_withvals(w, h, d, imgname, img.image_data))) {
    Tcl_AppendResult(interp, argv[0], ": unable to create image ", imgname, 
		     (char *) NULL);
    free(img.image_data);
    return TCL_ERROR;
  }
  return imgAddImage(interp, newimage, imgname);
}

static int imgLoadPNGCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  IMG_IMAGE *newimage;
  static char imgname[32];
  unsigned char *pixeldata;	/* returned from decode */
  unsigned int w, h;
  int d, size;
  
  unsigned char* buffer;
  size_t buffersize;
  unsigned error;
  int bitDepth;
  int colorType;
  extern unsigned LodePNG_loadFile(unsigned char** out,
				   size_t* outsize, const char* filename);
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " filename", NULL);
    return TCL_ERROR;
  }
  
  error = LodePNG_loadFile(&buffer, &buffersize, argv[1]);
  if (error) {
  errorReturn:
    Tcl_AppendResult(interp, argv[0],
		     ": unable to load \"", argv[1] ,"\"", NULL);
    return(TCL_ERROR);
  }
  
  /* Get depth info */
  PNG_GetInfo(buffer, buffersize, NULL, NULL, &bitDepth, &colorType);
  switch(colorType)
    {
    case 0: d = 1; break; /*grey*/
    case 2: d = 3; break; /*RGB*/
    case 3: d = 1; break; /*palette*/
    case 4: d = 2; break; /*grey + alpha*/
    case 6: d = 4; break; /*RGBA*/
    }
  

  if (colorType != 0 && colorType != 2 && colorType != 6)  {
    Tcl_AppendResult(interp, argv[0],
		     ": invalid PNG file type \"", argv[1] ,"\"", NULL);
    return(TCL_ERROR);
  }
  
  error = LodePNG_decode(&pixeldata, &w, &h, buffer, buffersize, colorType, bitDepth);
  free(buffer);
  if (error) {
    Tcl_AppendResult(interp, argv[0], ": error loading file \"", argv[1],
		     "\"", NULL);
    return(TCL_ERROR);
  }
  
  size = d*w*h;

  sprintf(imgname, "img%d", imgCount++);
  if (!(newimage = initimage_withvals(w, h, d, imgname, pixeldata))) {
    Tcl_AppendResult(interp, argv[0], ": unable to create image ", imgname, 
		     (char *) NULL);
    free(pixeldata);
    return TCL_ERROR;
  }
  return imgAddImage(interp, newimage, imgname);
}

static int imgPremultiplyAlphaCmd(ClientData clientData, Tcl_Interp *interp,
				  int argc, char *argv[])
{
  IMG_IMAGE *image;
  int i, j, n, a, r, g, b;

  if (argc < 5) {
    Tcl_AppendResult(interp, "usage: ",
		     argv[0], " src r g b", NULL);
    return TCL_ERROR;
  }

  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &r) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &g) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[4], &b) != TCL_OK) return TCL_ERROR;

  if (image->depth != 4) {
    Tcl_AppendResult(interp, argv[0],
		     ": source images has no alpha", NULL);
    return TCL_ERROR;
  }

  n = image->width*image->height;
  for (i = 0, j = 0; i < n; i++) {
    a = image->data[j+3]/255.;
    image->data[j] = (char) ((r*(1-a))+a*image->data[j]);
    j++;
    image->data[j] = (char) ((g*(1-a))+a*image->data[j]);
    j++;
    image->data[j] = (char) ((b*(1-a))+a*image->data[j]);
    j+=2;			/* skip alpha... */
  }
  
  Tcl_SetResult(interp, argv[1], TCL_STATIC);
  return TCL_OK;
}

static int imgScaleAlphaCmd(ClientData clientData, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  IMG_IMAGE *alphaimage;
  double scale;
  int i, j, n;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ",
		     argv[0], " src scale", NULL);
    return TCL_ERROR;
  }

  if (imgFindImg(interp, argv[1], &alphaimage) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &scale) != TCL_OK) return TCL_ERROR;

  if (alphaimage->depth != 1 && alphaimage->depth != 4) {
    Tcl_SetResult(interp, "img_addAlpha: invalid alpha_image", TCL_STATIC);
    return TCL_ERROR;
  }

  n = alphaimage->width * alphaimage->height;

  if (alphaimage->depth == 4) {	/* copy original with one memcpy */
    for (i = 0, j = 3; i < n; i++, j+=4) {
      alphaimage->data[j] = alphaimage->data[j]*scale;
    }
  }
  if (alphaimage->depth == 1) {
    for (i = 0; i < n; i++) {
      alphaimage->data[i] = alphaimage->data[i]*scale;
    }
  }
  Tcl_SetResult(interp, alphaimage->name, TCL_STATIC);
  return TCL_OK;
}


static int imgAddAlphaCmd(ClientData clientData, Tcl_Interp *interp,
			  int argc, char *argv[])
{
  IMG_IMAGE *rgbimage, *alphaimage, *newimage;

  int i, j, k = 0, m, n;
  static char imgname[32];

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ",
		     argv[0], " src alpha", NULL);
    return TCL_ERROR;
  }

  if (imgFindImg(interp, argv[1], &rgbimage) != TCL_OK) return TCL_ERROR;
  if (imgFindImg(interp, argv[2], &alphaimage) != TCL_OK) return TCL_ERROR;

  if (alphaimage->depth != 1 && alphaimage->depth != 4) {
    Tcl_SetResult(interp, "img_addAlpha: invalid alpha_image", TCL_STATIC);
    return TCL_ERROR;
  }

  if (alphaimage->width != rgbimage->width || 
      alphaimage->height != rgbimage->height) {
    Tcl_AppendResult(interp, argv[0],
		     ": source and alpha images not compatible", NULL);
    return TCL_ERROR;
  }

  n = rgbimage->width * rgbimage->height;


  sprintf(imgname, "img%d", imgCount++);
  if (!(newimage = initimage(rgbimage->width, rgbimage->height, 4, imgname))) {
    Tcl_AppendResult(interp, argv[0], ": unable to create image ", imgname, 
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (rgbimage->depth == 4) {	/* copy original with one memcpy */
    memcpy(newimage->data, rgbimage->data, 4*n);
    if (alphaimage->depth == 1) {
      for (i = 0, j = 3; i < n; i++, j+=4) {
	newimage->data[j] = alphaimage->data[i];
      }
    }
    if (alphaimage->depth == 4) {
      for (i = 0, j = 3; i < n; i++, j+=4) {
	rgbimage->data[j] = alphaimage->data[j];
      }
    }
  }
  else if (rgbimage->depth == 3) {
    if (alphaimage->depth == 1) {
      for (i = 0, j = 0, m = 0; i < n; i++) {
	newimage->data[j++] = rgbimage->data[m++];
	newimage->data[j++] = rgbimage->data[m++];
	newimage->data[j++] = rgbimage->data[m++];
	newimage->data[j++] = alphaimage->data[i];
      }
    }
    else if (alphaimage->depth == 4) {
      for (i = 0, j = 0, m = 0, k = 3; i < n; i++, k+=4) {
	newimage->data[j++] = rgbimage->data[m++];
	newimage->data[j++] = rgbimage->data[m++];
	newimage->data[j++] = rgbimage->data[m++];
	newimage->data[j++] = alphaimage->data[k];
      }
    }
  }
  return imgAddImage(interp, newimage, imgname);
}

static int imgAlphaAreaCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  IMG_IMAGE *image;
  float prop;

  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ",
		     argv[0], " img", NULL);
    return TCL_ERROR;
  }

  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  prop = img_alphaArea(image);
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj(prop));
  return TCL_OK;
}

static int imgAlphaBBCmd(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  IMG_IMAGE *image;
  int x0, y0, x1, y1, result;
  float cx, cy;
  
  if (argc < 2) {
    Tcl_AppendResult(interp, "usage: ",
		     argv[0], " img", NULL);
    return TCL_ERROR;
  }

  if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
  result = img_alphaBB(image, &x0, &y0, &x1, &y1, &cx, &cy);
  if (result) {    
    Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(x0));
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(y0));
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(x1));
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(y1));
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(cx));
    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(cy));
    Tcl_SetObjResult(interp, listPtr);
    
    return TCL_OK;
  }
  else {
    Tcl_AppendResult(interp, argv[0],
		     ": error finding bounding box", NULL);
    return TCL_ERROR;
  }
}


static int imgWriteTargaCmd(ClientData clientData, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  IMG_IMAGE *image;
  int w = 0, h = 0;
  DYN_LIST *dl;

  if (argc != 5 && argc != 3) {
    Tcl_SetResult(interp,
		  "usage: img_writeTarga {image filename | pixels w h filename}",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  
  if (argc == 3) {
    if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
    
    switch(image->depth) {
    case 1:
      tga_write_mono_rle(argv[2], image->data, image->width, image->height);
      return TCL_OK;
      break;
    case 3:
      tga_write_rgb_rle(argv[2], image->data, image->width, image->height, 24);
      return TCL_OK;
      break;
    case 4:
      tga_write_rgb_rle(argv[2], image->data, image->width, image->height, 32);
      return TCL_OK;
      break;
    }
  }

  else {
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[2], &w) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[3], &h) != TCL_OK) return TCL_ERROR;
    
    if (DYN_LIST_DATATYPE(dl) != DF_CHAR) {
      Tcl_AppendResult(interp, argv[0], ": pixels must be of type char", NULL);
      return TCL_ERROR;
    }
    if (w*h == DYN_LIST_N(dl)) {
      tga_write_mono_rle(argv[4], (unsigned char *) DYN_LIST_VALS(dl),
			 w, h);
      return TCL_OK;
    }
    if (w*h*3 == DYN_LIST_N(dl)) {
      tga_write_rgb_rle(argv[4], (unsigned char *) DYN_LIST_VALS(dl),
			w, h, 24);
      return TCL_OK;
    }
    if (w*h*4 == DYN_LIST_N(dl)) {
      tga_write_rgb_rle(argv[4], (unsigned char *) DYN_LIST_VALS(dl),
			w, h, 32);
      return TCL_OK;
    }
  }

  Tcl_AppendResult(interp, argv[0], ": invalid pixel length", NULL);
  return TCL_ERROR;
}

static int imgWritePNGCmd(ClientData clientData, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  IMG_IMAGE *image;
  int w = 0, h = 0;
  DYN_LIST *dl;

  if (argc != 5 && argc != 3) {
    Tcl_SetResult(interp,
		  "usage: img_writePNG {image filename | pixels w h filename}",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  if (argc == 3) {
    if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
    switch(image->depth) {
    case 1:
      LodePNG_encode_file(argv[2], image->data, image->width, image->height,
			  0, 8);
      return TCL_OK;
      break;
    case 3:
      LodePNG_encode_file(argv[2], image->data, image->width, image->height,
			  2, 8);
      return TCL_OK;
      break;
    case 4:
      LodePNG_encode_file(argv[2], image->data, image->width, image->height,
			  6, 8);
      return TCL_OK;
      break;
    }
  }

  else {
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[2], &w) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[3], &h) != TCL_OK) return TCL_ERROR;
    
    if (DYN_LIST_DATATYPE(dl) != DF_CHAR) {
      Tcl_AppendResult(interp, argv[0], ": pixels must be of type char", NULL);
      return TCL_ERROR;
    }
    if (w*h == DYN_LIST_N(dl)) {
      LodePNG_encode_file(argv[4], (unsigned char *) DYN_LIST_VALS(dl), w, h,
			  0, 8);
      return TCL_OK;
    }
    if (w*h*3 == DYN_LIST_N(dl)) {
      LodePNG_encode_file(argv[4], (unsigned char *) DYN_LIST_VALS(dl), w, h,
			  2, 8);
      return TCL_OK;
    }
    if (w*h*4 == DYN_LIST_N(dl)) {
      LodePNG_encode_file(argv[4], (unsigned char *) DYN_LIST_VALS(dl), w, h,
			  6, 8);
      return TCL_OK;
    }
  }

  Tcl_AppendResult(interp, argv[0], ": invalid pixel length", NULL);
  return TCL_ERROR;
}

static int imgToPNGCmd(ClientData clientData, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  IMG_IMAGE *image;
  int w = 0, h = 0;
  DYN_LIST *dl;
  unsigned char *encoded = NULL;
  size_t encoded_size;
  
  if (argc != 4 && argc != 2) {
    Tcl_SetResult(interp,
		  "usage: img_toPNG {image filename | pixels w h filename}",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  if (argc == 2) {
    if (imgFindImg(interp, argv[1], &image) != TCL_OK) return TCL_ERROR;
    switch(image->depth) {
    case 1:
      LodePNG_encode(&encoded, &encoded_size, image->data, image->width, image->height,
		     0, 8);
      break;
    case 3:
      LodePNG_encode(&encoded, &encoded_size, image->data, image->width, image->height,
		     2, 8);
      break;
    case 4:
      LodePNG_encode(&encoded, &encoded_size, image->data, image->width, image->height,
		     6, 8);
      break;
    default:
      Tcl_AppendResult(interp, argv[0], ": invalid image", NULL);
      return TCL_ERROR;
      break;
    }
  }

  else {
    if (tclFindDynList(interp, argv[1], &dl) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[2], &w) != TCL_OK) return TCL_ERROR;
    if (Tcl_GetInt(interp, argv[3], &h) != TCL_OK) return TCL_ERROR;
    
    if (DYN_LIST_DATATYPE(dl) != DF_CHAR) {
      Tcl_AppendResult(interp, argv[0], ": pixels must be of type char", NULL);
      return TCL_ERROR;
    }
    if (w*h == DYN_LIST_N(dl)) {
      LodePNG_encode(&encoded, &encoded_size, (unsigned char *) DYN_LIST_VALS(dl), w, h,
		     0, 8);
    }
    if (w*h*3 == DYN_LIST_N(dl)) {
      LodePNG_encode(&encoded, &encoded_size, (unsigned char *) DYN_LIST_VALS(dl), w, h,
			  2, 8);
    }
    if (w*h*4 == DYN_LIST_N(dl)) {
      LodePNG_encode(&encoded, &encoded_size, (unsigned char *) DYN_LIST_VALS(dl), w, h,
			  6, 8);
    }
    if (!encoded) {
      Tcl_AppendResult(interp, argv[0], ": invalid image", NULL);
      return TCL_ERROR;
    }
  }
  Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(encoded, encoded_size));
  return TCL_OK;
}

static int imgReadRawFilesCmd(ClientData clientData, Tcl_Interp *interp,
			      int argc, char *argv[])
{
  int i;
  FILE *infp;
  unsigned char *pixels;
  DYN_LIST *newlist, *retlist = NULL;
  int w = 0, h = 0, d = 0, header_bytes;
  unsigned int size;
  Tcl_Size f_argc;
  char **f_argv;

  if (argc < 2) {
    Tcl_AppendResult(interp, argv[0], 
		     ": img_readraws \"filename(s)\" [w] [h] [d]",
		     NULL);
    return TCL_ERROR;
  }

  /* Split up list of files to read */
  Tcl_SplitList(interp, argv[1], &f_argc, (const char ***) &f_argv); 

  if (argc > 2) {
    if (Tcl_GetInt(interp, argv[2], &w) != TCL_OK) return TCL_ERROR;
  }
  if (argc > 3) {
    if (Tcl_GetInt(interp, argv[3], &h) != TCL_OK) return TCL_ERROR;
  }
  if (argc > 4) {
    if (Tcl_GetInt(interp, argv[4], &d) != TCL_OK) return TCL_ERROR;
  }

  /* One pass thru to check on files */
  for (i = 0; i < f_argc; i++) {
    if (!raw_getImageDims(f_argv[i], &w, &h, &d, &header_bytes)) {
      Tcl_AppendResult(interp, argv[0],
		       ": unable to determine dims for \"", argv[1] ,"\"", NULL);
      Tcl_Free((char *) f_argv);
      return(TCL_ERROR);
    }
  }

  retlist = dfuCreateDynList(DF_LIST, f_argc);
  /* Now do the work */
  for (i = 0; i < f_argc; i++) {
    raw_getImageDims(f_argv[i], &w, &h, &d, &header_bytes);
    size = w*h*d;
    pixels = malloc(size);
    if (!pixels) {
      Tcl_AppendResult(interp, argv[0], ": error allocating memory", NULL);
      dfuFreeDynList(retlist);
      Tcl_Free((char *) f_argv);
      return TCL_ERROR;
    }
    
    infp = fopen(f_argv[i], "rb");
    if (header_bytes) fseek(infp, header_bytes, SEEK_SET);
    if (fread(pixels, 1, size, infp) != size) {
      Tcl_AppendResult(interp, argv[0], ": error reading raw file", NULL);
      fclose(infp);
      free(pixels);
      dfuFreeDynList(retlist);
      Tcl_Free((char *) f_argv);
      return TCL_ERROR;
    }
    fclose(infp);
    
    newlist = dfuCreateDynListWithVals(DF_CHAR, size, pixels);
    dfuMoveDynListList(retlist, newlist);
  }
  Tcl_Free((char *) f_argv);
  return(tclPutList(interp, retlist));
}


static int imgAddHeaderCmd(ClientData clientData, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  int w = 0, h = 0, d = 0;
  if (argc < 3) {
    Tcl_AppendResult(interp, argv[0],
		     ": img_addHeader imgfile newname [w h d]", NULL);
    return TCL_ERROR;
  }

  if (argc > 3) {
    if (Tcl_GetInt(interp, argv[3], &w) != TCL_OK) return TCL_ERROR;
  }
  if (argc > 4) {
    if (Tcl_GetInt(interp, argv[4], &h) != TCL_OK) return TCL_ERROR;
  }
  if (argc > 5) {
    if (Tcl_GetInt(interp, argv[5], &d) != TCL_OK) return TCL_ERROR;
  }

  if (!raw_addHeader(argv[1], argv[2], w, h, d)) {
    Tcl_AppendResult(interp, argv[0],
		     ": unable to add header to image \"", argv[1]
		     ,"\"", NULL);
    return(TCL_ERROR);
  }
  Tcl_SetResult(interp, argv[2], TCL_STATIC);
  return(TCL_OK);
}


DYN_LIST *imageMakeDynList(IMG_IMAGE *image) 
{
  int size = image->width*image->height*image->depth;
  DYN_LIST *imgdata;
  char *vals = (char *) malloc(size);
  memcpy(vals, image->data, size);
  imgdata = dfuCreateDynListWithVals(DF_CHAR, size, vals);
  return imgdata;
}

#ifdef WIN32
BOOL APIENTRY
DllEntryPoint(hInst, reason, reserved)
    HINSTANCE hInst;
    DWORD reason;
    LPVOID reserved;
{
	return TRUE;
}
#endif

/*****************************************************************************
 *
 * FUNCTIONS
 *    base64encode/base64decode
 *
 * DESCRIPTION
 *    Move to/from b64 encoding
 *
 * SOURCE
 *    http://en.wikibooks.org/wiki/Algorithm_Implementation/Miscellaneous/Base64
 *
 *****************************************************************************/


static int base64encode(const void* data_buf, int dataLength, char* result, int resultSize)
{
   const char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
   const unsigned char *data = (const unsigned char *)data_buf;
   int resultIndex = 0;
   int x;
   unsigned int n = 0;
   int padCount = dataLength % 3;
   unsigned char n0, n1, n2, n3;
 
   /* increment over the length of the string, three characters at a time */
   for (x = 0; x < dataLength; x += 3) 
   {
      /* these three 8-bit (ASCII) characters become one 24-bit number */
      n = data[x] << 16;
 
      if((x+1) < dataLength)
         n += data[x+1] << 8;
 
      if((x+2) < dataLength)
         n += data[x+2];
 
      /* this 24-bit number gets separated into four 6-bit numbers */
      n0 = (unsigned char)(n >> 18) & 63;
      n1 = (unsigned char)(n >> 12) & 63;
      n2 = (unsigned char)(n >> 6) & 63;
      n3 = (unsigned char)n & 63;
 
      /*
       * if we have one byte available, then its encoding is spread
       * out over two characters
       */
      if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
      result[resultIndex++] = base64chars[n0];
      if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
      result[resultIndex++] = base64chars[n1];
 
      /*
       * if we have only two bytes available, then their encoding is
       * spread out over three chars
       */
      if((x+1) < dataLength)
      {
         if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
         result[resultIndex++] = base64chars[n2];
      }
 
      /*
       * if we have all three bytes available, then their encoding is spread
       * out over four characters
       */
      if((x+2) < dataLength)
      {
         if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
         result[resultIndex++] = base64chars[n3];
      }
   }  
 
   /*
    * create and add padding that is required if we did not have a multiple of 3
    * number of characters available
    */
   if (padCount > 0) 
   { 
      for (; padCount < 3; padCount++) 
      { 
         if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
         result[resultIndex++] = '=';
      } 
   }
   if(resultIndex >= resultSize) return 0;   /* indicate failure: buffer too small */
   result[resultIndex] = 0;
   return 1;   /* indicate success */
}

#define B64_WHITESPACE 64
#define B64_EQUALS     65
#define B64_INVALID    66
 
static const unsigned char d[] = {
    66,66,66,66,66,66,66,66,66,64,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,62,66,66,66,63,52,53,
    54,55,56,57,58,59,60,61,66,66,66,65,66,66,66, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,66,66,66,66,66,66,26,27,28,
    29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66
};
 
static int base64decode (char *in, unsigned int inLen, unsigned char *out, unsigned int *outLen) { 
  char *end = in + inLen;
  int buf = 1;
  unsigned int len = 0;
  
  while (in < end) {
    unsigned char c = d[*in++];
    
    switch (c) {
    case B64_WHITESPACE: continue;   /* skip whitespace */
    case B64_INVALID:    return 1;   /* invalid input, return error */
    case B64_EQUALS:                 /* pad character, end of data */
      in = end;
      continue;
    default:
      buf = buf << 6 | c;
      
      /* If the buffer is full, split it into bytes */
      if (buf & 0x1000000) {
	if ((len += 3) > *outLen) return 1; /* buffer overflow */
	*out++ = buf >> 16;
	*out++ = buf >> 8;
	*out++ = buf;
	buf = 1;
      }   
        }
  }
  
  if (buf & 0x40000) {
    if ((len += 2) > *outLen) return 1; /* buffer overflow */
    *out++ = buf >> 10;
    *out++ = buf >> 2;
  }
  else if (buf & 0x1000) {
    if (++len > *outLen) return 1; /* buffer overflow */
    *out++ = buf >> 4;
    }
  
  *outLen = len; /* modify to reflect the actual output size */
  return 0;
}
