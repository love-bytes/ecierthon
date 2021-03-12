/*
* See Copyright Notice at the end of this file
*/


#ifndef ecierthon_h
#define ecierthon_h

#include <stdarg.h>
#include <stddef.h>


#include "ecierthonconf.h"


#define ecierthon_VERSION_MAJOR	"5"
#define ecierthon_VERSION_MINOR	"4"
#define ecierthon_VERSION_RELEASE	"2"

#define ecierthon_VERSION_NUM			504
#define ecierthon_VERSION_RELEASE_NUM		(ecierthon_VERSION_NUM * 100 + 0)

#define ecierthon_VERSION	"ecierthon " ecierthon_VERSION_MAJOR "." ecierthon_VERSION_MINOR
#define ecierthon_RELEASE	ecierthon_VERSION "." ecierthon_VERSION_RELEASE
#define ecierthon_COPYRIGHT	ecierthon_RELEASE "  Copyright (C) 2021 ecierthon"
#define ecierthon_AUTHORS	"cfl_mit"


/* mark for precompiled code ('<esc>ecierthon') */
#define LUA_SIGNATURE	"\x1blua"

/* option for multiple returns in 'ecierthon_pcall' and 'ecierthon_call' */
#define ecierthon_MULTRET	(-1)


/*
** Pseudo-indices
** (-ecierthonI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define ecierthon_REGISTRYINDEX	(-ecierthonI_MAXSTACK - 1000)
#define ecierthon_upvalueindex(i)	(ecierthon_REGISTRYINDEX - (i))


/* thread status */
#define ecierthon_OK		0
#define ecierthon_YIELD	1
#define ecierthon_ERRRUN	2
#define ecierthon_ERRSYNTAX	3
#define ecierthon_ERRMEM	4
#define ecierthon_ERRERR	5


typedef struct ecierthon_State ecierthon_State;


/*
** basic types
*/
#define ecierthon_TNONE		(-1)

#define ecierthon_TNIL		0
#define ecierthon_TBOOLEAN		1
#define ecierthon_TLIGHTUSERDATA	2
#define ecierthon_TNUMBER		3
#define ecierthon_TSTRING		4
#define ecierthon_TTABLE		5
#define ecierthon_TFUNCTION		6
#define ecierthon_TUSERDATA		7
#define ecierthon_TTHREAD		8

#define ecierthon_NUMTYPES		9



/* minimum ecierthon stack available to a C function */
#define ecierthon_MINSTACK	20


/* predefined values in the registry */
#define ecierthon_RIDX_MAINTHREAD	1
#define ecierthon_RIDX_GLOBALS	2
#define ecierthon_RIDX_LAST		ecierthon_RIDX_GLOBALS


/* type of numbers in ecierthon */
typedef ecierthon_NUMBER ecierthon_Number;


/* type for integer functions */
typedef ecierthon_INTEGER ecierthon_Integer;

/* unsigned integer type */
typedef ecierthon_UNSIGNED ecierthon_Unsigned;

/* type for continuation-function contexts */
typedef ecierthon_KCONTEXT ecierthon_KContext;


/*
** Type for C functions registered with ecierthon
*/
typedef int (*ecierthon_CFunction) (ecierthon_State *L);

