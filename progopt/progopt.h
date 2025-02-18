/*
 *
 */
#ifndef _PROGOPT_H_
#define _PROGOPT_H_

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <getopt.h>

struct prog_option {
	char opshort;
	const char *oplong;
	const char *desc;
	enum {
		OPT_NO = 0,
		OPT_INT,
		OPT_HEX,
		OPT_STRING,
		OPT_END = -1,
	} type;
	size_t offt;
	int set;
};

#define PROG_END		\
		{.opshort = 0, .oplong = NULL, .type = OPT_END,}

#define PROG_OPT(sopt, lopt, desc, type, obj, memb, set) \
		{sopt, lopt, desc, type, offsetof(obj, memb), set}

int prog_option_make(struct prog_option *popt, struct option *opt, char *optstr, int len);

int prog_option_load(int argc, char * const argv[],
		struct prog_option *popt, struct option *opt,
		const char *optstr, void *ctx);

void prog_option_printf(FILE *out, struct prog_option *opt);

#endif
