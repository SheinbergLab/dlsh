/**************************************************************************/
/*      gbuf.c - graphics buffer package for use with cgraph              */
/*      Created: 4-Apr-94                                                 */
/*      DLS                                                               */
/*                                                                        */
/*      The following functions were designed to be used in conjunction   */
/*   with the cgraph package in order to keep an off screen, object based */
/*   representation of the current drawing surface.  For each of the      */
/*   drawing primitives, there is a unique function byte code, which      */
/*   identifies the drawing command (e.g. point, line, or polygon) and    */
/*   indicates how many parameters follow.                                */
/*                                                                        */
/*      Graphics buffers ("gbufs") are initialized by calling             */
/*   gbInitGevents(), which allocates space for a new buffer and sets up  */
/*   a pair of indices for keeping track of the current contents of the   */
/*   buffer.  By default, a standard graphics buffer is used, which is    */
/*   fine for a single windowed application.  However, in order to keep   */
/*   track of multiple active windows, each with its own gbuf, the        */
/*   gbInitGbufData() function can be used to set the current buffer to   */
/*   the GBUF_DATA structure pointer supplied as an argument.  By         */
/*   setting and resetting different graphics buffer data structures      */
/*   using the gbSetGeventBuffer, it is possible to keep track of more    */
/*   than one window.                                                     */
/*                                                                        */
/**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#endif
#ifndef QNX
#include <memory.h>
#endif
#include "cgraph.h"

#include "gbuf.h"
#include "gbufutl.h"

#define VERSION_NUMBER (2.0)

#define EVENT_BUFFER_SIZE 64000

char RecordGEvents = 0;		/* are graphic events recorded */
char AppendGEventTimes = 0;	/* are times being recorded?   */
int  GEventTime = 0;		/* current gevent time         */

static GBUF_DATA DefaultGB;	/* Default graphics buf        */
static GBUF_DATA *curGB = &DefaultGB;

static void send_time(int);
static void send_event(char type, unsigned char *data);
static void send_bytes(int n, unsigned char *data);
static void push(unsigned char *data, int, int);


/**************************************************************************/
/*                      Initialization Routines                           */
/**************************************************************************/

GBUF_DATA *gbInitGeventBuffer(GBUF_DATA *gb)
{
  GBUF_DATA *oldgb = curGB;
  curGB = gb;
  GB_IMAGES(gb).nimages = 0;
  GB_RECORD_EVENTS(gb) = 1;
  gbInitGevents();
  return(oldgb);
}


GBUF_DATA *gbSetGeventBuffer(GBUF_DATA *gb)
{
  GBUF_DATA *oldgb = curGB;
  curGB = gb;
  return(oldgb);
}

GBUF_DATA *gbGetGeventBuffer()
{
  return(curGB);
}

void gbEnableGeventBuffer(GBUF_DATA *gb)
{
   if (gb) GB_RECORD_EVENTS(gb) = 1;
}

void gbDisableGeventBuffer(GBUF_DATA *gb)  
{
   if (gb) GB_RECORD_EVENTS(gb) = 0;
}

// Convenience functions for current buffer
void gbEnableCurrentBuffer(void)
{
   GB_RECORD_EVENTS(curGB) = 1;
}

void gbDisableCurrentBuffer(void)
{
   GB_RECORD_EVENTS(curGB) = 0;
}

void gbResetCurrentBuffer(void)
{
  gbResetGeventBuffer(curGB);
}

int gbIsRecordingEnabled(void)
{
  return GB_RECORD_EVENTS(curGB);
}
  
/*
 * gbAddImage(w, h, d, data, x0, y0, x1, y1) - add image to local storage
 */