/*
** Type for continuation functions
*/
typedef int (*ecierthon_KFunction) (ecierthon_State *L, int status, ecierthon_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping ecierthon chunks
*/
typedef const char * (*ecierthon_Reader) (ecierthon_State *L, void *ud, size_t *sz);

typedef int (*ecierthon_Writer) (ecierthon_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*ecierthon_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*ecierthon_WarnFunction) (void *ud, const char *msg, int tocont);




/*
** generic extra include file
*/
#if defined(ecierthon_USER_H)
#include ecierthon_USER_H
#endif


/*
** RCS ident string
*/
extern const char ecierthon_ident[];


/*
** state manipulation
*/
ecierthon_API ecierthon_State *(ecierthon_newstate) (ecierthon_Alloc f, void *ud);
ecierthon_API void       (ecierthon_close) (ecierthon_State *L);
ecierthon_API ecierthon_State *(ecierthon_newthread) (ecierthon_State *L);
ecierthon_API int        (ecierthon_resetthread) (ecierthon_State *L);

ecierthon_API ecierthon_CFunction (ecierthon_atpanic) (ecierthon_State *L, ecierthon_CFunction panicf);


ecierthon_API ecierthon_Number (ecierthon_version) (ecierthon_State *L);


/*
** basic stack manipulation
*/
ecierthon_API int   (ecierthon_absindex) (ecierthon_State *L, int idx);
ecierthon_API int   (ecierthon_gettop) (ecierthon_State *L);
ecierthon_API void  (ecierthon_settop) (ecierthon_State *L, int idx);
ecierthon_API void  (ecierthon_pushvalue) (ecierthon_State *L, int idx);
ecierthon_API void  (ecierthon_rotate) (ecierthon_State *L, int idx, int n);
ecierthon_API void  (ecierthon_copy) (ecierthon_State *L, int fromidx, int toidx);
ecierthon_API int   (ecierthon_checkstack) (ecierthon_State *L, int n);

ecierthon_API void  (ecierthon_xmove) (ecierthon_State *from, ecierthon_State *to, int n);


/*
** access functions (stack -> C)
*/

ecierthon_API int             (ecierthon_isnumber) (ecierthon_State *L, int idx);
ecierthon_API int             (ecierthon_isstring) (ecierthon_State *L, int idx);
ecierthon_API int             (ecierthon_iscfunction) (ecierthon_State *L, int idx);
ecierthon_API int             (ecierthon_isinteger) (ecierthon_State *L, int idx);
ecierthon_API int             (ecierthon_isuserdata) (ecierthon_State *L, int idx);
ecierthon_API int             (ecierthon_type) (ecierthon_State *L, int idx);
ecierthon_API const char     *(ecierthon_typename) (ecierthon_State *L, int tp);

ecierthon_API ecierthon_Number      (ecierthon_tonumberx) (ecierthon_State *L, int idx, int *isnum);
ecierthon_API ecierthon_Integer     (ecierthon_tointegerx) (ecierthon_State *L, int idx, int *isnum);
ecierthon_API int             (ecierthon_toboolean) (ecierthon_State *L, int idx);
ecierthon_API const char     *(ecierthon_tolstring) (ecierthon_State *L, int idx, size_t *len);
ecierthon_API ecierthon_Unsigned    (ecierthon_rawlen) (ecierthon_State *L, int idx);
ecierthon_API ecierthon_CFunction   (ecierthon_tocfunction) (ecierthon_State *L, int idx);
ecierthon_API void	       *(ecierthon_touserdata) (ecierthon_State *L, int idx);
ecierthon_API ecierthon_State      *(ecierthon_tothread) (ecierthon_State *L, int idx);
ecierthon_API const void     *(ecierthon_topointer) (ecierthon_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define ecierthon_OPADD	0	/* ORDER TM, ORDER OP */
#define ecierthon_OPSUB	1
#define ecierthon_OPMUL	2
#define ecierthon_OPMOD	3
#define ecierthon_OPPOW	4
#define ecierthon_OPDIV	5
#define ecierthon_OPIDIV	6
#define ecierthon_OPBAND	7
#define ecierthon_OPBOR	8
#define ecierthon_OPBXOR	9
#define ecierthon_OPSHL	10
#define ecierthon_OPSHR	11
#define ecierthon_OPUNM	12
#define ecierthon_OPBNOT	13

ecierthon_API void  (ecierthon_arith) (ecierthon_State *L, int op);

#define ecierthon_OPEQ	0
#define ecierthon_OPLT	1
#define ecierthon_OPLE	2

ecierthon_API int   (ecierthon_rawequal) (ecierthon_State *L, int idx1, int idx2);
ecierthon_API int   (ecierthon_compare) (ecierthon_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
ecierthon_API void        (ecierthon_pushnil) (ecierthon_State *L);
ecierthon_API void        (ecierthon_pushnumber) (ecierthon_State *L, ecierthon_Number n);
ecierthon_API void        (ecierthon_pushinteger) (ecierthon_State *L, ecierthon_Integer n);
ecierthon_API const char *(ecierthon_pushlstring) (ecierthon_State *L, const char *s, size_t len);
ecierthon_API const char *(ecierthon_pushstring) (ecierthon_State *L, const char *s);
ecierthon_API const char *(ecierthon_pushvfstring) (ecierthon_State *L, const char *fmt,
                                                      va_list argp);
ecierthon_API const char *(ecierthon_pushfstring) (ecierthon_State *L, const char *fmt, ...);
ecierthon_API void  (ecierthon_pushcclosure) (ecierthon_State *L, ecierthon_CFunction fn, int n);
ecierthon_API void  (ecierthon_pushboolean) (ecierthon_State *L, int b);
ecierthon_API void  (ecierthon_pushlightuserdata) (ecierthon_State *L, void *p);
ecierthon_API int   (ecierthon_pushthread) (ecierthon_State *L);


/*
** get functions (ecierthon -> stack)
*/
ecierthon_API int (ecierthon_getglobal) (ecierthon_State *L, const char *name);
ecierthon_API int (ecierthon_gettable) (ecierthon_State *L, int idx);
ecierthon_API int (ecierthon_getfield) (ecierthon_State *L, int idx, const char *k);
ecierthon_API int (ecierthon_geti) (ecierthon_State *L, int idx, ecierthon_Integer n);
ecierthon_API int (ecierthon_rawget) (ecierthon_State *L, int idx);
ecierthon_API int (ecierthon_rawgeti) (ecierthon_State *L, int idx, ecierthon_Integer n);
ecierthon_API int (ecierthon_rawgetp) (ecierthon_State *L, int idx, const void *p);

ecierthon_API void  (ecierthon_createtable) (ecierthon_State *L, int narr, int nrec);
ecierthon_API void *(ecierthon_newuserdatauv) (ecierthon_State *L, size_t sz, int nuvalue);
ecierthon_API int   (ecierthon_getmetatable) (ecierthon_State *L, int objindex);
ecierthon_API int  (ecierthon_getiuservalue) (ecierthon_State *L, int idx, int n);


/*
** set functions (stack -> ecierthon)
*/
ecierthon_API void  (ecierthon_setglobal) (ecierthon_State *L, const char *name);
ecierthon_API void  (ecierthon_settable) (ecierthon_State *L, int idx);
ecierthon_API void  (ecierthon_setfield) (ecierthon_State *L, int idx, const char *k);
ecierthon_API void  (ecierthon_seti) (ecierthon_State *L, int idx, ecierthon_Integer n);
ecierthon_API void  (ecierthon_rawset) (ecierthon_State *L, int idx);
ecierthon_API void  (ecierthon_rawseti) (ecierthon_State *L, int idx, ecierthon_Integer n);
ecierthon_API void  (ecierthon_rawsetp) (ecierthon_State *L, int idx, const void *p);
ecierthon_API int   (ecierthon_setmetatable) (ecierthon_State *L, int objindex);
ecierthon_API int   (ecierthon_setiuservalue) (ecierthon_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run ecierthon code)
*/
ecierthon_API void  (ecierthon_callk) (ecierthon_State *L, int nargs, int nresults,
                           ecierthon_KContext ctx, ecierthon_KFunction k);
#define ecierthon_call(L,n,r)		ecierthon_callk(L, (n), (r), 0, NULL)

ecierthon_API int   (ecierthon_pcallk) (ecierthon_State *L, int nargs, int nresults, int errfunc,
                            ecierthon_KContext ctx, ecierthon_KFunction k);
#define ecierthon_pcall(L,n,r,f)	ecierthon_pcallk(L, (n), (r), (f), 0, NULL)

ecierthon_API int   (ecierthon_load) (ecierthon_State *L, ecierthon_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

ecierthon_API int (ecierthon_dump) (ecierthon_State *L, ecierthon_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
ecierthon_API int  (ecierthon_yieldk)     (ecierthon_State *L, int nresults, ecierthon_KContext ctx,
                               ecierthon_KFunction k);
ecierthon_API int  (ecierthon_resume)     (ecierthon_State *L, ecierthon_State *from, int narg,
                               int *nres);
ecierthon_API int  (ecierthon_status)     (ecierthon_State *L);
ecierthon_API int (ecierthon_isyieldable) (ecierthon_State *L);

#define ecierthon_yield(L,n)		ecierthon_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
ecierthon_API void (ecierthon_setwarnf) (ecierthon_State *L, ecierthon_WarnFunction f, void *ud);
ecierthon_API void (ecierthon_warning)  (ecierthon_State *L, const char *msg, int tocont);


/*
** garbage-collection function and options
*/

#define ecierthon_GCSTOP		0
#define ecierthon_GCRESTART		1
#define ecierthon_GCCOLLECT		2
#define ecierthon_GCCOUNT		3
#define ecierthon_GCCOUNTB		4
#define ecierthon_GCSTEP		5
#define ecierthon_GCSETPAUSE		6
#define ecierthon_GCSETSTEPMUL	7
#define ecierthon_GCISRUNNING		9
#define ecierthon_GCGEN		10
#define ecierthon_GCINC		11

ecierthon_API int (ecierthon_gc) (ecierthon_State *L, int what, ...);


/*
** miscellaneous functions
*/

ecierthon_API int   (ecierthon_error) (ecierthon_State *L);

ecierthon_API int   (ecierthon_next) (ecierthon_State *L, int idx);

ecierthon_API void  (ecierthon_concat) (ecierthon_State *L, int n);
ecierthon_API void  (ecierthon_len)    (ecierthon_State *L, int idx);

ecierthon_API size_t   (ecierthon_stringtonumber) (ecierthon_State *L, const char *s);

ecierthon_API ecierthon_Alloc (ecierthon_getallocf) (ecierthon_State *L, void **ud);
ecierthon_API void      (ecierthon_setallocf) (ecierthon_State *L, ecierthon_Alloc f, void *ud);

ecierthon_API void  (ecierthon_toclose) (ecierthon_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define ecierthon_getextraspace(L)	((void *)((char *)(L) - ecierthon_EXTRASPACE))

#define ecierthon_tonumber(L,i)	ecierthon_tonumberx(L,(i),NULL)
#define ecierthon_tointeger(L,i)	ecierthon_tointegerx(L,(i),NULL)

#define ecierthon_pop(L,n)		ecierthon_settop(L, -(n)-1)

#define ecierthon_newtable(L)		ecierthon_createtable(L, 0, 0)

#define ecierthon_register(L,n,f) (ecierthon_pushcfunction(L, (f)), ecierthon_setglobal(L, (n)))

#define ecierthon_pushcfunction(L,f)	ecierthon_pushcclosure(L, (f), 0)

#define ecierthon_isfunction(L,n)	(ecierthon_type(L, (n)) == ecierthon_TFUNCTION)
#define ecierthon_istable(L,n)	(ecierthon_type(L, (n)) == ecierthon_TTABLE)
#define ecierthon_islightuserdata(L,n)	(ecierthon_type(L, (n)) == ecierthon_TLIGHTUSERDATA)
#define ecierthon_isnil(L,n)		(ecierthon_type(L, (n)) == ecierthon_TNIL)
#define ecierthon_isboolean(L,n)	(ecierthon_type(L, (n)) == ecierthon_TBOOLEAN)
#define ecierthon_isthread(L,n)	(ecierthon_type(L, (n)) == ecierthon_TTHREAD)
#define ecierthon_isnone(L,n)		(ecierthon_type(L, (n)) == ecierthon_TNONE)
#define ecierthon_isnoneornil(L, n)	(ecierthon_type(L, (n)) <= 0)

#define ecierthon_pushliteral(L, s)	ecierthon_pushstring(L, "" s)

#define ecierthon_pushglobaltable(L)  \
	((void)ecierthon_rawgeti(L, ecierthon_REGISTRYINDEX, ecierthon_RIDX_GLOBALS))

#define ecierthon_tostring(L,i)	ecierthon_tolstring(L, (i), NULL)


#define ecierthon_insert(L,idx)	ecierthon_rotate(L, (idx), 1)

#define ecierthon_remove(L,idx)	(ecierthon_rotate(L, (idx), -1), ecierthon_pop(L, 1))

#define ecierthon_replace(L,idx)	(ecierthon_copy(L, -1, (idx)), ecierthon_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(ecierthon_COMPAT_APIINTCASTS)

#define ecierthon_pushunsigned(L,n)	ecierthon_pushinteger(L, (ecierthon_Integer)(n))
#define ecierthon_tounsignedx(L,i,is)	((ecierthon_Unsigned)ecierthon_tointegerx(L,i,is))
#define ecierthon_tounsigned(L,i)	ecierthon_tounsignedx(L,(i),NULL)

#endif

#define ecierthon_newuserdata(L,s)	ecierthon_newuserdatauv(L,s,1)
#define ecierthon_getuservalue(L,idx)	ecierthon_getiuservalue(L,idx,1)
#define ecierthon_setuservalue(L,idx)	ecierthon_setiuservalue(L,idx,1)

#define ecierthon_NUMTAGS		ecierthon_NUMTYPES

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define ecierthon_HOOKCALL	0
#define ecierthon_HOOKRET	1
#define ecierthon_HOOKLINE	2
#define ecierthon_HOOKCOUNT	3
#define ecierthon_HOOKTAILCALL 4


/*
** Event masks
*/
#define ecierthon_MASKCALL	(1 << ecierthon_HOOKCALL)
#define ecierthon_MASKRET	(1 << ecierthon_HOOKRET)
#define ecierthon_MASKLINE	(1 << ecierthon_HOOKLINE)
#define ecierthon_MASKCOUNT	(1 << ecierthon_HOOKCOUNT)

typedef struct ecierthon_Debug ecierthon_Debug;  /* activation record */


/* Functions to be called by the debugger in specific events */
typedef void (*ecierthon_Hook) (ecierthon_State *L, ecierthon_Debug *ar);


ecierthon_API int (ecierthon_getstack) (ecierthon_State *L, int level, ecierthon_Debug *ar);
ecierthon_API int (ecierthon_getinfo) (ecierthon_State *L, const char *what, ecierthon_Debug *ar);
ecierthon_API const char *(ecierthon_getlocal) (ecierthon_State *L, const ecierthon_Debug *ar, int n);
ecierthon_API const char *(ecierthon_setlocal) (ecierthon_State *L, const ecierthon_Debug *ar, int n);
ecierthon_API const char *(ecierthon_getupvalue) (ecierthon_State *L, int funcindex, int n);
ecierthon_API const char *(ecierthon_setupvalue) (ecierthon_State *L, int funcindex, int n);

ecierthon_API void *(ecierthon_upvalueid) (ecierthon_State *L, int fidx, int n);
ecierthon_API void  (ecierthon_upvaluejoin) (ecierthon_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

ecierthon_API void (ecierthon_sethook) (ecierthon_State *L, ecierthon_Hook func, int mask, int count);
ecierthon_API ecierthon_Hook (ecierthon_gethook) (ecierthon_State *L);
ecierthon_API int (ecierthon_gethookmask) (ecierthon_State *L);
ecierthon_API int (ecierthon_gethookcount) (ecierthon_State *L);

ecierthon_API int (ecierthon_setcstacklimit) (ecierthon_State *L, unsigned int limit);

struct ecierthon_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'ecierthon', 'C', 'main', 'tail' */
  const char *source;	/* (S) */
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  char istailcall;	/* (t) */
  unsigned short ftransfer;   /* (r) index of first value transferred */
  unsigned short ntransfer;   /* (r) number of transferred values */
  char short_src[ecierthon_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};

/* }====================================================================== */


/******************************************************************************
* Copyright (C) 1994-2020 ecierthon.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/


#endif
