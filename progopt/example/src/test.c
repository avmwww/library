/*
 * Example use of progopt interface
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 *
 * ./test -a 0x8723fde -d 238743 -s "hello world" --string "HELLO" -n
 */

#include <stdlib.h>
#include <stdio.h>

#include "progopt.h"

#define INT_VALUE_DEFAULT		1234567

struct test_context {
	int hex_value;
	int int_value;
	char *str_value;
	char *string_value;
	int noopt;
	int help;
};

#define XINTSTR(s)				INTSTR(s)
#define INTSTR(s)				#s


/* <short opt> <long opt> <description> <type> <offset> <value> */
#define TEST_OPT(s, l, d, t, o, v) \
		PROG_OPT(s, l, d, t, struct test_context, o, v)

#define TEST_OPT_NO(s, l, d, o, v)		TEST_OPT(s, l, d, OPT_NO, o, v)
#define TEST_OPT_INT(s, l, d, o)		TEST_OPT(s, l, d, OPT_INT, o, 0)
#define TEST_OPT_HEX(s, l, d, o)		TEST_OPT(s, l, d, OPT_HEX, o, 0)
#define TEST_OPT_STR(s, l, d, o)		TEST_OPT(s, l, d, OPT_STRING, o, 0)

static struct prog_option test_options[] = {
	TEST_OPT_HEX('a', "hex", "test hex value", hex_value),
	TEST_OPT_INT('d', "int", "test integer value, default " XINTSTR(INT_VALUE_DEFAULT), int_value),
	TEST_OPT_STR('s', "str", "test string value", str_value),
	TEST_OPT_STR('\0', "string", "test long option string without short option", string_value),
	TEST_OPT_NO ('n', "noopt", "test without option", noopt, 1),
	TEST_OPT_NO ('h', "help", "help usage", help, 1),
	PROG_END,
};

#define OPT_LEN		(sizeof(test_options) / sizeof(test_options[0]))

static void usage(char *prog, struct prog_option *opt)
{
	printf("Test of progopt interface, build on %s, %s\n",
			__DATE__, __TIME__);
	printf("Usage: %s [options]\n", prog);
	prog_option_printf(stderr, opt);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	struct option opt[OPT_LEN + 1];
	char optstr[2 * OPT_LEN + 1];
	struct test_context ctx;

	if (prog_option_make(test_options, opt, optstr, OPT_LEN) < 0) {
		fprintf(stderr, "Invalid options");
		exit(EXIT_FAILURE);
	}

	if (prog_option_load(argc, argv, test_options, opt, optstr, &ctx) < 0)
		usage(argv[0], test_options);

	if (ctx.help)
		usage(argv[0], test_options);

	printf("Test progopt\n");
	printf("\tHEX value: %X\n", ctx.hex_value);
	printf("\tINT value: %d\n", ctx.int_value);
	printf("\tSTR value: %s\n", ctx.str_value);
	printf("\tSTRING value: %s\n", ctx.string_value);
	printf("\tNOOPT value: %d\n", ctx.noopt);
	exit(EXIT_SUCCESS);
}
