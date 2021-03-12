/*
** $Id: ecierthonconf.h $
** Configuration file for ecierthon
** See Copyright Notice in ecierthon.h
*/


#ifndef ecierthonconf_h
#define ecierthonconf_h

#include <limits.h>
#include <stddef.h>


/*
** ===================================================================
** General Configuration File for ecierthon
**
** Some definitions here can be changed externally, through the
** compiler (e.g., with '-D' options). Those are protected by
** '#if !defined' guards. However, several other definitions should
** be changed directly here, either because they affect the ecierthon
** ABI (by making the changes here, you ensure that all software
** connected to ecierthon, such as C libraries, will be compiled with the
** same configuration); or because they are seldom changed.
**
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** {====================================================================
** System Configuration: macros to adapt (if needed) ecierthon to some
** particular platform, for instance restricting it to C89.
** =====================================================================
*/

/*
@@ ecierthon_USE_C89 controls the use of non-ISO-C89 features.
** Define it if you want ecierthon to avoid the use of a few C99 features
** or Windows-specific features on Windows.
*/
/* #define ecierthon_USE_C89 */


/*
** By default, ecierthon on Windows use (some) specific Windows features
*/
#if !defined(ecierthon_USE_C89) && defined(_WIN32) && !defined(_WIN32_WCE)
#define ecierthon_USE_WINDOWS  /* enable goodies for regular Windows */
#endif


#if defined(ecierthon_USE_WINDOWS)
#define ecierthon_DL_DLL	/* enable support for DLL */
#define ecierthon_USE_C89	/* broadly, Windows is C89 */
#endif


#if defined(ecierthon_USE_LINUX)
#define ecierthon_USE_POSIX
#define ecierthon_USE_DLOPEN		/* needs an extra library: -ldl */
#endif


#if defined(ecierthon_USE_MACOSX)
#define ecierthon_USE_POSIX
#define ecierthon_USE_DLOPEN		/* MacOS does not need -ldl */
#endif


/*
@@ ecierthonI_IS32INT is true iff 'int' has (at least) 32 bits.
*/
#define ecierthonI_IS32INT	((UINT_MAX >> 30) >= 3)

/* }================================================================== */



/*
** {==================================================================
** Configuration for Number types.
** ===================================================================
*/

/*
@@ ecierthon_32BITS enables ecierthon with 32-bit integers and 32-bit floats.
*/
/* #define ecierthon_32BITS */


/*
@@ ecierthon_C89_NUMBERS ensures that ecierthon uses the largest types available for
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** not need to use this case.
*/
#if defined(ecierthon_USE_C89) && !defined(ecierthon_USE_WINDOWS)
#define ecierthon_C89_NUMBERS
#endif


/*
@@ ecierthon_INT_TYPE defines the type for ecierthon integers.
@@ ecierthon_FLOAT_TYPE defines the type for ecierthon floats.
** ecierthon should work fine with any mix of these options supported
** by your C compiler. The usual configurations are 64-bit integers
** and 'double' (the default), 32-bit integers and 'float' (for
** restricted platforms), and 'long'/'double' (for C compilers not
** compliant with C99, which may not have support for 'long long').
*/

/* predefined options for ecierthon_INT_TYPE */
#define ecierthon_INT_INT		1
#define ecierthon_INT_LONG		2
#define ecierthon_INT_LONGLONG	3

/* predefined options for ecierthon_FLOAT_TYPE */
#define ecierthon_FLOAT_FLOAT		1
#define ecierthon_FLOAT_DOUBLE	2
#define ecierthon_FLOAT_LONGDOUBLE	3

#if defined(ecierthon_32BITS)		/* { */
/*
** 32-bit integers and 'float'
*/
#if ecierthonI_IS32INT  /* use 'int' if big enough */
#define ecierthon_INT_TYPE	ecierthon_INT_INT
#else  /* otherwise use 'long' */
#define ecierthon_INT_TYPE	ecierthon_INT_LONG
#endif
#define ecierthon_FLOAT_TYPE	ecierthon_FLOAT_FLOAT

#elif defined(ecierthon_C89_NUMBERS)	/* }{ */
/*
** largest types available for C89 ('long' and 'double')
*/
#define ecierthon_INT_TYPE	ecierthon_INT_LONG
#define ecierthon_FLOAT_TYPE	ecierthon_FLOAT_DOUBLE

#endif				/* } */


/*
** default configuration for 64-bit ecierthon ('long long' and 'double')
*/
#if !defined(ecierthon_INT_TYPE)
#define ecierthon_INT_TYPE	ecierthon_INT_LONGLONG
#endif

