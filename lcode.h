/*
** $Id: lcode.h $
** Code generator for ecierthon
** See Copyright Notice in ecierthon.h
*/

#ifndef lcode_h
#define lcode_h

#include "llex.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)


/*
** grep "ORDER OPR" if you change these enums  (ORDER OP)
*/
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR,
  OPR_SHL, OPR_SHR,
  /* string operator */
  OPR_CONCAT,
  /* comparison operators */
  OPR_EQ, OPR_LT, OPR_LE,
  OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;


/* true if operation is foldable (that is, it is arithmetic or bitwise) */
#define foldbinop(op)	((op) <= OPR_SHR)


#define ecierthonK_codeABC(fs,o,a,b,c)	ecierthonK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define ecierthonK_setmultret(fs,e)	ecierthonK_setreturns(fs, e, ecierthon_MULTRET)

#define ecierthonK_jumpto(fs,t)	ecierthonK_patchlist(fs, ecierthonK_jump(fs), t)

ecierthonI_FUNC int ecierthonK_code (FuncState *fs, Instruction i);
ecierthonI_FUNC int ecierthonK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx);
ecierthonI_FUNC int ecierthonK_codeAsBx (FuncState *fs, OpCode o, int A, int Bx);
ecierthonI_FUNC int ecierthonK_codeABCk (FuncState *fs, OpCode o, int A,
                                            int B, int C, int k);
ecierthonI_FUNC int ecierthonK_isKint (expdesc *e);
ecierthonI_FUNC int ecierthonK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
ecierthonI_FUNC void ecierthonK_fixline (FuncState *fs, int line);
ecierthonI_FUNC void ecierthonK_nil (FuncState *fs, int from, int n);
ecierthonI_FUNC void ecierthonK_reserveregs (FuncState *fs, int n);
ecierthonI_FUNC void ecierthonK_checkstack (FuncState *fs, int n);
ecierthonI_FUNC void ecierthonK_int (FuncState *fs, int reg, ecierthon_Integer n);
ecierthonI_FUNC void ecierthonK_dischargevars (FuncState *fs, expdesc *e);
ecierthonI_FUNC int ecierthonK_exp2anyreg (FuncState *fs, expdesc *e);
ecierthonI_FUNC void ecierthonK_exp2anyregup (FuncState *fs, expdesc *e);
ecierthonI_FUNC void ecierthonK_exp2nextreg (FuncState *fs, expdesc *e);
ecierthonI_FUNC void ecierthonK_exp2val (FuncState *fs, expdesc *e);
ecierthonI_FUNC int ecierthonK_exp2RK (FuncState *fs, expdesc *e);
ecierthonI_FUNC void ecierthonK_self (FuncState *fs, expdesc *e, expdesc *key);
ecierthonI_FUNC void ecierthonK_indexed (FuncState *fs, expdesc *t, expdesc *k);
ecierthonI_FUNC void ecierthonK_goiftrue (FuncState *fs, expdesc *e);
ecierthonI_FUNC void ecierthonK_goiffalse (FuncState *fs, expdesc *e);
ecierthonI_FUNC void ecierthonK_storevar (FuncState *fs, expdesc *var, expdesc *e);
ecierthonI_FUNC void ecierthonK_setreturns (FuncState *fs, expdesc *e, int nresults);
ecierthonI_FUNC void ecierthonK_setoneret (FuncState *fs, expdesc *e);
ecierthonI_FUNC int ecierthonK_jump (FuncState *fs);
ecierthonI_FUNC void ecierthonK_ret (FuncState *fs, int first, int nret);
ecierthonI_FUNC void ecierthonK_patchlist (FuncState *fs, int list, int target);
ecierthonI_FUNC void ecierthonK_patchtohere (FuncState *fs, int list);
ecierthonI_FUNC void ecierthonK_concat (FuncState *fs, int *l1, int l2);
ecierthonI_FUNC int ecierthonK_getlabel (FuncState *fs);
ecierthonI_FUNC void ecierthonK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
ecierthonI_FUNC void ecierthonK_infix (FuncState *fs, BinOpr op, expdesc *v);
ecierthonI_FUNC void ecierthonK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
ecierthonI_FUNC void ecierthonK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
ecierthonI_FUNC void ecierthonK_setlist (FuncState *fs, int base, int nelems, int tostore);
ecierthonI_FUNC void ecierthonK_finish (FuncState *fs);
ecierthonI_FUNC l_noret ecierthonK_semerror (LexState *ls, const char *msg);


#endif
