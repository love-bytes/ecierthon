/*
** $Id: lauxlib.h $
** Auxiliary functions for building ecierthon libraries
** See Copyright Notice in ecierthon.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

#include "ecierthon.h"


/* global table */
#define ecierthon_GNAME	"_G"


typedef struct ecierthonL_Buffer ecierthonL_Buffer;


/* extra error code for 'ecierthonL_loadfilex' */
#define ecierthon_ERRFILE     (ecierthon_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define ecierthon_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define ecierthon_PRELOAD_TABLE	"_PRELOAD"


typedef struct ecierthonL_Reg {
  const char *name;
  ecierthon_CFunction func;
} ecierthonL_Reg;


#define ecierthonL_NUMSIZES	(sizeof(ecierthon_Integer)*16 + sizeof(ecierthon_Number))

ecierthonLIB_API void (ecierthonL_checkversion_) (ecierthon_State *L, ecierthon_Number ver, size_t sz);
#define ecierthonL_checkversion(L)  \
	  ecierthonL_checkversion_(L, ecierthon_VERSION_NUM, ecierthonL_NUMSIZES)

ecierthonLIB_API int (ecierthonL_getmetafield) (ecierthon_State *L, int obj, const char *e);
ecierthonLIB_API int (ecierthonL_callmeta) (ecierthon_State *L, int obj, const char *e);
ecierthonLIB_API const char *(ecierthonL_tolstring) (ecierthon_State *L, int idx, size_t *len);
ecierthonLIB_API int (ecierthonL_argerror) (ecierthon_State *L, int arg, const char *extramsg);
ecierthonLIB_API int (ecierthonL_typeerror) (ecierthon_State *L, int arg, const char *tname);
ecierthonLIB_API const char *(ecierthonL_checklstring) (ecierthon_State *L, int arg,
                                                          size_t *l);
ecierthonLIB_API const char *(ecierthonL_optlstring) (ecierthon_State *L, int arg,
                                          const char *def, size_t *l);
ecierthonLIB_API ecierthon_Number (ecierthonL_checknumber) (ecierthon_State *L, int arg);
ecierthonLIB_API ecierthon_Number (ecierthonL_optnumber) (ecierthon_State *L, int arg, ecierthon_Number def);

ecierthonLIB_API ecierthon_Integer (ecierthonL_checkinteger) (ecierthon_State *L, int arg);
ecierthonLIB_API ecierthon_Integer (ecierthonL_optinteger) (ecierthon_State *L, int arg,
                                          ecierthon_Integer def);

ecierthonLIB_API void (ecierthonL_checkstack) (ecierthon_State *L, int sz, const char *msg);
ecierthonLIB_API void (ecierthonL_checktype) (ecierthon_State *L, int arg, int t);
ecierthonLIB_API void (ecierthonL_checkany) (ecierthon_State *L, int arg);

ecierthonLIB_API int   (ecierthonL_newmetatable) (ecierthon_State *L, const char *tname);
ecierthonLIB_API void  (ecierthonL_setmetatable) (ecierthon_State *L, const char *tname);
ecierthonLIB_API void *(ecierthonL_testudata) (ecierthon_State *L, int ud, const char *tname);
ecierthonLIB_API void *(ecierthonL_checkudata) (ecierthon_State *L, int ud, const char *tname);

ecierthonLIB_API void (ecierthonL_where) (ecierthon_State *L, int lvl);
ecierthonLIB_API int (ecierthonL_error) (ecierthon_State *L, const char *fmt, ...);

ecierthonLIB_API int (ecierthonL_checkoption) (ecierthon_State *L, int arg, const char *def,
                                   const char *const lst[]);

ecierthonLIB_API int (ecierthonL_fileresult) (ecierthon_State *L, int stat, const char *fname);
ecierthonLIB_API int (ecierthonL_execresult) (ecierthon_State *L, int stat);


/* predefined references */
#define ecierthon_NOREF       (-2)
#define ecierthon_REFNIL      (-1)

ecierthonLIB_API int (ecierthonL_ref) (ecierthon_State *L, int t);
ecierthonLIB_API void (ecierthonL_unref) (ecierthon_State *L, int t, int ref);

ecierthonLIB_API int (ecierthonL_loadfilex) (ecierthon_State *L, const char *filename,
                                               const char *mode);

#define ecierthonL_loadfile(L,f)	ecierthonL_loadfilex(L,f,NULL)

