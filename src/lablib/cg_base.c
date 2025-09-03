#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(SUN4) || defined(LYNX)
#include <unistd.h>
#endif

#include <tcl.h>
#include <jansson.h>
#include "cgraph.h"
#include "gbuf.h"
#include "gbufutl.h"

static int cgClearWindow(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  setfviewport(ctx, 0,0,1,1);
  clearscreen(ctx);
  return TCL_OK;
}

static int cgDumpWindow(ClientData clientData, Tcl_Interp *interp,
         int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  char *outfile = NULL;
  static char *usage = "usage: dumpwin {printer|ascii|raw|pdf|string|json}";
  if (argc > 2) outfile = argv[2];
  if (argc < 2) {
    Tcl_SetResult(interp, usage, TCL_STATIC);
    return TCL_ERROR;
  }
  
  if (!strcmp(argv[1],"printer")) {
    gbPrintGevents(ctx);
    return TCL_OK;
  }
  else if (!strcmp(argv[1],"raw")) {
    if (argc < 3) {
      Tcl_SetResult(interp, "usage: dumpwin raw filename", TCL_STATIC);
      return TCL_ERROR;
    }
    gbWriteGevents(ctx, outfile, GBUF_RAW);
    return(TCL_OK);
  }
  else if (!strcmp(argv[1],"ascii")) {
    gbWriteGevents(ctx, outfile, GBUF_ASCII);
    return(TCL_OK);
  }
  else if (!strcmp(argv[1],"pdf")) {
    gbWriteGevents(ctx, outfile, GBUF_PDF);
    return(TCL_OK);
  }
  else if (!strcmp(argv[1],"string")) {
    int original_size, clean_size;
    
    // Use gbuf_clean function with context data
    unsigned char *clean_buffer = gbuf_clean(GB_GBUF(ctx), GB_GBUFINDEX(ctx), &clean_size);
    if (!clean_buffer) {
      Tcl_SetResult(interp, "Error: Unable to clean graphics buffer", TCL_STATIC);
      return TCL_ERROR;
    }
    
    // Use cleaned buffer for string conversion
    char *result_string = gbuf_dump_ascii_to_string(clean_buffer, clean_size);
    free(clean_buffer); // Clean up the temporary cleaned buffer
    
    if (!result_string) {
      Tcl_SetResult(interp,
		    "Error: Unable to convert graphics buffer to string",
		    TCL_STATIC);
      return TCL_ERROR;
    }
    
    if (argc >= 3) {
      /* Store result in variable and return byte count */
      if (Tcl_SetVar(interp, argv[2], result_string, TCL_LEAVE_ERR_MSG) == NULL) {
	free(result_string);
	return TCL_ERROR;
      }
      char length_str[32];
      sprintf(length_str, "%d", (int)strlen(result_string));
      Tcl_SetResult(interp, length_str, TCL_VOLATILE);
      free(result_string);
    } else {
      /* ASCII string uses malloc - need to copy for Tcl */
      Tcl_SetResult(interp, result_string, TCL_VOLATILE);
      free(result_string);
    }
    return TCL_OK;
  }
  else if (!strcmp(argv[1], "json")) {
    int original_size, clean_size;
    
    // Use gbuf_clean function with context data
    unsigned char *clean_buffer = gbuf_clean(GB_GBUF(ctx), GB_GBUFINDEX(ctx), &clean_size);
    if (!clean_buffer) {
      Tcl_SetResult(interp, "Error: Unable to clean graphics buffer", TCL_STATIC);
      return TCL_ERROR;
    }
    
    // Use cleaned buffer for JSON conversion
    char *result_string = gbuf_dump_json_direct(clean_buffer, clean_size);
    free(clean_buffer); // Clean up the temporary cleaned buffer
    
    if (!result_string) {
      Tcl_SetResult(interp,
		    "Error: Unable to convert graphics buffer to JSON",
		    TCL_STATIC);
      return TCL_ERROR;
    }
    
    if (argc >= 3) {
      /* Store result in variable and return byte count */
      if (Tcl_SetVar(interp, argv[2],
		     result_string, TCL_LEAVE_ERR_MSG) == NULL) {
	free(result_string);  // jansson uses free()
	return TCL_ERROR;
      }
      char length_str[32];
      sprintf(length_str, "%d", (int)strlen(result_string));
      Tcl_SetResult(interp, length_str, TCL_VOLATILE);
      free(result_string);
    } else {
      /* Let Tcl make its own copy, then free the jansson memory */
      Tcl_SetResult(interp, result_string, TCL_VOLATILE);
      free(result_string);
    }
    return TCL_OK;
  }
  else {
    Tcl_SetResult(interp, usage, TCL_STATIC);
    return TCL_ERROR;
  }
}

