/*
 *
 */
#ifndef _FAILURE_H_
#define _FAILURE_H_

#include <stdarg.h>

void vfailure(int err, const char *fmt, va_list ap);
void failure(int err, const char *fmt, ...);

void verrmsg(int err, const char *fmt, va_list ap);
void errmsg(int err, const char *fmt, ...);

void errmsg_errno(const char *fmt, ...);
void failure_errno(const char *fmt, ...);

#endif
