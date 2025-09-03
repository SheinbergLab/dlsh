/**************************************************************************/
/*      cgraph.c - graphics package for c process control system          */
/*      Created: 20-Aug-82                                                */
/*      By: Sean D. True and Doug Whittington                             */
/*      Modified to run under DOS on IBM-AT by: Nikos Logothetis,20/11/88 */
/*      Modified by Nikos K. Logothetis, 22-MAY-90: Modules CGRAPH.C and  */
/*      GUTIL1.C are squeezed in one module with the name CGRAPH.C.       */
/*      Modified by Sheinberg & Leopold, 19-DEC-93: the character handler */
/*                                                  now takes ! strings ! */
/*      Modified by DLS,  APR-94 to record graphics events for later dump */
/*      Overhauled by DLS, APR-94 to use floats instead of ints           */
/*      Adapted for LYNX by DLS & DAL 17-APR-94                           */
/*	Modified by Michael Thayer, DEC-97: added extra handlers to       */
/*                                          ``FRAME'' structure           */
/*      Refactored for thread-safety with explicit context passing        */
/**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <cgraph.h>
#include <math.h>
#include <tcl.h>

#ifdef WIN32
#pragma warning (disable:4244)
#pragma warning (disable:4305)
#pragma warning (disable:4761)
#endif

#include "gbuf.h"

#define CR 015
#define LF 012
#define TAB 011
#define FF 014
#define EOT 04

/* AssocData key for storing interpreter-specific cgraph data */
#define CGRAPH_ASSOC_KEY "cgraph_context"

/* Default frame initialization values */
static FRAME default_frame_template = {0.0,0.0,640.0,480.0,0.0,0.0,1000.0,1000.0,
		      1.0,1.0,1.0,1.0,7.0,9.0,NULL,10.0, 0.0,0.0,8.0,8.0,
		      1,100,0,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
		      NULL,NULL,NULL,NULL,
		      NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
		      0.0,0.0,0.0,0.0,
		      0.0,0.0,0.0,0.0,NULL};

/* Forward declarations */
static void DeleteContextData(ClientData clientData, Tcl_Interp *interp);
static void CgraphInterpDeleteProc(ClientData clientData, Tcl_Interp *interp);
static int GetNorm(CgraphContext *ctx, float *x, float *y);
static void setvw(CgraphContext *ctx);
static void linutl(CgraphContext *ctx, float x, float y);
static int dclip(CgraphContext *ctx);
static int fclip(CgraphContext *ctx);

#define MULDIV(x,y,z) (((x)*(y))/(z))
#define SWAP(x,y) do { float _t = x; x = y; y = _t; } while(0)
#define WINDOW(f,x,y) {x=f->xl+MULDIV(x-f->xul,f->xs,f->xus); \
                       y=f->yb+MULDIV(y-f->yub,f->ys,f->yus);}
#define SCREEN(f,x,y) {x=f->xul + MULDIV(x-f->xl,f->xus,f->xs); \
                       y=f->yub + MULDIV(y-f->yb,f->yus,f->ys);}
#define SCALEXY(f,x,y){x=MULDIV((x),(f->xs),(f->xus));\
                       y=MULDIV((y),(f->ys),(f->yus));}
#define CHECKMIN(x) {if (x < 0.0) x=0.0;}
#define CHECKXMX(f,x) {if (x>((f->xsres)-1.0)) x=((f->xsres)-1.0);}
#define CHECKYMX(f,x) {if (x>((f->ysres)-1.0)) x=((f->ysres)-1.0);}

/*********************************************************************
 * Context management functions
 *********************************************************************/

static void DeleteContextData(ClientData clientData, Tcl_Interp *interp)
{
    CgraphContext *ctx = (CgraphContext *)clientData;
    if (!ctx) return;
    
    // Free gbuf
    gbCleanupGeventBuffer(ctx);
    
    // Clean up frames
    FRAME *frame = ctx->current_frame;
    while (frame) {
        FRAME *next = frame->parent;
        if (frame->fontname) {
            free(frame->fontname);
        }
        free(frame);
        frame = next;
    }
    
    // Clean up viewport stack
    if (ctx->viewport_stack) {
        if (ctx->viewport_stack->vals) {
            free(ctx->viewport_stack->vals);
        }
        free(ctx->viewport_stack);
    }
    
    free(ctx);
}

// dummy handler functions to allow proper gbuf recording
static void Dummy_Point(float x, float y) {}
static void Dummy_Line(float x1, float y1, float x2, float y2) {}
static void Dummy_Poly(float *verts, int nverts) {}
static void Dummy_Circle(float x, float y, float width, int filled) {}

CgraphContext *CgraphCreateContext(Tcl_Interp *interp)
{
    if (!interp) return NULL;
    
    // Check if already exists
    CgraphContext *existing = 
    (CgraphContext *)Tcl_GetAssocData(interp, CGRAPH_ASSOC_KEY, NULL);
    if (existing) {
        return existing;
    }
    
    // Create new context
    CgraphContext *ctx = (CgraphContext *)calloc(1, sizeof(CgraphContext));
    if (!ctx) return NULL;
    
    // Initialize with defaults
    memcpy(&ctx->default_frame, &default_frame_template, sizeof(FRAME));
    
    // Create initial frame
    ctx->current_frame = (FRAME *)malloc(sizeof(FRAME));
    if (!ctx->current_frame) {
        free(ctx);
        return NULL;
    }
    memcpy(ctx->current_frame, &ctx->default_frame, sizeof(FRAME));
    
    // Set default font
    const char *default_font = ctx->default_frame.fontname ? 
                               ctx->default_frame.fontname : "Helvetica";
    ctx->current_frame->fontname = (char *)malloc(strlen(default_font) + 1);
    if (!ctx->current_frame->fontname) {
        free(ctx->current_frame);
        free(ctx);
        return NULL;
    }
    strcpy(ctx->current_frame->fontname, default_font);
    
    setpoint(ctx, (PHANDLER) Dummy_Point);
	setline(ctx, (LHANDLER) Dummy_Line);  
	setfilledpoly(ctx, (FHANDLER) Dummy_Poly);
	setcircfunc(ctx, (CHANDLER) Dummy_Circle);

    ctx->current_frame->parent = NULL;
    ctx->barwidth = 10.0;
    
    setresol(ctx, 640.0, 480.0);
    user(ctx);
	setwindow(ctx, 0.0, 0.0, 639.0, 479.0);
	setfviewport(ctx, 0.0, 0.0, 1.0, 1.0);
	ctx->current_frame->fontsize = 10.0;

    // Initialize gbuf
    gbInitGeventBuffer(ctx);
    ctx->gbuf_initialized = 1;
    
    // Register with Tcl for automatic cleanup
    Tcl_SetAssocData(interp, CGRAPH_ASSOC_KEY, DeleteContextData, (ClientData)ctx);
    
    return ctx;
}

/* Simple getter - no creation, just retrieval */
CgraphContext *CgraphGetContext(Tcl_Interp *interp)
{
    if (!interp) return NULL;
    return (CgraphContext *)Tcl_GetAssocData(interp, CGRAPH_ASSOC_KEY, NULL);
}


/*********************************************************************
 *          Allow pushing & popping of frames and viewports
 *********************************************************************/

FRAME *gsave(CgraphContext *ctx)
{
   if (!ctx) return NULL;
   
   FRAME *currentFrame = ctx->current_frame;
   FRAME *newframe = (FRAME *) malloc(sizeof(FRAME));
   copyframe(currentFrame, newframe);
   newframe->parent = currentFrame;
   setstatus(ctx, newframe);
   if (gbIsRecordingEnabled(ctx)) record_gattr(ctx, G_SAVE, 1);
   return(newframe);
}

FRAME *grestore(CgraphContext *ctx)
{
   if (!ctx) return NULL;
   
   FRAME *currentFrame = ctx->current_frame;
   FRAME *oldframe = currentFrame;
   if (!currentFrame->parent) return NULL;
   setstatus(ctx, currentFrame->parent);
   if (oldframe->fontname) free((void *) oldframe->fontname);
   free((void *) oldframe);
   currentFrame = ctx->current_frame;
   if (currentFrame->dsetfont)
       currentFrame->dsetfont(currentFrame->fontname, currentFrame->fontsize);
   if (gbIsRecordingEnabled(ctx)) record_gattr(ctx, G_SAVE, -1);
   return(currentFrame);
}

FRAME *setstatus(CgraphContext *ctx, FRAME *newframe)
{
    if (!ctx) return NULL;
    
    FRAME *f = ctx->current_frame;
    ctx->current_frame = newframe;
    return f;
}

FRAME *setframe(CgraphContext *ctx, FRAME *newframe)
{
  return setstatus(ctx, newframe);
}

FRAME *getframe(CgraphContext *ctx)
{
  return ctx ? ctx->current_frame : NULL;
}

