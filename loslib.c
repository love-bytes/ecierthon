/*
** $Id: loslib.c $
** Standard Operating System library
** See Copyright Notice in ecierthon.h
*/

#define loslib_c
#define ecierthon_LIB

#include "lprefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ecierthon.h"

#include "lauxlib.h"
#include "ecierthonlib.h"


/*
** {==================================================================
** List of valid conversion specifiers for the 'strftime' function;
** options are grouped by length; group of length 2 start with '||'.
** ===================================================================
*/
#if !defined(ecierthon_STRFTIMEOPTIONS)	/* { */

/* options for ANSI C 89 (only 1-char options) */
#define L_STRFTIMEC89		"aAbBcdHIjmMpSUwWxXyYZ%"

/* options for ISO C 99 and POSIX */
#define L_STRFTIMEC99 "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"  /* two-char options */

/* options for Windows */
#define L_STRFTIMEWIN "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"  /* two-char options */

#if defined(ecierthon_USE_WINDOWS)
#define ecierthon_STRFTIMEOPTIONS	L_STRFTIMEWIN
#elif defined(ecierthon_USE_C89)
#define ecierthon_STRFTIMEOPTIONS	L_STRFTIMEC89
#else  /* C99 specification */
#define ecierthon_STRFTIMEOPTIONS	L_STRFTIMEC99
#endif

#endif					/* } */
/* }================================================================== */


/*
** {==================================================================
** Configuration for time-related stuff
** ===================================================================
*/

/*
** type to represent time_t in ecierthon
*/
#if !defined(ecierthon_NUMTIME)	/* { */

#define l_timet			ecierthon_Integer
#define l_pushtime(L,t)		ecierthon_pushinteger(L,(ecierthon_Integer)(t))
#define l_gettime(L,arg)	ecierthonL_checkinteger(L, arg)

#else				/* }{ */

#define l_timet			ecierthon_Number
#define l_pushtime(L,t)		ecierthon_pushnumber(L,(ecierthon_Number)(t))
#define l_gettime(L,arg)	ecierthonL_checknumber(L, arg)

#endif				/* } */


#if !defined(l_gmtime)		/* { */
/*
** By default, ecierthon uses gmtime/localtime, except when POSIX is available,
** where it uses gmtime_r/localtime_r
*/

#if defined(ecierthon_USE_POSIX)	/* { */

#define l_gmtime(t,r)		gmtime_r(t,r)
#define l_localtime(t,r)	localtime_r(t,r)

#else				/* }{ */

/* ISO C definitions */
#define l_gmtime(t,r)		((void)(r)->tm_sec, gmtime(t))
#define l_localtime(t,r)	((void)(r)->tm_sec, localtime(t))

#endif				/* } */

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Configuration for 'tmpnam':
** By default, ecierthon uses tmpnam except when POSIX is available, where
** it uses mkstemp.
** ===================================================================
*/
#if !defined(ecierthon_tmpnam)	/* { */

#if defined(ecierthon_USE_POSIX)	/* { */

#include <unistd.h>

#define ecierthon_TMPNAMBUFSIZE	32

#if !defined(ecierthon_TMPNAMTEMPLATE)
#define ecierthon_TMPNAMTEMPLATE	"/tmp/ecierthon_XXXXXX"
#endif

