/*
 * NAME
 *    imgload.c
 *
 * DESCRIPTION
 *    Raw image file loading functions for the impro tcl library
 *
 *
 * DLS, JUNE-1996
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>             
#include <sys/stat.h>		/* low-level file information             */

#include "img.h"
#include <rawapi.h>		/* API for reading raw files              */

static int img_guessImageDimensions(char *filename, int *width, int *height);

/*
 ****************************************************************************
 *                             img_load
 ****************************************************************************
 */
IMG_IMAGE *img_load (char *filename, char *name, int width, int height)
{
  IMG_IMAGE *image;
  FILE *fp;
  int d = 0, header_bytes;
  int w = width;
  int h = height;

  if (!raw_getImageDims(filename, &w, &h, &d, &header_bytes)) {
    return 0;
  }

  if (!(fp = fopen(filename, "rb"))) return 0;

  /* Skip header if there */
  if (header_bytes) fseek(fp, header_bytes, SEEK_SET);

  /* If no name is supplied, use the filename */
  if (!name) name = filename;

  image = (IMG_IMAGE *) calloc (1, sizeof(IMG_IMAGE));
  image->depth = 1;
  image->width = width;
  image->height = height;
  image->size = image->width * image->height;
  image->data = (byte *)calloc (image->size, sizeof(byte));
  strncpy(image->name, name, 127);

  fread(image->data, sizeof(char), image->size, fp); 
  fclose(fp);

  return (image);
}


static int img_guessImageDimensions(char *filename, int *width, int *height)
{
  struct stat statbuf;
  int status;
  int size;
  
  status = stat(filename, &statbuf);
  if (status < 0) return(-1);
  size = statbuf.st_size;
  
  switch (size) {
    case 64*64:
      *width = 64;
      *height = 64;
      break;
    case 32*32:
      *width = 64;
      *height = 64;
      break;
    case 256*256:
      *width = 256;
      *height = 256;
      break;
    case 128*128:
      *width = 128;
      *height = 128;
      break;
    case 348*478:
      *width = 348;
      *height = 478;
      break;
    case 345*512:
      *width = 345;
      *height = 512;
      break;
    case 407*487:
      *width = 407;
      *height = 487;
      break;
    case 480*480:
      *width = 480;
      *height = 480;
      break;
    case 768*768:
       *width = 768;
       *height = 768;
       break;
    case 512*400:
      *width = 512;
      *height = 400;
      break;
    case 512*384:
      *width = 512;
      *height = 384;
      break;
    case 512*512:
      *width = 512;
      *height = 512;
      break;
    case 512*482:
      *width = 512;
      *height = 482;
      break;
    case 600*400:
      *width = 600;
      *height = 400;
      break;
    case 640*480:
      *width = 640;
      *height = 480;
      break;
    case 800*600:
      *width = 800;
      *height = 600;
      break;
    case 1024*786:
      *width = 1024;
      *height = 768;
      break;
    default:
      return(0);
      break;
  }
  return(1);
}
