/*
** $Id: loadlib.c $
** Dynamic library loader for ecierthon
** See Copyright Notice in ecierthon.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define ecierthon_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ecierthon.h"

#include "lauxlib.h"
#include "ecierthonlib.h"


/*
** ecierthon_IGMARK is a mark to ignore all before it when building the
** ecierthonopen_ function name.
*/
#if !defined (ecierthon_IGMARK)
#define ecierthon_IGMARK		"-"
#endif


/*
** ecierthon_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** ecierthon_LSUBSEP is the character that replaces dots in submodule names
** when searching for a ecierthon loader.
*/
#if !defined(ecierthon_CSUBSEP)
#define ecierthon_CSUBSEP		ecierthon_DIRSEP
#endif

#if !defined(ecierthon_LSUBSEP)
#define ecierthon_LSUBSEP		ecierthon_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define ecierthon_POF		"ecierthonopen_"

/* separator for open functions in C libraries */
#define ecierthon_OFSEP	"_"


/*
** key for table in the registry that keeps handles
** for all loaded C libraries
*/
static const char *const CLIBS = "_CLIBS";

#define LIB_FAIL	"open"


#define setprogdir(L)           ((void)0)


/*
** Special type equivalent to '(void*)' for functions in gcc
** (to suppress warnings when converting function pointers)
*/
typedef void (*voidf)(void);


/*
** system-dependent functions
*/

/*
** unload library 'lib'
*/
static void lsys_unloadlib (void *lib);

