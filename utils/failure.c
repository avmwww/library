/*
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "failure.h"

void failure(int err, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (err)
		fprintf(stderr, ", %s\n", strerror(err));
	else
		fprintf(stderr, "\n");

	exit(EXIT_FAILURE);
}

