/* gbufutl.c - graphics event processing utils */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#if defined (QNX) || defined (LYNX) || defined (FREEBSD) || defined (LINUX) 
#include <unistd.h>
#endif
#if defined (WIN32) || defined (WIN64)
#include <io.h>
#pragma warning (disable:4244)
#pragma warning (disable:4305)
#pragma warning (disable:4761)
#endif

/* PDF creation support using libharu */
#include <setjmp.h>
#include "hpdf.h"

#include "cgraph.h"
#include "utilc.h"
#include "gbuf.h"
#include "gbufutl.h"
#include "timer.h"
#include "rawapi.h"
#include "color.h"
#include "lablib.h"

#include "lodepng.h"

float GB_Version = 2.0;
static int FlipEvents = 0;
static int TimeStamped = 0, CurrentTimeStamp = 0;

char *BoundingBox = NULL;
static char PS_Stroking = 0, PS_Filling = 0, PS_Moveto = 0;
char PS_Orientation = PS_AUTOMATIC;
static float PS_linetox, PS_linetoy;
static float PS_curx, PS_cury;
static char PDF_Stroking = 0, PDF_Filling = 0, PDF_Moveto = 0;
static float PDF_linetox, PDF_linetoy;
static float PDF_curx, PDF_cury;

static void pdf_init(HPDF_Page page, float w, float h);
static void pdf_gsave(HPDF_Page page);
static void pdf_grestore(HPDF_Page page);
static void pdf_check_path(HPDF_Page page);
static void pdf_clip(HPDF_Page page, float x1, float y1, float x2, float y2);
static void pdf_postscript(HPDF_Page page, float scalex, float scaley,
			   char *fname, int just, int orientation);
static void pdf_image(HPDF_Doc pdf, HPDF_Page page,
		      float scalex, float scaley,
		      GBUF_IMAGE *img);
static void pdf_group(HPDF_Page page);
static void pdf_ungroup(HPDF_Page page);
static void pdf_point(HPDF_Page page, float x, float y);
static void pdf_circle(HPDF_Page page, float x, float y,
		       float size, float fill);
static void pdf_line(HPDF_Page page, float x1, float y1, float x2, float y2);
static void pdf_filled_poly (HPDF_Page page, int n, float *verts);
static void pdf_poly (HPDF_Page page, int n, float *verts);
static void pdf_filled_rect(HPDF_Page page, float x1, float y1,
			    float x2, float y2);
static void pdf_moveto(HPDF_Page page, float x, float y);
static void pdf_lineto(HPDF_Page page, float x, float y);
static void pdf_stroke(HPDF_Page page);
static void pdf_newpath(HPDF_Page page);
static void pdf_fill(HPDF_Page page);
static void pdf_text(HPDF_Page page, float x, float y,
		     char *str, char *fontname,
		     float fontsize, int just, int orientation);
static void pdf_setdash(HPDF_Page page, int lstyle);
static void pdf_setwidth(HPDF_Page page, int lwidth);
static void pdf_setcolor(HPDF_Page page, int color);
static void pdf_font(HPDF_Doc pdf, HPDF_Page page, char *fontname, float size);

static char *look_for_file(char *name);

/*
 * For portrait mode, fill page (1) or use actual viewport
 * as bounding box (0)
 */
static int PS_FillPage = 0;

static float Fontsize = 10.0;
static char  Fontname[80] = "Arial";

static int FigHeight;
static float FigScale = 4.0;

static int myexit(int ret)
{
  return ret;
}

static void
show_version(char *name)
{
  fprintf(stderr,"%s version %3.1f\n",name,GB_Version);
}

/*
 * Helper string function for drawtext
 */

static int strNumEscapes(char *str)
{
  int nEsc = 0;
  int i, len = strlen(str);
  for (i = 0; i < len; i++) {
    switch (str[i]) 
      {
      case '"':
      case '[':
      case ']':
      case '{':
      case '}':
	nEsc++;
      default:
	break;
      }
  }
  return nEsc;
}

static char *strEscapeString(char *inStr, char *outStr) {
  int i, len = strlen(inStr);
  char *p = outStr;
  for (i = 0; i < len; i++) {
    switch (inStr[i]) 
      {
      case '"':
      case '[':
      case ']':
      case '{':
      case '}':
	*p++ = '\\'; /* escape the " or [] character */;
      default:
	break;
      }
    *p++ = inStr[i];
  }
  *p = '\0';

  return outStr;
}


/*
 * Routines for dumping events to stdout
 */

void
read_gheader(FILE *InFP, FILE *OutFP)
{
  GHeader header;
  float t_vers;
  
  if (fread(&header, GHEADER_S, 1, InFP) != 1) {
     fprintf(stderr,"Error reading header info\n");
     myexit(-1);
  }

  /* 
   * The VERSION should stay as a float, so that byte ordering can be 
   * checked dynamically.  If it doesn't match the first way, then the
   * FlipEvents flag is set and it's tried again.
   */

  if (G_VERSION(&header) != GB_Version) {
    t_vers = G_VERSION(&header);
    FlipEvents = 1;
    flip_gheader(&header);
    if (G_VERSION(&header) != GB_Version) {
      fprintf(stderr,
	      "Unable to read this version of event data (V %5.1f/%5.1f)\n",
	      t_vers,G_VERSION(&header));
      myexit(-1);
    }
  }
  fprintf(OutFP,"# GRAPHICS VERSION\t%3.1f\n", G_VERSION(&header));
  fprintf(OutFP,"setwindow\t0\t0\t%6.2f\t%6.2f\n",
	  G_WIDTH(&header), G_HEIGHT(&header));
}

void
read_gline(char type, FILE *InFP, FILE *OutFP)
{
  GLine gline;

  if (fread(&gline, GLINE_S, 1, InFP) != 1) {
     fprintf(stderr,"Error reading line\n");
     myexit(-1);
  }
  
  if (FlipEvents) flip_gline(&gline);

  switch (type) {
     case G_FILLEDRECT: fprintf(OutFP,"filledrect\t"); break;
     case G_LINE: fprintf(OutFP,"line\t"); break;
     case G_CLIP: fprintf(OutFP,"setclipregion\t"); break;
     case G_CIRCLE: 
       if (GLINE_Y1(&gline) == 0.0) fprintf(OutFP,"circle\t"); 
       else fprintf(OutFP,"fcircle\t"); 
       break;
     case G_IMAGE: fprintf(OutFP,"image\t"); break;
  }
	
  fprintf(OutFP, "%6.2f %6.2f %6.2f %6.2f\n",
	  GLINE_X0(&gline), GLINE_Y0(&gline),
	  GLINE_X1(&gline), GLINE_Y1(&gline));
}

void
read_gpoint(char type, FILE *InFP, FILE *OutFP)
{
  GPoint gpoint;

  if (fread(&gpoint, GPOINT_S, 1, InFP) != 1) {
     fprintf(stderr,"Error reading point\n");
     myexit(-1);
  }
  
  if (FlipEvents) flip_gpoint(&gpoint);

  switch(type) {
     case G_POINT: fprintf(OutFP, "point\t"); break;
     case G_LINETO: fprintf(OutFP, "lineto\t"); break;
     case G_MOVETO: fprintf(OutFP, "moveto\t"); break;
  }		   
  fprintf(OutFP, "%6.2f %6.2f\n",
	  GPOINT_X(&gpoint), GPOINT_Y(&gpoint));
}

void
read_gpoly(char type, FILE *InFP, FILE *OutFP)
{
  GPointList gplst, *gpointlist = &gplst;
  int i;

  if (fread(gpointlist, GPOINTLIST_S, 1, InFP) != 1) {
    fprintf(stderr,"Error reading point list\n");
    myexit(-1);
  }
  
  if (FlipEvents) flip_gpointlist(gpointlist);
  
  if (!(GPOINTLIST_PTS(gpointlist) = 
	(float *) calloc(GPOINTLIST_N(gpointlist), sizeof(float)))) {
    fprintf(stderr,"Error allocating memory for float array\n");
    myexit(-1);
  }
  
  if (fread(GPOINTLIST_PTS(gpointlist), sizeof(float), 
	    GPOINTLIST_N(gpointlist), InFP) != 
      (unsigned) GPOINTLIST_N(gpointlist)) {
    fprintf(stderr,"Error reading float array\n");
    myexit(-1);
  }
  
  if (FlipEvents) flipfloats(GPOINTLIST_N(gpointlist), 
			     GPOINTLIST_PTS(gpointlist));
  
  switch(type) {
  case G_POLY: fprintf(OutFP, "poly"); break;
  case G_FILLEDPOLY: fprintf(OutFP, "fpoly"); break;
  }	
  for (i = 0; i < GPOINTLIST_N(gpointlist); i++) {
    fprintf(OutFP, " %6.2f", GPOINTLIST_PTS(gpointlist)[i]);
  }
  fprintf(OutFP, "\n");
  
  free(GPOINTLIST_PTS(gpointlist));
}

void
read_gtext(char type, FILE *InFP, FILE *OutFP)
{
  int n;
  GText gtext;

   if (fread(&gtext, GTEXT_S, 1, InFP) != 1) {
      fprintf(stderr,"Error reading text\n");
      myexit(-1);
   }
  
   if (FlipEvents) flip_gtext(&gtext);

   GTEXT_STRING(&gtext) = (char *) malloc(GTEXT_LENGTH(&gtext));

   
   if (fread(GTEXT_STRING(&gtext), GTEXT_LENGTH(&gtext), 1, InFP) != 1) {
      fprintf(stderr,"Error reading text string\n");
      myexit(-1);
   }

   switch(type) {
      case G_TEXT:
	if ((n = strNumEscapes(GTEXT_STRING(&gtext)))) {
	  char *newStr = (char *) malloc(strlen(GTEXT_STRING(&gtext))+ n);
	  strEscapeString(GTEXT_STRING(&gtext), newStr);
	  fprintf(OutFP, "drawtext\t\"%s\"\n", newStr);
	  free(newStr);
	}
	else fprintf(OutFP, "drawtext\t\"%s\"\n", GTEXT_STRING(&gtext));
	break;
      case G_FONT:
	 fprintf(OutFP, "setfont\t%s\t%6.2f\n",
		 GTEXT_STRING(&gtext), GTEXT_X(&gtext));
	 break;
      case G_POSTSCRIPT:
	 fprintf(OutFP, "postscript\t%s\t%6.2f\t%6.2f\n",
		 GTEXT_STRING(&gtext), GTEXT_X(&gtext), GTEXT_Y(&gtext));
	 break;
   }
   free(GTEXT_STRING(&gtext));
}   

void
read_gattr(char type, FILE *InFP, FILE *OutFP)
{
  GAttr gattr;

  if (fread(&gattr, GATTR_S, 1, InFP) != 1) {
     fprintf(stderr,"Error reading attribute\n");
     myexit(-1);
  }
  
  if (FlipEvents) flip_gattr(&gattr);

  switch (type) {
     case G_GROUP:
	fprintf(OutFP, "group\t");
	break;
     case G_SAVE:
	fprintf(OutFP, "gsave\t");
	break;
     case G_ORIENTATION:
	fprintf(OutFP, "setorientation\t");
	break;
     case G_JUSTIFICATION:
	fprintf(OutFP, "setjust\t");
	break;
     case G_COLOR:
	fprintf(OutFP, "setcolor\t");
	break;
     case G_LSTYLE:
	fprintf(OutFP, "setlstyle\t");
	break;
     case G_LWIDTH:
	fprintf(OutFP, "setlwidth\t");
	break;
     case G_TIMESTAMP:
	fprintf(OutFP, "timestamp\t");
	if (GATTR_VAL(&gattr)) TimeStamped = 1;
	else TimeStamped = 0;
	break;
  }
  fprintf(OutFP, "%5d\n", GATTR_VAL(&gattr));
}


/*
 * Routines for reading from a buffer & dumping events to stdout
 * These functions (which start with a g...) all return the number
 * of bytes which were accessed from the buffer.
 */

int
gread_gheader(GHeader *hdr, FILE *OutFP)
{
  GHeader head, *header = &head;
  memcpy(header, hdr, GHEADER_S);

   /* 
    * The VERSION should stay as a float, so that byte ordering can be 
    * checked dynamically.  If it doesn't match the first way, then the
    * FlipEvents flag is set and it's tried again.
    */

  if (G_VERSION(header) != GB_Version) {
     FlipEvents = 1;
  }
  else { 
    FlipEvents = 0;
  }
  
  if (FlipEvents) flip_gheader(header);
  
  if (G_VERSION(header) != GB_Version) {
     fprintf(stderr,
	     "Sorry, unable to read this version of event data (V %f)\n",
	     G_VERSION(header));
     myexit(-1);
  }
  
  else {
     fprintf(OutFP, "# GRAPHICS VERSION\t%3.1f\n", G_VERSION(header));
     fprintf(OutFP, "setwindow\t0\t0\t%6.2f\t%6.2f\n",
	     G_WIDTH(header), G_HEIGHT(header));
  }
  return(GHEADER_S);
}

int
gread_gline(char type, GLine *gln, FILE *OutFP)
{
  GLine gli, *gline = &gli;
  memcpy(gline, gln, GLINE_S);
  
  if (FlipEvents) flip_gline(gline);
  
  switch (type) {
  case G_FILLEDRECT: fprintf(OutFP,"filledrect\t"); break;
  case G_LINE: fprintf(OutFP,"line\t"); break;
  case G_CLIP: fprintf(OutFP,"setclipregion\t"); break;
  case G_CIRCLE: 
    if (GLINE_Y1(gline) == 0.0) 
      fprintf(OutFP,"circle\t"); 
    else 
      fprintf(OutFP,"fcircle\t"); 
    break;
  case G_IMAGE: fprintf(OutFP,"image\t"); break;
  }
	
  fprintf(OutFP, "%6.2f %6.2f %6.2f %6.2f\n",
	  GLINE_X0(gline), GLINE_Y0(gline),
	  GLINE_X1(gline), GLINE_Y1(gline));
  return(GLINE_S);
}

int
gread_gpoly(char type, GPointList *gpl, FILE *OutFP)
{
  int i;
  GPointList gplst, *gpointlist = &gplst;
  memcpy(gpointlist, gpl, GPOINTLIST_S);
  if (FlipEvents) flip_gpointlist(gpointlist);
  
  if (!(GPOINTLIST_PTS(gpointlist) = 
	(float *) calloc(GPOINTLIST_N(gpointlist), sizeof(float)))) {
    fprintf(stderr,"Error allocating memory for float array\n");
    myexit(-1);
  }
  
  memcpy(GPOINTLIST_PTS(gpointlist), (char *)gpl+GPOINTLIST_S,
	 GPOINTLIST_N(gpointlist)*sizeof(float));
  
  if (FlipEvents) flipfloats(GPOINTLIST_N(gpointlist), 
			     GPOINTLIST_PTS(gpointlist));
  
  switch(type) {
  case G_POLY: fprintf(OutFP, "poly"); break;
  case G_FILLEDPOLY: fprintf(OutFP, "fpoly"); break;
  }	
  for (i = 0; i < GPOINTLIST_N(gpointlist); i++) {
    fprintf(OutFP, " %6.2f", GPOINTLIST_PTS(gpointlist)[i]);
  }
  fprintf(OutFP, "\n");
  
  free(GPOINTLIST_PTS(gpointlist));
  
  return(GPOINTLIST_S+GPOINTLIST_N(gpointlist)*sizeof(float));
}

