/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in ecierthon.h
*/


#ifndef lzio_h
#define lzio_h

#include "ecierthon.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : ecierthonZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define ecierthonZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define ecierthonZ_buffer(buff)	((buff)->buffer)
#define ecierthonZ_sizebuffer(buff)	((buff)->buffsize)
#define ecierthonZ_bufflen(buff)	((buff)->n)

#define ecierthonZ_buffremove(buff,i)	((buff)->n -= (i))
#define ecierthonZ_resetbuffer(buff) ((buff)->n = 0)


#define ecierthonZ_resizebuffer(L, buff, size) \
	((buff)->buffer = ecierthonM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define ecierthonZ_freebuffer(L, buff)	ecierthonZ_resizebuffer(L, buff, 0)


ecierthonI_FUNC void ecierthonZ_init (ecierthon_State *L, ZIO *z, ecierthon_Reader reader,
                                        void *data);
ecierthonI_FUNC size_t ecierthonZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  ecierthon_Reader reader;		/* reader function */
  void *data;			/* additional data */
  ecierthon_State *L;			/* ecierthon state (for reader) */
};


ecierthonI_FUNC int ecierthonZ_fill (ZIO *z);

#endif