static int cgPlayback(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  FILE *fp;
  if (argc < 2) {
    Tcl_SetResult(interp, "usage: gbufplay filename", TCL_STATIC);
    return TCL_ERROR;
  }
  
  if (!(fp = fopen(argv[1],"rb"))) {
    Tcl_AppendResult(interp, argv[0], ": unable to open file ", argv[1], NULL);
    return TCL_ERROR;
  }
  playback_gfile(ctx, fp);
  
  fclose(fp);
  
  return(TCL_OK);
}

static int gbSizeCmd(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  Tcl_SetObjResult(interp, Tcl_NewIntObj(gbSize(ctx)));
  return TCL_OK;
}

static int gbIsEmptyCmd(ClientData clientData, Tcl_Interp *interp,
		     int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  Tcl_SetObjResult(interp, Tcl_NewIntObj(gbIsEmpty(ctx)));
  return TCL_OK;
}

static int gbResetCmd(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  gbResetGeventBuffer(ctx);
  return TCL_OK;
}

static int gbCleanCmd(ClientData clientData, Tcl_Interp *interp, 
                      int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int original_size, clean_size;
  if (argc != 1) {
      Tcl_SetResult(interp, "Usage: gbufclean", TCL_STATIC);
      return TCL_ERROR;
  }
  
  // Clean the current buffer
  int result = gbCleanGeventBuffer(ctx);
  
  if (result != 0) {
      Tcl_SetResult(interp, "Failed to clean graphics buffer", TCL_STATIC);
      return TCL_ERROR;
  }
  return TCL_OK;
}

static int cgGetResol(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  float x, y;
  getresol(ctx, &x, &y);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}

static int cgGetFrame(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  char resultstr[32];
  snprintf(resultstr, sizeof(resultstr), "%p", getframe(ctx));
  Tcl_AppendResult(interp, resultstr, NULL);
  return TCL_OK;
}

static int cgGetXScale(ClientData clientData, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj(getxscale(ctx)));
  return TCL_OK;
}

static int cgGetYScale(ClientData clientData, Tcl_Interp *interp,
		       int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj(getyscale(ctx)));
  return TCL_OK;
}

static int cgWindowToScreen(ClientData clientData, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x0, y0;
  int x, y;
  if (argc != 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " x y", (char *) NULL);
      return TCL_ERROR;
  }
  if (Tcl_GetDouble(interp, argv[1], &x0) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y0) != TCL_OK) return TCL_ERROR;
  window_to_screen(ctx, x0, y0, &x, &y);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(x));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(y));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}

static int cgScreenToWindow(ClientData clientData, Tcl_Interp *interp,
			    int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int x0, y0;
  float x, y;
  if (argc != 3) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " x y", (char *) NULL);
      return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &x0) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &y0) != TCL_OK) return TCL_ERROR;
  screen_to_window(ctx, x0, y0, &x, &y);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}

static int cgPushViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  if (!strcmp(argv[0],"pushpviewport")) {
    double x0, y0, x1, y1;
    if (argc != 5 && argc != 1) {
      Tcl_AppendResult(interp, "usage: ", argv[0], " [x0 y0 x1 y1]", 
		       (char *) NULL);
      return TCL_ERROR;
    }
    if (argc == 5) {
      if (Tcl_GetDouble(interp, argv[1], &x0) != TCL_OK) return TCL_ERROR;
      if (Tcl_GetDouble(interp, argv[2], &y0) != TCL_OK) return TCL_ERROR;
      if (Tcl_GetDouble(interp, argv[3], &x1) != TCL_OK) return TCL_ERROR;
      if (Tcl_GetDouble(interp, argv[4], &y1) != TCL_OK) return TCL_ERROR;
    }
    pushviewport(ctx);
    if (argc == 5) setpviewport(ctx, x0, y0, x1, y1);
    return TCL_OK;
  }
  pushviewport(ctx);
  return TCL_OK;
}

