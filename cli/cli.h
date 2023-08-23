/*
 *
 */

#ifndef _CLI_H_
#define _CLI_H_

#include "selector.h"

struct cli;

struct cli *cli_create(void);

void cli_destroy(struct cli *cli);

int cli_disconnect(struct cli *cli, int fd);

int cli_connect(struct cli *cli, struct selector *sel, int fd, int fdout);

int cli_set_callback(struct cli *cli, int fd, void (*callback)(int, int, void *), void *arg);

int cli_set_prompt(struct cli *cli, const char *prompt);

int cli_set_completion(struct cli *cli, int fd, int enable);

int cli_cmd_register(struct cli *cli, const char *name,
		     int (*handler)(void *, int, char **, void *priv), void *context,
		     const char *help);

int cli_send(struct cli *cli, int fd, const void *s, size_t size);

#define CLI_EXIT		(-10)
#define CLI_ERROR		(-1)
#define CLI_OK			0
#define CLI_PROCESSED		1
#define CLI_INVALID_ARG		9
#define CLI_UNKNOWN_CMD		10

#endif