#if !defined(ecierthon_FLOAT_TYPE)
#define ecierthon_FLOAT_TYPE	ecierthon_FLOAT_DOUBLE
#endif

/* }================================================================== */



/*
** {==================================================================
** Configuration for Paths.
** ===================================================================
*/

/*
** ecierthon_PATH_SEP is the character that separates templates in a path.
** ecierthon_PATH_MARK is the string that marks the substitution points in a
** template.
** ecierthon_EXEC_DIR in a Windows path is replaced by the executable's
** directory.
*/
#define ecierthon_PATH_SEP            ";"
#define ecierthon_PATH_MARK           "?"
#define ecierthon_EXEC_DIR            "!"


/*
@@ ecierthon_PATH_DEFAULT is the default path that ecierthon uses to look for
** ecierthon libraries.
@@ ecierthon_CPATH_DEFAULT is the default path that ecierthon uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/

#define ecierthon_VDIR	ecierthon_VERSION_MAJOR "." ecierthon_VERSION_MINOR
#if defined(_WIN32)	/* { */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define ecierthon_LDIR	"!\\ecierthon\\"
#define ecierthon_CDIR	"!\\"
#define ecierthon_SHRDIR	"!\\..\\share\\ecierthon\\" ecierthon_VDIR "\\"

#if !defined(ecierthon_PATH_DEFAULT)
#define ecierthon_PATH_DEFAULT  \
		ecierthon_LDIR"?.ecierthon;"  ecierthon_LDIR"?\\init.ecierthon;" \
		ecierthon_CDIR"?.ecierthon;"  ecierthon_CDIR"?\\init.ecierthon;" \
		ecierthon_SHRDIR"?.ecierthon;" ecierthon_SHRDIR"?\\init.ecierthon;" \
		".\\?.ecierthon;" ".\\?\\init.ecierthon"
#endif

#if !defined(ecierthon_CPATH_DEFAULT)
#define ecierthon_CPATH_DEFAULT \
		ecierthon_CDIR"?.dll;" \
		ecierthon_CDIR"..\\lib\\ecierthon\\" ecierthon_VDIR "\\?.dll;" \
		ecierthon_CDIR"loadall.dll;" ".\\?.dll"
#endif

#else			/* }{ */

#define ecierthon_ROOT	"/usr/local/"
#define ecierthon_LDIR	ecierthon_ROOT "share/ecierthon/" ecierthon_VDIR "/"
#define ecierthon_CDIR	ecierthon_ROOT "lib/ecierthon/" ecierthon_VDIR "/"

#if !defined(ecierthon_PATH_DEFAULT)
#define ecierthon_PATH_DEFAULT  \
		ecierthon_LDIR"?.ecierthon;"  ecierthon_LDIR"?/init.ecierthon;" \
		ecierthon_CDIR"?.ecierthon;"  ecierthon_CDIR"?/init.ecierthon;" \
		"./?.ecierthon;" "./?/init.ecierthon"
#endif

#if !defined(ecierthon_CPATH_DEFAULT)
#define ecierthon_CPATH_DEFAULT \
		ecierthon_CDIR"?.so;" ecierthon_CDIR"loadall.so;" "./?.so"
#endif

#endif			/* } */


/*
@@ ecierthon_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows ecierthon automatically uses "\".)
*/
#if !defined(ecierthon_DIRSEP)

#if defined(_WIN32)
#define ecierthon_DIRSEP	"\\"
#else
#define ecierthon_DIRSEP	"/"
#endif

#endif

/* }================================================================== */


/*
** {==================================================================
** Marks for exported symbols in the C code
** ===================================================================
*/

/*
@@ ecierthon_API is a mark for all core API functions.
@@ ecierthonLIB_API is a mark for all auxiliary library functions.
@@ ecierthonMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** ecierthon_BUILD_AS_DLL to get it).
*/
#if defined(ecierthon_BUILD_AS_DLL)	/* { */

#if defined(ecierthon_CORE) || defined(ecierthon_LIB)	/* { */
#define ecierthon_API __declspec(dllexport)
#else						/* }{ */
#define ecierthon_API __declspec(dllimport)
#endif						/* } */

#else				/* }{ */

#define ecierthon_API		extern

#endif				/* } */


/*
** More often than not the libs go together with the core.
*/
#define ecierthonLIB_API	ecierthon_API
#define ecierthonMOD_API	ecierthon_API


