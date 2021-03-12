/*
** $Id: ldblib.c $
** Interface from ecierthon to its debug API
** See Copyright Notice in ecierthon.h
*/

#define ldblib_c
#define ecierthon_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ecierthon.h"

#include "lauxlib.h"
#include "ecierthonlib.h"


/*
** The hook table at registry[HOOKKEY] maps threads to their current
** hook function.
*/
static const char *const HOOKKEY = "_HOOKKEY";


/*
** If L1 != L, L1 can be in any state, and therefore there are no
** guarantees about its stack space; any push in L1 must be
** checked.
*/
static void checkstack (ecierthon_State *L, ecierthon_State *L1, int n) {
  if (L != L1 && !ecierthon_checkstack(L1, n))
    ecierthonL_error(L, "stack overflow");
}


static int db_getregistry (ecierthon_State *L) {
  ecierthon_pushvalue(L, ecierthon_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (ecierthon_State *L) {
  ecierthonL_checkany(L, 1);
  if (!ecierthon_getmetatable(L, 1)) {
    ecierthon_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (ecierthon_State *L) {
  int t = ecierthon_type(L, 2);
  ecierthonL_argexpected(L, t == ecierthon_TNIL || t == ecierthon_TTABLE, 2, "nil or table");
  ecierthon_settop(L, 2);
  ecierthon_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (ecierthon_State *L) {
  int n = (int)ecierthonL_optinteger(L, 2, 1);
  if (ecierthon_type(L, 1) != ecierthon_TUSERDATA)
    ecierthonL_pushfail(L);
  else if (ecierthon_getiuservalue(L, 1, n) != ecierthon_TNONE) {
    ecierthon_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (ecierthon_State *L) {
  int n = (int)ecierthonL_optinteger(L, 3, 1);
  ecierthonL_checktype(L, 1, ecierthon_TUSERDATA);
  ecierthonL_checkany(L, 2);
  ecierthon_settop(L, 2);
  if (!ecierthon_setiuservalue(L, 1, n))
    ecierthonL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static ecierthon_State *getthread (ecierthon_State *L, int *arg) {
  if (ecierthon_isthread(L, 1)) {
    *arg = 1;
    return ecierthon_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'ecierthon_settable', used by 'db_getinfo' to put results
** from 'ecierthon_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (ecierthon_State *L, const char *k, const char *v) {
  ecierthon_pushstring(L, v);
  ecierthon_setfield(L, -2, k);
}

static void settabsi (ecierthon_State *L, const char *k, int v) {
  ecierthon_pushinteger(L, v);
  ecierthon_setfield(L, -2, k);
}

static void settabsb (ecierthon_State *L, const char *k, int v) {
  ecierthon_pushboolean(L, v);
  ecierthon_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'ecierthon_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'ecierthon_getinfo' on top of the result table so that it can call
** 'ecierthon_setfield'.
*/
static void treatstackoption (ecierthon_State *L, ecierthon_State *L1, const char *fname) {
  if (L == L1)
    ecierthon_rotate(L, -2, 1);  /* exchange object and table */
  else
    ecierthon_xmove(L1, L, 1);  /* move object to the "main" stack */
  ecierthon_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'ecierthon_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'ecierthon_getinfo'.
*/
static int db_getinfo (ecierthon_State *L) {
  ecierthon_Debug ar;
  int arg;
  ecierthon_State *L1 = getthread(L, &arg);
  const char *options = ecierthonL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  if (ecierthon_isfunction(L, arg + 1)) {  /* info about a function? */
    options = ecierthon_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    ecierthon_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    ecierthon_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!ecierthon_getstack(L1, (int)ecierthonL_checkinteger(L, arg + 1), &ar)) {
      ecierthonL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!ecierthon_getinfo(L1, options, &ar))
    return ecierthonL_argerror(L, arg+2, "invalid option");
  ecierthon_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    ecierthon_pushlstring(L, ar.source, ar.srclen);
    ecierthon_setfield(L, -2, "source");
    settabss(L, "short_src", ar.short_src);
    settabsi(L, "linedefined", ar.linedefined);
    settabsi(L, "lastlinedefined", ar.lastlinedefined);
    settabss(L, "what", ar.what);
  }
  if (strchr(options, 'l'))
    settabsi(L, "currentline", ar.currentline);
  if (strchr(options, 'u')) {
    settabsi(L, "nups", ar.nups);
    settabsi(L, "nparams", ar.nparams);
    settabsb(L, "isvararg", ar.isvararg);
  }
  if (strchr(options, 'n')) {
    settabss(L, "name", ar.name);
    settabss(L, "namewhat", ar.namewhat);
  }
  if (strchr(options, 'r')) {
    settabsi(L, "ftransfer", ar.ftransfer);
    settabsi(L, "ntransfer", ar.ntransfer);
  }
  if (strchr(options, 't'))
    settabsb(L, "istailcall", ar.istailcall);
  if (strchr(options, 'L'))
    treatstackoption(L, L1, "activelines");
  if (strchr(options, 'f'))
    treatstackoption(L, L1, "func");
  return 1;  /* return table */
}


static int db_getlocal (ecierthon_State *L) {
  int arg;
  ecierthon_State *L1 = getthread(L, &arg);
  int nvar = (int)ecierthonL_checkinteger(L, arg + 2);  /* local-variable index */
  if (ecierthon_isfunction(L, arg + 1)) {  /* function argument? */
    ecierthon_pushvalue(L, arg + 1);  /* push function */
    ecierthon_pushstring(L, ecierthon_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    ecierthon_Debug ar;
    const char *name;
    int level = (int)ecierthonL_checkinteger(L, arg + 1);
    if (!ecierthon_getstack(L1, level, &ar))  /* out of range? */
      return ecierthonL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = ecierthon_getlocal(L1, &ar, nvar);
    if (name) {
      ecierthon_xmove(L1, L, 1);  /* move local value */
      ecierthon_pushstring(L, name);  /* push name */
      ecierthon_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      ecierthonL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (ecierthon_State *L) {
  int arg;
  const char *name;
  ecierthon_State *L1 = getthread(L, &arg);
  ecierthon_Debug ar;
  int level = (int)ecierthonL_checkinteger(L, arg + 1);
  int nvar = (int)ecierthonL_checkinteger(L, arg + 2);
  if (!ecierthon_getstack(L1, level, &ar))  /* out of range? */
    return ecierthonL_argerror(L, arg+1, "level out of range");
  ecierthonL_checkany(L, arg+3);
  ecierthon_settop(L, arg+3);
  checkstack(L, L1, 1);
  ecierthon_xmove(L, L1, 1);
  name = ecierthon_setlocal(L1, &ar, nvar);
  if (name == NULL)
    ecierthon_pop(L1, 1);  /* pop value (if not popped by 'ecierthon_setlocal') */
  ecierthon_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (ecierthon_State *L, int get) {
  const char *name;
  int n = (int)ecierthonL_checkinteger(L, 2);  /* upvalue index */
  ecierthonL_checktype(L, 1, ecierthon_TFUNCTION);  /* closure */
  name = get ? ecierthon_getupvalue(L, 1, n) : ecierthon_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  ecierthon_pushstring(L, name);
  ecierthon_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (ecierthon_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (ecierthon_State *L) {
  ecierthonL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (ecierthon_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)ecierthonL_checkinteger(L, argnup);  /* upvalue index */
  ecierthonL_checktype(L, argf, ecierthon_TFUNCTION);  /* closure */
  id = ecierthon_upvalueid(L, argf, nup);
  if (pnup) {
    ecierthonL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (ecierthon_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    ecierthon_pushlightuserdata(L, id);
  else
    ecierthonL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (ecierthon_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  ecierthonL_argcheck(L, !ecierthon_iscfunction(L, 1), 1, "ecierthon function expected");
  ecierthonL_argcheck(L, !ecierthon_iscfunction(L, 3), 3, "ecierthon function expected");
  ecierthon_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (ecierthon_State *L, ecierthon_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  ecierthon_getfield(L, ecierthon_REGISTRYINDEX, HOOKKEY);
  ecierthon_pushthread(L);
  if (ecierthon_rawget(L, -2) == ecierthon_TFUNCTION) {  /* is there a hook function? */
    ecierthon_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      ecierthon_pushinteger(L, ar->currentline);  /* push current line */
    else ecierthon_pushnil(L);
    ecierthon_assert(ecierthon_getinfo(L, "lS", ar));
    ecierthon_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= ecierthon_MASKCALL;
  if (strchr(smask, 'r')) mask |= ecierthon_MASKRET;
  if (strchr(smask, 'l')) mask |= ecierthon_MASKLINE;
  if (count > 0) mask |= ecierthon_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & ecierthon_MASKCALL) smask[i++] = 'c';
  if (mask & ecierthon_MASKRET) smask[i++] = 'r';
  if (mask & ecierthon_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (ecierthon_State *L) {
  int arg, mask, count;
  ecierthon_Hook func;
  ecierthon_State *L1 = getthread(L, &arg);
  if (ecierthon_isnoneornil(L, arg+1)) {  /* no hook? */
    ecierthon_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = ecierthonL_checkstring(L, arg+2);
    ecierthonL_checktype(L, arg+1, ecierthon_TFUNCTION);
    count = (int)ecierthonL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!ecierthonL_getsubtable(L, ecierthon_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    ecierthon_pushstring(L, "k");
    ecierthon_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    ecierthon_pushvalue(L, -1);
    ecierthon_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  ecierthon_pushthread(L1); ecierthon_xmove(L1, L, 1);  /* key (thread) */
  ecierthon_pushvalue(L, arg + 1);  /* value (hook function) */
  ecierthon_rawset(L, -3);  /* hooktable[L1] = new ecierthon hook */
  ecierthon_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (ecierthon_State *L) {
  int arg;
  ecierthon_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = ecierthon_gethookmask(L1);
  ecierthon_Hook hook = ecierthon_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    ecierthonL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    ecierthon_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    ecierthon_getfield(L, ecierthon_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    ecierthon_pushthread(L1); ecierthon_xmove(L1, L, 1);
    ecierthon_rawget(L, -2);   /* 1st result = hooktable[L1] */
    ecierthon_remove(L, -2);  /* remove hook table */
  }
  ecierthon_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  ecierthon_pushinteger(L, ecierthon_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (ecierthon_State *L) {
  for (;;) {
    char buffer[250];
    ecierthon_writestringerror("%s", "ecierthon_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == 0 ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (ecierthonL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        ecierthon_pcall(L, 0, 0, 0))
      ecierthon_writestringerror("%s\n", ecierthonL_tolstring(L, -1, NULL));
    ecierthon_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (ecierthon_State *L) {
  int arg;
  ecierthon_State *L1 = getthread(L, &arg);
  const char *msg = ecierthon_tostring(L, arg + 1);
  if (msg == NULL && !ecierthon_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    ecierthon_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)ecierthonL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    ecierthonL_traceback(L, L1, msg, level);
  }
  return 1;
}


static int db_setcstacklimit (ecierthon_State *L) {
  int limit = (int)ecierthonL_checkinteger(L, 1);
  int res = ecierthon_setcstacklimit(L, limit);
  ecierthon_pushinteger(L, res);
  return 1;
}


static const ecierthonL_Reg dblib[] = {
  {"debug", db_debug},
  {"getuservalue", db_getuservalue},
  {"gethook", db_gethook},
  {"getinfo", db_getinfo},
  {"getlocal", db_getlocal},
  {"getregistry", db_getregistry},
  {"getmetatable", db_getmetatable},
  {"getupvalue", db_getupvalue},
  {"upvaluejoin", db_upvaluejoin},
  {"upvalueid", db_upvalueid},
  {"setuservalue", db_setuservalue},
  {"sethook", db_sethook},
  {"setlocal", db_setlocal},
  {"setmetatable", db_setmetatable},
  {"setupvalue", db_setupvalue},
  {"traceback", db_traceback},
  {"setcstacklimit", db_setcstacklimit},
  {NULL, NULL}
};


ecierthonMOD_API int ecierthonopen_debug (ecierthon_State *L) {
  ecierthonL_newlib(L, dblib);
  return 1;
}

