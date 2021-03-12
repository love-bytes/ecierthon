/*
** $Id: lapi.c $
** ecierthon API
** See Copyright Notice in ecierthon.h
*/

#define lapi_c
#define ecierthon_CORE

#include "lprefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "ecierthon.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"



const char ecierthon_ident[] =
  "$ecierthonVersion: " ecierthon_COPYRIGHT " $"
  "$ecierthonAuthors: " ecierthon_AUTHORS " $";



/*
** Test for a valid index.
** '!ttisnil(o)' implies 'o != &G(L)->nilvalue', so it is not needed.
** However, it covers the most common cases in a faster way.
*/
#define isvalid(L, o)	(!ttisnil(o) || o != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= ecierthon_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < ecierthon_REGISTRYINDEX)


static TValue *index2value (ecierthon_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func + idx;
    api_check(L, idx <= L->ci->top - (ci->func + 1), "unacceptable index");
    if (o >= L->top) return &G(L)->nilvalue;
    else return s2v(o);
  }
  else if (!ispseudo(idx)) {  /* negative index */
    api_check(L, idx != 0 && -idx <= L->top - (ci->func + 1), "invalid index");
    return s2v(L->top + idx);
  }
  else if (idx == ecierthon_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = ecierthon_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttislcf(s2v(ci->func)))  /* light C function? */
      return &G(L)->nilvalue;  /* it has no upvalues */
    else {
      CClosure *func = clCvalue(s2v(ci->func));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1] : &G(L)->nilvalue;
    }
  }
}


static StkId index2stack (ecierthon_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func + idx;
    api_check(L, o < L->top, "unacceptable index");
    return o;
  }
  else {    /* non-positive index */
    api_check(L, idx != 0 && -idx <= L->top - (ci->func + 1), "invalid index");
    api_check(L, !ispseudo(idx), "invalid index");
    return L->top + idx;
  }
}


ecierthon_API int ecierthon_checkstack (ecierthon_State *L, int n) {
  int res;
  CallInfo *ci;
  ecierthon_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last - L->top > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else {  /* no; need to grow stack */
    int inuse = cast_int(L->top - L->stack) + EXTRA_STACK;
    if (inuse > ecierthonI_MAXSTACK - n)  /* can grow without overflow? */
      res = 0;  /* no */
    else  /* try to grow stack */
      res = ecierthonD_growstack(L, n, 0);
  }
  if (res && ci->top < L->top + n)
    ci->top = L->top + n;  /* adjust frame top */
  ecierthon_unlock(L);
  return res;
}


ecierthon_API void ecierthon_xmove (ecierthon_State *from, ecierthon_State *to, int n) {
  int i;
  if (from == to) return;
  ecierthon_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top - to->top >= n, "stack overflow");
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top, from->top + i);
    to->top++;  /* stack already checked by previous 'api_check' */
  }
  ecierthon_unlock(to);
}


ecierthon_API ecierthon_CFunction ecierthon_atpanic (ecierthon_State *L, ecierthon_CFunction panicf) {
  ecierthon_CFunction old;
  ecierthon_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  ecierthon_unlock(L);
  return old;
}


ecierthon_API ecierthon_Number ecierthon_version (ecierthon_State *L) {
  UNUSED(L);
  return ecierthon_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
ecierthon_API int ecierthon_absindex (ecierthon_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top - L->ci->func) + idx;
}


ecierthon_API int ecierthon_gettop (ecierthon_State *L) {
  return cast_int(L->top - (L->ci->func + 1));
}


