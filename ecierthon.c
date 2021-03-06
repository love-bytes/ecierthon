/*
** $Id: ecierthon.c $
** ecierthon stand-alone interpreter
** See Copyright Notice in ecierthon.h
*/

#define ecierthon_c

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include "ecierthon.h"

#include "lauxlib.h"
#include "ecierthonlib.h"


#if !defined(ecierthon_PROGNAME)
#define ecierthon_PROGNAME		"ecierthon"
#endif

#if !defined(ecierthon_INIT_VAR)
#define ecierthon_INIT_VAR		"ecierthon_INIT"
#endif

#define ecierthon_INITVARVERSION	ecierthon_INIT_VAR ecierthon_VERSUFFIX


static ecierthon_State *globalL = NULL;

static const char *progname = ecierthon_PROGNAME;


/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop (ecierthon_State *L, ecierthon_Debug *ar) {
  (void)ar;  /* unused arg. */
  ecierthon_sethook(L, NULL, 0, 0);  /* reset hook */
  ecierthonL_error(L, "interrupted!");
}


/*
** Function to be called at a C signal. Because a C signal cannot
** just change a ecierthon state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction (int i) {
  int flag = ecierthon_MASKCALL | ecierthon_MASKRET | ecierthon_MASKLINE | ecierthon_MASKCOUNT;
  signal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
  ecierthon_sethook(globalL, lstop, flag, 1);
}


static void print_usage (const char *badoption) {
  ecierthon_writestringerror("%s: ", progname);
  if (badoption[1] == 'e' || badoption[1] == 'l')
    ecierthon_writestringerror("'%s' needs argument\n", badoption);
  else
    ecierthon_writestringerror("unrecognized option '%s'\n", badoption);
  ecierthon_writestringerror(
  "usage: %s [options] [script [args]]\n"
  "Available options are:\n"
  "  -e stat  execute string 'stat'\n"
  "  -i       enter interactive mode after executing 'script'\n"
  "  -l name  require library 'name' into global 'name'\n"
  "  -v       show version information\n"
  "  -E       ignore environment variables\n"
  "  -W       turn warnings on\n"
  "  --       stop handling options\n"
  "  -        stop handling options and execute stdin\n"
  ,
  progname);
}


/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message (const char *pname, const char *msg) {
  if (pname) ecierthon_writestringerror("%s: ", pname);
  ecierthon_writestringerror("%s\n", msg);
}


/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by ecierthon or by 'msghandler'.
*/
static int report (ecierthon_State *L, int status) {
  if (status != ecierthon_OK) {
    const char *msg = ecierthon_tostring(L, -1);
    l_message(progname, msg);
    ecierthon_pop(L, 1);  /* remove message */
  }
  return status;
}


/*
** Message handler used to run all chunks
*/
static int msghandler (ecierthon_State *L) {
  const char *msg = ecierthon_tostring(L, 1);
  if (msg == NULL) {  /* is error object not a string? */
    if (ecierthonL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
        ecierthon_type(L, -1) == ecierthon_TSTRING)  /* that produces a string? */
      return 1;  /* that is the message */
    else
      msg = ecierthon_pushfstring(L, "(error object is a %s value)",
                               ecierthonL_typename(L, 1));
  }
  ecierthonL_traceback(L, L, msg, 1);  /* append a standard traceback */
  return 1;  /* return the traceback */
}


/*
** Interface to 'ecierthon_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall (ecierthon_State *L, int narg, int nres) {
  int status;
  int base = ecierthon_gettop(L) - narg;  /* function index */
  ecierthon_pushcfunction(L, msghandler);  /* push message handler */
  ecierthon_insert(L, base);  /* put it under function and args */
  globalL = L;  /* to be available to 'laction' */
  signal(SIGINT, laction);  /* set C-signal handler */
  status = ecierthon_pcall(L, narg, nres, base);
  signal(SIGINT, SIG_DFL); /* reset C-signal handler */
  ecierthon_remove(L, base);  /* remove message handler from the stack */
  return status;
}


static void print_version (void) {
  ecierthon_writestring(ecierthon_COPYRIGHT, strlen(ecierthon_COPYRIGHT));
  ecierthon_writeline();
}


