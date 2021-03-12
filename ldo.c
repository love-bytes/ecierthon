/*
** $Id: ldo.c $
** Stack and Call structure of ecierthon
** See Copyright Notice in ecierthon.h
*/

#define ldo_c
#define ecierthon_CORE

#include "lprefix.h"


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "ecierthon.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"



#define errorstatus(s)	((s) > ecierthon_YIELD)


/*
** {======================================================
** Error-recovery functions
** =======================================================
*/

/*
** ecierthonI_THROW/ecierthonI_TRY define how ecierthon does exception handling. By
** default, ecierthon handles errors with exceptions when compiling as
** C++ code, with _longjmp/_setjmp when asked to use them, and with
** longjmp/setjmp otherwise.
*/
#if !defined(ecierthonI_THROW)				/* { */

#if defined(__cplusplus) && !defined(ecierthon_USE_LONGJMP)	/* { */

/* C++ exceptions */
#define ecierthonI_THROW(L,c)		throw(c)
#define ecierthonI_TRY(L,c,a) \
	try { a } catch(...) { if ((c)->status == 0) (c)->status = -1; }
#define ecierthoni_jmpbuf		int  /* dummy variable */

#elif defined(ecierthon_USE_POSIX)				/* }{ */

/* in POSIX, try _longjmp/_setjmp (more efficient) */
#define ecierthonI_THROW(L,c)		_longjmp((c)->b, 1)
#define ecierthonI_TRY(L,c,a)		if (_setjmp((c)->b) == 0) { a }
#define ecierthoni_jmpbuf		jmp_buf

#else							/* }{ */

/* ISO C handling with long jumps */
#define ecierthonI_THROW(L,c)		longjmp((c)->b, 1)
#define ecierthonI_TRY(L,c,a)		if (setjmp((c)->b) == 0) { a }
#define ecierthoni_jmpbuf		jmp_buf

#endif							/* } */

#endif							/* } */



/* chain list of long jump buffers */
struct ecierthon_longjmp {
  struct ecierthon_longjmp *previous;
  ecierthoni_jmpbuf b;
  volatile int status;  /* error code */
};


void ecierthonD_seterrorobj (ecierthon_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case ecierthon_ERRMEM: {  /* memory error? */
      setsvalue2s(L, oldtop, G(L)->memerrmsg); /* reuse preregistered msg. */
      break;
    }
    case ecierthon_ERRERR: {
      setsvalue2s(L, oldtop, ecierthonS_newliteral(L, "error in error handling"));
      break;
    }
    case CLOSEPROTECT: {
      setnilvalue(s2v(oldtop));  /* no error message */
      break;
    }
    default: {
      setobjs2s(L, oldtop, L->top - 1);  /* error message on current top */
      break;
    }
  }
  L->top = oldtop + 1;
}


l_noret ecierthonD_throw (ecierthon_State *L, int errcode) {
  if (L->errorJmp) {  /* thread has an error handler? */
    L->errorJmp->status = errcode;  /* set status */
    ecierthonI_THROW(L, L->errorJmp);  /* jump to it */
  }
  else {  /* thread has no error handler */
    global_State *g = G(L);
    errcode = ecierthonF_close(L, L->stack, errcode);  /* close all upvalues */
    L->status = cast_byte(errcode);  /* mark it as dead */
    if (g->mainthread->errorJmp) {  /* main thread has a handler? */
      setobjs2s(L, g->mainthread->top++, L->top - 1);  /* copy error obj. */
      ecierthonD_throw(g->mainthread, errcode);  /* re-throw in main thread */
    }
    else {  /* no handler at all; abort */
      if (g->panic) {  /* panic function? */
        ecierthonD_seterrorobj(L, errcode, L->top);  /* assume EXTRA_STACK */
        if (L->ci->top < L->top)
          L->ci->top = L->top;  /* pushing msg. can break this invariant */
        ecierthon_unlock(L);
        g->panic(L);  /* call panic function (last chance to jump out) */
      }
      abort();
    }
  }
}