int gbAddImage(int w, int h, int d, unsigned char *data,
	       float x0, float y0, float x1, float y1)
{
  GBUF_IMAGES *images = &GB_IMAGES(curGB);
  GBUF_IMAGE *img;
  int retval;
  int nbytes = w*h*d;
  if ((retval = images->nimages) == images->maximages) {
    images->maximages += images->allocinc;
    images->images = (GBUF_IMAGE *) realloc(images->images,
			 sizeof(GBUF_IMAGE) * images->maximages);
  }
  
  /* Now put in actual data for new images */
  img = &images->images[images->nimages];
  img->w = w;
  img->h = h;
  img->d = d;
  img->x0 = x0;
  img->y0 = y0;
  img->x1 = x1;
  img->y1 = y1;
  img->data = (unsigned char *) calloc(nbytes, sizeof(char));
  
  memcpy(img->data, data, nbytes);
  
  images->nimages++;
  
  return retval;
}

/*
 * gbFindImage(int ref) - retrieve image reference
 */
GBUF_IMAGE *gbFindImage(int ref)
{
  GBUF_IMAGES *images = &GB_IMAGES(curGB);
  if (!images) return NULL;
  
  if (ref >= images->nimages) return NULL;
  return(&images->images[ref]);
}


/*
 * gbReplaceImage(int ref) - replace image reference
 */
int gbReplaceImage(int ref, int w, int h, int d, unsigned char *data)
{
  GBUF_IMAGE *gimg;
  GBUF_IMAGES *images = &GB_IMAGES(curGB);
  int nbytes = w*h*d;
  if (!images) return 0;
  
  if (ref >= images->nimages) return 0;
  gimg = &images->images[ref];

  /* Easy case -- same size just overwrite data */
  if (gimg->w == w && gimg->h == h && gimg->d == d) {
    memcpy(gimg->data, data, nbytes);
  }
  else {			/* reallocate */
    gimg->data = (unsigned char *) realloc(gimg->data, nbytes);
    gimg->w = w;
    gimg->h = h;
    gimg->d = d;
    memcpy(gimg->data, data, nbytes);
  }
  return 1;
}


/*
 * gbFreeImage() - free individual images
 */

void gbFreeImage(GBUF_IMAGE *image)
{
  if (image && image->data) free(image->data);
}

/*
 * gbFreeImages() - free any currently stored images
 */

void gbFreeImages(void)
{
  int i;
  GBUF_IMAGES *images = &GB_IMAGES(curGB);
  for (i = 0; i < images->nimages; i++) {
    gbFreeImage(&images->images[i]);
  }
  images->nimages = 0;
}

/*
 * gbFreeImageList() - free entire image list
 */

void gbFreeImageList(void)
{
  GBUF_IMAGES *images = &GB_IMAGES(curGB);
  gbFreeImages();
  if (images->images) free(images->images);
  images->nimages = 0;
  images->maximages = 0;
}

int gbInitImageList(int n)
{
  GBUF_IMAGES *images = &GB_IMAGES(curGB);
  gbFreeImageList();
  if (n <= 0) n = 1;
  images->allocinc = n;
  images->maximages = n;
  images->images = (GBUF_IMAGE *) calloc(n, sizeof(GBUF_IMAGE));
  images->nimages = 0;
  return n;
}

int gbWriteImageFile(FILE *fp)
{
  int i, n;
  GBUF_IMAGES *imgheader = &GB_IMAGES(curGB);
  GBUF_IMAGE *img;
  fwrite(imgheader, sizeof(GBUF_IMAGES), 1, fp);
  fwrite(imgheader->images, sizeof(GBUF_IMAGE), imgheader->nimages, fp);
  for (i = 0; i < imgheader->nimages; i++) {
    img = &(imgheader->images[i]);
    n = img->w*img->h*img->d;
    fwrite(img->data, sizeof(char), n, fp);
  }
  return 1;
}