/*
** Create the 'arg' table, which stores all arguments from the
** command line ('argv'). It should be aligned so that, at index 0,
** it has 'argv[script]', which is the script name. The arguments
** to the script (everything after 'script') go to positive indices;
** other arguments (before the script name) go to negative indices.
** If there is no script name, assume interpreter's name as base.
*/
static void createargtable (ecierthon_State *L, char **argv, int argc, int script) {
  int i, narg;
  if (script == argc) script = 0;  /* no script name? */
  narg = argc - (script + 1);  /* number of positive indices */
  ecierthon_createtable(L, narg, script + 1);
  for (i = 0; i < argc; i++) {
    ecierthon_pushstring(L, argv[i]);
    ecierthon_rawseti(L, -2, i - script);
  }
  ecierthon_setglobal(L, "arg");
}


static int dochunk (ecierthon_State *L, int status) {
  if (status == ecierthon_OK) status = docall(L, 0, 0);
  return report(L, status);
}


static int dofile (ecierthon_State *L, const char *name) {
  return dochunk(L, ecierthonL_loadfile(L, name));
}


static int dostring (ecierthon_State *L, const char *s, const char *name) {
  return dochunk(L, ecierthonL_loadbuffer(L, s, strlen(s), name));
}


/*
** Calls 'require(name)' and stores the result in a global variable
** with the given name.
*/
static int dolibrary (ecierthon_State *L, const char *name) {
  int status;
  ecierthon_getglobal(L, "require");
  ecierthon_pushstring(L, name);
  status = docall(L, 1, 1);  /* call 'require(name)' */
  if (status == ecierthon_OK)
    ecierthon_setglobal(L, name);  /* global[name] = require return */
  return report(L, status);
}


/*
** Push on the stack the contents of table 'arg' from 1 to #arg
*/
static int pushargs (ecierthon_State *L) {
  int i, n;
  if (ecierthon_getglobal(L, "arg") != ecierthon_TTABLE)
    ecierthonL_error(L, "'arg' is not a table");
  n = (int)ecierthonL_len(L, -1);
  ecierthonL_checkstack(L, n + 3, "too many arguments to script");
  for (i = 1; i <= n; i++)
    ecierthon_rawgeti(L, -i, i);
  ecierthon_remove(L, -i);  /* remove table from the stack */
  return n;
}


static int handle_script (ecierthon_State *L, char **argv) {
  int status;
  const char *fname = argv[0];
  if (strcmp(fname, "-") == 0 && strcmp(argv[-1], "--") != 0)
    fname = NULL;  /* stdin */
  status = ecierthonL_loadfile(L, fname);
  if (status == ecierthon_OK) {
    int n = pushargs(L);  /* push arguments to script */
    status = docall(L, n, ecierthon_MULTRET);
  }
  return report(L, status);
}


/* bits of various argument indicators in 'args' */
#define has_error	1	/* bad option */
#define has_i		2	/* -i */
#define has_v		4	/* -v */
#define has_e		8	/* -e */
#define has_E		16	/* -E */


/*
** Traverses all arguments from 'argv', returning a mask with those
** needed before running any ecierthon code (or an error code if it finds
** any invalid argument). 'first' returns the first not-handled argument
** (either the script name or a bad argument in case of error).
*/
static int collectargs (char **argv, int *first) {
  int args = 0;
  int i;
  for (i = 1; argv[i] != NULL; i++) {
    *first = i;
    if (argv[i][0] != '-')  /* not an option? */
        return args;  /* stop handling options */
    switch (argv[i][1]) {  /* else check option */
      case '-':  /* '--' */
        if (argv[i][2] != '\0')  /* extra characters after '--'? */
          return has_error;  /* invalid option */
        *first = i + 1;
        return args;
      case '\0':  /* '-' */
        return args;  /* script "name" is '-' */
      case 'E':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        args |= has_E;
        break;
      case 'W':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        break;
      case 'i':
        args |= has_i;  /* (-i implies -v) *//* FALLTHROUGH */
      case 'v':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        args |= has_v;
        break;
      case 'e':
        args |= has_e;  /* FALLTHROUGH */
      case 'l':  /* both options need an argument */
        if (argv[i][2] == '\0') {  /* no concatenated argument? */
          i++;  /* try next 'argv' */
          if (argv[i] == NULL || argv[i][0] == '-')
            return has_error;  /* no next argument or it is another option */
        }
        break;
      default:  /* invalid option */
        return has_error;
    }
  }
  *first = i;  /* no script name */
  return args;
}