int ecierthonD_rawrunprotected (ecierthon_State *L, Pfunc f, void *ud) {
  l_uint32 oldnCcalls = L->nCcalls;
  struct ecierthon_longjmp lj;
  lj.status = ecierthon_OK;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  ecierthonI_TRY(L, &lj,
    (*f)(L, ud);
  );
  L->errorJmp = lj.previous;  /* restore old error handler */
  L->nCcalls = oldnCcalls;
  return lj.status;
}

/* }====================================================== */


/*
** {==================================================================
** Stack reallocation
** ===================================================================
*/
static void correctstack (ecierthon_State *L, StkId oldstack, StkId newstack) {
  CallInfo *ci;
  UpVal *up;
  if (oldstack == newstack)
    return;  /* stack address did not change */
  L->top = (L->top - oldstack) + newstack;
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v = s2v((uplevel(up) - oldstack) + newstack);
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top = (ci->top - oldstack) + newstack;
    ci->func = (ci->func - oldstack) + newstack;
    if (isecierthon(ci))
      ci->u.l.trap = 1;  /* signal to update 'trap' in 'ecierthonV_execute' */
  }
}


/* some space for error handling */
#define ERRORSTACKSIZE	(ecierthonI_MAXSTACK + 200)


int ecierthonD_reallocstack (ecierthon_State *L, int newsize, int raiseerror) {
  int lim = stacksize(L);
  StkId newstack = ecierthonM_reallocvector(L, L->stack,
                      lim + EXTRA_STACK, newsize + EXTRA_STACK, StackValue);
  ecierthon_assert(newsize <= ecierthonI_MAXSTACK || newsize == ERRORSTACKSIZE);
  if (unlikely(newstack == NULL)) {  /* reallocation failed? */
    if (raiseerror)
      ecierthonM_error(L);
    else return 0;  /* do not raise an error */
  }
  for (; lim < newsize; lim++)
    setnilvalue(s2v(newstack + lim + EXTRA_STACK)); /* erase new segment */
  correctstack(L, L->stack, newstack);
  L->stack = newstack;
  L->stack_last = L->stack + newsize;
  return 1;
}


/*
** Try to grow the stack by at least 'n' elements. when 'raiseerror'
** is true, raises any error; otherwise, return 0 in case of errors.
*/
int ecierthonD_growstack (ecierthon_State *L, int n, int raiseerror) {
  int size = stacksize(L);
  if (unlikely(size > ecierthonI_MAXSTACK)) {
    /* if stack is larger than maximum, thread is already using the
       extra space reserved for errors, that is, thread is handling
       a stack error; cannot grow further than that. */
    ecierthon_assert(stacksize(L) == ERRORSTACKSIZE);
    if (raiseerror)
      ecierthonD_throw(L, ecierthon_ERRERR);  /* error inside message handler */
    return 0;  /* if not 'raiseerror', just signal it */
  }
  else {
    int newsize = 2 * size;  /* tentative new size */
    int needed = cast_int(L->top - L->stack) + n;
    if (newsize > ecierthonI_MAXSTACK)  /* cannot cross the limit */
      newsize = ecierthonI_MAXSTACK;
    if (newsize < needed)  /* but must respect what was asked for */
      newsize = needed;
    if (likely(newsize <= ecierthonI_MAXSTACK))
      return ecierthonD_reallocstack(L, newsize, raiseerror);
    else {  /* stack overflow */
      /* add extra size to be able to handle the error message */
      ecierthonD_reallocstack(L, ERRORSTACKSIZE, raiseerror);
      if (raiseerror)
        ecierthonG_runerror(L, "stack overflow");
      return 0;
    }
  }
}


static int stackinuse (ecierthon_State *L) {
  CallInfo *ci;
  int res;
  StkId lim = L->top;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    if (lim < ci->top) lim = ci->top;
  }
  ecierthon_assert(lim <= L->stack_last);
  res = cast_int(lim - L->stack) + 1;  /* part of stack in use */
  if (res < ecierthon_MINSTACK)
    res = ecierthon_MINSTACK;  /* ensure a minimum size */
  return res;
}


