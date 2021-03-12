/*
** $Id: lundump.c $
** load precompiled ecierthon chunks
** See Copyright Notice in ecierthon.h
*/

#define lundump_c
#define ecierthon_CORE

#include "lprefix.h"


#include <limits.h>
#include <string.h>

#include "ecierthon.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstring.h"
#include "lundump.h"
#include "lzio.h"


#if !defined(ecierthoni_verifycode)
#define ecierthoni_verifycode(L,f)  /* empty */
#endif


typedef struct {
  ecierthon_State *L;
  ZIO *Z;
  const char *name;
} LoadState;


static l_noret error (LoadState *S, const char *why) {
  ecierthonO_pushfstring(S->L, "%s: bad binary format (%s)", S->name, why);
  ecierthonD_throw(S->L, ecierthon_ERRSYNTAX);
}


/*
** All high-level loads go through loadVector; you can change it to
** adapt to the endianness of the input
*/
#define loadVector(S,b,n)	loadBlock(S,b,(n)*sizeof((b)[0]))

static void loadBlock (LoadState *S, void *b, size_t size) {
  if (ecierthonZ_read(S->Z, b, size) != 0)
    error(S, "truncated chunk");
}


#define loadVar(S,x)		loadVector(S,&x,1)


static lu_byte loadByte (LoadState *S) {
  int b = zgetc(S->Z);
  if (b == EOZ)
    error(S, "truncated chunk");
  return cast_byte(b);
}


static size_t loadUnsigned (LoadState *S, size_t limit) {
  size_t x = 0;
  int b;
  limit >>= 7;
  do {
    b = loadByte(S);
    if (x >= limit)
      error(S, "integer overflow");
    x = (x << 7) | (b & 0x7f);
  } while ((b & 0x80) == 0);
  return x;
}


static size_t loadSize (LoadState *S) {
  return loadUnsigned(S, ~(size_t)0);
}


static int loadInt (LoadState *S) {
  return cast_int(loadUnsigned(S, INT_MAX));
}


static ecierthon_Number loadNumber (LoadState *S) {
  ecierthon_Number x;
  loadVar(S, x);
  return x;
}


static ecierthon_Integer loadInteger (LoadState *S) {
  ecierthon_Integer x;
  loadVar(S, x);
  return x;
}


/*
** Load a nullable string into prototype 'p'.
*/
static TString *loadStringN (LoadState *S, Proto *p) {
  ecierthon_State *L = S->L;
  TString *ts;
  size_t size = loadSize(S);
  if (size == 0)  /* no string? */
    return NULL;
  else if (--size <= ecierthonI_MAXSHORTLEN) {  /* short string? */
    char buff[ecierthonI_MAXSHORTLEN];
    loadVector(S, buff, size);  /* load string into buffer */
    ts = ecierthonS_newlstr(L, buff, size);  /* create string */
  }
  else {  /* long string */
    ts = ecierthonS_createlngstrobj(L, size);  /* create string */
    setsvalue2s(L, L->top, ts);  /* anchor it ('loadVector' can GC) */
    ecierthonD_inctop(L);
    loadVector(S, getstr(ts), size);  /* load directly in final place */
    L->top--;  /* pop string */
  }
  ecierthonC_objbarrier(L, p, ts);
  return ts;
}


/*
** Load a non-nullable string into prototype 'p'.
*/
static TString *loadString (LoadState *S, Proto *p) {
  TString *st = loadStringN(S, p);
  if (st == NULL)
    error(S, "bad format for constant string");
  return st;
}


static void loadCode (LoadState *S, Proto *f) {
  int n = loadInt(S);
  f->code = ecierthonM_newvectorchecked(S->L, n, Instruction);
  f->sizecode = n;
  loadVector(S, f->code, n);
}


static void loadFunction(LoadState *S, Proto *f, TString *psource);


static void loadConstants (LoadState *S, Proto *f) {
  int i;
  int n = loadInt(S);
  f->k = ecierthonM_newvectorchecked(S->L, n, TValue);
  f->sizek = n;
  for (i = 0; i < n; i++)
    setnilvalue(&f->k[i]);
  for (i = 0; i < n; i++) {
    TValue *o = &f->k[i];
    int t = loadByte(S);
    switch (t) {
      case ecierthon_VNIL:
        setnilvalue(o);
        break;
      case ecierthon_VFALSE:
        setbfvalue(o);
        break;
      case ecierthon_VTRUE:
        setbtvalue(o);
        break;
      case ecierthon_VNUMFLT:
        setfltvalue(o, loadNumber(S));
        break;
      case ecierthon_VNUMINT:
        setivalue(o, loadInteger(S));
        break;
      case ecierthon_VSHRSTR:
      case ecierthon_VLNGSTR:
        setsvalue2n(S->L, o, loadString(S, f));
        break;
      default: ecierthon_assert(0);
    }
  }
}


static void loadProtos (LoadState *S, Proto *f) {
  int i;
  int n = loadInt(S);
  f->p = ecierthonM_newvectorchecked(S->L, n, Proto *);
  f->sizep = n;
  for (i = 0; i < n; i++)
    f->p[i] = NULL;
  for (i = 0; i < n; i++) {
    f->p[i] = ecierthonF_newproto(S->L);
    ecierthonC_objbarrier(S->L, f, f->p[i]);
    loadFunction(S, f->p[i], f->source);
  }
}


/*
** Load the upvalues for a function. The names must be filled first,
** because the filling of the other fields can raise read errors and
** the creation of the error message can call an emergency collection;
** in that case all prototypes must be consistent for the GC.
*/
static void loadUpvalues (LoadState *S, Proto *f) {
  int i, n;
  n = loadInt(S);
  f->upvalues = ecierthonM_newvectorchecked(S->L, n, Upvaldesc);
  f->sizeupvalues = n;
  for (i = 0; i < n; i++)  /* make array valid for GC */
    f->upvalues[i].name = NULL;
  for (i = 0; i < n; i++) {  /* following calls can raise errors */
    f->upvalues[i].instack = loadByte(S);
    f->upvalues[i].idx = loadByte(S);
    f->upvalues[i].kind = loadByte(S);
  }
}


static void loadDebug (LoadState *S, Proto *f) {
  int i, n;
  n = loadInt(S);
  f->lineinfo = ecierthonM_newvectorchecked(S->L, n, ls_byte);
  f->sizelineinfo = n;
  loadVector(S, f->lineinfo, n);
  n = loadInt(S);
  f->abslineinfo = ecierthonM_newvectorchecked(S->L, n, AbsLineInfo);
  f->sizeabslineinfo = n;
  for (i = 0; i < n; i++) {
    f->abslineinfo[i].pc = loadInt(S);
    f->abslineinfo[i].line = loadInt(S);
  }
  n = loadInt(S);
  f->locvars = ecierthonM_newvectorchecked(S->L, n, LocVar);
  f->sizelocvars = n;
  for (i = 0; i < n; i++)
    f->locvars[i].varname = NULL;
  for (i = 0; i < n; i++) {
    f->locvars[i].varname = loadStringN(S, f);
    f->locvars[i].startpc = loadInt(S);
    f->locvars[i].endpc = loadInt(S);
  }
  n = loadInt(S);
  for (i = 0; i < n; i++)
    f->upvalues[i].name = loadStringN(S, f);
}


static void loadFunction (LoadState *S, Proto *f, TString *psource) {
  f->source = loadStringN(S, f);
  if (f->source == NULL)  /* no source in dump? */
    f->source = psource;  /* reuse parent's source */
  f->linedefined = loadInt(S);
  f->lastlinedefined = loadInt(S);
  f->numparams = loadByte(S);
  f->is_vararg = loadByte(S);
  f->maxstacksize = loadByte(S);
  loadCode(S, f);
  loadConstants(S, f);
  loadUpvalues(S, f);
  loadProtos(S, f);
  loadDebug(S, f);
}


static void checkliteral (LoadState *S, const char *s, const char *msg) {
  char buff[sizeof(LUA_SIGNATURE) + sizeof(ecierthonC_DATA)]; /* larger than both */
  size_t len = strlen(s);
  loadVector(S, buff, len);
  if (memcmp(s, buff, len) != 0)
    error(S, msg);
}


static void fchecksize (LoadState *S, size_t size, const char *tname) {
  if (loadByte(S) != size)
    error(S, ecierthonO_pushfstring(S->L, "%s size mismatch", tname));
}


#define checksize(S,t)	fchecksize(S,sizeof(t),#t)

static void checkHeader (LoadState *S) {
  /* skip 1st char (already read and checked) */
  checkliteral(S, &LUA_SIGNATURE[1], "not a binary chunk");
  if (loadByte(S) != ecierthonC_VERSION)
    error(S, "version mismatch");
  if (loadByte(S) != ecierthonC_FORMAT)
    error(S, "format mismatch");
  checkliteral(S, ecierthonC_DATA, "corrupted chunk");
  checksize(S, Instruction);
  checksize(S, ecierthon_Integer);
  checksize(S, ecierthon_Number);
  if (loadInteger(S) != ecierthonC_INT)
    error(S, "integer format mismatch");
  if (loadNumber(S) != ecierthonC_NUM)
    error(S, "float format mismatch");
}


/*
** Load precompiled chunk.
*/
LClosure *ecierthonU_undump(ecierthon_State *L, ZIO *Z, const char *name) {
  LoadState S;
  LClosure *cl;
  if (*name == '@' || *name == '=')
    S.name = name + 1;
  else if (*name == LUA_SIGNATURE[0])
    S.name = "binary string";
  else
    S.name = name;
  S.L = L;
  S.Z = Z;
  checkHeader(&S);
  cl = ecierthonF_newLclosure(L, loadByte(&S));
  setclLvalue2s(L, L->top, cl);
  ecierthonD_inctop(L);
  cl->p = ecierthonF_newproto(L);
  ecierthonC_objbarrier(L, cl, cl->p);
  loadFunction(&S, cl->p, NULL);
  ecierthon_assert(cl->nupvalues == cl->p->sizeupvalues);
  ecierthoni_verifycode(L, cl->p);
  return cl;
}

