/************************************************************************/
/*                                                                      */
/*                              utilc.c                                 */
/*                    (general utility routines)                        */
/*                                                                      */
/*  This module includes functions for:                                 */
/*         Manipulating filenames/paths                                 */
/*         Finding matching files                                       */
/*         Flipping bytes                                               */
/*         Basic mathematical utils (canonicalize_angle)                */
/************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#ifndef WIN32
#define _REGEX_RE_COMP
#include <regex.h>
#include <sys/time.h>
#include <dirent.h>
#else
#include <time.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include "utilc.h"              /* utility functions                    */
#include "lablib.h"


static FileList FileMatchList;    /* used by find_matching_files()      */
static int rejected(char *pattern);
int find_matching_files(char *template, char *path);
int free_file_list(FileList *fl);
int re_adjust(char *newpattern, char *oldpattern);

/*
 * global routine to append given extention to a given filename
 */
char *SetExtension (char *pdest, char *psrc, char *pext)
{
  char *p;
  
  strcpy (pdest, psrc);
  p = strchr (pdest, '.');
    
  if (!p) {
    strcat(pdest,".");
    strcat(pdest, pext);
  }

  return (pdest);
}

char *SetNewExtension (char *pdest, char *psrc, char *pext)
{
  char *p;
  
  strcpy (pdest, psrc);

  for (p = pdest+strlen(pdest); p >= pdest; p--) {
    if (*p == '.') break;
  }
    
  if (*p != '.') {
    strcat(pdest, ".");
    strcat(pdest, pext);
  }
  else strcpy(++p, pext);

  return (pdest);
}

void ParseFileName (char *fname, char *name, char *ext)
{
   register int i;
   char *pd = name;

   *ext = 0;
   *name = 0;

   for (i = 0; i < 13; i++) {
	if (fname[i] == '.') {
		*pd = '\0';
		pd = ext;
	}
	if (fname[i] == '\0') break;
	*pd++ = fname[i];
   }
   *pd = '\0';
}

/*
 * Simulation of the "prefix" function. "Prefix" tests if string 's2' is a
 * prefix of the string 's1'. The value returned is YES, if the string 's2'
 * is a prefix of 's1', and NO, if otherwise.
 */
short prefix(char *s1, char *s2)
{
  if ((strstr(s1,s2) == s1) && (strcmp(s1,s2) > 0))
    return(YES);
  else
    return(NO);
}


int file_size(char *filename)
{
  struct stat buf;
  if (stat(filename, &buf) != -1) return(buf.st_size);
  else return(-1);
}

/*
 * find_matching_files: find matches for a specified (regex) pattern
 *
 * SYNOPSIS
 *   find_matching_files(pattern, path)
 *
 * DESCRIPTION
 *   find_matching_files finds all files which match "pattern" in the 
 * directory pointed to by "path", or the current dir if "path" is NULL.
 * pattern can include many of the regular expression wildcard conventions
 * including:
 *
 *           *:    match anything
 *           ?:    match any single character
 *       [a-z]:    match any of a, b, c, ..., z
 *       
 * Note that the period (as in *.* should be used only if the period is
 * supposed to be matched.  In other words, *.* will NOT match "makefile",
 * but * will.
 * 
 * RETURNS
 *  number of files matched
 *  0 upon failure
 *
 * REMARKS
 *  All filenames are converted to lowercase and are sorted for subsequent
 * calls to first_matching_file() or next_matching_file().
 *
 */

