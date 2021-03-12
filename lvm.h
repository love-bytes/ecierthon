/*
** $Id: lvm.h $
** ecierthon virtual machine
** See Copyright Notice in ecierthon.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#if !defined(ecierthon_NOCVTN2S)
#define cvt2str(o)	ttisnumber(o)
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(ecierthon_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define ecierthon_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(ecierthon_FLOORN2I)
#define ecierthon_FLOORN2I		F2Ieq
#endif


/*
** Rounding modes for float->integer coercion
 */
typedef enum {
  F2Ieq,     /* no rounding; accepts only integral values */
  F2Ifloor,  /* takes the floor of the number */
  F2Iceil    /* takes the ceil of the number */
} F2Imod;


/* convert an object to a float (including string coercion) */
#define tonumber(o,n) \
	(ttisfloat(o) ? (*(n) = fltvalue(o), 1) : ecierthonV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
	(ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
	(ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (ttisinteger(o) ? (*(i) = ivalue(o), 1) : ecierthonV_tointeger(o,i,ecierthon_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (ttisinteger(o) ? (*(i) = ivalue(o), 1) : ecierthonV_tointegerns(o,i,ecierthon_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define ecierthonV_rawequalobj(t1,t2)		ecierthonV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define ecierthonV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */


/*
** Special case of 'ecierthonV_fastget' for integers, inlining the fast case
** of 'ecierthonH_getint'.
*/
#define ecierthonV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->alimit) \
              ? &hvalue(t)->array[k - 1] : ecierthonH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */


/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/
#define ecierthonV_finishfastset(L,t,slot,v) \
    { setobj2t(L, cast(TValue *,slot), v); \
      ecierthonC_barrierback(L, gcvalue(t), v); }




ecierthonI_FUNC int ecierthonV_equalobj (ecierthon_State *L, const TValue *t1, const TValue *t2);
ecierthonI_FUNC int ecierthonV_lessthan (ecierthon_State *L, const TValue *l, const TValue *r);
ecierthonI_FUNC int ecierthonV_lessequal (ecierthon_State *L, const TValue *l, const TValue *r);
ecierthonI_FUNC int ecierthonV_tonumber_ (const TValue *obj, ecierthon_Number *n);
ecierthonI_FUNC int ecierthonV_tointeger (const TValue *obj, ecierthon_Integer *p, F2Imod mode);
ecierthonI_FUNC int ecierthonV_tointegerns (const TValue *obj, ecierthon_Integer *p,
                                F2Imod mode);
ecierthonI_FUNC int ecierthonV_flttointeger (ecierthon_Number n, ecierthon_Integer *p, F2Imod mode);
ecierthonI_FUNC void ecierthonV_finishget (ecierthon_State *L, const TValue *t, TValue *key,
                               StkId val, const TValue *slot);
ecierthonI_FUNC void ecierthonV_finishset (ecierthon_State *L, const TValue *t, TValue *key,
                               TValue *val, const TValue *slot);
ecierthonI_FUNC void ecierthonV_finishOp (ecierthon_State *L);
ecierthonI_FUNC void ecierthonV_execute (ecierthon_State *L, CallInfo *ci);
ecierthonI_FUNC void ecierthonV_concat (ecierthon_State *L, int total);
ecierthonI_FUNC ecierthon_Integer ecierthonV_idiv (ecierthon_State *L, ecierthon_Integer x, ecierthon_Integer y);
ecierthonI_FUNC ecierthon_Integer ecierthonV_mod (ecierthon_State *L, ecierthon_Integer x, ecierthon_Integer y);
ecierthonI_FUNC ecierthon_Number ecierthonV_modf (ecierthon_State *L, ecierthon_Number x, ecierthon_Number y);
ecierthonI_FUNC ecierthon_Integer ecierthonV_shiftl (ecierthon_Integer x, ecierthon_Integer y);
ecierthonI_FUNC void ecierthonV_objlen (ecierthon_State *L, StkId ra, const TValue *rb);

#endif
