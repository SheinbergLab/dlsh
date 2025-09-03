/* gbufutl.h - header fil for gevent reader */

#ifndef _GBUF_STRING_H_
#define _GBUF_STRING_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct CgraphContext CgraphContext;

void gbuf_dump(CgraphContext *ctx, char *buffer, int n, int type, FILE *fp);
int gbuf_dump_ascii(unsigned char *gbuf, int bufsize, FILE *fp);
int gbuf_dump_ps(CgraphContext *ctx, unsigned char *gbuf, int bufsize, int type, FILE *OutFP);

/* PDF dump function - requires context for image access */
int gbuf_dump_pdf(CgraphContext *ctx, char *gbuf, int bufsize, char *filename);

void playback_gbuf(CgraphContext *ctx, unsigned char *gbuf, int bufsize);
void playback_gfile(CgraphContext *ctx, FILE *fp);

void read_gheader(FILE *InFP, FILE *OutFP);
void read_gline(char, FILE *InFP, FILE *OutFP);
void read_gpoint(char, FILE *InFP, FILE *OutFP);
void read_gpoly(char, FILE *InFP, FILE *OutFP);
void read_gtext(char, FILE *InFP, FILE *OutFP);
void read_gattr(char, FILE *InFP, FILE *OutFP);

void skip_gheader(FILE *InFP);
void skip_gline(FILE *InFP);
void skip_gpoint(FILE *InFP);
void skip_gpoly(FILE *InFP);
void skip_gtext(FILE *InFP);
void skip_gattr(FILE *InFP);

void get_gheader(FILE *InFP, float *, float *, float *);
void get_gpoint(FILE *InFP, float *, float *);
void get_gline(FILE *InFP, float *, float *, float *, float *);
void get_gpoly(FILE *InFP, int *, float **);
void get_gtext(FILE *InFP, float *, float *, int *, char **);
void get_gattr(char, FILE *InFP, int *);
int  get_timestamp(FILE *InFP, int *);

int gread_gheader(GHeader *, FILE *OutFP);
int gread_gline(char, GLine *, FILE *OutFP);
int gread_gpoint(char type, GPoint *, FILE *OutFP);
int gread_gpoly(char type, GPointList *, FILE *OutFP);
int gread_gtext(char type, GText *, FILE *OutFP);
int gread_gattr(char, GAttr *, FILE *OutFP);

int gskip_gheader(GHeader *);
int gskip_gline(GLine *);
int gskip_gpoint(GPoint *);
int gskip_gpoly(GPointList *);
int gskip_gtext(GText *);
int gskip_gattr(GAttr *);

int gget_gheader(GHeader *, float *, float *, float *);
int gget_gpoint(GPoint *, float *, float *);
int gget_gpoly(GPointList *, int *, float **);
int gget_gline(GLine *, float *, float *, float *, float *);
int gget_gtext(GText *, float *, float *, int *, char **);
int gget_gattr(char, GAttr *, int *);

void flip_gheader(GHeader *header);
void flip_gline(GLine *gline);
void flip_gpoint(GPoint *gpoint);
void flip_gpointlist(GPointList *gpointlist);
void flip_gtext(GText *gtext);
void flip_gattr(GAttr *gattr);

void ps_init(int type, float w, float h, FILE *fp);
void ps_portrait_mode(float w, float h, FILE *fp);
void ps_landscape_mode(float w, float h, FILE *fp);
void ps_gsave(int type, FILE *fp);
void ps_grestore(int type, FILE *fp);
void ps_clip(int type, float, float, float, float, FILE *fp);
void ps_point(int type, float x, float y, FILE *fp);
void ps_circle(int type, float x, float y, float size, float fill, FILE *fp);
void ps_line(int type, float, float, float, float, FILE *fp);
void ps_filled_rect(int type, float, float, float, float, FILE *fp);
void ps_filled_poly(int type, int, float *, FILE *fp);
void ps_poly(int type, int, float *, FILE *fp);
void ps_moveto(int type, float x, float y, FILE *fp);
void ps_lineto(int type, float x, float y, FILE *fp);
void ps_check_path(int type, FILE *fp);
void ps_stroke(int type, FILE *fp);
void ps_newpath(int type, FILE *fp);
void ps_font(int type, char *fontname, float size, FILE *fp);
void ps_setdash(int type, int style, FILE *fp);
void ps_setwidth(int type, int width, FILE *fp);
void ps_fill(int type, FILE *fp);
void ps_text(int type, float x, float y, char *str, char *, float,
	int, int, FILE *);