void pushviewport(CgraphContext *ctx)
{
  if (!ctx) return;
  
  float *vw;
  VW_STACK *stack = ctx->viewport_stack;

  /* if haven't created the stack yet, allocate */
  if (!stack) {
    stack = ctx->viewport_stack = (VW_STACK *) calloc(1, sizeof(VW_STACK));
    stack->size = 0;
    stack->index = -1;
    stack->vals = NULL;
    stack->increment = 4;
  }

  /* if there isn't space to add the current viewport, allocate */  
  if (stack->index == (stack->size-1)) {
    stack->size += stack->increment;
    if (stack->vals)
      stack->vals = (float *) realloc(stack->vals, stack->size*4*sizeof(float));
    else stack->vals = (float *) calloc(stack->size*4, sizeof(float));
  }
  stack->index++;
  
  /* set vw to the top of the stack */
  vw = &stack->vals[4*stack->index];
  
  getviewport(ctx, &vw[0], &vw[1], &vw[2], &vw[3]);
}

void poppushviewport(CgraphContext *ctx)
{
  popviewport(ctx);
  pushviewport(ctx);
}
    
int popviewport(CgraphContext *ctx)
{
  if (!ctx || !ctx->viewport_stack) return 0;
  
  VW_STACK *stack = ctx->viewport_stack;
  float *vw;
  
  if (stack->index < 0) {
    return(0);
  }
  vw = &stack->vals[4*stack->index];
  setviewport(ctx, vw[0], vw[1], vw[2], vw[3]);
  stack->index--;
  return(1);
}

 void endframe(CgraphContext *ctx)
{
   if (ctx && ctx->eframe)
      (*ctx->eframe)();
}

void clearscreen(CgraphContext *ctx)
{
  if (!ctx) return;
  
  FRAME *currentFrame = ctx->current_frame;
  
  if (currentFrame->dclearfunc)
      currentFrame->dclearfunc();
  else if (ctx->bframe) (*ctx->bframe)();

  if (gbIsRecordingEnabled(ctx)) 
      gbResetGeventBuffer(ctx);
}

void noplot(void)
{
}

/* sets the ``clearfunc'' handler function */
HANDLER setclearfunc(CgraphContext *ctx, HANDLER hnew)
{
    if (!ctx) return NULL;
    FRAME *currentFrame = ctx->current_frame;
    HANDLER hold = currentFrame->dclearfunc;
    currentFrame->dclearfunc = hnew;
    return(hold);
}

PHANDLER getpoint(CgraphContext *ctx)
{
    if (!ctx) return NULL;
    FRAME *currentFrame = ctx->current_frame;
    return (currentFrame->dpoint);
}

PHANDLER setpoint(CgraphContext *ctx, PHANDLER pointp)
{
   if (!ctx) return NULL;
   FRAME *currentFrame = ctx->current_frame;
   PHANDLER oldfunc = currentFrame->dpoint;
   currentFrame->dpoint = pointp;
   return(oldfunc);
}

PHANDLER setclrpnt(CgraphContext *ctx, PHANDLER clrpntp)
{
   if (!ctx) return NULL;
   FRAME *currentFrame = ctx->current_frame;
   PHANDLER oldfunc = currentFrame->dclrpnt;
   currentFrame->dclrpnt = clrpntp;
   return(oldfunc);
}

THANDLER setchar(CgraphContext *ctx, THANDLER charp)
{
   if (!ctx) return NULL;
   FRAME *currentFrame = ctx->current_frame;
   THANDLER oldfunc = currentFrame->dchar;
   currentFrame->dchar = charp;
   if (charp == NULL) setchrsize(ctx, 6.0, 8.0);
   return(oldfunc);
}

THANDLER settext(CgraphContext *ctx, THANDLER textp)
{
   if (!ctx) return NULL;
   FRAME *currentFrame = ctx->current_frame;
   THANDLER oldfunc = currentFrame->dtext;
   currentFrame->dtext = textp;
   return(oldfunc);
}

LHANDLER setline(CgraphContext *ctx, LHANDLER linep)
{
   if (!ctx) return NULL;
   FRAME *currentFrame = ctx->current_frame;
   LHANDLER oldfunc = currentFrame->dline;
   currentFrame->dline = linep;
   return(oldfunc);
}

FHANDLER setfilledpoly(CgraphContext *ctx, FHANDLER fillp)
{
   if (!ctx) return NULL;
   FRAME *currentFrame = ctx->current_frame;
   FHANDLER oldfunc = currentFrame->dfilledpoly;
   currentFrame->dfilledpoly = fillp;
   return(oldfunc);
}

FHANDLER setpolyline(CgraphContext *ctx, FHANDLER polyl)
{
   if (!ctx) return NULL;
   FRAME *currentFrame = ctx->current_frame;
   FHANDLER oldfunc = currentFrame->dpolyline;
   currentFrame->dpolyline = polyl;
   return(oldfunc);
}

LHANDLER setclipfunc(CgraphContext *ctx, LHANDLER clipf)
{
   if (!ctx) return NULL;
   FRAME *currentFrame = ctx->current_frame;
   LHANDLER oldfunc = currentFrame->dclip;
   currentFrame->dclip = clipf;
   return(oldfunc);
}

CHANDLER setcircfunc(CgraphContext *ctx, CHANDLER circfunc)
{
  if (!ctx) return NULL;
  FRAME *currentFrame = ctx->current_frame;
  CHANDLER oldfunc = currentFrame->dcircfunc;
  currentFrame->dcircfunc = circfunc;
  return(oldfunc);
}


/* sets the ``set line style'' handler function */

LSHANDLER setlstylefunc(CgraphContext *ctx, LSHANDLER hnew)
{
  if (!ctx) return NULL;
  FRAME *currentFrame = ctx->current_frame;
  LSHANDLER hold = currentFrame->dlinestyle ;

  currentFrame->dlinestyle = hnew ;
  return(hold) ;
}


/* sets the ``set line width'' handler function */

LWHANDLER setlwidthfunc(CgraphContext *ctx, LWHANDLER hnew)
{
  if (!ctx) return NULL;
  FRAME *currentFrame = ctx->current_frame;

  LWHANDLER hold = currentFrame->dlinewidth ;

  currentFrame->dlinewidth = hnew ;
  return(hold) ;
}


/* sets the ``set drawing color'' handler function */

COHANDLER setcolorfunc(CgraphContext *ctx, COHANDLER hnew)
{
  if (!ctx) return NULL;
  FRAME *currentFrame = ctx->current_frame;
	COHANDLER hold = currentFrame->dsetcolor ;

	currentFrame->dsetcolor = hnew ;
	return(hold) ;
}


/* sets the ``set background color'' handler function */

COHANDLER setbgfunc(CgraphContext *ctx, COHANDLER hnew)
{
  if (!ctx) return NULL;
  FRAME *currentFrame = ctx->current_frame;
	COHANDLER hold = currentFrame->dsetbg ;

	currentFrame->dsetbg = hnew ;
	return(hold) ;
}


/* sets the ``get string width'' handler function */

SWHANDLER strwidthfunc(CgraphContext *ctx, SWHANDLER hnew)
{
  if (!ctx) return NULL;
  FRAME *currentFrame = ctx->current_frame;
	SWHANDLER hold = currentFrame->dstrwidth ;

	currentFrame->dstrwidth = hnew ;
	return(hold) ;
}

/* sets the ``get string height'' handler function */

SHHANDLER strheightfunc(CgraphContext *ctx, SHHANDLER hnew)
{
  if (!ctx) return NULL;
  FRAME *currentFrame = ctx->current_frame;
	SHHANDLER hold = currentFrame->dstrheight ;

	currentFrame->dstrheight = hnew ;
	return(hold) ;
}


/* sets the ``set text orientation'' handler function */

SOHANDLER setorientfunc(CgraphContext *ctx, SOHANDLER hnew)
{
  if (!ctx) return NULL;
  FRAME *currentFrame = ctx->current_frame;
	SOHANDLER hold = currentFrame->dsetorient ;

	currentFrame->dsetorient = hnew ;
	return(hold) ;
}


/* sets the ``set text font'' handler function */

SFHANDLER setfontfunc(CgraphContext *ctx, SFHANDLER hnew)
{
  if (!ctx) return NULL;
  FRAME *currentFrame = ctx->current_frame;
	SFHANDLER hold = currentFrame->dsetfont ;

	currentFrame->dsetfont = hnew ;
	return(hold) ;
}

/* sets the ``file based image drawing'' handler function */

IMHANDLER setimagefunc(CgraphContext *ctx, IMHANDLER hnew)
{
  if (!ctx) return NULL;
  FRAME *currentFrame = ctx->current_frame;
  IMHANDLER hold = currentFrame->dimage;
  currentFrame->dimage = hnew;
  return(hold);
}

/* sets the ``in memory image drawing'' handler function */

MIMHANDLER setmemimagefunc(CgraphContext *ctx, MIMHANDLER hnew)
{
  if (!ctx) return NULL;
  FRAME *currentFrame = ctx->current_frame;
  MIMHANDLER hold = currentFrame->dmimage;
  currentFrame->dmimage = hnew;
  return(hold);
}