#if 0
int find_matching_files(char *template, char *path)
{
  static FileList *fl = &FileMatchList;             /* global structure */
  static int been_here;
  static DIR *dir;
  struct dirent *entry;
  int len;
  int filelist_compare ();
  char filename_only[255];


  if (!been_here) been_here = 1;
  else if (FL_FILENAMES(fl)) free_file_list(fl);
  
  if (path) {
    strcpy(FL_PATH(fl), path);
    file_basename(FL_PATTERN(fl), template);
  }
  
  else {
    file_pathname(FL_PATH(fl), template);
    file_basename(FL_PATTERN(fl), template);

    if (!strlen(FL_PATH(fl))) strcpy(FL_PATH(fl),"./");
  }

  
  /*
   * Add a trailing '/' if not already there
   */
  
  len = strlen(FL_PATH(fl));
  if (len && (FL_PATH(fl)[len-1] != '/')) {
    FL_PATH(fl)[len++] = '/';
    FL_PATH(fl)[len] = '\0';
  }

  
  /*
   * Set up the pattern for the regular expression matcher
   */
  
  re_adjust(FL_MANGLED_PATTERN(fl),FL_PATTERN(fl));
  re_comp(FL_MANGLED_PATTERN(fl));

  /* 
   * loop through once counting up the matches (not particularly efficient)
   */
  
  FL_NMATCHES(fl) = 0;
  if (!(dir = opendir(FL_PATH(fl)))) return(0);

  if (!(entry = readdir(dir))) { /* No files in the dir */
    return(0);
  }
  
  do {
    file_basename(filename_only,entry->d_name);
    if (!rejected(filename_only) && re_exec(filename_only)) FL_NMATCHES(fl)++;
  } while (entry = readdir(dir));
  
  
  /*
   * if no matches, return 0
   */
  
  if (!FL_NMATCHES(fl)) {
    FL_FILENAMES(fl) = NULL;
    return(0);
  }
  
  /*
   * get enough space for all matches
   */
  
  FL_CURRENT_MATCH(fl) = 0;
  FL_FILENAMES(fl) = (char **) calloc(FL_NMATCHES(fl), sizeof (char **));
  if (!FL_FILENAMES(fl)) {
    fprintf(stderr, "find_matching_files: out of memory\n");
    return(0);
  }
  
  /*
   * now that the space is allocated, loop through and store matches 
   */
  
  rewinddir(dir);
  
  if (!(entry = readdir(dir))) {
    return(0);
  }

  do {
    file_basename(filename_only,entry->d_name);
    if (!rejected(filename_only) && re_exec(filename_only)) {
      FL_FILENAME(fl, FL_CURRENT_MATCH(fl)) = 
	(char *) malloc(strlen(entry->d_name)+strlen(FL_PATH(fl))+1);
      sprintf(FL_FILENAME(fl, FL_CURRENT_MATCH(fl)), "%s%s", 
	      FL_PATH(fl), entry->d_name);
      FL_CURRENT_MATCH(fl)++;
    }
  } while (entry = readdir(dir));
  
  /* 
   * Now call qsort to sort the filenames alphabetically
   */
  
  qsort (FL_FILENAMES(fl), FL_NMATCHES(fl), sizeof (char *), filelist_compare);
  
  FL_CURRENT_MATCH(fl) = 0;
  return(FL_NMATCHES(fl));
}


/* 
 * compare() - used for qsort to compare two strings
 */

int filelist_compare (char **p1, char **p2)
{
  return(strcmp(*p1, *p2));
}

/* 
 * rejected() - used to disregard specific files (. and ..)
 */

static int rejected(char *pattern)
{
  return(!strcmp(pattern,".") || !strcmp(pattern,".."));
}


/*
 * NAME
 *  first_matching_file: returns the first file matching the pattern from
 *                       a previous find_matching_file() call
 *
 * SYNOPSIS
 *   char *first_matching_file()
 *
 * RETURNS
 *  pointer to filename 
 *  NULL if no matches exist
 *
 * REMARKS
 *  a call to first_matching_file resets the index so that calls to 
 *  next_matching_file will continue from the beginning
 *
 */

char *
first_matching_file(void)
{
  FileList *fl = &FileMatchList;
  if (FL_NMATCHES(fl)) {
    FL_CURRENT_MATCH(fl) = 1;
    return(FL_FILENAME(fl,0));
  }
  return(NULL);
}

char *
next_matching_file(void)
{
  FileList *fl = &FileMatchList;
  if (FL_CURRENT_MATCH(fl) == FL_NMATCHES(fl)) return(NULL);
  return(FL_FILENAME(fl,FL_CURRENT_MATCH(fl)++));
}

char **
all_matching_files(void)
{
  return(FL_FILENAMES(&FileMatchList));
}

int 
n_matching_files(void)
{
  return(FL_NMATCHES(&FileMatchList));
}
  
int free_file_list(FileList *fl)
{
  int i;
  for (i = 0; i < FL_NMATCHES(fl); i++) {
    free(FL_FILENAME(fl, i));
  }
  free(FL_FILENAMES(fl));
}