int gbReadImageFile(FILE *fp)
{
  int i, j;
  const int maximages = 4096;
  GBUF_IMAGES Images;		/* To read into before copying */
  GBUF_IMAGE *img;
  int n, bytes_needed = 0;
  char *imgdata;

  if (fread(&Images, sizeof(GBUF_IMAGES), 1, fp) != 1) {
    return 0;
  }
  if (Images.nimages < 0 || Images.nimages > maximages) return 0;
  
  Images.images = (GBUF_IMAGE *) calloc(Images.nimages, sizeof(GBUF_IMAGE));
  Images.maximages = Images.nimages;
  Images.allocinc = 10;

  /* First read all image headers (and get total space needed for images) */
  if (fread(Images.images, sizeof(GBUF_IMAGE), Images.nimages, fp) != 
      (unsigned int) Images.nimages) {
    free(Images.images);
    return 0;
  }
  /* Now read in all data */
  for (i = 0; i < Images.nimages; i++) {
    img = &(Images.images[i]);
    n = img->w*img->h*img->d;
    imgdata = (char *) calloc(n, sizeof(char));
    img->data = imgdata;
    if (fread(img->data, n, 1, fp) != 1) {
      /* Free all previously allocated images -- we won't use them */
      for (j = 0; j <= i; j++) {
	img = &(Images.images[j]);
	free(img->data);
      }
      free(Images.images);
      return 0;
    }
  }
  
  gbFreeImageList();
  memcpy(&GB_IMAGES(curGB), &Images, sizeof(GBUF_IMAGES));

  return 1;
}

/*
 * gbInitGevents() - initialize the default gbuf data structure           
 */

void gbInitGevents()
{
   GHeader header;

   GB_GBUFSIZE(curGB) = EVENT_BUFFER_SIZE;
   if (!(GB_GBUF(curGB) = (unsigned char *)
	 calloc(GB_GBUFSIZE(curGB), sizeof(unsigned char)))) {
     fprintf(stderr,"Unable to allocate event buffer\n");
     exit(-1);
   }
   
   gbInitImageList(16);
   GB_GBUFINDEX(curGB) = 0;
   
   G_VERSION(&header) = VERSION_NUMBER;
   getresol(&G_WIDTH(&header),&G_HEIGHT(&header));
   
   send_event(G_HEADER,(unsigned char *) &header);
   gbRecordDefaults();
}


void gbRecordDefaults()
{
  /* This sends the current font to the file */
  if (GB_RECORD_EVENTS(curGB) && getfontname(contexp))  // ← Use per-buffer state
    record_gtext(G_FONT, getfontsize(contexp), 0.0, getfontname(contexp));
  if (GB_RECORD_EVENTS(curGB)) record_gattr(G_COLOR, contexp->color); 
  if (GB_RECORD_EVENTS(curGB)) record_gattr(G_LSTYLE, contexp->grain);
  if (GB_RECORD_EVENTS(curGB)) record_gattr(G_LWIDTH, contexp->lwidth);
  if (GB_RECORD_EVENTS(curGB)) record_gattr(G_ORIENTATION, contexp->orientation);
  if (GB_RECORD_EVENTS(curGB)) record_gattr(G_JUSTIFICATION, contexp->just);
}

void gbEnableGeventTimes()
{
   record_gattr(G_TIMESTAMP, 1);
   AppendGEventTimes = 1;
}

void gbDisableGeventTimes()
{
   record_gattr(G_TIMESTAMP, 0);
   AppendGEventTimes = 0;
}

int gbSetTime(int time)
{
   int oldtime = GEventTime;
   GEventTime = time;
   return(oldtime);
}

int gbIncTime(int time)
{
   int oldtime = GEventTime;
   GEventTime += time;
   return(oldtime);
}

int gbIsEmpty()
{
  return (GB_EMPTY(curGB));
}

int gbSetEmpty(int empty)
{
  int old = GB_EMPTY(curGB);
  GB_EMPTY(curGB) = empty;
  return old;
}

int gbSize()
{
  return (GB_GBUFINDEX(curGB));
}

void gbCleanupGeventBuffer(GBUF_DATA *gb)
{
   if (!gb) return;
   
   // Disable recording
   GB_RECORD_EVENTS(gb) = 0;
   
   // Free images
   gbFreeImagesBuffer(gb);
   
   // Free buffer data
   if (gb->gbuf) {
       free(gb->gbuf);
       gb->gbuf = NULL;
   }
   
   // Reset counters
   gb->gbufindex = 0;
   gb->gbufsize = 0;
   gb->empty = 1;
}

