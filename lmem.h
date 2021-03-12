/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in ecierthon.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "ecierthon.h"


#define ecierthonM_error(L)	ecierthonD_throw(L, ecierthon_ERRMEM)


/*
** This macro tests whether it is safe to multiply 'n' by the size of
** type 't' without overflows. Because 'e' is always constant, it avoids
** the runtime division MAX_SIZET/(e).
** (The macro is somewhat complex to avoid warnings:  The 'sizeof'
** comparison avoids a runtime comparison when overflow cannot occur.
** The compiler should be able to optimize the real test by itself, but
** when it does it, it may give a warning about "comparison is always
** false due to limited range of data type"; the +1 tricks the compiler,
** avoiding this warning but also this optimization.)
*/
#define ecierthonM_testsize(n,e)  \
	(sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define ecierthonM_checksize(L,n,e)  \
	(ecierthonM_testsize(n,e) ? ecierthonM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' or 'unsigned int' and that 'int' is not larger than 'size_t'.)
*/
#define ecierthonM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_uint((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define ecierthonM_reallocvchar(L,b,on,n)  \
  cast_charp(ecierthonM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define ecierthonM_freemem(L, b, s)	ecierthonM_free_(L, (b), (s))
#define ecierthonM_free(L, b)		ecierthonM_free_(L, (b), sizeof(*(b)))
#define ecierthonM_freearray(L, b, n)   ecierthonM_free_(L, (b), (n)*sizeof(*(b)))

#define ecierthonM_new(L,t)		cast(t*, ecierthonM_malloc_(L, sizeof(t), 0))
#define ecierthonM_newvector(L,n,t)	cast(t*, ecierthonM_malloc_(L, (n)*sizeof(t), 0))
#define ecierthonM_newvectorchecked(L,n,t) \
  (ecierthonM_checksize(L,n,sizeof(t)), ecierthonM_newvector(L,n,t))

#define ecierthonM_newobject(L,tag,s)	ecierthonM_malloc_(L, (s), tag)

#define ecierthonM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t *, ecierthonM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         ecierthonM_limitN(limit,t),e)))

#define ecierthonM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, ecierthonM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define ecierthonM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, ecierthonM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

ecierthonI_FUNC l_noret ecierthonM_toobig (ecierthon_State *L);

/* not to be called directly */
ecierthonI_FUNC void *ecierthonM_realloc_ (ecierthon_State *L, void *block, size_t oldsize,
                                                          size_t size);
ecierthonI_FUNC void *ecierthonM_saferealloc_ (ecierthon_State *L, void *block, size_t oldsize,
                                                              size_t size);
ecierthonI_FUNC void ecierthonM_free_ (ecierthon_State *L, void *block, size_t osize);
ecierthonI_FUNC void *ecierthonM_growaux_ (ecierthon_State *L, void *block, int nelems,
                               int *size, int size_elem, int limit,
                               const char *what);
ecierthonI_FUNC void *ecierthonM_shrinkvector_ (ecierthon_State *L, void *block, int *nelem,
                                    int final_n, int size_elem);
ecierthonI_FUNC void *ecierthonM_malloc_ (ecierthon_State *L, size_t size, int tag);

#endif