#define ecierthon_tmpnam(b,e) { \
        strcpy(b, ecierthon_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else				/* }{ */

/* ISO C definitions */
#define ecierthon_TMPNAMBUFSIZE	L_tmpnam
#define ecierthon_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif				/* } */

#endif				/* } */
/* }================================================================== */



static int os_execute (ecierthon_State *L) {
  const char *cmd = ecierthonL_optstring(L, 1, NULL);
  int stat;
  errno = 0;
  stat = system(cmd);
  if (cmd != NULL)
    return ecierthonL_execresult(L, stat);
  else {
    ecierthon_pushboolean(L, stat);  /* true if there is a shell */
    return 1;
  }
}


static int os_remove (ecierthon_State *L) {
  const char *filename = ecierthonL_checkstring(L, 1);
  return ecierthonL_fileresult(L, remove(filename) == 0, filename);
}


static int os_rename (ecierthon_State *L) {
  const char *fromname = ecierthonL_checkstring(L, 1);
  const char *toname = ecierthonL_checkstring(L, 2);
  return ecierthonL_fileresult(L, rename(fromname, toname) == 0, NULL);
}


static int os_tmpname (ecierthon_State *L) {
  char buff[ecierthon_TMPNAMBUFSIZE];
  int err;
  ecierthon_tmpnam(buff, err);
  if (err)
    return ecierthonL_error(L, "unable to generate a unique filename");
  ecierthon_pushstring(L, buff);
  return 1;
}


static int os_getenv (ecierthon_State *L) {
  ecierthon_pushstring(L, getenv(ecierthonL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}


static int os_clock (ecierthon_State *L) {
  ecierthon_pushnumber(L, ((ecierthon_Number)clock())/(ecierthon_Number)CLOCKS_PER_SEC);
  return 1;
}


/*
** {======================================================
** Time/Date operations
** { year=%Y, month=%m, day=%d, hour=%H, min=%M, sec=%S,
**   wday=%w+1, yday=%j, isdst=? }
** =======================================================
*/

/*
** About the overflow check: an overflow cannot occur when time
** is represented by a ecierthon_Integer, because either ecierthon_Integer is
** large enough to represent all int fields or it is not large enough
** to represent a time that cause a field to overflow.  However, if
** times are represented as doubles and ecierthon_Integer is int, then the
** time 0x1.e1853b0d184f6p+55 would cause an overflow when adding 1900
** to compute the year.
*/
static void setfield (ecierthon_State *L, const char *key, int value, int delta) {
  #if (defined(ecierthon_NUMTIME) && ecierthon_MAXINTEGER <= INT_MAX)
    if (value > ecierthon_MAXINTEGER - delta)
      ecierthonL_error(L, "field '%s' is out-of-bound", key);
  #endif
  ecierthon_pushinteger(L, (ecierthon_Integer)value + delta);
  ecierthon_setfield(L, -2, key);
}


static void setboolfield (ecierthon_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  ecierthon_pushboolean(L, value);
  ecierthon_setfield(L, -2, key);
}


/*
** Set all fields from structure 'tm' in the table on top of the stack
*/
static void setallfields (ecierthon_State *L, struct tm *stm) {
  setfield(L, "year", stm->tm_year, 1900);
  setfield(L, "month", stm->tm_mon, 1);
  setfield(L, "day", stm->tm_mday, 0);
  setfield(L, "hour", stm->tm_hour, 0);
  setfield(L, "min", stm->tm_min, 0);
  setfield(L, "sec", stm->tm_sec, 0);
  setfield(L, "yday", stm->tm_yday, 1);
  setfield(L, "wday", stm->tm_wday, 1);
  setboolfield(L, "isdst", stm->tm_isdst);
}


static int getboolfield (ecierthon_State *L, const char *key) {
  int res;
  res = (ecierthon_getfield(L, -1, key) == ecierthon_TNIL) ? -1 : ecierthon_toboolean(L, -1);
  ecierthon_pop(L, 1);
  return res;
}


static int getfield (ecierthon_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = ecierthon_getfield(L, -1, key);  /* get field and its type */
  ecierthon_Integer res = ecierthon_tointegerx(L, -1, &isnum);
  if (!isnum) {  /* field is not an integer? */
    if (t != ecierthon_TNIL)  /* some other value? */
      return ecierthonL_error(L, "field '%s' is not an integer", key);
    else if (d < 0)  /* absent field; no default? */
      return ecierthonL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    /* unsigned avoids overflow when ecierthon_Integer has 32 bits */
    if (!(res >= 0 ? (ecierthon_Unsigned)res <= (ecierthon_Unsigned)INT_MAX + delta
                   : (ecierthon_Integer)INT_MIN + delta <= res))
      return ecierthonL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  ecierthon_pop(L, 1);
  return (int)res;
}


static const char *checkoption (ecierthon_State *L, const char *conv,
                                ptrdiff_t convlen, char *buff) {
  const char *option = ecierthon_STRFTIMEOPTIONS;
  int oplen = 1;  /* length of options being checked */
  for (; *option != '\0' && oplen <= convlen; option += oplen) {
    if (*option == '|')  /* next block? */
      oplen++;  /* will check options with next length (+1) */
    else if (memcmp(conv, option, oplen) == 0) {  /* match? */
      memcpy(buff, conv, oplen);  /* copy valid option to buffer */
      buff[oplen] = '\0';
      return conv + oplen;  /* return next item */
    }
  }
  ecierthonL_argerror(L, 1,
    ecierthon_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* to avoid warnings */
}


static time_t l_checktime (ecierthon_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  ecierthonL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}


/* maximum size for an individual 'strftime' item */
#define SIZETIMEFMT	250


static int os_date (ecierthon_State *L) {
  size_t slen;
  const char *s = ecierthonL_optlstring(L, 1, "%c", &slen);
  time_t t = ecierthonL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;  /* 's' end */
  struct tm tmr, *stm;
  if (*s == '!') {  /* UTC? */
    stm = l_gmtime(&t, &tmr);
    s++;  /* skip '!' */
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)  /* invalid date? */
    return ecierthonL_error(L,
                 "date result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    ecierthon_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
  }
  else {
    char cc[4];  /* buffer for individual conversion specifiers */
    ecierthonL_Buffer b;
    cc[0] = '%';
    ecierthonL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')  /* not a conversion specifier? */
        ecierthonL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = ecierthonL_prepbuffsize(&b, SIZETIMEFMT);
        s++;  /* skip '%' */
        s = checkoption(L, s, se - s, cc + 1);  /* copy specifier to 'cc' */
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        ecierthonL_addsize(&b, reslen);
      }
    }
    ecierthonL_pushresult(&b);
  }
  return 1;
}


static int os_time (ecierthon_State *L) {
  time_t t;
  if (ecierthon_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    ecierthonL_checktype(L, 1, ecierthon_TTABLE);
    ecierthon_settop(L, 1);  /* make sure table is at the top */
    ts.tm_year = getfield(L, "year", -1, 1900);
    ts.tm_mon = getfield(L, "month", -1, 1);
    ts.tm_mday = getfield(L, "day", -1, 0);
    ts.tm_hour = getfield(L, "hour", 12, 0);
    ts.tm_min = getfield(L, "min", 0, 0);
    ts.tm_sec = getfield(L, "sec", 0, 0);
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
    setallfields(L, &ts);  /* update fields with normalized values */
  }
  if (t != (time_t)(l_timet)t || t == (time_t)(-1))
    return ecierthonL_error(L,
                  "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}


static int os_difftime (ecierthon_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  ecierthon_pushnumber(L, (ecierthon_Number)difftime(t1, t2));
  return 1;
}

/* }====================================================== */


static int os_setlocale (ecierthon_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = ecierthonL_optstring(L, 1, NULL);
  int op = ecierthonL_checkoption(L, 2, "all", catnames);
  ecierthon_pushstring(L, setlocale(cat[op], l));
  return 1;
}


static int os_exit (ecierthon_State *L) {
  int status;
  if (ecierthon_isboolean(L, 1))
    status = (ecierthon_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)ecierthonL_optinteger(L, 1, EXIT_SUCCESS);
  if (ecierthon_toboolean(L, 2))
    ecierthon_close(L);
  if (L) exit(status);  /* 'if' to avoid warnings for unreachable 'return' */
  return 0;
}


static const ecierthonL_Reg syslib[] = {
  {"clock",     os_clock},
  {"date",      os_date},
  {"difftime",  os_difftime},
  {"execute",   os_execute},
  {"exit",      os_exit},
  {"getenv",    os_getenv},
  {"remove",    os_remove},
  {"rename",    os_rename},
  {"setlocale", os_setlocale},
  {"time",      os_time},
  {"tmpname",   os_tmpname},
  {NULL, NULL}
};

/* }====================================================== */



ecierthonMOD_API int ecierthonopen_os (ecierthon_State *L) {
  ecierthonL_newlib(L, syslib);
  return 1;
}