void gbResetGeventBuffer(GBUF_DATA *gb)
{
   GHeader header;
   
   if (!gb) return;
   
   // Reset this specific buffer
   gb->gbufindex = 0;
   
   G_VERSION(&header) = VERSION_NUMBER;
   getresol(&G_WIDTH(&header), &G_HEIGHT(&header));
   
   // Temporarily set as current to send header and record defaults
   GBUF_DATA *oldGB = gbSetGeventBuffer(gb);
   
   send_event(G_HEADER, (unsigned char *) &header);
   gbRecordDefaults();
   
   // Restore previous buffer
   gbSetGeventBuffer(oldGB);
   
   // Clean up images for this specific buffer
   gbFreeImagesBuffer(gb);
   gb->empty = 1;
}

// Helper function for per-buffer image cleanup
void gbFreeImagesBuffer(GBUF_DATA *gb)
{
   int i;
   GBUF_IMAGES *images = &gb->images;
   
   for (i = 0; i < images->nimages; i++) {
     gbFreeImage(&images->images[i]);
   }
   images->nimages = 0;
}


int gbPlaybackGevents(void)
{
  float xl, yb, xr, yt;
  float w, h;
  
  if (GB_GBUFINDEX(curGB)) { /* screen is not empty */

	gbDisableCurrentBuffer();

    getwindow(&xl, &yb, &xr, &yt) ;
    playback_gbuf(GB_GBUF(curGB),GB_GBUFINDEX(curGB));
    setwindow(xl, yb, xr, yt) ;

    gbEnableCurrentBuffer();

  }
  else {
    clearscreen();
  }
  return(GB_GBUFINDEX(curGB));
}

int gbWriteGevents(char *filename, int format)
{
   FILE *fp = stdout;
   char *filemode;
   
   switch (format) {
   case GBUF_PDF:
     return gbuf_dump_pdf((char *) GB_GBUF(curGB), GB_GBUFINDEX(curGB),
			  filename);
     break;
   case GBUF_RAW:
     filemode = "wb+";
     break;
   default:
     filemode = "w+";
     break;
   }
   
   if (filename && filename[0]) {
      if (!(fp = fopen(filename,filemode))) {
	 fprintf(stderr,"gbuf: unable to open file \"%s\" for output\n",
		 filename);
	 return(0);
      }
   }
   gbuf_dump((char *)GB_GBUF(curGB), GB_GBUFINDEX(curGB), format, fp);

   if (filename && filename[0]) fclose(fp);
   return(1);
}

void gbPrintGevents()
{
  char fname[L_tmpnam];
  static char buf[80];

  tmpnam(fname);
  gbWriteGevents(fname, GBUF_PS);

  sprintf(buf,"lpr %s", fname);
  system(buf);
  unlink(fname);
}   

void gbCloseGevents()
{
  if (GB_GBUF(curGB)) {
    free(GB_GBUF(curGB));
    GB_GBUF(curGB) = NULL;
    GB_GBUFINDEX(curGB) = 0;
    GB_GBUFSIZE(curGB) = 0;
    gbFreeImageList();
  }
}

/**
 * Clean the current graphics buffer in-place
 */
int gbCleanCurrentBuffer(int *original_size, int *clean_size)
{
    return gbCleanGeventBuffer(curGB, original_size, clean_size);
}

/**
 * Clean a specific graphics buffer in-place
 */
int gbCleanGeventBuffer(GBUF_DATA *gb, int *original_size, int *clean_size)
{
    unsigned char *cleaned_buffer;
    int cleaned_size;
    
    if (!gb || !gb->gbuf) {
        return -1; // Error: invalid buffer
    }
    
    if (original_size) *original_size = gb->gbufindex;
    
    // Clean the buffer
    cleaned_buffer = gbuf_clean(gb->gbuf, gb->gbufindex, &cleaned_size);
    if (!cleaned_buffer) {
        return -1; // Error: cleaning failed
    }
    
    // Replace the buffer contents
    if (gb->gbuf) {
        free(gb->gbuf);
    }
    
    gb->gbuf = cleaned_buffer;
    gb->gbufindex = cleaned_size;
    gb->gbufsize = cleaned_size;
    gb->empty = (cleaned_size <= GHEADER_S + 1) ? 1 : 0;
    
    if (clean_size) *clean_size = cleaned_size;
    
    return 0; // Success
}

