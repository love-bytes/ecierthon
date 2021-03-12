/*
** $Id: lfunc.c $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in ecierthon.h
*/

#define lfunc_c
#define ecierthon_CORE

#include "lprefix.h"


#include <stddef.h>

#include "ecierthon.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



CClosure *ecierthonF_newCclosure (ecierthon_State *L, int nupvals) {
  GCObject *o = ecierthonC_newobj(L, ecierthon_VCCL, sizeCclosure(nupvals));
  CClosure *c = gco2ccl(o);
  c->nupvalues = cast_byte(nupvals);
  return c;
}


LClosure *ecierthonF_newLclosure (ecierthon_State *L, int nupvals) {
  GCObject *o = ecierthonC_newobj(L, ecierthon_VLCL, sizeLclosure(nupvals));
  LClosure *c = gco2lcl(o);
  c->p = NULL;
  c->nupvalues = cast_byte(nupvals);
  while (nupvals--) c->upvals[nupvals] = NULL;
  return c;
}


/*
** fill a closure with new closed upvalues
*/
void ecierthonF_initupvals (ecierthon_State *L, LClosure *cl) {
  int i;
  for (i = 0; i < cl->nupvalues; i++) {
    GCObject *o = ecierthonC_newobj(L, ecierthon_VUPVAL, sizeof(UpVal));
    UpVal *uv = gco2upv(o);
    uv->v = &uv->u.value;  /* make it closed */
    setnilvalue(uv->v);
    cl->upvals[i] = uv;
    ecierthonC_objbarrier(L, cl, uv);
  }
}


/*
** Create a new upvalue at the given level, and link it to the list of
** open upvalues of 'L' after entry 'prev'.
**/
static UpVal *newupval (ecierthon_State *L, int tbc, StkId level, UpVal **prev) {
  GCObject *o = ecierthonC_newobj(L, ecierthon_VUPVAL, sizeof(UpVal));
  UpVal *uv = gco2upv(o);
  UpVal *next = *prev;
  uv->v = s2v(level);  /* current value lives in the stack */
  uv->tbc = tbc;
  uv->u.open.next = next;  /* link it to list of open upvalues */
  uv->u.open.previous = prev;
  if (next)
    next->u.open.previous = &uv->u.open.next;
  *prev = uv;
  if (!isintwups(L)) {  /* thread not in list of threads with upvalues? */
    L->twups = G(L)->twups;  /* link it to the list */
    G(L)->twups = L;
  }
  return uv;
}


/*
** Find and reuse, or create if it does not exist, an upvalue
** at the given level.
*/
UpVal *ecierthonF_findupval (ecierthon_State *L, StkId level) {
  UpVal **pp = &L->openupval;
  UpVal *p;
  ecierthon_assert(isintwups(L) || L->openupval == NULL);
  while ((p = *pp) != NULL && uplevel(p) >= level) {  /* search for it */
    ecierthon_assert(!isdead(G(L), p));
    if (uplevel(p) == level)  /* corresponding upvalue? */
      return p;  /* return it */
    pp = &p->u.open.next;
  }
  /* not found: create a new upvalue after 'pp' */
  return newupval(L, 0, level, pp);
}


static void callclose (ecierthon_State *L, void *ud) {
  UNUSED(ud);
  ecierthonD_callnoyield(L, L->top - 3, 0);
}


/*
** Prepare closing method plus its arguments for object 'obj' with
** error message 'err'. (This function assumes EXTRA_STACK.)
*/
static int prepclosingmethod (ecierthon_State *L, TValue *obj, TValue *err) {
  StkId top = L->top;
  const TValue *tm = ecierthonT_gettmbyobj(L, obj, TM_CLOSE);
  if (ttisnil(tm))  /* no metamethod? */
    return 0;  /* nothing to call */
  setobj2s(L, top, tm);  /* will call metamethod... */
  setobj2s(L, top + 1, obj);  /* with 'self' as the 1st argument */
  setobj2s(L, top + 2, err);  /* and error msg. as 2nd argument */
  L->top = top + 3;  /* add function and arguments */
  return 1;
}


/*
** Raise an error with message 'msg', inserting the name of the
** local variable at position 'level' in the stack.
*/
static void varerror (ecierthon_State *L, StkId level, const char *msg) {
  int idx = cast_int(level - L->ci->func);
  const char *vname = ecierthonG_findlocal(L, L->ci, idx, NULL);
  if (vname == NULL) vname = "?";
  ecierthonG_runerror(L, msg, vname);
}


/*
** Prepare and call a closing method. If status is OK, code is still
** inside the original protected call, and so any error will be handled
** there. Otherwise, a previous error already activated the original
** protected call, and so the call to the closing method must be
** protected here. (A status == CLOSEPROTECT behaves like a previous
** error, to also run the closing method in protected mode).
** If status is OK, the call to the closing method will be pushed
** at the top of the stack. Otherwise, values are pushed after
** the 'level' of the upvalue being closed, as everything after
** that won't be used again.
*/
static int callclosemth (ecierthon_State *L, StkId level, int status) {
  TValue *uv = s2v(level);  /* value being closed */
  if (likely(status == ecierthon_OK)) {
    if (prepclosingmethod(L, uv, &G(L)->nilvalue))  /* something to call? */
      callclose(L, NULL);  /* call closing method */
    else if (!l_isfalse(uv))  /* non-closable non-false value? */
      varerror(L, level, "attempt to close non-closable variable '%s'");
  }
  else {  /* must close the object in protected mode */
    ptrdiff_t oldtop;
    level++;  /* space for error message */
    oldtop = savestack(L, level + 1);  /* top will be after that */
    ecierthonD_seterrorobj(L, status, level);  /* set error message */
    if (prepclosingmethod(L, uv, s2v(level))) {  /* something to call? */
      int newstatus = ecierthonD_pcall(L, callclose, NULL, oldtop, 0);
      if (newstatus != ecierthon_OK && status == CLOSEPROTECT)  /* first error? */
        status = newstatus;  /* this will be the new error */
      else {
        if (newstatus != ecierthon_OK)  /* suppressed error? */
          ecierthonE_warnerror(L, "__close metamethod");
        /* leave original error (or nil) on top */
        L->top = restorestack(L, oldtop);
      }
    }
    /* else no metamethod; ignore this case and keep original error */
  }
  return status;
}