char *setfont(CgraphContext *ctx, char *fontname, float size)
{
  if (!ctx) return NULL;
  
  FRAME *currentFrame = ctx->current_frame;
  
  if (currentFrame->fontname && currentFrame->fontname != ctx->old_fontname) 
    strncpy(ctx->old_fontname, currentFrame->fontname, 63);
  else strcpy(ctx->old_fontname, "");
  
  if (currentFrame->fontname) free((void *) currentFrame->fontname);
  currentFrame->fontname = (char *) malloc(strlen(fontname)+1);
  strcpy(currentFrame->fontname, fontname);
  currentFrame->fontsize = size;
  if (currentFrame->dsetfont)
      currentFrame->dsetfont(currentFrame->fontname, size);
  if (gbIsRecordingEnabled(ctx)) 
      record_gtext(ctx, G_FONT, size, 0.0, fontname);
  
  if (strcmp(ctx->old_fontname,"")) return(ctx->old_fontname);
  else return (NULL);
}

float setsfont(CgraphContext *ctx, char *fontname, float ssize)
{
  if (!ctx) return 0.0;
  FRAME *currentFrame = ctx->current_frame;
  float size = ssize * ((currentFrame->xr-currentFrame->xl)/currentFrame->xsres);
  setfont(ctx, fontname, size);
  return(size);
}

float getxscale(CgraphContext *ctx)
{
  if (!ctx) return 1.0;
  FRAME *currentFrame = ctx->current_frame;
  return((currentFrame->xr-currentFrame->xl)/currentFrame->xsres);
}

float getyscale(CgraphContext *ctx)
{
  if (!ctx) return 1.0;
  FRAME *currentFrame = ctx->current_frame;
  return((currentFrame->yt-currentFrame->yb)/currentFrame->ysres);
}

char *getfontname(CgraphContext *ctx)
{
   if (!ctx) return NULL;
   FRAME *currentFrame = ctx->current_frame;
   return(currentFrame->fontname);
}

float getfontsize(CgraphContext *ctx)
{
   if (!ctx) return 10.0;
   FRAME *currentFrame = ctx->current_frame;
   return(currentFrame->fontsize);
}

void setbframe(CgraphContext *ctx, HANDLER clearfunc)
{
   if (ctx) ctx->bframe = clearfunc;
}

void seteframe(CgraphContext *ctx, HANDLER eoffunc)
{
   if (ctx) ctx->eframe = eoffunc;
}

void group(CgraphContext *ctx)
{
   if (gbIsRecordingEnabled(ctx)) 
       record_gattr(ctx, G_GROUP, 1); 
}

void ungroup(CgraphContext *ctx)
{
  if (gbIsRecordingEnabled(ctx)) 
      record_gattr(ctx, G_GROUP, 0); 
}

int setimgpreview(CgraphContext *ctx, int val)
{
  if (!ctx) return 0;
  
  int old = ctx->img_preview;
  ctx->img_preview = val;
  return old;
}

void postscript(CgraphContext *ctx, char *filename, float xsize, float ysize)
{
  if (!ctx) return;
  
  FRAME *f = ctx->current_frame;
  int recording, oldmode, oldclip;
  float x = f->xpos;
  float y = f->ypos;
  float xs = xsize;
  float ys = ysize;

  if((oldmode = f->mode))
    SCALEXY(f,xs,ys);
  
  oldclip = setclip(ctx, 0);
  if ((recording = gbIsRecordingEnabled(ctx))) {
    record_gpoint(ctx, G_MOVETO, f->xpos, f->ypos);
    record_gtext(ctx, G_POSTSCRIPT, xs, ys, filename); 
  }
  if (!ctx->img_preview || !f->dimage) { /* Put a frame where image will appear */
  keyline:
    if (recording) gbDisableGeventBuffer(ctx);
    screen(ctx);
    rect(ctx, x, y, x+xs, y+ys);
    moveto(ctx, x, y);
    lineto(ctx, x+xs, y+ys);
    moveto(ctx, x+xs, y);
    lineto(ctx, x, y+ys);
    if (oldmode) user(ctx);
    if (recording) gbEnableGeventBuffer(ctx);
    setclip(ctx, oldclip);
    return;
  }
  else {
    if (!(f->dimage(x, y, x+xs, y+ys, filename))) 
      goto keyline;
    setclip(ctx, oldclip);
  }
}

int place_image(CgraphContext *ctx, int w, int h, int d, unsigned char *data, 
                 float xsize, float ysize)
{
  if (!ctx) return -1;
  
  FRAME *f = ctx->current_frame;
  int recording, oldmode, ref = -1;
  int oldclip;
  float x = f->xpos;
  float y = f->ypos;
  float xs = xsize;
  float ys = ysize;

  if((oldmode = f->mode))
    SCALEXY(f,xs,ys);
  
  oldclip = setclip(ctx, 0);

  if ((recording = gbIsRecordingEnabled(ctx))) {
    ref = gbAddImage(ctx, w, h, d, data, x, y, x+xs, y+ys);
    record_gpoint(ctx, G_MOVETO, f->xpos, f->ypos);
    record_gline(ctx, G_IMAGE, xs, ys, (float) ref, 0);
  }

  if (!ctx->img_preview || !f->dmimage) { /* Put a frame where image will appear */
  keyline:
    if (recording) gbDisableGeventBuffer(ctx);
    screen(ctx);
    rect(ctx, x, y, x+xs, y+ys);
    moveto(ctx, x, y);
    lineto(ctx, x+xs, y+ys);
    moveto(ctx, x+xs, y);
    lineto(ctx, x, y+ys);
    if (oldmode) user(ctx);
    if (recording) gbEnableGeventBuffer(ctx);
    setclip(ctx, oldclip);
    return 0;
  }
  else {
    if (!(f->dmimage(x, y, x+xs, y+ys, w, h, d, data))) 
      goto keyline;
  }
  setclip(ctx, oldclip);
  return ref;
}

int replace_image(CgraphContext *ctx, int ref, int w, int h, int d, unsigned char *data)
{
  if (!ctx) return 0;
  
  FRAME *f = ctx->current_frame;
  GBUF_IMAGE *gimg;
  gimg = gbFindImage(ctx, ref);
  if (!gimg) return 0;
  gbReplaceImage(ctx, ref, w, h, d, data);
  if (f->dmimage) {
    f->dmimage(gimg->x0, gimg->y0, gimg->x1, gimg->y1, w, h, d, data);
  }
  return 1;
}

int setcolor(CgraphContext *ctx, int color)
{
   if (!ctx) return 0;
   FRAME *currentFrame = ctx->current_frame;
   int oldcolor = currentFrame->color;
   if (gbIsRecordingEnabled(ctx)) 
       record_gattr(ctx, G_COLOR, color); 
   currentFrame->color = color;
   if (currentFrame->dsetcolor)
       currentFrame->dsetcolor(color);
   return(oldcolor);
}

int getcolor(CgraphContext *ctx)
{
   if (!ctx) return 0;
   FRAME *currentFrame = ctx->current_frame;
   return(currentFrame->color);
}

int setbackgroundcolor(CgraphContext *ctx, int color)
{
   if (!ctx) return 0;
   FRAME *currentFrame = ctx->current_frame;
   int oldcolor = currentFrame->background_color;
   currentFrame->background_color = color;
   
   if (gbIsRecordingEnabled(ctx)) 
       record_gattr(ctx, G_BACKGROUND, color);
   
   if (currentFrame->dsetbg)
       currentFrame->dsetbg(color);
   return(oldcolor);
}

int getbackgroundcolor(CgraphContext *ctx)
{
  if (!ctx) return 0;
  FRAME *currentFrame = ctx->current_frame;
  return(currentFrame->background_color);
}
int setuser(CgraphContext *ctx, int modearg)
{
   if (!ctx) return 0;
   FRAME *currentFrame = ctx->current_frame;
   int olduser = currentFrame->mode;
   currentFrame->mode = modearg;
   return(olduser);
}

int setclip(CgraphContext *ctx, int cliparg)
{
  if (!ctx) return 0;
  FRAME *f = ctx->current_frame;
  LHANDLER dclip = f->dclip;
  int oldclip = f->clipf;
  if (oldclip == cliparg) return oldclip;

  if (cliparg) {
    if (dclip) (*dclip)(f->xl, f->yb, f->xr, f->yt);
    if (gbIsRecordingEnabled(ctx))
      record_gline(ctx, G_CLIP, f->xl, f->yb, f->xr, f->yt);
  }
  else {
    if (dclip) (*dclip)(0, 0, f->xsres, f->ysres);
    if (gbIsRecordingEnabled(ctx))
      record_gline(ctx, G_CLIP, 0, 0, f->xsres, f->ysres);
  }
  f->clipf = cliparg;
  return oldclip;
}

int getclip(CgraphContext *ctx)
{
  if (!ctx) return 0;
  FRAME *f = ctx->current_frame;
  return f->clipf;
}
void setchrsize(CgraphContext *ctx, float chrwarg, float chrharg)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   
   if(chrharg < 9.0) f->linsiz = 8.0;
   else f->linsiz = chrharg;
   f->yinc = 1.0;
   
   if(chrwarg < 7.0) f->colsiz = 7.0;
   else f->colsiz = chrwarg;
   f->xinc = 1.0;
}

static void setvw(CgraphContext *ctx)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   f->xus = f->xur - f->xul;
   f->yus = f->yut - f->yub;
   f->xs = f->xr - f->xl;
   f->ys = f->yt - f->yb;
}

