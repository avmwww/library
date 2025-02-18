/*
 *
 */
#ifndef _DUMP_HEX_H_
#define _DUMP_HEX_H_

#include <stdlib.h>

/*
 * Dump buffer in hex format to stdout
 * \@param asc : != 0 print ASCII chars
 */
void dump_hex_prefix(const char *prefix, const void *buf, size_t size, int asc);
void dump_hex(const void *buf, size_t size, int asc);

#endif
