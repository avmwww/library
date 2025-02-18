/*
 * Example usage of simple cli interface
 *
 * Author Andrey Mitrofanov <avmwww@gmail.com>
 * 2024
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "selector.h"
#include "cli.h"

struct testctx {
	int fd;
	int fdout;
	struct selector *sel;
	struct cli *cli;
};

static void test_exit(int err, void *arg)
{
	struct testctx *ctx = arg;

	printf("\ntest exit\n");

	cli_disconnect(ctx->cli, ctx->fd);
	selector_destroy(ctx->sel);
}

static int exit_cmd(void *ctx, int argc, char **argv, void *priv)
{
	printf("\nbye\n");
	return CLI_EXIT;
}

static int temp_cmd(void *ctx, int argc, char **argv, void *priv)
{
	printf("\nTEMP command: argc %d\n", argc);
	return CLI_OK;
}

static int test_cmd(void *ctx, int argc, char **argv, void *priv)
{
	int i;

	printf("\nTEST command: argc %d\n", argc);
	for (i = 0; i < argc; i++) {
		printf("argv[%d] = %s\n", i, argv[i]);
	}
	return CLI_OK;
}

static const char *test_help = "test help\n\ttest without arguments";
static const char *exit_help = "exit from cli, terminate session";

int main(int argc, char **argv)
{
	struct testctx ctx;

	ctx.fd = fileno(stdin);
	ctx.fdout = fileno(stdout);
	ctx.sel = selector_create();
	ctx.cli = cli_create();

	cli_set_prompt(ctx.cli, "test > ");

	cli_cmd_register(ctx.cli, "test", test_cmd, NULL, test_help);
	cli_cmd_register(ctx.cli, "temp", temp_cmd, NULL, NULL);
	cli_cmd_register(ctx.cli, "exit", exit_cmd, NULL, exit_help);

	cli_connect(ctx.cli, ctx.sel, ctx.fd, ctx.fdout);
	/* enable command completion */
	cli_set_completion(ctx.cli, ctx.fd, 1);

	on_exit(test_exit, &ctx);

	while (selector_exec(ctx.sel) != -1) {
	}

	exit(EXIT_SUCCESS);
}
