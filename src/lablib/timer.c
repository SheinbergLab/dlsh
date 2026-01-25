/********************************************************************
  NAME
    timer.c - timer utilities for DOS and UNIX

  AUTHOR
    DLS, APR-1994
********************************************************************/

#include <math.h>

/* Using IMSL datedays.c code */

static int lv_ndate[12] = 
    {-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333};
static int lv_ndate2[12] = 
    {366, 335, 307, 276, 246, 215, 185, 154, 123, 93, 62, 32};
static int lv_days_per_month[] =
    {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* check date: return 0 if not legal, 1 o.w. */
static int check_date(int day, int month, int year, int *leap)
{
  int            ileap1;

  /* Check input arguments */
  if (month < 1 || month > 12) return 0;
  if (day < 1) return 0;
  if (year == 0) return 0;
  if (year == 1582 && month == 10) {
    if (day > 4 && day < 15) {
      /* Year = 1582, month = 10 and day = %(i1) is
	 illegal because, due to the conversion from
	 the Julian calendar to the Gregorian
	 calendar, October 5, 1582 through
	 October 14, 1582 do not exist. */
      return 0;
    }
  }
  if (year < -45) {
    /* The Julian Calendar first went into use in 45
       B.C. (-45).  No calendar prior to that time
       was as universally used nor as accurate as
       the Julian.  Therefore, it is assumed that
       the Julian Calendar was in use prior to
       45 B.C. and year = %(I1). */
    return 0;
  }
  /*
   * Determine if IYEAR is a *leap year or not
   */
  ileap1 = year % 4;
  if (ileap1 == 0) {
    if (year > 1582) {
      ileap1 = year % 100;
      if (ileap1 != 0) {
	*leap = 1;
      } else {
	ileap1 = year % 400;
	*leap = (ileap1 == 0) ? 1 : -1;
      }
    } else {
      *leap = 1;
    }
  } else {
    *leap = -1;
  }
  /*
   * Check to make sure IDAY is not larger than the number of days in a
   * given month
   */
  lv_days_per_month[2] = (*leap < 0) ? 28 : 29;
  if (day > lv_days_per_month[month]) return 0;
  return 1;
}


static int date_to_days(int day, int month, int year)
{
  int    i100, i4, i400, iadiff, idiff, ileap, itemp, ndays_v, nleap;

  if (!check_date(day, month, year, &ileap)) return 0;

  /*
   * Determine the number of days between input date and January 1, 1900
   */
  idiff = year - 1900;
  if (idiff >= 0) {
    /* Input date after January 1, 1900 */
    i4 = idiff / 4;
    i100 = idiff / 100;
    i400 = 0;
    /*
     * Make adjustment since 2000 and subsequent 400 years are
     * leap years
     */
    if (idiff >= 100)
      i400 = 1 + (idiff - 100) / 400;
    nleap = i4 - (i100 - i400);
    itemp = (idiff - nleap) * 365 + nleap * 366;
    ndays_v = itemp + lv_ndate[month - 1] + day;
    if (ileap > 0 && month <= 2)
      ndays_v -= 1;
  } else {
    /* Input date before January 1, 1900 */
    iadiff = idiff < 0 ? -idiff : idiff;
    i4 = iadiff / 4;
    /* 1800 and 1700 are not leap year */
    if (iadiff < 200) {
      i100 = iadiff / 100;
    } else {
      i100 = 2;
    }
    /*
     * Make adjustment since 1600 is a leap year
     */
    nleap = i4 - i100;
    itemp = (iadiff - nleap - 1) * 365 + nleap * 366;
    ndays_v = itemp + lv_ndate2[month - 1] - day;
    /*
     * Make adjustment for change in calendar system
     */
    if (year == 1582) {
      if ((month == 10 && day <= 4) || month <= 9) {
	ndays_v -= 10;
      }
    } else if (year < 1582) {
      ndays_v -= 10;
    }
    /*
     * Make adjustment since year 0 does not exist
     */
    if (year < 0)
      ndays_v -= 366;
    if (ileap > 0 && month > 2)
      ndays_v -= 1;
    ndays_v = -ndays_v;
  }
  return (ndays_v);
}


/*
 * QNX Versions - use clock() and nanosleep()
 */

#if defined (__WATCOMC__)
#include <time.h>
#include <i86.h>

unsigned int trGetTime()
{
   clock_t time = clock();

   return(1000*(time/CLOCKS_PER_SEC));
}

void trSleep(int sleep_ms)
{
   delay(sleep_ms);
}

#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <time.h>

unsigned int trGetTime()
{
   clock_t time = clock();

   return(1000*(time/CLOCKS_PER_SEC));
}

void trSleep(int sleep_ms)
{
  Sleep(sleep_ms);
}


int trDateToDays(int day, int month, int year)
{
  return (date_to_days(day, month, year));
}

void trDaysToDate(int days, int *day, int *month, int *year)
{
  struct tm *timeinfo;
  time_t rawtime, time0, time1900;
  int days_from_1900_to_1970 = 25566; /* 2208988800 secs  */

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  timeinfo->tm_year = 0;		/* 1970? */
  timeinfo->tm_mon = 0;			/* January */
  timeinfo->tm_mday = 1;		/* 1st */
  time1900 = mktime(timeinfo);

  /* now add number of seconds corresponding to days */
  time0 = time1900+((days-days_from_1900_to_1970)*24*60*60);

  timeinfo = gmtime(&time0);

  *year = 1900+timeinfo->tm_year;
  *month = timeinfo->tm_mon+1;
  *day = timeinfo->tm_mday;
  return;
}


/*
 * Lynx Versions - use getclock and nanosleep
 */

#elif defined(LYNX)

#include <sys/timers.h>

unsigned int trGetTime()
{
   struct timespec time;
   getclock(TIMEOFDAY, &time);
   return((time.tv_sec*1000000)+(time.tv_nsec/1000));
}

void trSleep(int sleep_ms)
{
   struct timespec rqt, rmt;
   rqt.tv_nsec = sleep_ms*1000;
   nanosleep(&rqt, &rmt);
}

#elif defined(SUN4) || defined(SGI) || defined(LINUX) || defined(FREEBSD)
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>

#if defined(LINUX)
#include <time.h>
#endif

unsigned int trGetTime()
{
  struct timeval t;
  gettimeofday(&t,NULL);
  return(t.tv_sec*1000 + t.tv_usec/1000);
}

void trSleep(int n)
{
  struct timeval t;
  long s, us;
  long elapsed;

  if (!n) return;

  gettimeofday(&t,NULL);
  s = t.tv_sec;
  us = t.tv_usec;

  for (elapsed=0 ; elapsed<n;) {
    gettimeofday(&t,NULL);
    elapsed = (t.tv_sec - s)*1000 + (t.tv_usec - us)/1000;
  }
}

int trDateToDays(int day, int month, int year)
{
  return (date_to_days(day, month, year));
}

void trDaysToDate(int days, int *day, int *month, int *year)
{
  struct tm *timeinfo, outtime;
  time_t rawtime, time0, time1900;
  double secs;
  int days_from_1900_to_1970 = 25566; /* 2208988800 secs  */

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  timeinfo->tm_year = 0;		/* 1970? */
  timeinfo->tm_mon = 0;			/* January */
  timeinfo->tm_mday = 0;		/* 1st */
  time1900 = mktime(timeinfo);

  /* now add number of seconds corresponding to days */
  time0 = time1900+((days-days_from_1900_to_1970)*24*60*60);

  gmtime_r(&time0, &outtime);

  *year = 1970+outtime.tm_year;
  *month = outtime.tm_mon+1;
  *day = outtime.tm_mday;
  return;
}

#endif
