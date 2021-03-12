/*
** $Id: ltable.h $
** ecierthon tables (hash)
** See Copyright Notice in ecierthon.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define gnode(t,i)	(&(t)->node[i])
#define gval(n)		(&(n)->i_val)
#define gnext(n)	((n)->u.next)


/*
** Clear all bits of fast-access metamethods, which means that the table
** may have any of these metamethods. (First access that fails after the
** clearing will set the bit again.)
*/
#define invalidateTMcache(t)	((t)->flags &= ~maskflags)


/* true when 't' is using 'dummynode' as its hash part */
#define isdummy(t)		((t)->lastfree == NULL)


/* allocated size for hash nodes */
#define allocsizenode(t)	(isdummy(t) ? 0 : sizenode(t))


/* returns the Node, given the value of a table entry */
#define nodefromval(v)	cast(Node *, (v))


ecierthonI_FUNC const TValue *ecierthonH_getint (Table *t, ecierthon_Integer key);
ecierthonI_FUNC void ecierthonH_setint (ecierthon_State *L, Table *t, ecierthon_Integer key,
                                                    TValue *value);
ecierthonI_FUNC const TValue *ecierthonH_getshortstr (Table *t, TString *key);
ecierthonI_FUNC const TValue *ecierthonH_getstr (Table *t, TString *key);
ecierthonI_FUNC const TValue *ecierthonH_get (Table *t, const TValue *key);
ecierthonI_FUNC TValue *ecierthonH_newkey (ecierthon_State *L, Table *t, const TValue *key);
ecierthonI_FUNC TValue *ecierthonH_set (ecierthon_State *L, Table *t, const TValue *key);
ecierthonI_FUNC Table *ecierthonH_new (ecierthon_State *L);
ecierthonI_FUNC void ecierthonH_resize (ecierthon_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
ecierthonI_FUNC void ecierthonH_resizearray (ecierthon_State *L, Table *t, unsigned int nasize);
ecierthonI_FUNC void ecierthonH_free (ecierthon_State *L, Table *t);
ecierthonI_FUNC int ecierthonH_next (ecierthon_State *L, Table *t, StkId key);
ecierthonI_FUNC ecierthon_Unsigned ecierthonH_getn (Table *t);
ecierthonI_FUNC unsigned int ecierthonH_realasize (const Table *t);


#if defined(ecierthon_DEBUG)
ecierthonI_FUNC Node *ecierthonH_mainposition (const Table *t, const TValue *key);
ecierthonI_FUNC int ecierthonH_isdummy (const Table *t);
#endif


#endif