/*
** If stack size is more than 3 times the current use, reduce that size
** to twice the current use. (So, the final stack size is at most 2/3 the
** previous size, and half of its entries are empty.)
** As a particular case, if stack was handling a stack overflow and now
** it is not, 'max' (limited by ecierthonI_MAXSTACK) will be smaller than
** stacksize (equal to ERRORSTACKSIZE in this case), and so the stack
** will be reduced to a "regular" size.
*/
void ecierthonD_shrinkstack (ecierthon_State *L) {
  int inuse = stackinuse(L);
  int nsize = inuse * 2;  /* proposed new size */
  int max = inuse * 3;  /* maximum "reasonable" size */
  if (max > ecierthonI_MAXSTACK) {
    max = ecierthonI_MAXSTACK;  /* respect stack limit */
    if (nsize > ecierthonI_MAXSTACK)
      nsize = ecierthonI_MAXSTACK;
  }
  /* if thread is currently not handling a stack overflow and its
     size is larger than maximum "reasonable" size, shrink it */
  if (inuse <= ecierthonI_MAXSTACK && stacksize(L) > max)
    ecierthonD_reallocstack(L, nsize, 0);  /* ok if that fails */
  else  /* don't change stack */
    condmovestack(L,{},{});  /* (change only for debugging) */
  ecierthonE_shrinkCI(L);  /* shrink CI list */
}


void ecierthonD_inctop (ecierthon_State *L) {
  ecierthonD_checkstack(L, 1);
  L->top++;
}

/* }================================================================== */


/*
** Call a hook for the given event. Make sure there is a hook to be
** called. (Both 'L->hook' and 'L->hookmask', which trigger this
** function, can be changed asynchronously by signals.)
*/
void ecierthonD_hook (ecierthon_State *L, int event, int line,
                              int ftransfer, int ntransfer) {
  ecierthon_Hook hook = L->hook;
  if (hook && L->allowhook) {  /* make sure there is a hook */
    int mask = CIST_HOOKED;
    CallInfo *ci = L->ci;
    ptrdiff_t top = savestack(L, L->top);
    ptrdiff_t ci_top = savestack(L, ci->top);
    ecierthon_Debug ar;
    ar.event = event;
    ar.currentline = line;
    ar.i_ci = ci;
    if (ntransfer != 0) {
      mask |= CIST_TRAN;  /* 'ci' has transfer information */
      ci->u2.transferinfo.ftransfer = ftransfer;
      ci->u2.transferinfo.ntransfer = ntransfer;
    }
    ecierthonD_checkstack(L, ecierthon_MINSTACK);  /* ensure minimum stack size */
    if (L->top + ecierthon_MINSTACK > ci->top)
      ci->top = L->top + ecierthon_MINSTACK;
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    ci->callstatus |= mask;
    ecierthon_unlock(L);
    (*hook)(L, &ar);
    ecierthon_lock(L);
    ecierthon_assert(!L->allowhook);
    L->allowhook = 1;
    ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
    ci->callstatus &= ~mask;
  }
}


/*
** Executes a call hook for ecierthon functions. This function is called
** whenever 'hookmask' is not zero, so it checks whether call hooks are
** active.
*/
void ecierthonD_hookcall (ecierthon_State *L, CallInfo *ci) {
  int hook = (ci->callstatus & CIST_TAIL) ? ecierthon_HOOKTAILCALL : ecierthon_HOOKCALL;
  Proto *p;
  if (!(L->hookmask & ecierthon_MASKCALL))  /* some other hook? */
    return;  /* don't call hook */
  p = clLvalue(s2v(ci->func))->p;
  L->top = ci->top;  /* prepare top */
  ci->u.l.savedpc++;  /* hooks assume 'pc' is already incremented */
  ecierthonD_hook(L, hook, -1, 1, p->numparams);
  ci->u.l.savedpc--;  /* correct 'pc' */
}


