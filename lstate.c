/*
** $Id: lstate.c $
** Global State
** See Copyright Notice in ecierthon.h
*/

#define lstate_c
#define ecierthon_CORE

#include "lprefix.h"


#include <stddef.h>
#include <string.h>

#include "ecierthon.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"



/*
** thread state + extra space
*/
typedef struct LX {
  lu_byte extra_[ecierthon_EXTRASPACE];
  ecierthon_State l;
} LX;


/*
** Main thread combines a thread state and the global state
*/
typedef struct LG {
  LX l;
  global_State g;
} LG;



#define fromstate(L)	(cast(LX *, cast(lu_byte *, (L)) - offsetof(LX, l)))


/*
** A macro to create a "random" seed when a state is created;
** the seed is used to randomize string hashes.
*/
#if !defined(ecierthoni_makeseed)

#include <time.h>

/*
** Compute an initial seed with some level of randomness.
** Rely on Address Space Layout Randomization (if present) and
** current time.
*/
#define addbuff(b,p,e) \
  { size_t t = cast_sizet(e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int ecierthoni_makeseed (ecierthon_State *L) {
  char buff[3 * sizeof(size_t)];
  unsigned int h = cast_uint(time(NULL));
  int p = 0;
  addbuff(buff, p, L);  /* heap variable */
  addbuff(buff, p, &h);  /* local variable */
  addbuff(buff, p, &ecierthon_newstate);  /* public function */
  ecierthon_assert(p == sizeof(buff));
  return ecierthonS_hash(buff, p, h);
}

#endif


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void ecierthonE_setdebt (global_State *g, l_mem debt) {
  l_mem tb = gettotalbytes(g);
  ecierthon_assert(tb > 0);
  if (debt < tb - MAX_LMEM)
    debt = tb - MAX_LMEM;  /* will make 'totalbytes == MAX_LMEM' */
  g->totalbytes = tb - debt;
  g->GCdebt = debt;
}


ecierthon_API int ecierthon_setcstacklimit (ecierthon_State *L, unsigned int limit) {
  UNUSED(L); UNUSED(limit);
  return ecierthonI_MAXCCALLS;  /* warning?? */
}


CallInfo *ecierthonE_extendCI (ecierthon_State *L) {
  CallInfo *ci;
  ecierthon_assert(L->ci->next == NULL);
  ci = ecierthonM_new(L, CallInfo);
  ecierthon_assert(L->ci->next == NULL);
  L->ci->next = ci;
  ci->previous = L->ci;
  ci->next = NULL;
  ci->u.l.trap = 0;
  L->nci++;
  return ci;
}


/*
** free all CallInfo structures not in use by a thread
*/
void ecierthonE_freeCI (ecierthon_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    ecierthonM_free(L, ci);
    L->nci--;
  }
}


/*
** free half of the CallInfo structures not in use by a thread,
** keeping the first one.
*/
void ecierthonE_shrinkCI (ecierthon_State *L) {
  CallInfo *ci = L->ci->next;  /* first free CallInfo */
  CallInfo *next;
  if (ci == NULL)
    return;  /* no extra elements */
  while ((next = ci->next) != NULL) {  /* two extra elements? */
    CallInfo *next2 = next->next;  /* next's next */
    ci->next = next2;  /* remove next from the list */
    L->nci--;
    ecierthonM_free(L, next);  /* free next */
    if (next2 == NULL)
      break;  /* no more elements */
    else {
      next2->previous = ci;
      ci = next2;  /* continue */
    }
  }
}


/*
** Called when 'getCcalls(L)' larger or equal to ecierthonI_MAXCCALLS.
** If equal, raises an overflow error. If value is larger than
** ecierthonI_MAXCCALLS (which means it is handling an overflow) but
** not much larger, does not report an error (to allow overflow
** handling to work).
*/
void ecierthonE_checkcstack (ecierthon_State *L) {
  if (getCcalls(L) == ecierthonI_MAXCCALLS)
    ecierthonG_runerror(L, "C stack overflow");
  else if (getCcalls(L) >= (ecierthonI_MAXCCALLS / 10 * 11))
    ecierthonD_throw(L, ecierthon_ERRERR);  /* error while handing stack error */
}


ecierthonI_FUNC void ecierthonE_incCstack (ecierthon_State *L) {
  L->nCcalls++;
  if (unlikely(getCcalls(L) >= ecierthonI_MAXCCALLS))
    ecierthonE_checkcstack(L);
}


static void stack_init (ecierthon_State *L1, ecierthon_State *L) {
  int i; CallInfo *ci;
  /* initialize stack array */
  L1->stack = ecierthonM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
  for (i = 0; i < BASIC_STACK_SIZE + EXTRA_STACK; i++)
    setnilvalue(s2v(L1->stack + i));  /* erase new stack */
  L1->top = L1->stack;
  L1->stack_last = L1->stack + BASIC_STACK_SIZE;
  /* initialize first ci */
  ci = &L1->base_ci;
  ci->next = ci->previous = NULL;
  ci->callstatus = CIST_C;
  ci->func = L1->top;
  ci->u.c.k = NULL;
  ci->nresults = 0;
  setnilvalue(s2v(L1->top));  /* 'function' entry for this 'ci' */
  L1->top++;
  ci->top = L1->top + ecierthon_MINSTACK;
  L1->ci = ci;
}


static void freestack (ecierthon_State *L) {
  if (L->stack == NULL)
    return;  /* stack not completely built yet */
  L->ci = &L->base_ci;  /* free the entire 'ci' list */
  ecierthonE_freeCI(L);
  ecierthon_assert(L->nci == 0);
  ecierthonM_freearray(L, L->stack, stacksize(L) + EXTRA_STACK);  /* free stack */
}


/*
** Create registry table and its predefined values
*/
static void init_registry (ecierthon_State *L, global_State *g) {
  TValue temp;
  /* create registry */
  Table *registry = ecierthonH_new(L);
  sethvalue(L, &g->l_registry, registry);
  ecierthonH_resize(L, registry, ecierthon_RIDX_LAST, 0);
  /* registry[ecierthon_RIDX_MAINTHREAD] = L */
  setthvalue(L, &temp, L);  /* temp = L */
  ecierthonH_setint(L, registry, ecierthon_RIDX_MAINTHREAD, &temp);
  /* registry[ecierthon_RIDX_GLOBALS] = table of globals */
  sethvalue(L, &temp, ecierthonH_new(L));  /* temp = new table (global table) */
  ecierthonH_setint(L, registry, ecierthon_RIDX_GLOBALS, &temp);
}


/*
** open parts of the state that may cause memory-allocation errors.
** ('g->nilvalue' being a nil value flags that the state was completely
** build.)
*/
static void f_ecierthonopen (ecierthon_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack */
  init_registry(L, g);
  ecierthonS_init(L);
  ecierthonT_init(L);
  ecierthonX_init(L);
  g->gcrunning = 1;  /* allow gc */
  setnilvalue(&g->nilvalue);
  ecierthoni_userstateopen(L);
}


/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread (ecierthon_State *L, global_State *g) {
  G(L) = g;
  L->stack = NULL;
  L->ci = NULL;
  L->nci = 0;
  L->twups = L;  /* thread has no upvalues */
  L->errorJmp = NULL;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->status = ecierthon_OK;
  L->errfunc = 0;
  L->oldpc = 0;
}


static void close_state (ecierthon_State *L) {
  global_State *g = G(L);
  ecierthonF_close(L, L->stack, CLOSEPROTECT);  /* close all upvalues */
  ecierthonC_freeallobjects(L);  /* collect all objects */
  if (ttisnil(&g->nilvalue))  /* closing a fully built state? */
    ecierthoni_userstateclose(L);
  ecierthonM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  freestack(L);
  ecierthon_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}


ecierthon_API ecierthon_State *ecierthon_newthread (ecierthon_State *L) {
  global_State *g;
  ecierthon_State *L1;
  ecierthon_lock(L);
  g = G(L);
  ecierthonC_checkGC(L);
  /* create new thread */
  L1 = &cast(LX *, ecierthonM_newobject(L, ecierthon_TTHREAD, sizeof(LX)))->l;
  L1->marked = ecierthonC_white(g);
  L1->tt = ecierthon_VTHREAD;
  /* link it on list 'allgc' */
  L1->next = g->allgc;
  g->allgc = obj2gco(L1);
  /* anchor it on L stack */
  setthvalue2s(L, L->top, L1);
  api_incr_top(L);
  preinit_thread(L1, g);
  L1->nCcalls = 0;
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  /* initialize L1 extra space */
  memcpy(ecierthon_getextraspace(L1), ecierthon_getextraspace(g->mainthread),
         ecierthon_EXTRASPACE);
  ecierthoni_userstatethread(L, L1);
  stack_init(L1, L);  /* init stack */
  ecierthon_unlock(L);
  return L1;
}


void ecierthonE_freethread (ecierthon_State *L, ecierthon_State *L1) {
  LX *l = fromstate(L1);
  ecierthonF_close(L1, L1->stack, NOCLOSINGMETH);  /* close all upvalues */
  ecierthon_assert(L1->openupval == NULL);
  ecierthoni_userstatefree(L, L1);
  freestack(L1);
  ecierthonM_free(L, l);
}


int ecierthon_resetthread (ecierthon_State *L) {
  CallInfo *ci;
  int status;
  ecierthon_lock(L);
  L->ci = ci = &L->base_ci;  /* unwind CallInfo list */
  setnilvalue(s2v(L->stack));  /* 'function' entry for basic 'ci' */
  ci->func = L->stack;
  ci->callstatus = CIST_C;
  status = ecierthonF_close(L, L->stack, CLOSEPROTECT);
  if (status != CLOSEPROTECT)  /* real errors? */
    ecierthonD_seterrorobj(L, status, L->stack + 1);
  else {
    status = ecierthon_OK;
    L->top = L->stack + 1;
  }
  ci->top = L->top + ecierthon_MINSTACK;
  L->status = status;
  ecierthon_unlock(L);
  return status;
}


ecierthon_API ecierthon_State *ecierthon_newstate (ecierthon_Alloc f, void *ud) {
  int i;
  ecierthon_State *L;
  global_State *g;
  LG *l = cast(LG *, (*f)(ud, NULL, ecierthon_TTHREAD, sizeof(LG)));
  if (l == NULL) return NULL;
  L = &l->l.l;
  g = &l->g;
  L->tt = ecierthon_VTHREAD;
  g->currentwhite = bitmask(WHITE0BIT);
  L->marked = ecierthonC_white(g);
  preinit_thread(L, g);
  g->allgc = obj2gco(L);  /* by now, only object is the main thread */
  L->next = NULL;
  L->nCcalls = 0;
  incnny(L);  /* main thread is always non yieldable */
  g->frealloc = f;
  g->ud = ud;
  g->warnf = NULL;
  g->ud_warn = NULL;
  g->mainthread = L;
  g->seed = ecierthoni_makeseed(L);
  g->gcrunning = 0;  /* no GC while building state */
  g->strt.size = g->strt.nuse = 0;
  g->strt.hash = NULL;
  setnilvalue(&g->l_registry);
  g->panic = NULL;
  g->gcstate = GCSpause;
  g->gckind = KGC_INC;
  g->gcemergency = 0;
  g->finobj = g->tobefnz = g->fixedgc = NULL;
  g->firstold1 = g->survival = g->old1 = g->reallyold = NULL;
  g->finobjsur = g->finobjold1 = g->finobjrold = NULL;
  g->sweepgc = NULL;
  g->gray = g->grayagain = NULL;
  g->weak = g->ephemeron = g->allweak = NULL;
  g->twups = NULL;
  g->totalbytes = sizeof(LG);
  g->GCdebt = 0;
  g->lastatomic = 0;
  setivalue(&g->nilvalue, 0);  /* to signal that state is not yet built */
  setgcparam(g->gcpause, ecierthonI_GCPAUSE);
  setgcparam(g->gcstepmul, ecierthonI_GCMUL);
  g->gcstepsize = ecierthonI_GCSTEPSIZE;
  setgcparam(g->genmajormul, ecierthonI_GENMAJORMUL);
  g->genminormul = ecierthonI_GENMINORMUL;
  for (i=0; i < ecierthon_NUMTAGS; i++) g->mt[i] = NULL;
  if (ecierthonD_rawrunprotected(L, f_ecierthonopen, NULL) != ecierthon_OK) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  return L;
}


ecierthon_API void ecierthon_close (ecierthon_State *L) {
  ecierthon_lock(L);
  L = G(L)->mainthread;  /* only the main thread can be closed */
  close_state(L);
}


void ecierthonE_warning (ecierthon_State *L, const char *msg, int tocont) {
  ecierthon_WarnFunction wf = G(L)->warnf;
  if (wf != NULL)
    wf(G(L)->ud_warn, msg, tocont);
}


/*
** Generate a warning from an error message
*/
void ecierthonE_warnerror (ecierthon_State *L, const char *where) {
  TValue *errobj = s2v(L->top - 1);  /* error object */
  const char *msg = (ttisstring(errobj))
                  ? svalue(errobj)
                  : "error object is not a string";
  /* produce warning "error in %s (%s)" (where, msg) */
  ecierthonE_warning(L, "error in ", 1);
  ecierthonE_warning(L, where, 1);
  ecierthonE_warning(L, " (", 1);
  ecierthonE_warning(L, msg, 1);
  ecierthonE_warning(L, ")", 0);
}