static int cgPopViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  if (argc > 1) {
    while (popviewport(ctx));
    return TCL_OK;
  }
  if (popviewport(ctx)) return TCL_OK;
  else {
    Tcl_AppendResult(interp, argv[0], ": popped empty stack", NULL);
    return TCL_ERROR;
  }
}

static int cgSetViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " lx by rx ty", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  setviewport(ctx, x1, y1, x2, y2);
  return TCL_OK;
}

static int cgGetViewport(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  float x1, y1, x2, y2;

  if (argc != 1) {
    Tcl_AppendResult(interp, "usage: ", argv[0], NULL);
    return TCL_ERROR;
  }

  getviewport(ctx, &x1, &y1, &x2, &y2);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x1));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y1));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x2));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y2));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}

static int cgGetFViewport(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  float x1, y1, x2, y2;
  float w, h;

  if (argc != 1) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " getfviewport", NULL);
    return TCL_ERROR;
  }

  getviewport(ctx, &x1, &y1, &x2, &y2);
  getresol(ctx, &w, &h);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x1/w));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y1/h));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x2/w));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y2/h));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;  
}

static int cgGetWindow(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  float x1, y1, x2, y2;

  if (argc != 1) {
    Tcl_AppendResult(interp, "usage: ", argv[0], NULL);
    return TCL_ERROR;
  }

  getwindow(ctx, &x1, &y1, &x2, &y2);

  Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x1));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y1));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(x2));
  Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewDoubleObj(y2));
  Tcl_SetObjResult(interp, listPtr);

  return TCL_OK;
}

static int cgGetAspect(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  float x1, y1, x2, y2;

  if (argc != 1) {
    Tcl_AppendResult(interp, "usage: ", argv[0], NULL);
    return TCL_ERROR;
  }

  getviewport(ctx, &x1, &y1, &x2, &y2);
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj((x2-x1)/(y2-y1)));
  return TCL_OK;
}

static int cgGetUAspect(ClientData clientData, Tcl_Interp *interp,
			int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj(getuaspect(ctx)));
  return TCL_OK;
}

static int cgSetFViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " lx by rx ty", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  setfviewport(ctx, x1, y1, x2, y2);
  return TCL_OK;
}

static int cgSetPViewport(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " lx by rx ty", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  setpviewport(ctx, x1, y1, x2, y2);
  return TCL_OK;
}

static int cgSetWindow(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " top left bottom right",
    	(char *) NULL) ;
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  setwindow(ctx, x1, y1, x2, y2);
  return TCL_OK;
}

static int cgSetResol(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double w, h;

  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " width height",
    	(char *) NULL) ;
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &w) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &h) != TCL_OK) return TCL_ERROR;
  
  setresol(ctx, w, h);
  return TCL_OK;
}

static int cgSetPSPageOri(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  static char *usage_message = "usage: setpageori {landscape|portrait}";

  if (argc != 2) {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }
  if (!strcmp(argv[1],"landscape") || !strcmp(argv[1],"LANDSCAPE"))
    gbSetPageOrientation(ctx, PS_LANDSCAPE);
  else if (!strcmp(argv[1],"portrait") || !strcmp(argv[1],"PORTRAIT"))
    gbSetPageOrientation(ctx, PS_PORTRAIT);
  else {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }
  return TCL_OK;
}

static int cgSetPSPageFill(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  static char *usage_message = "usage: setpagefill {0|1}";
  int status = 0;

  if (argc != 2) {
    Tcl_SetResult(interp, usage_message, TCL_STATIC);
    return TCL_ERROR;
  }
  
  if (Tcl_GetInt(interp, argv[1], &status) != TCL_OK) return TCL_ERROR;
  gbSetPageFill(ctx, status);

  return TCL_OK;
}