/*
** Try to create a to-be-closed upvalue
** (can raise a memory-allocation error)
*/
static void trynewtbcupval (ecierthon_State *L, void *ud) {
  newupval(L, 1, cast(StkId, ud), &L->openupval);
}


/*
** Create a to-be-closed upvalue. If there is a memory error
** when creating the upvalue, the closing method must be called here,
** as there is no upvalue to call it later.
*/
void ecierthonF_newtbcupval (ecierthon_State *L, StkId level) {
  TValue *obj = s2v(level);
  ecierthon_assert(L->openupval == NULL || uplevel(L->openupval) < level);
  if (!l_isfalse(obj)) {  /* false doesn't need to be closed */
    int status;
    const TValue *tm = ecierthonT_gettmbyobj(L, obj, TM_CLOSE);
    if (ttisnil(tm))  /* no metamethod? */
      varerror(L, level, "variable '%s' got a non-closable value");
    status = ecierthonD_rawrunprotected(L, trynewtbcupval, level);
    if (unlikely(status != ecierthon_OK)) {  /* memory error creating upvalue? */
      ecierthon_assert(status == ecierthon_ERRMEM);
      ecierthonD_seterrorobj(L, ecierthon_ERRMEM, level + 1);  /* save error message */
      /* next call must succeed, as object is closable */
      prepclosingmethod(L, s2v(level), s2v(level + 1));
      callclose(L, NULL);  /* call closing method */
      ecierthonD_throw(L, ecierthon_ERRMEM);  /* throw memory error */
    }
  }
}


void ecierthonF_unlinkupval (UpVal *uv) {
  ecierthon_assert(upisopen(uv));
  *uv->u.open.previous = uv->u.open.next;
  if (uv->u.open.next)
    uv->u.open.next->u.open.previous = uv->u.open.previous;
}


int ecierthonF_close (ecierthon_State *L, StkId level, int status) {
  UpVal *uv;
  while ((uv = L->openupval) != NULL && uplevel(uv) >= level) {
    TValue *slot = &uv->u.value;  /* new position for value */
    ecierthon_assert(uplevel(uv) < L->top);
    if (uv->tbc && status != NOCLOSINGMETH) {
      /* must run closing method, which may change the stack */
      ptrdiff_t levelrel = savestack(L, level);
      status = callclosemth(L, uplevel(uv), status);
      level = restorestack(L, levelrel);
    }
    ecierthonF_unlinkupval(uv);
    setobj(L, slot, uv->v);  /* move value to upvalue slot */
    uv->v = slot;  /* now current value lives here */
    if (!iswhite(uv)) {  /* neither white nor dead? */
      nw2black(uv);  /* closed upvalues cannot be gray */
      ecierthonC_barrier(L, uv, slot);
    }
  }
  return status;
}


Proto *ecierthonF_newproto (ecierthon_State *L) {
  GCObject *o = ecierthonC_newobj(L, ecierthon_VPROTO, sizeof(Proto));
  Proto *f = gco2p(o);
  f->k = NULL;
  f->sizek = 0;
  f->p = NULL;
  f->sizep = 0;
  f->code = NULL;
  f->sizecode = 0;
  f->lineinfo = NULL;
  f->sizelineinfo = 0;
  f->abslineinfo = NULL;
  f->sizeabslineinfo = 0;
  f->upvalues = NULL;
  f->sizeupvalues = 0;
  f->numparams = 0;
  f->is_vararg = 0;
  f->maxstacksize = 0;
  f->locvars = NULL;
  f->sizelocvars = 0;
  f->linedefined = 0;
  f->lastlinedefined = 0;
  f->source = NULL;
  return f;
}


void ecierthonF_freeproto (ecierthon_State *L, Proto *f) {
  ecierthonM_freearray(L, f->code, f->sizecode);
  ecierthonM_freearray(L, f->p, f->sizep);
  ecierthonM_freearray(L, f->k, f->sizek);
  ecierthonM_freearray(L, f->lineinfo, f->sizelineinfo);
  ecierthonM_freearray(L, f->abslineinfo, f->sizeabslineinfo);
  ecierthonM_freearray(L, f->locvars, f->sizelocvars);
  ecierthonM_freearray(L, f->upvalues, f->sizeupvalues);
  ecierthonM_free(L, f);
}


/*
** Look for n-th local variable at line 'line' in function 'func'.
** Returns NULL if not found.
*/
const char *ecierthonF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i = 0; i<f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {  /* is variable active? */
      local_number--;
      if (local_number == 0)
        return getstr(f->locvars[i].varname);
    }
  }
  return NULL;  /* not found */
}