int
gread_gpoint(char type, GPoint *gpt, FILE *OutFP)
{
  GPoint gpnt, *gpoint = &gpnt;
  memcpy(gpoint, gpt, GPOINT_S);

  if (FlipEvents) flip_gpoint(gpoint);

  switch(type) {
     case G_POINT: fprintf(OutFP, "point\t"); break;
     case G_LINETO: fprintf(OutFP, "lineto\t"); break;
     case G_MOVETO: fprintf(OutFP, "moveto\t"); break;
  }		   
  
  fprintf(OutFP, "%6.2f %6.2f\n",
	  GPOINT_X(gpoint), GPOINT_Y(gpoint));

  return(GPOINT_S);
}

int
gread_gtext(char type, GText *gtx, FILE *OutFP)
{
  int n;
  GText gtxt, *gtext = &gtxt;
  memcpy(gtext, gtx, GTEXT_S);

  if (FlipEvents) flip_gtext(gtext);
   
  GTEXT_STRING(gtext) = (char *) ((char *)gtx+GTEXT_S);

  switch(type) {
  case G_TEXT:
    if ((n = strNumEscapes(GTEXT_STRING(gtext)))) {
      char *newStr = (char *) malloc(strlen(GTEXT_STRING(gtext))+ n);
      strEscapeString(GTEXT_STRING(gtext), newStr);
      fprintf(OutFP, "drawtext\t\"%s\"\n", newStr);
      free(newStr);
    }
    else fprintf(OutFP, "drawtext\t\"%s\"\n", GTEXT_STRING(gtext));
    break;
  case G_FONT:
    /* Font pointsize stored in X slot */
    fprintf(OutFP, "setfont\t%s\t%6.2f\n",
	    GTEXT_STRING(gtext), GTEXT_X(gtext));
    break;
  case G_POSTSCRIPT:
    fprintf(OutFP, "postscript\t%s\t%6.2f\t%6.2f\t\n",
	    GTEXT_STRING(gtext), GTEXT_X(gtext), GTEXT_Y(gtext));
    break;
  }
  return(GTEXT_S+GTEXT_LENGTH(gtext));
}   

int
gread_gattr(char type, GAttr *gtr, FILE *OutFP)
{
  GAttr gatr, *gattr = &gatr;
  memcpy(gattr, gtr, GATTR_S);

  if (FlipEvents) flip_gattr(gattr);

  switch (type) {
     case G_GROUP:
	fprintf(OutFP, "group\t");
	break;
     case G_SAVE:
	fprintf(OutFP, "gsave\t");
	break;
     case G_ORIENTATION:
	fprintf(OutFP, "setorientation\t");
	break;
     case G_JUSTIFICATION:
	fprintf(OutFP, "setjust\t");
	break;
     case G_COLOR:
	fprintf(OutFP, "setcolor\t");
	break;
     case G_LSTYLE:
	fprintf(OutFP, "setlstyle\t");
	break;
     case G_LWIDTH:
	fprintf(OutFP, "setlwidth\t");
	break;
     case G_TIMESTAMP:
	fprintf(OutFP, "timestamp\t");
	if (GATTR_VAL(gattr)) TimeStamped = 1;
	else TimeStamped = 0;
	break;
  }
  fprintf(OutFP, "%5d\n", GATTR_VAL(gattr));
  return(GATTR_S);
}



/*
 * Routines to SKIP events from FILE *
 */

void
skip_gheader(FILE *InFP)
{
  GHeader header;

  if (fread(&header, GHEADER_S, 1, InFP) != 1) {
	fprintf(stderr,"Error reading header info\n");
	myexit(-1);
  }

  if (G_VERSION(&header) != GB_Version) {
     FlipEvents = 1;
  }
  
  if (FlipEvents) flip_gheader(&header);
  
  if (G_VERSION(&header) != GB_Version) {
     fprintf(stderr,
	     "Sorry, unable to read this graphics file\n");
     myexit(-1);
  }
}

void
skip_gline(FILE *InFP)
{
   if (fseek(InFP, (int) GLINE_S, SEEK_CUR)) {
      fprintf(stderr,"Error skipping gline info\n");
      myexit(-1);
   }
}

void
skip_gpoint(FILE *InFP)
{
   if (fseek(InFP, (int) GPOINT_S, SEEK_CUR)) {
      fprintf(stderr,"Error skipping gpoint info\n");
      myexit(-1);
   }
}

void
skip_gattr(FILE *InFP)
{
   if (fseek(InFP, (int) GATTR_S, SEEK_CUR)) {
      fprintf(stderr,"Error skipping attribute info\n");
      myexit(-1);
   }
}


void
skip_gtext(FILE *InFP)
{
   GText gtext;

   if (fread(&gtext, GTEXT_S, 1, InFP) != 1) {
      fprintf(stderr,"Error reading text\n");
      myexit(-1);
   }
   
   if (FlipEvents) flip_gtext(&gtext);
   
   if (fseek(InFP, (int) GTEXT_LENGTH(&gtext), SEEK_CUR)) {
      fprintf(stderr,"Error skipping text string\n");
      myexit(-1);
   }
}   


void
skip_gpoly(FILE *InFP)
{
  GPointList gpl, *gpointlist = &gpl;
  if (fread(gpointlist, GPOINTLIST_S, 1, InFP) != 1) {
    fprintf(stderr,"Error reading point list\n");
    myexit(-1);
  }
  
  if (FlipEvents) flip_gpointlist(gpointlist);
   
  if (fseek(InFP, (int) GPOINTLIST_N(gpointlist)*sizeof(float), SEEK_CUR)) {
    fprintf(stderr,"Error skipping text string\n");
    myexit(-1);
  }
}   


/*
 * Routines to SKIP events from a buffer.
 * Each function just returns the number of bytes to skip.
 */

int
gskip_gheader(GHeader *hdr)
{
  GHeader head, *header = &head;
  memcpy(header, hdr, GHEADER_S);

   if (G_VERSION(header) != GB_Version) {
      FlipEvents = 1;
   }
   
   if (FlipEvents) flip_gheader(header);
   
   if (G_VERSION(header) != GB_Version) {
      fprintf(stderr,
	      "Sorry, unable to read this graphics file\n");
      myexit(-1);
   }
   return(GHEADER_S);
}

int
gskip_gline(GLine *gline)
{
   return(GLINE_S);
}

int
gskip_gpoint(GPoint *gpoint)
{
   return(GPOINT_S);
}

int
gskip_gattr(GAttr *gattr)
{
   return(GATTR_S);
}

int
gskip_gtext(GText *gtx)
{
  GText gtxt, *gtext = &gtxt;
  memcpy(gtext, gtx, GTEXT_S);
  
  if (FlipEvents) flip_gtext(gtext);
  
  return(GTEXT_S+GTEXT_LENGTH(gtext));
}

int
gskip_gpoly(GPointList *gpl)
{
  GPointList gplst, *gpointlist = &gplst;
  memcpy(gpointlist, gpl, GPOINTLIST_S);
  if (FlipEvents) flip_gpointlist(gpointlist);
  
  return(GPOINTLIST_S+GPOINTLIST_N(gpointlist)*sizeof(float));
}

/*
 * Routines for getting events from a FILE *
 */


void
get_gheader(FILE *InFP, float *version, float *width, float *height)
{
   GHeader header;
   
   if (fread(&header, GHEADER_S, 1, InFP) != 1) {
      fprintf(stderr,"Error reading header info\n");
      myexit(-1);
   }
   
   /* 
    * The VERSION should stay as a float, so that byte ordering can be 
    * checked dynamically.  If it doesn't match the first way, then the
    * FlipEvents flag is set and it's tried again.
    */
   
   if (G_VERSION(&header) != GB_Version) {
      FlipEvents = 1;
   }
   
   if (FlipEvents) flip_gheader(&header);
   
   if (G_VERSION(&header) != GB_Version) {
      fprintf(stderr,
	      "Sorry, unable to read this version of event data (V %f)\n",
	      G_VERSION(&header));
      myexit(-1);
   }
  
   else {
      *version = G_VERSION(&header);
      *width = G_WIDTH(&header);
      *height = G_HEIGHT(&header);
   }
}
   

void
get_gline(FILE *InFP,  float *x0, float *y0, float *x1, float *y1)
{
   GLine gline;
   
   if (fread(&gline, GLINE_S, 1, InFP) != 1) {
      fprintf(stderr,"Error reading line\n");
      myexit(-1);
   }
   
   if (FlipEvents) flip_gline(&gline);

   *x0 = GLINE_X0(&gline);
   *y0 = GLINE_Y0(&gline);
   *x1 = GLINE_X1(&gline);
   *y1 = GLINE_Y1(&gline);
}

void
get_gpoint(FILE *InFP,  float *x, float *y)
{
   GPoint gpoint;
   
   if (fread(&gpoint, GPOINT_S, 1, InFP) != 1) {
      fprintf(stderr,"Error reading point\n");
	 myexit(-1);
   }
   
   if (FlipEvents) flip_gpoint(&gpoint);
   
   *x = GPOINT_X(&gpoint);
   *y = GPOINT_Y(&gpoint);
}



void
get_gattr(char type, FILE *InFP, int *val)
{
  GAttr gattr;

  if (fread(&gattr, GATTR_S, 1, InFP) != 1) {
     fprintf(stderr,"Error reading attribute\n");
     myexit(-1);
  }
  
  if (FlipEvents) flip_gattr(&gattr);

  *val = GATTR_VAL(&gattr);
}

int
get_timestamp(FILE *InFP, int *time)
{
   if (fread((char *)time, sizeof(int), 1, InFP) != 1) {
      return(0);
   }
  
  if (FlipEvents) *time = fliplong(*time);
  return(1);
}
   

void
get_gtext(FILE *InFP, float *x, float *y, int *length, char **str)
{
   GText gtext;

   if (fread(&gtext, GTEXT_S, 1, InFP) != 1) {
      fprintf(stderr,"Error reading text\n");
      myexit(-1);
   }
  
   if (FlipEvents) flip_gtext(&gtext);

   GTEXT_STRING(&gtext) = (char *) malloc(GTEXT_LENGTH(&gtext));

   if (fread(GTEXT_STRING(&gtext), GTEXT_LENGTH(&gtext), 1, InFP) != 1) {
      fprintf(stderr,"Error reading text string\n");
      myexit(-1);
   }
   
   *x = GTEXT_X(&gtext);
   *y = GTEXT_Y(&gtext);
   *length = GTEXT_LENGTH(&gtext);
   *str = GTEXT_STRING(&gtext);
}   

void
get_gpoly(FILE *InFP, int *n, float **points)
{
  GPointList gplst, *gpointlist = &gplst;
  if (fread(gpointlist, GPOINTLIST_S, 1, InFP) != 1) {
    fprintf(stderr,"Error reading point list\n");
    myexit(-1);
  }
  
  if (FlipEvents) flip_gpointlist(gpointlist);

  if (!(GPOINTLIST_PTS(gpointlist) = 
	(float *) calloc(GPOINTLIST_N(gpointlist), sizeof(float)))) {
    fprintf(stderr,"Error allocating memory for float array\n");
    myexit(-1);
  }
  
  if (fread(GPOINTLIST_PTS(gpointlist), sizeof(float), 
	    GPOINTLIST_N(gpointlist), InFP) != 
      (unsigned) GPOINTLIST_N(gpointlist)) {
    fprintf(stderr,"Error reading float array\n");
    myexit(-1);
  }
  
  if (FlipEvents) flipfloats(GPOINTLIST_N(gpointlist), 
			     GPOINTLIST_PTS(gpointlist));
  
  *n = GPOINTLIST_N(gpointlist);
  *points = GPOINTLIST_PTS(gpointlist);
}

/*
 * Routines to GET events from event buffer.
 * Each function returns the number of bytes to advance the buffer.
 */

int
gget_gheader(GHeader *hdr, float *version, float *width, float *height)
{
  GHeader head, *header = &head;
  memcpy(header, hdr, GHEADER_S);
  /* 
    * The VERSION should stay as a float, so that byte ordering can be 
    * checked dynamically.  If it doesn't match the first way, then the
    * FlipEvents flag is set and it's tried again.
    */
   
  if (G_VERSION(header) != GB_Version) {
    FlipEvents = 1;
  }
  else FlipEvents = 0;
   
  if (FlipEvents) flip_gheader(header);
  
  if (G_VERSION(header) != GB_Version) {
    fprintf(stderr,
	    "Sorry, unable to read this version of event data (V %f)\n",
	    G_VERSION(header));
    myexit(-1);
  }
  
  else {
    *version = G_VERSION(header);
    *width = G_WIDTH(header);
    *height = G_HEIGHT(header);
  }
  return(GHEADER_S);
}
   

int
gget_gline(GLine *gln, float *x0, float *y0, float *x1, float *y1)
{
  GLine gli, *gline = &gli;
  memcpy(gline, gln, GLINE_S);
  
  if (FlipEvents) flip_gline(gline);
  
  *x0 = GLINE_X0(gline);
  *y0 = GLINE_Y0(gline);
  *x1 = GLINE_X1(gline);
  *y1 = GLINE_Y1(gline);
  
  return(GLINE_S);
}

int
gget_gpoint(GPoint *gpt,  float *x, float *y)
{
  GPoint gpnt, *gpoint = &gpnt;
  memcpy(gpoint, gpt, GPOINT_S);

  if (FlipEvents) flip_gpoint(gpoint);
	 
  *x = GPOINT_X(gpoint);
  *y = GPOINT_Y(gpoint);
  
  return(GPOINT_S);
}


int
gget_gattr(char type, GAttr *gtr, int *val)
{
  GAttr gatr, *gattr = &gatr;
  memcpy(gattr, gtr, GATTR_S);
  
  if (FlipEvents) flip_gattr(gattr);

  *val = GATTR_VAL(gattr);
  return(GATTR_S);
}


int
gget_gtext(GText *gtx, float *x, float *y, int *length, char **str)
{
  GText gtxt, *gtext = &gtxt;
  char *string;

  memcpy(gtext, gtx, GTEXT_S);

  if (FlipEvents) flip_gtext(gtext);

  GTEXT_STRING(gtext) = (char *) ((char *)gtx+GTEXT_S);
  string = malloc(strlen(GTEXT_STRING(gtext))+1);
  strcpy(string, GTEXT_STRING(gtext));
  
  *x = GTEXT_X(gtext);
  *y = GTEXT_Y(gtext);
  *length = GTEXT_LENGTH(gtext);
  *str = string;
  
  return(GTEXT_S+GTEXT_LENGTH(gtext));
}   


