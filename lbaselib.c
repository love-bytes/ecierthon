/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in ecierthon.h
*/

#define lbaselib_c
#define ecierthon_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ecierthon.h"

#include "lauxlib.h"
#include "ecierthonlib.h"


static int ecierthonB_print (ecierthon_State *L) {
  int n = ecierthon_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = ecierthonL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      ecierthon_writestring("\t", 1);  /* add a tab before it */
    ecierthon_writestring(s, l);  /* print it */
    ecierthon_pop(L, 1);  /* pop result */
  }
  ecierthon_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int ecierthonB_warn (ecierthon_State *L) {
  int n = ecierthon_gettop(L);  /* number of arguments */
  int i;
  ecierthonL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    ecierthonL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    ecierthon_warning(L, ecierthon_tostring(L, i), 1);
  ecierthon_warning(L, ecierthon_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, int base, ecierthon_Integer *pn) {
  ecierthon_Unsigned n = 0;
  int neg = 0;
  s += strspn(s, SPACECHARS);  /* skip initial spaces */
  if (*s == '-') { s++; neg = 1; }  /* handle sign */
  else if (*s == '+') s++;
  if (!isalnum((unsigned char)*s))  /* no digit? */
    return NULL;
  do {
    int digit = (isdigit((unsigned char)*s)) ? *s - '0'
                   : (toupper((unsigned char)*s) - 'A') + 10;
    if (digit >= base) return NULL;  /* invalid numeral */
    n = n * base + digit;
    s++;
  } while (isalnum((unsigned char)*s));
  s += strspn(s, SPACECHARS);  /* skip trailing spaces */
  *pn = (ecierthon_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int ecierthonB_tonumber (ecierthon_State *L) {
  if (ecierthon_isnoneornil(L, 2)) {  /* standard conversion? */
    if (ecierthon_type(L, 1) == ecierthon_TNUMBER) {  /* already a number? */
      ecierthon_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = ecierthon_tolstring(L, 1, &l);
      if (s != NULL && ecierthon_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      ecierthonL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    ecierthon_Integer n = 0;  /* to avoid warnings */
    ecierthon_Integer base = ecierthonL_checkinteger(L, 2);
    ecierthonL_checktype(L, 1, ecierthon_TSTRING);  /* no numbers as strings */
    s = ecierthon_tolstring(L, 1, &l);
    ecierthonL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      ecierthon_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  ecierthonL_pushfail(L);  /* not a number */
  return 1;
}


static int ecierthonB_error (ecierthon_State *L) {
  int level = (int)ecierthonL_optinteger(L, 2, 1);
  ecierthon_settop(L, 1);
  if (ecierthon_type(L, 1) == ecierthon_TSTRING && level > 0) {
    ecierthonL_where(L, level);   /* add extra information */
    ecierthon_pushvalue(L, 1);
    ecierthon_concat(L, 2);
  }
  return ecierthon_error(L);
}


static int ecierthonB_getmetatable (ecierthon_State *L) {
  ecierthonL_checkany(L, 1);
  if (!ecierthon_getmetatable(L, 1)) {
    ecierthon_pushnil(L);
    return 1;  /* no metatable */
  }
  ecierthonL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int ecierthonB_setmetatable (ecierthon_State *L) {
  int t = ecierthon_type(L, 2);
  ecierthonL_checktype(L, 1, ecierthon_TTABLE);
  ecierthonL_argexpected(L, t == ecierthon_TNIL || t == ecierthon_TTABLE, 2, "nil or table");
  if (ecierthonL_getmetafield(L, 1, "__metatable") != ecierthon_TNIL)
    return ecierthonL_error(L, "cannot change a protected metatable");
  ecierthon_settop(L, 2);
  ecierthon_setmetatable(L, 1);
  return 1;
}


static int ecierthonB_rawequal (ecierthon_State *L) {
  ecierthonL_checkany(L, 1);
  ecierthonL_checkany(L, 2);
  ecierthon_pushboolean(L, ecierthon_rawequal(L, 1, 2));
  return 1;
}


static int ecierthonB_rawlen (ecierthon_State *L) {
  int t = ecierthon_type(L, 1);
  ecierthonL_argexpected(L, t == ecierthon_TTABLE || t == ecierthon_TSTRING, 1,
                      "table or string");
  ecierthon_pushinteger(L, ecierthon_rawlen(L, 1));
  return 1;
}


static int ecierthonB_rawget (ecierthon_State *L) {
  ecierthonL_checktype(L, 1, ecierthon_TTABLE);
  ecierthonL_checkany(L, 2);
  ecierthon_settop(L, 2);
  ecierthon_rawget(L, 1);
  return 1;
}

static int ecierthonB_rawset (ecierthon_State *L) {
  ecierthonL_checktype(L, 1, ecierthon_TTABLE);
  ecierthonL_checkany(L, 2);
  ecierthonL_checkany(L, 3);
  ecierthon_settop(L, 3);
  ecierthon_rawset(L, 1);
  return 1;
}


static int pushmode (ecierthon_State *L, int oldmode) {
  ecierthon_pushstring(L, (oldmode == ecierthon_GCINC) ? "incremental" : "generational");
  return 1;
}


static int ecierthonB_collectgarbage (ecierthon_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", "generational", "incremental", NULL};
  static const int optsnum[] = {ecierthon_GCSTOP, ecierthon_GCRESTART, ecierthon_GCCOLLECT,
    ecierthon_GCCOUNT, ecierthon_GCSTEP, ecierthon_GCSETPAUSE, ecierthon_GCSETSTEPMUL,
    ecierthon_GCISRUNNING, ecierthon_GCGEN, ecierthon_GCINC};
  int o = optsnum[ecierthonL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case ecierthon_GCCOUNT: {
      int k = ecierthon_gc(L, o);
      int b = ecierthon_gc(L, ecierthon_GCCOUNTB);
      ecierthon_pushnumber(L, (ecierthon_Number)k + ((ecierthon_Number)b/1024));
      return 1;
    }
    case ecierthon_GCSTEP: {
      int step = (int)ecierthonL_optinteger(L, 2, 0);
      int res = ecierthon_gc(L, o, step);
      ecierthon_pushboolean(L, res);
      return 1;
    }
    case ecierthon_GCSETPAUSE:
    case ecierthon_GCSETSTEPMUL: {
      int p = (int)ecierthonL_optinteger(L, 2, 0);
      int previous = ecierthon_gc(L, o, p);
      ecierthon_pushinteger(L, previous);
      return 1;
    }
    case ecierthon_GCISRUNNING: {
      int res = ecierthon_gc(L, o);
      ecierthon_pushboolean(L, res);
      return 1;
    }
    case ecierthon_GCGEN: {
      int minormul = (int)ecierthonL_optinteger(L, 2, 0);
      int majormul = (int)ecierthonL_optinteger(L, 3, 0);
      return pushmode(L, ecierthon_gc(L, o, minormul, majormul));
    }
    case ecierthon_GCINC: {
      int pause = (int)ecierthonL_optinteger(L, 2, 0);
      int stepmul = (int)ecierthonL_optinteger(L, 3, 0);
      int stepsize = (int)ecierthonL_optinteger(L, 4, 0);
      return pushmode(L, ecierthon_gc(L, o, pause, stepmul, stepsize));
    }
    default: {
      int res = ecierthon_gc(L, o);
      ecierthon_pushinteger(L, res);
      return 1;
    }
  }
}


static int ecierthonB_type (ecierthon_State *L) {
  int t = ecierthon_type(L, 1);
  ecierthonL_argcheck(L, t != ecierthon_TNONE, 1, "value expected");
  ecierthon_pushstring(L, ecierthon_typename(L, t));
  return 1;
}


static int ecierthonB_next (ecierthon_State *L) {
  ecierthonL_checktype(L, 1, ecierthon_TTABLE);
  ecierthon_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (ecierthon_next(L, 1))
    return 2;
  else {
    ecierthon_pushnil(L);
    return 1;
  }
}


static int ecierthonB_pairs (ecierthon_State *L) {
  ecierthonL_checkany(L, 1);
  if (ecierthonL_getmetafield(L, 1, "__pairs") == ecierthon_TNIL) {  /* no metamethod? */
    ecierthon_pushcfunction(L, ecierthonB_next);  /* will return generator, */
    ecierthon_pushvalue(L, 1);  /* state, */
    ecierthon_pushnil(L);  /* and initial value */
  }
  else {
    ecierthon_pushvalue(L, 1);  /* argument 'self' to metamethod */
    ecierthon_call(L, 1, 3);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (ecierthon_State *L) {
  ecierthon_Integer i = ecierthonL_checkinteger(L, 2) + 1;
  ecierthon_pushinteger(L, i);
  return (ecierthon_geti(L, 1, i) == ecierthon_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int ecierthonB_ipairs (ecierthon_State *L) {
  ecierthonL_checkany(L, 1);
  ecierthon_pushcfunction(L, ipairsaux);  /* iteration function */
  ecierthon_pushvalue(L, 1);  /* state */
  ecierthon_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (ecierthon_State *L, int status, int envidx) {
  if (status == ecierthon_OK) {
    if (envidx != 0) {  /* 'env' parameter? */
      ecierthon_pushvalue(L, envidx);  /* environment for loaded function */
      if (!ecierthon_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        ecierthon_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    ecierthonL_pushfail(L);
    ecierthon_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static int ecierthonB_loadfile (ecierthon_State *L) {
  const char *fname = ecierthonL_optstring(L, 1, NULL);
  const char *mode = ecierthonL_optstring(L, 2, NULL);
  int env = (!ecierthon_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = ecierthonL_loadfilex(L, fname, mode);
  return load_aux(L, status, env);
}


/*
** {======================================================
** Generic Read function
** =======================================================
*/


/*
** reserved slot, above all arguments, to hold a copy of the returned
** string to avoid it being collected while parsed. 'load' has four
** optional arguments (chunk, source name, mode, and environment).
*/
#define RESERVEDSLOT	5


/*
** Reader for generic 'load' function: 'ecierthon_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (ecierthon_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  ecierthonL_checkstack(L, 2, "too many nested functions");
  ecierthon_pushvalue(L, 1);  /* get function */
  ecierthon_call(L, 0, 1);  /* call it */
  if (ecierthon_isnil(L, -1)) {
    ecierthon_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (!ecierthon_isstring(L, -1))
    ecierthonL_error(L, "reader function must return a string");
  ecierthon_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return ecierthon_tolstring(L, RESERVEDSLOT, size);
}


static int ecierthonB_load (ecierthon_State *L) {
  int status;
  size_t l;
  const char *s = ecierthon_tolstring(L, 1, &l);
  const char *mode = ecierthonL_optstring(L, 3, "bt");
  int env = (!ecierthon_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = ecierthonL_optstring(L, 2, s);
    status = ecierthonL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = ecierthonL_optstring(L, 2, "=(load)");
    ecierthonL_checktype(L, 1, ecierthon_TFUNCTION);
    ecierthon_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = ecierthon_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (ecierthon_State *L, int d1, ecierthon_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'ecierthon_Kfunction' prototype */
  return ecierthon_gettop(L) - 1;
}


static int ecierthonB_dofile (ecierthon_State *L) {
  const char *fname = ecierthonL_optstring(L, 1, NULL);
  ecierthon_settop(L, 1);
  if (ecierthonL_loadfile(L, fname) != ecierthon_OK)
    return ecierthon_error(L);
  ecierthon_callk(L, 0, ecierthon_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int ecierthonB_assert (ecierthon_State *L) {
  if (ecierthon_toboolean(L, 1))  /* condition is true? */
    return ecierthon_gettop(L);  /* return all arguments */
  else {  /* error */
    ecierthonL_checkany(L, 1);  /* there must be a condition */
    ecierthon_remove(L, 1);  /* remove it */
    ecierthon_pushliteral(L, "assertion failed!");  /* default message */
    ecierthon_settop(L, 1);  /* leave only message (default if no other one) */
    return ecierthonB_error(L);  /* call 'error' */
  }
}


static int ecierthonB_select (ecierthon_State *L) {
  int n = ecierthon_gettop(L);
  if (ecierthon_type(L, 1) == ecierthon_TSTRING && *ecierthon_tostring(L, 1) == '#') {
    ecierthon_pushinteger(L, n-1);
    return 1;
  }
  else {
    ecierthon_Integer i = ecierthonL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    ecierthonL_argcheck(L, 1 <= i, 1, "index out of range");
    return n - (int)i;
  }
}


/*
** Continuation function for 'pcall' and 'xpcall'. Both functions
** already pushed a 'true' before doing the call, so in case of success
** 'finishpcall' only has to return everything in the stack minus
** 'extra' values (where 'extra' is exactly the number of items to be
** ignored).
*/
static int finishpcall (ecierthon_State *L, int status, ecierthon_KContext extra) {
  if (status != ecierthon_OK && status != ecierthon_YIELD) {  /* error? */
    ecierthon_pushboolean(L, 0);  /* first result (false) */
    ecierthon_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return ecierthon_gettop(L) - (int)extra;  /* return all results */
}


static int ecierthonB_pcall (ecierthon_State *L) {
  int status;
  ecierthonL_checkany(L, 1);
  ecierthon_pushboolean(L, 1);  /* first result if no errors */
  ecierthon_insert(L, 1);  /* put it in place */
  status = ecierthon_pcallk(L, ecierthon_gettop(L) - 2, ecierthon_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'ecierthon_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int ecierthonB_xpcall (ecierthon_State *L) {
  int status;
  int n = ecierthon_gettop(L);
  ecierthonL_checktype(L, 2, ecierthon_TFUNCTION);  /* check error function */
  ecierthon_pushboolean(L, 1);  /* first result */
  ecierthon_pushvalue(L, 1);  /* function */
  ecierthon_rotate(L, 3, 2);  /* move them below function's arguments */
  status = ecierthon_pcallk(L, n - 2, ecierthon_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int ecierthonB_tostring (ecierthon_State *L) {
  ecierthonL_checkany(L, 1);
  ecierthonL_tolstring(L, 1, NULL);
  return 1;
}


static const ecierthonL_Reg base_funcs[] = {
  {"assert", ecierthonB_assert},
  {"collectgarbage", ecierthonB_collectgarbage},
  {"dofile", ecierthonB_dofile},
  {"error", ecierthonB_error},
  {"getmetatable", ecierthonB_getmetatable},
  {"ipairs", ecierthonB_ipairs},
  {"loadfile", ecierthonB_loadfile},
  {"load", ecierthonB_load},
  {"next", ecierthonB_next},
  {"pairs", ecierthonB_pairs},
  {"pcall", ecierthonB_pcall},
  {"print", ecierthonB_print},
  {"warn", ecierthonB_warn},
  {"rawequal", ecierthonB_rawequal},
  {"rawlen", ecierthonB_rawlen},
  {"rawget", ecierthonB_rawget},
  {"rawset", ecierthonB_rawset},
  {"select", ecierthonB_select},
  {"setmetatable", ecierthonB_setmetatable},
  {"tonumber", ecierthonB_tonumber},
  {"tostring", ecierthonB_tostring},
  {"type", ecierthonB_type},
  {"xpcall", ecierthonB_xpcall},
  /* placeholders */
  {ecierthon_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


ecierthonMOD_API int ecierthonopen_base (ecierthon_State *L) {
  /* open lib into global table */
  ecierthon_pushglobaltable(L);
  ecierthonL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  ecierthon_pushvalue(L, -1);
  ecierthon_setfield(L, -2, ecierthon_GNAME);
  /* set global _VERSION */
  ecierthon_pushliteral(L, ecierthon_VERSION);
  ecierthon_setfield(L, -2, "_VERSION");
  return 1;
}