void setclipregion(CgraphContext *ctx, float xlarg, float ybarg, float xrarg, float ytarg)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;

  if (f->mode) WINDOW(f, xlarg, ybarg);
  if (f->mode) WINDOW(f, xrarg, ytarg);

  if (f->dclip) (*(f->dclip))(xlarg, ybarg, xrarg, ytarg);
  if (gbIsRecordingEnabled(ctx))
    record_gline(ctx, G_CLIP, xlarg, ybarg, xrarg, ytarg);
}

void setviewport(CgraphContext *ctx, float xlarg, float ybarg, float xrarg, float ytarg)
{
    if (!ctx) return;
    FRAME *f = ctx->current_frame;
    LHANDLER dclip = f->dclip;

    CHECKMIN(xlarg);        /* check for lower than minimum values */
    CHECKMIN(xrarg);
    CHECKMIN(ybarg);
    CHECKMIN(ytarg);

    CHECKXMX(f,xlarg);      /* check for greater than maximum values */
    CHECKXMX(f,xrarg);
    CHECKYMX(f,ybarg);
    CHECKYMX(f,ytarg);

    if (xlarg>xrarg)        /* swap x-arguments if needed */
       SWAP(xlarg,xrarg);
    if (ybarg>ytarg)        /* swap y-arguments if needed */
       SWAP(ybarg,ytarg);

    if (xrarg - xlarg < 1.0) xrarg += 1.0;
    if (ytarg - ybarg < 1.0) ytarg += 1.0;

    f->xl = xlarg;
    f->yb = ybarg;
    f->xr = xrarg;
    f->yt = ytarg;

    if (dclip) (*dclip)(xlarg, ybarg, xrarg, ytarg);
    if (gbIsRecordingEnabled(ctx))
       record_gline(ctx, G_CLIP, xlarg, ybarg, xrarg, ytarg);

    setvw(ctx);
}

void setpviewport(CgraphContext *ctx, float xlarg, float ybarg, float xrarg, float ytarg)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  float x0, x1, y0, y1;
  
  x0 = f->xl+xlarg*f->xs;
  x1 = f->xl+xrarg*f->xs;
  y0 = f->yb+ybarg*f->ys;
  y1 = f->yb+ytarg*f->ys;
  setviewport(ctx, x0, y0, x1, y1);
}


float getuaspect(CgraphContext *ctx)
{
    if (!ctx) return 1.0;
    float wlx, wly, wux, wuy;
    float vlx, vly, vux, vuy;

    /* Get viewport/window extents to calc user coord aspect ratio */
    getviewport(ctx, &vlx, &vly, &vux, &vuy);
    getwindow(ctx, &wlx, &wly, &wux, &wuy);

    /* Make upper bounds "correct" */
    vux+=1.0;
    vuy+=1.0;

    return (((vux-vlx)/(vuy-vly))*((wuy-wly)/(wux-wlx)));
}

void getviewport(CgraphContext *ctx, float *xlarg, float *ybarg, float *xrarg, float *ytarg)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  *xlarg = f->xl;
  *ybarg = f->yb;
  *xrarg = f->xr;
  *ytarg = f->yt;
}

void setfviewport(CgraphContext *ctx, float fxlarg, float fybarg, float fxrarg, float fytarg)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  setviewport(ctx, fxlarg*f->xsres, fybarg*f->ysres,
              fxrarg*f->xsres, fytarg * f->ysres);
}

void getwindow(CgraphContext *ctx, float *xul, float *yub, float *xur, float *yut)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  *xul = f->xul;
  *yub = f->yub;
  *xur = f->xur;
  *yut = f->yut;
}

void setwindow(CgraphContext *ctx, float xularg, float yubarg, float xurarg, float yutarg)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  f->xul = xularg;
  f->yub = yubarg;
  f->xur = xurarg;
  f->yut = yutarg;
  setvw(ctx);
}

void setresol(CgraphContext *ctx, float xres, float yres)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   f->xsres = xres;
   f->ysres = yres;
}

void getresol(CgraphContext *ctx, float *xres, float *yres)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   *xres = f->xsres;
   *yres = f->ysres;
}


int setlstyle(CgraphContext *ctx, int grainarg)
{
  if (!ctx) return -1;
  FRAME *f = ctx->current_frame;
   int oldlstyle = f->grain;
   if(grainarg <= 0) grainarg = 1;
   f->grain = grainarg;
   if (f->dlinestyle)
   	(*f->dlinestyle)(grainarg);
   if (gbIsRecordingEnabled(ctx)) record_gattr(ctx, G_LSTYLE, grainarg);
   return(oldlstyle);
}

int setlwidth(CgraphContext *ctx, int width)
{
  if (!ctx) return -1;
  FRAME *f = ctx->current_frame;
   int oldwidth = f->lwidth;
   if (width < 0) width = 1;
   f->lwidth = width;
   if (f->dlinewidth)
   	(*f->dlinewidth)(width);
   if (gbIsRecordingEnabled(ctx)) record_gattr(ctx, G_LWIDTH, width);
   return(oldwidth);
}

int setgrain(CgraphContext *ctx, int grainarg)
{
   return(setlstyle(ctx, grainarg));
}

int setorientation(CgraphContext *ctx, int path)
{
  if (!ctx) return -1;
  FRAME *f = ctx->current_frame;
  
  int oldorient = f->orientation;
  
  if (gbIsRecordingEnabled(ctx)) record_gattr(ctx, G_ORIENTATION, path);
  f->orientation = path;
  if (f->dsetorient)
  	f->dsetorient(path);
  return(oldorient);
}


/* returns the current text orientation */

int getorientation(CgraphContext *ctx)
{
  if (!ctx) return 0;
  FRAME *f = ctx->current_frame;

  return(f->orientation);
}


int setjust(CgraphContext *ctx, int just)
{
  if (!ctx) return 0;
  FRAME *f = ctx->current_frame;
  int oldjust = f->just;

  if (gbIsRecordingEnabled(ctx)) record_gattr(ctx, G_JUSTIFICATION, just);
   f->just = just;
  return(oldjust);
}



static int GetNorm (CgraphContext *ctx, float *x, float *y)
{
  if (!ctx) return -1;
  FRAME *f = ctx->current_frame;
	float argx = *x;
	float argy = *y;

	if(f->mode)
		WINDOW(f,argx,argy);
	f->xpos = argx;
	f->ypos = argy;

	if(f->clipf){
		if(code(f,argx,argy))
			return (0);
	}
	*x = argx;
	*y = argy;
	return (1);
}

/**************************************************************************/
/* Routines for drawing points, lines, shapes, etc.                       */
/**************************************************************************/

void dotat(CgraphContext *ctx, float xarg, float yarg)
{
    if (!ctx) return;
    FRAME *f = ctx->current_frame;
    PHANDLER dp = f->dpoint;

    if(f->mode)
        WINDOW(f,xarg,yarg);
    f->xpos = xarg;
    f->ypos = yarg;
    if (dp) (*dp)(xarg,yarg);
    if (gbIsRecordingEnabled(ctx))
       record_gpoint(ctx, G_POINT, xarg, yarg);
}


void BigDotAt(CgraphContext *ctx, float x, float y)                 /* make a big dot         */
{
    if (!ctx) return;
    FRAME *f = ctx->current_frame;
	PHANDLER pfunc;

	pfunc = getpoint(ctx);

	if (!GetNorm(ctx, &x, &y)) return;

	(*pfunc)(x, y);
	(*pfunc)((x+=XUNIT(f)), y);
	(*pfunc)(x, (y+=YUNIT(f)));
	(*pfunc)((x-=XUNIT(f)), y);
	if (gbIsRecordingEnabled(ctx))
	   record_gpoint(ctx, G_POINT, x, y);
}

void SmallSquareAt(CgraphContext *ctx, float x, float y) 
{
    if (!ctx) return;
    FRAME *f = ctx->current_frame;
   PHANDLER pfunc;
   register float i, j;

   pfunc = getpoint(ctx);

   if (!GetNorm(ctx, &x, &y)) return;
   
   for (j=y-2.0*YUNIT(f); j<y+2.0*YUNIT(f); j+=YUNIT(f))
	  for (i=x-2.0*XUNIT(f); i<x+2.0*XUNIT(f); i+=XUNIT(f))
      (*pfunc)(i,j);
}

void SquareAt(CgraphContext *ctx, float x, float y) 
{
    if (!ctx) return;
    FRAME *f = ctx->current_frame;
   filledrect(ctx, x-3.0*XUNIT(f),y-3.0*YUNIT(f),
	      x+3.0*XUNIT(f),y+3.0*YUNIT(f));
}

void triangle(CgraphContext *ctx, float x, float y, float scale) /* make a hollow triangle */
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   float root2 = sqrt(2.);
   float xoff = root2*0.5*scale*XUNIT(f);
   float yoff = root2*0.5*scale*YUNIT(f);
   float yoff2 = .75*yoff;
   moveto(ctx, x-xoff,y-yoff2);
   lineto(ctx, x+xoff,y-yoff2);
   lineto(ctx, x,y+yoff);
   lineto(ctx, x-xoff,y-yoff2);
}