int
gget_gpoly(GPointList *gpl, int *n, float **points)
{
  GPointList gplst, *gpointlist = &gplst;
  memcpy(gpointlist, gpl, GPOINTLIST_S);
  if (FlipEvents) flip_gpointlist(gpointlist);
  
  if (!(GPOINTLIST_PTS(gpointlist) = 
	(float *) calloc(GPOINTLIST_N(gpointlist), sizeof(float)))) {
    fprintf(stderr,"Error allocating memory for float array\n");
    myexit(-1);
  }
  
  memcpy(GPOINTLIST_PTS(gpointlist), (char *)gpl+GPOINTLIST_S,
	 GPOINTLIST_N(gpointlist)*sizeof(float));
  
  if (FlipEvents) flipfloats(GPOINTLIST_N(gpointlist), 
			     GPOINTLIST_PTS(gpointlist));
  
  *n = GPOINTLIST_N(gpointlist);
  *points = GPOINTLIST_PTS(gpointlist);
  
  return(GPOINTLIST_S+GPOINTLIST_N(gpointlist)*sizeof(float));
}

/*********************************************************************/
/*         Flip Functions to Correct for Byte Order Differences      */
/*********************************************************************/

void
flip_gheader(GHeader *header)
{
  G_VERSION(header) = flipfloat(G_VERSION(header));
  G_WIDTH(header) = flipfloat(G_WIDTH(header));
  G_HEIGHT(header) = flipfloat(G_HEIGHT(header));
}

void
flip_gpoint(GPoint *gpoint)
{
   GPOINT_X(gpoint) = flipfloat(GPOINT_X(gpoint));
   GPOINT_Y(gpoint) = flipfloat(GPOINT_Y(gpoint));
}

void
flip_gattr(GAttr *gattr)
{
   GATTR_VAL(gattr) = fliplong(GATTR_VAL(gattr));
}

void
flip_gline(GLine *gline)
{
   GLINE_X0(gline) = flipfloat(GLINE_X0(gline));
   GLINE_X1(gline) = flipfloat(GLINE_X1(gline));
   GLINE_Y0(gline) = flipfloat(GLINE_Y0(gline));
   GLINE_Y1(gline) = flipfloat(GLINE_Y1(gline));
}

void
flip_gtext(GText *gtext)
{
   GTEXT_X(gtext) = flipfloat(GTEXT_X(gtext));
   GTEXT_Y(gtext) = flipfloat(GTEXT_Y(gtext));
   GTEXT_LENGTH(gtext) = fliplong(GTEXT_LENGTH(gtext));
}

void
flip_gpointlist(GPointList *gpointlist)
{
  GPOINTLIST_N(gpointlist) = fliplong(GPOINTLIST_N(gpointlist));
}

/*************************************************************************/
/*                           Output Functions                            */
/*************************************************************************/

void gbuf_dump(char *buffer, int n, int type, FILE *fp)
{
   switch(type) {
      case GBUF_RAW:
	 fwrite(buffer, sizeof(unsigned char), n, fp);
	 fflush(fp);
	 break;
      case GBUF_ASCII:
	gbuf_dump_ascii((unsigned char *) buffer, n, fp);
	 break;
      case GBUF_AI88:
	 gbuf_dump_ps((unsigned char *) buffer, n, type, fp);
	 add_ai88_trailer(fp);
	 break;
      case GBUF_AI3:
	 gbuf_dump_ps((unsigned char *) buffer, n, type, fp);
	 add_ai3_trailer(fp);
	 break;
      case GBUF_EPS:
      case GBUF_PS:
	 gbuf_dump_ps((unsigned char *) buffer, n, type, fp);
	 add_ps_trailer(fp);
	 break;
       case GBUF_FIG:
	 gbuf_dump_fig((unsigned char *) buffer, n, type, fp);
	 break;
      default:
	 break;
   }
}

/*********************************************************************/
/************************ LIBHARU PDF OUTPUT *************************/
/*********************************************************************/

/*
 * "exception" handling using setjmp/longjmp
 */
static jmp_buf pdf_env;

//#define HARU_ERROR_LOG "/tmp/haru_errors.log"
//#define HARU_LOG "/tmp/haru.log"

#ifdef HPDF_DLL
void __stdcall
#else
void
#endif
static haru_error_handler  (HPDF_STATUS   error_no,
			    HPDF_STATUS   detail_no,
			    void         *user_data)
{
#ifdef HARU_ERROR_LOG
  FILE *errorlog;
  errorlog = fopen(HARU_ERROR_LOG, "a");
  fprintf (errorlog, "ERROR: error_no=%04X, detail_no=%u\n",
	   (HPDF_UINT) error_no, (HPDF_UINT) detail_no);
  fclose(errorlog);
#endif
  longjmp(pdf_env, 1);
}

static int haru_log(char *op) {
#define HARU_LOG "/tmp/haru.log"
#ifdef HARU_LOG
  FILE *log;
  log = fopen(HARU_LOG, "a");
  if (!log) return 0;
  fprintf (log, "LOG: %s\n", op);
  fclose(log);
#endif
  return 1;
}

int gbuf_dump_pdf(char *gbuf, int bufsize, char *filename)
{
  HPDF_Doc  pdf;
  HPDF_Page page;
  
  pdf = HPDF_New (haru_error_handler, NULL);
  if (!pdf) {
    printf ("error: cannot create PdfDoc object\n");
    return 1;
  }
  
  if (setjmp(pdf_env)) {
    HPDF_Free (pdf);
    return 1;
  }

  HPDF_SetCompressionMode (pdf, HPDF_COMP_ALL);
  
  page = HPDF_AddPage(pdf);
  int clipping = 0;
  
  {
    int c, n;
    int orientation = 0, lstyle = 0, color = 1, just = 0, lwidth = 1;
    float w, h;
    float x0, y0, x1, y1, *points;
    int length, save, group;
    float version;
    char *string;
    int i, advance_bytes = 0;
    
    for (i = 0; i < bufsize; i+=advance_bytes) {
      c = gbuf[i++];
      if (TimeStamped) {
	CurrentTimeStamp = (int) gbuf[i];
	i+=sizeof(int);
      }
      if (PDF_Moveto == 1) {
	switch(c) {		
	case G_LINETO:		
	case G_ORIENTATION:
	case G_JUSTIFICATION:
	case G_COLOR:
	case G_LSTYLE:
	case G_LWIDTH:
	  break;
	default:
	  pdf_newpath(page);
	  break;
	}
      }
      switch (c) {
      case G_HEADER:
	advance_bytes = gget_gheader((GHeader *) &gbuf[i],
				     &version, &w, &h);
	pdf_init(page, w, h);
	pdf_gsave(page);
	break;

      case G_CLIP:
	advance_bytes = gget_gline((GLine *) &gbuf[i],
				   &x0, &y0, &x1, &y1);
	pdf_check_path(page);
	if (clipping) pdf_grestore(page);
	pdf_gsave(page);

	/* Set things that may have just gotten popped off */
	pdf_font(pdf, page, Fontname, Fontsize);
	pdf_setdash(page, lstyle);
	pdf_setwidth(page, lwidth);
	pdf_setcolor(page, color);

	pdf_clip(page, x0, y0, x1, y1);
	clipping = 1;
	break;
      case G_FILLEDPOLY:
	pdf_check_path(page);
	advance_bytes = gget_gpoly((GPointList *) &gbuf[i], &n, &points);
	pdf_filled_poly(page, n, points);
	free(points);
	break;
      case G_POLY:
	pdf_check_path(page);
	advance_bytes = gget_gpoly((GPointList *) &gbuf[i], &n, &points);
	pdf_poly(page, n, points);
	free(points);
	break;
      case G_FILLEDRECT:
	pdf_check_path(page);
	advance_bytes = gget_gline((GLine *) &gbuf[i],
				   &x0, &y0, &x1, &y1);
	pdf_filled_rect(page, x0, y0, x1, y1);
	break;
      case G_LINE:
	pdf_check_path(page);
	advance_bytes = gget_gline((GLine *) &gbuf[i],
				   &x0, &y0, &x1, &y1);
	pdf_line(page, x0, y0, x1, y1);
	break;
      case G_CIRCLE:
	pdf_check_path(page);
	advance_bytes = gget_gline((GLine *) &gbuf[i],
				   &x0, &y0, &x1, &y1);
	pdf_circle(page, x0, y0, x1, y1);
	break;
      case G_LINETO:
	advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
	pdf_lineto(page, x0, y0);
	PDF_Stroking = 1;
	break;
      case G_MOVETO:
	pdf_check_path(page);
	advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
	pdf_moveto(page, x0, y0);
	PDF_Moveto = 1;
	break;
      case G_POINT:
	pdf_check_path(page);
	advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
	pdf_point(page, x0, y0);
	break;
      case G_TEXT:
	pdf_check_path(page);
	advance_bytes = gget_gtext((GText *) &gbuf[i],
				   &x0, &y0, &length, &string);
	pdf_text(page, x0, y0, string, Fontname, Fontsize,
		just, orientation);
	free(string);
	break;
      case G_IMAGE:
	{
	  GBUF_IMAGE *img;
	  pdf_check_path(page);
	  advance_bytes = gget_gline((GLine *) &gbuf[i],
				     &x0, &y0, &x1, &y1);
	  if ((img = gbFindImage(x1)))
	    pdf_image(pdf, page, x0, y0, img);
	}
	break;
      case G_POSTSCRIPT:
	pdf_check_path(page);
	advance_bytes = gget_gtext((GText *) &gbuf[i],
				   &x0, &y0, &length, &string);
	pdf_postscript(page, x0, y0, string, just, orientation);
	free(string);
	break;
      case G_FONT:
	pdf_check_path(page);
	advance_bytes = gget_gtext((GText *) &gbuf[i],
				   &Fontsize, &y0, &length, &string);
	strcpy(Fontname, string);
	pdf_font(pdf, page, Fontname, Fontsize);
	break;
      case G_ORIENTATION:
	advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &orientation);
	break;
      case G_JUSTIFICATION:
	advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &just);
	break;
      case G_GROUP:
	pdf_check_path(page);
	advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &group);
	if (group) pdf_group(page);
	else pdf_ungroup(page);
	break;
      case G_SAVE:
	pdf_check_path(page);
	advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &save);
	if (save == 1) pdf_gsave(page);
	else if (save == -1) pdf_grestore(page);
	break;
      case G_LSTYLE:
	pdf_check_path(page);
	advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &lstyle);
	if (!PDF_Moveto) pdf_setdash(page, lstyle);
	break;
      case G_LWIDTH:
	pdf_check_path(page);
	advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &lwidth);
	if (!PDF_Moveto) pdf_setwidth(page, lwidth);
	break;
      case G_COLOR:
	pdf_check_path(page);
	advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &color);
	pdf_setcolor(page, color);
	break;
      case G_TIMESTAMP:
	advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &TimeStamped);
	break;
      default:
	fprintf(stderr,"unknown event type %d\n", c);
	break;
      }
      switch(c) {
      case G_MOVETO:
      case G_ORIENTATION:
      case G_JUSTIFICATION:
      case G_COLOR:
      case G_LSTYLE:
      case G_LWIDTH:
      case G_GROUP:
	break;
      default:
	PDF_Moveto = 0;
	break;
      }
    }
    pdf_check_path(page);
  }

  if (clipping) pdf_grestore(page);
  
  pdf_grestore(page);		/* from save in G_HEADER */

  /* save the document to a file */
  HPDF_SaveToFile (pdf, filename);
  
  /* clean up */
  HPDF_Free (pdf);
  
  return 1;
}

