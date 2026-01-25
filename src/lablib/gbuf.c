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
/*   9/25 Refactored for thread safety - all functions now per-context    */
/**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
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

/* Forward declarations - updated to take CgraphContext parameter */
static void send_time(CgraphContext *ctx, int time);
static void send_event(CgraphContext *ctx, char type, unsigned char *data);
static void send_bytes(CgraphContext *ctx, int n, unsigned char *data);
static void push(CgraphContext *ctx, unsigned char *data, int size, int count);

/**************************************************************************/
/*                      Initialization Routines                           */
/**************************************************************************/

/* Initialize graphics buffer within context */
void gbInitGeventBuffer(CgraphContext *ctx)
{
    GHeader header;
    GBUF_DATA *gb;
    
    if (!ctx) return;
    gb = &ctx->gbuf_data;
    
    /* Initialize buffer structure */
    gb->gbufsize = EVENT_BUFFER_SIZE;
    gb->gbuf = (unsigned char *)calloc(gb->gbufsize, sizeof(unsigned char));
    if (!gb->gbuf) {
        fprintf(stderr, "Unable to allocate event buffer\n");
        exit(-1);
    }
    
    /* Initialize images */
    gb->images.nimages = 0;
    gb->images.maximages = 16;
    gb->images.allocinc = 16;
    gb->images.images = (GBUF_IMAGE *)calloc(gb->images.maximages, sizeof(GBUF_IMAGE));
    
    /* Initialize state */
    gb->gbufindex = 0;
    gb->empty = 1;
    gb->record_events = 1;
    gb->append_times = 0;
    gb->event_time = 0;
    
    /* Write header - now we have context for getresol! */
    G_VERSION(&header) = VERSION_NUMBER;
    getresol(ctx, &G_WIDTH(&header), &G_HEIGHT(&header));
    
    send_event(ctx, G_HEADER, (unsigned char *)&header);
    
    /* Record initial defaults */
    gbRecordDefaults(ctx);
}

/* Reset graphics buffer to initial state */
void gbResetGeventBuffer(CgraphContext *ctx)
{
    GHeader header;
    GBUF_DATA *gb;
    
    if (!ctx) return;
    gb = &ctx->gbuf_data;
    
    /* Reset buffer index */
    gb->gbufindex = 0;
    gb->empty = 1;
    
    /* Write new header */
    G_VERSION(&header) = VERSION_NUMBER;
    getresol(ctx, &G_WIDTH(&header), &G_HEIGHT(&header));
    
    send_event(ctx, G_HEADER, (unsigned char *)&header);
    
    /* Clean up images and record defaults */
    gbFreeImagesBuffer(ctx);
    gbRecordDefaults(ctx);
}

/* Clean up and free all resources in graphics buffer */
void gbCleanupGeventBuffer(CgraphContext *ctx)
{
    GBUF_DATA *gb;
    
    if (!ctx) return;
    gb = &ctx->gbuf_data;
    
    /* Disable recording */
    gb->record_events = 0;
    
    /* Free images */
    gbFreeImagesBuffer(ctx);
    if (gb->images.images) {
        free(gb->images.images);
        gb->images.images = NULL;
    }
    
    /* Free buffer data */
    if (gb->gbuf) {
        free(gb->gbuf);
        gb->gbuf = NULL;
    }
    
    /* Reset counters */
    gb->gbufindex = 0;
    gb->gbufsize = 0;
    gb->empty = 1;
    gb->images.nimages = 0;
    gb->images.maximages = 0;
}

/**************************************************************************/
/*                      Recording Control Functions                       */
/**************************************************************************/

void gbEnableGeventBuffer(CgraphContext *ctx)
{
    if (ctx) ctx->gbuf_data.record_events = 1;
}

void gbDisableGeventBuffer(CgraphContext *ctx)
{
    if (ctx) ctx->gbuf_data.record_events = 0;
}

int gbIsRecordingEnabled(CgraphContext *ctx)
{
    return ctx ? ctx->gbuf_data.record_events : 0;
}