void diamond(CgraphContext *ctx, float x, float y, float scale) /* make a hollow diamond */
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   float xoff = 0.3*scale*XUNIT(f);
   float yoff = 0.5*scale*YUNIT(f);
   moveto(ctx, x-xoff,y);
   lineto(ctx, x,y+yoff);
   lineto(ctx, x+xoff,y);
   lineto(ctx, x,y-yoff);
   lineto(ctx, x-xoff,y);
}

void square(CgraphContext *ctx, float x, float y, float scale) /* make a hollow square */
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   float xoff = 0.5*scale*XUNIT(f);
   float yoff = 0.5*scale*YUNIT(f);
   moveto(ctx, x-xoff,y-yoff);
   lineto(ctx, x+xoff,y-yoff);
   lineto(ctx, x+xoff,y+yoff);
   lineto(ctx, x-xoff,y+yoff);
   lineto(ctx, x-xoff,y-yoff);
}

void fsquare(CgraphContext *ctx, float x, float y, float scale) /* make a filled square */
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   filledrect(ctx, x-0.5*scale*XUNIT(f),y-0.5*scale*YUNIT(f),
              x+0.5*scale*XUNIT(f),y+0.5*scale*YUNIT(f));
}

void circle(CgraphContext *ctx, float xarg, float yarg, float scale)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  CHANDLER circfunc = f->dcircfunc;
  float xsize = scale/XUNIT(f);
  xsize = scale;
  
  if(f->mode) {
    WINDOW(f,xarg,yarg);
  }

  if (f->clipf) {
    f->wx1 = xarg-(xsize/2);
    f->wy1 = yarg-(xsize/2);
    f->wx2 = xarg+(xsize/2);
    f->wy2 = yarg+(xsize/2);
    if (dclip(ctx))
      return;
  }

  if (circfunc) {
    (*circfunc)(xarg-(xsize/2), yarg+(xsize/2), xsize, 0);
  }
  
  if (gbIsRecordingEnabled(ctx))
    record_gline(ctx, G_CIRCLE, xarg, yarg, xsize, 0.0);
}

void fcircle(CgraphContext *ctx, float xarg, float yarg, float scale)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  CHANDLER circfunc = f->dcircfunc;
  float xsize = scale/XUNIT(f);
  xsize = scale;

  if(f->mode) {
    WINDOW(f,xarg,yarg);
  }
  
  if (f->clipf) {
    f->wx1 = xarg-(xsize/2);
    f->wy1 = yarg-(xsize/2);
    f->wx2 = xarg+(xsize/2);
    f->wy2 = yarg+(xsize/2);
    if (dclip(ctx))
      return;
  }

  if (circfunc) {
    (*circfunc)(xarg-(xsize/2), yarg+(xsize/2), xsize, 1);
  }
  
  if (gbIsRecordingEnabled(ctx))
    record_gline(ctx, G_CIRCLE, xarg, yarg, xsize, 1.0);
}

void vtick(CgraphContext *ctx, float x, float y, float scale) /* make a vertical tick */
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  float yoff = scale*YUNIT(f);
  moveto(ctx, x, y-yoff/2.0);
  lineto(ctx, x, y+yoff/2.0);
}

void htick(CgraphContext *ctx, float x, float y, float scale) /* make a horizontal tick */
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  float xoff = scale*XUNIT(f);
  moveto(ctx, x-xoff/2.0, y);
  lineto(ctx, x+xoff/2.0, y);
}

void vtick_up(CgraphContext *ctx, float x, float y, float scale) /* make a vertical tick */
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  float yoff = scale*YUNIT(f);
  moveto(ctx, x, y);
  lineto(ctx, x, y+yoff/2.0);
}

void vtick_down(CgraphContext *ctx, float x, float y, float scale) /* make a vertical tick */
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  float yoff = scale*YUNIT(f);
  moveto(ctx, x, y);
  lineto(ctx, x, y-yoff/2.0);
}

void htick_left(CgraphContext *ctx, float x, float y, float scale) /* make a horizontal tick */
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  float xoff = scale*XUNIT(f);
  moveto(ctx, x, y);
  lineto(ctx, x-xoff/2.0, y);
}

void htick_right(CgraphContext *ctx, float x, float y, float scale) /* make a horizontal tick */
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  float xoff = scale*XUNIT(f);
  moveto(ctx, x, y);
  lineto(ctx, x+xoff/2.0, y);
}

void plus(CgraphContext *ctx, float x, float y, float scale) /* make a plus sign tick */
{
  htick(ctx, x, y, scale);
  vtick(ctx, x, y, scale);
}

void TriangleAt(CgraphContext *ctx, float x, float y) /* make a filled triangle */
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   PHANDLER pfunc;
   float i, j, t;
   
   pfunc = getpoint(ctx);
   
   if (GetNorm(ctx, &x, &y) == 0.0) return;
   
   for (t=0.0, j=y+3.0*YUNIT(f); j>y-3.0*YUNIT(f); j--, t+=XUNIT(f))
      for (i=x-t; i<x+t; i+=XUNIT(f))
         (*pfunc)(i,j);
}

float setwidth(CgraphContext *ctx, float w)
{
  if (!ctx) return 10.0;
  
  float oldw = ctx->barwidth;
  ctx->barwidth = w;
  return(oldw);
}

void VbarsAt(CgraphContext *ctx, float x, float y) /* draw vertical bars */
{
    if (!ctx) return;
    FRAME *currentFrame = ctx->current_frame;
    
    PHANDLER pfunc;
    float i, j;

    pfunc = getpoint(ctx);

    if (GetNorm(ctx, &x, &y) == 0.0) return;

    for (j=currentFrame->yb; j<y; j++)
       for (i=x-ctx->barwidth/2.0; i<x+ctx->barwidth/2.0; i++)
          (*pfunc)(i,j);
}

void HbarsAt(CgraphContext *ctx, float x, float y) /* draw horizontal bars */
{
   if (!ctx) return;
   FRAME *currentFrame = ctx->current_frame;
   
   PHANDLER pfunc;
   float i, j;
   
   pfunc = getpoint(ctx);
   
   if (GetNorm(ctx, &x, &y) == 0.0) return;
   
   for (j=y+ctx->barwidth/2.0; j>y-ctx->barwidth/2.0; j--)
      for (i=currentFrame->xl; i<x; i++)
         (*pfunc)(i,j);
}

int code(FRAME *f, float x, float y)
{
   int c;
   c = 0;
   if(x < f->xl) c |= 1;
   if(y < f->yb) c |= 4;
   if(x > f->xr) c |= 2;
   if(y > f->yt) c |= 8;
   return(c);
}

void moveto(CgraphContext *ctx, float xarg, float yarg)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   
   if(f->mode)
      WINDOW(f,xarg,yarg);
   f->xpos = xarg;
   f->ypos = yarg;
   
   if (gbIsRecordingEnabled(ctx)) 
       record_gpoint(ctx, G_MOVETO, f->xpos, f->ypos);
}

void moverel(CgraphContext *ctx, float dxarg, float dyarg)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   
   if(f->mode)
      SCALEXY(f,dxarg,dyarg);
   f->xpos += dxarg;
   f->ypos += dyarg;
   if (gbIsRecordingEnabled(ctx)) 
       record_gpoint(ctx, G_MOVETO, f->xpos, f->ypos);
}

static int dclip(CgraphContext *ctx)
{
   if (!ctx) return -1;
   FRAME *f = ctx->current_frame;
   
   if (!f) return -1;
   
   float x1 = f->wx1;
   float y1 = f->wy1;
   float x2 = f->wx2;
   float y2 = f->wy2;
   int c1,c2;
   
   c2 = code(f,x2,y2);
   for(;;){
      c1 = code(f,x1,y1);
      if((c1 == 0) && (c2 == 0))
      {
	 f->wx1 = x1;
	 f->wy1 = y1;
	 f->wx2 = x2;
	 f->wy2 = y2;
	 f->c1 = c1;
	 f->c2 = c2;
	 return(0);
      }
      if(c1 & c2)
	 return(-1);
      if(!c1){
	 SWAP(c1,c2);   /* Works with new SWAP macro */
	 SWAP(x1,x2);
	 SWAP(y1,y2);
      }
      if(1 & c1){
	 y1 += MULDIV(y2-y1,f->xl-x1,x2-x1);
	 x1 = f->xl;
	 continue;
      }
      if(2 & c1){
	 y1 += MULDIV(y2-y1,f->xr-x1,x2-x1);
	 x1 = f->xr;
	 continue;
      }
      if(4 & c1){
	 x1 += MULDIV(x2-x1,f->yb-y1,y2-y1);
	 y1 = f->yb;
	 continue;
      }
      if(8 & c1){
	 x1 += MULDIV(x2-x1,f->yt-y1,y2-y1);
	 y1 = f->yt;
	 continue;
      }
   }
}
static int fclip(CgraphContext *ctx)
{
   if (!ctx) return -1;
   FRAME *f = ctx->current_frame;
   
   if (!f) return -1;
   
   float x1 = f->wx1;
   float y1 = f->wy1;
   float x2 = f->wx2;
   float y2 = f->wy2;
   int c1,c2;
   
   c2 = code(f,x2,y2);
   for(;;){
      c1 = code(f,x1,y1);
      if((c1 == 0) && (c2 == 0))
      {
	 f->wx1 = x1;
	 f->wy1 = y1;
	 f->wx2 = x2;
	 f->wy2 = y2;
	 f->c1 = c1;
	 f->c2 = c2;
	 return(0);
      }
      if(c1 & c2)
	 return(-1);
      if(!c1){
	 SWAP(c1,c2);   /* Works with new SWAP macro */
	 SWAP(x1,x2);
	 SWAP(y1,y2);
      }
      if(1 & c1){
	 x1 = f->xl;
	 continue;
      }
      if(2 & c1){
	 x1 = f->xr;
	 continue;
      }
      if(4 & c1){
	 y1 = f->yb;
	 continue;
      }
      if(8 & c1){
	 y1 = f->yt;
	 continue;
      }
   }
}

