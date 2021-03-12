/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in ecierthon.h
*/

#ifndef llimits_h
#define llimits_h


#include <limits.h>
#include <stddef.h>


#include "ecierthon.h"


/*
** 'lu_mem' and 'l_mem' are unsigned/signed integers big enough to count
** the total memory used by ecierthon (in bytes). Usually, 'size_t' and
** 'ptrdiff_t' should work, but we use 'long' for 16-bit machines.
*/
#if defined(ecierthonI_MEM)		/* { external definitions? */
typedef ecierthonI_UMEM lu_mem;
typedef ecierthonI_MEM l_mem;
#elif ecierthonI_IS32INT	/* }{ */
typedef size_t lu_mem;
typedef ptrdiff_t l_mem;
#else  /* 16-bit ints */	/* }{ */
typedef unsigned long lu_mem;
typedef long l_mem;
#endif				/* } */


/* chars used as small naturals (so that 'char' is reserved for characters) */
typedef unsigned char lu_byte;
typedef signed char ls_byte;


/* maximum value for size_t */
#define MAX_SIZET	((size_t)(~(size_t)0))

/* maximum size visible for ecierthon (must be representable in a ecierthon_Integer) */
#define MAX_SIZE	(sizeof(size_t) < sizeof(ecierthon_Integer) ? MAX_SIZET \
                          : (size_t)(ecierthon_MAXINTEGER))


#define MAX_LUMEM	((lu_mem)(~(lu_mem)0))

#define MAX_LMEM	((l_mem)(MAX_LUMEM >> 1))


#define MAX_INT		INT_MAX  /* maximum value of an int */


/*
** floor of the log2 of the maximum signed value for integral type 't'.
** (That is, maximum 'n' such that '2^n' fits in the given signed type.)
*/
#define log2maxs(t)	(sizeof(t) * 8 - 2)


/*
** test whether an unsigned value is a power of 2 (or zero)
*/
#define ispow2(x)	(((x) & ((x) - 1)) == 0)


/* number of chars of a literal string without the ending \0 */
#define LL(x)   (sizeof(x)/sizeof(char) - 1)


/*
** conversion of pointer to unsigned integer:
** this is for hashing only; there is no problem if the integer
** cannot hold the whole pointer value
*/
#define point2uint(p)	((unsigned int)((size_t)(p) & UINT_MAX))



/* types of 'usual argument conversions' for ecierthon_Number and ecierthon_Integer */
typedef ecierthonI_UACNUMBER l_uacNumber;
typedef ecierthonI_UACINT l_uacInt;


/*
** Internal assertions for in-house debugging
*/
#if defined ecierthonI_ASSERT
#undef NDEBUG
#include <assert.h>
#define ecierthon_assert(c)           assert(c)
#endif

#if defined(ecierthon_assert)
#define check_exp(c,e)		(ecierthon_assert(c), (e))
/* to avoid problems with conditions too long */
#define ecierthon_longassert(c)	((c) ? (void)0 : ecierthon_assert(0))
#else
#define ecierthon_assert(c)		((void)0)
#define check_exp(c,e)		(e)
#define ecierthon_longassert(c)	((void)0)
#endif

/*
** assertion for checking API calls
*/
#if !defined(ecierthoni_apicheck)
#define ecierthoni_apicheck(l,e)	((void)l, ecierthon_assert(e))
#endif

#define api_check(l,e,msg)	ecierthoni_apicheck(l,(e) && msg)


/* macro to avoid warnings about unused variables */
#if !defined(UNUSED)
#define UNUSED(x)	((void)(x))
#endif


/* type casts (a macro highlights casts in the code) */
#define cast(t, exp)	((t)(exp))

#define cast_void(i)	cast(void, (i))
#define cast_voidp(i)	cast(void *, (i))
#define cast_num(i)	cast(ecierthon_Number, (i))
#define cast_int(i)	cast(int, (i))
#define cast_uint(i)	cast(unsigned int, (i))
#define cast_byte(i)	cast(lu_byte, (i))
#define cast_uchar(i)	cast(unsigned char, (i))
#define cast_char(i)	cast(char, (i))
#define cast_charp(i)	cast(char *, (i))
#define cast_sizet(i)	cast(size_t, (i))


/* cast a signed ecierthon_Integer to ecierthon_Unsigned */
#if !defined(l_castS2U)
#define l_castS2U(i)	((ecierthon_Unsigned)(i))
#endif

/*
** cast a ecierthon_Unsigned to a signed ecierthon_Integer; this cast is
** not strict ISO C, but two-complement architectures should
** work fine.
*/
#if !defined(l_castU2S)
#define l_castU2S(i)	((ecierthon_Integer)(i))
#endif


/*
** macros to improve jump prediction (used mainly for error handling)
*/
#if !defined(likely)

#if defined(__GNUC__)
#define likely(x)	(__builtin_expect(((x) != 0), 1))
#define unlikely(x)	(__builtin_expect(((x) != 0), 0))
#else
#define likely(x)	(x)
#define unlikely(x)	(x)
#endif

#endif


/*
** non-return type
*/
#if !defined(l_noret)

#if defined(__GNUC__)
#define l_noret		void __attribute__((noreturn))
#elif defined(_MSC_VER) && _MSC_VER >= 1200
#define l_noret		void __declspec(noreturn)
#else
#define l_noret		void
#endif

#endif


/*
** type for virtual-machine instructions;
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
*/
#if ecierthonI_IS32INT
typedef unsigned int l_uint32;
#else
typedef unsigned long l_uint32;
#endif

typedef l_uint32 Instruction;



