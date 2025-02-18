/*
 *
 */

#include <stdlib.h>
#include <string.h>

void *zalloc(size_t size)
{
	void *p;

	p = malloc(size);
	if (!p)
		return NULL;

	memset(p, 0, size);
	return p;
}