int gbuf_dump_ascii(unsigned char *gbuf, int bufsize, FILE *fp)
{
  int c;
  int i, advance_bytes = 0, *tptr;
   
  for (i = 0; i < bufsize; i+=advance_bytes) {
    c = gbuf[i++];
    if (TimeStamped) {
      tptr = (int *) &gbuf[i];
      i+=sizeof(int);
      CurrentTimeStamp = *tptr;
      fprintf(fp, "[%d]\t",CurrentTimeStamp);
    }
    switch (c) {
    case G_HEADER:
      advance_bytes = gread_gheader((GHeader *) &gbuf[i], fp);
      break;
    case G_CLIP:
    case G_LINE:
    case G_FILLEDRECT:
    case G_CIRCLE:
    case G_IMAGE:
      advance_bytes = gread_gline(c, (GLine *) &gbuf[i], fp);
      break;
    case G_LINETO:
    case G_MOVETO:
    case G_POINT:
      advance_bytes = gread_gpoint(c, (GPoint *) &gbuf[i], fp);
      break;
    case G_POLY:
    case G_FILLEDPOLY:
      advance_bytes = gread_gpoly(c, (GPointList *) &gbuf[i], fp);
      break;
    case G_POSTSCRIPT:
    case G_FONT:
    case G_TEXT:
      advance_bytes = gread_gtext(c, (GText *) &gbuf[i], fp);
      break;
    case G_GROUP:
    case G_SAVE:
    case G_ORIENTATION:
    case G_JUSTIFICATION:
    case G_LSTYLE:
    case G_LWIDTH:
    case G_COLOR:
      advance_bytes = gread_gattr(c, (GAttr *) &gbuf[i], fp);
      break;
    case G_TIMESTAMP:
      advance_bytes = gread_gattr(c, (GAttr *) &gbuf[i], fp);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
  }
  return(0);
}


int
gfile_to_ascii(FILE *InFP, FILE *OutFP)
{
  int c;
   
  while((c = getc(InFP)) != EOF) {
    if (TimeStamped) {
      get_timestamp(InFP, &CurrentTimeStamp);
      fprintf(OutFP, "[%d]\t",CurrentTimeStamp);
    }
    switch (c) {
    case G_HEADER:
      read_gheader(InFP, OutFP);
      break;
    case G_FILLEDRECT:
    case G_LINE:
    case G_CLIP:
    case G_CIRCLE:
    case G_IMAGE:
      read_gline(c, InFP, OutFP);
      break;
    case G_MOVETO:
    case G_LINETO:
    case G_POINT:
      read_gpoint(c, InFP, OutFP);
      break;
    case G_POLY:
    case G_FILLEDPOLY:
      read_gpoly(c, InFP, OutFP);
      break;
    case G_POSTSCRIPT:
    case G_FONT:
    case G_TEXT:
      read_gtext(c, InFP, OutFP);
      break;
    case G_GROUP:
    case G_SAVE:
    case G_JUSTIFICATION:
    case G_ORIENTATION:
    case G_LSTYLE:
    case G_LWIDTH:
    case G_COLOR:
      read_gattr(c, InFP, OutFP);
      break;
    case G_TIMESTAMP:
      read_gattr(c, InFP, OutFP);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
  }
  return(0);
}

/*
 * gbuf_dump_ps()
 *
 * Convert a gbuffer to a postscript file.  Most events are self
 * explanatory.  It should be noted, however, that PostScript does not
 * automatically "stroke" lines that are connected so that an explicit
 * "stroke" must be added.  Currently, a stroke is added before a moveto
 * which follows one or more lineto's.  If filling is specifically
 * requested, then the fill operator is used instead of the stroke command.
 */

int gbuf_dump_ps(unsigned char *gbuf, int bufsize, int type, FILE *OutFP)
{
  int c, n;
  int orientation = 0, lstyle = 0, color = 1, just = 0, lwidth = 1;
  float w, h;
  float x0, y0, x1, y1, *points;
  int length, save, group;
  float version;
  char *string;
  int i, advance_bytes = 0;
  
  for (i = 0; i < bufsize; i+=advance_bytes) {
    c = gbuf[i++];
    if (TimeStamped) {
      CurrentTimeStamp = (int) gbuf[i];
      i+=sizeof(int);
    }
    if (PS_Moveto == 1) {		/* This is to prevent mulitple */
      switch(c) {			/* moveto's in a row (not ok   */
      case G_LINETO:		        /* in AI88                     */
      case G_ORIENTATION:
      case G_JUSTIFICATION:
      case G_COLOR:
      case G_LSTYLE:
      case G_LWIDTH:
	break;
      default:
	ps_newpath(type, OutFP);
	break;
      }
    }
    switch (c) {
    case G_HEADER:
      advance_bytes = gget_gheader((GHeader *) &gbuf[i],
				   &version, &w, &h);
      ps_init(type, w, h, OutFP);
      ps_gsave(type, OutFP);
      break;
    case G_CLIP:
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      ps_check_path(type, OutFP);
      ps_grestore(type, OutFP);
      ps_gsave(type, OutFP);

      /* Set things that may have just gotten popped off */
      ps_font(type, Fontname, Fontsize, OutFP);
      ps_setdash(type, lstyle, OutFP);
      ps_setwidth(type, lwidth, OutFP);
      ps_setcolor(type, color, OutFP);

      ps_clip(type, x0, y0, x1, y1, OutFP);
      break;
    case G_FILLEDPOLY:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gpoly((GPointList *) &gbuf[i], &n, &points);
      ps_filled_poly(type, n, points, OutFP);
      free(points);
      break;
    case G_POLY:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gpoly((GPointList *) &gbuf[i], &n, &points);
      ps_poly(type, n, points, OutFP);
      free(points);
      break;
    case G_FILLEDRECT:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      ps_filled_rect(type, x0, y0, x1, y1, OutFP);
      break;
    case G_LINE:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      ps_line(type, x0, y0, x1, y1, OutFP);
      break;
    case G_CIRCLE:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      ps_circle(type, x0, y0, x1, y1, OutFP);
      break;
    case G_LINETO:
      advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
      ps_lineto(type, x0, y0, OutFP);
      PS_Stroking = 1;
      break;
    case G_MOVETO:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
      ps_moveto(type, x0, y0, OutFP);
      PS_Moveto = 1;
      break;
    case G_POINT:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
      ps_point(type, x0, y0, OutFP);
      break;
    case G_TEXT:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gtext((GText *) &gbuf[i],
				 &x0, &y0, &length, &string);
      ps_text(type, x0, y0, string, Fontname, Fontsize,
	      just, orientation, OutFP);
      free(string);
      break;
    case G_IMAGE:
      {
	GBUF_IMAGE *img;
	ps_check_path(type, OutFP);
	advance_bytes = gget_gline((GLine *) &gbuf[i],
				   &x0, &y0, &x1, &y1);
	if ((img = gbFindImage(x1))) ps_image(type, x0, y0, img, OutFP);
      }
      break;
    case G_POSTSCRIPT:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gtext((GText *) &gbuf[i],
				 &x0, &y0, &length, &string);
      ps_postscript(type, x0, y0, string, just, orientation, OutFP);
      free(string);
      break;
    case G_FONT:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gtext((GText *) &gbuf[i],
				 &Fontsize, &y0, &length, &string);
      strcpy(Fontname, string);
      ps_font(type, Fontname, Fontsize, OutFP);
      break;
    case G_ORIENTATION:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &orientation);
      break;
    case G_JUSTIFICATION:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &just);
      break;
    case G_GROUP:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &group);
      if (group) ps_group(type, OutFP);
      else ps_ungroup(type, OutFP);
      break;
    case G_SAVE:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &save);
      if (save == 1) ps_gsave(type, OutFP);
      else if (save == -1) ps_grestore(type, OutFP);
      break;
    case G_LSTYLE:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &lstyle);
      ps_setdash(type, lstyle, OutFP);
      break;
    case G_LWIDTH:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &lwidth);
      ps_setwidth(type, lwidth, OutFP);
      break;
    case G_COLOR:
      ps_check_path(type, OutFP);
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &color);
      ps_setcolor(type, color, OutFP);
      break;
    case G_TIMESTAMP:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &TimeStamped);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
    switch(c) {
    case G_MOVETO:
    case G_ORIENTATION:
    case G_JUSTIFICATION:
    case G_COLOR:
    case G_LSTYLE:
    case G_LWIDTH:
    case G_GROUP:
      break;
    default:
      PS_Moveto = 0;
      break;
    }
  }
  ps_check_path(type, OutFP);
  return(0);
}


int
gfile_to_ps(FILE *InFP, int type, FILE *OutFP)
{
  int c, n;
  int orientation = 0, lstyle = 0, color = 1, just = 0, save, lwidth = 1;
  float w, h;
  float x0, y0, x1, y1, *points;
  int length, group;
  float version;
  char *string;

  while((c = getc(InFP)) != EOF) {
    if (TimeStamped) get_timestamp(InFP, &CurrentTimeStamp);
    if (PS_Moveto == 1) {
      switch(c) {
      case G_LINETO:
      case G_ORIENTATION:
      case G_JUSTIFICATION:
      case G_COLOR:
      case G_LSTYLE:
      case G_LWIDTH:
	break;
      default:
	ps_newpath(type, OutFP);
	break;
      }
    }
    switch (c) {
    case G_HEADER:
      get_gheader(InFP, &version, &w, &h);
      ps_init(type, w, h, OutFP);
      ps_gsave(type, OutFP);
      break;
    case G_CLIP:
      ps_check_path(type, OutFP);
      ps_grestore(type, OutFP);
      ps_gsave(type, OutFP);
      get_gline(InFP, &x0, &y0, &x1, &y1);

      /* Set things that may have just gotten popped off */
      ps_font(type, Fontname, Fontsize, OutFP);
      ps_setdash(type, lstyle, OutFP);
      ps_setwidth(type, lwidth, OutFP);
      ps_setcolor(type, color, OutFP);

      ps_clip(type, x0, y0, x1, y1, OutFP);
      break;
    case G_LINE:
      ps_check_path(type, OutFP);
      get_gline(InFP, &x0, &y0, &x1, &y1);
      ps_line(type, x0, y0, x1, y1, OutFP);
      break;
    case G_CIRCLE:
      ps_check_path(type, OutFP);
      get_gline(InFP, &x0, &y0, &x1, &y1);
      ps_circle(type, x0, y0, x1, y1, OutFP);
      break;
    case G_FILLEDRECT:
      ps_check_path(type, OutFP);
      get_gline(InFP, &x0, &y0, &x1, &y1);
      ps_filled_rect(type, x0, y0, x1, y1, OutFP);
      break;
    case G_FILLEDPOLY:
      ps_check_path(type, OutFP);
      get_gpoly(InFP, &n, &points);
      ps_filled_poly(type, n, points, OutFP);
      free(points);
      break;
    case G_POLY:
      ps_check_path(type, OutFP);
      get_gpoly(InFP, &n, &points);
      ps_poly(type, n, points, OutFP);
      free(points);
      break;
    case G_POINT:
      ps_check_path(type, OutFP);
      get_gpoint(InFP, &x0, &y0);
      ps_point(type, x0, y0, OutFP);
      break;
    case G_MOVETO:
      ps_check_path(type, OutFP);
      get_gpoint(InFP, &x0, &y0);
      ps_moveto(type, x0, y0, OutFP);
      PS_Moveto = 1;
      break;
    case G_LINETO:
      get_gpoint(InFP, &x0, &y0);
      ps_lineto(type, x0, y0, OutFP);
      PS_Stroking = 1;
      break;
    case G_TEXT:
      ps_check_path(type, OutFP);
      get_gtext(InFP, &x0, &y0, &length, &string);
      ps_text(type, x0, y0, string, Fontname, Fontsize,
	      just, orientation, OutFP);
      free(string);
      break;
    case G_POSTSCRIPT:
      ps_check_path(type, OutFP);
      get_gtext(InFP, &x0, &y0, &length, &string);
      ps_postscript(type, x0, y0, string, just, orientation, OutFP);
      free(string);
      break;
    case G_IMAGE:
      {
	GBUF_IMAGE *img;
	ps_check_path(type, OutFP);
	get_gline(InFP, &x0, &y0, &x1, &y1);
	if ((img = gbFindImage(x1))) ps_image(type, x0, y0, img, OutFP);
      }
      break;
    case G_FONT:
      ps_check_path(type, OutFP);
      get_gtext(InFP, &Fontsize, &y0, &length, &string);
      strcpy(Fontname, string);
      ps_font(type, Fontname, Fontsize, OutFP);
      break;
    case G_ORIENTATION:
      get_gattr(c, InFP, &orientation);
      break;
    case G_JUSTIFICATION:
      get_gattr(c, InFP, &just);
      break;
    case G_LSTYLE:
      ps_check_path(type, OutFP);
      get_gattr(c, InFP, &lstyle);
      ps_setdash(type, lstyle, OutFP);
      break;
    case G_LWIDTH:
      ps_check_path(type, OutFP);
      get_gattr(c, InFP, &lwidth);
      ps_setwidth(type, lwidth, OutFP);
      break;
    case G_GROUP:
      ps_check_path(type, OutFP);
      get_gattr(c, InFP, &group);
      if (group) ps_group(type, OutFP);
      else ps_ungroup(type, OutFP);
      break;
    case G_SAVE:
      ps_check_path(type, OutFP);
      get_gattr(c, InFP, &save);
      if (save == 1) ps_gsave(type, OutFP);
      else if (save == -1) ps_grestore(type, OutFP);
      break;
    case G_COLOR:
      ps_check_path(type, OutFP);
      get_gattr(c, InFP, &color);
      ps_setcolor(type, color, OutFP);
      break;
    case G_TIMESTAMP:
      get_gattr(c, InFP, &TimeStamped);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
    switch(c) {
    case G_MOVETO:
    case G_ORIENTATION:
    case G_JUSTIFICATION:
    case G_COLOR:
    case G_LSTYLE:
    case G_LWIDTH:
    case G_GROUP:
      break;
    default:
      PS_Moveto = 0;
      break;
    }
  }
   
  ps_check_path(type, OutFP);
   
  switch(type) {
  case GBUF_AI88:
    add_ai88_trailer(OutFP);
    break;
  case GBUF_AI3:
    add_ai3_trailer(OutFP);
    break;
  case GBUF_PS:
  case GBUF_EPS:
    add_ps_trailer(OutFP);
    break;
  default:
    break;
  }
  return(0);
}


/*
 * gbuf_dump_fig()
 *
 * Convert a gbuffer to an (x)fig file for editing.
 */

int gbuf_dump_fig(unsigned char *gbuf, int bufsize, int type, FILE *OutFP)
{
  int c, n;
  int orientation = 0, lstyle = 0, color = 1, just = 0, lwidth = 1;
  float w, h;
  float x0, y0, x1, y1, *points;
  int length, save, group;
  float version;
  char *string;
  int i, advance_bytes = 0;
  int Fig_Filling = 0, Fig_Stroking = 0, Fig_Moveto = 0;
   
  for (i = 0; i < bufsize; i+=advance_bytes) {
    c = gbuf[i++];
    if (TimeStamped) {
      CurrentTimeStamp = (int) gbuf[i];
      i+=sizeof(int);
    }
    switch (c) {
    case G_HEADER:
      advance_bytes = gget_gheader((GHeader *) &gbuf[i],
				   &version, &w, &h);
      fig_init(type, w, h, OutFP);
      break;
    case G_CLIP:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      break;
    case G_FILLEDRECT:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      fig_filled_rect(type, x0, y0, x1, y1, color, OutFP);
      break;
    case G_FILLEDPOLY:
    case G_POLY:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gpoly((GPointList *) &gbuf[i], &n, &points);
      /* do something here if you want to see it... */
      free(points);
      break;
    case G_LINE:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      fig_line(type, x0, y0, x1, y1, lstyle, color, OutFP);
      break;

      /*  NEEDS TO BE FIXED TO TAKE RADIUS INTO ACCOUNT */
    case G_CIRCLE:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      fig_point(type, x0, y0, color, OutFP);
      break;
    case G_LINETO:
      if (!Fig_Stroking) {
	fig_startline(type, x0, y0, lstyle, color, OutFP);
	Fig_Stroking = 1;
      }
      advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
      fig_lineto(type, x0, y0, OutFP);
      break;
    case G_MOVETO:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
      Fig_Moveto = 1;
      break;
    case G_POINT:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
      fig_point(type, x0, y0, color, OutFP);
      break;
    case G_TEXT:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gtext((GText *) &gbuf[i],
				 &x0, &y0, &length, &string);
      fig_text(type, x0, y0, string, Fontname, Fontsize,
	       just, orientation, OutFP);
      break;
    case G_POSTSCRIPT:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gtext((GText *) &gbuf[i],
				 &x0, &y0, &length, &string);
      /* DO NOTHING */
      break;
    case G_IMAGE:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      /* DO NOTHING */
      break;
    case G_FONT:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gtext((GText *) &gbuf[i],
				 &Fontsize, &y0, &length, &string);
      strcpy(Fontname, string);
      break;
    case G_ORIENTATION:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &orientation);
      break;
    case G_GROUP:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &group);
      break;
    case G_JUSTIFICATION:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &just);
      break;
    case G_SAVE:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &save);
      break;
    case G_LSTYLE:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &lstyle);
      break;
    case G_LWIDTH:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &lwidth);
      break;
    case G_COLOR:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &color);
      break;
    case G_TIMESTAMP:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &TimeStamped);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
    switch(c) {
    case G_MOVETO:
    case G_ORIENTATION:
    case G_JUSTIFICATION:
    case G_GROUP:
      break;
    default:
      Fig_Moveto = 0;
      break;
    }
  }
  fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
  return(0);
}



