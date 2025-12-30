/************************************************************************/
/*                                                                      */
/* randvars.c - random number functions                                 */
/*                                                                      */
/************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <sys/time.h>
#else
#include <time.h>
#include <windows.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include "utilc.h"              /* utility functions                    */
#include "lablib.h"

#ifdef QNX
#include <unix.h>
#endif

static float ran1(int *idum);  /* Numerical recipes                    */
static float ran4(int *idum);  /* Numerical recipes                    */
static float gasdev(int *idum);
static void psdes(unsigned int *lword, unsigned int *irword);

double drand48();
static int my_rseed = 0; 

/*
 * Random Number Stuff.....
 */

int raninit()
{
#ifdef WIN32
  __int64 timeval;

  if (!QueryPerformanceFrequency((LARGE_INTEGER *) &timeval)) {
    struct tm time;
    mktime(&time);
    my_rseed = (int) ((time.tm_sec*time.tm_sec) + time.tm_min +
		      time.tm_hour*time.tm_mday);
  }
  else {
    QueryPerformanceCounter((LARGE_INTEGER *) &timeval);
    my_rseed = (int) timeval;
  }
  srand(my_rseed);
  return((int) my_rseed);
#elif defined(LINUX) || defined(MACOS) || defined(FREEBSD)
  FILE *fp;
  if ((fp = fopen("/dev/urandom", "r")) == NULL) {
    my_rseed = 1234567;	/* should never happen */
  }
  else {
    fread(&my_rseed, sizeof(int), 1, fp);
    fclose(fp);
  }
  srand48(my_rseed);
  srand(my_rseed);
  return(my_rseed);
#else
  struct timeval time;
  struct timezone tz;
#ifndef QNX
  my_rseed = (int) ((time.tv_sec*time.tv_sec) + time.tv_usec+time.tm_min);
#else
  my_rseed = (int) ((time.tv_sec*time.tv_sec) + time.tv_usec);
#endif
  srand48(my_rseed);
  return(my_rseed);
#endif
}

/*
 * ranset(int seed)
 *   If seed > 0: use that seed directly
 *   If seed <= 0: auto-seed from system entropy (like raninit)
 *   Returns the seed that was used
 */

int ranset(int seed)
{
    if (seed > 0) {
        /* Use provided seed directly */
        my_rseed = seed;
    }
    else {
        /* Auto-seed from system entropy */
#ifdef WIN32
        __int64 timeval;
        if (!QueryPerformanceFrequency((LARGE_INTEGER *) &timeval)) {
            struct tm time;
            mktime(&time);
            my_rseed = (int) ((time.tm_sec*time.tm_sec) + time.tm_min +
                              time.tm_hour*time.tm_mday);
        }
        else {
            QueryPerformanceCounter((LARGE_INTEGER *) &timeval);
            my_rseed = (int) timeval;
        }
#elif defined(LINUX) || defined(MACOS) || defined(FREEBSD)
        FILE *fp;
        if ((fp = fopen("/dev/urandom", "r")) == NULL) {
            my_rseed = 1234567;
        }
        else {
            fread(&my_rseed, sizeof(int), 1, fp);
            fclose(fp);
        }
#else
        struct timeval time;
        struct timezone tz;
        gettimeofday(&time, &tz);
#ifndef QNX
        my_rseed = (int) ((time.tv_sec*time.tv_sec) + time.tv_usec);
#else
        my_rseed = (int) ((time.tv_sec*time.tv_sec) + time.tv_usec);
#endif
#endif
    }

    /* Seed all the generators */
#if defined(LINUX) || defined(MACOS) || defined(FREEBSD)
    srand48(my_rseed);
#endif
    srand(my_rseed);
    
    return my_rseed;
}

/*
 * ranget() - return current seed value
 */
int ranget(void)
{
    return my_rseed;
}

