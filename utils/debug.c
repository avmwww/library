/*
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "debug.h"

#ifdef DEBUG
static FILE *dbgio = NULL;

void dbgts(const char *fmt, ...)
{
	char dbuf[64];
	struct timeval tv;
	struct tm tm;
	time_t t;
	va_list ap;
	int n;

	if (!dbgio)
		dbgio = stdout;

	gettimeofday(&tv, NULL);
	t = tv.tv_sec;
	localtime_r(&t, &tm);
	dbuf[0] = '\0';
	strftime(dbuf, sizeof(dbuf), "%b %d %Y %2H:%2M:%2S", &tm);
	n = strlen(dbuf);
	if (n > 0)
		dbuf[n] = '\0';

	fprintf(dbgio, "%s.%.03ld: ", dbuf, tv.tv_usec / 1000);
	va_start(ap, fmt);
	vfprintf(dbgio, fmt, ap);
	va_end(ap);
	n = strlen(fmt);

	if (n > 0 && fmt[n - 1] != '\n')
		fprintf(dbgio, "\n");
}

void dbg_set_pipe(FILE *pipe)
{
	dbgio = pipe;
}
#endif