int gfile_to_fig(FILE *InFP, int type, FILE *OutFP)
{
  int c, n;
  int orientation = 0, lstyle = 0, color = 1, just = 0, save, lwidth;
  float w, h;
  float x0, y0, x1, y1, *points;
  int length, group;
  float version;
  char *string;
  int Fig_Filling = 0, Fig_Stroking = 0, Fig_Moveto = 0;
  
  while((c = getc(InFP)) != EOF) {
    if (TimeStamped) get_timestamp(InFP, &CurrentTimeStamp);
    switch (c) {
    case G_HEADER:
      get_gheader(InFP, &version, &w, &h);
      fig_init(type, w, h, OutFP);
      break;
    case G_LINE:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gline(InFP, &x0, &y0, &x1, &y1);
      fig_line(type, x0, y0, x1, y1, lstyle, color, OutFP);
      break;
    case G_CIRCLE:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gline(InFP, &x0, &y0, &x1, &y1);
      fig_point(type, x0, y0, color, OutFP);
      break;
    case G_FILLEDRECT:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gline(InFP, &x0, &y0, &x1, &y1);
      fig_filled_rect(type, x0, y0, x1, y1, color, OutFP);
      break;
    case G_FILLEDPOLY:
    case G_POLY:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gpoly(InFP, &n, &points);
      /* do something here if you want to see it... */
      free(points);
      break;
    case G_CLIP:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gline(InFP, &x0, &y0, &x1, &y1);
      break;
    case G_POINT:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gpoint(InFP, &x0, &y0);
      fig_point(type, x0, y0, color, OutFP);
      break;
    case G_MOVETO:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gpoint(InFP, &x0, &y0);
      Fig_Moveto = 1;
      break;
    case G_LINETO:
      if (!Fig_Stroking) {
	fig_startline(type, x0, y0, lstyle, color, OutFP);
	Fig_Stroking = 1;
      }
      get_gpoint(InFP, &x0, &y0);
      fig_lineto(type, x0, y0, OutFP);
      break;
    case G_TEXT:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gtext(InFP, &x0, &y0, &length, &string);
      fig_text(type, x0, y0, string, Fontname, Fontsize,
	       just, orientation, OutFP);
      break;
    case G_POSTSCRIPT:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gtext(InFP, &x0, &y0, &length, &string);
      /* DO NOTHING */
      break;
    case G_IMAGE:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gline(InFP, &x0, &y0, &x1, &y1);
      /* DO NOTHING */
      break;
    case G_FONT:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gtext(InFP, &Fontsize, &y0, &length, &string);
      strcpy(Fontname, string);
      break;
    case G_ORIENTATION:
      get_gattr(c, InFP, &orientation);
      break;
    case G_JUSTIFICATION:
      get_gattr(c, InFP, &just);
      break;
    case G_GROUP:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gattr(c, InFP, &group);
      break;
    case G_LSTYLE:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gattr(c, InFP, &lstyle);
      ps_setdash(type, lstyle, OutFP);
      break;
    case G_LWIDTH:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gattr(c, InFP, &lwidth);
      ps_setwidth(type, lwidth, OutFP);
      break;
    case G_SAVE:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gattr(c, InFP, &save);
      break;
    case G_COLOR:
      fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
      get_gattr(c, InFP, &color);
      break;
    case G_TIMESTAMP:
      get_gattr(c, InFP, &TimeStamped);
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
    switch(c) {
    case G_MOVETO:
    case G_GROUP:
    case G_ORIENTATION:
    case G_JUSTIFICATION:
      break;
    default:
      Fig_Moveto = 0;
      break;
    }
  }
  fig_check_path(type, &Fig_Filling, &Fig_Stroking, OutFP);
  return(0);
}


void playback_gfile(FILE *InFP)
{
  int c, n;
  int orientation = 0, lstyle = 0, color = 1, just = 0, save, lwidth = 1;
  float w, h;
  float x0, y0, x1, y1, *points;
  int length, group;
  float fontsize;
  float version;
  char *string;
  int start_time = 0, current_time, elapsed_time; /* in msec */
  
  while((c = getc(InFP)) != EOF) {
    if (TimeStamped) {
      get_timestamp(InFP, &CurrentTimeStamp);
      current_time = trGetTime();
      elapsed_time = current_time-start_time;
      if (elapsed_time < CurrentTimeStamp)
	trSleep(CurrentTimeStamp - elapsed_time);
    }
    switch (c) {
    case G_HEADER:
      get_gheader(InFP, &version, &w, &h);
      setwindow(0, 0, w-1.0f,  h-1.0f);
      break;
    case G_LINE:
      get_gline(InFP, &x0, &y0, &x1, &y1);
      moveto(x0, y0);
      lineto(x1,y1);
      break;
    case G_CIRCLE:
      get_gline(InFP, &x0, &y0, &x1, &y1);
      if (y1 == 0.0) circle(x0, y0, x1);
      else fcircle(x0, y0, x1);
      break;
    case G_FILLEDRECT:
      get_gline(InFP, &x0, &y0, &x1, &y1);
      filledrect(x0, y0, x1, y1);
      break;
    case G_POLY:
      get_gpoly(InFP, &n, &points);
      polyline(n/2, points);
      free(points);
      break;
    case G_FILLEDPOLY:
      get_gpoly(InFP, &n, &points);
      filledpoly(n/2, points);
      free(points);
      break;
    case G_CLIP:
      get_gline(InFP, &x0, &y0, &x1, &y1);
      setclipregion(x0, y0, x1, y1);
      break;
    case G_POINT:
      get_gpoint(InFP, &x0, &y0);
      dotat(x0,y0);
      break;
    case G_MOVETO:
      get_gpoint(InFP, &x0, &y0);
      moveto(x0, y0);
      break;
    case G_LINETO:
      get_gpoint(InFP, &x0, &y0);
      lineto(x0, y0);
      break;
    case G_TEXT:
      get_gtext(InFP, &x0, &y0, &length, &string);
      moveto(x0, y0);
      drawtext(string);
      free(string);
      break;
    case G_POSTSCRIPT:
      get_gtext(InFP, &x0, &y0, &length, &string);
      postscript(string, x0, y0);
      free(string);
      break;
    case G_IMAGE:
      {
	GBUF_IMAGE *img;
	get_gline(InFP, &x0, &y0, &x1, &y1);
	if ((img = gbFindImage(x1))) 
	  place_image(img->w, img->h, img->d, img->data, x0, y0);
      }
      break;
    case G_FONT:
      get_gtext(InFP, &fontsize, &y0, &length, &string);
      if (fontsize > 1.0) setfont(string, fontsize);
      free(string);
      break;
    case G_ORIENTATION:
      get_gattr(c, InFP, &orientation);
      setorientation(orientation);
      break;
    case G_JUSTIFICATION:
      get_gattr(c, InFP, &just);
      setjust(just);
      break;
    case G_LSTYLE:
      get_gattr(c, InFP, &lstyle);
      setlstyle(lstyle);
      break;
    case G_LWIDTH:
      get_gattr(c, InFP, &lwidth);
      setlwidth(lwidth);
      break;
    case G_SAVE:
      get_gattr(c, InFP, &save);
      if (save == 1) gsave();
      else if (save == -1) grestore();
      break;
    case G_COLOR:
      get_gattr(c, InFP, &color);
      setcolor(color);
      break;
    case G_GROUP:
      get_gattr(c, InFP, &group);
      break;
    case G_TIMESTAMP:
      get_gattr(c, InFP, &TimeStamped);
      if (TimeStamped) start_time = trGetTime();
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
  }
}

void playback_gbuf(unsigned char *gbuf, int bufsize)
{
  int c, n;
  float w, h;
  float x0, y0, x1, y1, *points;
  int length, save;
  float version;
  char *string;
  int i, advance_bytes = 0;
  int start_time = 0, current_time, elapsed_time; /* in msec */
  int orientation = 0, lstyle = 0, color = 1, just = 0, group, lwidth = 1;
  float fontsize = 12.0;
   
  for (i = 0; i < bufsize; i+=advance_bytes) {
    c = gbuf[i++];
    if (TimeStamped) {
      memcpy((char *) &CurrentTimeStamp, (char *) &gbuf[i], sizeof(int));
      i+=sizeof(int);
      current_time = trGetTime();
      elapsed_time = current_time-start_time;
      if (elapsed_time < CurrentTimeStamp)
	trSleep(CurrentTimeStamp - elapsed_time);
    }
    switch (c) {
    case G_HEADER:
      advance_bytes = gget_gheader((GHeader *) &gbuf[i],
				   &version, &w, &h);
      setwindow(0, 0, w-1.0f, h-1.0f);
      break;
    case G_LINE:
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      moveto(x0, y0);
      lineto(x1,y1);
      break;
    case G_CIRCLE:
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      if (y1 == 0.0) circle(x0, y0, x1);
      else fcircle(x0, y0, x1);
      break;
    case G_FILLEDRECT:
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      filledrect(x0, y0, x1, y1);
      break;
    case G_POLY:
      advance_bytes = gget_gpoly((GPointList *) &gbuf[i], &n, &points);
      polyline(n/2, points);
      free(points);
      break;
    case G_FILLEDPOLY:
      advance_bytes = gget_gpoly((GPointList *) &gbuf[i], &n, &points);
      filledpoly(n/2, points);
      free(points);
      break;
    case G_CLIP:
      advance_bytes = gget_gline((GLine *) &gbuf[i],
				 &x0, &y0, &x1, &y1);
      setclipregion(x0, y0, x1, y1);
      break;
    case G_POINT:
      advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
      dotat(x0,y0);
      break;
    case G_MOVETO:
      advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
      moveto(x0, y0);
      break;
    case G_LINETO:
      advance_bytes = gget_gpoint((GPoint *) &gbuf[i], &x0, &y0);
      lineto(x0, y0);
      break;
    case G_TEXT:
      advance_bytes = gget_gtext((GText *) &gbuf[i],
				 &x0, &y0, &length, &string);
      moveto(x0, y0);
      drawtext(string);
      break;
    case G_POSTSCRIPT:
      advance_bytes = gget_gtext((GText *) &gbuf[i],
				 &x0, &y0, &length, &string);
      postscript(string, x0, y0);
      break;
    case G_IMAGE:
      {
	GBUF_IMAGE *img;
	advance_bytes = gget_gline((GLine *) &gbuf[i],
				   &x0, &y0, &x1, &y1);
	if ((img = gbFindImage(x1))) 
	  place_image(img->w, img->h, img->d, img->data, x0, y0);
      }
      break;
    case G_FONT:
      advance_bytes = gget_gtext((GText *) &gbuf[i],
				 &fontsize, &y0, &length, &string);
      if (fontsize > 1.0) setfont(string, fontsize);
      break;
    case G_ORIENTATION:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &orientation);
      setorientation(orientation);
      break;
    case G_JUSTIFICATION:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &just);
      setjust(just);
      break;
    case G_LSTYLE:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &lstyle);
      setlstyle(lstyle);
      break;
    case G_LWIDTH:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &lwidth);
      setlwidth(lwidth);
      break;
    case G_SAVE:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &save);
      if (save == 1) gsave();
      else if (save == -1) grestore();
      break;
    case G_COLOR:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &color);
      setcolor(color);
      break;
    case G_GROUP:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &group);
      break;
    case G_TIMESTAMP:
      advance_bytes = gget_gattr(c, (GAttr *) &gbuf[i], &TimeStamped);
      if (TimeStamped) start_time = trGetTime();
      break;
    default:
      fprintf(stderr,"unknown event type %d\n", c);
      break;
    }
  }
}


int gbClearAndPlayback(void)
{
  clearscreen();
  gbPlaybackGevents();
  return(0);
}  
  

void gbSetPageOrientation(char ori)
{
  if(ori != PS_LANDSCAPE && ori != PS_PORTRAIT) {
    PS_Orientation = PS_AUTOMATIC;
    return;
  }
  PS_Orientation = ori;
}

void gbSetPageFill(int status)
{
  PS_FillPage = (status != 0);
}


/*************************************************************************/
/*                             PDF Functions                             */
/*************************************************************************/

static void pdf_check_scope(HPDF_Page page);

static void pdf_init(HPDF_Page page, float w, float h)
{
  //  haru_log("init");
  HPDF_Page_SetWidth (page, w);
  HPDF_Page_SetHeight (page, h);
}

static void pdf_gsave(HPDF_Page page)
{
  //  haru_log("save");
  pdf_check_scope(page);
  HPDF_Page_GSave(page);
}

static void pdf_grestore(HPDF_Page page)
{
  //  haru_log("restore");
  pdf_check_scope(page);
  HPDF_Page_GRestore(page);
}

static void pdf_check_scope(HPDF_Page page)
{
  if (PDF_Moveto) {
    //    haru_log("scope-stroke");
    HPDF_Page_Stroke(page);		/* leave path if necessary */
    PDF_Moveto = 0;
  }
}

/*
 * void pdf_check_path
 *   see if the current path needs to be stroked or filled
 */

static void pdf_check_path(HPDF_Page page)
{
  if (PDF_Filling) {
    pdf_fill(page);
    PDF_Filling = PDF_Stroking = 0;
  }	       
  else if (PDF_Stroking) {
    pdf_stroke(page);
    PDF_Stroking = 0;
  }
}

static void pdf_clip(HPDF_Page page,
		     float x1, float y1, float x2, float y2)
{
  HPDF_Page_Rectangle(page, x1, y1,  x2-x1, y2-y1);
  HPDF_Page_Clip(page);

  // must end the path before moving on to drawing inside the clip region
  HPDF_Page_EndPath(page);
}

static void pdf_postscript(HPDF_Page page,
			   float scalex, float scaley, char *fname, 
			   int just, int orientation)
{
  char inbuff[256];
  char rawtops = 0;		/* is this a raw image to encapsulate? */
  FILE *ifp;
  char *s;
  double bx1 = 0, by1 = 0, bx2 = 0, by2 = 0;
  char *filename;

  if (!(filename = look_for_file(fname))) {
    fprintf(stderr, "image file %s not found\n", fname);
    return;
  }

  if (strstr(filename, ".raw")) {
    int w, h, d;
    w = h = d = 0;
    if (!raw_getImageDims(filename, &w, &h, &d, NULL)) {
      fprintf(stderr, "Error encapsulating raw image file %s\n", filename);
      return;
    }
    rawtops = 1;
    bx1 = by1 = 0;
    bx2 = w; by2 = h;
  }
  else {
    if (!(ifp = fopen(filename, "rb"))) {
      fprintf(stderr, "Unable to open input file %s\n", filename);
      return;
    }
    
    while (!feof(ifp)) {
      if (fgets(inbuff, 250, ifp) != NULL) {
	if (strncmp(inbuff,"%%BoundingBox:", 14) == 0) {
	  s = strtok(inbuff, " :\t");
	  bx1 = atof(strtok(0, " :\t"));
	  by1 = atof(strtok(0, " :\t"));
	  bx2 = atof(strtok(0, " :\t"));
	  by2 = atof(strtok(0, " :\t"));
	  break;
	}
      }
    }
    if (bx2 == 0 || by2 == 0) { 
      fprintf(stderr, "%s: invalid bounding box\n", filename); 
      fclose(ifp);
      return;
    }
    
    bx2 -= bx1;
    by2 -= by1;
  }
}


static const HPDF_BYTE RAW_IMAGE_DATA[128] = {
    0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xfc,
    0xff, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xff, 0xf0,
    0xf3, 0xf3, 0xff, 0xe0, 0xf3, 0xf3, 0xff, 0xc0,
    0xf3, 0xf3, 0xff, 0x80, 0xf3, 0x33, 0xff, 0x00,
    0xf3, 0x33, 0xfe, 0x00, 0xf3, 0x33, 0xfc, 0x00,
    0xf8, 0x07, 0xf8, 0x00, 0xf8, 0x07, 0xf0, 0x00,
    0xfc, 0xcf, 0xe0, 0x00, 0xfc, 0xcf, 0xc0, 0x00,
    0xff, 0xff, 0x80, 0x00, 0xff, 0xff, 0x00, 0x00,
    0xff, 0xfe, 0x00, 0x00, 0xff, 0xfc, 0x00, 0x00,
    0xff, 0xf8, 0x0f, 0xe0, 0xff, 0xf0, 0x0f, 0xe0,
    0xff, 0xe0, 0x0c, 0x30, 0xff, 0xc0, 0x0c, 0x30,
    0xff, 0x80, 0x0f, 0xe0, 0xff, 0x00, 0x0f, 0xe0,
    0xfe, 0x00, 0x0c, 0x30, 0xfc, 0x00, 0x0c, 0x30,
    0xf8, 0x00, 0x0f, 0xe0, 0xf0, 0x00, 0x0f, 0xe0,
    0xe0, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};