static void linutl(CgraphContext *ctx, float x, float y)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;

   PHANDLER dp = f->dpoint;
   LHANDLER dl = f->dline;
   float xx, yy, error;
   float index, incx, incy, step, startx, starty, endx, endy;

   if (!dp) return ;
   endx = xx = x;
   endy = yy = y;
   startx = x = f->xpos;
   starty = y = f->ypos;
   f->xpos = xx;
   f->ypos = yy;
   if (f->clipf) {
      f->wx1 = x;
      f->wy1 = y;
      f->wx2 = xx;
      f->wy2 = yy;
      if (dclip(ctx)) {
	/* 
	 * Even if the entire line is clipped, we need to moveto the
	 * destination for following lineto's...
	 */
	if (gbIsRecordingEnabled(ctx)) {
	  record_gpoint(ctx, G_MOVETO, f->xpos, f->ypos);
	}
	return;
      }
      else {
	 x = f->wx1;
	 y = f->wy1;
	 xx = f->wx2;
	 yy = f->wy2;
      }
   }
   if (dl){
      (*dl)(xx, yy, x, y);
      if (gbIsRecordingEnabled(ctx)) {

	 /*******************************************************************
	  *  Recording the lineto as a "lineto" requires that the correct
	  *  x,y pair be used.  Unfortunately, if clipping occurs, the
	  *  start and finish x,y pairs can be swapped in the computation.
	  *  For real lines this isn't a problem, because there's no
	  *  detectable difference between the "start" and "end" of a line. 
	  *  For lineto's, however, the line must continue to the new, possibly
	  *  clipped point, so a small calculation is performed to figure
	  *  out which pair of points is the actual "lineto" pair.  
	  *  We just figure out which pair is farther from the start pair
	  *  and use that as the new lineto pair.
	  ********************************************************************/
	 
 	 if (((startx-x)*(startx-x))+((starty-y)*(starty-y)) >
	     ((startx-xx)*(startx-xx))+((starty-yy)*(starty-yy))) {
	    SWAP(x, xx);
	    SWAP(y, yy);
	 }
	 /*
	  * If the starting coord was clipped, then we need a moveto
	  */
	 if (startx != x || starty != y) record_gpoint(ctx, G_MOVETO, x, y);
	 record_gpoint(ctx, G_LINETO, xx, yy);
	 if (xx != endx || yy != endy) record_gpoint(ctx, G_MOVETO, endx, endy);
      }
      return;
   }
   step = f->grain;
   incx = XUNIT(f);
   if ((xx -= x) < 0) {
      incx = -incx;
      xx = -xx;
   }
   incy = YUNIT(f);
   if ((yy -= y) < 0){
      incy = -incy;
      yy = -yy;
   }
   if (xx > yy){
      error = xx;
      error = error / 2.0;
      index = xx + XUNIT(f);
      for (;;) {
	 if (dp) (*dp)(x,y);
	 while (step-- > 0.0) {
	    error += yy;
	    if (error > xx){
	       error -= xx;
	       y += incy;
	    }
	    x += incx;
	    if(--index == 0.0)
	       return;
	 }
	 step = f->grain;
      }
   }
   else {
      error = yy / 2.0;
      index = yy + YUNIT(f);
      for (;;) {
	 if (dp) (*dp)(x,y);
	 while (step-- > 0.0){
	    error += xx;
	    if (error > yy) {
	       error -= yy;
	       x += incx;
	    }
	    y += incy;
	    if (--index <= 0.0)
	       return;
	 }
	 step = f->grain;
      }
   }
}

void linerel(CgraphContext *ctx, float dxarg, float dyarg)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   
   if (f->mode)
      SCALEXY(f, dxarg, dyarg);
   linutl(ctx, dxarg + f->xpos, dyarg + f->ypos);
}

void lineto(CgraphContext *ctx, float xarg, float yarg)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   
   if (f->mode)
      WINDOW(f, xarg, yarg);
   linutl(ctx, xarg, yarg);
}

void polyline(CgraphContext *ctx, int nverts, float *verts)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  
  FHANDLER polylinefunc = f->dpolyline;
  int i;
  
  /*
    This is not very good, but we're still battling with
     the clipping problem.  The basic issue is whether
     to record the line a G_POLY or as a series of G_LINETOs
  */
  if (f->dpolyline) {
    if (f->mode) {
      for(i = 0;i < nverts; i++)
	WINDOW(f, verts[2*i], verts[2*i+1]);
    }
  
    if (polylinefunc) {
      (*polylinefunc)(&verts[0], nverts);
    }
    if (gbIsRecordingEnabled(ctx)) record_gpoly(ctx, G_POLY, nverts*2, verts);
  }
  else {
    int i, j;
    moveto(ctx, verts[0], verts[1]);
    for (i = 1, j = 2; i < nverts; i++, j+=2) {
      lineto(ctx, verts[j], verts[j+1]);
    }
  }
}

void filledpoly(CgraphContext *ctx, int nverts, float *verts)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  
  int i;
  FHANDLER fillfunc = f->dfilledpoly;
  
  
  if (f->mode) {
    for(i = 0;i < nverts; i++)
      WINDOW(f, verts[2*i], verts[2*i+1]);
  }
  
  if (fillfunc) {
    (*fillfunc)(&verts[0], nverts);
  }

  if (gbIsRecordingEnabled(ctx)) record_gpoly(ctx, G_FILLEDPOLY, nverts*2, verts);
}

void filledrect(CgraphContext *ctx, float x1, float y1, float x2, float y2)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;

  FHANDLER fillfunc = f->dfilledpoly;
  float verts[8];

  if (f->mode) {
    WINDOW(f, x1, y1);
    WINDOW(f, x2, y2);
  }
  
  /* NOTE:  This is preliminary clipping code!  */
  if (f->clipf) {
    f->wx1 = x1;
    f->wy1 = y1;
    f->wx2 = x2;
    f->wy2 = y2;
    if (fclip(ctx))
      return;
  }

  if (fillfunc) {
    verts[0] = f->wx1;   verts[1] = f->wy1;
    verts[2] = f->wx2;   verts[3] = f->wy1;
    verts[4] = f->wx2;   verts[5] = f->wy2;
    verts[6] = f->wx1;   verts[7] = f->wy2;

    (*fillfunc)(&verts[0], 4);
  }

  if (gbIsRecordingEnabled(ctx))
    record_gline(ctx, G_FILLEDRECT, f->wx1, f->wy1, f->wx2, f->wy2);
}

/* make a hollow rectangle */
void rect(CgraphContext *ctx, float x1, float y1, float x2, float y2) 
{
  float verts[10];
  verts[0] = x1; verts[1] = y1;
  verts[2] = x1; verts[3] = y2;
  verts[4] = x2; verts[5] = y2;
  verts[6] = x2; verts[7] = y1;
  verts[8] = x1; verts[9] = y1;
  polyline(ctx, 5, verts);
}

void cleararea(CgraphContext *ctx, float x1, float y1, float x2, float y2)
{
  int oldcolor;
  
  oldcolor = setcolor(ctx, getbackgroundcolor(ctx));
  filledrect(ctx, x1, y1, x2, y2);
  setcolor(ctx, oldcolor);
}

void clearline(CgraphContext *ctx, float x1, float y1, float x2, float y2)
{
  int oldcolor;
  
  oldcolor = setcolor(ctx, getbackgroundcolor(ctx));
  moveto(ctx, x1, y1);
  lineto(ctx, x2, y2);
  setcolor(ctx, oldcolor);
}


void drawtextf(CgraphContext *ctx, char *str, ...)
{
  static char msg[1024];
  va_list arglist;
  
  va_start(arglist, str);
  vsprintf(msg, str, arglist);
  va_end(arglist);
  
  drawtext(ctx, msg);
}

 
void cleartextf(CgraphContext *ctx, char *str, ...)
{
  int oldcolor = setcolor(ctx, getbackgroundcolor(ctx));
  static char msg[1024];
  va_list arglist;

  va_start(arglist, str);
  vsprintf(msg, str, arglist);
  va_end(arglist);
  drawtext(ctx, msg);
  setcolor(ctx, oldcolor);
}