static StkId rethook (ecierthon_State *L, CallInfo *ci, StkId firstres, int nres) {
  ptrdiff_t oldtop = savestack(L, L->top);  /* hook may change top */
  int delta = 0;
  if (isecierthoncode(ci)) {
    Proto *p = ci_func(ci)->p;
    if (p->is_vararg)
      delta = ci->u.l.nextraargs + p->numparams + 1;
    if (L->top < ci->top)
      L->top = ci->top;  /* correct top to run hook */
  }
  if (L->hookmask & ecierthon_MASKRET) {  /* is return hook on? */
    int ftransfer;
    ci->func += delta;  /* if vararg, back to virtual 'func' */
    ftransfer = cast(unsigned short, firstres - ci->func);
    ecierthonD_hook(L, ecierthon_HOOKRET, -1, ftransfer, nres);  /* call it */
    ci->func -= delta;
  }
  if (isecierthon(ci = ci->previous))
    L->oldpc = pcRel(ci->u.l.savedpc, ci_func(ci)->p);  /* update 'oldpc' */
  return restorestack(L, oldtop);
}


/*
** Check whether 'func' has a '__call' metafield. If so, put it in the
** stack, below original 'func', so that 'ecierthonD_precall' can call it. Raise
** an error if there is no '__call' metafield.
*/
void ecierthonD_tryfuncTM (ecierthon_State *L, StkId func) {
  const TValue *tm = ecierthonT_gettmbyobj(L, s2v(func), TM_CALL);
  StkId p;
  if (unlikely(ttisnil(tm)))
    ecierthonG_typeerror(L, s2v(func), "call");  /* nothing to call */
  for (p = L->top; p > func; p--)  /* open space for metamethod */
    setobjs2s(L, p, p-1);
  L->top++;  /* stack space pre-allocated by the caller */
  setobj2s(L, func, tm);  /* metamethod is the new function to be called */
}


/*
** Given 'nres' results at 'firstResult', move 'wanted' of them to 'res'.
** Handle most typical cases (zero results for commands, one result for
** expressions, multiple results for tail calls/single parameters)
** separated.
*/
static void moveresults (ecierthon_State *L, StkId res, int nres, int wanted) {
  StkId firstresult;
  int i;
  switch (wanted) {  /* handle typical cases separately */
    case 0:  /* no values needed */
      L->top = res;
      return;
    case 1:  /* one value needed */
      if (nres == 0)   /* no results? */
        setnilvalue(s2v(res));  /* adjust with nil */
      else
        setobjs2s(L, res, L->top - nres);  /* move it to proper place */
      L->top = res + 1;
      return;
    case ecierthon_MULTRET:
      wanted = nres;  /* we want all results */
      break;
    default:  /* multiple results (or to-be-closed variables) */
      if (hastocloseCfunc(wanted)) {  /* to-be-closed variables? */
        ptrdiff_t savedres = savestack(L, res);
        ecierthonF_close(L, res, ecierthon_OK);  /* may change the stack */
        res = restorestack(L, savedres);
        wanted = codeNresults(wanted);  /* correct value */
        if (wanted == ecierthon_MULTRET)
          wanted = nres;
      }
      break;
  }
  firstresult = L->top - nres;  /* index of first result */
  /* move all results to correct place */
  for (i = 0; i < nres && i < wanted; i++)
    setobjs2s(L, res + i, firstresult + i);
  for (; i < wanted; i++)  /* complete wanted number of results */
    setnilvalue(s2v(res + i));
  L->top = res + wanted;  /* top points after the last result */
}


/*
** Finishes a function call: calls hook if necessary, removes CallInfo,
** moves current number of results to proper place.
*/
void ecierthonD_poscall (ecierthon_State *L, CallInfo *ci, int nres) {
  if (L->hookmask)
    L->top = rethook(L, ci, L->top - nres, nres);
  L->ci = ci->previous;  /* back to caller */
  /* move results to proper place */
  moveresults(L, ci->func, nres, ci->nresults);
}



#define next_ci(L)  (L->ci->next ? L->ci->next : ecierthonE_extendCI(L))