static void pdf_image(HPDF_Doc pdf, HPDF_Page page,
		      float scalex, float scaley, GBUF_IMAGE *img)
{
  int image, mask_image;
  int w, h, d, bpc = 8;
  HPDF_REAL sx, sy;
  double bx1 = 0, by1 = 0, bx2 = 0, by2 = 0;
  char *buf2 = NULL, *p1, *p2;	/* for fixing 4 component images */
  int masksize;
  unsigned char *pix, *maskpix;
  unsigned char *mask_data = NULL, maskbyte;

  pdf_check_scope(page);

  w = img->w;
  h = img->h;
  d = img->d;
  bx1 = by1 = 0;
  bx2 = w; by2 = h;
  sx = scalex/bx2;
  sy = scaley/by2;

  HPDF_Image hpdf_image, hpdf_maskimage;

  //  HPDF_Page_GetCurrentPos() ?
  float x = PDF_curx;
  float y = PDF_cury;

  /*
   * RGBA images need to be converted to RGB plus a mask image
   */
  if (d == 4) {
    HPDF_ColorSpace color_space = HPDF_CS_DEVICE_RGB;
    int i, j, n = w*h, total_n = w*h*4;
    p1 = buf2 = (unsigned char *) malloc(w*h*3);
    p2 = img->data;
    if (!buf2) return;
    for (i = 0; i < n; i++) {
      *p1++ = *p2++;
      *p1++ = *p2++;
      *p1++ = *p2++;
      p2++;			/* skip alpha */
    }
    d = 3;
    hpdf_image = HPDF_LoadRawImageFromMem(pdf, buf2, w, h, color_space, 8);
    free(buf2);

    /*
     * bail on mask if w*d is not a multiple of 8 - could be fixed in future...
     */
    if ((w*h)%8 == 0) {
      p1 = mask_data = (unsigned char *) malloc(w*h/8);
      p2 = img->data;
      for (i = 3; i < total_n; ) {
	for (j = 7; j >= 0; j--) {
	  maskbyte |= ((p2[i] == 0) << j);
	  i+=4;
	}
	*p1++ = maskbyte;
	maskbyte = 0;
      }
      
      /* load GrayScale mask image (1bit) file from memory. */
      hpdf_maskimage = HPDF_LoadRawImageFromMem (pdf, mask_data, w, h,
						 HPDF_CS_DEVICE_GRAY, 1);
      
      free(mask_data);
      
      HPDF_Image_SetMaskImage (hpdf_image, hpdf_maskimage);
    }
  }

  /* just an RGB image */
  else if (d == 3) {
    HPDF_ColorSpace color_space = HPDF_CS_DEVICE_RGB;
    hpdf_image = HPDF_LoadRawImageFromMem(pdf, img->data, w, h, color_space, 8);
  }

  /* just an RGB image */
  else if (d == 1) {
    HPDF_ColorSpace color_space = HPDF_CS_DEVICE_GRAY;
    hpdf_image = HPDF_LoadRawImageFromMem(pdf, img->data, w, h, color_space, 8);
  }
  
  /* Draw image to the canvas. (normal-mode with actual size.)*/
  HPDF_Page_DrawImage (page, hpdf_image, x, y, sx*w, sy*h);

  // This was to see what the mask looks like...
  // HPDF_Page_DrawImage (page, hpdf_maskimage, x, y, sx*w, sy*h);
  
  return;
}

static void pdf_group(HPDF_Page page)
{
}

static void pdf_ungroup(HPDF_Page page)
{
}

static void pdf_point(HPDF_Page page, float x, float y)
{
}

static void pdf_circle(HPDF_Page page,
		       float x, float y,
		       float size, float fill)
{
  //  haru_log("circle");
  if (size == 0.0) return;
  if (size < 0.0) size = -size;
  HPDF_Page_Circle(page, x, y, size/2.0);
  if (fill != 0.0f) HPDF_Page_Fill(page);
  else HPDF_Page_Stroke(page);
}

static void pdf_line(HPDF_Page page,
		     float x1, float y1, float x2, float y2)
{
  //  haru_log("line");
  HPDF_Page_MoveTo(page, x1, y1);
  HPDF_Page_LineTo(page, x1, y1);
  HPDF_Page_Stroke(page);
}

static void pdf_filled_poly (HPDF_Page page, int n, float *verts)
{
  int i;
  //  haru_log("filled_poly");
  HPDF_Page_MoveTo(page, verts[0], verts[1]);
  for (i = 2; i < n; i+=2) {
    HPDF_Page_LineTo(page, verts[i], verts[i+1]);
  }
  HPDF_Page_ClosePath(page);
  HPDF_Page_Fill(page);
}


static void pdf_poly (HPDF_Page page, int n, float *verts)
{
  int i;
  //  haru_log("poly");
  HPDF_Page_MoveTo(page, verts[0],  verts[1]);
  for (i = 2; i < n; i+=2) {
    HPDF_Page_LineTo(page, verts[i], verts[i+1]);
  }
  /* Close the path if the start and stop are the same */
  if ((verts[i-2] == verts[0]) && (verts[i-1] == verts[1]))
     HPDF_Page_ClosePathStroke(page);
}

static void pdf_filled_rect(HPDF_Page page,
			    float x1, float y1, float x2, float y2)
{
  //  haru_log("filled_rect");
  HPDF_Page_Rectangle(page, x1, y1,  x2-x1, y2-y1);
  HPDF_Page_Fill(page);
}


static void pdf_moveto(HPDF_Page page, float x, float y)
{
  //  haru_log("moveto");
  PDF_curx = x;
  PDF_cury = y;
  HPDF_Page_MoveTo(page, x, y);
}

static void pdf_lineto(HPDF_Page page, float x, float y)
{
  //  haru_log("moveto");
  PDF_linetox = x;
  PDF_linetoy = y;
  HPDF_Page_LineTo(page, x, y);
}

static void pdf_stroke(HPDF_Page page)
{
  //  haru_log("stroke");
  if (PDF_linetox == PDF_curx && PDF_linetoy == PDF_cury)
    HPDF_Page_ClosePathStroke(page);
  else
    HPDF_Page_Stroke(page);
}

static void pdf_newpath(HPDF_Page page)
{
}

static void pdf_fill(HPDF_Page page) 
{
  //  haru_log("fill");
  HPDF_Page_Fill(page);
}

static void pdf_text(HPDF_Page page,
		     float x, float y, char *str, char *fontname,
		     float fontsize, int just, int orientation)
{
  float angle = 0.0, radian = 0.0, offsety = 0.0, width, ascent, descent;
  float xpos, ypos;
  HPDF_Font font = HPDF_Page_GetCurrentFont (page);
  float fsize = HPDF_Page_GetCurrentFontSize (page);
  
  switch (orientation) {
  }

  width = HPDF_Page_TextWidth (page, str);

  ascent = HPDF_Font_GetAscent(font);
  descent = HPDF_Font_GetDescent(font);
  offsety = (((ascent-descent)/1000.)*fsize)/3.5;
  {
    char entry[64];
    sprintf(entry, "ascent: %f\t descent: %f\t offset %f\n", ascent, descent, offsety);
    //    haru_log(entry);
  }
  pdf_check_scope(page);
  
  switch (orientation) {
  case 0:
    angle = 0.0;
    switch (just) {
    case LEFT_JUST:   xpos = x; ypos = y-offsety; break;
    case CENTER_JUST: xpos = x-(width/2); ypos = y-offsety; break;
    case RIGHT_JUST:  xpos = x-(width); ypos =  y-offsety; break;
    }
    break;
  case 1:
    angle = 90.0;
    switch (just) {
    case LEFT_JUST:   xpos = x-offsety; ypos = y-width; break;
    case CENTER_JUST: xpos = x-offsety; ypos = y-(width/2); break;
    case RIGHT_JUST:  xpos = x-offsety; ypos = y; break;
    }
    break;
  case 2:
    angle = 180.0;
    switch (just) {
    case LEFT_JUST:   xpos = x-(width); ypos = y-offsety; break;
    case CENTER_JUST: xpos = x-(width/2); ypos = y-offsety; break;
    case RIGHT_JUST:  xpos = x; ypos =  y-offsety; break;
    }    
    break;
  case 3:
    angle = 270.0;
    switch (just) {
    case LEFT_JUST:   xpos = x-offsety; ypos = y; break;
    case CENTER_JUST: xpos = x-offsety; ypos = y-(width/2); break;
    case RIGHT_JUST:  xpos = x-offsety; ypos = y-width; break;
    }
    break;
  }
  radian = (angle/180)*3.1415;
   
  
  HPDF_Page_GSave(page);
  HPDF_Page_BeginText (page);
  HPDF_Page_SetTextMatrix (page,
			   cos(radian), sin(radian),
			   -sin(radian), cos(radian),
			   xpos, ypos);
  HPDF_Page_ShowText (page, str);
  HPDF_Page_EndText (page);
  HPDF_Page_GRestore(page);
}

static void pdf_setdash(HPDF_Page page, int lstyle)
{
  const HPDF_REAL DASH_MODE1[] = {1};
  const HPDF_REAL DASH_MODE2[] = {3, 3};
  const HPDF_REAL DASH_MODE3[] = {1, 4};
  const HPDF_REAL DASH_MODE4[] = {3, 5};

  //  haru_log("setdash");
  
  switch(lstyle) {
  case 0:
    HPDF_Page_SetDash (page, DASH_MODE1, 1, 0);    break;
  case 1:
    HPDF_Page_SetDash (page, NULL, 0, 0);    break;
  case 2:
    HPDF_Page_SetDash (page, DASH_MODE2, 2, 0);    break;
  case 3:
    HPDF_Page_SetDash (page, DASH_MODE3, 2, 0);    break;
  case 4:
  case 5:
  case 6:
  case 7:
    HPDF_Page_SetDash (page, DASH_MODE4, 4, 0);    break;
    break;
  }
}

static void pdf_setwidth(HPDF_Page page, int lwidth)
{
  //  haru_log("setlinewidth");

  if (lwidth < 1) return;
  float width = lwidth/100.;
  HPDF_Page_SetLineWidth(page, width);  
}

static void pdf_setcolor(HPDF_Page page, int color)
{
  pdf_check_scope(page);

  /* 
   * This is a very ugly repair to allow both RGB color specification
   * and color indexing (for historical reasons).  The idea is that the
   * lower 5 bits are for indexing, and bits 6-30 are for specification
   * of an RGB color.  Thus, to get out the unsigned long corresponding
   * to an RGB specified color, the value must be right shifted 5 bits,
   * then separated into the R, G, and B components. (Note that the
   * special case of 0 happened to have already been assigned to black,
   * phew!).
   */

  //  haru_log("setrgbcolor");
  
  if (color < 32) {
    HPDF_Page_SetRGBFill(page,
		    PSColorTableVals[color][0],
		    PSColorTableVals[color][1],
		    PSColorTableVals[color][2]);
    HPDF_Page_SetRGBStroke(page,
		      PSColorTableVals[color][0],
		      PSColorTableVals[color][1],
		      PSColorTableVals[color][2]);
  }
  else {
    unsigned int shifted = color >> 5;
    int r = ((shifted & 0xff0000) >> 16);
    int g = ((shifted & 0xff00) >> 8);
    int b = (shifted & 0xff);
    
    HPDF_Page_SetRGBFill(page, r/256.0, g/256.0, b/256.0);
    HPDF_Page_SetRGBStroke(page, r/256.0, g/256.0, b/256.0);
  }
}

static int pdf_check_font(char *fontname)
{
  if (!strncmp(fontname, "Arial", 5) ||
      !strncmp(fontname, "arial", 5)) return 1;
  if (!strncmp(fontname, "Helvetica", 9) ||
      !strncmp(fontname, "helvetica", 9)) return 1;
  if (!strncmp(fontname, "Times", 5) ||
      !strncmp(fontname, "times", 5)) return 1;
  if (!strncmp(fontname, "Symbol", 6) ||
      !strncmp(fontname, "symbol", 6)) return 1;
  return 0;
}

static void pdf_font(HPDF_Doc pdf, HPDF_Page page, char *fontname, float size)
{
  HPDF_Font font;

  pdf_check_scope(page);

  /* create default-font */
  font = HPDF_GetFont (pdf, fontname, NULL);
  HPDF_Page_SetFontAndSize (page, font, size);
}

/*************************************************************************/
/*                        Postscript Functions                           */
/*************************************************************************/


void ps_init(int type, float w, float h, FILE *fp)
{
  if (h > w) BoundingBox = "0 0 612 792";
  else BoundingBox = "0 0 792 612";
  
  switch (type) {
  case GBUF_AI88:
    add_ai88_prologue(fp);
    break;
  case GBUF_AI3:
    add_ai3_prologue(fp);
    break;
  case GBUF_PS:
    add_ps_prologue(w, h, fp);
    break;
  case GBUF_EPS:
    add_eps_prologue(w, h, fp);
    break;
  case GBUF_PDF:
    add_ps_prologue(w, h, fp);
    break;
  }
}

void ps_gsave(int type, FILE *fp)
{
   switch (type) {
      case GBUF_AI88:
      case GBUF_AI3:
	 break;
      case GBUF_PS:
      case GBUF_EPS:
	 fprintf(fp, "gsave\n");
	 break;
   }
}

void ps_grestore(int type, FILE *fp)
{
   switch (type) {
      case GBUF_AI88:
      case GBUF_AI3:
	 break;
      case GBUF_PS:
      case GBUF_EPS:
	 fprintf(fp, "grestore\n");
	 break;
   }
}

/*
 * void ps_check_path - see if the current path needs to be stroked or filled
 */

void ps_check_path(int type, FILE *fp)
{
  if (PS_Filling) {
    ps_fill(type, fp);
    PS_Filling = PS_Stroking = 0;
  }	       
  else if (PS_Stroking) {
    ps_stroke(type, fp);
    PS_Stroking = 0;
  }
}

void ps_clip(int type, float x1, float y1, float x2, float y2, FILE *fp)
{
   switch (type) {
      case GBUF_AI88:
      case GBUF_AI3:
      case GBUF_EPS:
	 break;
      case GBUF_PS:
	fprintf(fp, "newpath\n%6.2f %6.2f moveto\n", x1,  y1);
	fprintf(fp, "%6.2f %6.2f lineto\n", x1,  y2);
	fprintf(fp, "%6.2f %6.2f lineto\n", x2,  y2);
	fprintf(fp, "%6.2f %6.2f lineto\n", x2,  y1);
	fprintf(fp, "closepath\nclip\nnewpath\n");
	break;
   }
}

