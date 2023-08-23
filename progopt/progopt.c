/*
 *
 */

#include <getopt.h>

#include"progopt.h"

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
			if (c == 0) {
				/* long option returned */
				//p = opt[optindex];
			}
			if (p->opshort == c) {
				switch (p->type) {
					case OPT_INT:
						*((int *)(ctx + p->offt)) = strtol(optarg, NULL, 10);
						break;
					case OPT_HEX:
						*((int *)(ctx + p->offt)) = strtol(optarg, NULL, 0);
						break;
					case OPT_STRING:
						*((char **)(ctx + p->offt)) = optarg;
						break;
					case OPT_NO:
						*((int *)(ctx + p->offt)) = p->set;
						break;
					default:
						break;
				}
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

