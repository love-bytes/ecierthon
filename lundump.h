/*
** $Id: lundump.h $
** load precompiled ecierthon chunks
** See Copyright Notice in ecierthon.h
*/

#ifndef lundump_h
#define lundump_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define ecierthonC_DATA	"\x19\x93\r\n\x1a\n"

#define ecierthonC_INT	0x5678
#define ecierthonC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define MYINT(s)	(s[0]-'0')  /* assume one-digit numerals */
#define ecierthonC_VERSION	(MYINT(ecierthon_VERSION_MAJOR)*16+MYINT(ecierthon_VERSION_MINOR))

#define ecierthonC_FORMAT	0	/* this is the official format */

/* load one chunk; from lundump.c */
ecierthonI_FUNC LClosure* ecierthonU_undump (ecierthon_State* L, ZIO* Z, const char* name);

/* dump one chunk; from ldump.c */
ecierthonI_FUNC int ecierthonU_dump (ecierthon_State* L, const Proto* f, ecierthon_Writer w,
                         void* data, int strip);

#endif