static char *look_for_file(char *name)
{
  static char newname[256];
  char *mappath, *m;
  char *p;
  char *equal, *after;
  char *delims = { ";" };
  char from[256], to[256];

#if defined (WIN32) || defined (WIN64)
  if (_access(name, 4) == 0) return name;
#else
  if (access(name, R_OK) == 0) return name;
#endif

  m = getenv("GBUF_MAPPATH");
  if (!m) {
    return NULL;
  }

  mappath = strdup(m);

  p = strtok( mappath, delims );
  while( p != NULL ) {
    if (!(equal = strchr(p, '='))) {
      fprintf(stderr, "Bad format in GBUF_MANPATH env variable (%s)\n", p);
      goto error;
    }
    strncpy(from, p, equal-p);
    from[equal-p] = 0;
    strcpy(to, equal+1);

    if ((after = strstr(name, from))) {
      sprintf(newname, "%s/%s", to, &name[strlen(from)+1]);
#if defined (WIN32) || defined (WIN64)
      if (_access(newname, 04) == 0)
#else
      if (access(newname, R_OK) == 0)
#endif
      {
	free(mappath);
	return newname;
      }
      p = strtok( NULL, delims ); 
    }
  }

error:
  free(mappath);
  return NULL;
}

void ps_postscript(int type, float scalex, float scaley, char *fname, 
		   int just, int orientation, FILE *fp)
{
  char inbuff[256];
  char rawtops = 0;		/* is this a raw image to encapsulate? */
  FILE *ifp;
  char *s;
  double bx1 = 0, by1 = 0, bx2 = 0, by2 = 0;
  char *filename;

  if (!(filename = look_for_file(fname))) {
    fprintf(stderr, "image file %s not found\n", fname);
    return;
  }

  switch (type) {
  case GBUF_AI88:
  case GBUF_AI3:
    break;
  case GBUF_PS:
  case GBUF_EPS:
    
    if (strstr(filename, ".raw")) {
      int w, h, d;
      w = h = d = 0;
      if (!raw_getImageDims(filename, &w, &h, &d, NULL)) {
	fprintf(stderr, "Error encapsulating raw image file %s\n", filename);
	return;
      }
      rawtops = 1;
      bx1 = by1 = 0;
      bx2 = w; by2 = h;
    }
    else {
      if (!(ifp = fopen(filename, "rb"))) {
	fprintf(stderr, "Unable to open input file %s\n", filename);
	return;
      }
      
      while (!feof(ifp)) {
	if (fgets(inbuff, 250, ifp) != NULL) {
	  if (strncmp(inbuff,"%%BoundingBox:", 14) == 0) {
	    s = strtok(inbuff, " :\t");
	    bx1 = atof(strtok(0, " :\t"));
	    by1 = atof(strtok(0, " :\t"));
	    bx2 = atof(strtok(0, " :\t"));
	    by2 = atof(strtok(0, " :\t"));
	    break;
	  }
	}
      }
      if (bx2 == 0 || by2 == 0) { 
	fprintf(stderr, "%s: invalid bounding box\n", filename); 
	fclose(ifp);
	return;
      }
      
      bx2 -= bx1;
      by2 -= by1;
    }

    fputs("/GBUFSTATE save def\n", fp);
    fputs("gsave\n", fp);
    fputs("/a4small {} def /legal {} def\n", fp);
    fputs("/letter {} def /note {} def /copypage {} def\n", fp);
    fputs("/erasepage {} def /showpage {} def\n", fp);
    
    fprintf(fp, "%7.3f %7.3f translate\n", PS_curx, PS_cury);
    fprintf(fp, "%7.3f %7.3f div  %7.3f %7.3f div scale\n", 
	    scalex, (float) bx2, scaley, by2);
    fprintf(fp, "%7.3f %7.3f translate\n", -bx1, -by1);
    
    fputs("0 setgray 0 setlinecap 0 setlinewidth 0 setlinejoin\n", fp);
    fputs("10 setmiterlimit [] 0 setdash\n", fp);
    
    if (!rawtops) {
      rewind(ifp);
      while (!(feof(ifp))) {
	if (fgets(inbuff, 255, ifp) != NULL) {
	  /* Don't copy in EOF's */
	  if (!strncmp(inbuff, "%%EOF", 5)) continue;
	  fputs(inbuff, fp);
	}
      }
      fclose(ifp);
    }
    else raw_toPS(filename, fp, 0);

    fputs("grestore GBUFSTATE restore\n", fp);
    break;
  }
}

void ps_image(int type, float scalex, float scaley, GBUF_IMAGE *img, FILE *fp)
{
  int w, h, d;
  double bx1 = 0, by1 = 0, bx2 = 0, by2 = 0;
  
  switch (type) {
  case GBUF_AI88:
  case GBUF_AI3:
    break;
  case GBUF_PS:
  case GBUF_EPS:
    w = img->w;
    h = img->h;
    d = img->d;
    bx1 = by1 = 0;
    bx2 = w; by2 = h;
    
    fputs("/GBUFSTATE save def\n", fp);
    fputs("gsave\n", fp);
    fputs("/a4small {} def /legal {} def\n", fp);
    fputs("/letter {} def /note {} def /copypage {} def\n", fp);
    fputs("/erasepage {} def /showpage {} def\n", fp);
    
    fprintf(fp, "%7.3f %7.3f translate\n", PS_curx, PS_cury);
    fprintf(fp, "%7.3f %7.3f div  %7.3f %7.3f div scale\n", 
	    scalex, (float) bx2, scaley, by2);
    fprintf(fp, "%7.3f %7.3f translate\n", -bx1, -by1);
    
    fputs("0 setgray 0 setlinecap 0 setlinewidth 0 setlinejoin\n", fp);
    fputs("10 setmiterlimit [] 0 setdash\n", fp);

    raw_bufToPS(img->data, w, h, d, fp, 0);

    fputs("grestore GBUFSTATE restore\n", fp);
    break;
  }
}

void ps_group(int type, FILE *fp)
{
  switch (type) {
  case GBUF_AI88:
    fprintf(fp, "u\n");
    break;
  case GBUF_AI3:
  case GBUF_PS:
  case GBUF_EPS:
    break;
  }
}

void ps_ungroup(int type, FILE *fp)
{
  switch (type) {
  case GBUF_AI88:
    fprintf(fp, "U\n");
    break;
  case GBUF_AI3:
  case GBUF_PS:
  case GBUF_EPS:
    break;
  }
}



void ps_point(int type, float x, float y, FILE *fp)
{
   switch (type) {
      case GBUF_AI88:
      case GBUF_AI3:
	 fprintf(fp, "%6.2f %6.2f m\n", (x-0.2),  (y));
	 fprintf(fp, "%6.2f %6.2f L\n", (x+0.2),  (y));
	 fprintf(fp, "S\n");
	 break;
      case GBUF_PS:
      case GBUF_EPS:
	 fprintf(fp, "newpath %6.2f %6.2f .7 0 360 arc closepath fill\n", x, y);
	 break;
   }
}

void ps_circle(int type, float x, float y, float size, float fill, FILE *fp)
{
   switch (type) {
      case GBUF_AI88:
      case GBUF_AI3:
	 fprintf(fp, "%6.2f %6.2f m\n", (x-0.2),  (y));
	 fprintf(fp, "%6.2f %6.2f L\n", (x+0.2),  (y));
	 fprintf(fp, "S\n");
	 break;
      case GBUF_PS:
      case GBUF_EPS:
	 fprintf(fp, "newpath %6.2f %6.2f %6.2f 0 360 arc closepath ", 
		 x, y, size/2.0);
	 if (fill == 0.0) fprintf(fp, "stroke\n");
	 else  fprintf(fp, "fill\n");
	 break;
   }
}

void ps_line(int type, float x1, float y1, float x2, float y2, FILE *fp)
{
   switch (type) {
      case GBUF_AI88:
      case GBUF_AI3:
	 fprintf(fp, "%6.2f %6.2f m\n", x1,  y1);
	 fprintf(fp, "%6.2f %6.2f L\n", x2,  y2);
	 fprintf(fp, "S\n");
	 break;
      case GBUF_PS:
      case GBUF_EPS:
	 fprintf(fp, "%6.2f %6.2f moveto\n", x1,  y1);
	 fprintf(fp, "%6.2f %6.2f lineto\n", x2,  y2);
	 fprintf(fp, "stroke\n");
	 break;
   }
}

void ps_filled_poly (int type, int n, float *verts, FILE *fp)
{
  register int i;
  switch (type) {
  case GBUF_AI88:
  case GBUF_AI3:
    fprintf(fp, "%6.2f %6.2f m\n", verts[0],  verts[1]);
    for (i = 2; i < n; i+=2) {
      fprintf(fp, "%6.2f %6.2f L\n", verts[i],  verts[i+1]);
    }
    fprintf(fp, "F\n");
    break;
  case GBUF_PS:
  case GBUF_EPS:
    fprintf(fp, "%6.2f %6.2f moveto\n", verts[0],  verts[1]);
    for (i = 2; i < n; i+=2) {
      fprintf(fp, "%6.2f %6.2f lineto\n", verts[i],  verts[i+1]);
    }
    fprintf(fp, "closepath\nfill\nnewpath\n");
    break;
  }
}


void ps_poly (int type, int n, float *verts, FILE *fp)
{
  register int i;
  switch (type) {
  case GBUF_AI88:
  case GBUF_AI3:
    fprintf(fp, "%6.2f %6.2f m\n", verts[0],  verts[1]);
    for (i = 2; i < n; i+=2) {
      fprintf(fp, "%6.2f %6.2f L\n", verts[i],  verts[i+1]);
    }
    fprintf(fp, "S\n");
    break;
  case GBUF_PS:
  case GBUF_EPS:
    fprintf(fp, "%6.2f %6.2f moveto\n", verts[0],  verts[1]);
    for (i = 2; i < n; i+=2) {
      fprintf(fp, "%6.2f %6.2f lineto\n", verts[i],  verts[i+1]);
    }
    /* Close the path if the start and stop are the same */
    if ((verts[i-2] == verts[0]) && (verts[i-1] == verts[1]))
      fprintf(fp, "closepath\n");

    fprintf(fp, "stroke\nnewpath\n");
    break;
  }
}

void ps_filled_rect(int type, float x1, float y1, float x2, float y2, FILE *fp)
{
  switch (type) {
  case GBUF_AI88:
  case GBUF_AI3:
    fprintf(fp, "%6.2f %6.2f m\n", x1,  y1);
    fprintf(fp, "%6.2f %6.2f L\n", x1,  y2);
    fprintf(fp, "%6.2f %6.2f L\n", x2,  y2);
    fprintf(fp, "%6.2f %6.2f L\n", x2,  y1);
    fprintf(fp, "F\n");
    break;
  case GBUF_PS:
  case GBUF_EPS:
    fprintf(fp, "%6.2f %6.2f moveto\n", x1,  y1);
    fprintf(fp, "%6.2f %6.2f lineto\n", x1,  y2);
    fprintf(fp, "%6.2f %6.2f lineto\n", x2,  y2);
    fprintf(fp, "%6.2f %6.2f lineto\n", x2,  y1);
    fprintf(fp, "closepath\nfill\nnewpath\n");
    break;
  }
}


void ps_moveto(int type, float x, float y, FILE *fp)
{
  PS_curx = x;
  PS_cury = y;

  switch (type) {
  case GBUF_AI88:
  case GBUF_AI3:
  case GBUF_PS:
  case GBUF_EPS:
    fprintf(fp, "%6.2f %6.2f m\n", x, y);
    break;
  }
}

void ps_lineto(int type, float x, float y, FILE *fp)
{
  PS_linetox = x;
  PS_linetoy = y;

  switch (type) {
  case GBUF_AI88:
  case GBUF_AI3:
  case GBUF_PS:
  case GBUF_EPS:
    fprintf(fp, "%6.2f %6.2f L\n", x, y);
    break;
  }
}

void ps_stroke(int type, FILE *fp)
{
  if (PS_linetox == PS_curx && PS_linetoy == PS_cury) 
    fprintf(fp, "closepath\n");
  switch (type) {
  case GBUF_AI88:
  case GBUF_AI3:
  case GBUF_PS:
  case GBUF_EPS:
    fprintf(fp, "S\n");
    break;
  }
}

void ps_newpath(int type, FILE *fp)
{
   switch (type) {
      case GBUF_AI88:
      case GBUF_AI3:
	 fprintf(fp, "N\n");
	 break;
      case GBUF_PS:
      case GBUF_EPS:
	 break;
   }
}

void ps_fill(int type, FILE *fp) {
   switch (type) {
      case GBUF_AI88:
      case GBUF_AI3:
	 fprintf(fp, "f\n");
	 break;
      case GBUF_PS:
      case GBUF_EPS:
	 fprintf(fp, "fill\n");
	 break;
   }
}

void ps_text(int type, float x, float y, char *str, char *fontname,
	float fontsize, int just, int orientation, FILE *fp)
{
   float angle = 0.0, cs, sn;
   
   switch (orientation) {
      case 0: angle = 0.0;  break;
      case 1: angle = 90.0; break;
      case 2: angle = 180.0; break;
      case 3: angle = 270.0; break;
   }
   cs = (float) cos(angle*(PI/180.0));
   sn = (float) sin(angle*(PI/180.0));


   switch (type) {
      case GBUF_AI3:
	 fprintf(fp, "To\n");
	 fprintf(fp, "%5.4f %5.4f %5.4f %5.4f %6.2f %6.2f 0 Tp\nTP\n",
		 cs, sn, -sn, cs, x, y);
	 fprintf(fp, "/%s %4.1f Tf\n", fontname, fontsize);
	 fprintf(fp, "%d Tj\n", just+1);
	 fprintf(fp, "(%s) Tx\n", str);
	 fprintf(fp, "TO\n");
	 break;
      case GBUF_AI88:
	 fprintf(fp, "/%s %4.1f 8 0 %d z\n", fontname, fontsize, just+1);
	 fprintf(fp, "[%5.4f %5.4f %5.4f %5.4f %6.2f %6.2f]e\n",
		 cs, sn, -sn, cs, x, y);
	 fprintf(fp, "%d ", (int) strlen(str));
	 fprintf(fp, "(%s)t\nT\n", str);
	 break;
      case GBUF_PS:
      case GBUF_EPS:
	 switch (just) {
	    case LEFT_JUST:
	       fprintf(fp, "%6.3f %6.3f %6.3f (%s) l_show\n",
		       x, y, angle, str);
	       break;
	    case RIGHT_JUST:
	       fprintf(fp, "%6.3f %6.3f %6.3f (%s) r_show\n",
		       x, y, angle, str);
	       break;
	    case CENTER_JUST:
	       fprintf(fp, "%6.3f %6.3f %6.3f (%s) c_show\n",
		       x, y, angle, str);
	       break;
	 }
	 break;
   }
}

void ps_setdash(int type, int lstyle, FILE *fp)
{
   switch (type) {
      case GBUF_PS:
      case GBUF_EPS:
	 switch(lstyle) {
	    case 0:
	    case 1:
	       fprintf(fp, "[] 0 setdash\n");
	       break;
	    case 2:
	    case 3:
	    case 4:
	    case 5:
	    case 6:
	    case 7:
	       fprintf(fp, "[ %d %d ] 0 setdash\n", 1, lstyle-1);
	       break;
	 }
	 break;
   }
}

void ps_setwidth(int type, int lwidth, FILE *fp)
{
  switch (type) {
  case GBUF_PS:
  case GBUF_EPS:
    fprintf(fp, "%f setlinewidth\n", lwidth/100.0);
    break;
  }
}

