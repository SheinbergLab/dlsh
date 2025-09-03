/************************************************************************
      axes.c - axis drawing utilities                                        
      Nikos K. Logothetis
      Modified David A. Leopold 14-APR-94
      Refactored for thread-safety with explicit context passing
      NOTE: lxaxis() and lyaxis() are now changed in that they no longer
            include their fifth argument, the justification. Instead, to
            make things more compatible with the postscript drawing, the
            text is drawn at the center of the axes, and the specific 
            centering work is done by setting a justification flag.
*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cgraph.h>
#include <lablib.h>

void x_tic_label(CgraphContext *ctx, FRAME *, float, float, float, int);
void drawxtic(CgraphContext *ctx, FRAME *, float , float , float);
void y_tic_label(CgraphContext *ctx, FRAME *, float, float, float, int);
void drawytic(CgraphContext *ctx, FRAME *, float , float , float);

void axes(CgraphContext *ctx, char *xlabel, char *ylabel)
{
   xaxis(ctx, xlabel);
   yaxis(ctx, ylabel);
}

void uboxaxes(CgraphContext *ctx)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   float tic;

   tic = f->xus / 10.0;
   lxaxis(ctx, f->yub, -tic, 0, NULL);
   tic = f->yus / 10.0;
   lyaxis(ctx, f->xul, -tic, 0, NULL);
   up_xaxis(ctx, NULL);
   right_yaxis(ctx, NULL);
}

void boxaxes(CgraphContext *ctx, char *xlabel, char *ylabel)
{
   xaxis(ctx, xlabel);
   yaxis(ctx, ylabel);
   up_xaxis(ctx, NULL);
   right_yaxis(ctx, NULL);
}

void xaxis(CgraphContext *ctx, char *label)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   float tic;
   
   tic = f->xus / 10.0;
   lxaxis(ctx, f->yub, -tic, 2, label);
}

void yaxis(CgraphContext *ctx, char *label)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   float tic;
   
   tic = f->yus / 10.0;
   lyaxis(ctx, f->xul, -tic, 2, label);
}

void up_xaxis(CgraphContext *ctx, char *label)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   float tic;
   
   tic = f->xus / 10.0;
   lxaxis(ctx, f->yut, -tic, 0, label);
}

void right_yaxis(CgraphContext *ctx, char *label)
{
   if (!ctx) return;
   FRAME *f = ctx->current_frame;
   float tic;

   tic = f->yus / 10.0;
   lyaxis(ctx, f->xur, -tic, 0, label);
}

int lxaxis(CgraphContext *ctx, float y, float tic, int ltic, char *label)
{
  if (!ctx) return 0;
  
  static FRAME localf;              /* local frame */
  FRAME *fp = &localf;              /* local pointer */
  FRAME *oldframe;
  float x, low, high;
  float yoffset;           /* to align text */
  float log_tic;
  int dec_points;
  int oldj;
  
  /*
   * Make estimate of reasonable # of decimal points to use based
   * on tic separation.  The algorithm will take log10(tic) and if
   *    > 1 : No decimal points
   *    > 0 : 1 dp
   *    < -0: -x
   */
  
  if (tic == 0.0) log_tic = 0.0;
  else log_tic = (float) log10((double) fabs(tic));
  if (log_tic > 1.0) dec_points = 0;
  else if (log_tic > 0.0) dec_points = 1;
  else dec_points = (int) fabs(log_tic) + 1;
  
  copyframe(ctx->current_frame, fp);
  oldframe = setstatus(ctx, fp);
  user(ctx);                           /* draw axis */
  setclip(ctx, 0);
  moveto(ctx, fp->xul, y);
  lineto(ctx, fp->xur, y);

  if (label) {                            /* label x axis */
    oldj = setjust(ctx, CENTER_JUST);
    moveto(ctx, (fp->xul + fp->xur) / 2.0, y);
    screen(ctx);

    yoffset = 2.0*fp->linsiz; 
    moverel(ctx, 0.0, -yoffset);

    if (ltic)
      moverel(ctx, 0.0, -4.0 * fp->linsiz / 2.0);
    drawtext(ctx, label);
    setjust(ctx, oldj);
  }
  if (tic == 0.0) {
    setstatus(ctx, oldframe);
    return(1);
  }
  
  low = MIN(fp->xul,fp->xur);   /* make tics */
  high = MAX(fp->xul,fp->xur);

  if((low <= 0.0) && (high >= 0.0)) {
    for (x = 0.0; x >= low; x -= fabs(tic)) drawxtic(ctx, fp, x, y, tic);
    for (x = 0.0; x <= high; x += fabs(tic)) drawxtic(ctx, fp, x, y, tic);
  }
  else if (high < 0.0 || low > 0.0)  
      for (x = low; x <= high; x += fabs(tic)) drawxtic(ctx, fp, x, y, tic);
    
  if (ltic) {  
    tic*=abs(ltic);
    if(low <= 0.0 && high >= 0.0) {
      if(low) {
        for (x = 0; x >= low; x -= fabs(tic)) 
          x_tic_label(ctx, fp, x, y, tic, dec_points);
        for (x = 0; x <= high; x += fabs(tic)) 
          x_tic_label(ctx, fp, x, y, tic, dec_points);
      } else {
        for (x = 0.0; x <= high; x += fabs(tic)) 
          x_tic_label(ctx, fp, x, y, tic, dec_points);
      } 
    } else {
        for (x = low; x <= high; x += fabs(tic)) 
          x_tic_label(ctx, fp, x, y, tic, dec_points);
    }
  }
  setstatus(ctx, oldframe);
  return(1);
}