/*********************************************************************/
/*                      Gevent Recording Funcs                       */
/*********************************************************************/

void record_gline(char type, float x0, float y0, float x1, float y1)
{
  GLine gline;
  
  GLINE_X0(&gline) = x0;
  GLINE_Y0(&gline) = y0;
  GLINE_X1(&gline) = x1;
  GLINE_Y1(&gline) = y1;
  
  send_event(type, (unsigned char *) &gline);
}

void record_gpoly(char type, int nverts, float *verts)
{
  GPointList list;
  
  GPOINTLIST_N(&list) = nverts;
  GPOINTLIST_PTS(&list) = NULL;
  send_event(type, (unsigned char *) &list);
  send_bytes(GPOINTLIST_N(&list)*sizeof(float), (unsigned char *) verts);
}

void record_gtext(char type, float x, float y, char *str)
{
   GText gtext;
  
   GTEXT_X(&gtext) = x;
   GTEXT_Y(&gtext) = y;
   GTEXT_LENGTH(&gtext) = strlen(str)+1;
   GTEXT_STRING(&gtext) = NULL;
   
   send_event(type, (unsigned char *) &gtext);
   send_bytes(GTEXT_LENGTH(&gtext), (unsigned char *)str);
}

void record_gpoint(char type, float x, float y)
{
  GPoint gpoint;
  
  GPOINT_X(&gpoint) = x;
  GPOINT_Y(&gpoint) = y;

  send_event(type, (unsigned char *) &gpoint);
}

void record_gattr(char type, int val)
{
  GAttr gattr;
  
  GATTR_VAL(&gattr) = val;

  send_event(type, (unsigned char *) &gattr);
}

static void send_event(char type, unsigned char *data)
{
  push((unsigned char *)&type, 1, 1);
  if (AppendGEventTimes) send_time(GEventTime);
  switch(type) {
  case G_HEADER:
    push(data, GHEADER_S, 1);
    break;
  case G_FILLEDRECT:
  case G_LINE:
  case G_CLIP:
  case G_CIRCLE:
  case G_IMAGE:			/* Reference ID (cast to float), x, y */
    push(data, GLINE_S, 1);
    break;
  case G_MOVETO:
  case G_LINETO:
  case G_POINT:
    push(data, GPOINT_S, 1);
    break;
  case G_FONT:
  case G_TEXT:
  case G_POSTSCRIPT:
    push(data, GTEXT_S, 1);
    break;
  case G_COLOR:
  case G_LSTYLE:
  case G_LWIDTH:
  case G_ORIENTATION:
  case G_JUSTIFICATION:
  case G_SAVE:
  case G_GROUP:
  case G_TIMESTAMP:
    push(data, GATTR_S, 1);
    break;
  case G_FILLEDPOLY:
  case G_POLY:
    push(data, GPOINTLIST_S, 1);
    break;
  default:
    fprintf(stderr,"Unknown event type: %d\n", type);
    break;
  }
  gbSetEmpty(0);
}

static void send_time(int time)
{
   push((unsigned char *)&time, sizeof(int), 1);
}

static void send_bytes(int n, unsigned char *data)
{
  push(data, sizeof(unsigned char), n);
}

static void push(unsigned char *data, int size, int count)
{
   int nbytes, newsize;
   
   nbytes = count * size;
   
   if (GB_GBUFINDEX(curGB) + nbytes >= GB_GBUFSIZE(curGB)) {
     do {
       newsize = GB_GBUFSIZE(curGB) + EVENT_BUFFER_SIZE;
       GB_GBUF(curGB) = (unsigned char *) realloc(GB_GBUF(curGB), newsize);
       GB_GBUFSIZE(curGB) = newsize;
     } while (GB_GBUFINDEX(curGB) + nbytes >= GB_GBUFSIZE(curGB));
   }
   
   memcpy(&GB_GBUF(curGB)[GB_GBUFINDEX(curGB)], data, nbytes);
   GB_GBUFINDEX(curGB) += nbytes;
 }