static int cgGsave(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  gsave(ctx);
  return TCL_OK;
}

static int cgGrestore(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  if (grestore(ctx)) return TCL_OK;
  else {
    Tcl_SetResult(interp, "grestore: popped empty stack", TCL_STATIC);
    return TCL_ERROR;
  }
}

static int cgGroup(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  group(ctx);
  return TCL_OK;
}

static int cgUngroup(ClientData clientData, Tcl_Interp *interp,
		   int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  ungroup(ctx);
  return TCL_OK;
}

static int cgFrame(ClientData clientData, Tcl_Interp *interp,
	    int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  frame(ctx);
  return TCL_OK;
}

static int cgMoveto(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x, y;

  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  moveto(ctx, x, y);
  return TCL_OK;
}

static int cgLineto(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x, y;

  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  lineto(ctx, x, y);
  return TCL_OK;
}

static int cgDotAt(ClientData clientData, Tcl_Interp *interp,
	     int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x, y;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  dotat(ctx, x, y);
  return TCL_OK;
}

static int cgSquare(ClientData clientData, Tcl_Interp *interp,
	     int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x, y, scale = 3.0;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y {scale}", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  if (argc > 3) {
    if (Tcl_GetDouble(interp, argv[3], &scale) != TCL_OK) return TCL_ERROR;
  }

  square(ctx, x, y, scale);
  return TCL_OK;
}

static int cgFsquare(ClientData clientData, Tcl_Interp *interp,
	     int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x, y, scale = 3.0;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y {scale}", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  if (argc > 3) {
    if (Tcl_GetDouble(interp, argv[3], &scale) != TCL_OK) return TCL_ERROR;
  }

  fsquare(ctx, x, y, scale);
  return TCL_OK;
}

static int cgPoly(ClientData clientData, Tcl_Interp *interp,
	    int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int i, n = argc-1;
  float *verts;
  double vert;
  
  if (n < 6 || n%2) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " x0 y0 x1 y1 x2 y2 [x3 y3 ... xn yn]", NULL);
    return TCL_ERROR;
  }
  
  verts = (float *) calloc(n, sizeof(float));
  
  for (i = 0; i < n; i++) {
    if (Tcl_GetDouble(interp, argv[i+1], &vert) != TCL_OK) return TCL_ERROR;
    verts[i] = vert;
  }
  
  polyline(ctx, n/2, verts);
  
  free(verts);
  return TCL_OK;
}

static int cgFpoly(ClientData clientData, Tcl_Interp *interp,
	    int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int i, n = argc-1;
  float *verts;
  double vert;
  
  if (n < 6 || n%2) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " x0 y0 x1 y1 x2 y2 [x3 y3 ... xn yn]", NULL);
    return TCL_ERROR;
  }
  
  verts = (float *) calloc(n, sizeof(float));
  
  for (i = 0; i < n; i++) {
    if (Tcl_GetDouble(interp, argv[i+1], &vert) != TCL_OK) return TCL_ERROR;
    verts[i] = vert;
  }
  
  filledpoly(ctx, n/2, verts);
  
  free(verts);
  return TCL_OK;
}

static int cgCircle(ClientData clientData, Tcl_Interp *interp,
	     int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x, y, scale = 3.0;

  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y {scale}", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  if (argc > 3) {
    if (Tcl_GetDouble(interp, argv[3], &scale) != TCL_OK) return TCL_ERROR;
  }
  
  circle(ctx, x, y, scale);
  return TCL_OK;
}

static int cgFcircle(ClientData clientData, Tcl_Interp *interp,
	      int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x, y, scale = 3.0;
  
  if (argc < 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " x y {scale}", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[1], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y) != TCL_OK) return TCL_ERROR;
  
  if (argc > 3) {
    if (Tcl_GetDouble(interp, argv[3], &scale) != TCL_OK) return TCL_ERROR;
  }

  fcircle(ctx, x, y, scale);
  return TCL_OK;
}