void ps_setcolor(int type, int color, FILE *fp);
void ps_group(int type, FILE *fp);
void ps_ungroup(int type, FILE *fp);
void ps_postscript(int type, float x0, float y0, char *string, 
		   int just, int orientation, FILE *fp);
void ps_image(int type, float x0, float y0, GBUF_IMAGE *img, FILE *fp);

void add_ps_prologue(float, float, FILE *fp);
void add_eps_prologue(float, float, FILE *fp);
void add_ai88_prologue(FILE *fp);
void add_ps_trailer(FILE *fp);
void add_ai88_trailer(FILE *fp);
void add_ai3_prologue(FILE *fp);
void add_ai3_trailer(FILE *fp);

extern float GB_Version;

/* String buffer structure for dynamic string building */
typedef struct {
    char *data;
    size_t size;        /* current allocated size */
    size_t length;      /* current string length (excluding null terminator) */
    size_t capacity;    /* initial/increment size */
} GBUF_STRING;

/* String buffer management functions */
GBUF_STRING *gbuf_string_create(size_t initial_capacity);
void gbuf_string_free(GBUF_STRING *str);
int gbuf_string_append(GBUF_STRING *str, const char *format, ...);
int gbuf_string_append_data(GBUF_STRING *str, const char *data, size_t len);
char *gbuf_string_detach(GBUF_STRING *str); /* Returns string, caller must free */
void gbuf_string_reset(GBUF_STRING *str);   /* Clear content but keep buffer */

unsigned char *gbuf_clean(unsigned char *input_gbuf, int input_size, int *output_size);

/* String output functions - ASCII command output only */
char *gbuf_dump_ascii_to_string(unsigned char *gbuf, int bufsize);

/* JSON output (using libjansson) */
char *gbuf_dump_json_direct(unsigned char *gbuf, int bufsize);

/* Lower-level string output functions */
int gbuf_dump_ascii_to_gbuf_string(unsigned char *gbuf, int bufsize, GBUF_STRING *str);

/* String versions of read functions */
int gread_gheader_to_string(GHeader *hdr, GBUF_STRING *str);
int gread_gline_to_string(char type, GLine *gln, GBUF_STRING *str);
int gread_gpoint_to_string(char type, GPoint *gpt, GBUF_STRING *str);
int gread_gpoly_to_string(char type, GPointList *gpl, GBUF_STRING *str);
int gread_gtext_to_string(char type, GText *gtx, GBUF_STRING *str);
int gread_gattr_to_string(char type, GAttr *gtr, GBUF_STRING *str);

int gbuf_dump_fig(CgraphContext *ctx, unsigned char *gbuf, int bufsize, int type, FILE *OutFP);
int gfile_to_fig(CgraphContext *ctx, FILE *InFP, int type, FILE *OutFP);
int gfile_to_ps(CgraphContext *ctx, FILE *InFP, int type, FILE *OutFP);

void fig_init(int type, float w, float h, FILE *fp);
void fig_check_path(int type, int *filling, int *stroking, FILE *fp);
void fig_point(int type, float x, float y, int color, FILE *fp);
void fig_line(int type, float x1, float y1, float x2, float y2, 
	      int style, int color, FILE *fp);
void fig_filled_rect(int type, float x1, float y1, float x2, float y2, 
		     int color, FILE *fp);
void fig_startline(int type, float x0, float y0, int style, 
		   int color, FILE *fp);
void fig_lineto(int type, float x0, float y0, FILE *fp);
void fig_text(int type, float x, float y, char *str, char *fontname,
	float fontsize, int just, int orientation, FILE *fp);

/* Context-aware functions that need updating */
int gbClearAndPlayback(CgraphContext *ctx);
void gbSetPageOrientation(CgraphContext *ctx, char ori);
void gbSetPageFill(CgraphContext *ctx, int status);

#ifdef __cplusplus
}
#endif

#endif