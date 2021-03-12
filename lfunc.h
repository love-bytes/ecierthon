/*
** $Id: lfunc.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in ecierthon.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"


#define sizeCclosure(n)	(cast_int(offsetof(CClosure, upvalue)) + \
                         cast_int(sizeof(TValue)) * (n))

#define sizeLclosure(n)	(cast_int(offsetof(LClosure, upvals)) + \
                         cast_int(sizeof(TValue *)) * (n))


/* test whether thread is in 'twups' list */
#define isintwups(L)	(L->twups != L)


/*
** maximum number of upvalues in a closure (both C and ecierthon). (Value
** must fit in a VM register.)
*/
#define MAXUPVAL	255


#define upisopen(up)	((up)->v != &(up)->u.value)


#define uplevel(up)	check_exp(upisopen(up), cast(StkId, (up)->v))


/*
** maximum number of misses before giving up the cache of closures
** in prototypes
*/
#define MAXMISS		10


/*
** Special "status" for 'ecierthonF_close'
*/

/* close upvalues without running their closing methods */
#define NOCLOSINGMETH	(-1)

/* close upvalues running all closing methods in protected mode */
#define CLOSEPROTECT	(-2)


ecierthonI_FUNC Proto *ecierthonF_newproto (ecierthon_State *L);
ecierthonI_FUNC CClosure *ecierthonF_newCclosure (ecierthon_State *L, int nupvals);
ecierthonI_FUNC LClosure *ecierthonF_newLclosure (ecierthon_State *L, int nupvals);
ecierthonI_FUNC void ecierthonF_initupvals (ecierthon_State *L, LClosure *cl);
ecierthonI_FUNC UpVal *ecierthonF_findupval (ecierthon_State *L, StkId level);
ecierthonI_FUNC void ecierthonF_newtbcupval (ecierthon_State *L, StkId level);
ecierthonI_FUNC int ecierthonF_close (ecierthon_State *L, StkId level, int status);
ecierthonI_FUNC void ecierthonF_unlinkupval (UpVal *uv);
ecierthonI_FUNC void ecierthonF_freeproto (ecierthon_State *L, Proto *f);
ecierthonI_FUNC const char *ecierthonF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