#endif

int file_rootname(char *rootname, char *name)
{
  char bname[128], *pch;

  file_basename(bname, name);
  if(pch = strrchr(bname, '.')) {
    *pch = 0;
  }
  strcpy(rootname, bname);
  
  if(pch) return(1);
  else return(0);
}


int file_basename(char *basename, char *name)
{
  int i;
  if (!strchr(name,'\\') && !strchr(name,'/')) {
    strcpy(basename, name);
    return(1);
  }
  
  i = strlen(name);
  while(--i) {
    if (name[i] == '\\' || name[i] == '/') {
      strcpy(basename, &name[i+1]);
      return(1);
    }
  }
  return(0);
}

int file_pathname(char *path, char *name)
{
  int i;
  if (!strchr(name,'\\') && !strchr(name,'/')) {
    strcpy(path, "");
    return(0);
  }
  
  i = strlen(name);
  while(--i) {
    if (name[i] == '\\' || name[i] == '/') {
      strncpy(path, name, i);
      path[i] = 0;
      return(1);
    }
  }
  return(0);
}

/*
 * re_adjust() - changes the supplied pattern for use with the traditional
 *  regular expression matcher, because *'s and .'s are treated differently
 */

int re_adjust(char *newpattern, char *oldpattern)
{
  int i;
  
  *newpattern++ = '^';
  for (i = 0; i < strlen(oldpattern); i++) {
    switch(oldpattern[i]) {
      case '*':
	*newpattern++ = '.';
	*newpattern++ = '*';
	break;
      case '.':
	*newpattern++ = '\\';
	*newpattern++ = '.';
	break;
      case '?':
	*newpattern++ = '.';
	break;
      default:
	*newpattern++ = tolower(oldpattern[i]);
	break;
    }
  }
  *newpattern++ = '$';
  *newpattern = 0;
}

/*
 * Routines for flipping bytes
 */

float
flipfloat(float oldf)
{
  float newf;
  char *old, *new;

  old = (char *) &oldf;
  new = (char *) &newf;

  new[0] = old[3];
  new[1] = old[2];
  new[2] = old[1];
  new[3] = old[0];
  
  return(newf);
}


double
flipdouble(double oldd)
{
  double newd;
  char *old, *new;

  old = (char *) &oldd;
  new = (char *) &newd;

  new[0] = old[7];
  new[1] = old[6];
  new[2] = old[5];
  new[3] = old[4];
  new[4] = old[3];
  new[5] = old[2];
  new[6] = old[1];
  new[7] = old[0];

  return(newd);
}


int
fliplong(int oldl)
{
  int newl;
  char *old, *new;

  old = (char *) &oldl;
  new = (char *) &newl;

  new[0] = old[3];
  new[1] = old[2];
  new[2] = old[1];
  new[3] = old[0];
  
  return(newl);
}

short
flipshort(short olds)
{
  short news;
  char *old, *new;

  old = (char *) &olds;
  new = (char *) &news;

  new[0] = old[1];
  new[1] = old[0];
  
  return(news);
}

void fliplongs(int n, int *vals)
{
  int i;
  for (i = 0; i < n; i++) vals[i] = fliplong(vals[i]);
}

void flipshorts(int n, short *vals)
{
  int i;
  for (i = 0; i < n; i++) vals[i] = flipshort(vals[i]);
}

void flipfloats(int n, float *vals)
{
  int i;
  for (i = 0; i < n; i++) vals[i] = flipfloat(vals[i]);
}

float canonicalize_angle(float angle)
{
  float newangle;
  int tmpangle;

  if (angle >= 0.0 && angle < 360.0) return(angle);
  
  tmpangle = (int) 100.0*angle;
  if (tmpangle < 0) {
    do {
      tmpangle += 36000;
    } while (tmpangle < 0);
  }
  else {
    do {
      tmpangle -= 36000;
    } while (tmpangle > 36000);
  }

  newangle = (float) tmpangle / 100.0;
  return(newangle);
}

void WaitForReturn (void)
{
   printf("Hit Return to continue: ");
   getchar();
}


#ifdef WIN32
int strcasecmp(char *a,char *b) { return stricmp(a,b); }
int strncasecmp(char *a,char *b, int n) 
{ 
	return strnicmp(a,b,n);
}
#endif
