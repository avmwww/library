/*
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "dump_hex.h"

/*
 *
 */
void dump_hex_prefix(const char *prefix, const void *buf, size_t size, int asc)
{
	const uint8_t *p = (const uint8_t *)buf;
	int i = 0;
	char ascbuf[17];

	memset(ascbuf, 0, sizeof(ascbuf));
	while (size-- > 0) {
		if (i == 0) {
			printf("%s ", prefix);
		}
		if (asc) {
			if (isprint(*p))
				ascbuf[i] = *p;
			else
				ascbuf[i] = '.';
		}
		printf("%02x ", *p++);
		if (++i == 16) {
			i = 0;
			if (asc)
				printf("\t%s", ascbuf);
			printf("\n");
		}
	}
	if (i) {
		ascbuf[i] = '\0';
		if (asc) {
			while (i++ < 16)
				printf("   ");
			printf("\t%s", ascbuf);
		}
		printf("\n");
	}
}

void dump_hex(const void *buf, size_t size, int asc)
{
	dump_hex_prefix(NULL, buf, size, asc);
}