ecierthon_API void ecierthon_settop (ecierthon_State *L, int idx) {
  CallInfo *ci;
  StkId func;
  ptrdiff_t diff;  /* difference for new top */
  ecierthon_lock(L);
  ci = L->ci;
  func = ci->func;
  if (idx >= 0) {
    api_check(L, idx <= ci->top - (func + 1), "new top too large");
    diff = ((func + 1) + idx) - L->top;
    for (; diff > 0; diff--)
      setnilvalue(s2v(L->top++));  /* clear new slots */
  }
  else {
    api_check(L, -(idx+1) <= (L->top - (func + 1)), "invalid new top");
    diff = idx + 1;  /* will "subtract" index (as it is negative) */
  }
  if (diff < 0 && hastocloseCfunc(ci->nresults))
    ecierthonF_close(L, L->top + diff, ecierthon_OK);
  L->top += diff;  /* correct top only after closing any upvalue */
  ecierthon_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'ecierthon_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
static void reverse (ecierthon_State *L, StkId from, StkId to) {
  for (; from < to; from++, to--) {
    TValue temp;
    setobj(L, &temp, s2v(from));
    setobjs2s(L, from, to);
    setobj2s(L, to, &temp);
  }
}


/*
** Let x = AB, where A is a prefix of length 'n'. Then,
** rotate x n == BA. But BA == (A^r . B^r)^r.
*/
ecierthon_API void ecierthon_rotate (ecierthon_State *L, int idx, int n) {
  StkId p, t, m;
  ecierthon_lock(L);
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  ecierthon_unlock(L);
}


ecierthon_API void ecierthon_copy (ecierthon_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  ecierthon_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    ecierthonC_barrier(L, clCvalue(s2v(L->ci->func)), fr);
  /* ecierthon_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  ecierthon_unlock(L);
}


ecierthon_API void ecierthon_pushvalue (ecierthon_State *L, int idx) {
  ecierthon_lock(L);
  setobj2s(L, L->top, index2value(L, idx));
  api_incr_top(L);
  ecierthon_unlock(L);
}



/*
** access functions (stack -> C)
*/


ecierthon_API int ecierthon_type (ecierthon_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : ecierthon_TNONE);
}


ecierthon_API const char *ecierthon_typename (ecierthon_State *L, int t) {
  UNUSED(L);
  api_check(L, ecierthon_TNONE <= t && t < ecierthon_NUMTYPES, "invalid type");
  return ttypename(t);
}


ecierthon_API int ecierthon_iscfunction (ecierthon_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


ecierthon_API int ecierthon_isinteger (ecierthon_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


ecierthon_API int ecierthon_isnumber (ecierthon_State *L, int idx) {
  ecierthon_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


ecierthon_API int ecierthon_isstring (ecierthon_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


ecierthon_API int ecierthon_isuserdata (ecierthon_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


ecierthon_API int ecierthon_rawequal (ecierthon_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? ecierthonV_rawequalobj(o1, o2) : 0;
}


ecierthon_API void ecierthon_arith (ecierthon_State *L, int op) {
  ecierthon_lock(L);
  if (op != ecierthon_OPUNM && op != ecierthon_OPBNOT)
    api_checknelems(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top, L->top - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result go to top - 2 */
  ecierthonO_arith(L, op, s2v(L->top - 2), s2v(L->top - 1), L->top - 2);
  L->top--;  /* remove second operand */
  ecierthon_unlock(L);
}


ecierthon_API int ecierthon_compare (ecierthon_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  ecierthon_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case ecierthon_OPEQ: i = ecierthonV_equalobj(L, o1, o2); break;
      case ecierthon_OPLT: i = ecierthonV_lessthan(L, o1, o2); break;
      case ecierthon_OPLE: i = ecierthonV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  ecierthon_unlock(L);
  return i;
}


ecierthon_API size_t ecierthon_stringtonumber (ecierthon_State *L, const char *s) {
  size_t sz = ecierthonO_str2num(s, s2v(L->top));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


ecierthon_API ecierthon_Number ecierthon_tonumberx (ecierthon_State *L, int idx, int *pisnum) {
  ecierthon_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


ecierthon_API ecierthon_Integer ecierthon_tointegerx (ecierthon_State *L, int idx, int *pisnum) {
  ecierthon_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


ecierthon_API int ecierthon_toboolean (ecierthon_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


ecierthon_API const char *ecierthon_tolstring (ecierthon_State *L, int idx, size_t *len) {
  TValue *o;
  ecierthon_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      ecierthon_unlock(L);
      return NULL;
    }
    ecierthonO_tostring(L, o);
    ecierthonC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  if (len != NULL)
    *len = vslen(o);
  ecierthon_unlock(L);
  return svalue(o);
}


ecierthon_API ecierthon_Unsigned ecierthon_rawlen (ecierthon_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case ecierthon_VSHRSTR: return tsvalue(o)->shrlen;
    case ecierthon_VLNGSTR: return tsvalue(o)->u.lnglen;
    case ecierthon_VUSERDATA: return uvalue(o)->len;
    case ecierthon_VTABLE: return ecierthonH_getn(hvalue(o));
    default: return 0;
  }
}


ecierthon_API ecierthon_CFunction ecierthon_tocfunction (ecierthon_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


static void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case ecierthon_TUSERDATA: return getudatamem(uvalue(o));
    case ecierthon_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


ecierthon_API void *ecierthon_touserdata (ecierthon_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


ecierthon_API ecierthon_State *ecierthon_tothread (ecierthon_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here goes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
ecierthon_API const void *ecierthon_topointer (ecierthon_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case ecierthon_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case ecierthon_VUSERDATA: case ecierthon_VLIGHTUSERDATA:
      return touserdata(o);
    default: {
      if (iscollectable(o))
        return gcvalue(o);
      else
        return NULL;
    }
  }
}



/*
** push functions (C -> stack)
*/


ecierthon_API void ecierthon_pushnil (ecierthon_State *L) {
  ecierthon_lock(L);
  setnilvalue(s2v(L->top));
  api_incr_top(L);
  ecierthon_unlock(L);
}


ecierthon_API void ecierthon_pushnumber (ecierthon_State *L, ecierthon_Number n) {
  ecierthon_lock(L);
  setfltvalue(s2v(L->top), n);
  api_incr_top(L);
  ecierthon_unlock(L);
}


ecierthon_API void ecierthon_pushinteger (ecierthon_State *L, ecierthon_Integer n) {
  ecierthon_lock(L);
  setivalue(s2v(L->top), n);
  api_incr_top(L);
  ecierthon_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
ecierthon_API const char *ecierthon_pushlstring (ecierthon_State *L, const char *s, size_t len) {
  TString *ts;
  ecierthon_lock(L);
  ts = (len == 0) ? ecierthonS_new(L, "") : ecierthonS_newlstr(L, s, len);
  setsvalue2s(L, L->top, ts);
  api_incr_top(L);
  ecierthonC_checkGC(L);
  ecierthon_unlock(L);
  return getstr(ts);
}


ecierthon_API const char *ecierthon_pushstring (ecierthon_State *L, const char *s) {
  ecierthon_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top));
  else {
    TString *ts;
    ts = ecierthonS_new(L, s);
    setsvalue2s(L, L->top, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  ecierthonC_checkGC(L);
  ecierthon_unlock(L);
  return s;
}


ecierthon_API const char *ecierthon_pushvfstring (ecierthon_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  ecierthon_lock(L);
  ret = ecierthonO_pushvfstring(L, fmt, argp);
  ecierthonC_checkGC(L);
  ecierthon_unlock(L);
  return ret;
}


ecierthon_API const char *ecierthon_pushfstring (ecierthon_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  ecierthon_lock(L);
  va_start(argp, fmt);
  ret = ecierthonO_pushvfstring(L, fmt, argp);
  va_end(argp);
  ecierthonC_checkGC(L);
  ecierthon_unlock(L);
  return ret;
}


ecierthon_API void ecierthon_pushcclosure (ecierthon_State *L, ecierthon_CFunction fn, int n) {
  ecierthon_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top), fn);
    api_incr_top(L);
  }
  else {
    CClosure *cl;
    api_checknelems(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = ecierthonF_newCclosure(L, n);
    cl->f = fn;
    L->top -= n;
    while (n--) {
      setobj2n(L, &cl->upvalue[n], s2v(L->top + n));
      /* does not need barrier because closure is white */
      ecierthon_assert(iswhite(cl));
    }
    setclCvalue(L, s2v(L->top), cl);
    api_incr_top(L);
    ecierthonC_checkGC(L);
  }
  ecierthon_unlock(L);
}


ecierthon_API void ecierthon_pushboolean (ecierthon_State *L, int b) {
  ecierthon_lock(L);
  if (b)
    setbtvalue(s2v(L->top));
  else
    setbfvalue(s2v(L->top));
  api_incr_top(L);
  ecierthon_unlock(L);
}


ecierthon_API void ecierthon_pushlightuserdata (ecierthon_State *L, void *p) {
  ecierthon_lock(L);
  setpvalue(s2v(L->top), p);
  api_incr_top(L);
  ecierthon_unlock(L);
}


ecierthon_API int ecierthon_pushthread (ecierthon_State *L) {
  ecierthon_lock(L);
  setthvalue(L, s2v(L->top), L);
  api_incr_top(L);
  ecierthon_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (ecierthon -> stack)
*/


static int auxgetstr (ecierthon_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = ecierthonS_new(L, k);
  if (ecierthonV_fastget(L, t, str, slot, ecierthonH_getstr)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  }
  else {
    setsvalue2s(L, L->top, str);
    api_incr_top(L);
    ecierthonV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  ecierthon_unlock(L);
  return ttype(s2v(L->top - 1));
}


ecierthon_API int ecierthon_getglobal (ecierthon_State *L, const char *name) {
  Table *reg;
  ecierthon_lock(L);
  reg = hvalue(&G(L)->l_registry);
  return auxgetstr(L, ecierthonH_getint(reg, ecierthon_RIDX_GLOBALS), name);
}


ecierthon_API int ecierthon_gettable (ecierthon_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  ecierthon_lock(L);
  t = index2value(L, idx);
  if (ecierthonV_fastget(L, t, s2v(L->top - 1), slot, ecierthonH_get)) {
    setobj2s(L, L->top - 1, slot);
  }
  else
    ecierthonV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  ecierthon_unlock(L);
  return ttype(s2v(L->top - 1));
}


ecierthon_API int ecierthon_getfield (ecierthon_State *L, int idx, const char *k) {
  ecierthon_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


ecierthon_API int ecierthon_geti (ecierthon_State *L, int idx, ecierthon_Integer n) {
  TValue *t;
  const TValue *slot;
  ecierthon_lock(L);
  t = index2value(L, idx);
  if (ecierthonV_fastgeti(L, t, n, slot)) {
    setobj2s(L, L->top, slot);
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    ecierthonV_finishget(L, t, &aux, L->top, slot);
  }
  api_incr_top(L);
  ecierthon_unlock(L);
  return ttype(s2v(L->top - 1));
}


static int finishrawget (ecierthon_State *L, const TValue *val) {
  if (isempty(val))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top));
  else
    setobj2s(L, L->top, val);
  api_incr_top(L);
  ecierthon_unlock(L);
  return ttype(s2v(L->top - 1));
}


static Table *gettable (ecierthon_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


ecierthon_API int ecierthon_rawget (ecierthon_State *L, int idx) {
  Table *t;
  const TValue *val;
  ecierthon_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  val = ecierthonH_get(t, s2v(L->top - 1));
  L->top--;  /* remove key */
  return finishrawget(L, val);
}


ecierthon_API int ecierthon_rawgeti (ecierthon_State *L, int idx, ecierthon_Integer n) {
  Table *t;
  ecierthon_lock(L);
  t = gettable(L, idx);
  return finishrawget(L, ecierthonH_getint(t, n));
}


ecierthon_API int ecierthon_rawgetp (ecierthon_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  ecierthon_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, ecierthonH_get(t, &k));
}


ecierthon_API void ecierthon_createtable (ecierthon_State *L, int narray, int nrec) {
  Table *t;
  ecierthon_lock(L);
  t = ecierthonH_new(L);
  sethvalue2s(L, L->top, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    ecierthonH_resize(L, t, narray, nrec);
  ecierthonC_checkGC(L);
  ecierthon_unlock(L);
}


ecierthon_API int ecierthon_getmetatable (ecierthon_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  ecierthon_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case ecierthon_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case ecierthon_TUSERDATA:
      mt = uvalue(obj)->metatable;
      break;
    default:
      mt = G(L)->mt[ttype(obj)];
      break;
  }
  if (mt != NULL) {
    sethvalue2s(L, L->top, mt);
    api_incr_top(L);
    res = 1;
  }
  ecierthon_unlock(L);
  return res;
}


ecierthon_API int ecierthon_getiuservalue (ecierthon_State *L, int idx, int n) {
  TValue *o;
  int t;
  ecierthon_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top));
    t = ecierthon_TNONE;
  }
  else {
    setobj2s(L, L->top, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top));
  }
  api_incr_top(L);
  ecierthon_unlock(L);
  return t;
}


/*
** set functions (stack -> ecierthon)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (ecierthon_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = ecierthonS_new(L, k);
  api_checknelems(L, 1);
  if (ecierthonV_fastget(L, t, str, slot, ecierthonH_getstr)) {
    ecierthonV_finishfastset(L, t, slot, s2v(L->top - 1));
    L->top--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    ecierthonV_finishset(L, t, s2v(L->top - 1), s2v(L->top - 2), slot);
    L->top -= 2;  /* pop value and key */
  }
  ecierthon_unlock(L);  /* lock done by caller */
}


ecierthon_API void ecierthon_setglobal (ecierthon_State *L, const char *name) {
  Table *reg;
  ecierthon_lock(L);  /* unlock done in 'auxsetstr' */
  reg = hvalue(&G(L)->l_registry);
  auxsetstr(L, ecierthonH_getint(reg, ecierthon_RIDX_GLOBALS), name);
}


ecierthon_API void ecierthon_settable (ecierthon_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  ecierthon_lock(L);
  api_checknelems(L, 2);
  t = index2value(L, idx);
  if (ecierthonV_fastget(L, t, s2v(L->top - 2), slot, ecierthonH_get)) {
    ecierthonV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else
    ecierthonV_finishset(L, t, s2v(L->top - 2), s2v(L->top - 1), slot);
  L->top -= 2;  /* pop index and value */
  ecierthon_unlock(L);
}


ecierthon_API void ecierthon_setfield (ecierthon_State *L, int idx, const char *k) {
  ecierthon_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


ecierthon_API void ecierthon_seti (ecierthon_State *L, int idx, ecierthon_Integer n) {
  TValue *t;
  const TValue *slot;
  ecierthon_lock(L);
  api_checknelems(L, 1);
  t = index2value(L, idx);
  if (ecierthonV_fastgeti(L, t, n, slot)) {
    ecierthonV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    ecierthonV_finishset(L, t, &aux, s2v(L->top - 1), slot);
  }
  L->top--;  /* pop value */
  ecierthon_unlock(L);
}


static void aux_rawset (ecierthon_State *L, int idx, TValue *key, int n) {
  Table *t;
  TValue *slot;
  ecierthon_lock(L);
  api_checknelems(L, n);
  t = gettable(L, idx);
  slot = ecierthonH_set(L, t, key);
  setobj2t(L, slot, s2v(L->top - 1));
  invalidateTMcache(t);
  ecierthonC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top -= n;
  ecierthon_unlock(L);
}


ecierthon_API void ecierthon_rawset (ecierthon_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top - 2), 2);
}


ecierthon_API void ecierthon_rawsetp (ecierthon_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


ecierthon_API void ecierthon_rawseti (ecierthon_State *L, int idx, ecierthon_Integer n) {
  Table *t;
  ecierthon_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  ecierthonH_setint(L, t, n, s2v(L->top - 1));
  ecierthonC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top--;
  ecierthon_unlock(L);
}


ecierthon_API int ecierthon_setmetatable (ecierthon_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  ecierthon_lock(L);
  api_checknelems(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top - 1)), "table expected");
    mt = hvalue(s2v(L->top - 1));
  }
  switch (ttype(obj)) {
    case ecierthon_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        ecierthonC_objbarrier(L, gcvalue(obj), mt);
        ecierthonC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case ecierthon_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        ecierthonC_objbarrier(L, uvalue(obj), mt);
        ecierthonC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top--;
  ecierthon_unlock(L);
  return 1;
}


ecierthon_API int ecierthon_setiuservalue (ecierthon_State *L, int idx, int n) {
  TValue *o;
  int res;
  ecierthon_lock(L);
  api_checknelems(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top - 1));
    ecierthonC_barrierback(L, gcvalue(o), s2v(L->top - 1));
    res = 1;
  }
  L->top--;
  ecierthon_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run ecierthon code)
*/


#define checkresults(L,na,nr) \
     api_check(L, (nr) == ecierthon_MULTRET || (L->ci->top - L->top >= (nr) - (na)), \
	"results from function overflow current stack size")


ecierthon_API void ecierthon_callk (ecierthon_State *L, int nargs, int nresults,
                        ecierthon_KContext ctx, ecierthon_KFunction k) {
  StkId func;
  ecierthon_lock(L);
  api_check(L, k == NULL || !isecierthon(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == ecierthon_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    ecierthonD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    ecierthonD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  ecierthon_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (ecierthon_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  ecierthonD_callnoyield(L, c->func, c->nresults);
}



ecierthon_API int ecierthon_pcallk (ecierthon_State *L, int nargs, int nresults, int errfunc,
                        ecierthon_KContext ctx, ecierthon_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  ecierthon_lock(L);
  api_check(L, k == NULL || !isecierthon(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == ecierthon_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  if (errfunc == 0)
    func = 0;
  else {
    StkId o = index2stack(L, errfunc);
    api_check(L, ttisfunction(s2v(o)), "error handler must be a function");
    func = savestack(L, o);
  }
  c.func = L->top - (nargs+1);  /* function to be called */
  if (k == NULL || !yieldable(L)) {  /* no continuation or no yieldable? */
    c.nresults = nresults;  /* do a 'conventional' protected call */
    status = ecierthonD_pcall(L, f_call, &c, savestack(L, c.func), func);
  }
  else {  /* prepare continuation (call is already protected by 'resume') */
    CallInfo *ci = L->ci;
    ci->u.c.k = k;  /* save continuation */
    ci->u.c.ctx = ctx;  /* save context */
    /* save information for error recovery */
    ci->u2.funcidx = cast_int(savestack(L, c.func));
    ci->u.c.old_errfunc = L->errfunc;
    L->errfunc = func;
    setoah(ci->callstatus, L->allowhook);  /* save value of 'allowhook' */
    ci->callstatus |= CIST_YPCALL;  /* function can do error recovery */
    ecierthonD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = ecierthon_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  ecierthon_unlock(L);
  return status;
}


ecierthon_API int ecierthon_load (ecierthon_State *L, ecierthon_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  ecierthon_lock(L);
  if (!chunkname) chunkname = "?";
  ecierthonZ_init(L, &z, reader, data);
  status = ecierthonD_protectedparser(L, &z, chunkname, mode);
  if (status == ecierthon_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top - 1));  /* get newly created function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      Table *reg = hvalue(&G(L)->l_registry);
      const TValue *gt = ecierthonH_getint(reg, ecierthon_RIDX_GLOBALS);
      /* set global table as 1st upvalue of 'f' (may be ecierthon_ENV) */
      setobj(L, f->upvals[0]->v, gt);
      ecierthonC_barrier(L, f->upvals[0], gt);
    }
  }
  ecierthon_unlock(L);
  return status;
}


ecierthon_API int ecierthon_dump (ecierthon_State *L, ecierthon_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  ecierthon_lock(L);
  api_checknelems(L, 1);
  o = s2v(L->top - 1);
  if (isLfunction(o))
    status = ecierthonU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  ecierthon_unlock(L);
  return status;
}


ecierthon_API int ecierthon_status (ecierthon_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/
ecierthon_API int ecierthon_gc (ecierthon_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g;
  ecierthon_lock(L);
  g = G(L);
  va_start(argp, what);
  switch (what) {
    case ecierthon_GCSTOP: {
      g->gcrunning = 0;
      break;
    }
    case ecierthon_GCRESTART: {
      ecierthonE_setdebt(g, 0);
      g->gcrunning = 1;
      break;
    }
    case ecierthon_GCCOLLECT: {
      ecierthonC_fullgc(L, 0);
      break;
    }
    case ecierthon_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case ecierthon_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case ecierthon_GCSTEP: {
      int data = va_arg(argp, int);
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldrunning = g->gcrunning;
      g->gcrunning = 1;  /* allow GC to run */
      if (data == 0) {
        ecierthonE_setdebt(g, 0);  /* do a basic step */
        ecierthonC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = cast(l_mem, data) * 1024 + g->GCdebt;
        ecierthonE_setdebt(g, debt);
        ecierthonC_checkGC(L);
      }
      g->gcrunning = oldrunning;  /* restore previous state */
      if (debt > 0 && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case ecierthon_GCSETPAUSE: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcpause);
      setgcparam(g->gcpause, data);
      break;
    }
    case ecierthon_GCSETSTEPMUL: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcstepmul);
      setgcparam(g->gcstepmul, data);
      break;
    }
    case ecierthon_GCISRUNNING: {
      res = g->gcrunning;
      break;
    }
    case ecierthon_GCGEN: {
      int minormul = va_arg(argp, int);
      int majormul = va_arg(argp, int);
      res = isdecGCmodegen(g) ? ecierthon_GCGEN : ecierthon_GCINC;
      if (minormul != 0)
        g->genminormul = minormul;
      if (majormul != 0)
        setgcparam(g->genmajormul, majormul);
      ecierthonC_changemode(L, KGC_GEN);
      break;
    }
    case ecierthon_GCINC: {
      int pause = va_arg(argp, int);
      int stepmul = va_arg(argp, int);
      int stepsize = va_arg(argp, int);
      res = isdecGCmodegen(g) ? ecierthon_GCGEN : ecierthon_GCINC;
      if (pause != 0)
        setgcparam(g->gcpause, pause);
      if (stepmul != 0)
        setgcparam(g->gcstepmul, stepmul);
      if (stepsize != 0)
        g->gcstepsize = stepsize;
      ecierthonC_changemode(L, KGC_INC);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  ecierthon_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


ecierthon_API int ecierthon_error (ecierthon_State *L) {
  TValue *errobj;
  ecierthon_lock(L);
  errobj = s2v(L->top - 1);
  api_checknelems(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    ecierthonM_error(L);  /* raise a memory error */
  else
    ecierthonG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


ecierthon_API int ecierthon_next (ecierthon_State *L, int idx) {
  Table *t;
  int more;
  ecierthon_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  more = ecierthonH_next(L, t, L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  ecierthon_unlock(L);
  return more;
}


ecierthon_API void ecierthon_toclose (ecierthon_State *L, int idx) {
  int nresults;
  StkId o;
  ecierthon_lock(L);
  o = index2stack(L, idx);
  nresults = L->ci->nresults;
  api_check(L, L->openupval == NULL || uplevel(L->openupval) <= o,
               "marked index below or equal new one");
  ecierthonF_newtbcupval(L, o);  /* create new to-be-closed upvalue */
  if (!hastocloseCfunc(nresults))  /* function not marked yet? */
    L->ci->nresults = codeNresults(nresults);  /* mark it */
  ecierthon_assert(hastocloseCfunc(L->ci->nresults));
  ecierthon_unlock(L);
}


ecierthon_API void ecierthon_concat (ecierthon_State *L, int n) {
  ecierthon_lock(L);
  api_checknelems(L, n);
  if (n > 0)
    ecierthonV_concat(L, n);
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top, ecierthonS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  ecierthonC_checkGC(L);
  ecierthon_unlock(L);
}


ecierthon_API void ecierthon_len (ecierthon_State *L, int idx) {
  TValue *t;
  ecierthon_lock(L);
  t = index2value(L, idx);
  ecierthonV_objlen(L, L->top, t);
  api_incr_top(L);
  ecierthon_unlock(L);
}


ecierthon_API ecierthon_Alloc ecierthon_getallocf (ecierthon_State *L, void **ud) {
  ecierthon_Alloc f;
  ecierthon_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  ecierthon_unlock(L);
  return f;
}


ecierthon_API void ecierthon_setallocf (ecierthon_State *L, ecierthon_Alloc f, void *ud) {
  ecierthon_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  ecierthon_unlock(L);
}


void ecierthon_setwarnf (ecierthon_State *L, ecierthon_WarnFunction f, void *ud) {
  ecierthon_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  ecierthon_unlock(L);
}


void ecierthon_warning (ecierthon_State *L, const char *msg, int tocont) {
  ecierthon_lock(L);
  ecierthonE_warning(L, msg, tocont);
  ecierthon_unlock(L);
}



ecierthon_API void *ecierthon_newuserdatauv (ecierthon_State *L, size_t size, int nuvalue) {
  Udata *u;
  ecierthon_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < USHRT_MAX, "invalid value");
  u = ecierthonS_newudata(L, size, nuvalue);
  setuvalue(L, s2v(L->top), u);
  api_incr_top(L);
  ecierthonC_checkGC(L);
  ecierthon_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case ecierthon_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case ecierthon_VLCL: {  /* ecierthon closure */
      LClosure *f = clLvalue(fi);
      TString *name;
      Proto *p = f->p;
      if (!(cast_uint(n) - 1u  < cast_uint(p->sizeupvalues)))
        return NULL;  /* 'n' not in [1, p->sizeupvalues] */
      *val = f->upvals[n-1]->v;
      if (owner) *owner = obj2gco(f->upvals[n - 1]);
      name = p->upvalues[n-1].name;
      return (name == NULL) ? "(no name)" : getstr(name);
    }
    default: return NULL;  /* not a closure */
  }
}


ecierthon_API const char *ecierthon_getupvalue (ecierthon_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  ecierthon_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  ecierthon_unlock(L);
  return name;
}


ecierthon_API const char *ecierthon_setupvalue (ecierthon_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  ecierthon_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top--;
    setobj(L, val, s2v(L->top));
    ecierthonC_barrier(L, owner, val);
  }
  ecierthon_unlock(L);
  return name;
}


static UpVal **getupvalref (ecierthon_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "ecierthon function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


ecierthon_API void *ecierthon_upvalueid (ecierthon_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case ecierthon_VLCL: {  /* ecierthon closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case ecierthon_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case ecierthon_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


ecierthon_API void ecierthon_upvaluejoin (ecierthon_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  ecierthonC_objbarrier(L, f1, *up1);
}


