/*
 *
 */
#ifndef _DEBUG_H_
#define _DEBUG_H_

#ifdef DEBUG
#include <stdio.h>
#include "dump_hex.h"
/* debug string with timestamp  */
void dbgts(const char *fmt, ...);
void dbg_set_pipe(FILE *pipe);

# define dbg(fmt, ...)		printf(DEBUG_PREFIX fmt "\n", ## __VA_ARGS__ )
# define dbg_dump_hex		dump_hex
#else
# define dbg_set_pipe(p)
# define dbgts(fmt, ...)
# define dbg(fmt, ...)
# define dbg_dump_hex(d, s, a)
#endif

#ifdef VERBOSE_DEBUG
# define vdbg		dbg
#else
# define vdbg(fmt, ...)
#endif

#endif
