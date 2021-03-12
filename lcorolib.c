/*
** $Id: lcorolib.c $
** Coroutine Library
** See Copyright Notice in ecierthon.h
*/

#define lcorolib_c
#define ecierthon_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "ecierthon.h"

#include "lauxlib.h"
#include "ecierthonlib.h"


static ecierthon_State *getco (ecierthon_State *L) {
  ecierthon_State *co = ecierthon_tothread(L, 1);
  ecierthonL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (ecierthon_State *L, ecierthon_State *co, int narg) {
  int status, nres;
  if (!ecierthon_checkstack(co, narg)) {
    ecierthon_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  ecierthon_xmove(L, co, narg);
  status = ecierthon_resume(co, L, narg, &nres);
  if (status == ecierthon_OK || status == ecierthon_YIELD) {
    if (!ecierthon_checkstack(L, nres + 1)) {
      ecierthon_pop(co, nres);  /* remove results anyway */
      ecierthon_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    ecierthon_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    ecierthon_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int ecierthonB_coresume (ecierthon_State *L) {
  ecierthon_State *co = getco(L);
  int r;
  r = auxresume(L, co, ecierthon_gettop(L) - 1);
  if (r < 0) {
    ecierthon_pushboolean(L, 0);
    ecierthon_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    ecierthon_pushboolean(L, 1);
    ecierthon_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int ecierthonB_auxwrap (ecierthon_State *L) {
  ecierthon_State *co = ecierthon_tothread(L, ecierthon_upvalueindex(1));
  int r = auxresume(L, co, ecierthon_gettop(L));
  if (r < 0) {  /* error? */
    int stat = ecierthon_status(co);
    if (stat != ecierthon_OK && stat != ecierthon_YIELD)  /* error in the coroutine? */
      ecierthon_resetthread(co);  /* close its tbc variables */
    if (stat != ecierthon_ERRMEM &&  /* not a memory error and ... */
        ecierthon_type(L, -1) == ecierthon_TSTRING) {  /* ... error object is a string? */
      ecierthonL_where(L, 1);  /* add extra info, if available */
      ecierthon_insert(L, -2);
      ecierthon_concat(L, 2);
    }
    return ecierthon_error(L);  /* propagate error */
  }
  return r;
}


static int ecierthonB_cocreate (ecierthon_State *L) {
  ecierthon_State *NL;
  ecierthonL_checktype(L, 1, ecierthon_TFUNCTION);
  NL = ecierthon_newthread(L);
  ecierthon_pushvalue(L, 1);  /* move function to top */
  ecierthon_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int ecierthonB_cowrap (ecierthon_State *L) {
  ecierthonB_cocreate(L);
  ecierthon_pushcclosure(L, ecierthonB_auxwrap, 1);
  return 1;
}


static int ecierthonB_yield (ecierthon_State *L) {
  return ecierthon_yield(L, ecierthon_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (ecierthon_State *L, ecierthon_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (ecierthon_status(co)) {
      case ecierthon_YIELD:
        return COS_YIELD;
      case ecierthon_OK: {
        ecierthon_Debug ar;
        if (ecierthon_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (ecierthon_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int ecierthonB_costatus (ecierthon_State *L) {
  ecierthon_State *co = getco(L);
  ecierthon_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int ecierthonB_yieldable (ecierthon_State *L) {
  ecierthon_State *co = ecierthon_isnone(L, 1) ? L : getco(L);
  ecierthon_pushboolean(L, ecierthon_isyieldable(co));
  return 1;
}


static int ecierthonB_corunning (ecierthon_State *L) {
  int ismain = ecierthon_pushthread(L);
  ecierthon_pushboolean(L, ismain);
  return 2;
}


static int ecierthonB_close (ecierthon_State *L) {
  ecierthon_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = ecierthon_resetthread(co);
      if (status == ecierthon_OK) {
        ecierthon_pushboolean(L, 1);
        return 1;
      }
      else {
        ecierthon_pushboolean(L, 0);
        ecierthon_xmove(co, L, 1);  /* copy error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      return ecierthonL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const ecierthonL_Reg co_funcs[] = {
  {"create", ecierthonB_cocreate},
  {"resume", ecierthonB_coresume},
  {"running", ecierthonB_corunning},
  {"status", ecierthonB_costatus},
  {"wrap", ecierthonB_cowrap},
  {"yield", ecierthonB_yield},
  {"isyieldable", ecierthonB_yieldable},
  {"close", ecierthonB_close},
  {NULL, NULL}
};



ecierthonMOD_API int ecierthonopen_coroutine (ecierthon_State *L) {
  ecierthonL_newlib(L, co_funcs);
  return 1;
}