void ps_setcolor(int type, int color, FILE *fp)
{
  switch(type) {
  case GBUF_PS:
  case GBUF_EPS:
#ifndef NO_RGBCOLOR
    /* 
     * This is a very ugly repair to allow both RGB color specification
     * and color indexing (for historical reasons).  The idea is that the
     * lower 5 bits are for indexing, and bits 6-30 are for specification
     * of an RGB color.  Thus, to get out the unsigned long corresponding
     * to an RGB specified color, the value must be right shifted 5 bits,
     * then separated into the R, G, and B components. (Note that the
     * special case of 0 happened to have already been assigned to black,
     * phew!).
     */

    if (color < 32) {
      fprintf(fp, "%4.2f %4.2f %4.2f setrgbcolor\n",
	      PSColorTableVals[color][0],
	      PSColorTableVals[color][1],
	      PSColorTableVals[color][2]);
    }
    else {
      unsigned int shifted = color >> 5;
      int r = ((shifted & 0xff0000) >> 16);
      int g = ((shifted & 0xff00) >> 8);
      int b = (shifted & 0xff);
      fprintf(fp, "%5.3f %5.3f %5.3f setrgbcolor\n",
	      r/256.0, g/256.0, b/256.0);
    }
#else
    fprintf(fp, "%4.2f setgray\n", PSColorTableVals[color][3]);
#endif    
    break;
  case GBUF_AI88:
  case GBUF_AI3:
    break;
  }
}
void ps_font(int type, char *fontname, float size, FILE *fp)
{
   switch(type) {
      case GBUF_PS:
      case GBUF_EPS:
	 fprintf(fp, "/%s findfont %5.1f scalefont setfont\n", fontname, size);
	 break;
   }
}

void add_ps_prologue(float w, float h, FILE *fp)
{
   time_t clock;
   int ori;

   fputs("%!PS-Adobe\n", fp);
   fputs("%%Creator: GBuf Graphics Utilities (BCM)\n", fp);
   fprintf(fp, "%%%%CreationDate: %s",(time(&clock),ctime(&clock)));
   
   if (PS_Orientation == PS_AUTOMATIC) {
     if (h > w) ori = PS_PORTRAIT;
     else ori = PS_LANDSCAPE;
   }
   else ori = PS_Orientation;

   /* Add bounding box info */
   switch (ori) {
   case PS_LANDSCAPE:
     fprintf(fp, "%%%%BoundingBox: %s\n", BoundingBox);
     break;
   case PS_PORTRAIT:
     if (!PS_FillPage) {		/* use w and h as bounding box */
       fprintf(fp,"%%%%BoundingBox: 0 0 %4.0f %4.0f\n", w, h);
     }
     else {
       fprintf(fp, "%%%%BoundingBox: %s\n", BoundingBox);
     }
   }

   fputs("/m { moveto } bind def\n", fp);
   fputs("/L { lineto } bind def\n", fp);
   fputs("/S { stroke } bind def\n", fp);
   fputs("/c { closepath } bind def\n", fp);

   ps_font(GBUF_PS, Fontname, Fontsize, fp);

   /* now a couple of definitions from "PostScript by Example", by */
   /* McGilton an Campione                                         */
   
   fputs("/charheight { gsave newpath 0 0 moveto false charpath\n", fp);
   fputs("flattenpath pathbbox exch pop 3 -1 roll pop grestore } def\n", fp);

   fputs("/stringheight { /lly 0.0 def /ury 0.0 def\n", fp);
   fputs("{ ( ) dup 0 4 -1 roll put charheight\n", fp);
   fputs("dup ury gt  { /ury exch def } { pop } ifelse\n", fp);
   fputs("dup lly lt { /lly exch def } { pop } ifelse } forall\n", fp);
   fputs("ury lly sub } def\n", fp);

   fputs("/c_show {                 %% stack = x y angle string\n", fp);
   fputs("matrix currentmatrix\n", fp);
   fputs("5 -2 roll translate\n", fp);
   fputs("3 -1 roll rotate\n", fp);
   fputs("exch\n", fp);
   fputs("dup stringwidth pop 2 div neg\n", fp);
   fputs("(1) stringheight 2 div neg moveto\n", fp);
   fputs("show\n", fp);
   fputs("setmatrix\n", fp);
   fputs("} def\n", fp);

   fputs("/l_show {                 %% stack = x y angle string\n", fp);
   fputs("matrix currentmatrix\n", fp);
   fputs("5 -2 roll translate\n", fp);
   fputs("3 -1 roll rotate\n", fp);
   fputs("exch\n", fp);
   fputs("0 \n", fp);
   fputs("(1) stringheight 2 div neg moveto\n", fp);
   fputs("show\n", fp);
   fputs("setmatrix\n", fp);
   fputs("} def\n", fp);

   fputs("/r_show {                 %% stack = x y angle string\n", fp);
   fputs("matrix currentmatrix\n", fp);
   fputs("5 -2 roll translate\n", fp);
   fputs("3 -1 roll rotate\n", fp);
   fputs("exch\n", fp);
   fputs("dup stringwidth pop neg\n", fp);
   fputs("(1) stringheight 2 div neg moveto\n", fp);
   fputs("show\n", fp);
   fputs("setmatrix\n", fp);
   fputs("} def\n", fp);

   /* Now put in final scaling/rotation info depending on orientation */
   switch (ori) {
   case PS_LANDSCAPE:
     fprintf(fp, "%% put in Landscape mode\n");
     fprintf(fp, "4.25 72 mul 6.5 72 mul translate\n");
     fprintf(fp, "90 rotate\n");
     fprintf(fp, "-6 72 mul -3.75 72 mul translate\n");
     fprintf(fp, "792 %7.2f div .9 mul 612 %6.2f div .9 mul scale\n", w, h); 
     break;
   case PS_PORTRAIT:
     if (PS_FillPage) {		/* use w and h as bounding box */
       fprintf(fp, "612 %6.2f div 792 %6.2f div scale\n", w, h); 
     }
     break;
   }
}

void add_eps_prologue(float w, float h, FILE *fp)
{
   time_t clock;
   fputs("%!PS-Adobe-3.0 EPSF-3.0\n", fp);
   fputs("%%Creator: GBuf Graphics Utilities (BCM)\n", fp);
   fprintf(fp, "%%%%CreationDate: %s",(time(&clock),ctime(&clock)));
   fprintf(fp,"%%%%BoundingBox: 0 0 %4.0f %4.0f\n", w, h);

   fputs("/m { moveto } bind def\n", fp);
   fputs("/L { lineto } bind def\n", fp);
   fputs("/S { stroke } bind def\n", fp);
   fputs("/c { closepath } bind def\n", fp);

   ps_font(GBUF_PS, Fontname, Fontsize, fp);

   /* now a couple of definitions from "PostScript by Example", by */
   /* McGilton an Campione                                         */
   
   fputs("/charheight { gsave newpath 0 0 moveto false charpath\n", fp);
   fputs("flattenpath pathbbox exch pop 3 -1 roll pop grestore } def\n", fp);

   fputs("/stringheight { /lly 0.0 def /ury 0.0 def\n", fp);
   fputs("{ ( ) dup 0 4 -1 roll put charheight\n", fp);
   fputs("dup ury gt  { /ury exch def } { pop } ifelse\n", fp);
   fputs("dup lly lt { /lly exch def } { pop } ifelse } forall\n", fp);
   fputs("ury lly sub } def\n", fp);

   fputs("/c_show {                 %% stack = x y angle string\n", fp);
   fputs("matrix currentmatrix\n", fp);
   fputs("5 -2 roll translate\n", fp);
   fputs("3 -1 roll rotate\n", fp);
   fputs("exch\n", fp);
   fputs("dup stringwidth pop 2 div neg\n", fp);
   fputs("(1) stringheight 2 div neg moveto\n", fp);
   fputs("show\n", fp);
   fputs("setmatrix\n", fp);
   fputs("} def\n", fp);

   fputs("/l_show {                 %% stack = x y angle string\n", fp);
   fputs("matrix currentmatrix\n", fp);
   fputs("5 -2 roll translate\n", fp);
   fputs("3 -1 roll rotate\n", fp);
   fputs("exch\n", fp);
   fputs("0 \n", fp);
   fputs("(1) stringheight 2 div neg moveto\n", fp);
   fputs("show\n", fp);
   fputs("setmatrix\n", fp);
   fputs("} def\n", fp);

   fputs("/r_show {                 %% stack = x y angle string\n", fp);
   fputs("matrix currentmatrix\n", fp);
   fputs("5 -2 roll translate\n", fp);
   fputs("3 -1 roll rotate\n", fp);
   fputs("exch\n", fp);
   fputs("dup stringwidth pop neg\n", fp);
   fputs("(1) stringheight 2 div neg moveto\n", fp);
   fputs("show\n", fp);
   fputs("setmatrix\n", fp);
   fputs("} def\n", fp);
}

void add_ai88_prologue(FILE *fp)
{
   time_t clock;
   fprintf(fp,"%%!PS-Adobe-2.0 EPSF-1.2\n");
   fprintf(fp,"%%%%Creator: GBuf Graphics Utilities (BCM)\n");
   fprintf(fp,"%%%%CreationDate: %s",(time(&clock),ctime(&clock)));
   fprintf(fp,"%%%%BoundingBox: %s\n", BoundingBox);
   fprintf(fp,"%%%%TemplateBox: %s\n", BoundingBox);
   fprintf(fp,"%%%%EndComments\n");
   fprintf(fp,"%%%%EndProlog\n");
   fprintf(fp,"%%%%BeginSetup\n");
   fprintf(fp,"%%%%EndSetup\n");
   ps_font(GBUF_AI88, "Helvetica", 12.0, fp);
   fprintf(fp,"0 G\n");
}

void add_ps_trailer(FILE *fp)
{
   fprintf(fp,"showpage\n");
}


void add_ai88_trailer(FILE *fp)
{
   fprintf(fp,"%%%%Trailer\n");
}

void add_ai3_prologue(FILE *fp)
{
   time_t clock;
   fprintf(fp,"%%!PS-Adobe-3.0 EPSF-3.0\n");
   fprintf(fp,"%%%%Creator: GBuf Graphics Utilities (BCM)\n");
   fprintf(fp,"%%%%CreationDate: %s",(time(&clock),ctime(&clock)));
   fprintf(fp,"%%%%BoundingBox: %s\n", BoundingBox);
   fprintf(fp,"%%%%EndComments\n");
   fprintf(fp,"%%%%EndProlog\n");
   fprintf(fp,"%%%%BeginSetup\n");
   fprintf(fp,"%%%%EndSetup\n");
   ps_font(GBUF_AI3, "Helvetica", 12.0, fp);
}

void add_ai3_trailer(FILE *fp)
{
   fprintf(fp, "%%%%Trailer\n");
   fprintf(fp, "%%%%EOF\n");
}


/*************************************************************************/
/*                            XFig Functions                             */
/*************************************************************************/


void fig_init(int type, float w, float h, FILE *fp)
{
  time_t clock;
  FigHeight = (int) h;
  fprintf(fp, "#FIG 2.1\n");
  fputs("# Creator: GBuf Graphics Utilities (BCM)\n", fp);
  fprintf(fp, "# CreationDate: %s",(time(&clock),ctime(&clock)));
  fputs("80\t2\n", fp);
}

/*
 * void fig_check_path - see if the current path needs to be stroked or filled
 */

void fig_check_path(int type, int *filling, int *stroking, FILE *fp)
{
  if (*filling  || *stroking) {
    fprintf(fp, "9999\t9999\n");
    *stroking = *filling = 0;
  }
}

void fig_point(int type, float ox, float oy, int color, FILE *fp)
{
  float y = (FigHeight-oy)*FigScale;
  float x = ox*FigScale;
  int cindex = -1;		/* should be a function of color */
  int radius = 3;
  fprintf(fp, 
	  "1 3 0 1 %d 0 0 21 0.0000 1 0.000 %d %d %d %d %d %d %d %d\n",
	  cindex, (int) x, (int) y, radius, radius, (int) x, (int) y,
	  (int) x+radius,  (int) y+radius);
}

void fig_line(int type, float ox1, float oy1, float ox2, float oy2, 
	      int style, int color, FILE *fp)
{
  float x1 = ox1*FigScale;
  float y1 = (FigHeight - oy1)*FigScale;
  float x2 = ox2*FigScale;
  float y2 = (FigHeight - oy2)*FigScale;
  int cindex = -1, styleindex = 0;
  float styleval = 0.0;
  if (style != 0) {
    styleindex = 2;
    styleval = style;
  }
  fprintf(fp, "2 1 %d 1 %d 0 0 0 %f -1 0 0\n", styleindex, cindex, styleval);
  fprintf(fp, "%d %d %d %d 9999 9999\n", (int)x1, (int)y1, (int)x2, (int)y2);
}

void fig_filled_rect(int type, float ox1, float oy1, float ox2, float oy2, 
		     int color, FILE *fp)
{
  float x1 = ox1*FigScale;
  float y1 = (FigHeight - oy1)*FigScale;
  float x2 = ox2*FigScale;
  float y2 = (FigHeight - oy2)*FigScale;
  int cindex = -1;
  fprintf(fp, "2 2 0 1 1 %d 0 0 21 0.000 0 0 0\n", cindex);
  fprintf(fp, "%d %d %d %d %d %d %d %d %d %d 9999 9999\n", 
	  (int)x1, (int)y1, (int)x1, (int)y2,
	  (int)x2, (int)y2, (int)x2, (int)y1,
	  (int)x1, (int)y1);
}

void fig_startline(int type, float ox0, float oy0, int style, 
		   int color, FILE *fp)
{
  float x0 = ox0*FigScale;
  float y0 = (FigHeight - oy0)*FigScale;
  int cindex = -1, styleindex = 0;
  float styleval = 0.0;
  if (style != 0) {
    styleindex = 2;
    styleval = style;
  }
    
  fprintf(fp, "2 1 %d 1 %d 0 0 0 %f -1 0 0\n", styleindex, cindex, styleval);
  fprintf(fp, "%d %d\n", (int) x0, (int) y0);
}

void fig_lineto(int type, float ox0, float oy0, FILE *fp)
{
  float x0 = ox0*FigScale;
  float y0 = (FigHeight - oy0)*FigScale;
  fprintf(fp, "%d %d\n", (int) x0, (int) y0);
}

void fig_text(int type, float ox, float oy, char *str, char *fontname,
	float ofontsize, int just, int orientation, FILE *fp)
{
  float y = (FigHeight-oy)*FigScale;
  float x = ox*FigScale;
  float fontsize = ofontsize*FigScale;
  float angle = 0.0;
  int fontid = 16;		/* should be based on fontname */
  int cindex = -1;
  
  switch (orientation) {
  case 0: 
    angle = 0.0;  break;
  case 1: 
    angle = PI/2.0; break;
  case 2: 
    angle = PI/2.0; break;
  }
  fprintf(fp, "4 %d %d %d 0 %d 0 %7.5f 4 %d %d %d %d %s%c\n",
	  just+1, fontid, 
	  (int) fontsize, cindex, angle, 
	  (int) fontsize+1, (int) (strlen(str)*fontsize), (int) x, (int) y,
	  str, 1);
}