/*
@@ ecierthonI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
@@ ecierthonI_DDEF and ecierthonI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (ecierthonI_DDEF for
** definitions and ecierthonI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when ecierthon is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/
#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		/* { */
#define ecierthonI_FUNC	__attribute__((visibility("internal"))) extern
#else				/* }{ */
#define ecierthonI_FUNC	extern
#endif				/* } */

#define ecierthonI_DDEC(dec)	ecierthonI_FUNC dec
#define ecierthonI_DDEF	/* empty */

/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ ecierthon_COMPAT_5_3 controls other macros for compatibility with ecierthon 5.3.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/
#if defined(ecierthon_COMPAT_5_3)	/* { */

/*
@@ ecierthon_COMPAT_MATHLIB controls the presence of several deprecated
** functions in the mathematical library.
** (These functions were already officially removed in 5.3;
** nevertheless they are still available here.)
*/
#define ecierthon_COMPAT_MATHLIB

/*
@@ ecierthon_COMPAT_APIINTCASTS controls the presence of macros for
** manipulating other integer types (ecierthon_pushunsigned, ecierthon_tounsigned,
** ecierthonL_checkint, ecierthonL_checklong, etc.)
** (These macros were also officially removed in 5.3, but they are still
** available here.)
*/
#define ecierthon_COMPAT_APIINTCASTS


/*
@@ ecierthon_COMPAT_LT_LE controls the emulation of the '__le' metamethod
** using '__lt'.
*/
#define ecierthon_COMPAT_LT_LE


/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
** (Once more, these macros were officially removed in 5.3, but they are
** still available here.)
*/
#define ecierthon_strlen(L,i)		ecierthon_rawlen(L, (i))

#define ecierthon_objlen(L,i)		ecierthon_rawlen(L, (i))

#define ecierthon_equal(L,idx1,idx2)		ecierthon_compare(L,(idx1),(idx2),ecierthon_OPEQ)
#define ecierthon_lessthan(L,idx1,idx2)	ecierthon_compare(L,(idx1),(idx2),ecierthon_OPLT)

#endif				/* } */

/* }================================================================== */



/*
** {==================================================================
** Configuration for Numbers.
** Change these definitions if no predefined ecierthon_FLOAT_* / ecierthon_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ ecierthon_NUMBER is the floating-point type used by ecierthon.
@@ ecierthonI_UACNUMBER is the result of a 'default argument promotion'
@@ over a floating number.
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** by prefixing it with one of FLT/DBL/LDBL.
@@ ecierthon_NUMBER_FRMLEN is the length modifier for writing floats.
@@ ecierthon_NUMBER_FMT is the format for writing floats.
@@ ecierthon_number2str converts a float to a string.
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ ecierthon_str2number converts a decimal numeral to a number.
*/


/* The following definitions are good for most cases here */

#define l_floor(x)		(l_mathop(floor)(x))

#define ecierthon_number2str(s,sz,n)  \
	l_sprintf((s), sz, ecierthon_NUMBER_FMT, (ecierthonI_UACNUMBER)(n))

/*
@@ ecierthon_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if float is not within the range of
** a ecierthon_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
*/
#define ecierthon_numbertointeger(n,p) \
  ((n) >= (ecierthon_NUMBER)(ecierthon_MININTEGER) && \
   (n) < -(ecierthon_NUMBER)(ecierthon_MININTEGER) && \
      (*(p) = (ecierthon_INTEGER)(n), 1))


/* now the variable definitions */

#if ecierthon_FLOAT_TYPE == ecierthon_FLOAT_FLOAT		/* { single float */

#define ecierthon_NUMBER	float

#define l_floatatt(n)		(FLT_##n)

#define ecierthonI_UACNUMBER	double

#define ecierthon_NUMBER_FRMLEN	""
#define ecierthon_NUMBER_FMT		"%.7g"

#define l_mathop(op)		op##f

#define ecierthon_str2number(s,p)	strtof((s), (p))


#elif ecierthon_FLOAT_TYPE == ecierthon_FLOAT_LONGDOUBLE	/* }{ long double */

#define ecierthon_NUMBER	long double

#define l_floatatt(n)		(LDBL_##n)

#define ecierthonI_UACNUMBER	long double

#define ecierthon_NUMBER_FRMLEN	"L"
#define ecierthon_NUMBER_FMT		"%.19Lg"

#define l_mathop(op)		op##l

#define ecierthon_str2number(s,p)	strtold((s), (p))

#elif ecierthon_FLOAT_TYPE == ecierthon_FLOAT_DOUBLE	/* }{ double */

#define ecierthon_NUMBER	double

#define l_floatatt(n)		(DBL_##n)

#define ecierthonI_UACNUMBER	double

