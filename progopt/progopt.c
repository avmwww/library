/*
 *
 */

#include <getopt.h>
#include <string.h>
#include <stdio.h>

#include"progopt.h"

#ifdef __MINGW32__
#define PRI_L ""
#else
#define PRI_L "l"
#endif

void prog_option_dump(struct prog_option *popt)
{
	printf("PROG OPTIONS\n");
	printf("\tShort: %c(0x%02x)\n", popt->opshort, popt->opshort);
	printf("\tLong: %s\n", popt->oplong);
	printf("\tDescription: %s\n", popt->desc);
	printf("\tType: %d\n", popt->type);
	printf("\tOffset: %" PRI_L "d\n", popt->offt);
	printf("\tValue: %d\n", popt->set);
}

int prog_option_make(struct prog_option *popt, struct option *opt,
		     char *optstr, int len)
{
	while (popt->type != OPT_END && len > 0) {

		opt->name = popt->oplong;
		opt->val = popt->opshort;
		opt->has_arg = (popt->type == OPT_NO) ? no_argument : required_argument;
		opt->flag = 0; /* return val */
		if (popt->opshort) {
			*optstr++ = popt->opshort;
			if (popt->type != OPT_NO)
				*optstr++ = ':';
		}

		opt++;
		popt++;
		len--;
	}

	if (len == 0 && popt->type != OPT_END)
		return -1;

	opt->name = 0;
	opt->val = 0;
	opt->has_arg = 0;
	opt->flag = 0;

	*optstr = '\0';

	return 0;
}

static int prog_option_get(struct prog_option *popt, char *arg, void *ctx)
{
	switch (popt->type) {
		case OPT_INT:
			*((int *)(ctx + popt->offt)) = strtol(arg, NULL, 10);
			break;
		case OPT_HEX:
			*((int *)(ctx + popt->offt)) = strtol(arg, NULL, 0);
			break;
		case OPT_STRING:
			*((char **)(ctx + popt->offt)) = arg;
			break;
		case OPT_NO:
			*((int *)(ctx + popt->offt)) = popt->set;
			break;
		default:
			return -1;
	}
	return 0;
}

int prog_option_load(int argc, char * const argv[],
		     struct prog_option *popt, struct option *opt,
		     const char *optstr, void *ctx)
{
	int c;
	int optindex = 0;
	struct prog_option *p;

	while ((c = getopt_long(argc, argv, optstr, opt, &optindex)) != -1) {
		p = popt;
		while (p->type != OPT_END) {
#if 0
			printf("getopt_long return %02x(%d), index %d\n", c, c, optindex);
#endif
			if (c == 0) {
#if 0
				prog_option_dump(p);
#endif
				/* long option returned */
				if (!strcmp(opt[optindex].name, p->oplong)) {
#if 0
					printf("Long option: %s, index %d\n", opt[optindex].name, optindex);
#endif
					prog_option_get(p, optarg, ctx);
					break;
				}
			} else if (p->opshort == c) {
				prog_option_get(p, optarg, ctx);
				break;
			}
			p++;
		}
		if (p->opshort == '\0' && p->oplong == 0)
			return -1;
	}
	return 0;
}

/*
 *
 */
void prog_option_printf(FILE *out, struct prog_option *opt)
{
	fprintf(out, "Options:\n");

	while (opt->type != OPT_END) {
		fprintf(out, "\t");
		if (opt->opshort)
			fprintf(out, "-%c", opt->opshort);

		if (opt->oplong) {
			if (opt->opshort)
				fprintf(out, ", ");
			else
				fprintf(out, "    ");
			fprintf(out, "--%s", opt->oplong);
		}

		fprintf(out, " %s\n", opt->desc);
		opt++;
	}
}

