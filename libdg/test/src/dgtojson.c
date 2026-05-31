/*
 * dg_to_json
 *
 *   Turn a dynamic group into a json object
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <df.h>
#include <dynio.h>

#include <jansson.h>

extern json_t *dg_to_json(DYN_GROUP *dg);


/* gzip (.dgz) reads are decompressed fully in memory by
 * dguGzipFileToStruct() (dynio.c) -- no temp file. */

int main(int argc, char *argv[])
{
  DYN_GROUP *dg;
  FILE *fp;
  char *newname = NULL, *suffix;
  char tempname[128];
  json_t *root;
  char *json_str;
  
  if (argc < 2) {
    printf("usage: %s dgfile\n", argv[0]);
    exit(-1);
  }

  if (!(dg = dfuCreateDynGroup(4))) {
    printf("dg_read: error creating new dyngroup\n");
    exit(-1);
  }

  /* No need to uncompress a .dg file */
  if ((suffix = strrchr(argv[1], '.')) && strstr(suffix, "dg") &&
      !strstr(suffix, "dgz")) {
    fp = fopen(argv[1], "rb");
    if (!fp) {
      printf("dg_read: file %s not found\n", argv[1]);
      exit(-1);
    }
    tempname[0] = 0;
  }
  
  else if ((suffix = strrchr(argv[1], '.')) &&
	   strlen(suffix) == 4 &&
	   ((suffix[1] == 'l' && suffix[2] == 'z' && suffix[3] == '4') ||
	    (suffix[1] == 'L' && suffix[2] == 'Z' && suffix[3] == '4'))) {
    if (dgReadDynGroup(argv[1], dg) == DF_OK) {
      goto process_dg;
    }
    else {
      printf("dg_read: error reading .lz4 file\n");
      exit(-1);
    }
  }

  else {
    /* gzip-compressed (.dgz etc.): decompress fully in memory, no temp file. */
    int gstat = dguGzipFileToStruct(argv[1], dg);
    if (gstat != DF_OK) {
      char fullname[128];
      snprintf(fullname, sizeof(fullname), "%s.dg", argv[1]);
      gstat = dguGzipFileToStruct(fullname, dg);
      if (gstat != DF_OK) {
	snprintf(fullname, sizeof(fullname), "%s.dgz", argv[1]);
	gstat = dguGzipFileToStruct(fullname, dg);
      }
    }
    if (gstat != DF_OK) {
      printf("dg_read: file \"%s\" not found or not a dg file\n", argv[1]);
      exit(-1);
    }
    goto process_dg;
  }

  /* Only the raw uncompressed .dg branch reaches here (fp set above). */
  if (!dguFileToStruct(fp, dg)) {
    printf("dg_read: file %s not recognized as dg format\n", 
	    argv[1]);
    fclose(fp);
    if (tempname[0]) unlink(tempname);
    exit(-1);
  }
  fclose(fp);
  if (tempname[0]) unlink(tempname);

 process_dg:
  root = dg_to_json(dg);
  json_str = json_dumps(root, JSON_INDENT(4));
  printf("%s\n", json_str);
  free(json_str);
  
  dfuFreeDynGroup(dg);

  return 0;
}