int strwidth(CgraphContext *ctx, char *str)
{
   if (!ctx) return -1;
   FRAME *f = ctx->current_frame;

   if (f->dchar && f->dstrwidth)
   	return((*f->dstrwidth)(str));
   else return(strlen(str)*f->colsiz);
}

int strheight(CgraphContext *ctx, char *str)
{
   if (!ctx) return -1;
   FRAME *f = ctx->current_frame;

   if (f->dchar && f->dstrheight)
   	return((*f->dstrheight)(str));
   else return(f->linsiz);
}

void cleartext(CgraphContext *ctx, char *str)
{
  int oldcolor = setcolor(ctx, getbackgroundcolor(ctx));
  drawtext(ctx, str);
  setcolor(ctx, oldcolor);
}


void drawtext(CgraphContext *ctx, char *string)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  
  int c, add_newline = 0;
  THANDLER dc = f->dchar;
  THANDLER dt = f->dtext;
  float xoffset = 0.0, yoffset = 0.0;
  
  if (strstr(string, "\n\r")) {
    string[strlen(string)-2] = 0;
    add_newline = 1;
  }
  
  else if (strstr(string, "\n")) {
    string[strlen(string)-1] = 0;
    add_newline = 1;
  }
  
  if (gbIsRecordingEnabled(ctx))
    record_gtext(ctx, G_TEXT, f->xpos, f->ypos, string);
  
  /*
   * now figure where text should begin based on current orientation
   * and justification
   */
  
  if (dt) {
    (*dt)(f->xpos,f->ypos,string);
    return;
  }
  
  switch (f->orientation) {
  case TXT_HORIZONTAL:
    switch(f->just) {
    case LEFT_JUST:
      yoffset -= strheight(ctx, string)/2.0;
      break;
    case RIGHT_JUST:
      xoffset -= strwidth(ctx, string);
      yoffset -= strheight(ctx, string)/2.0;
      break;
    case CENTER_JUST:
      xoffset -= strwidth(ctx, string) / 2.0;
      yoffset -= strheight(ctx, string)/2.0;
      break;
    }
    break;
  case TXT_VERTICAL:
    switch(f->just) {
    case CENTER_JUST:
      /*
       * presumably, the char handler can write rotated text so
       * we move DOWN to offset the text
       */
      if (dc) {
	xoffset -= strheight(ctx, string) / 2.0;
	yoffset -= strwidth(ctx, string) / 2.0;
      }
      /*
       * the char handler using the point hander, however, must 
       * back up, so we move UP half of the width of the string
       */
      else {
	xoffset -= f->colsiz / 2.0;
	yoffset += (f->linsiz*strlen(string)) / 2.0;
      }
      break;
    case LEFT_JUST:
      if (dc) {
	xoffset -= strheight(ctx, string) / 2.0;
      }
      else {
	yoffset += f->linsiz*strlen(string);
	xoffset -= f->colsiz / 2.0;
      }
      break;
    case RIGHT_JUST:
      if (dc) {
	xoffset -= strheight(ctx, string) / 2.0;
	yoffset -= strwidth(ctx, string);
      }
      else {
	xoffset -= f->colsiz / 2.0;
      }
      break;
    }
    break;
  case 2:
  case 3:
    break;
  }
  
  /* based on new position, now call drawing routines */
  
  if (dc) {
    (*dc)(f->xpos+xoffset,f->ypos+yoffset,string);
    if (add_newline) NXTLIN(f);
  }
  
  else {
    moverel(ctx, xoffset, yoffset);
    switch (f->orientation) {
    case TXT_HORIZONTAL:
      while((c = *string++)) drawchar(ctx, c);
      if (add_newline) {
	NXTLIN(f); LEFTMARG(f);
      }
      break;
    case TXT_VERTICAL:
      while((c = *string++)) {
	drawchar(ctx, c);
	moverel(ctx, -(f->colsiz), -(f->linsiz));
      }
    }
  }
}

static char chartab[90][5] = {
	{0000,0000,0276,0000,0000},	/* ! */
	{0000,0016,0000,0016,0000},	/* " */
	{0050,0356,0000,0356,0050},	/* # */
	{0110,0124,0326,0124,0044},	/* $ */
	{0306,0046,0026,0310,0306},	/* % */
	{0154,0222,0254,0100,0240},	/* & */
	{0000,0000,0016,0016,0000},	/* ' */
	{0000,0070,0104,0202,0000},	/* ( */
	{0000,0202,0104,0070,0000},	/* ) */
	{0114,0070,0174,0070,0114},	/* * */
	{0020,0020,0376,0020,0020},	/* + */
	{0000,0000,0260,0160,0000},	/* , */
	{0040,0040,0040,0040,0040},	/* - */
	{0000,0000,0300,0300,0000},	/* . */
	{0100,0040,0020,0010,0004},	/* / */
	{0000,0174,0202,0202,0174},	/* 0 */
	{0000,0204,0376,0200,0000},	/* 1 */
	{0344,0222,0222,0222,0214},	/* 2 */
	{0104,0202,0222,0222,0154},	/* 3 */
	{0060,0050,0044,0376,0040},	/* 4 */
	{0116,0212,0212,0212,0162},	/* 5 */
	{0170,0224,0222,0222,0140},	/* 6 */
	{0002,0342,0022,0012,0006},	/* 7 */
	{0154,0222,0222,0222,0154},	/* 8 */
	{0014,0222,0222,0122,0074},	/* 9 */
	{0000,0000,0154,0154,0000},	/* : */
	{0000,0200,0166,0066,0000},	/* ; */
	{0020,0050,0104,0202,0000},	/* < */
	{0050,0050,0050,0050,0050},	/* = */
	{0000,0202,0104,0050,0020},	/* > */
	{0000,0004,0242,0022,0014},	/* ? */
	{0144,0222,0362,0202,0174},	/* @ */
	{0370,0044,0042,0044,0370},	/* A */
	{0376,0222,0222,0222,0154},	/* B */
	{0174,0202,0202,0202,0104},	/* C */
	{0202,0376,0202,0202,0174},	/* D */
	{0376,0222,0222,0202,0202},	/* E */
	{0376,0022,0022,0002,0002},	/* F */
	{0174,0202,0202,0222,0362},	/* G */
	{0376,0020,0020,0020,0376},	/* H */
	{0000,0202,0376,0202,0000},	/* I */
	{0100,0200,0200,0200,0176},	/* J */
	{0376,0020,0050,0104,0202},	/* K */
	{0376,0200,0200,0200,0200},	/* L */
	{0376,0004,0030,0004,0376},	/* M */
	{0376,0004,0010,0020,0376},	/* N */
	{0376,0202,0202,0202,0376},	/* O */
	{0376,0022,0022,0022,0014},	/* P */
	{0174,0202,0242,0102,0274},	/* Q */
	{0376,0022,0062,0122,0214},	/* R */
	{0104,0212,0222,0242,0104},	/* S */
	{0002,0002,0376,0002,0002},	/* T */
	{0176,0200,0200,0200,0176},	/* U */
	{0016,0060,0300,0060,0016},	/* V */
	{0376,0100,0040,0100,0376},	/* W */
	{0306,0050,0020,0050,0306},	/* X */
	{0006,0010,0360,0010,0006},	/* Y */
	{0302,0242,0222,0212,0206},     /* Z */
	{0000,0376,0202,0202,0000},	/* [ */
	{0002,0004,0010,0020,0040},	/* \ */
	{0000,0202,0202,0376,0000},	/* ] */
	{0010,0004,0376,0004,0010},	/* ^ */
	{0020,0070,0124,0020,0020},	/* _ */
	{0000,0000,0000,0000,0000},	/*  */
	{0100,0250,0250,0250,0360},	/* a */
	{0366,0210,0210,0210,0160},	/* b */
	{0160,0210,0210,0210,0020},	/* c */
	{0160,0210,0210,0210,0366},	/* d */
	{0160,0250,0250,0250,0060},	/* e */
	{0010,0374,0012,0002,0004},	/* f */
	{0220,0250,0250,0360,0010},	/* g */
	{0366,0010,0010,0010,0360},	/* h */
	{0000,0210,0372,0200,0000},	/* i */
	{0000,0140,0200,0232,0140},	/* j */
	{0366,0050,0150,0250,0220},	/* k */
	{0000,0202,0376,0200,0000},	/* l */
	{0370,0010,0360,0010,0360},	/* m */
	{0370,0020,0010,0010,0360},	/* n */
	{0160,0210,0210,0210,0160},	/* o */
	{0370,0050,0050,0050,0020},	/* p */
	{0060,0110,0110,0310,0260},	/* q */
	{0370,0020,0010,0010,0020},	/* r */
	{0020,0250,0250,0250,0100},	/* s */
	{0000,0010,0176,0210,0100},	/* t */
	{0170,0200,0200,0100,0370},	/* u */
	{0070,0100,0200,0100,0070},	/* v */
	{0170,0200,0160,0200,0170},	/* w */
	{0210,0120,0040,0120,0210},	/* x */
	{0030,0240,0240,0240,0170},	/* y */
	{0210,0310,0250,0230,0210}	/* z */
	};