#define ecierthon_NUMBER_FRMLEN	""
#define ecierthon_NUMBER_FMT		"%.14g"

#define l_mathop(op)		op

#define ecierthon_str2number(s,p)	strtod((s), (p))

#else						/* }{ */

#error "numeric float type not defined"

#endif					/* } */



/*
@@ ecierthon_INTEGER is the integer type used by ecierthon.
**
@@ ecierthon_UNSIGNED is the unsigned version of ecierthon_INTEGER.
**
@@ ecierthonI_UACINT is the result of a 'default argument promotion'
@@ over a ecierthon_INTEGER.
@@ ecierthon_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ ecierthon_INTEGER_FMT is the format for writing integers.
@@ ecierthon_MAXINTEGER is the maximum value for a ecierthon_INTEGER.
@@ ecierthon_MININTEGER is the minimum value for a ecierthon_INTEGER.
@@ ecierthon_MAXUNSIGNED is the maximum value for a ecierthon_UNSIGNED.
@@ ecierthon_UNSIGNEDBITS is the number of bits in a ecierthon_UNSIGNED.
@@ ecierthon_integer2str converts an integer to a string.
*/


/* The following definitions are good for most cases here */

#define ecierthon_INTEGER_FMT		"%" ecierthon_INTEGER_FRMLEN "d"

#define ecierthonI_UACINT		ecierthon_INTEGER

#define ecierthon_integer2str(s,sz,n)  \
	l_sprintf((s), sz, ecierthon_INTEGER_FMT, (ecierthonI_UACINT)(n))

/*
** use ecierthonI_UACINT here to avoid problems with promotions (which
** can turn a comparison between unsigneds into a signed comparison)
*/
#define ecierthon_UNSIGNED		unsigned ecierthonI_UACINT


#define ecierthon_UNSIGNEDBITS	(sizeof(ecierthon_UNSIGNED) * CHAR_BIT)


/* now the variable definitions */

#if ecierthon_INT_TYPE == ecierthon_INT_INT		/* { int */

#define ecierthon_INTEGER		int
#define ecierthon_INTEGER_FRMLEN	""

#define ecierthon_MAXINTEGER		INT_MAX
#define ecierthon_MININTEGER		INT_MIN

#define ecierthon_MAXUNSIGNED		UINT_MAX

#elif ecierthon_INT_TYPE == ecierthon_INT_LONG	/* }{ long */

#define ecierthon_INTEGER		long
#define ecierthon_INTEGER_FRMLEN	"l"

#define ecierthon_MAXINTEGER		LONG_MAX
#define ecierthon_MININTEGER		LONG_MIN

#define ecierthon_MAXUNSIGNED		ULONG_MAX

#elif ecierthon_INT_TYPE == ecierthon_INT_LONGLONG	/* }{ long long */

/* use presence of macro LLONG_MAX as proxy for C99 compliance */
#if defined(LLONG_MAX)		/* { */
/* use ISO C99 stuff */

#define ecierthon_INTEGER		long long
#define ecierthon_INTEGER_FRMLEN	"ll"

#define ecierthon_MAXINTEGER		LLONG_MAX
#define ecierthon_MININTEGER		LLONG_MIN

#define ecierthon_MAXUNSIGNED		ULLONG_MAX

#elif defined(ecierthon_USE_WINDOWS) /* }{ */
/* in Windows, can use specific Windows types */

#define ecierthon_INTEGER		__int64
#define ecierthon_INTEGER_FRMLEN	"I64"

#define ecierthon_MAXINTEGER		_I64_MAX
#define ecierthon_MININTEGER		_I64_MIN

#define ecierthon_MAXUNSIGNED		_UI64_MAX

#else				/* }{ */

#error "Compiler does not support 'long long'. Use option '-Decierthon_32BITS' \
  or '-Decierthon_C89_NUMBERS' (see file 'ecierthonconf.h' for details)"

#endif				/* } */

#else				/* }{ */

#error "numeric integer type not defined"

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Dependencies with C99 and other C details
** ===================================================================
*/

/*
@@ l_sprintf is equivalent to 'snprintf' or 'sprintf' in C89.
** (All uses in ecierthon have only one format item.)
*/
#if !defined(ecierthon_USE_C89)
#define l_sprintf(s,sz,f,i)	snprintf(s,sz,f,i)
#else
#define l_sprintf(s,sz,f,i)	((void)(sz), sprintf(s,f,i))
#endif


/*
@@ ecierthon_strx2number converts a hexadecimal numeral to a number.
** In C99, 'strtod' does that conversion. Otherwise, you can
** leave 'ecierthon_strx2number' undefined and ecierthon will provide its own
** implementation.
*/
#if !defined(ecierthon_USE_C89)
#define ecierthon_strx2number(s,p)		ecierthon_str2number(s,p)
#endif