/*
** Processes options 'e' and 'l', which involve running ecierthon code, and
** 'W', which also affects the state.
** Returns 0 if some code raises an error.
*/
static int runargs (ecierthon_State *L, char **argv, int n) {
  int i;
  for (i = 1; i < n; i++) {
    int option = argv[i][1];
    ecierthon_assert(argv[i][0] == '-');  /* already checked */
    switch (option) {
      case 'e':  case 'l': {
        int status;
        const char *extra = argv[i] + 2;  /* both options need an argument */
        if (*extra == '\0') extra = argv[++i];
        ecierthon_assert(extra != NULL);
        status = (option == 'e')
                 ? dostring(L, extra, "=(command line)")
                 : dolibrary(L, extra);
        if (status != ecierthon_OK) return 0;
        break;
      }
      case 'W':
        ecierthon_warning(L, "@on", 0);  /* warnings on */
        break;
    }
  }
  return 1;
}


static int handle_ecierthoninit (ecierthon_State *L) {
  const char *name = "=" ecierthon_INITVARVERSION;
  const char *init = getenv(name + 1);
  if (init == NULL) {
    name = "=" ecierthon_INIT_VAR;
    init = getenv(name + 1);  /* try alternative name */
  }
  if (init == NULL) return ecierthon_OK;
  else if (init[0] == '@')
    return dofile(L, init+1);
  else
    return dostring(L, init, name);
}


/*
** {==================================================================
** Read-Eval-Print Loop (REPL)
** ===================================================================
*/

#if !defined(ecierthon_PROMPT)
#define ecierthon_PROMPT		"> "
#define ecierthon_PROMPT2		">> "
#endif

#if !defined(ecierthon_MAXINPUT)
#define ecierthon_MAXINPUT		512
#endif


/*
** ecierthon_stdin_is_tty detects whether the standard input is a 'tty' (that
** is, whether we're running ecierthon interactively).
*/
#if !defined(ecierthon_stdin_is_tty)	/* { */

#if defined(ecierthon_USE_POSIX)	/* { */

#include <unistd.h>
#define ecierthon_stdin_is_tty()	isatty(0)

#elif defined(ecierthon_USE_WINDOWS)	/* }{ */

#include <io.h>
#include <windows.h>

#define ecierthon_stdin_is_tty()	_isatty(_fileno(stdin))

#else				/* }{ */

/* ISO C definition */
#define ecierthon_stdin_is_tty()	1  /* assume stdin is a tty */

#endif				/* } */

#endif				/* } */


/*
** ecierthon_readline defines how to show a prompt and then read a line from
** the standard input.
** ecierthon_saveline defines how to "save" a read line in a "history".
** ecierthon_freeline defines how to free a line read by ecierthon_readline.
*/
#if !defined(ecierthon_readline)	/* { */

#if defined(ecierthon_USE_READLINE)	/* { */

#include <readline/readline.h>
#include <readline/history.h>
#define ecierthon_initreadline(L)	((void)L, rl_readline_name="ecierthon")
#define ecierthon_readline(L,b,p)	((void)L, ((b)=readline(p)) != NULL)
#define ecierthon_saveline(L,line)	((void)L, add_history(line))
#define ecierthon_freeline(L,b)	((void)L, free(b))

#else				/* }{ */

#define ecierthon_initreadline(L)  ((void)L)
#define ecierthon_readline(L,b,p) \
        ((void)L, fputs(p, stdout), fflush(stdout),  /* show prompt */ \
        fgets(b, ecierthon_MAXINPUT, stdin) != NULL)  /* get line */
#define ecierthon_saveline(L,line)	{ (void)L; (void)line; }
#define ecierthon_freeline(L,b)	{ (void)L; (void)b; }