/*
** Prepare a function for a tail call, building its call info on top
** of the current call info. 'narg1' is the number of arguments plus 1
** (so that it includes the function itself).
*/
void ecierthonD_pretailcall (ecierthon_State *L, CallInfo *ci, StkId func, int narg1) {
  Proto *p = clLvalue(s2v(func))->p;
  int fsize = p->maxstacksize;  /* frame size */
  int nfixparams = p->numparams;
  int i;
  for (i = 0; i < narg1; i++)  /* move down function and arguments */
    setobjs2s(L, ci->func + i, func + i);
  checkstackGC(L, fsize);
  func = ci->func;  /* moved-down function */
  for (; narg1 <= nfixparams; narg1++)
    setnilvalue(s2v(func + narg1));  /* complete missing arguments */
  ci->top = func + 1 + fsize;  /* top for new function */
  ecierthon_assert(ci->top <= L->stack_last);
  ci->u.l.savedpc = p->code;  /* starting point */
  ci->callstatus |= CIST_TAIL;
  L->top = func + narg1;  /* set top */
}


/*
** Prepares the call to a function (C or ecierthon). For C functions, also do
** the call. The function to be called is at '*func'.  The arguments
** are on the stack, right after the function.  Returns the CallInfo
** to be executed, if it was a ecierthon function. Otherwise (a C function)
** returns NULL, with all the results on the stack, starting at the
** original function position.
*/
CallInfo *ecierthonD_precall (ecierthon_State *L, StkId func, int nresults) {
  ecierthon_CFunction f;
 retry:
  switch (ttypetag(s2v(func))) {
    case ecierthon_VCCL:  /* C closure */
      f = clCvalue(s2v(func))->f;
      goto Cfunc;
    case ecierthon_VLCF:  /* light C function */
      f = fvalue(s2v(func));
     Cfunc: {
      int n;  /* number of returns */
      CallInfo *ci;
      checkstackGCp(L, ecierthon_MINSTACK, func);  /* ensure minimum stack size */
      L->ci = ci = next_ci(L);
      ci->nresults = nresults;
      ci->callstatus = CIST_C;
      ci->top = L->top + ecierthon_MINSTACK;
      ci->func = func;
      ecierthon_assert(ci->top <= L->stack_last);
      if (L->hookmask & ecierthon_MASKCALL) {
        int narg = cast_int(L->top - func) - 1;
        ecierthonD_hook(L, ecierthon_HOOKCALL, -1, 1, narg);
      }
      ecierthon_unlock(L);
      n = (*f)(L);  /* do the actual call */
      ecierthon_lock(L);
      api_checknelems(L, n);
      ecierthonD_poscall(L, ci, n);
      return NULL;
    }
    case ecierthon_VLCL: {  /* ecierthon function */
      CallInfo *ci;
      Proto *p = clLvalue(s2v(func))->p;
      int narg = cast_int(L->top - func) - 1;  /* number of real arguments */
      int nfixparams = p->numparams;
      int fsize = p->maxstacksize;  /* frame size */
      checkstackGCp(L, fsize, func);
      L->ci = ci = next_ci(L);
      ci->nresults = nresults;
      ci->u.l.savedpc = p->code;  /* starting point */
      ci->top = func + 1 + fsize;
      ci->func = func;
      L->ci = ci;
      for (; narg < nfixparams; narg++)
        setnilvalue(s2v(L->top++));  /* complete missing arguments */
      ecierthon_assert(ci->top <= L->stack_last);
      return ci;
    }
    default: {  /* not a function */
      checkstackGCp(L, 1, func);  /* space for metamethod */
      ecierthonD_tryfuncTM(L, func);  /* try to get '__call' metamethod */
      goto retry;  /* try again with metamethod */
    }
  }
}


/*
** Call a function (C or ecierthon) through C. 'inc' can be 1 (increment
** number of recursive invocations in the C stack) or nyci (the same
** plus increment number of non-yieldable calls).
*/
static void ccall (ecierthon_State *L, StkId func, int nResults, int inc) {
  CallInfo *ci;
  L->nCcalls += inc;
  if (unlikely(getCcalls(L) >= ecierthonI_MAXCCALLS))
    ecierthonE_checkcstack(L);
  if ((ci = ecierthonD_precall(L, func, nResults)) != NULL) {  /* ecierthon function? */
    ci->callstatus = CIST_FRESH;  /* mark that it is a "fresh" execute */
    ecierthonV_execute(L, ci);  /* call it */
  }
  L->nCcalls -= inc;
}


