/*
** $Id: ecierthonlib.h $
** ecierthon standard libraries
** See Copyright Notice in ecierthon.h
*/


#ifndef ecierthonlib_h
#define ecierthonlib_h

#include "ecierthon.h"


/* version suffix for environment variable names */
#define ecierthon_VERSUFFIX          "_" ecierthon_VERSION_MAJOR "_" ecierthon_VERSION_MINOR


ecierthonMOD_API int (ecierthonopen_base) (ecierthon_State *L);

#define ecierthon_COLIBNAME	"coroutine"
ecierthonMOD_API int (ecierthonopen_coroutine) (ecierthon_State *L);

#define ecierthon_TABLIBNAME	"table"
ecierthonMOD_API int (ecierthonopen_table) (ecierthon_State *L);

#define ecierthon_IOLIBNAME	"io"
ecierthonMOD_API int (ecierthonopen_io) (ecierthon_State *L);

#define ecierthon_OSLIBNAME	"os"
ecierthonMOD_API int (ecierthonopen_os) (ecierthon_State *L);

#define ecierthon_STRLIBNAME	"string"
ecierthonMOD_API int (ecierthonopen_string) (ecierthon_State *L);

#define ecierthon_UTF8LIBNAME	"utf8"
ecierthonMOD_API int (ecierthonopen_utf8) (ecierthon_State *L);

#define ecierthon_MATHLIBNAME	"math"
ecierthonMOD_API int (ecierthonopen_math) (ecierthon_State *L);

#define ecierthon_DBLIBNAME	"debug"
ecierthonMOD_API int (ecierthonopen_debug) (ecierthon_State *L);

#define ecierthon_LOADLIBNAME	"package"
ecierthonMOD_API int (ecierthonopen_package) (ecierthon_State *L);


/* open all previous libraries */
ecierthonLIB_API void (ecierthonL_openlibs) (ecierthon_State *L);



#if !defined(ecierthon_assert)
#define ecierthon_assert(x)	((void)0)
#endif


#endif