int lyaxis(CgraphContext *ctx, float x, float tic, int ltic, char *label)
{
  if (!ctx) return 0;
  
  static FRAME localf;        /* local frame */
  FRAME *fp = &localf;        /* local pointer */
  FRAME *oldframe;
  float y, low, high;
  float xoffset;
  float log_tic;
  int dec_points;
  int tic_label_chars;        /* Number of chars in tic label */
  int oldj, oldo;
  
  /*
   * Make estimate of reasonable # of decimal points to use based
   * on tic separation.  The algorithm will take log10(tic) and if
   *    > 1 : No decimal points
   *    > 0 : 1 dp
   *    < -0: -x
   */
  
  if (tic == 0.0) log_tic = 0.0;
  else log_tic = (float) log10((double) fabs(tic));
  if (log_tic > 1.0) dec_points = 0;
  else if (log_tic > 0.0) dec_points = 1;
  else dec_points = (int) fabs(log_tic) + 1;
  
  copyframe(ctx->current_frame, fp);
  oldframe = setstatus(ctx, fp);
  user(ctx);                           /* draw axis */
  setclip(ctx, 0);
  moveto(ctx, x, fp->yub);
  lineto(ctx, x, fp->yut);

  /* figure out the MAX # of chars used for tic labels */
  if (fp->yus < 10.0) tic_label_chars = 2+dec_points;
  else tic_label_chars = (int) log10(fabs(fp->yus)) + (1+dec_points);
  
  if (label) {                            /* label y axis */
    moveto(ctx, x, (fp->yut + fp->yub) / 2.0);
    screen(ctx);

    /* move the label #chars + space + ticsize over to the left */
    
    xoffset = (tic_label_chars+1.5+1.5)*fp->colsiz;
    moverel(ctx, -xoffset, 0.0);

    oldj = setjust(ctx, CENTER_JUST);
    oldo = setorientation(ctx, 1);
    drawtext(ctx, label);
    setjust(ctx, oldj);
    setorientation(ctx, oldo);
  }
  if (tic == 0.0) {
    setstatus(ctx, oldframe);
    return(1);
  }
  
  low = MIN(fp->yub,fp->yut);   /* make tics */
  high = MAX(fp->yub,fp->yut);

  if((low <= 0.0) && (high >= 0.0)) {
    for (y = 0.0; y >= low; y -= fabs(tic)) drawytic(ctx, fp, x, y, tic);
    for (y = 0.0; y <= high; y += fabs(tic)) drawytic(ctx, fp, x, y, tic);
  } else if (high < 0.0 || low > 0.0)  
    for (y = low; y <= high; y += fabs(tic)) drawytic(ctx, fp, x, y, tic);

  if (ltic) {  
    tic *= abs(ltic);
    if(low <= 0.0 && high >= 0.0) {
      if(low) {
        for (y = 0; y >= low; y -= fabs(tic)) 
          y_tic_label(ctx, fp, x, y, tic, dec_points);
        for (y = 0; y <= high; y += fabs(tic)) 
          y_tic_label(ctx, fp, x, y, tic, dec_points);
      } else {
        for (y = 0.0; y <= high; y += fabs(tic)) 
          y_tic_label(ctx, fp, x, y, tic, dec_points);
      } 
    } else {
        for (y = low; y <= high; y += fabs(tic)) 
          y_tic_label(ctx, fp, x, y, tic, dec_points);
    }
  }
  setstatus(ctx, oldframe);

  return(1);
}

void drawxtic(CgraphContext *ctx, FRAME *fp, float x, float y, float tic)
{
    if (!ctx) return;
    user(ctx);
    moveto(ctx, x, y);
    screen(ctx);
    if (tic < 0.0)
      linerel(ctx, 0.0, -fp->linsiz / 2.0);
    else {
      moverel(ctx, 0.0, fp->linsiz / 2.0);
      linerel(ctx, 0.0, -fp->linsiz);
    }
}  

void x_tic_label(CgraphContext *ctx, FRAME *fp, float x, float y, float tic, int dp)
{
  if (!ctx) return;
  int oldo, oldj;
  
  user(ctx);
  moveto(ctx, x, y);
  screen(ctx);
  oldj = setjust(ctx, CENTER_JUST);
  oldo = setorientation(ctx, 0);
  if (tic < 0.0)
    linerel(ctx, 0.0, -fp->linsiz);
  else {
    moverel(ctx, 0.0, fp->linsiz);
    linerel(ctx, 0.0, -2.0*fp->linsiz);
  }
  moverel(ctx, 0.0, fp->linsiz);

  moverel(ctx, 0.0, -2.0*fp->linsiz);
  drawfnum(ctx, dp, x);
  setjust(ctx, oldj);
  setorientation(ctx, oldo);
} 

void drawytic(CgraphContext *ctx, FRAME *fp, float x, float y, float tic)
{
    if (!ctx) return;
    user(ctx);
    moveto(ctx, x, y);
    screen(ctx);
    if (tic < 0.0)
      linerel(ctx, -fp->colsiz/2.0, 0.0);
    else {
      moverel(ctx, fp->colsiz / 2.0, 0.0);
      linerel(ctx, -fp->colsiz, 0.0);
    }
}  

void y_tic_label(CgraphContext *ctx, FRAME *fp, float x, float y, float tic, int dp)
{
  if (!ctx) return;
  int oldj = setjust(ctx, RIGHT_JUST);
  user(ctx);
  moveto(ctx, x, y);
  screen(ctx);
  if (tic < 0.0)
    linerel(ctx, -fp->colsiz, 0.0);
  else {
    moverel(ctx, fp->colsiz, 0.0);
    linerel(ctx, -2.0*fp->colsiz, 0.0);
  }
  moverel(ctx, fp->colsiz, 0.0);

  moverel(ctx, -2.0*fp->colsiz, 0.0);
  drawfnum(ctx, dp, y);
  setjust(ctx, oldj);
}