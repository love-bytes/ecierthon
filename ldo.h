/*
** $Id: ldo.h $
** Stack and Call structure of ecierthon
** See Copyright Notice in ecierthon.h
*/

#ifndef ldo_h
#define ldo_h


#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


/*
** Macro to check stack size and grow stack if needed.  Parameters
** 'pre'/'pos' allow the macro to preserve a pointer into the
** stack across reallocations, doing the work only when needed.
** It also allows the running of one GC step when the stack is
** reallocated.
** 'condmovestack' is used in heavy tests to force a stack reallocation
** at every check.
*/
#define ecierthonD_checkstackaux(L,n,pre,pos)  \
	if (L->stack_last - L->top <= (n)) \
	  { pre; ecierthonD_growstack(L, n, 1); pos; } \
        else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define ecierthonD_checkstack(L,n)	ecierthonD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((StkId)((char *)L->stack + (n)))


/* macro to check stack size, preserving 'p' */
#define checkstackGCp(L,n,p)  \
  ecierthonD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    ecierthonC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC */
#define checkstackGC(L,fsize)  \
	ecierthonD_checkstackaux(L, (fsize), ecierthonC_checkGC(L), (void)0)


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (ecierthon_State *L, void *ud);

ecierthonI_FUNC void ecierthonD_seterrorobj (ecierthon_State *L, int errcode, StkId oldtop);
ecierthonI_FUNC int ecierthonD_protectedparser (ecierthon_State *L, ZIO *z, const char *name,
                                                  const char *mode);
ecierthonI_FUNC void ecierthonD_hook (ecierthon_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
ecierthonI_FUNC void ecierthonD_hookcall (ecierthon_State *L, CallInfo *ci);
ecierthonI_FUNC void ecierthonD_pretailcall (ecierthon_State *L, CallInfo *ci, StkId func, int n);
ecierthonI_FUNC CallInfo *ecierthonD_precall (ecierthon_State *L, StkId func, int nResults);
ecierthonI_FUNC void ecierthonD_call (ecierthon_State *L, StkId func, int nResults);
ecierthonI_FUNC void ecierthonD_callnoyield (ecierthon_State *L, StkId func, int nResults);
ecierthonI_FUNC void ecierthonD_tryfuncTM (ecierthon_State *L, StkId func);
ecierthonI_FUNC int ecierthonD_pcall (ecierthon_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
ecierthonI_FUNC void ecierthonD_poscall (ecierthon_State *L, CallInfo *ci, int nres);
ecierthonI_FUNC int ecierthonD_reallocstack (ecierthon_State *L, int newsize, int raiseerror);
ecierthonI_FUNC int ecierthonD_growstack (ecierthon_State *L, int n, int raiseerror);
ecierthonI_FUNC void ecierthonD_shrinkstack (ecierthon_State *L);
ecierthonI_FUNC void ecierthonD_inctop (ecierthon_State *L);

ecierthonI_FUNC l_noret ecierthonD_throw (ecierthon_State *L, int errcode);
ecierthonI_FUNC int ecierthonD_rawrunprotected (ecierthon_State *L, Pfunc f, void *ud);

#endif

