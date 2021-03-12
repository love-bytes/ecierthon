/*
** $Id: lauxlib.c $
** Auxiliary functions for building ecierthon libraries
** See Copyright Notice in ecierthon.h
*/

#define lauxlib_c
#define ecierthon_LIB

#include "lprefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of ecierthon.
** Any function declared here could be written as an application function.
*/

#include "ecierthon.h"

#include "lauxlib.h"


#if !defined(MAX_SIZET)
/* maximum value for size_t */
#define MAX_SIZET	((size_t)(~(size_t)0))
#endif


/*
** {======================================================
** Traceback
** =======================================================
*/


#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */



/*
** Search for 'objidx' in table at index -1. ('objidx' must be an
** absolute index.) Return 1 + string at top if it found a good name.
*/
static int findfield (ecierthon_State *L, int objidx, int level) {
  if (level == 0 || !ecierthon_istable(L, -1))
    return 0;  /* not found */
  ecierthon_pushnil(L);  /* start 'next' loop */
  while (ecierthon_next(L, -2)) {  /* for each pair in table */
    if (ecierthon_type(L, -2) == ecierthon_TSTRING) {  /* ignore non-string keys */
      if (ecierthon_rawequal(L, objidx, -1)) {  /* found object? */
        ecierthon_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        ecierthon_pushliteral(L, ".");  /* place '.' between the two names */
        ecierthon_replace(L, -3);  /* (in the slot occupied by table) */
        ecierthon_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    ecierthon_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (ecierthon_State *L, ecierthon_Debug *ar) {
  int top = ecierthon_gettop(L);
  ecierthon_getinfo(L, "f", ar);  /* push function */
  ecierthon_getfield(L, ecierthon_REGISTRYINDEX, ecierthon_LOADED_TABLE);
  if (findfield(L, top + 1, 2)) {
    const char *name = ecierthon_tostring(L, -1);
    if (strncmp(name, ecierthon_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      ecierthon_pushstring(L, name + 3);  /* push name without prefix */
      ecierthon_remove(L, -2);  /* remove original name */
    }
    ecierthon_copy(L, -1, top + 1);  /* copy name to proper place */
    ecierthon_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    ecierthon_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (ecierthon_State *L, ecierthon_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    ecierthon_pushfstring(L, "function '%s'", ecierthon_tostring(L, -1));
    ecierthon_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    ecierthon_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      ecierthon_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for ecierthon functions, use <file:line> */
    ecierthon_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    ecierthon_pushliteral(L, "?");
}


static int lastlevel (ecierthon_State *L) {
  ecierthon_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (ecierthon_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (ecierthon_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


ecierthonLIB_API void ecierthonL_traceback (ecierthon_State *L, ecierthon_State *L1,
                                const char *msg, int level) {
  ecierthonL_Buffer b;
  ecierthon_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  ecierthonL_buffinit(L, &b);
  if (msg) {
    ecierthonL_addstring(&b, msg);
    ecierthonL_addchar(&b, '\n');
  }
  ecierthonL_addstring(&b, "stack traceback:");
  while (ecierthon_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      ecierthon_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      ecierthonL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      ecierthon_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        ecierthon_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        ecierthon_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      ecierthonL_addvalue(&b);
      pushfuncname(L, &ar);
      ecierthonL_addvalue(&b);
      if (ar.istailcall)
        ecierthonL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  ecierthonL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

ecierthonLIB_API int ecierthonL_argerror (ecierthon_State *L, int arg, const char *extramsg) {
  ecierthon_Debug ar;
  if (!ecierthon_getstack(L, 0, &ar))  /* no stack frame? */
    return ecierthonL_error(L, "bad argument #%d (%s)", arg, extramsg);
  ecierthon_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    arg--;  /* do not count 'self' */
    if (arg == 0)  /* error is in the self argument itself? */
      return ecierthonL_error(L, "calling '%s' on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? ecierthon_tostring(L, -1) : "?";
  return ecierthonL_error(L, "bad argument #%d to '%s' (%s)",
                        arg, ar.name, extramsg);
}


int ecierthonL_typeerror (ecierthon_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (ecierthonL_getmetafield(L, arg, "__name") == ecierthon_TSTRING)
    typearg = ecierthon_tostring(L, -1);  /* use the given type name */
  else if (ecierthon_type(L, arg) == ecierthon_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = ecierthonL_typename(L, arg);  /* standard name */
  msg = ecierthon_pushfstring(L, "%s expected, got %s", tname, typearg);
  return ecierthonL_argerror(L, arg, msg);
}


static void tag_error (ecierthon_State *L, int arg, int tag) {
  ecierthonL_typeerror(L, arg, ecierthon_typename(L, tag));
}


/*
** The use of 'ecierthon_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
ecierthonLIB_API void ecierthonL_where (ecierthon_State *L, int level) {
  ecierthon_Debug ar;
  if (ecierthon_getstack(L, level, &ar)) {  /* check function at level */
    ecierthon_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      ecierthon_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  ecierthon_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'ecierthon_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
ecierthonLIB_API int ecierthonL_error (ecierthon_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  ecierthonL_where(L, 1);
  ecierthon_pushvfstring(L, fmt, argp);
  va_end(argp);
  ecierthon_concat(L, 2);
  return ecierthon_error(L);
}


ecierthonLIB_API int ecierthonL_fileresult (ecierthon_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to ecierthon API may change this value */
  if (stat) {
    ecierthon_pushboolean(L, 1);
    return 1;
  }
  else {
    ecierthonL_pushfail(L);
    if (fname)
      ecierthon_pushfstring(L, "%s: %s", fname, strerror(en));
    else
      ecierthon_pushstring(L, strerror(en));
    ecierthon_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(ecierthon_USE_POSIX)

#include <sys/wait.h>

/*
** use appropriate macros to interpret 'pclose' return status
*/
#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#else

#define l_inspectstat(stat,what)  /* no op */

#endif

#endif				/* } */


ecierthonLIB_API int ecierthonL_execresult (ecierthon_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return ecierthonL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      ecierthon_pushboolean(L, 1);
    else
      ecierthonL_pushfail(L);
    ecierthon_pushstring(L, what);
    ecierthon_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

ecierthonLIB_API int ecierthonL_newmetatable (ecierthon_State *L, const char *tname) {
  if (ecierthonL_getmetatable(L, tname) != ecierthon_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  ecierthon_pop(L, 1);
  ecierthon_createtable(L, 0, 2);  /* create metatable */
  ecierthon_pushstring(L, tname);
  ecierthon_setfield(L, -2, "__name");  /* metatable.__name = tname */
  ecierthon_pushvalue(L, -1);
  ecierthon_setfield(L, ecierthon_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


ecierthonLIB_API void ecierthonL_setmetatable (ecierthon_State *L, const char *tname) {
  ecierthonL_getmetatable(L, tname);
  ecierthon_setmetatable(L, -2);
}


ecierthonLIB_API void *ecierthonL_testudata (ecierthon_State *L, int ud, const char *tname) {
  void *p = ecierthon_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (ecierthon_getmetatable(L, ud)) {  /* does it have a metatable? */
      ecierthonL_getmetatable(L, tname);  /* get correct metatable */
      if (!ecierthon_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      ecierthon_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


ecierthonLIB_API void *ecierthonL_checkudata (ecierthon_State *L, int ud, const char *tname) {
  void *p = ecierthonL_testudata(L, ud, tname);
  ecierthonL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

ecierthonLIB_API int ecierthonL_checkoption (ecierthon_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? ecierthonL_optstring(L, arg, def) :
                             ecierthonL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return ecierthonL_argerror(L, arg,
                       ecierthon_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, ecierthon will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
ecierthonLIB_API void ecierthonL_checkstack (ecierthon_State *L, int space, const char *msg) {
  if (!ecierthon_checkstack(L, space)) {
    if (msg)
      ecierthonL_error(L, "stack overflow (%s)", msg);
    else
      ecierthonL_error(L, "stack overflow");
  }
}


ecierthonLIB_API void ecierthonL_checktype (ecierthon_State *L, int arg, int t) {
  if (ecierthon_type(L, arg) != t)
    tag_error(L, arg, t);
}


ecierthonLIB_API void ecierthonL_checkany (ecierthon_State *L, int arg) {
  if (ecierthon_type(L, arg) == ecierthon_TNONE)
    ecierthonL_argerror(L, arg, "value expected");
}


ecierthonLIB_API const char *ecierthonL_checklstring (ecierthon_State *L, int arg, size_t *len) {
  const char *s = ecierthon_tolstring(L, arg, len);
  if (!s) tag_error(L, arg, ecierthon_TSTRING);
  return s;
}


ecierthonLIB_API const char *ecierthonL_optlstring (ecierthon_State *L, int arg,
                                        const char *def, size_t *len) {
  if (ecierthon_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return ecierthonL_checklstring(L, arg, len);
}


ecierthonLIB_API ecierthon_Number ecierthonL_checknumber (ecierthon_State *L, int arg) {
  int isnum;
  ecierthon_Number d = ecierthon_tonumberx(L, arg, &isnum);
  if (!isnum)
    tag_error(L, arg, ecierthon_TNUMBER);
  return d;
}


ecierthonLIB_API ecierthon_Number ecierthonL_optnumber (ecierthon_State *L, int arg, ecierthon_Number def) {
  return ecierthonL_opt(L, ecierthonL_checknumber, arg, def);
}


static void interror (ecierthon_State *L, int arg) {
  if (ecierthon_isnumber(L, arg))
    ecierthonL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, ecierthon_TNUMBER);
}


ecierthonLIB_API ecierthon_Integer ecierthonL_checkinteger (ecierthon_State *L, int arg) {
  int isnum;
  ecierthon_Integer d = ecierthon_tointegerx(L, arg, &isnum);
  if (!isnum) {
    interror(L, arg);
  }
  return d;
}


ecierthonLIB_API ecierthon_Integer ecierthonL_optinteger (ecierthon_State *L, int arg,
                                                      ecierthon_Integer def) {
  return ecierthonL_opt(L, ecierthonL_checkinteger, arg, def);
}

/* }====================================================== */


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/* userdata to box arbitrary data */
typedef struct UBox {
  void *box;
  size_t bsize;
} UBox;


static void *resizebox (ecierthon_State *L, int idx, size_t newsize) {
  void *ud;
  ecierthon_Alloc allocf = ecierthon_getallocf(L, &ud);
  UBox *box = (UBox *)ecierthon_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (temp == NULL && newsize > 0) {  /* allocation error? */
    ecierthon_pushliteral(L, "not enough memory");
    ecierthon_error(L);  /* raise a memory error */
  }
  box->box = temp;
  box->bsize = newsize;
  return temp;
}


static int boxgc (ecierthon_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const ecierthonL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (ecierthon_State *L) {
  UBox *box = (UBox *)ecierthon_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (ecierthonL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    ecierthonL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  ecierthon_setmetatable(L, -2);
}


/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->init.b)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes.
*/
static size_t newbuffsize (ecierthonL_Buffer *B, size_t sz) {
  size_t newsize = B->size * 2;  /* double buffer size */
  if (MAX_SIZET - sz < B->n)  /* overflow in (B->n + sz)? */
    return ecierthonL_error(B->L, "buffer too large");
  if (newsize < B->n + sz)  /* double is not big enough? */
    newsize = B->n + sz;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where the
** buffer's box is or should be.
*/
static char *prepbuffsize (ecierthonL_Buffer *B, size_t sz, int boxidx) {
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    ecierthon_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      ecierthon_pushnil(L);  /* reserve slot for final result */
      newbox(L);  /* create a new box */
      /* move box (and slot) to its intended position */
      ecierthon_rotate(L, boxidx - 1, 2);
      ecierthon_toclose(L, boxidx);
      newbuff = (char *)resizebox(L, boxidx, newsize);
      memcpy(newbuff, B->b, B->n * sizeof(char));  /* copy original content */
    }
    B->b = newbuff;
    B->size = newsize;
    return newbuff + B->n;
  }
}

/*
** returns a pointer to a free area with at least 'sz' bytes
*/
ecierthonLIB_API char *ecierthonL_prepbuffsize (ecierthonL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


ecierthonLIB_API void ecierthonL_addlstring (ecierthonL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    ecierthonL_addsize(B, l);
  }
}


ecierthonLIB_API void ecierthonL_addstring (ecierthonL_Buffer *B, const char *s) {
  ecierthonL_addlstring(B, s, strlen(s));
}


ecierthonLIB_API void ecierthonL_pushresult (ecierthonL_Buffer *B) {
  ecierthon_State *L = B->L;
  ecierthon_pushlstring(L, B->b, B->n);
  if (buffonstack(B)) {
    ecierthon_copy(L, -1, -3);  /* move string to reserved slot */
    ecierthon_pop(L, 2);  /* pop string and box (closing the box) */
  }
}


ecierthonLIB_API void ecierthonL_pushresultsize (ecierthonL_Buffer *B, size_t sz) {
  ecierthonL_addsize(B, sz);
  ecierthonL_pushresult(B);
}


/*
** 'ecierthonL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'ecierthonL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) bellow the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
ecierthonLIB_API void ecierthonL_addvalue (ecierthonL_Buffer *B) {
  ecierthon_State *L = B->L;
  size_t len;
  const char *s = ecierthon_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  ecierthonL_addsize(B, len);
  ecierthon_pop(L, 1);  /* pop string */
}


ecierthonLIB_API void ecierthonL_buffinit (ecierthon_State *L, ecierthonL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = ecierthonL_BUFFERSIZE;
}


ecierthonLIB_API char *ecierthonL_buffinitsize (ecierthon_State *L, ecierthonL_Buffer *B, size_t sz) {
  ecierthonL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header */
#define freelist	0


ecierthonLIB_API int ecierthonL_ref (ecierthon_State *L, int t) {
  int ref;
  if (ecierthon_isnil(L, -1)) {
    ecierthon_pop(L, 1);  /* remove from stack */
    return ecierthon_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = ecierthon_absindex(L, t);
  ecierthon_rawgeti(L, t, freelist);  /* get first free element */
  ref = (int)ecierthon_tointeger(L, -1);  /* ref = t[freelist] */
  ecierthon_pop(L, 1);  /* remove it from stack */
  if (ref != 0) {  /* any free element? */
    ecierthon_rawgeti(L, t, ref);  /* remove it from list */
    ecierthon_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)ecierthon_rawlen(L, t) + 1;  /* get a new reference */
  ecierthon_rawseti(L, t, ref);
  return ref;
}


ecierthonLIB_API void ecierthonL_unref (ecierthon_State *L, int t, int ref) {
  if (ref >= 0) {
    t = ecierthon_absindex(L, t);
    ecierthon_rawgeti(L, t, freelist);
    ecierthon_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
    ecierthon_pushinteger(L, ref);
    ecierthon_rawseti(L, t, freelist);  /* t[freelist] = ref */
  }
}

/* }====================================================== */


/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
  int n;  /* number of pre-read characters */
  FILE *f;  /* file being read */
  char buff[BUFSIZ];  /* area for reading file */
} LoadF;


static const char *getF (ecierthon_State *L, void *ud, size_t *size) {
  LoadF *lf = (LoadF *)ud;
  (void)L;  /* not used */
  if (lf->n > 0) {  /* are there pre-read characters to be read? */
    *size = lf->n;  /* return them (chars already in buffer) */
    lf->n = 0;  /* no more pre-read characters */
  }
  else {  /* read a block from file */
    /* 'fread' can return > 0 *and* set the EOF flag. If next call to
       'getF' called 'fread', it might still wait for user input.
       The next check avoids this problem. */
    if (feof(lf->f)) return NULL;
    *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);  /* read block */
  }
  return lf->buff;
}


static int errfile (ecierthon_State *L, const char *what, int fnameindex) {
  const char *serr = strerror(errno);
  const char *filename = ecierthon_tostring(L, fnameindex) + 1;
  ecierthon_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  ecierthon_remove(L, fnameindex);
  return ecierthon_ERRFILE;
}


static int skipBOM (LoadF *lf) {
  const char *p = "\xEF\xBB\xBF";  /* UTF-8 BOM mark */
  int c;
  lf->n = 0;
  do {
    c = getc(lf->f);
    if (c == EOF || c != *(const unsigned char *)p++) return c;
    lf->buff[lf->n++] = c;  /* to be read by the parser */
  } while (*p != '\0');
  lf->n = 0;  /* prefix matched; discard it */
  return getc(lf->f);  /* return next character */
}


/*
** reads the first character of file 'f' and skips an optional BOM mark
** in its beginning plus its first line if it starts with '#'. Returns
** true if it skipped the first line.  In any case, '*cp' has the
** first "valid" character of the file (after the optional BOM and
** a first-line comment).
*/
static int skipcomment (LoadF *lf, int *cp) {
  int c = *cp = skipBOM(lf);
  if (c == '#') {  /* first line is a comment (Unix exec. file)? */
    do {  /* skip first line */
      c = getc(lf->f);
    } while (c != EOF && c != '\n');
    *cp = getc(lf->f);  /* skip end-of-line, if present */
    return 1;  /* there was a comment */
  }
  else return 0;  /* no comment */
}


ecierthonLIB_API int ecierthonL_loadfilex (ecierthon_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = ecierthon_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    ecierthon_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    ecierthon_pushfstring(L, "@%s", filename);
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  if (skipcomment(&lf, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add line to correct line numbers */
  if (c == LUA_SIGNATURE[0] && filename) {  /* binary file? */
    lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
    if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
    skipcomment(&lf, &c);  /* re-read initial portion */
  }
  if (c != EOF)
    lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
  status = ecierthon_load(L, getF, &lf, ecierthon_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    ecierthon_settop(L, fnameindex);  /* ignore results from 'ecierthon_load' */
    return errfile(L, "read", fnameindex);
  }
  ecierthon_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (ecierthon_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


ecierthonLIB_API int ecierthonL_loadbufferx (ecierthon_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return ecierthon_load(L, getS, &ls, name, mode);
}


ecierthonLIB_API int ecierthonL_loadstring (ecierthon_State *L, const char *s) {
  return ecierthonL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



ecierthonLIB_API int ecierthonL_getmetafield (ecierthon_State *L, int obj, const char *event) {
  if (!ecierthon_getmetatable(L, obj))  /* no metatable? */
    return ecierthon_TNIL;
  else {
    int tt;
    ecierthon_pushstring(L, event);
    tt = ecierthon_rawget(L, -2);
    if (tt == ecierthon_TNIL)  /* is metafield nil? */
      ecierthon_pop(L, 2);  /* remove metatable and metafield */
    else
      ecierthon_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


ecierthonLIB_API int ecierthonL_callmeta (ecierthon_State *L, int obj, const char *event) {
  obj = ecierthon_absindex(L, obj);
  if (ecierthonL_getmetafield(L, obj, event) == ecierthon_TNIL)  /* no metafield? */
    return 0;
  ecierthon_pushvalue(L, obj);
  ecierthon_call(L, 1, 1);
  return 1;
}


ecierthonLIB_API ecierthon_Integer ecierthonL_len (ecierthon_State *L, int idx) {
  ecierthon_Integer l;
  int isnum;
  ecierthon_len(L, idx);
  l = ecierthon_tointegerx(L, -1, &isnum);
  if (!isnum)
    ecierthonL_error(L, "object length is not an integer");
  ecierthon_pop(L, 1);  /* remove object */
  return l;
}


ecierthonLIB_API const char *ecierthonL_tolstring (ecierthon_State *L, int idx, size_t *len) {
  if (ecierthonL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!ecierthon_isstring(L, -1))
      ecierthonL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (ecierthon_type(L, idx)) {
      case ecierthon_TNUMBER: {
        if (ecierthon_isinteger(L, idx))
          ecierthon_pushfstring(L, "%I", (ecierthonI_UACINT)ecierthon_tointeger(L, idx));
        else
          ecierthon_pushfstring(L, "%f", (ecierthonI_UACNUMBER)ecierthon_tonumber(L, idx));
        break;
      }
      case ecierthon_TSTRING:
        ecierthon_pushvalue(L, idx);
        break;
      case ecierthon_TBOOLEAN:
        ecierthon_pushstring(L, (ecierthon_toboolean(L, idx) ? "true" : "false"));
        break;
      case ecierthon_TNIL:
        ecierthon_pushliteral(L, "nil");
        break;
      default: {
        int tt = ecierthonL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == ecierthon_TSTRING) ? ecierthon_tostring(L, -1) :
                                                 ecierthonL_typename(L, idx);
        ecierthon_pushfstring(L, "%s: %p", kind, ecierthon_topointer(L, idx));
        if (tt != ecierthon_TNIL)
          ecierthon_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return ecierthon_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
ecierthonLIB_API void ecierthonL_setfuncs (ecierthon_State *L, const ecierthonL_Reg *l, int nup) {
  ecierthonL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* place holder? */
      ecierthon_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        ecierthon_pushvalue(L, -nup);
      ecierthon_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    ecierthon_setfield(L, -(nup + 2), l->name);
  }
  ecierthon_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
ecierthonLIB_API int ecierthonL_getsubtable (ecierthon_State *L, int idx, const char *fname) {
  if (ecierthon_getfield(L, idx, fname) == ecierthon_TTABLE)
    return 1;  /* table already there */
  else {
    ecierthon_pop(L, 1);  /* remove previous result */
    idx = ecierthon_absindex(L, idx);
    ecierthon_newtable(L);
    ecierthon_pushvalue(L, -1);  /* copy to be left at top */
    ecierthon_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
ecierthonLIB_API void ecierthonL_requiref (ecierthon_State *L, const char *modname,
                               ecierthon_CFunction openf, int glb) {
  ecierthonL_getsubtable(L, ecierthon_REGISTRYINDEX, ecierthon_LOADED_TABLE);
  ecierthon_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!ecierthon_toboolean(L, -1)) {  /* package not already loaded? */
    ecierthon_pop(L, 1);  /* remove field */
    ecierthon_pushcfunction(L, openf);
    ecierthon_pushstring(L, modname);  /* argument to open function */
    ecierthon_call(L, 1, 1);  /* call 'openf' to open module */
    ecierthon_pushvalue(L, -1);  /* make copy of module (call result) */
    ecierthon_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  ecierthon_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    ecierthon_pushvalue(L, -1);  /* copy of module */
    ecierthon_setglobal(L, modname);  /* _G[modname] = module */
  }
}


ecierthonLIB_API void ecierthonL_addgsub (ecierthonL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    ecierthonL_addlstring(b, s, wild - s);  /* push prefix */
    ecierthonL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  ecierthonL_addstring(b, s);  /* push last suffix */
}


ecierthonLIB_API const char *ecierthonL_gsub (ecierthon_State *L, const char *s,
                                  const char *p, const char *r) {
  ecierthonL_Buffer b;
  ecierthonL_buffinit(L, &b);
  ecierthonL_addgsub(&b, s, p, r);
  ecierthonL_pushresult(&b);
  return ecierthon_tostring(L, -1);
}


static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}


static int panic (ecierthon_State *L) {
  const char *msg = ecierthon_tostring(L, -1);
  if (msg == NULL) msg = "error object is not a string";
  ecierthon_writestringerror("PANIC: unprotected error in call to ecierthon API (%s)\n",
                        msg);
  return 0;  /* return to ecierthon to abort */
}


/*
** Warning functions:
** warnfoff: warning system is off
** warnfon: ready to start a new message
** warnfcont: previous message is to be continued
*/
static void warnfoff (void *ud, const char *message, int tocont);
static void warnfon (void *ud, const char *message, int tocont);
static void warnfcont (void *ud, const char *message, int tocont);


/*
** Check whether message is a control message. If so, execute the
** control or ignore it if unknown.
*/
static int checkcontrol (ecierthon_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      ecierthon_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      ecierthon_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((ecierthon_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  ecierthon_State *L = (ecierthon_State *)ud;
  ecierthon_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    ecierthon_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    ecierthon_writestringerror("%s", "\n");  /* finish message with end-of-line */
    ecierthon_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((ecierthon_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  ecierthon_writestringerror("%s", "ecierthon warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}


ecierthonLIB_API ecierthon_State *ecierthonL_newstate (void) {
  ecierthon_State *L = ecierthon_newstate(l_alloc, NULL);
  if (L) {
    ecierthon_atpanic(L, &panic);
    ecierthon_setwarnf(L, warnfoff, L);  /* default is warnings off */
  }
  return L;
}


ecierthonLIB_API void ecierthonL_checkversion_ (ecierthon_State *L, ecierthon_Number ver, size_t sz) {
  ecierthon_Number v = ecierthon_version(L);
  if (sz != ecierthonL_NUMSIZES)  /* check numeric types */
    ecierthonL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    ecierthonL_error(L, "version mismatch: app. needs %f, ecierthon core provides %f",
                  (ecierthonI_UACNUMBER)ver, (ecierthonI_UACNUMBER)v);
}

