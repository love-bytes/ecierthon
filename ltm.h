/*
** $Id: ltm.h $
** Tag methods
** See Copyright Notice in ecierthon.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM" and "ORDER OP"
*/
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_LEN,
  TM_EQ,  /* last tag method with fast access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_MOD,
  TM_POW,
  TM_DIV,
  TM_IDIV,
  TM_BAND,
  TM_BOR,
  TM_BXOR,
  TM_SHL,
  TM_SHR,
  TM_UNM,
  TM_BNOT,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_CLOSE,
  TM_N		/* number of elements in the enum */
} TMS;


/*
** Mask with 1 in all fast-access methods. A 1 in any of these bits
** in the flag of a (meta)table means the metatable does not have the
** corresponding metamethod field. (Bit 7 of the flag is used for
** 'isrealasize'.)
*/
#define maskflags	(~(~0u << (TM_EQ + 1)))


/*
** Test whether there is no tagmethod.
** (Because tagmethods use raw accesses, the result may be an "empty" nil.)
*/
#define notm(tm)	ttisnil(tm)


#define gfasttm(g,et,e) ((et) == NULL ? NULL : \
  ((et)->flags & (1u<<(e))) ? NULL : ecierthonT_gettm(et, e, (g)->tmname[e]))

#define fasttm(l,et,e)	gfasttm(G(l), et, e)

#define ttypename(x)	ecierthonT_typenames_[(x) + 1]

ecierthonI_DDEC(const char *const ecierthonT_typenames_[ecierthon_TOTALTYPES];)


ecierthonI_FUNC const char *ecierthonT_objtypename (ecierthon_State *L, const TValue *o);

ecierthonI_FUNC const TValue *ecierthonT_gettm (Table *events, TMS event, TString *ename);
ecierthonI_FUNC const TValue *ecierthonT_gettmbyobj (ecierthon_State *L, const TValue *o,
                                                       TMS event);
ecierthonI_FUNC void ecierthonT_init (ecierthon_State *L);

ecierthonI_FUNC void ecierthonT_callTM (ecierthon_State *L, const TValue *f, const TValue *p1,
                            const TValue *p2, const TValue *p3);
ecierthonI_FUNC void ecierthonT_callTMres (ecierthon_State *L, const TValue *f,
                            const TValue *p1, const TValue *p2, StkId p3);
ecierthonI_FUNC void ecierthonT_trybinTM (ecierthon_State *L, const TValue *p1, const TValue *p2,
                              StkId res, TMS event);
ecierthonI_FUNC void ecierthonT_tryconcatTM (ecierthon_State *L);
ecierthonI_FUNC void ecierthonT_trybinassocTM (ecierthon_State *L, const TValue *p1,
       const TValue *p2, int inv, StkId res, TMS event);
ecierthonI_FUNC void ecierthonT_trybiniTM (ecierthon_State *L, const TValue *p1, ecierthon_Integer i2,
                               int inv, StkId res, TMS event);
ecierthonI_FUNC int ecierthonT_callorderTM (ecierthon_State *L, const TValue *p1,
                                const TValue *p2, TMS event);
ecierthonI_FUNC int ecierthonT_callorderiTM (ecierthon_State *L, const TValue *p1, int v2,
                                 int inv, int isfloat, TMS event);

ecierthonI_FUNC void ecierthonT_adjustvarargs (ecierthon_State *L, int nfixparams,
                                   struct CallInfo *ci, const Proto *p);
ecierthonI_FUNC void ecierthonT_getvarargs (ecierthon_State *L, struct CallInfo *ci,
                                              StkId where, int wanted);


#endif