void drawchar(CgraphContext *ctx, int c)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;

   PHANDLER dcp = f->dclrpnt;
   PHANDLER dp = f->dpoint;
   THANDLER dc = f->dchar;
   if(f->clipf)
   {
      float x, y;
      x = f->xpos;
      y = f->ypos;
      if(code(f,x,y))
      {
	 NXTCOL(f);
	 return;
      }
      x += f->colsiz;
      y += f->linsiz;
      if(code(f,x,y))
      {
	 NXTCOL(f);
	 return;
      }
   }
   c &= 0377;
   
   if(c == 040 && !dcp){
      NXTCOL(f);
      return;
   }
   
   if(c < 040){
      switch(c){
	 case CR:        LEFTMARG(f);
			 return;
	 case LF:        NXTLIN(f);
			 return;
	 case TAB:       NXTCOL(f);
			 return;
	 case FF:        HOME(f);
			 return;
	 default:        NXTCOL(f);
			 return;
      }
   }
   if(dc)
   {
      char text[2];
      text[0] = c;
      text[1] = '\0';
      if (dc) (*dc)(f->xpos,f->ypos,text);
      NXTCOL(f);
   }
   else
   {
      float yp, yinc;
      float xp,xinc;
      char *table;
      int icol, irow, column;

      yp = f->ypos;
      xp = f->xpos;
      xinc = f->xinc;
      yinc = f->yinc;
      table = &(chartab[c-33][0]);
      
      if (dcp) {
	 for (icol = 0; icol < 5; icol++)
	 {
	    for (irow = 0; irow < 7; irow++) {
	       (*dcp)(xp,yp);
	       yp += yinc;
	    }
	    xp += xinc;
	    yp = f->ypos;
	 }
	 yp = f->ypos;
	 xp = f->xpos;
	 
	 if(c == 040) {
	    NXTCOL(f);
	    return;
	 }
      }
      
      for (icol = 0; icol < 5; icol++)
      {
	 column = table[icol];
	 if(column)
	 {
	    if(column & 0200)
	       if (dp) (*dp)(xp,yp);
	    column <<= 1;
	    yp += yinc;
	    if(column & 0200)
	       if (dp) (*dp)(xp,yp);
	    column <<= 1;
	    yp += yinc;
	    if(column & 0200)
	       if (dp) (*dp)(xp,yp);
	    column <<= 1;
	    yp += yinc;
	    if(column & 0200)
	       if (dp) (*dp)(xp,yp);
	    column <<= 1;
	    yp += yinc;
	    if(column & 0200)
	       if (dp) (*dp)(xp,yp);
	    column <<= 1;
	    yp += yinc;
	    if(column & 0200)
	       if (dp) (*dp)(xp,yp);
	    column <<= 1;
	    yp += yinc;
	    if(column & 0200)
	       if (dp) (*dp)(xp,yp);
	    column <<= 1;
	    yp += yinc;
	 }
	 xp += xinc;
	 yp = f->ypos;
      }
      NXTCOL(f);
      return;
   }
}

//#define MAX_STR_LEN 80
//static char s[MAX_STR_LEN];

void drawnum (CgraphContext *ctx, char *fmt, float n)
{
  if (!ctx) return;
  
  snprintf (ctx->draw_buffer, sizeof(ctx->draw_buffer), fmt, n);
  drawtext (ctx, ctx->draw_buffer);
}

void drawfnum (CgraphContext *ctx, int dpoints, float n)
{
  if (!ctx) return;
  
  char fmt[12];
  snprintf(fmt, sizeof(fmt), "%%.%df", dpoints); 
  snprintf (ctx->draw_buffer, sizeof(ctx->draw_buffer), fmt, n);
  drawtext (ctx, ctx->draw_buffer);
}

void drawf(CgraphContext *ctx, char *fmt, double n)
{
  if (!ctx) return;
  
  sprintf (ctx->draw_buffer, fmt, n);
  drawtext(ctx, ctx->draw_buffer);   
}

void HitRetKey(void)
{
  printf("Hit return to continue: ");
  getchar();
}

void copyframe(FRAME *from, FRAME *to)
{
  memcpy(to, from, sizeof(FRAME));
  to->fontname = (char *) malloc(strlen(from->fontname)+1);
  strcpy(to->fontname, from->fontname);
}

void frame(CgraphContext *ctx)
{
  if (!ctx) return; 
  FRAME *f = ctx->current_frame;
  
  int olduser;
  
  olduser = f->mode;               /* save user/device stat */
  user(ctx);
  moveto(ctx, f->xul,f->yub);
  lineto(ctx, f->xul,f->yut);
  lineto(ctx, f->xur,f->yut);
  lineto(ctx, f->xur,f->yub);
  lineto(ctx, f->xul,f->yub);
  setuser(ctx, olduser); 
}

void frameport(CgraphContext *ctx)    /* draw a frame around current viewport */
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  
  int olduser = f->mode;
  user(ctx);
  moveto(ctx, f->xl,f->yb);
  lineto(ctx, f->xl,f->yt);
  lineto(ctx, f->xr,f->yt);
  lineto(ctx, f->xr,f->yb);
  lineto(ctx, f->xl,f->yb);
  setuser(ctx, olduser);
}

void gfill(CgraphContext *ctx, float xl, float yl, float xh, float yh)     /* fill a screen region */
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  
  float x, y;
  float xsl, xsh, ysl, ysh;
  int saveuser;
  
  moveto(ctx, xl, yl);                 /* get screen coordinates       */
  xsl = f->xpos;
  ysl = f->ypos;
  moveto(ctx, xh, yh);
  xsh = f->xpos;
  ysh = f->ypos;
  saveuser = f->mode;
  f->mode = 0;
  for (x = xsl; x <= xsh; x++)    /* fill region                  */
    for (y = ysl; y <= ysh; y++)
      dotat(ctx, x, y);
  f->mode = saveuser;
}

int roundiv(int x, int y)
{
   x += y >> 1;
   return(x / y);
}


void tck(CgraphContext *ctx, char *title)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;

   if (!f) return;
   
   if (ctx->labeltick != 0) {
     setuser(ctx, 0);
     linerel(ctx, 0.0,-f->linsiz/2.0);
     moverel(ctx, f->colsiz/2.0,0.0);
     if (ctx->labeltick < 0)
       drawtext(ctx, title);
     setuser(ctx, 1);
   }
}


void tickat(CgraphContext *ctx, float x, float y, char *title)
{
  moveto(ctx, x,y);
  tck(ctx, title);
}

void screen(CgraphContext *ctx)
{
  setuser(ctx, 0);
  setclip(ctx, 0);
}

void user(CgraphContext *ctx)
{
  setuser(ctx, 1);
  setclip(ctx, 1);
}

void cross(CgraphContext *ctx)                     /* draw a cross at current pos  */
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;

   screen(ctx);
   moverel(ctx, -3.0*XUNIT(f),0.0);
   linerel(ctx, 6*XUNIT(f),0.0);
   moverel(ctx, -3*XUNIT(f),-3.0*YUNIT(f));
   linerel(ctx, 0.0,6.0*YUNIT(f));
   user(ctx);
}

void drawbox(CgraphContext *ctx, float xl, float yl, float xh, float yh)        /* draw a box */
{
  moveto(ctx, xl,yl);
  lineto(ctx, xl,yh);
  lineto(ctx, xh,yh);
  lineto(ctx, xh,yl);
  lineto(ctx, xl,yl);
}

void screen2window(CgraphContext *ctx, int x, int y, float *px, float *py)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  
  float argx, argy;
  
  argx = (float)x;
  argy = (float)y;
  SCREEN(f,argx,argy);
  *px = argx;
  *py = argy;
}

void window2screen(CgraphContext *ctx, int *px, int *py, float x, float y)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  
  float argx, argy;

  argx = x;
  argy = y;
  WINDOW(f,argx,argy);

  *px = (int)argx;
  *py = (int)argy;
}


void window_to_screen(CgraphContext *ctx, float x, float y, int *px, int *py)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  
  WINDOW(f, x, y);
  *px = (int) x;
  *py = (int) ((f->ysres-1) - y);
}

void screen_to_window(CgraphContext *ctx, int x, int y, float *px, float *py)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   
   float argx = x;
   float argy = ((f->ysres-1) - y);
   
   SCREEN(f,argx,argy);
   *px = argx;
   *py = argy;
}

void maketitle(CgraphContext *ctx, char *s, float x, float y)
{
  int olduser = setuser(ctx, 0);
  int oldj = setjust(ctx, 0);
  moveto(ctx, x,y);
  drawtext(ctx, s);
  setjust(ctx, oldj);
  setuser(ctx, olduser);
} 

void makeftitle(CgraphContext *ctx, char *s, float x, float y)
{
  if (!ctx) return;
  FRAME *f = ctx->current_frame;
  
  int olduser = setuser(ctx, 0);
  int oldj = setjust(ctx, 0);
  float x1 = f->xl + x*(f->xr - f->xl);
  float y1 = f->yb + y*(f->yt - f->yb);
  
  moveto(ctx, x1, y1);
  drawtext(ctx, s);
  setjust(ctx, oldj);
  setuser(ctx, olduser);
}
