/*
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "failure.h"

void verrmsg(int err, const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
	if (err)
		fprintf(stderr, ", %s\n", strerror(err));
	else
		fprintf(stderr, "\n");
}

void errmsg(int err, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verrmsg(err, fmt, ap);
	va_end(ap);
}

void errmsg_errno(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verrmsg(errno, fmt, ap);
	va_end(ap);
}

void vfailure(int err, const char *fmt, va_list ap)
{
	verrmsg(err, fmt, ap);
	exit(EXIT_FAILURE);
}

void failure(int err, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfailure(err, fmt, ap);
	va_end(ap);
}

void failure_errno(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfailure(errno, fmt, ap);
	va_end(ap);
}