int ran(int n)
{
#if defined (OS2) || defined(WIN32)
  return((int) (ran4((int *)&my_rseed) * n));
#else
  if (!n) return 0;
  return(lrand48() % n);
#endif
}

/***********************************************************/
/* ran_range(int lo, int hi)                               */
/*   returns a uniformly distributed int between lo and hi */
/***********************************************************/
int ran_range (int lo, int hi)
{
  int r;

  if (lo >= hi) return (lo);
  r = hi - lo + 1;	/* no. of nos. in range hi thru lo */
  
  return(lo+ran(r));
}

/***********************************************************/
/* frand()                                                 */
/*   returns a uniformly distributed float between 0 and 1 */
/***********************************************************/
float
frand()
{
#if defined (OS2) || defined (WIN32)
  return(ran4((int *) &my_rseed));
#else
  return((float) drand48());
#endif
}


/***********************************************************/
/* zrand()                                                 */
/*   return a pseudo normally distributed random number    */
/***********************************************************/
float 
zrand()
{
  return(gasdev(&my_rseed));
}

/***********************************************************/
/* rand_fill()                                             */
/*   fill up array buff with ints between 0 and n in       */
/*   random order                                          */
/***********************************************************/
void rand_fill(int n, int buff[])
{
  int i,pos;	
  for (i = 0; i < n; i++) buff[i] = -1;
  
  for (i = 0; i < n; i++) {
    while (buff[(pos = ran(n))] != -1);
    buff[pos] = i;
  }
}


/*************************************************************************
 *            Floyd's Algorithm for Choosing Random Numbers
 *
 * void choose_rands(int m, int n, int buff[]);
 *
 *************************************************************************/

typedef struct _llelt {
  int val;
  struct _llelt *prev;
  struct _llelt *next;
} LLELT;

typedef struct _llist {
  int nused;
  int nalloced;
  LLELT *lelts;
  LLELT *first;
} LLIST;

static LLIST *llist_create(int n);
static LLELT *llist_find(LLIST *, int val);
static void   llist_destroy(LLIST *);
static void   llist_insertBefore(LLIST *, LLELT *lelt, int val);
static void   llist_insertAfter(LLIST *, LLELT *lelt, int val);

void rand_choose(int m, int n, int buff[])
{
  LLIST *list;
  LLELT *lelt;

  int i, j, t;

  list = llist_create(n);

  for (j = m-n+1; j <= m; j++) {
    t = ran(j);
    for (lelt = list->first; lelt != NULL; lelt = lelt->next) {
      if (lelt->val == t) break;
    }
    if (!lelt) llist_insertBefore(list, list->first, t);
    else llist_insertAfter(list, lelt, j-1);
  }

  /* Copy the linked list into the buffer */
  for (i = 0, lelt = list->first; lelt != NULL; lelt = lelt->next) {
    buff[i++] = lelt->val;
  }

  llist_destroy(list);
}


static LLIST *llist_create(int n)
{
  LLIST *list = (LLIST *) calloc(1, sizeof(LLIST));
  LLELT *elts = (LLELT *) calloc(n, sizeof(LLELT));

  list->nalloced = n;
  list->nused = 0;
  list->lelts = elts;
  list->first = NULL;
  return list;
}

static void llist_destroy(LLIST *list)
{
  if (!list) return;
  if (list->lelts) free((void *) list->lelts);
  free((void *) list);
}

static LLELT *llist_find(LLIST *list, int val) 
{
  LLELT *lelt;

  for (lelt = list->first; lelt != NULL; lelt = lelt->next) {
    if (val == lelt->val) return lelt;
  }

  return NULL;
}

static void llist_insertBefore(LLIST *list, LLELT *elt, int val)
{
  LLELT *newelt = &(list->lelts[(list->nused)++]);

  newelt->val = val;
  newelt->next = elt;

  if (elt) {
    newelt->prev = elt->prev;
    elt->prev = newelt;
  }
  else {
    list->first = newelt;
    newelt->prev = NULL;
  }

  if (!newelt->prev) list->first = newelt;
}