/*
@@ ecierthon_pointer2str converts a pointer to a readable string in a
** non-specified way.
*/
#define ecierthon_pointer2str(buff,sz,p)	l_sprintf(buff,sz,"%p",p)


/*
@@ ecierthon_number2strx converts a float to a hexadecimal numeral.
** In C99, 'sprintf' (with format specifiers '%a'/'%A') does that.
** Otherwise, you can leave 'ecierthon_number2strx' undefined and ecierthon will
** provide its own implementation.
*/
#if !defined(ecierthon_USE_C89)
#define ecierthon_number2strx(L,b,sz,f,n)  \
	((void)L, l_sprintf(b,sz,f,(ecierthonI_UACNUMBER)(n)))
#endif


/*
** 'strtof' and 'opf' variants for math functions are not valid in
** C89. Otherwise, the macro 'HUGE_VALF' is a good proxy for testing the
** availability of these variants. ('math.h' is already included in
** all files that use these macros.)
*/
#if defined(ecierthon_USE_C89) || (defined(HUGE_VAL) && !defined(HUGE_VALF))
#undef l_mathop  /* variants not available */
#undef ecierthon_str2number
#define l_mathop(op)		(ecierthon_Number)op  /* no variant */
#define ecierthon_str2number(s,p)	((ecierthon_Number)strtod((s), (p)))
#endif


/*
@@ ecierthon_KCONTEXT is the type of the context ('ctx') for continuation
** functions.  It must be a numerical type; ecierthon will use 'intptr_t' if
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** 'intptr_t' in C89)
*/
#define ecierthon_KCONTEXT	ptrdiff_t

#if !defined(ecierthon_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>
#if defined(INTPTR_MAX)  /* even in C99 this type is optional */
#undef ecierthon_KCONTEXT
#define ecierthon_KCONTEXT	intptr_t
#endif
#endif


/*
@@ ecierthon_getlocaledecpoint gets the locale "radix character" (decimal point).
** Change that if you do not want to use C locales. (Code using this
** macro must include the header 'locale.h'.)
*/
#if !defined(ecierthon_getlocaledecpoint)
#define ecierthon_getlocaledecpoint()		(localeconv()->decimal_point[0])
#endif

/* }================================================================== */


/*
** {==================================================================
** Language Variations
** =====================================================================
*/

/*
@@ ecierthon_NOCVTN2S/ecierthon_NOCVTS2N control how ecierthon performs some
** coercions. Define ecierthon_NOCVTN2S to turn off automatic coercion from
** numbers to strings. Define ecierthon_NOCVTS2N to turn off automatic
** coercion from strings to numbers.
*/
/* #define ecierthon_NOCVTN2S */
/* #define ecierthon_NOCVTS2N */


/*
@@ ecierthon_USE_APICHECK turns on several consistency checks on the C API.
** Define it as a help when debugging C code.
*/
#if defined(ecierthon_USE_APICHECK)
#include <assert.h>
#define ecierthoni_apicheck(l,e)	assert(e)
#endif

/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile ecierthon and when you compile code that links to
** ecierthon).
** =====================================================================
*/

/*
@@ ecierthonI_MAXSTACK limits the size of the ecierthon stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop ecierthon from consuming unlimited stack
** space (and to reserve some numbers for pseudo-indices).
** (It must fit into max(size_t)/32.)
*/
#if ecierthonI_IS32INT
#define ecierthonI_MAXSTACK		1000000
#else
#define ecierthonI_MAXSTACK		15000
#endif


/*
@@ ecierthon_EXTRASPACE defines the size of a raw memory area associated with
** a ecierthon state with very fast access.
** CHANGE it if you need a different size.
*/
#define ecierthon_EXTRASPACE		(sizeof(void *))


/*
@@ ecierthon_IDSIZE gives the maximum size for the description of the source
@@ of a function in debug information.
** CHANGE it if you want a different size.
*/
#define ecierthon_IDSIZE	60


/*
@@ ecierthonL_BUFFERSIZE is the buffer size used by the lauxlib buffer system.
*/
#define ecierthonL_BUFFERSIZE   ((int)(16 * sizeof(void*) * sizeof(ecierthon_Number)))


/*
@@ ecierthonI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/
#define ecierthonI_MAXALIGN  ecierthon_Number n; double u; void *s; ecierthon_Integer i; long l

/* }================================================================== */





/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** without modifying the main part of the file.
*/





#endif