static int cgFilledRect(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " lx by rx ty", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  filledrect(ctx, x1, y1, x2, y2);
  return TCL_OK;
}

static int cgRect(ClientData clientData, Tcl_Interp *interp,
		  int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " lx by rx ty", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  rect(ctx, x1, y1, x2, y2);
  return TCL_OK;
}

static int cgSetorientation(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int orient, oldori;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " {0|1}", NULL); 
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &orient) != TCL_OK) return TCL_ERROR;
  oldori = setorientation(ctx, orient);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldori));
  return TCL_OK;
}

static int cgSetjust(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int just, oldjust;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " {-1|0|1}", NULL); 
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &just) != TCL_OK) return TCL_ERROR;
  oldjust = setjust(ctx, just);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldjust));
  return TCL_OK;
}

static int cgSetlstyle(ClientData clientData, Tcl_Interp *interp,
		 int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int lstyle, oldlstyle;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " {0-8}", NULL); 
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &lstyle) != TCL_OK) return TCL_ERROR;
  oldlstyle = setlstyle(ctx, lstyle);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldlstyle));
  return TCL_OK;
}

static int cgSetlwidth(ClientData clientData, Tcl_Interp *interp,
	    int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int lwidth, oldlwidth;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " points*100", NULL); 
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &lwidth) != TCL_OK) return TCL_ERROR;
  oldlwidth = setlwidth(ctx, lwidth);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldlwidth));
  return TCL_OK;
}

static int cgRGBcolor(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  int colorval, r, g, b;
  if (argc != 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " r g b", NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &r) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[2], &g) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetInt(interp, argv[3], &b) != TCL_OK) return TCL_ERROR;
  
  if (r > 255 || g > 255 || b > 255 || r < 0 || g < 0 || b < 0) {
    Tcl_AppendResult(interp, argv[0], ": color out of range", NULL);
    return TCL_ERROR;
  }
  
  /* 
   * This is a crazy color scheme, which allows 5 bits of color index
   * and another 24 bits above that for direct color specification
   */
  colorval = (r << 21) + (g << 13) + (b << 5);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(colorval));
  return TCL_OK;
}

static int cgGetcolor(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int oldcolor = getcolor(ctx);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldcolor));
  return TCL_OK;
}

static int cgSetcolor(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int color, oldcolor;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " color", NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &color) != TCL_OK) return TCL_ERROR;
  oldcolor = setcolor(ctx, color);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldcolor));
  return TCL_OK;
}

static int cgSetBackgroundColor(ClientData clientData, Tcl_Interp *interp,
                                int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int color, oldcolor;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " color", NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &color) != TCL_OK) return TCL_ERROR;
  oldcolor = setbackgroundcolor(ctx, color);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(oldcolor));
  return TCL_OK;
}

static int cgSetfont(ClientData clientData, Tcl_Interp *interp,
		     int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double size;
  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " fontname pointsize", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[2], &size) != TCL_OK) return TCL_ERROR;
  setfont(ctx, argv[1], size);
  return TCL_OK;
}

static int cgSetsfont(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double size, newsize;
  if (argc != 3) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " fontname pointsize", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[2], &size) != TCL_OK) return TCL_ERROR;
    
  newsize = setsfont(ctx, argv[1], size);
  Tcl_SetObjResult(interp, Tcl_NewDoubleObj(newsize));
  return TCL_OK;
}

static int cgPostscript(ClientData clientData, Tcl_Interp *interp,
			int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x, y;
  if (argc != 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     " filename xscale yscale", NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetDouble(interp, argv[2], &x) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &y) != TCL_OK) return TCL_ERROR;
  postscript(ctx, argv[1], x, y);
  return TCL_OK;
}

static int cgSetImagePreview(ClientData clientData, Tcl_Interp *interp,
			     int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int val;
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " 0|1", NULL);
    return TCL_ERROR;
  }
  if (Tcl_GetInt(interp, argv[1], &val) != TCL_OK) return TCL_ERROR;
  int old_val = setimgpreview(ctx, val);
  Tcl_SetObjResult(interp, Tcl_NewIntObj(old_val));
  return TCL_OK;
}

