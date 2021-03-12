/*
** $Id: linit.c $
** Initialization of libraries for ecierthon.c and other clients
** See Copyright Notice in ecierthon.h
*/


#define linit_c
#define ecierthon_LIB

/*
** If you embed ecierthon in your program and need to open the standard
** libraries, call ecierthonL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  ecierthonL_getsubtable(L, ecierthon_REGISTRYINDEX, ecierthon_PRELOAD_TABLE);
**  ecierthon_pushcfunction(L, ecierthonopen_modname);
**  ecierthon_setfield(L, -2, modname);
**  ecierthon_pop(L, 1);  // remove PRELOAD table
*/

#include "lprefix.h"


#include <stddef.h>

#include "ecierthon.h"

#include "ecierthonlib.h"
#include "lauxlib.h"


/*
** these libs are loaded by ecierthon.c and are readily available to any ecierthon
** program
*/
static const ecierthonL_Reg loadedlibs[] = {
  {ecierthon_GNAME, ecierthonopen_base},
  {ecierthon_LOADLIBNAME, ecierthonopen_package},
  {ecierthon_COLIBNAME, ecierthonopen_coroutine},
  {ecierthon_TABLIBNAME, ecierthonopen_table},
  {ecierthon_IOLIBNAME, ecierthonopen_io},
  {ecierthon_OSLIBNAME, ecierthonopen_os},
  {ecierthon_STRLIBNAME, ecierthonopen_string},
  {ecierthon_MATHLIBNAME, ecierthonopen_math},
  {ecierthon_UTF8LIBNAME, ecierthonopen_utf8},
  {ecierthon_DBLIBNAME, ecierthonopen_debug},
  {NULL, NULL}
};


ecierthonLIB_API void ecierthonL_openlibs (ecierthon_State *L) {
  const ecierthonL_Reg *lib;
  /* "require" functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    ecierthonL_requiref(L, lib->name, lib->func, 1);
    ecierthon_pop(L, 1);  /* remove lib */
  }
}