/*
** External interface for 'ccall'
*/
void ecierthonD_call (ecierthon_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, 1);
}


/*
** Similar to 'ecierthonD_call', but does not allow yields during the call.
*/
void ecierthonD_callnoyield (ecierthon_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, nyci);
}


/*
** Completes the execution of an interrupted C function, calling its
** continuation function.
*/
static void finishCcall (ecierthon_State *L, int status) {
  CallInfo *ci = L->ci;
  int n;
  /* must have a continuation and must be able to call it */
  ecierthon_assert(ci->u.c.k != NULL && yieldable(L));
  /* error status can only happen in a protected call */
  ecierthon_assert((ci->callstatus & CIST_YPCALL) || status == ecierthon_YIELD);
  if (ci->callstatus & CIST_YPCALL) {  /* was inside a pcall? */
    ci->callstatus &= ~CIST_YPCALL;  /* continuation is also inside it */
    L->errfunc = ci->u.c.old_errfunc;  /* with the same error function */
  }
  /* finish 'ecierthon_callk'/'ecierthon_pcall'; CIST_YPCALL and 'errfunc' already
     handled */
  adjustresults(L, ci->nresults);
  ecierthon_unlock(L);
  n = (*ci->u.c.k)(L, status, ci->u.c.ctx);  /* call continuation function */
  ecierthon_lock(L);
  api_checknelems(L, n);
  ecierthonD_poscall(L, ci, n);  /* finish 'ecierthonD_call' */
}


/*
** Executes "full continuation" (everything in the stack) of a
** previously interrupted coroutine until the stack is empty (or another
** interruption long-jumps out of the loop). If the coroutine is
** recovering from an error, 'ud' points to the error status, which must
** be passed to the first continuation function (otherwise the default
** status is ecierthon_YIELD).
*/
static void unroll (ecierthon_State *L, void *ud) {
  CallInfo *ci;
  if (ud != NULL)  /* error status? */
    finishCcall(L, *(int *)ud);  /* finish 'ecierthon_pcallk' callee */
  while ((ci = L->ci) != &L->base_ci) {  /* something in the stack */
    if (!isecierthon(ci))  /* C function? */
      finishCcall(L, ecierthon_YIELD);  /* complete its execution */
    else {  /* ecierthon function */
      ecierthonV_finishOp(L);  /* finish interrupted instruction */
      ecierthonV_execute(L, ci);  /* execute down to higher C 'boundary' */
    }
  }
}


/*
** Try to find a suspended protected call (a "recover point") for the
** given thread.
*/
static CallInfo *findpcall (ecierthon_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {  /* search for a pcall */
    if (ci->callstatus & CIST_YPCALL)
      return ci;
  }
  return NULL;  /* no pending pcall */
}


/*
** Recovers from an error in a coroutine. Finds a recover point (if
** there is one) and completes the execution of the interrupted
** 'ecierthonD_pcall'. If there is no recover point, returns zero.
*/
static int recover (ecierthon_State *L, int status) {
  StkId oldtop;
  CallInfo *ci = findpcall(L);
  if (ci == NULL) return 0;  /* no recovery point */
  /* "finish" ecierthonD_pcall */
  oldtop = restorestack(L, ci->u2.funcidx);
  L->ci = ci;
  L->allowhook = getoah(ci->callstatus);  /* restore original 'allowhook' */
  status = ecierthonF_close(L, oldtop, status);  /* may change the stack */
  oldtop = restorestack(L, ci->u2.funcidx);
  ecierthonD_seterrorobj(L, status, oldtop);
  ecierthonD_shrinkstack(L);   /* restore stack size in case of overflow */
  L->errfunc = ci->u.c.old_errfunc;
  return 1;  /* continue running the coroutine */
}