ecierthonLIB_API int (ecierthonL_loadbufferx) (ecierthon_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
ecierthonLIB_API int (ecierthonL_loadstring) (ecierthon_State *L, const char *s);

ecierthonLIB_API ecierthon_State *(ecierthonL_newstate) (void);

ecierthonLIB_API ecierthon_Integer (ecierthonL_len) (ecierthon_State *L, int idx);

ecierthonLIB_API void ecierthonL_addgsub (ecierthonL_Buffer *b, const char *s,
                                     const char *p, const char *r);
ecierthonLIB_API const char *(ecierthonL_gsub) (ecierthon_State *L, const char *s,
                                    const char *p, const char *r);

ecierthonLIB_API void (ecierthonL_setfuncs) (ecierthon_State *L, const ecierthonL_Reg *l, int nup);

ecierthonLIB_API int (ecierthonL_getsubtable) (ecierthon_State *L, int idx, const char *fname);

ecierthonLIB_API void (ecierthonL_traceback) (ecierthon_State *L, ecierthon_State *L1,
                                  const char *msg, int level);

ecierthonLIB_API void (ecierthonL_requiref) (ecierthon_State *L, const char *modname,
                                 ecierthon_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define ecierthonL_newlibtable(L,l)	\
  ecierthon_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define ecierthonL_newlib(L,l)  \
  (ecierthonL_checkversion(L), ecierthonL_newlibtable(L,l), ecierthonL_setfuncs(L,l,0))

#define ecierthonL_argcheck(L, cond,arg,extramsg)	\
		((void)((cond) || ecierthonL_argerror(L, (arg), (extramsg))))

#define ecierthonL_argexpected(L,cond,arg,tname)	\
		((void)((cond) || ecierthonL_typeerror(L, (arg), (tname))))

#define ecierthonL_checkstring(L,n)	(ecierthonL_checklstring(L, (n), NULL))
#define ecierthonL_optstring(L,n,d)	(ecierthonL_optlstring(L, (n), (d), NULL))

#define ecierthonL_typename(L,i)	ecierthon_typename(L, ecierthon_type(L,(i)))

#define ecierthonL_dofile(L, fn) \
	(ecierthonL_loadfile(L, fn) || ecierthon_pcall(L, 0, ecierthon_MULTRET, 0))

#define ecierthonL_dostring(L, s) \
	(ecierthonL_loadstring(L, s) || ecierthon_pcall(L, 0, ecierthon_MULTRET, 0))

#define ecierthonL_getmetatable(L,n)	(ecierthon_getfield(L, ecierthon_REGISTRYINDEX, (n)))

#define ecierthonL_opt(L,f,n,d)	(ecierthon_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define ecierthonL_loadbuffer(L,s,sz,n)	ecierthonL_loadbufferx(L,s,sz,n,NULL)


/* push the value used to represent failure/error */
#define ecierthonL_pushfail(L)	ecierthon_pushnil(L)


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct ecierthonL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  ecierthon_State *L;
  union {
    ecierthonI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[ecierthonL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define ecierthonL_bufflen(bf)	((bf)->n)
#define ecierthonL_buffaddr(bf)	((bf)->b)


#define ecierthonL_addchar(B,c) \
  ((void)((B)->n < (B)->size || ecierthonL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define ecierthonL_addsize(B,s)	((B)->n += (s))

#define ecierthonL_buffsub(B,s)	((B)->n -= (s))

ecierthonLIB_API void (ecierthonL_buffinit) (ecierthon_State *L, ecierthonL_Buffer *B);
ecierthonLIB_API char *(ecierthonL_prepbuffsize) (ecierthonL_Buffer *B, size_t sz);
ecierthonLIB_API void (ecierthonL_addlstring) (ecierthonL_Buffer *B, const char *s, size_t l);
ecierthonLIB_API void (ecierthonL_addstring) (ecierthonL_Buffer *B, const char *s);
ecierthonLIB_API void (ecierthonL_addvalue) (ecierthonL_Buffer *B);
ecierthonLIB_API void (ecierthonL_pushresult) (ecierthonL_Buffer *B);
ecierthonLIB_API void (ecierthonL_pushresultsize) (ecierthonL_Buffer *B, size_t sz);
ecierthonLIB_API char *(ecierthonL_buffinitsize) (ecierthon_State *L, ecierthonL_Buffer *B, size_t sz);

#define ecierthonL_prepbuffer(B)	ecierthonL_prepbuffsize(B, ecierthonL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'ecierthon_FILEHANDLE' and
** initial structure 'ecierthonL_Stream' (it may contain other fields
** after that initial structure).
*/

#define ecierthon_FILEHANDLE          "FILE*"


typedef struct ecierthonL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  ecierthon_CFunction closef;  /* to close stream (NULL for closed streams) */
} ecierthonL_Stream;

/* }====================================================== */

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(ecierthon_writestring)
#define ecierthon_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(ecierthon_writeline)
#define ecierthon_writeline()        (ecierthon_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(ecierthon_writestringerror)
#define ecierthon_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(ecierthon_COMPAT_APIINTCASTS)

#define ecierthonL_checkunsigned(L,a)	((ecierthon_Unsigned)ecierthonL_checkinteger(L,a))
#define ecierthonL_optunsigned(L,a,d)	\
	((ecierthon_Unsigned)ecierthonL_optinteger(L,a,(ecierthon_Integer)(d)))

#define ecierthonL_checkint(L,n)	((int)ecierthonL_checkinteger(L, (n)))
#define ecierthonL_optint(L,n,d)	((int)ecierthonL_optinteger(L, (n), (d)))

#define ecierthonL_checklong(L,n)	((long)ecierthonL_checkinteger(L, (n)))
#define ecierthonL_optlong(L,n,d)	((long)ecierthonL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif


