/*
** $Id: lobject.h $
** Type definitions for ecierthon objects
** See Copyright Notice in ecierthon.h
*/


#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "ecierthon.h"


/*
** Extra types for collectable non-values
*/
#define ecierthon_TUPVAL	ecierthon_NUMTYPES  /* upvalues */
#define ecierthon_TPROTO	(ecierthon_NUMTYPES+1)  /* function prototypes */
#define ecierthon_TDEADKEY	(ecierthon_NUMTYPES+2)  /* removed keys in tables */



/*
** number of all possible types (including ecierthon_TNONE but excluding DEADKEY)
*/
#define ecierthon_TOTALTYPES		(ecierthon_TPROTO + 2)


/*
** tags for Tagged Values have the following use of bits:
** bits 0-3: actual tag (a ecierthon_T* constant)
** bits 4-5: variant bits
** bit 6: whether value is collectable
*/

/* add variant bits to a type */
#define makevariant(t,v)	((t) | ((v) << 4))



/*
** Union of all ecierthon values
*/
typedef union Value {
  struct GCObject *gc;    /* collectable objects */
  void *p;         /* light userdata */
  ecierthon_CFunction f; /* light C functions */
  ecierthon_Integer i;   /* integer numbers */
  ecierthon_Number n;    /* float numbers */
} Value;


/*
** Tagged Values. This is the basic representation of values in ecierthon:
** an actual value plus a tag with its type.
*/

#define TValuefields	Value value_; lu_byte tt_

typedef struct TValue {
  TValuefields;
} TValue;


#define val_(o)		((o)->value_)
#define valraw(o)	(&val_(o))


/* raw type tag of a TValue */
#define rawtt(o)	((o)->tt_)

/* tag with no variants (bits 0-3) */
#define novariant(t)	((t) & 0x0F)

/* type tag of a TValue (bits 0-3 for tags + variant bits 4-5) */
#define withvariant(t)	((t) & 0x3F)
#define ttypetag(o)	withvariant(rawtt(o))

/* type of a TValue */
#define ttype(o)	(novariant(rawtt(o)))


/* Macros to test type */
#define checktag(o,t)		(rawtt(o) == (t))
#define checktype(o,t)		(ttype(o) == (t))


/* Macros for internal tests */

/* collectable object has the same tag as the original value */
#define righttt(obj)		(ttypetag(obj) == gcvalue(obj)->tt)

/*
** Any value being manipulated by the program either is non
** collectable, or the collectable object has the right tag
** and it is not dead. The option 'L == NULL' allows other
** macros using this one to be used where L is not available.
*/
#define checkliveness(L,obj) \
	((void)L, ecierthon_longassert(!iscollectable(obj) || \
		(righttt(obj) && (L == NULL || !isdead(G(L),gcvalue(obj))))))


/* Macros to set values */

/* set a value's tag */
#define settt_(o,t)	((o)->tt_=(t))


/* main macro to copy values (from 'obj1' to 'obj2') */
#define setobj(L,obj1,obj2) \
	{ TValue *io1=(obj1); const TValue *io2=(obj2); \
          io1->value_ = io2->value_; settt_(io1, io2->tt_); \
	  checkliveness(L,io1); ecierthon_assert(!isnonstrictnil(io1)); }

/*
** Different types of assignments, according to source and destination.
** (They are mostly equal now, but may be different in the future.)
*/

/* from stack to stack */
#define setobjs2s(L,o1,o2)	setobj(L,s2v(o1),s2v(o2))
/* to stack (not from same stack) */
#define setobj2s(L,o1,o2)	setobj(L,s2v(o1),o2)
/* from table to same table */
#define setobjt2t	setobj
/* to new object */
#define setobj2n	setobj
/* to table */
#define setobj2t	setobj


/*
** Entries in the ecierthon stack
*/
typedef union StackValue {
  TValue val;
} StackValue;


/* index to stack elements */
typedef StackValue *StkId;

/* convert a 'StackValue' to a 'TValue' */
#define s2v(o)	(&(o)->val)



/*
** {==================================================================
** Nil
** ===================================================================
*/

/* Standard nil */
#define ecierthon_VNIL	makevariant(ecierthon_TNIL, 0)

/* Empty slot (which might be different from a slot containing nil) */
#define ecierthon_VEMPTY	makevariant(ecierthon_TNIL, 1)

/* Value returned for a key not found in a table (absent key) */
#define ecierthon_VABSTKEY	makevariant(ecierthon_TNIL, 2)


/* macro to test for (any kind of) nil */
#define ttisnil(v)		checktype((v), ecierthon_TNIL)


/* macro to test for a standard nil */
#define ttisstrictnil(o)	checktag((o), ecierthon_VNIL)


#define setnilvalue(obj) settt_(obj, ecierthon_VNIL)


#define isabstkey(v)		checktag((v), ecierthon_VABSTKEY)


/*
** macro to detect non-standard nils (used only in assertions)
*/
#define isnonstrictnil(v)	(ttisnil(v) && !ttisstrictnil(v))


/*
** By default, entries with any kind of nil are considered empty.
** (In any definition, values associated with absent keys must also
** be accepted as empty.)
*/
#define isempty(v)		ttisnil(v)


/* macro defining a value corresponding to an absent key */
#define ABSTKEYCONSTANT		{NULL}, ecierthon_VABSTKEY


/* mark an entry as empty */
#define setempty(v)		settt_(v, ecierthon_VEMPTY)



/* }================================================================== */


/*
** {==================================================================
** Booleans
** ===================================================================
*/


#define ecierthon_VFALSE	makevariant(ecierthon_TBOOLEAN, 0)
#define ecierthon_VTRUE	makevariant(ecierthon_TBOOLEAN, 1)

#define ttisboolean(o)		checktype((o), ecierthon_TBOOLEAN)
#define ttisfalse(o)		checktag((o), ecierthon_VFALSE)
#define ttistrue(o)		checktag((o), ecierthon_VTRUE)


#define l_isfalse(o)	(ttisfalse(o) || ttisnil(o))


#define setbfvalue(obj)		settt_(obj, ecierthon_VFALSE)
#define setbtvalue(obj)		settt_(obj, ecierthon_VTRUE)

/* }================================================================== */


/*
** {==================================================================
** Threads
** ===================================================================
*/

#define ecierthon_VTHREAD		makevariant(ecierthon_TTHREAD, 0)

#define ttisthread(o)		checktag((o), ctb(ecierthon_VTHREAD))

#define thvalue(o)	check_exp(ttisthread(o), gco2th(val_(o).gc))

#define setthvalue(L,obj,x) \
  { TValue *io = (obj); ecierthon_State *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(ecierthon_VTHREAD)); \
    checkliveness(L,io); }

#define setthvalue2s(L,o,t)	setthvalue(L,s2v(o),t)

/* }================================================================== */


/*
** {==================================================================
** Collectable Objects
** ===================================================================
*/

/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
#define CommonHeader	struct GCObject *next; lu_byte tt; lu_byte marked


/* Common type for all collectable objects */
typedef struct GCObject {
  CommonHeader;
} GCObject;


/* Bit mark for collectable types */
#define BIT_ISCOLLECTABLE	(1 << 6)

#define iscollectable(o)	(rawtt(o) & BIT_ISCOLLECTABLE)

/* mark a tag as collectable */
#define ctb(t)			((t) | BIT_ISCOLLECTABLE)

#define gcvalue(o)	check_exp(iscollectable(o), val_(o).gc)

#define gcvalueraw(v)	((v).gc)

#define setgcovalue(L,obj,x) \
  { TValue *io = (obj); GCObject *i_g=(x); \
    val_(io).gc = i_g; settt_(io, ctb(i_g->tt)); }

/* }================================================================== */


/*
** {==================================================================
** Numbers
** ===================================================================
*/

/* Variant tags for numbers */
#define ecierthon_VNUMINT	makevariant(ecierthon_TNUMBER, 0)  /* integer numbers */
#define ecierthon_VNUMFLT	makevariant(ecierthon_TNUMBER, 1)  /* float numbers */

#define ttisnumber(o)		checktype((o), ecierthon_TNUMBER)
#define ttisfloat(o)		checktag((o), ecierthon_VNUMFLT)
#define ttisinteger(o)		checktag((o), ecierthon_VNUMINT)

#define nvalue(o)	check_exp(ttisnumber(o), \
	(ttisinteger(o) ? cast_num(ivalue(o)) : fltvalue(o)))
#define fltvalue(o)	check_exp(ttisfloat(o), val_(o).n)
#define ivalue(o)	check_exp(ttisinteger(o), val_(o).i)

#define fltvalueraw(v)	((v).n)
#define ivalueraw(v)	((v).i)

#define setfltvalue(obj,x) \
  { TValue *io=(obj); val_(io).n=(x); settt_(io, ecierthon_VNUMFLT); }

#define chgfltvalue(obj,x) \
  { TValue *io=(obj); ecierthon_assert(ttisfloat(io)); val_(io).n=(x); }

#define setivalue(obj,x) \
  { TValue *io=(obj); val_(io).i=(x); settt_(io, ecierthon_VNUMINT); }

#define chgivalue(obj,x) \
  { TValue *io=(obj); ecierthon_assert(ttisinteger(io)); val_(io).i=(x); }

/* }================================================================== */


/*
** {==================================================================
** Strings
** ===================================================================
*/

/* Variant tags for strings */
#define ecierthon_VSHRSTR	makevariant(ecierthon_TSTRING, 0)  /* short strings */
#define ecierthon_VLNGSTR	makevariant(ecierthon_TSTRING, 1)  /* long strings */

#define ttisstring(o)		checktype((o), ecierthon_TSTRING)
#define ttisshrstring(o)	checktag((o), ctb(ecierthon_VSHRSTR))
#define ttislngstring(o)	checktag((o), ctb(ecierthon_VLNGSTR))

#define tsvalueraw(v)	(gco2ts((v).gc))

#define tsvalue(o)	check_exp(ttisstring(o), gco2ts(val_(o).gc))

#define setsvalue(L,obj,x) \
  { TValue *io = (obj); TString *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(x_->tt)); \
    checkliveness(L,io); }

/* set a string to the stack */
#define setsvalue2s(L,o,s)	setsvalue(L,s2v(o),s)

/* set a string to a new object */
#define setsvalue2n	setsvalue


/*
** Header for a string value.
*/
typedef struct TString {
  CommonHeader;
  lu_byte extra;  /* reserved words for short strings; "has hash" for longs */
  lu_byte shrlen;  /* length for short strings */
  unsigned int hash;
  union {
    size_t lnglen;  /* length for long strings */
    struct TString *hnext;  /* linked list for hash table */
  } u;
  char contents[1];
} TString;



/*
** Get the actual string (array of bytes) from a 'TString'.
*/
#define getstr(ts)  ((ts)->contents)


/* get the actual string (array of bytes) from a ecierthon value */
#define svalue(o)       getstr(tsvalue(o))

/* get string length from 'TString *s' */
#define tsslen(s)	((s)->tt == ecierthon_VSHRSTR ? (s)->shrlen : (s)->u.lnglen)

/* get string length from 'TValue *o' */
#define vslen(o)	tsslen(tsvalue(o))

/* }================================================================== */


/*
** {==================================================================
** Userdata
** ===================================================================
*/


/*
** Light userdata should be a variant of userdata, but for compatibility
** reasons they are also different types.
*/
#define ecierthon_VLIGHTUSERDATA	makevariant(ecierthon_TLIGHTUSERDATA, 0)

#define ecierthon_VUSERDATA		makevariant(ecierthon_TUSERDATA, 0)

#define ttislightuserdata(o)	checktag((o), ecierthon_VLIGHTUSERDATA)
#define ttisfulluserdata(o)	checktag((o), ctb(ecierthon_VUSERDATA))

#define pvalue(o)	check_exp(ttislightuserdata(o), val_(o).p)
#define uvalue(o)	check_exp(ttisfulluserdata(o), gco2u(val_(o).gc))

#define pvalueraw(v)	((v).p)

#define setpvalue(obj,x) \
  { TValue *io=(obj); val_(io).p=(x); settt_(io, ecierthon_VLIGHTUSERDATA); }

#define setuvalue(L,obj,x) \
  { TValue *io = (obj); Udata *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(ecierthon_VUSERDATA)); \
    checkliveness(L,io); }


/* Ensures that addresses after this type are always fully aligned. */
typedef union UValue {
  TValue uv;
  ecierthonI_MAXALIGN;  /* ensures maximum alignment for udata bytes */
} UValue;


/*
** Header for userdata with user values;
** memory area follows the end of this structure.
*/
typedef struct Udata {
  CommonHeader;
  unsigned short nuvalue;  /* number of user values */
  size_t len;  /* number of bytes */
  struct Table *metatable;
  GCObject *gclist;
  UValue uv[1];  /* user values */
} Udata;


/*
** Header for userdata with no user values. These userdata do not need
** to be gray during GC, and therefore do not need a 'gclist' field.
** To simplify, the code always use 'Udata' for both kinds of userdata,
** making sure it never accesses 'gclist' on userdata with no user values.
** This structure here is used only to compute the correct size for
** this representation. (The 'bindata' field in its end ensures correct
** alignment for binary data following this header.)
*/
typedef struct Udata0 {
  CommonHeader;
  unsigned short nuvalue;  /* number of user values */
  size_t len;  /* number of bytes */
  struct Table *metatable;
  union {ecierthonI_MAXALIGN;} bindata;
} Udata0;


/* compute the offset of the memory area of a userdata */
#define udatamemoffset(nuv) \
	((nuv) == 0 ? offsetof(Udata0, bindata)  \
                    : offsetof(Udata, uv) + (sizeof(UValue) * (nuv)))

/* get the address of the memory block inside 'Udata' */
#define getudatamem(u)	(cast_charp(u) + udatamemoffset((u)->nuvalue))

/* compute the size of a userdata */
#define sizeudata(nuv,nb)	(udatamemoffset(nuv) + (nb))

/* }================================================================== */


/*
** {==================================================================
** Prototypes
** ===================================================================
*/

#define ecierthon_VPROTO	makevariant(ecierthon_TPROTO, 0)


/*
** Description of an upvalue for function prototypes
*/
typedef struct Upvaldesc {
  TString *name;  /* upvalue name (for debug information) */
  lu_byte instack;  /* whether it is in stack (register) */
  lu_byte idx;  /* index of upvalue (in stack or in outer function's list) */
  lu_byte kind;  /* kind of corresponding variable */
} Upvaldesc;


/*
** Description of a local variable for function prototypes
** (used for debug information)
*/
typedef struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;


/*
** Associates the absolute line source for a given instruction ('pc').
** The array 'lineinfo' gives, for each instruction, the difference in
** lines from the previous instruction. When that difference does not
** fit into a byte, ecierthon saves the absolute line for that instruction.
** (ecierthon also saves the absolute line periodically, to speed up the
** computation of a line number: we can use binary search in the
** absolute-line array, but we must traverse the 'lineinfo' array
** linearly to compute a line.)
*/
typedef struct AbsLineInfo {
  int pc;
  int line;
} AbsLineInfo;

/*
** Function Prototypes
*/
typedef struct Proto {
  CommonHeader;
  lu_byte numparams;  /* number of fixed (named) parameters */
  lu_byte is_vararg;
  lu_byte maxstacksize;  /* number of registers needed by this function */
  int sizeupvalues;  /* size of 'upvalues' */
  int sizek;  /* size of 'k' */
  int sizecode;
  int sizelineinfo;
  int sizep;  /* size of 'p' */
  int sizelocvars;
  int sizeabslineinfo;  /* size of 'abslineinfo' */
  int linedefined;  /* debug information  */
  int lastlinedefined;  /* debug information  */
  TValue *k;  /* constants used by the function */
  Instruction *code;  /* opcodes */
  struct Proto **p;  /* functions defined inside the function */
  Upvaldesc *upvalues;  /* upvalue information */
  ls_byte *lineinfo;  /* information about source lines (debug information) */
  AbsLineInfo *abslineinfo;  /* idem */
  LocVar *locvars;  /* information about local variables (debug information) */
  TString  *source;  /* used for debug information */
  GCObject *gclist;
} Proto;

/* }================================================================== */


/*
** {==================================================================
** Functions
** ===================================================================
*/

#define ecierthon_VUPVAL	makevariant(ecierthon_TUPVAL, 0)


/* Variant tags for functions */
#define ecierthon_VLCL	makevariant(ecierthon_TFUNCTION, 0)  /* ecierthon closure */
#define ecierthon_VLCF	makevariant(ecierthon_TFUNCTION, 1)  /* light C function */
#define ecierthon_VCCL	makevariant(ecierthon_TFUNCTION, 2)  /* C closure */

#define ttisfunction(o)		checktype(o, ecierthon_TFUNCTION)
#define ttisclosure(o)		((rawtt(o) & 0x1F) == ecierthon_VLCL)
#define ttisLclosure(o)		checktag((o), ctb(ecierthon_VLCL))
#define ttislcf(o)		checktag((o), ecierthon_VLCF)
#define ttisCclosure(o)		checktag((o), ctb(ecierthon_VCCL))

#define isLfunction(o)	ttisLclosure(o)

#define clvalue(o)	check_exp(ttisclosure(o), gco2cl(val_(o).gc))
#define clLvalue(o)	check_exp(ttisLclosure(o), gco2lcl(val_(o).gc))
#define fvalue(o)	check_exp(ttislcf(o), val_(o).f)
#define clCvalue(o)	check_exp(ttisCclosure(o), gco2ccl(val_(o).gc))

#define fvalueraw(v)	((v).f)

#define setclLvalue(L,obj,x) \
  { TValue *io = (obj); LClosure *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(ecierthon_VLCL)); \
    checkliveness(L,io); }

#define setclLvalue2s(L,o,cl)	setclLvalue(L,s2v(o),cl)

#define setfvalue(obj,x) \
  { TValue *io=(obj); val_(io).f=(x); settt_(io, ecierthon_VLCF); }

#define setclCvalue(L,obj,x) \
  { TValue *io = (obj); CClosure *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(ecierthon_VCCL)); \
    checkliveness(L,io); }


/*
** Upvalues for ecierthon closures
*/
typedef struct UpVal {
  CommonHeader;
  lu_byte tbc;  /* true if it represents a to-be-closed variable */
  TValue *v;  /* points to stack or to its own value */
  union {
    struct {  /* (when open) */
      struct UpVal *next;  /* linked list */
      struct UpVal **previous;
    } open;
    TValue value;  /* the value (when closed) */
  } u;
} UpVal;



#define ClosureHeader \
	CommonHeader; lu_byte nupvalues; GCObject *gclist

typedef struct CClosure {
  ClosureHeader;
  ecierthon_CFunction f;
  TValue upvalue[1];  /* list of upvalues */
} CClosure;


typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;
  UpVal *upvals[1];  /* list of upvalues */
} LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;


#define getproto(o)	(clLvalue(o)->p)

/* }================================================================== */


/*
** {==================================================================
** Tables
** ===================================================================
*/

#define ecierthon_VTABLE	makevariant(ecierthon_TTABLE, 0)

#define ttistable(o)		checktag((o), ctb(ecierthon_VTABLE))

#define hvalue(o)	check_exp(ttistable(o), gco2t(val_(o).gc))

#define sethvalue(L,obj,x) \
  { TValue *io = (obj); Table *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(ecierthon_VTABLE)); \
    checkliveness(L,io); }

#define sethvalue2s(L,o,h)	sethvalue(L,s2v(o),h)


/*
** Nodes for Hash tables: A pack of two TValue's (key-value pairs)
** plus a 'next' field to link colliding entries. The distribution
** of the key's fields ('key_tt' and 'key_val') not forming a proper
** 'TValue' allows for a smaller size for 'Node' both in 4-byte
** and 8-byte alignments.
*/
typedef union Node {
  struct NodeKey {
    TValuefields;  /* fields for value */
    lu_byte key_tt;  /* key type */
    int next;  /* for chaining */
    Value key_val;  /* key value */
  } u;
  TValue i_val;  /* direct access to node's value as a proper 'TValue' */
} Node;


/* copy a value into a key */
#define setnodekey(L,node,obj) \
	{ Node *n_=(node); const TValue *io_=(obj); \
	  n_->u.key_val = io_->value_; n_->u.key_tt = io_->tt_; \
	  checkliveness(L,io_); }


/* copy a value from a key */
#define getnodekey(L,obj,node) \
	{ TValue *io_=(obj); const Node *n_=(node); \
	  io_->value_ = n_->u.key_val; io_->tt_ = n_->u.key_tt; \
	  checkliveness(L,io_); }


/*
** About 'alimit': if 'isrealasize(t)' is true, then 'alimit' is the
** real size of 'array'. Otherwise, the real size of 'array' is the
** smallest power of two not smaller than 'alimit' (or zero iff 'alimit'
** is zero); 'alimit' is then used as a hint for #t.
*/

#define BITRAS		(1 << 7)
#define isrealasize(t)		(!((t)->flags & BITRAS))
#define setrealasize(t)		((t)->flags &= cast_byte(~BITRAS))
#define setnorealasize(t)	((t)->flags |= BITRAS)


typedef struct Table {
  CommonHeader;
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */
  lu_byte lsizenode;  /* log2 of size of 'node' array */
  unsigned int alimit;  /* "limit" of 'array' array */
  TValue *array;  /* array part */
  Node *node;
  Node *lastfree;  /* any free position is before this position */
  struct Table *metatable;
  GCObject *gclist;
} Table;


/*
** Macros to manipulate keys inserted in nodes
*/
#define keytt(node)		((node)->u.key_tt)
#define keyval(node)		((node)->u.key_val)

#define keyisnil(node)		(keytt(node) == ecierthon_TNIL)
#define keyisinteger(node)	(keytt(node) == ecierthon_VNUMINT)
#define keyival(node)		(keyval(node).i)
#define keyisshrstr(node)	(keytt(node) == ctb(ecierthon_VSHRSTR))
#define keystrval(node)		(gco2ts(keyval(node).gc))

#define setnilkey(node)		(keytt(node) = ecierthon_TNIL)

#define keyiscollectable(n)	(keytt(n) & BIT_ISCOLLECTABLE)

#define gckey(n)	(keyval(n).gc)
#define gckeyN(n)	(keyiscollectable(n) ? gckey(n) : NULL)


/*
** Dead keys in tables have the tag DEADKEY but keep their original
** gcvalue. This distinguishes them from regular keys but allows them to
** be found when searched in a special way. ('next' needs that to find
** keys removed from a table during a traversal.)
*/
#define setdeadkey(node)	(keytt(node) = ecierthon_TDEADKEY)
#define keyisdead(node)		(keytt(node) == ecierthon_TDEADKEY)

/* }================================================================== */



/*
** 'module' operation for hashing (size is always a power of 2)
*/
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast_int((s) & ((size)-1)))))


#define twoto(x)	(1<<(x))
#define sizenode(t)	(twoto((t)->lsizenode))


/* size of buffer for 'ecierthonO_utf8esc' function */
#define UTF8BUFFSZ	8

ecierthonI_FUNC int ecierthonO_utf8esc (char *buff, unsigned long x);
ecierthonI_FUNC int ecierthonO_ceillog2 (unsigned int x);
ecierthonI_FUNC int ecierthonO_rawarith (ecierthon_State *L, int op, const TValue *p1,
                             const TValue *p2, TValue *res);
ecierthonI_FUNC void ecierthonO_arith (ecierthon_State *L, int op, const TValue *p1,
                           const TValue *p2, StkId res);
ecierthonI_FUNC size_t ecierthonO_str2num (const char *s, TValue *o);
ecierthonI_FUNC int ecierthonO_hexavalue (int c);
ecierthonI_FUNC void ecierthonO_tostring (ecierthon_State *L, TValue *obj);
ecierthonI_FUNC const char *ecierthonO_pushvfstring (ecierthon_State *L, const char *fmt,
                                                       va_list argp);
ecierthonI_FUNC const char *ecierthonO_pushfstring (ecierthon_State *L, const char *fmt, ...);
ecierthonI_FUNC void ecierthonO_chunkid (char *out, const char *source, size_t srclen);


#endif