/*
** load C library in file 'path'. If 'seeglb', load with all names in
** the library global.
** Returns the library; in case of error, returns NULL plus an
** error string in the stack.
*/
static void *lsys_load (ecierthon_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static ecierthon_CFunction lsys_sym (ecierthon_State *L, void *lib, const char *sym);




#if defined(ecierthon_USE_DLOPEN)	/* { */
/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface.
** The dlfcn interface is available in Linux, SunOS, Solaris, IRIX, FreeBSD,
** NetBSD, AIX 4.2, HPUX 11, and  probably most other Unix flavors, at least
** as an emulation layer on top of native functions.
** =========================================================================
*/

#include <dlfcn.h>

/*
** Macro to convert pointer-to-void* to pointer-to-function. This cast
** is undefined according to ISO C, but POSIX assumes that it works.
** (The '__extension__' in gnu compilers is only to avoid warnings.)
*/
#if defined(__GNUC__)
#define cast_func(p) (__extension__ (ecierthon_CFunction)(p))
#else
#define cast_func(p) ((ecierthon_CFunction)(p))
#endif


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (ecierthon_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (lib == NULL) ecierthon_pushstring(L, dlerror());
  return lib;
}


static ecierthon_CFunction lsys_sym (ecierthon_State *L, void *lib, const char *sym) {
  ecierthon_CFunction f = cast_func(dlsym(lib, sym));
  if (f == NULL) ecierthon_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(ecierthon_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(ecierthon_LLE_FLAGS)
#define ecierthon_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of ecierthon_EXEC_DIR with the executable's path.
*/
static void setprogdir (ecierthon_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    ecierthonL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    ecierthonL_gsub(L, ecierthon_tostring(L, -1), ecierthon_EXEC_DIR, buff);
    ecierthon_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (ecierthon_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    ecierthon_pushstring(L, buffer);
  else
    ecierthon_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (ecierthon_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, ecierthon_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static ecierthon_CFunction lsys_sym (ecierthon_State *L, void *lib, const char *sym) {
  ecierthon_CFunction f = (ecierthon_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
  if (f == NULL) pusherror(L);
  return f;
}

/* }====================================================== */


#else				/* }{ */
/*
** {======================================================
** Fallback for other systems
** =======================================================
*/

#undef LIB_FAIL
#define LIB_FAIL	"absent"


#define DLMSG	"dynamic libraries not enabled; check your ecierthon installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (ecierthon_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  ecierthon_pushliteral(L, DLMSG);
  return NULL;
}


static ecierthon_CFunction lsys_sym (ecierthon_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  ecierthon_pushliteral(L, DLMSG);
  return NULL;
}

/* }====================================================== */
#endif				/* } */


/*
** {==================================================================
** Set Paths
** ===================================================================
*/

/*
** ecierthon_PATH_VAR and ecierthon_CPATH_VAR are the names of the environment
** variables that ecierthon check to set its paths.
*/
#if !defined(ecierthon_PATH_VAR)
#define ecierthon_PATH_VAR    "ecierthon_PATH"
#endif

#if !defined(ecierthon_CPATH_VAR)
#define ecierthon_CPATH_VAR   "ecierthon_CPATH"
#endif



/*
** return registry.ecierthon_NOENV as a boolean
*/
static int noenv (ecierthon_State *L) {
  int b;
  ecierthon_getfield(L, ecierthon_REGISTRYINDEX, "ecierthon_NOENV");
  b = ecierthon_toboolean(L, -1);
  ecierthon_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path
*/
static void setpath (ecierthon_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = ecierthon_pushfstring(L, "%s%s", envname, ecierthon_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    ecierthon_pushstring(L, dft);  /* use default */
  else if ((dftmark = strstr(path, ecierthon_PATH_SEP ecierthon_PATH_SEP)) == NULL)
    ecierthon_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    ecierthonL_Buffer b;
    ecierthonL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      ecierthonL_addlstring(&b, path, dftmark - path);  /* add it */
      ecierthonL_addchar(&b, *ecierthon_PATH_SEP);
    }
    ecierthonL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      ecierthonL_addchar(&b, *ecierthon_PATH_SEP);
      ecierthonL_addlstring(&b, dftmark + 2, (path + len - 2) - dftmark);
    }
    ecierthonL_pushresult(&b);
  }
  setprogdir(L);
  ecierthon_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  ecierthon_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (ecierthon_State *L, const char *path) {
  void *plib;
  ecierthon_getfield(L, ecierthon_REGISTRYINDEX, CLIBS);
  ecierthon_getfield(L, -1, path);
  plib = ecierthon_touserdata(L, -1);  /* plib = CLIBS[path] */
  ecierthon_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (ecierthon_State *L, const char *path, void *plib) {
  ecierthon_getfield(L, ecierthon_REGISTRYINDEX, CLIBS);
  ecierthon_pushlightuserdata(L, plib);
  ecierthon_pushvalue(L, -1);
  ecierthon_setfield(L, -3, path);  /* CLIBS[path] = plib */
  ecierthon_rawseti(L, -2, ecierthonL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  ecierthon_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (ecierthon_State *L) {
  ecierthon_Integer n = ecierthonL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    ecierthon_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(ecierthon_touserdata(L, -1));
    ecierthon_pop(L, 1);  /* pop handle */
  }
  return 0;
}



/* error codes for 'lookforfunc' */
#define ERRLIB		1
#define ERRFUNC		2

/*
** Look for a C function named 'sym' in a dynamically loaded library
** 'path'.
** First, check whether the library is already loaded; if not, try
** to load it.
** Then, if 'sym' is '*', return true (as library has been loaded).
** Otherwise, look for symbol 'sym' in the library and push a
** C function with that symbol.
** Return 0 and 'true' or a function in the stack; in case of
** errors, return an error code and an error message in the stack.
*/
static int lookforfunc (ecierthon_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    ecierthon_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    ecierthon_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    ecierthon_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (ecierthon_State *L) {
  const char *path = ecierthonL_checkstring(L, 1);
  const char *init = ecierthonL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (stat == 0)  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    ecierthonL_pushfail(L);
    ecierthon_insert(L, -2);
    ecierthon_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
    return 3;  /* return fail, error message, and where */
  }
}



/*
** {======================================================
** 'require' function
** =======================================================
*/


static int readable (const char *filename) {
  FILE *f = fopen(filename, "r");  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}


/*
** Get the next name in '*path' = 'name1;name2;name3;...', changing
** the ending ';' to '\0' to create a zero-terminated string. Return
** NULL when list ends.
*/
static const char *getnextfilename (char **path, char *end) {
  char *sep;
  char *name = *path;
  if (name == end)
    return NULL;  /* no more names */
  else if (*name == '\0') {  /* from previous iteration? */
    *name = *ecierthon_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *ecierthon_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name goes until the end */
  *sep = '\0';  /* finish file name */
  *path = sep;  /* will start next search from here */
  return name;
}


/*
** Given a path such as ";blabla.so;blublu.so", pushes the string
**
** no file 'blabla.so'
**	no file 'blublu.so'
*/
static void pusherrornotfound (ecierthon_State *L, const char *path) {
  ecierthonL_Buffer b;
  ecierthonL_buffinit(L, &b);
  ecierthonL_addstring(&b, "no file '");
  ecierthonL_addgsub(&b, path, ecierthon_PATH_SEP, "'\n\tno file '");
  ecierthonL_addstring(&b, "'");
  ecierthonL_pushresult(&b);
}


static const char *searchpath (ecierthon_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  ecierthonL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = ecierthonL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  ecierthonL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  ecierthonL_addgsub(&buff, path, ecierthon_PATH_MARK, name);
  ecierthonL_addchar(&buff, '\0');
  pathname = ecierthonL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + ecierthonL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return ecierthon_pushstring(L, filename);  /* save and return name */
  }
  ecierthonL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, ecierthon_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (ecierthon_State *L) {
  const char *f = searchpath(L, ecierthonL_checkstring(L, 1),
                                ecierthonL_checkstring(L, 2),
                                ecierthonL_optstring(L, 3, "."),
                                ecierthonL_optstring(L, 4, ecierthon_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    ecierthonL_pushfail(L);
    ecierthon_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (ecierthon_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  ecierthon_getfield(L, ecierthon_upvalueindex(1), pname);
  path = ecierthon_tostring(L, -1);
  if (path == NULL)
    ecierthonL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (ecierthon_State *L, int stat, const char *filename) {
  if (stat) {  /* module loaded successfully? */
    ecierthon_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return ecierthonL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          ecierthon_tostring(L, 1), filename, ecierthon_tostring(L, -1));
}


static int searcher_ecierthon (ecierthon_State *L) {
  const char *filename;
  const char *name = ecierthonL_checkstring(L, 1);
  filename = findfile(L, name, "path", ecierthon_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (ecierthonL_loadfile(L, filename) == ecierthon_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "ecierthonopen_X" and look for it. (For compatibility, if that
** fails, it also tries "ecierthonopen_Y".) If there is no ignore mark,
** look for a function named "ecierthonopen_modname".
*/
static int loadfunc (ecierthon_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = ecierthonL_gsub(L, modname, ".", ecierthon_OFSEP);
  mark = strchr(modname, *ecierthon_IGMARK);
  if (mark) {
    int stat;
    openfunc = ecierthon_pushlstring(L, modname, mark - modname);
    openfunc = ecierthon_pushfstring(L, ecierthon_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else go ahead and try old-style name */
  }
  openfunc = ecierthon_pushfstring(L, ecierthon_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (ecierthon_State *L) {
  const char *name = ecierthonL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", ecierthon_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (ecierthon_State *L) {
  const char *filename;
  const char *name = ecierthonL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  ecierthon_pushlstring(L, name, p - name);
  filename = findfile(L, ecierthon_tostring(L, -1), "cpath", ecierthon_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      ecierthon_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  ecierthon_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (ecierthon_State *L) {
  const char *name = ecierthonL_checkstring(L, 1);
  ecierthon_getfield(L, ecierthon_REGISTRYINDEX, ecierthon_PRELOAD_TABLE);
  if (ecierthon_getfield(L, -1, name) == ecierthon_TNIL) {  /* not found? */
    ecierthon_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    ecierthon_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (ecierthon_State *L, const char *name) {
  int i;
  ecierthonL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (ecierthon_getfield(L, ecierthon_upvalueindex(1), "searchers") != ecierthon_TTABLE)
    ecierthonL_error(L, "'package.searchers' must be a table");
  ecierthonL_buffinit(L, &msg);
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    ecierthonL_addstring(&msg, "\n\t");  /* error-message prefix */
    if (ecierthon_rawgeti(L, 3, i) == ecierthon_TNIL) {  /* no more searchers? */
      ecierthon_pop(L, 1);  /* remove nil */
      ecierthonL_buffsub(&msg, 2);  /* remove prefix */
      ecierthonL_pushresult(&msg);  /* create error message */
      ecierthonL_error(L, "module '%s' not found:%s", name, ecierthon_tostring(L, -1));
    }
    ecierthon_pushstring(L, name);
    ecierthon_call(L, 1, 2);  /* call it */
    if (ecierthon_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (ecierthon_isstring(L, -2)) {  /* searcher returned error message? */
      ecierthon_pop(L, 1);  /* remove extra return */
      ecierthonL_addvalue(&msg);  /* concatenate error message */
    }
    else {  /* no error message */
      ecierthon_pop(L, 2);  /* remove both returns */
      ecierthonL_buffsub(&msg, 2);  /* remove prefix */
    }
  }
}


static int ll_require (ecierthon_State *L) {
  const char *name = ecierthonL_checkstring(L, 1);
  ecierthon_settop(L, 1);  /* LOADED table will be at index 2 */
  ecierthon_getfield(L, ecierthon_REGISTRYINDEX, ecierthon_LOADED_TABLE);
  ecierthon_getfield(L, 2, name);  /* LOADED[name] */
  if (ecierthon_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  ecierthon_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  ecierthon_rotate(L, -2, 1);  /* function <-> loader data */
  ecierthon_pushvalue(L, 1);  /* name is 1st argument to module loader */
  ecierthon_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  ecierthon_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!ecierthon_isnil(L, -1))  /* non-nil return? */
    ecierthon_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    ecierthon_pop(L, 1);  /* pop nil */
  if (ecierthon_getfield(L, 2, name) == ecierthon_TNIL) {   /* module set no value? */
    ecierthon_pushboolean(L, 1);  /* use true as result */
    ecierthon_copy(L, -1, -2);  /* replace loader result */
    ecierthon_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  ecierthon_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const ecierthonL_Reg pk_funcs[] = {
  {"loadlib", ll_loadlib},
  {"searchpath", ll_searchpath},
  /* placeholders */
  {"preload", NULL},
  {"cpath", NULL},
  {"path", NULL},
  {"searchers", NULL},
  {"loaded", NULL},
  {NULL, NULL}
};


static const ecierthonL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (ecierthon_State *L) {
  static const ecierthon_CFunction searchers[] =
    {searcher_preload, searcher_ecierthon, searcher_C, searcher_Croot, NULL};
  int i;
  /* create 'searchers' table */
  ecierthon_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    ecierthon_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    ecierthon_pushcclosure(L, searchers[i], 1);
    ecierthon_rawseti(L, -2, i+1);
  }
  ecierthon_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (ecierthon_State *L) {
  ecierthonL_getsubtable(L, ecierthon_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  ecierthon_createtable(L, 0, 1);  /* create metatable for CLIBS */
  ecierthon_pushcfunction(L, gctm);
  ecierthon_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  ecierthon_setmetatable(L, -2);
}


ecierthonMOD_API int ecierthonopen_package (ecierthon_State *L) {
  createclibstable(L);
  ecierthonL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", ecierthon_PATH_VAR, ecierthon_PATH_DEFAULT);
  setpath(L, "cpath", ecierthon_CPATH_VAR, ecierthon_CPATH_DEFAULT);
  /* store config information */
  ecierthon_pushliteral(L, ecierthon_DIRSEP "\n" ecierthon_PATH_SEP "\n" ecierthon_PATH_MARK "\n"
                     ecierthon_EXEC_DIR "\n" ecierthon_IGMARK "\n");
  ecierthon_setfield(L, -2, "config");
  /* set field 'loaded' */
  ecierthonL_getsubtable(L, ecierthon_REGISTRYINDEX, ecierthon_LOADED_TABLE);
  ecierthon_setfield(L, -2, "loaded");
  /* set field 'preload' */
  ecierthonL_getsubtable(L, ecierthon_REGISTRYINDEX, ecierthon_PRELOAD_TABLE);
  ecierthon_setfield(L, -2, "preload");
  ecierthon_pushglobaltable(L);
  ecierthon_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  ecierthonL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  ecierthon_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