static void llist_insertAfter(LLIST *list, LLELT *elt, int val)
{
  LLELT *newelt = &(list->lelts[(list->nused)++]);

  newelt->val = val;
  newelt->prev = elt;

  if (elt) {
    newelt->next = elt->next;
    elt->next = newelt;
  }
  else {
    list->first = newelt;
    newelt->next = NULL;
  }

  if (!newelt->prev) list->first = newelt;
}

/***********************************************************************/
/*                    Imported from Numerical Recipes                  */
/***********************************************************************/

#define IA 16807
#define IM 2147483647
#define AM (1.0/IM)
#define IQ 127773
#define IR 2836
#define NTAB 32
#define NDIV (1+(IM-1)/NTAB)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

static float ran1(int *idum)
{
	int j;
	int k;
	static int iy=0;
	static int iv[NTAB];
	float temp;

	if (*idum <= 0 || !iy) {
		if (-(*idum) < 1) *idum=1;
		else *idum = -(*idum);
		for (j=NTAB+7;j>=0;j--) {
			k=(*idum)/IQ;
			*idum=IA*(*idum-k*IQ)-IR*k;
			if (*idum < 0) *idum += IM;
			if (j < NTAB) iv[j] = *idum;
		}
		iy=iv[0];
	}
	k=(*idum)/IQ;
	*idum=IA*(*idum-k*IQ)-IR*k;
	if (*idum < 0) *idum += IM;
	j=iy/NDIV;
	iy=iv[j];
	iv[j] = *idum;
	if ((temp=AM*iy) > RNMX) return RNMX;
	else return temp;
}
#undef IA
#undef IM
#undef AM
#undef IQ
#undef IR
#undef NTAB
#undef NDIV
#undef EPS
#undef RNMX

static float ran4(int *idum)
{
	void psdes(unsigned int *lword, unsigned int *irword);
	unsigned int irword,itemp,lword;
	static int idums = 0;

	static unsigned int jflone = 0x3f800000;
	static unsigned int jflmsk = 0x007fffff;

	if (*idum < 0) {
		idums = -(*idum);
		*idum=1;
	}
	irword=(*idum);
	lword=idums;
	psdes(&lword,&irword);
	itemp=jflone | (jflmsk & irword);
	++(*idum);
	return (*(float *)&itemp)-1.0;
}


static float gasdev(int *idum)
{
  float ran1(int *idum);
  static int iset=0;
  static float gset;
  float fac,rsq,v1,v2;
  
  if  (iset == 0) {
    do {
      v1=2.0*ran1(idum)-1.0;
      v2=2.0*ran1(idum)-1.0;
      rsq=v1*v1+v2*v2;
    } while (rsq >= 1.0 || rsq == 0.0);
    fac=sqrt(-2.0*log(rsq)/rsq);
    gset=v1*fac;
    iset=1;
    return v2*fac;
  } else {
    iset=0;
    return gset;
  }
}

#define NITER 4

static void psdes(unsigned int *lword, unsigned int *irword)
{
	unsigned int i,ia,ib,iswap,itmph=0,itmpl=0;
	static unsigned int c1[NITER]={
		0xbaa96887L, 0x1e17d32cL, 0x03bcdc3cL, 0x0f33d1b2L};
	static unsigned int c2[NITER]={
		0x4b0f3b58L, 0xe874f0c3L, 0x6955c5a6L, 0x55a7ca46L};

	for (i=0;i<NITER;i++) {
		ia=(iswap=(*irword)) ^ c1[i];
		itmpl = ia & 0xffff;
		itmph = ia >> 16;
		ib=itmpl*itmpl+ ~(itmph*itmph);
		*irword=(*lword) ^ (((ia = (ib >> 16) |
			((ib & 0xffff) << 16)) ^ c2[i])+itmpl*itmph);
		*lword=iswap;
	}
}
#undef NITER