static int cgDrawtext(ClientData clientData, Tcl_Interp *interp,
		      int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  if (argc != 2) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " text", NULL);
    return TCL_ERROR;
  }

  drawtext(ctx, argv[1]);
  return TCL_OK;
}

static int cgSetclip(ClientData clientData, Tcl_Interp *interp,
		     int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  int clip;
  if (argc > 1) {
    if (Tcl_GetInt(interp, argv[1], &clip) != TCL_OK) return TCL_ERROR;
    setclip(ctx, clip);
  }
  Tcl_SetObjResult(interp, Tcl_NewIntObj(getclip(ctx)));
  return TCL_OK;
}

static int cgSetClipRegion(ClientData clientData, Tcl_Interp *interp,
			   int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double x1, y1, x2, y2;

  if (argc != 5) {
    Tcl_AppendResult(interp, "usage: ", argv[0], " top left bottom right",
    	(char *) NULL) ;
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[1], &x1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[2], &y1) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[3], &x2) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetDouble(interp, argv[4], &y2) != TCL_OK) return TCL_ERROR;
  
  setclipregion(ctx, x1, y1, x2, y2);
  return TCL_OK;
}

static int cgLYaxis(ClientData clientData, Tcl_Interp *interp,
		    int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double pos, tic;
  int interval;
  char *title = NULL;
  
  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     "xpos tick_interval label_interval [title]", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[1], &pos) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad xposition specified", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[2], &tic) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad tick interval specified", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetInt(interp, argv[3], &interval) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad label interval specified", NULL);
    return TCL_ERROR;
  }
  
  if (argc > 4) title = argv[4];
  lyaxis(ctx, pos, tic, interval, title);
  return TCL_OK;
}