/*
** Signal an error in the call to 'ecierthon_resume', not in the execution
** of the coroutine itself. (Such errors should not be handled by any
** coroutine error handler and should not kill the coroutine.)
*/
static int resume_error (ecierthon_State *L, const char *msg, int narg) {
  L->top -= narg;  /* remove args from the stack */
  setsvalue2s(L, L->top, ecierthonS_new(L, msg));  /* push error message */
  api_incr_top(L);
  ecierthon_unlock(L);
  return ecierthon_ERRRUN;
}


/*
** Do the work for 'ecierthon_resume' in protected mode. Most of the work
** depends on the status of the coroutine: initial state, suspended
** inside a hook, or regularly suspended (optionally with a continuation
** function), plus erroneous cases: non-suspended coroutine or dead
** coroutine.
*/
static void resume (ecierthon_State *L, void *ud) {
  int n = *(cast(int*, ud));  /* number of arguments */
  StkId firstArg = L->top - n;  /* first argument */
  CallInfo *ci = L->ci;
  if (L->status == ecierthon_OK)  /* starting a coroutine? */
    ccall(L, firstArg - 1, ecierthon_MULTRET, 1);  /* just call its body */
  else {  /* resuming from previous yield */
    ecierthon_assert(L->status == ecierthon_YIELD);
    L->status = ecierthon_OK;  /* mark that it is running (again) */
    ecierthonE_incCstack(L);  /* control the C stack */
    if (isecierthon(ci))  /* yielded inside a hook? */
      ecierthonV_execute(L, ci);  /* just continue running ecierthon code */
    else {  /* 'common' yield */
      if (ci->u.c.k != NULL) {  /* does it have a continuation function? */
        ecierthon_unlock(L);
        n = (*ci->u.c.k)(L, ecierthon_YIELD, ci->u.c.ctx); /* call continuation */
        ecierthon_lock(L);
        api_checknelems(L, n);
      }
      ecierthonD_poscall(L, ci, n);  /* finish 'ecierthonD_call' */
    }
    unroll(L, NULL);  /* run continuation */
  }
}

ecierthon_API int ecierthon_resume (ecierthon_State *L, ecierthon_State *from, int nargs,
                                      int *nresults) {
  int status;
  ecierthon_lock(L);
  if (L->status == ecierthon_OK) {  /* may be starting a coroutine */
    if (L->ci != &L->base_ci)  /* not in base level? */
      return resume_error(L, "cannot resume non-suspended coroutine", nargs);
    else if (L->top - (L->ci->func + 1) == nargs)  /* no function? */
      return resume_error(L, "cannot resume dead coroutine", nargs);
  }
  else if (L->status != ecierthon_YIELD)  /* ended with errors? */
    return resume_error(L, "cannot resume dead coroutine", nargs);
  L->nCcalls = (from) ? getCcalls(from) : 0;
  ecierthoni_userstateresume(L, nargs);
  api_checknelems(L, (L->status == ecierthon_OK) ? nargs + 1 : nargs);
  status = ecierthonD_rawrunprotected(L, resume, &nargs);
   /* continue running after recoverable errors */
  while (errorstatus(status) && recover(L, status)) {
    /* unroll continuation */
    status = ecierthonD_rawrunprotected(L, unroll, &status);
  }
  if (likely(!errorstatus(status)))
    ecierthon_assert(status == L->status);  /* normal end or yield */
  else {  /* unrecoverable error */
    L->status = cast_byte(status);  /* mark thread as 'dead' */
    ecierthonD_seterrorobj(L, status, L->top);  /* push error message */
    L->ci->top = L->top;
  }
  *nresults = (status == ecierthon_YIELD) ? L->ci->u2.nyield
                                    : cast_int(L->top - (L->ci->func + 1));
  ecierthon_unlock(L);
  return status;
}


ecierthon_API int ecierthon_isyieldable (ecierthon_State *L) {
  return yieldable(L);
}