#endif				/* } */

#endif				/* } */


/*
** Return the string to be used as a prompt by the interpreter. Leave
** the string (or nil, if using the default value) on the stack, to keep
** it anchored.
*/
static const char *get_prompt (ecierthon_State *L, int firstline) {
  if (ecierthon_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2") == ecierthon_TNIL)
    return (firstline ? ecierthon_PROMPT : ecierthon_PROMPT2);  /* use the default */
  else {  /* apply 'tostring' over the value */
    const char *p = ecierthonL_tolstring(L, -1, NULL);
    ecierthon_remove(L, -2);  /* remove original value */
    return p;
  }
}

/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)


/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int incomplete (ecierthon_State *L, int status) {
  if (status == ecierthon_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = ecierthon_tolstring(L, -1, &lmsg);
    if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
      ecierthon_pop(L, 1);
      return 1;
    }
  }
  return 0;  /* else... */
}


/*
** Prompt the user, read a line, and push it into the ecierthon stack.
*/
static int pushline (ecierthon_State *L, int firstline) {
  char buffer[ecierthon_MAXINPUT];
  char *b = buffer;
  size_t l;
  const char *prmt = get_prompt(L, firstline);
  int readstatus = ecierthon_readline(L, b, prmt);
  if (readstatus == 0)
    return 0;  /* no input (prompt will be popped by caller) */
  ecierthon_pop(L, 1);  /* remove prompt */
  l = strlen(b);
  if (l > 0 && b[l-1] == '\n')  /* line ends with newline? */
    b[--l] = '\0';  /* remove it */
  if (firstline && b[0] == '=')  /* for compatibility with 5.2, ... */
    ecierthon_pushfstring(L, "return %s", b + 1);  /* change '=' to 'return' */
  else
    ecierthon_pushlstring(L, b, l);
  ecierthon_freeline(L, b);
  return 1;
}


/*
** Try to compile line on the stack as 'return <line>;'; on return, stack
** has either compiled chunk or original line (if compilation failed).
*/
static int addreturn (ecierthon_State *L) {
  const char *line = ecierthon_tostring(L, -1);  /* original line */
  const char *retline = ecierthon_pushfstring(L, "return %s;", line);
  int status = ecierthonL_loadbuffer(L, retline, strlen(retline), "=stdin");
  if (status == ecierthon_OK) {
    ecierthon_remove(L, -2);  /* remove modified line */
    if (line[0] != '\0')  /* non empty? */
      ecierthon_saveline(L, line);  /* keep history */
  }
  else
    ecierthon_pop(L, 2);  /* pop result from 'ecierthonL_loadbuffer' and modified line */
  return status;
}


/*
** Read multiple lines until a complete ecierthon statement
*/
static int multiline (ecierthon_State *L) {
  for (;;) {  /* repeat until gets a complete statement */
    size_t len;
    const char *line = ecierthon_tolstring(L, 1, &len);  /* get what it has */
    int status = ecierthonL_loadbuffer(L, line, len, "=stdin");  /* try it */
    if (!incomplete(L, status) || !pushline(L, 0)) {
      ecierthon_saveline(L, line);  /* keep history */
      return status;  /* cannot or should not try to add continuation line */
    }
    ecierthon_pushliteral(L, "\n");  /* add newline... */
    ecierthon_insert(L, -2);  /* ...between the two lines */
    ecierthon_concat(L, 3);  /* join them */
  }
}


/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement. Return
** the final status of load/call with the resulting function (if any)
** in the top of the stack.
*/
static int loadline (ecierthon_State *L) {
  int status;
  ecierthon_settop(L, 0);
  if (!pushline(L, 1))
    return -1;  /* no input */
  if ((status = addreturn(L)) != ecierthon_OK)  /* 'return ...' did not work? */
    status = multiline(L);  /* try as command, maybe with continuation lines */
  ecierthon_remove(L, 1);  /* remove line from the stack */
  ecierthon_assert(ecierthon_gettop(L) == 1);
  return status;
}


