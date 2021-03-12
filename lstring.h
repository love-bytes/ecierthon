/*
** $Id: lstring.h $
** String table (keep all strings handled by ecierthon)
** See Copyright Notice in ecierthon.h
*/

#ifndef lstring_h
#define lstring_h

#include "lgc.h"
#include "lobject.h"
#include "lstate.h"


/*
** Memory-allocation error message must be preallocated (it cannot
** be created after memory is exhausted)
*/
#define MEMERRMSG       "not enough memory"


/*
** Size of a TString: Size of the header plus space for the string
** itself (including final '\0').
*/
#define sizelstring(l)  (offsetof(TString, contents) + ((l) + 1) * sizeof(char))

#define ecierthonS_newliteral(L, s)	(ecierthonS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	((s)->tt == ecierthon_VSHRSTR && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == ecierthon_VSHRSTR, (a) == (b))


ecierthonI_FUNC unsigned int ecierthonS_hash (const char *str, size_t l, unsigned int seed);
ecierthonI_FUNC unsigned int ecierthonS_hashlongstr (TString *ts);
ecierthonI_FUNC int ecierthonS_eqlngstr (TString *a, TString *b);
ecierthonI_FUNC void ecierthonS_resize (ecierthon_State *L, int newsize);
ecierthonI_FUNC void ecierthonS_clearcache (global_State *g);
ecierthonI_FUNC void ecierthonS_init (ecierthon_State *L);
ecierthonI_FUNC void ecierthonS_remove (ecierthon_State *L, TString *ts);
ecierthonI_FUNC Udata *ecierthonS_newudata (ecierthon_State *L, size_t s, int nuvalue);
ecierthonI_FUNC TString *ecierthonS_newlstr (ecierthon_State *L, const char *str, size_t l);
ecierthonI_FUNC TString *ecierthonS_new (ecierthon_State *L, const char *str);
ecierthonI_FUNC TString *ecierthonS_createlngstrobj (ecierthon_State *L, size_t l);


#endif