static int cgLXaxis(ClientData clientData, Tcl_Interp *interp,
		    int argc, char *argv[])
{
  CgraphContext *ctx = (CgraphContext *) clientData;
  if (!ctx) {
    Tcl_SetResult(interp, "Failed to get graphics context", TCL_STATIC);
    return TCL_ERROR;
  }
  
  double pos, tic;
  int interval;
  char *title = NULL;
  
  if (argc < 4) {
    Tcl_AppendResult(interp, "usage: ", argv[0],
		     "ypos tick_interval label_interval [title]", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetDouble(interp, argv[1], &pos) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad yposition specified", NULL);
    return TCL_ERROR;
  }

  if (Tcl_GetDouble(interp, argv[2], &tic) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad tick interval specified", NULL);
    return TCL_ERROR;
  }
  
  if (Tcl_GetInt(interp, argv[3], &interval) != TCL_OK) {
    Tcl_AppendResult(interp, argv[0], ": bad label interval specified", NULL);
    return TCL_ERROR;
  }
  
  if (argc > 4) title = argv[4];
  lxaxis(ctx, pos, tic, interval, title);
  return TCL_OK;
}

/* Clean initialization in cg_base.c */

int Cgbase_Init(Tcl_Interp *interp)
{
    if (
#ifdef USE_TCL_STUBS
        Tcl_InitStubs(interp, "8.6-", 0)
#else
        Tcl_PkgRequire(interp, "Tcl", "8.6-", 0)
#endif
        == NULL) {
        return TCL_ERROR;
    }
    
    /* Create and initialize the graphics context for this interpreter */
    CgraphContext *ctx = CgraphCreateContext(interp);
    if (!ctx) {
        Tcl_SetResult(interp, "Failed to create graphics context", TCL_STATIC);
        return TCL_ERROR;
    }
    
    /* Register all Tcl commands with the context as ClientData */
    Tcl_CreateCommand(interp, "clearwin", (Tcl_CmdProc *) cgClearWindow, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "getresol", (Tcl_CmdProc *) cgGetResol, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "getxscale", (Tcl_CmdProc *) cgGetXScale, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "getyscale", (Tcl_CmdProc *) cgGetYScale, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "getframe", (Tcl_CmdProc *) cgGetFrame, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "wintoscreen", (Tcl_CmdProc *) cgWindowToScreen, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "screentowin", (Tcl_CmdProc *) cgScreenToWindow, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "dumpwin", (Tcl_CmdProc *) cgDumpWindow, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "gbufplay", (Tcl_CmdProc *) cgPlayback, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "pushviewport", (Tcl_CmdProc *) cgPushViewport, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "pushpviewport", (Tcl_CmdProc *) cgPushViewport, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "popviewport", (Tcl_CmdProc *) cgPopViewport, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "poppviewport", (Tcl_CmdProc *) cgPopViewport, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setviewport", (Tcl_CmdProc *) cgSetViewport, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "getviewport", (Tcl_CmdProc *) cgGetViewport, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "getwindow", (Tcl_CmdProc *) cgGetWindow, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "getfviewport", (Tcl_CmdProc *) cgGetFViewport, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "getaspect", (Tcl_CmdProc *) cgGetAspect, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "getuaspect", (Tcl_CmdProc *) cgGetUAspect, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setfviewport", (Tcl_CmdProc *) cgSetFViewport, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setpviewport", (Tcl_CmdProc *) cgSetPViewport, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setresol", (Tcl_CmdProc *) cgSetResol, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setwindow", (Tcl_CmdProc *) cgSetWindow, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setpageori", (Tcl_CmdProc *) cgSetPSPageOri, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setpagefill", (Tcl_CmdProc *) cgSetPSPageFill, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "postscript", (Tcl_CmdProc *) cgPostscript, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setimgpreview", (Tcl_CmdProc *) cgSetImagePreview, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "group", (Tcl_CmdProc *) cgGroup, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "ungroup", (Tcl_CmdProc *) cgUngroup, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "gsave", (Tcl_CmdProc *) cgGsave, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "grestore", (Tcl_CmdProc *) cgGrestore, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "cgframe", (Tcl_CmdProc *) cgFrame, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "moveto", (Tcl_CmdProc *) cgMoveto, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "lineto", (Tcl_CmdProc *) cgLineto, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "poly", (Tcl_CmdProc *) cgPoly, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "fpoly", (Tcl_CmdProc *) cgFpoly, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "fsquare", (Tcl_CmdProc *) cgFsquare, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "square", (Tcl_CmdProc *) cgSquare, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "fcircle", (Tcl_CmdProc *) cgFcircle, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "circle", (Tcl_CmdProc *) cgCircle, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "point", (Tcl_CmdProc *) cgDotAt, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "rect", (Tcl_CmdProc *) cgRect, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "filledrect", (Tcl_CmdProc *) cgFilledRect, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setfont", (Tcl_CmdProc *) cgSetfont, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setsfont", (Tcl_CmdProc *) cgSetsfont, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "drawtext", (Tcl_CmdProc *) cgDrawtext, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setjust", (Tcl_CmdProc *) cgSetjust, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setclip", (Tcl_CmdProc *) cgSetclip, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setclipregion",
		      (Tcl_CmdProc *) cgSetClipRegion, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setorientation",
		      (Tcl_CmdProc *) cgSetorientation, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setlstyle", (Tcl_CmdProc *) cgSetlstyle, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setlwidth", (Tcl_CmdProc *) cgSetlwidth, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setcolor", (Tcl_CmdProc *) cgSetcolor, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "setbackground", (Tcl_CmdProc *) cgSetBackgroundColor,
		      (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "getcolor", (Tcl_CmdProc *) cgGetcolor, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "rgbcolor", (Tcl_CmdProc *) cgRGBcolor, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "lxaxis", (Tcl_CmdProc *) cgLXaxis, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "lyaxis", (Tcl_CmdProc *) cgLYaxis, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "gbufsize", (Tcl_CmdProc *) gbSizeCmd, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "gbufclean", (Tcl_CmdProc *) gbCleanCmd, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "gbufisempty", (Tcl_CmdProc *) gbIsEmptyCmd, (ClientData)ctx, NULL);
    Tcl_CreateCommand(interp, "gbufreset", (Tcl_CmdProc *) gbResetCmd, (ClientData)ctx, NULL);
    
    return TCL_OK;
}