ecierthon_API int ecierthon_yieldk (ecierthon_State *L, int nresults, ecierthon_KContext ctx,
                        ecierthon_KFunction k) {
  CallInfo *ci;
  ecierthoni_userstateyield(L, nresults);
  ecierthon_lock(L);
  ci = L->ci;
  api_checknelems(L, nresults);
  if (unlikely(!yieldable(L))) {
    if (L != G(L)->mainthread)
      ecierthonG_runerror(L, "attempt to yield across a C-call boundary");
    else
      ecierthonG_runerror(L, "attempt to yield from outside a coroutine");
  }
  L->status = ecierthon_YIELD;
  if (isecierthon(ci)) {  /* inside a hook? */
    ecierthon_assert(!isecierthoncode(ci));
    api_check(L, k == NULL, "hooks cannot continue after yielding");
    ci->u2.nyield = 0;  /* no results */
  }
  else {
    if ((ci->u.c.k = k) != NULL)  /* is there a continuation? */
      ci->u.c.ctx = ctx;  /* save context */
    ci->u2.nyield = nresults;  /* save number of results */
    ecierthonD_throw(L, ecierthon_YIELD);
  }
  ecierthon_assert(ci->callstatus & CIST_HOOKED);  /* must be inside a hook */
  ecierthon_unlock(L);
  return 0;  /* return to 'ecierthonD_hook' */
}


/*
** Call the C function 'func' in protected mode, restoring basic
** thread information ('allowhook', etc.) and in particular
** its stack level in case of errors.
*/
int ecierthonD_pcall (ecierthon_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = ecierthonD_rawrunprotected(L, func, u);
  if (unlikely(status != ecierthon_OK)) {  /* an error occurred? */
    StkId oldtop = restorestack(L, old_top);
    L->ci = old_ci;
    L->allowhook = old_allowhooks;
    status = ecierthonF_close(L, oldtop, status);
    oldtop = restorestack(L, old_top);  /* previous call may change stack */
    ecierthonD_seterrorobj(L, status, oldtop);
    ecierthonD_shrinkstack(L);   /* restore stack size in case of overflow */
  }
  L->errfunc = old_errfunc;
  return status;
}



/*
** Execute a protected parser.
*/
struct SParser {  /* data to 'f_parser' */
  ZIO *z;
  Mbuffer buff;  /* dynamic structure used by the scanner */
  Dyndata dyd;  /* dynamic structures used by the parser */
  const char *mode;
  const char *name;
};


static void checkmode (ecierthon_State *L, const char *mode, const char *x) {
  if (mode && strchr(mode, x[0]) == NULL) {
    ecierthonO_pushfstring(L,
       "attempt to load a %s chunk (mode is '%s')", x, mode);
    ecierthonD_throw(L, ecierthon_ERRSYNTAX);
  }
}


static void f_parser (ecierthon_State *L, void *ud) {
  LClosure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = zgetc(p->z);  /* read first character */
  if (c == LUA_SIGNATURE[0]) {
    checkmode(L, p->mode, "binary");
    cl = ecierthonU_undump(L, p->z, p->name);
  }
  else {
    checkmode(L, p->mode, "text");
    cl = ecierthonY_parser(L, p->z, &p->buff, &p->dyd, p->name, c);
  }
  ecierthon_assert(cl->nupvalues == cl->p->sizeupvalues);
  ecierthonF_initupvals(L, cl);
}


int ecierthonD_protectedparser (ecierthon_State *L, ZIO *z, const char *name,
                                        const char *mode) {
  struct SParser p;
  int status;
  incnny(L);  /* cannot yield during parsing */
  p.z = z; p.name = name; p.mode = mode;
  p.dyd.actvar.arr = NULL; p.dyd.actvar.size = 0;
  p.dyd.gt.arr = NULL; p.dyd.gt.size = 0;
  p.dyd.label.arr = NULL; p.dyd.label.size = 0;
  ecierthonZ_initbuffer(L, &p.buff);
  status = ecierthonD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
  ecierthonZ_freebuffer(L, &p.buff);
  ecierthonM_freearray(L, p.dyd.actvar.arr, p.dyd.actvar.size);
  ecierthonM_freearray(L, p.dyd.gt.arr, p.dyd.gt.size);
  ecierthonM_freearray(L, p.dyd.label.arr, p.dyd.label.size);
  decnny(L);
  return status;
}


