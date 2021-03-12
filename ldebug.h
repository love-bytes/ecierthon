/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in ecierthon.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active ecierthon function (given call info) */
#define ci_func(ci)		(clLvalue(s2v((ci)->func)))


#define resethookcount(L)	(L->hookcount = L->basehookcount)

/*
** mark for entries in 'lineinfo' array that has absolute information in
** 'abslineinfo' array
*/
#define ABSLINEINFO	(-0x80)

ecierthonI_FUNC int ecierthonG_getfuncline (const Proto *f, int pc);
ecierthonI_FUNC const char *ecierthonG_findlocal (ecierthon_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
ecierthonI_FUNC l_noret ecierthonG_typeerror (ecierthon_State *L, const TValue *o,
                                                const char *opname);
ecierthonI_FUNC l_noret ecierthonG_forerror (ecierthon_State *L, const TValue *o,
                                               const char *what);
ecierthonI_FUNC l_noret ecierthonG_concaterror (ecierthon_State *L, const TValue *p1,
                                                  const TValue *p2);
ecierthonI_FUNC l_noret ecierthonG_opinterror (ecierthon_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
ecierthonI_FUNC l_noret ecierthonG_tointerror (ecierthon_State *L, const TValue *p1,
                                                 const TValue *p2);
ecierthonI_FUNC l_noret ecierthonG_ordererror (ecierthon_State *L, const TValue *p1,
                                                 const TValue *p2);
ecierthonI_FUNC l_noret ecierthonG_runerror (ecierthon_State *L, const char *fmt, ...);
ecierthonI_FUNC const char *ecierthonG_addinfo (ecierthon_State *L, const char *msg,
                                                  TString *src, int line);
ecierthonI_FUNC l_noret ecierthonG_errormsg (ecierthon_State *L);
ecierthonI_FUNC int ecierthonG_traceexec (ecierthon_State *L, const Instruction *pc);


#endif