/*
** Prints (calling the ecierthon 'print' function) any values on the stack
*/
static void l_print (ecierthon_State *L) {
  int n = ecierthon_gettop(L);
  if (n > 0) {  /* any result to be printed? */
    ecierthonL_checkstack(L, ecierthon_MINSTACK, "too many results to print");
    ecierthon_getglobal(L, "print");
    ecierthon_insert(L, 1);
    if (ecierthon_pcall(L, n, 0, 0) != ecierthon_OK)
      l_message(progname, ecierthon_pushfstring(L, "error calling 'print' (%s)",
                                             ecierthon_tostring(L, -1)));
  }
}


/*
** Do the REPL: repeatedly read (load) a line, evaecierthonte (call) it, and
** print any results.
*/
static void doREPL (ecierthon_State *L) {
  int status;
  const char *oldprogname = progname;
  progname = NULL;  /* no 'progname' on errors in interactive mode */
  ecierthon_initreadline(L);
  while ((status = loadline(L)) != -1) {
    if (status == ecierthon_OK)
      status = docall(L, 0, ecierthon_MULTRET);
    if (status == ecierthon_OK) l_print(L);
    else report(L, status);
  }
  ecierthon_settop(L, 0);  /* clear stack */
  ecierthon_writeline();
  progname = oldprogname;
}

/* }================================================================== */


/*
** Main body of stand-alone interpreter (to be called in protected mode).
** Reads the options and handles them all.
*/
static int pmain (ecierthon_State *L) {
  int argc = (int)ecierthon_tointeger(L, 1);
  char **argv = (char **)ecierthon_touserdata(L, 2);
  int script;
  int args = collectargs(argv, &script);
  ecierthonL_checkversion(L);  /* check that interpreter has correct version */
  if (argv[0] && argv[0][0]) progname = argv[0];
  if (args == has_error) {  /* bad arg? */
    print_usage(argv[script]);  /* 'script' has index of bad arg. */
    return 0;
  }
  if (args & has_v)  /* option '-v'? */
    print_version();
  if (args & has_E) {  /* option '-E'? */
    ecierthon_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
    ecierthon_setfield(L, ecierthon_REGISTRYINDEX, "ecierthon_NOENV");
  }
  ecierthonL_openlibs(L);  /* open standard libraries */
  createargtable(L, argv, argc, script);  /* create table 'arg' */
  ecierthon_gc(L, ecierthon_GCGEN, 0, 0);  /* GC in generational mode */
  if (!(args & has_E)) {  /* no option '-E'? */
    if (handle_ecierthoninit(L) != ecierthon_OK)  /* run ecierthon_INIT */
      return 0;  /* error running ecierthon_INIT */
  }
  if (!runargs(L, argv, script))  /* execute arguments -e and -l */
    return 0;  /* something failed */
  if (script < argc &&  /* execute main script (if there is one) */
      handle_script(L, argv + script) != ecierthon_OK)
    return 0;
  if (args & has_i)  /* -i option? */
    doREPL(L);  /* do read-eval-print loop */
  else if (script == argc && !(args & (has_e | has_v))) {  /* no arguments? */
    if (ecierthon_stdin_is_tty()) {  /* running in interactive mode? */
      print_version();
      doREPL(L);  /* do read-eval-print loop */
    }
    else dofile(L, NULL);  /* executes stdin as a file */
  }
  ecierthon_pushboolean(L, 1);  /* signal no errors */
  return 1;
}


int main (int argc, char **argv) {
  int status, result;
  ecierthon_State *L = ecierthonL_newstate();  /* create state */
  if (L == NULL) {
    l_message(argv[0], "cannot create state: not enough memory");
    return EXIT_FAILURE;
  }
  ecierthon_pushcfunction(L, &pmain);  /* to call 'pmain' in protected mode */
  ecierthon_pushinteger(L, argc);  /* 1st argument */
  ecierthon_pushlightuserdata(L, argv); /* 2nd argument */
  status = ecierthon_pcall(L, 2, 1, 0);  /* do the call */
  result = ecierthon_toboolean(L, -1);  /* get result */
  report(L, status);
  ecierthon_close(L);
  return (result && status == ecierthon_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

