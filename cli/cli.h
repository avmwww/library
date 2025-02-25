/*
 * simple cli interface
 *
 * Copyright (C) 2024 by Andrey Mitrofanov <avmwww@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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

int cli_send_redraw(struct cli *cli, int fd);

int cli_send_telnet_negotiation(struct cli *cli, int fd);

#define CLI_EXIT		(-10)
#define CLI_ERROR		(-1)
#define CLI_OK			0
#define CLI_PROCESSED		1
#define CLI_INVALID_ARG		9
#define CLI_UNKNOWN_CMD		10

#endif