/**************************************************************************/
/*                      Buffer State Functions                            */
/**************************************************************************/

int gbIsEmpty(CgraphContext *ctx)
{
    return ctx ? ctx->gbuf_data.empty : 1;
}

int gbSize(CgraphContext *ctx)
{
    return ctx ? ctx->gbuf_data.gbufindex : 0;
}

int gbSetEmpty(CgraphContext *ctx, int empty)
{
    if (!ctx) return 0;
    int old = ctx->gbuf_data.empty;
    ctx->gbuf_data.empty = empty;
    return old;
}

/**************************************************************************/
/*                      Timing Functions (kept for compatibility)         */
/**************************************************************************/

void gbEnableGeventTimes(CgraphContext *ctx)
{
    if (!ctx) return;
    record_gattr(ctx, G_TIMESTAMP, 1);
    ctx->gbuf_data.append_times = 1;
}

void gbDisableGeventTimes(CgraphContext *ctx)
{
    if (!ctx) return;
    record_gattr(ctx, G_TIMESTAMP, 0);
    ctx->gbuf_data.append_times = 0;
}

int gbSetTime(CgraphContext *ctx, int time)
{
    if (!ctx) return 0;
    int oldtime = ctx->gbuf_data.event_time;
    ctx->gbuf_data.event_time = time;
    return oldtime;
}

int gbIncTime(CgraphContext *ctx, int time)
{
    if (!ctx) return 0;
    int oldtime = ctx->gbuf_data.event_time;
    ctx->gbuf_data.event_time += time;
    return oldtime;
}

/**************************************************************************/
/*                      Image Management Functions                        */
/**************************************************************************/