/*
** Maximum length for short strings, that is, strings that are
** internalized. (Cannot be smaller than reserved words or tags for
** metamethods, as these strings must be internalized;
** #("function") = 8, #("__newindex") = 10.)
*/
#if !defined(ecierthonI_MAXSHORTLEN)
#define ecierthonI_MAXSHORTLEN	40
#endif


/*
** Initial size for the string table (must be power of 2).
** The ecierthon core alone registers ~50 strings (reserved words +
** metaevent keys + a few others). Libraries would typically add
** a few dozens more.
*/
#if !defined(MINSTRTABSIZE)
#define MINSTRTABSIZE	128
#endif


/*
** Size of cache for strings in the API. 'N' is the number of
** sets (better be a prime) and "M" is the size of each set (M == 1
** makes a direct cache.)
*/
#if !defined(STRCACHE_N)
#define STRCACHE_N		53
#define STRCACHE_M		2
#endif


/* minimum size for string buffer */
#if !defined(ecierthon_MINBUFFER)
#define ecierthon_MINBUFFER	32
#endif


/*
** Maximum depth for nested C calls, syntactical nested non-terminals,
** and other features implemented through recursion in C. (Value must
** fit in a 16-bit unsigned integer. It must also be compatible with
** the size of the C stack.)
*/
#if !defined(ecierthonI_MAXCCALLS)
#define ecierthonI_MAXCCALLS		200
#endif


/*
** macros that are executed whenever program enters the ecierthon core
** ('ecierthon_lock') and leaves the core ('ecierthon_unlock')
*/
#if !defined(ecierthon_lock)
#define ecierthon_lock(L)	((void) 0)
#define ecierthon_unlock(L)	((void) 0)
#endif

/*
** macro executed during ecierthon functions at points where the
** function can yield.
*/
#if !defined(ecierthoni_threadyield)
#define ecierthoni_threadyield(L)	{ecierthon_unlock(L); ecierthon_lock(L);}
#endif


/*
** these macros allow user-specific actions when a thread is
** created/deleted/resumed/yielded.
*/
#if !defined(ecierthoni_userstateopen)
#define ecierthoni_userstateopen(L)		((void)L)
#endif

#if !defined(ecierthoni_userstateclose)
#define ecierthoni_userstateclose(L)		((void)L)
#endif

#if !defined(ecierthoni_userstatethread)
#define ecierthoni_userstatethread(L,L1)	((void)L)
#endif

#if !defined(ecierthoni_userstatefree)
#define ecierthoni_userstatefree(L,L1)	((void)L)
#endif

#if !defined(ecierthoni_userstateresume)
#define ecierthoni_userstateresume(L,n)	((void)L)
#endif

#if !defined(ecierthoni_userstateyield)
#define ecierthoni_userstateyield(L,n)	((void)L)
#endif



/*
** The ecierthoni_num* macros define the primitive operations over numbers.
*/

/* floor division (defined as 'floor(a/b)') */
#if !defined(ecierthoni_numidiv)
#define ecierthoni_numidiv(L,a,b)     ((void)L, l_floor(ecierthoni_numdiv(L,a,b)))
#endif

/* float division */
#if !defined(ecierthoni_numdiv)
#define ecierthoni_numdiv(L,a,b)      ((a)/(b))
#endif

/*
** modulo: defined as 'a - floor(a/b)*b'; the direct computation
** using this definition has several problems with rounding errors,
** so it is better to use 'fmod'. 'fmod' gives the result of
** 'a - trunc(a/b)*b', and therefore must be corrected when
** 'trunc(a/b) ~= floor(a/b)'. That happens when the division has a
** non-integer negative result: non-integer result is equivalent to
** a non-zero remainder 'm'; negative result is equivalent to 'a' and
** 'b' with different signs, or 'm' and 'b' with different signs
** (as the result 'm' of 'fmod' has the same sign of 'a').
*/
#if !defined(ecierthoni_nummod)
#define ecierthoni_nummod(L,a,b,m)  \
  { (void)L; (m) = l_mathop(fmod)(a,b); \
    if (((m) > 0) ? (b) < 0 : ((m) < 0 && (b) > 0)) (m) += (b); }
#endif

/* exponentiation */
#if !defined(ecierthoni_numpow)
#define ecierthoni_numpow(L,a,b)  \
  ((void)L, (b == 2) ? (a)*(a) : l_mathop(pow)(a,b))
#endif

/* the others are quite standard operations */
#if !defined(ecierthoni_numadd)
#define ecierthoni_numadd(L,a,b)      ((a)+(b))
#define ecierthoni_numsub(L,a,b)      ((a)-(b))
#define ecierthoni_nummul(L,a,b)      ((a)*(b))
#define ecierthoni_numunm(L,a)        (-(a))
#define ecierthoni_numeq(a,b)         ((a)==(b))
#define ecierthoni_numlt(a,b)         ((a)<(b))
#define ecierthoni_numle(a,b)         ((a)<=(b))
#define ecierthoni_numgt(a,b)         ((a)>(b))
#define ecierthoni_numge(a,b)         ((a)>=(b))
#define ecierthoni_numisnan(a)        (!ecierthoni_numeq((a), (a)))
#endif





/*
** macro to control inclusion of some hard tests on stack reallocation
*/
#if !defined(HARDSTACKTESTS)
#define condmovestack(L,pre,pos)	((void)0)
#else
/* realloc stack keeping its size */
#define condmovestack(L,pre,pos)  \
  { int sz_ = stacksize(L); pre; ecierthonD_reallocstack((L), sz_, 0); pos; }
#endif

#if !defined(HARDMEMTESTS)
#define condchangemem(L,pre,pos)	((void)0)
#else
#define condchangemem(L,pre,pos)  \
	{ if (G(L)->gcrunning) { pre; ecierthonC_fullgc(L, 0); pos; } }
#endif

#endif