int gbAddImage(CgraphContext *ctx, int w, int h, int d, unsigned char *data,
               float x0, float y0, float x1, float y1)
{
    GBUF_IMAGES *images;
    GBUF_IMAGE *img;
    int retval;
    int nbytes = w*h*d;
    
    if (!ctx) return -1;
    images = &ctx->gbuf_data.images;
    
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

GBUF_IMAGE *gbFindImage(CgraphContext *ctx, int ref)
{
    GBUF_IMAGES *images;
    
    if (!ctx) return NULL;
    images = &ctx->gbuf_data.images;
    
    if (ref >= images->nimages) return NULL;
    return(&images->images[ref]);
}

int gbReplaceImage(CgraphContext *ctx, int ref, int w, int h, int d, unsigned char *data)
{
    GBUF_IMAGE *gimg;
    GBUF_IMAGES *images;
    int nbytes = w*h*d;
    
    if (!ctx) return 0;
    images = &ctx->gbuf_data.images;
    
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

void gbFreeImage(GBUF_IMAGE *image)
{
    if (image && image->data) {
        free(image->data);
        image->data = NULL;
    }
}

void gbFreeImagesBuffer(CgraphContext *ctx)
{
    int i;
    GBUF_IMAGES *images;
    
    if (!ctx) return;
    images = &ctx->gbuf_data.images;
    
    for (i = 0; i < images->nimages; i++) {
        gbFreeImage(&images->images[i]);
    }
    images->nimages = 0;
}

int gbInitImageList(CgraphContext *ctx, int n)
{
    GBUF_IMAGES *images;
    
    if (!ctx) return 0;
    images = &ctx->gbuf_data.images;
    
    gbFreeImagesBuffer(ctx);
    if (images->images) {
        free(images->images);
    }
    
    if (n <= 0) n = 1;
    images->allocinc = n;
    images->maximages = n;
    images->images = (GBUF_IMAGE *) calloc(n, sizeof(GBUF_IMAGE));
    images->nimages = 0;
    return n;
}

int gbWriteImageFile(CgraphContext *ctx, FILE *fp)
{
    int i, n;
    GBUF_IMAGES *imgheader;
    GBUF_IMAGE *img;
    
    if (!ctx || !fp) return 0;
    
    imgheader = &ctx->gbuf_data.images;
    
    fwrite(imgheader, sizeof(GBUF_IMAGES), 1, fp);
    fwrite(imgheader->images, sizeof(GBUF_IMAGE), imgheader->nimages, fp);
    
    for (i = 0; i < imgheader->nimages; i++) {
        img = &(imgheader->images[i]);
        n = img->w * img->h * img->d;
        fwrite(img->data, sizeof(char), n, fp);
    }
    return 1;
}

int gbReadImageFile(CgraphContext *ctx, FILE *fp)
{
    int i, j;
    const int maximages = 4096;
    GBUF_IMAGES tempImages;     /* Temporary read buffer */
    GBUF_IMAGE *img;
    int n;
    char *imgdata;

    if (!ctx || !fp) return 0;

    /* Read the image header */
    if (fread(&tempImages, sizeof(GBUF_IMAGES), 1, fp) != 1) {
        return 0;
    }
    if (tempImages.nimages < 0 || tempImages.nimages > maximages) return 0;
    
    /* Allocate temporary space for image headers */
    tempImages.images = (GBUF_IMAGE *) calloc(tempImages.nimages, sizeof(GBUF_IMAGE));
    if (!tempImages.images) return 0;
    
    tempImages.maximages = tempImages.nimages;
    tempImages.allocinc = 10;

    /* Read all image headers */
    if (fread(tempImages.images, sizeof(GBUF_IMAGE), tempImages.nimages, fp) != 
        (unsigned int) tempImages.nimages) {
        free(tempImages.images);
        return 0;
    }
    
    /* Now read in all image data */
    for (i = 0; i < tempImages.nimages; i++) {
        img = &(tempImages.images[i]);
        n = img->w * img->h * img->d;
        imgdata = (char *) calloc(n, sizeof(char));
        if (!imgdata) {
            /* Free all previously allocated images on failure */
            for (j = 0; j < i; j++) {
                free(tempImages.images[j].data);
            }
            free(tempImages.images);
            return 0;
        }
        img->data = (unsigned char *)imgdata;
        
        if (fread(img->data, n, 1, fp) != 1) {
            /* Free all previously allocated images on failure */
            for (j = 0; j <= i; j++) {
                free(tempImages.images[j].data);
            }
            free(tempImages.images);
            return 0;
        }
    }
    
    /* Success - clean up existing images and replace with new ones */
    gbFreeImagesBuffer(ctx);
    if (ctx->gbuf_data.images.images) {
        free(ctx->gbuf_data.images.images);
    }
    
    /* Copy the successfully loaded images to context */
    memcpy(&ctx->gbuf_data.images, &tempImages, sizeof(GBUF_IMAGES));

    return 1;
}

/**************************************************************************/
/*                      Legacy Compatibility Functions                    */
/**************************************************************************/

/* Note: gbInitGevents() is now replaced by gbInitGeventBuffer() 
 * These functions were in the original but are no longer needed:
 * - gbInitGevents() -> use gbInitGeventBuffer() 
 * - gbResetGevents() -> use gbResetGeventBuffer()
 * - gbCloseGevents() -> use gbCleanupGeventBuffer()
 */

/* Cleanup function - replaces old gbCloseGevents() */
void gbCloseGevents(CgraphContext *ctx)
{
    if (!ctx) return;
    gbCleanupGeventBuffer(ctx);
}

/**************************************************************************/
/*                      Buffer Cleaning and Output Functions             */
/**************************************************************************/

int gbPlaybackGevents(CgraphContext *ctx)
{
    float xl, yb, xr, yt;
    
    if (!ctx) return 0;
    
    if (ctx->gbuf_data.gbufindex) { /* buffer is not empty */
        
        gbDisableGeventBuffer(ctx);
        
        getwindow(ctx, &xl, &yb, &xr, &yt);
        playback_gbuf(ctx, ctx->gbuf_data.gbuf, ctx->gbuf_data.gbufindex);
        setwindow(ctx, xl, yb, xr, yt);
        
        gbEnableGeventBuffer(ctx);
        
    }
    else {
        clearscreen(ctx);
    }
    return(ctx->gbuf_data.gbufindex);
}

int gbWriteGevents(CgraphContext *ctx, char *filename, int format)
{
    FILE *fp = stdout;
    char *filemode;
    
    if (!ctx) return 0;
    
    switch (format) {
    case GBUF_PDF:
      return gbuf_dump_pdf(ctx, (char *) ctx->gbuf_data.gbuf, ctx->gbuf_data.gbufindex, filename);
        break;
    case GBUF_RAW:
        filemode = "wb+";
        break;
    default:
        filemode = "w+";
        break;
    }
    
    if (filename && filename[0]) {
        if (!(fp = fopen(filename, filemode))) {
            fprintf(stderr, "gbuf: unable to open file \"%s\" for output\n", filename);
            return(0);
        }
    }
    
    gbuf_dump(ctx, (char *)ctx->gbuf_data.gbuf, ctx->gbuf_data.gbufindex, format, fp);

    if (filename && filename[0]) fclose(fp);
    return(1);
}

void gbPrintGevents(CgraphContext *ctx)
{
    char fname[L_tmpnam];
    static char buf[80];

    if (!ctx) return;
    
    tmpnam(fname);
    gbWriteGevents(ctx, fname, GBUF_PS);

    sprintf(buf, "lpr %s", fname);
    system(buf);
    unlink(fname);
}

int gbCleanGeventBuffer(CgraphContext *ctx)
{
    int clean_size;
    unsigned char *clean_buffer = gbuf_clean(GB_GBUF(ctx), GB_GBUFINDEX(ctx), &clean_size);
    
    if (!clean_buffer) return 0;
    
    if (GB_GBUF(ctx)) free(GB_GBUF(ctx));
    
    ctx->gbuf_data.gbuf = clean_buffer;
    ctx->gbuf_data.gbufindex = clean_size;
    ctx->gbuf_data.gbufsize = clean_size;
    
    return 1;
}

/**************************************************************************/
/*                      Page Setup Functions                             */
/**************************************************************************/

void gbSetPageOrientation(CgraphContext *ctx, char ori)
{
    if (!ctx) return;
    /* Store page orientation in context - could add to GBUF_DATA if needed */
    /* For now, this affects the output format functions */
    /* Implementation depends on how page setup is stored */
}

void gbSetPageFill(CgraphContext *ctx, int fill)
{
    if (!ctx) return;
    /* Store page fill setting in context */
    /* Implementation depends on how page setup is stored */
}

/**************************************************************************/
/*                      Recording Functions                               */
/**************************************************************************/

void record_gline(CgraphContext *ctx, char type, float x0, float y0, float x1, float y1)
{
    GLine gline;
    
    if (!ctx) return;
    
    GLINE_X0(&gline) = x0;
    GLINE_Y0(&gline) = y0;
    GLINE_X1(&gline) = x1;
    GLINE_Y1(&gline) = y1;
    
    send_event(ctx, type, (unsigned char *) &gline);
}

void record_gpoly(CgraphContext *ctx, char type, int nverts, float *verts)
{
    GPointList list;
    
    if (!ctx) return;
    
    GPOINTLIST_N(&list) = nverts;
    GPOINTLIST_PTS(&list) = NULL;
    send_event(ctx, type, (unsigned char *) &list);
    send_bytes(ctx, GPOINTLIST_N(&list)*sizeof(float), (unsigned char *) verts);
}

void record_gtext(CgraphContext *ctx, char type, float x, float y, char *str)
{
    GText gtext;
    
    if (!ctx || !str) return;
    
    GTEXT_X(&gtext) = x;
    GTEXT_Y(&gtext) = y;
    GTEXT_LENGTH(&gtext) = strlen(str)+1;
    GTEXT_STRING(&gtext) = NULL;
    
    send_event(ctx, type, (unsigned char *) &gtext);
    send_bytes(ctx, GTEXT_LENGTH(&gtext), (unsigned char *)str);
}

void record_gpoint(CgraphContext *ctx, char type, float x, float y)
{
    GPoint gpoint;
    
    if (!ctx) return;
    
    GPOINT_X(&gpoint) = x;
    GPOINT_Y(&gpoint) = y;

    send_event(ctx, type, (unsigned char *) &gpoint);
}

void record_gattr(CgraphContext *ctx, char type, int val)
{
    GAttr gattr;
    
    if (!ctx) return;
    
    GATTR_VAL(&gattr) = val;

    send_event(ctx, type, (unsigned char *) &gattr);
}

/* Record current graphics defaults - now has access to all context state! */
void gbRecordDefaults(CgraphContext *ctx)
{
    if (!ctx) return;
    
    /* Only record if recording is enabled */
    if (!ctx->gbuf_data.record_events) return;
    
    /* Record current font */
    if (getfontname(ctx)) {
        record_gtext(ctx, G_FONT, getfontsize(ctx), 0.0, getfontname(ctx));
    }
    
    /* Record current graphics attributes */
    record_gattr(ctx, G_COLOR, getcolor(ctx)); 
    record_gattr(ctx, G_LSTYLE, getframe(ctx)->grain);
    record_gattr(ctx, G_LWIDTH, getframe(ctx)->lwidth);
    record_gattr(ctx, G_ORIENTATION, getorientation(ctx));
    record_gattr(ctx, G_JUSTIFICATION, getframe(ctx)->just);
}

/**************************************************************************/
/*                      Internal Helper Functions                         */
/**************************************************************************/

static void send_event(CgraphContext *ctx, char type, unsigned char *data)
{
    if (!ctx) return;
    
    push(ctx, (unsigned char *)&type, 1, 1);
    if (ctx->gbuf_data.append_times) send_time(ctx, ctx->gbuf_data.event_time);
    
    switch(type) {
    case G_HEADER:
        push(ctx, data, GHEADER_S, 1);
        break;
    case G_FILLEDRECT:
    case G_LINE:
    case G_CLIP:
    case G_CIRCLE:
    case G_IMAGE:
        push(ctx, data, GLINE_S, 1);
        break;
    case G_MOVETO:
    case G_LINETO:
    case G_POINT:
        push(ctx, data, GPOINT_S, 1);
        break;
    case G_FONT:
    case G_TEXT:
    case G_POSTSCRIPT:
        push(ctx, data, GTEXT_S, 1);
        break;
    case G_COLOR:
    case G_BACKGROUND:
    case G_LSTYLE:
    case G_LWIDTH:
    case G_ORIENTATION:
    case G_JUSTIFICATION:
    case G_SAVE:
    case G_GROUP:
    case G_TIMESTAMP:
        push(ctx, data, GATTR_S, 1);
        break;
    case G_FILLEDPOLY:
    case G_POLY:
        push(ctx, data, GPOINTLIST_S, 1);
        break;
    default:
        fprintf(stderr, "Unknown event type: %d\n", type);
        break;
    }
    gbSetEmpty(ctx, 0);
}

static void send_time(CgraphContext *ctx, int time)
{
    if (!ctx) return;
    push(ctx, (unsigned char *)&time, sizeof(int), 1);
}

static void send_bytes(CgraphContext *ctx, int n, unsigned char *data)
{
    if (!ctx) return;
    push(ctx, data, sizeof(unsigned char), n);
}

static void push(CgraphContext *ctx, unsigned char *data, int size, int count)
{
    int nbytes, newsize;
    GBUF_DATA *gb;
    
    if (!ctx || !data) return;
    gb = &ctx->gbuf_data;
    
    nbytes = count * size;
    
    if (gb->gbufindex + nbytes >= gb->gbufsize) {
        do {
            newsize = gb->gbufsize + EVENT_BUFFER_SIZE;
            gb->gbuf = (unsigned char *) realloc(gb->gbuf, newsize);
            gb->gbufsize = newsize;
        } while (gb->gbufindex + nbytes >= gb->gbufsize);
    }
    
    memcpy(&gb->gbuf[gb->gbufindex], data, nbytes);
    gb->gbufindex += nbytes;
}
