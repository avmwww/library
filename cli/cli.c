/*
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/queue.h>

#include "cli.h"
#include "selector.h"

#define DEBUG
#define DEBUG_PREFIX	"CLI: "
#include "debug.h"

#define CLI_NEW_LINE		"\r\n"
#define CLI_BUF_SIZE	65536

/* Telnet negotiation */
#define TELNET_IAC		0xff
/* The sender suggests it would like to use an available option. */
#define TELNET_WILL		0xfb
/* The sender informs the recipient it will not use that option. */
#define TELNET_WON_T		0xfc
/* The sender instructs the recipient to use an available option. */
#define TELNET_DO		0xfd
/* The sender instructs the recipient not to use that option. */
#define TELNET_DON_T		0xfe

#define TELNET_ECHO		1
#define TELNET_SGA		3

#define CTRL(c)			((c) - '@')

struct cli_historyq {
	TAILQ_ENTRY(cli_historyq) queue;
	char *str;
};

struct cli_cmdq {
	TAILQ_ENTRY(cli_cmdq) queue;
	char *name;
	char *help;
	int (*handler)(void *, int, char **, void *priv);
	void *context;
};

struct cliq {
	TAILQ_ENTRY(cliq) queue;
	struct cli *cli;
	struct selector *sel;
	struct {
		void (*callback)(int, int, void *);
		void *arg;
	} connection;
	selector_buf_t rd;
	selector_buf_t wr;
	int fd;
	int fdout;
	char esc;
	bool echo;
	bool suppress_go_ahead;
	struct {
		int count;
		uint8_t buf[8];
	} telnet;
	struct {
		TAILQ_HEAD(cli_history, cli_historyq) queue;
		struct cli_historyq *current;
	} history;
	struct cmd {
		char *str;
		int size;
		int limit;
		int cursor;
		int pos;
		int argc;
		char **argv;
	} cmd;
	struct completion {
		int enable;
		int count;
	} comp;
};

/* define struct cli */
struct cli {
	TAILQ_HEAD(, cliq);
	TAILQ_HEAD(cli_cmd, cli_cmdq) cmdq;
	char *prompt;
};

/* history */
static void cli_history_remove(struct cliq *cq, struct cli_historyq *chq)
{
	if (cq->history.current == chq) {
		/* remove current history command */
		struct cli_historyq *ch = TAILQ_NEXT(cq->history.current, queue);
		if (!ch)
			ch = TAILQ_PREV(cq->history.current, cli_history, queue);
		cq->history.current = ch;
	}

	TAILQ_REMOVE(&cq->history.queue, chq, queue);
	if (chq->str)
		free(chq->str);

	free(chq);
}

static int cli_history_add(struct cliq *cq, char *str)
{
	struct cli_historyq *chq = malloc(sizeof(struct cli_historyq));

	if (!chq)
		return -1;

	memset(chq, 0, sizeof(struct cli_historyq));
	
	chq->str = strdup(str);
	if (!chq->str) {
		free(chq);
		return -1;
	}

	/* history in lifo order */
	TAILQ_INSERT_HEAD(&cq->history.queue, chq, queue);
	cq->history.current = chq;

	return 0;
}

static int cli_history_next(struct cliq *cq)
{
	struct cli_historyq *chq;

	if (!cq->history.current)
		chq = TAILQ_FIRST(&cq->history.queue);
	else
		chq = TAILQ_PREV(cq->history.current, cli_history, queue);

	if (chq)
		cq->history.current = chq;

	return 0;
}

static int cli_history_prev(struct cliq *cq)
{
	struct cli_historyq *chq;

	if (!cq->history.current)
		chq = TAILQ_FIRST(&cq->history.queue);
	else
		chq = TAILQ_NEXT(cq->history.current, queue);

	if (chq)
		cq->history.current = chq;

	return 0;
}

/* supported commands */
static void cli_cmd_remove(struct cli *cli, struct cli_cmdq *cmdq)
{
	if (!cmdq)
		return;

	TAILQ_REMOVE(&cli->cmdq, cmdq, queue);
	if (cmdq->name)
		free(cmdq->name);
	if (cmdq->help)
		free(cmdq->help);

	free(cmdq);
}

static int cli_cmd_add(struct cli *cli, const char *name,
		int (*handler)(void *, int, char **, void *), void *context,
		const char *help)
{
	struct cli_cmdq *cmdq = malloc(sizeof(struct cli_cmdq));

	if (!cmdq)
		return -1;

	memset(cmdq, 0, sizeof(struct cli_cmdq));
	cmdq->name = strdup(name);

	if (!cmdq->name) {
		free(cmdq);
		return -1;
	}

	if (help) {
		cmdq->help = strdup(help);
		if (!cmdq->help) {
			free(cmdq->name);
			free(cmdq);
			return -1;
		}
	}

	cmdq->context = context;
	cmdq->handler = handler;

	TAILQ_INSERT_TAIL(&cli->cmdq, cmdq, queue);

	return 0;
}

/* search supported command by \@name of first chars \@size
 * if \@size < 0 then whole command searched
 * \@num  number of command
 */
static struct cli_cmdq *cli_cmd_search(struct cli *cli, const char *name, int size, int num)
{
	struct cli_cmdq *cmdq;

	TAILQ_FOREACH(cmdq, &cli->cmdq, queue) {
		if (size < 0) {
			if (!strcmp(cmdq->name, name)) {
				if (!num)
					return cmdq;
				num--;
			}
		} else {
			if (!strncmp(cmdq->name, name, size)) {
				if (!num)
					return cmdq;
				num--;
			}
		}
	}
	return NULL;
}

static struct cli_cmdq *cli_cmd_search_num(struct cli *cli, const char *name,
					   int size, int *count)
{
	struct cli_cmdq *cmdq, *c = NULL;

	if (size < 0)
		size = strlen(name);

	*count = 0;
	TAILQ_FOREACH(cmdq, &cli->cmdq, queue) {
		if (!strncmp(cmdq->name, name, size)) {
			*count = *count + 1;
			if (!c)
				c = cmdq;
		}
	}
	return c;
}

/*
 *
 */
static struct cliq *cli_search(struct cli *cli, int fd)
{
	struct cliq *cq;

	if (!cli)
		return NULL;

	TAILQ_FOREACH(cq, cli, queue) {
		if (cq->fd == fd)
			return cq;
	}
	return NULL;
}

static void cli_remove(struct cli *cli, struct cliq *cq)
{
	struct cli_historyq *chq;

	TAILQ_REMOVE(cli, cq, queue);

	/* history queue */
	while (!TAILQ_EMPTY(&cq->history.queue)) {
		chq = TAILQ_FIRST(&cq->history.queue);
		cli_history_remove(cq, chq);
	}

	selector_remove(cq->sel, cq->fd);

	if (cq->rd.buf)
		free(cq->rd.buf);
	if (cq->cmd.str)
		free(cq->cmd.str);

	free(cq);
}

static int cli_write(struct cliq *cq, const void *s, size_t size)
{
	return selector_write(cq->sel, cq->fdout, s, size);
}

static int cli_put_new_line(struct cliq *cq)
{
	return cli_write(cq, CLI_NEW_LINE, 2);
}

static int cli_putc(struct cliq *cq, char c)
{
	return cli_write(cq, &c, 1);
}

static int cli_puts(struct cliq *cq, const char *s)
{
	return cli_write(cq, s, strlen(s));
}

static int cli_printf(struct cliq *cli, const char *fmt, ...)
{
	char *str;
	int n;
	va_list ap;

	va_start(ap, fmt);
	n = vasprintf(&str, fmt, ap);
	va_end(ap);
	if (n < 0)
		return n;

	n = selector_write(cli->sel, cli->fdout, str, n);
	free(str);
	return n;
}

/* Handle ANSI arrows */
static char cli_ansi_arrows_handler(struct cliq *cq, char c)
{
	/* Remap to readline control codes */
	switch (c) {
		case 'A':  /* Up */
			c = CTRL('P');
			break;

		case 'B':  /* Down */
			c = CTRL('N');
			break;

		case 'C':  /* Right */
			c = CTRL('F');
			break;

		case 'D':  /* Left */
			c = CTRL('B');
			break;

		default:
			c = 0;
	}

	return c;
}

static int cli_set_pos(struct cliq *cq, int pos)
{
	int i;

	if (pos > cq->cmd.size)
		pos = cq->cmd.size;
	else if (pos < 0)
		pos = 0;

	if (pos > cq->cmd.pos) {
		/* forward */
		for (i = cq->cmd.pos; i < pos; i++)
			cli_putc(cq, cq->cmd.str[i]);
	} else if (pos < cq->cmd.pos) {
		/* backward */
		for (i = cq->cmd.pos; i > pos; i--)
			cli_putc(cq, '\b');
	}
	cq->cmd.pos = pos;
	return pos;
}

static int cli_set_cursor(struct cliq *cq, int pos)
{
	cq->cmd.cursor = cli_set_pos(cq, pos);

	return cq->cmd.cursor;
}

static int cli_cmd_fill(struct cliq *cq, char fill, int start)
{
	int i;
	int pos = cq->cmd.pos;

	if (start > cq->cmd.size)
		return 0;

	cli_set_pos(cq, start);

	for (i = start; i < cq->cmd.size; i++) {
		if (cli_putc(cq, fill) < 0)
			return -1;

		cq->cmd.pos++;
	}
	/* set old position */
	cli_set_pos(cq, pos);
	return 0;
}

static int cli_cmd_print(struct cliq *cq)
{
	cq->cmd.str[cq->cmd.size] = '\0';

	cli_set_cursor(cq, 0);

	if (cli_puts(cq, cq->cmd.str) < 0)
		return -1;

	cq->cmd.cursor = cq->cmd.size;
	cq->cmd.pos = cq->cmd.size;
	return 0;
}

/* clear command string from position \@pos to end of line */
static int cli_cmd_clear(struct cliq *cq, int pos)
{
	if (pos > cq->cmd.size)
		return 0;

	memset(&cq->cmd.str[pos], ' ', cq->cmd.size - pos);

	if (cli_cmd_print(cq) < 0)
		return -1;

	cli_set_cursor(cq, pos);

	cq->cmd.size = pos;
	return 0;
}

/* put char to command buffer and print */
static int cli_cmd_put(struct cliq *cq, char c)
{
	if (cq->cmd.size >= (cq->cmd.limit - 1))
		return 0;

	cq->comp.count = 0;
	/* cleanup command line from current position */
	cli_cmd_fill(cq, ' ', cq->cmd.pos);

	/* move string from cursor to after cursor position in cmd buffer */
	memmove(&cq->cmd.str[cq->cmd.cursor + 1], &cq->cmd.str[cq->cmd.cursor],
			cq->cmd.size - cq->cmd.cursor);

	/* put char to command buffer */
	cq->cmd.str[cq->cmd.cursor] = c;
	cq->cmd.size++;
	cq->cmd.str[cq->cmd.size] = '\0';

	/* print from cursor */
	cli_puts(cq, &cq->cmd.str[cq->cmd.cursor++]);
	cq->cmd.pos = cq->cmd.size;

	cli_set_pos(cq, cq->cmd.cursor);
	return 0;
}

static int cli_cmd_puts(struct cliq *cq, const char *s)
{
	while (*s != '\0')
		cli_cmd_put(cq, *s++);
	return 0;
}

static int cli_cmd_from_history(struct cliq *cq)
{
	struct cli_historyq *chq = cq->history.current;

	if (!chq)
		return 0;

	/* clear last command */
	if (cli_cmd_clear(cq, 0) < 0)
		return -1;

	cq->cmd.size = strlen(chq->str);
	memcpy(cq->cmd.str, chq->str, cq->cmd.size + 1);
	return cli_cmd_print(cq);
}

/* split string into words */
static int cli_argv_alloc(char *str, char ***argv)
{
	char *p = str;
	int i = 0;
	char *save;
	char *arg;
	char **a = NULL;

	while ((arg = strtok_r(p, " ", &save)) != NULL) {
		p = NULL;

		/* allocate storage by 8 pointers  */
		if ((i & 7) == 0) {
			int size = sizeof(char *) * (((i >> 3) + 1) * 8);
			void *pa = realloc(a, size);

			if (!pa) {
				if (a)
					free(a);
				return 0;
			}
			a = pa;
		}

		a[i++] = arg;
	}
	*argv = a;

	return i;
}

static void cli_argv_free(int argc, char **argv)
{
	if (argv)
		free(argv);
}

#ifdef DEBUG
static void dbg_cli_argv(int argc, char **argv)
{
	int i;
	for (i = 0; i < argc; i++) {
		dbg("param %d = %s", i, argv[i]);
	}
}
#else
# define dbg_cli_argv(argc, argv)
#endif

static int cli_prompt_print(struct cliq *cq)
{
	/* clear command line */
	cq->history.current = NULL;
	cq->cmd.cursor = 0;
	cq->cmd.pos = 0;
	cq->cmd.size = 0;
	cq->cmd.str[0] = '\0';
	/* print prompt */
	return cli_printf(cq, cq->cli->prompt);
}

static int cli_cmd_usage(struct cliq *cq, struct cli_cmdq *cmdq)
{
	if (cmdq->help) {
		cli_puts(cq, cmdq->help);
		cli_put_new_line(cq);
	}
	return 0;
}

/* internal supported commands: help, history */
static int cli_cmd_history(void *ctx, int argc, char **argv, void *priv)
{
	struct cliq *cq = priv;
	struct cli_historyq *chq;
	int i = 0;

	TAILQ_FOREACH(chq, &cq->history.queue, queue) {
		cli_printf(cq, "%5d  %s" CLI_NEW_LINE, i++, chq->str);
	}
	return 0;
}

static int cli_cmd_help(void *ctx, int argc, char **argv, void *priv)
{
	struct cliq *cq = priv;
	struct cli *cli = cq->cli;
	struct cli_cmdq *cmdq;

	TAILQ_FOREACH(cmdq, &cli->cmdq, queue) {
		cli_printf(cq, "%s: %s" CLI_NEW_LINE,
				cmdq->name, cmdq->help ? cmdq->help : "");
	}
	return 0;
}

/* execute command */
static int cli_cmd_execute(struct cliq *cq)
{
	struct cli_cmdq *cmdq;
	int ret = CLI_UNKNOWN_CMD;

	cli_put_new_line(cq);

	cq->cmd.str[cq->cmd.size] = '\0';

	if (cq->cmd.size == 0)
		return 0;

	/* add whole string to history */
	if (cli_history_add(cq, cq->cmd.str) < 0)
		return -1;

	cq->cmd.argc = cli_argv_alloc(cq->cmd.str, &cq->cmd.argv);

	dbg("Execute command %s", cq->cmd.argv[0]);
	dbg_cli_argv(cq->cmd.argc, cq->cmd.argv);

	cmdq = cli_cmd_search(cq->cli, cq->cmd.argv[0], -1, 0);
	if (cmdq && cmdq->handler) {
		if (cq->cmd.argv[1] && !strcmp(cq->cmd.argv[1], "help"))
			ret = cli_cmd_usage(cq, cmdq);
		else
			ret = cmdq->handler(cmdq->context, cq->cmd.argc,
					    cq->cmd.argv, cq);
	}

	if (ret == CLI_UNKNOWN_CMD)
		cli_printf(cq, "unknown command %s" CLI_NEW_LINE, cq->cmd.argv[0]);
	else if (ret == CLI_INVALID_ARG)
		cli_printf(cq, "invalid arguments" CLI_NEW_LINE);

	cli_argv_free(cq->cmd.argc, cq->cmd.argv);
	return ret;
}

static int cli_telnet_negotiate(struct cliq *cq)
{
	const uint8_t negotiate[] = {
		TELNET_IAC,
		TELNET_WILL,
		TELNET_SGA, /* Suppress Go Ahead */
		TELNET_IAC,
		TELNET_WILL,
		TELNET_ECHO,
	};
	return cli_write(cq, negotiate, sizeof(negotiate));
}

/*
 *  Control character handlers
 */
static int cli_beep(struct cliq *cq)
{
	return cli_putc(cq, '\a');
}

static int cli_redraw(struct cliq *cq)
{
	if (cli_put_new_line(cq) < 0)
		return -1;

	if (cli_printf(cq, cq->cli->prompt) < 0)
		return -1;

	if (cli_printf(cq, cq->cmd.str) < 0)
		return -1;

	if (cli_set_cursor(cq, cq->cmd.cursor) < 0)
		return -1;

	return 0;
}

static int cli_end_of_line(struct cliq *cq)
{
	return cli_set_cursor(cq, cq->cmd.size);
}

static int cli_start_of_line(struct cliq *cq)
{
	return cli_set_cursor(cq, 0);
}

static int cli_clear_line(struct cliq *cq)
{
	return cli_cmd_clear(cq, 0);
}

static int cli_terminate(struct cliq *cq)
{
	/* session terminate */
	return -1;
}

static int cli_cursor_up(struct cliq *cq)
{
	/* history backward */
	if (cli_history_prev(cq) < 0)
		return -1;

	return cli_cmd_from_history(cq);
}

static int cli_cursor_down(struct cliq *cq)
{
	/* history forward */
	if (cli_history_next(cq) < 0)
		return -1;

	return cli_cmd_from_history(cq);
}

static int cli_cursor_right(struct cliq *cq)
{
	return cli_set_cursor(cq, cq->cmd.cursor + 1);
}

static int cli_cursor_left(struct cliq *cq)
{
	return cli_set_cursor(cq, cq->cmd.cursor - 1);
}

/* delete char on prev to cursor position */
static int cli_delete_char(struct cliq *cq)
{
	if (cq->cmd.cursor == 0 || cq->cmd.size == 0)
		return 0;

	/* cleanup command line from prev position */
	cli_cmd_fill(cq, ' ', cq->cmd.pos - 1);

	/* move string from cursor to prev cursor position in cmd buffer */
	memmove(&cq->cmd.str[cq->cmd.cursor - 1],
			&cq->cmd.str[cq->cmd.cursor],
			cq->cmd.size - cq->cmd.cursor);

	cq->cmd.size--;
	cq->cmd.str[cq->cmd.size] = '\0';

	/* print from prev cursor */
	cq->cmd.cursor--;
	cli_set_pos(cq, cq->cmd.cursor);
	cli_puts(cq, &cq->cmd.str[cq->cmd.cursor]);
	cq->cmd.pos = cq->cmd.size;

	cli_set_pos(cq, cq->cmd.cursor);
	return 0;
}

static int cli_delete_word(struct cliq *cq)
{
	char c;

	/* delete spaces */
	while (cq->cmd.cursor && (c = cq->cmd.str[cq->cmd.cursor - 1]) == ' ') {
		cli_delete_char(cq);
	}

	/* delete word */
	while (cq->cmd.cursor && (c = cq->cmd.str[cq->cmd.cursor - 1]) != ' ') {
		cli_delete_char(cq);
	}
	return 0;
}

static int cli_kill_to_eol(struct cliq *cq)
{
	return cli_cmd_clear(cq, cq->cmd.cursor);
}

static int cli_disable(struct cliq *cq)
{
	// TODO
	return 0;
}

static int cli_completion(struct cliq *cq)
{
	struct cli_cmdq *cmdq;
	char *cmd, *p;
	int num;
	int i, cmd_len;

	cq->comp.count++;

	cmd = cq->cmd.str;
	while (*cmd == ' ')
		cmd++;

	p = strchr(cmd, ' ');
	if (p)
		cmd_len = (int)(p - cq->cmd.str);
	else
		cmd_len = strlen(cq->cmd.str);

	/* check cursor on command */
	if (cq->cmd.cursor > cmd_len)
		return 0;

	cmd_len = cq->cmd.cursor;

	/* search number of commands is equal to string */
	cmdq = cli_cmd_search_num(cq->cli, cmd, cmd_len, &num);

	if (cq->comp.count < 2 && !cmd_len)
		return 0;

	/* double tab process */
	if (cq->comp.count > 1) {
		cli_put_new_line(cq);

		cq->comp.count = 1;
		while (num) {
			cmdq = cli_cmd_search(cq->cli, cmd, cmd_len, num - 1);
			cli_puts(cq, cmdq->name);
			cli_putc(cq, ' ');
			num--;
		}
		cli_put_new_line(cq);
		cli_printf(cq, cq->cli->prompt);
		cli_puts(cq, cq->cmd.str);
		cli_set_pos(cq, cq->cmd.cursor);
		return 0;
	}

	if (!num)
		return 0;

	if (num > 1) {
		/* more then 1 command found */
		i = cmd_len;
		while (num > 1) {
			cmd_len++;
			cmdq = cli_cmd_search_num(cq->cli, cmdq->name, cmd_len, &num);
			if (num > 1)
				cli_cmd_put(cq, cmdq->name[i++]);
		}
		return 0;
	}

	/* complete command */
	cli_cmd_puts(cq, &cmdq->name[cmd_len]);
	cli_cmd_put(cq, ' ');
	return 0;
}

/*
 *
 */
struct cli_ctrl {
	char ch;
	int (*handler)(struct cliq *);
};

static const struct cli_ctrl cli_ctrl[] = {
	{CTRL('A'), cli_start_of_line},
	{CTRL('B'), cli_cursor_left  },
	{CTRL('C'), cli_beep         },
	{CTRL('D'), cli_terminate    },
	{CTRL('E'), cli_end_of_line  },
	{CTRL('F'), cli_cursor_right },
	{CTRL('H'), cli_delete_char  },
	{CTRL('I'), cli_completion   },
	{CTRL('K'), cli_kill_to_eol  },
	{CTRL('L'), cli_redraw       },
	{CTRL('N'), cli_cursor_down  },
	{CTRL('P'), cli_cursor_up    },
	{CTRL('U'), cli_clear_line   },
	{CTRL('W'), cli_delete_word  },
	{CTRL('Z'), cli_disable      },
	{     0x7f, cli_delete_char  },
	{0, NULL},
};

static int cli_process_ctrl(struct cliq *cli, char c)
{
	const struct cli_ctrl *ctrl = cli_ctrl;

	while (ctrl->ch) {
		if (ctrl->ch == c) {
			if (ctrl->handler(cli) < 0)
				return -1;

			/* handled */
			return 1;
		}
		ctrl++;
	}
	/* not handled */
	return 0;
}

static int cli_telnet_negotiation(struct cliq *cq, char c)
{
	uint8_t cmd, op;
	uint8_t buf[3];

	cq->telnet.buf[cq->telnet.count++] = c;
	if (cq->telnet.count < 3) {
		return 0;
	}

	cmd = cq->telnet.buf[1];
	op = cq->telnet.buf[2];

	if (cmd == TELNET_WILL) {
		buf[0] = TELNET_IAC;
		buf[1] = TELNET_DO;
		buf[3] = op;
		cli_write(cq, buf, 3);
	} else {
		if (op == TELNET_ECHO)
			cq->echo = (cmd == TELNET_DO) ? true : false;
		else if (op == TELNET_SGA)
			cq->suppress_go_ahead = (cmd == TELNET_DO) ? true : false;
	}

	cq->telnet.count = 0;
	return 1;
}

static int cli_process(struct cliq *cq, uint8_t c)
{
	int ret;

	/*  */
	//printf("C= %02x\n", (uint8_t)c);

	if (cq->telnet.count) {
		/* telnet negotiation */
		return cli_telnet_negotiation(cq, c);
	}

	if (cq->esc == '[') {
		c = cli_ansi_arrows_handler(cq, c);
		cq->esc = 0;
	}

	switch (c) {
		case 27:
		case '[':
			cq->esc = c;
			return 0;
		case TELNET_IAC:
			cq->telnet.count = 1;
			return 0;
		case '\r':
			return 0;
		case 0:
		case '\n':
			if (cli_cmd_execute(cq) < 0)
				return -1;

			return cli_prompt_print(cq);
		default:
			break;
	}

	if ((ret = cli_process_ctrl(cq, c)) < 0)
		return -1;

	if (ret)
		return 0;

	/* clear completion */
	cq->comp.count = 0;
	if (cli_cmd_put(cq, c) < 0)
		return -1;

	return 0;
}

/*
 *
 */
static int cli_read_cb(struct cliq *cq, int size)
{
	char *p = cq->rd.buf;
	int ret = -1;

	while (size > 0) {
		if ((ret = cli_process(cq, *p++)) < 0)
			break;
		size--;
	}

	if (ret < 0) {
		dbg("session terminated");
		if (cq->connection.callback)
			cq->connection.callback(cq->fd, -1, cq->connection.arg);
	}

	return ret;
}

static int cli_write_cb(struct cliq *cq, int size)
{
	return 0;
}

static int cli_timeout_cb(struct cliq *cq, int size)
{
	return 0;
}

static int cli_callback(int ev, int size, void *arg)
{
	struct cliq *cq = arg;
	int err = 0;

	if (ev == SELECTOR_READ) {
		err = cli_read_cb(cq, size);
	} else if (ev == SELECTOR_WRITE) {
		err = cli_write_cb(cq, size);
	} else if (ev == SELECTOR_TIMEOUT) {
		return cli_timeout_cb(cq, size);
	}

	return err;
}

static int cli_start(struct cliq *cq)
{
	cq->rd.buf = malloc(cq->rd.buf_size);
	if (cq->rd.buf == NULL)
		return -1;

	if (selector_add(cq->sel, cq->fd, cli_callback, cq) < 0)
		return -1;

	if (cq->fdout != cq->fd) {
		if (selector_add(cq->sel, cq->fdout, cli_callback, cq) < 0)
			return -1;
	}

	cq->cmd.str = malloc(cq->cmd.limit);

	if (selector_read(cq->sel, cq->fd, cq->rd.buf, cq->rd.buf_size) < 0)
		return -1;

	return 0;
}

int cli_set_callback(struct cli *cli, int fd,
		void (*callback)(int, int, void *), void *arg)
{
	struct cliq *cq;

	cq = cli_search(cli, fd);
	if (!cq)
		return -1;

	cq->connection.callback = callback;
	cq->connection.arg = arg;
	return 0;
}

int cli_set_prompt(struct cli *cli, const char *prompt)
{
	if (cli->prompt)
		free(cli->prompt);

	cli->prompt = strdup(prompt);
	if (cli->prompt == NULL)
		return -1;

	return 0;
}

int cli_send(struct cli *cli, int fd, const void *s, size_t size)
{
	struct cliq *cq;

	cq = cli_search(cli, fd);
	if (!cq)
		return -1;

	return cli_write(cq, s, size);
}

int cli_set_completion(struct cli *cli, int fd, int enable)
{
	struct cliq *cq;

	cq = cli_search(cli, fd);
	if (!cq)
		return -1;

	cq->comp.enable = enable;
	cq->comp.count = 0;
	return 0;
}

int cli_cmd_register(struct cli *cli, const char *name,
		int (*handler)(void *, int, char **, void *), void *context,
		const char *help)
{
	return cli_cmd_add(cli, name, handler, context, help);
}

int cli_connect(struct cli *cli, struct selector *sel, int fd, int fdout)
{
	struct cliq *cq;

	if (!cli)
		return -1;

	cq = malloc(sizeof(struct cliq));
	if (!cq)
		return -1;

	memset(cq, 0, sizeof(struct cliq));
	cq->fd = fd;
	cq->fdout = fdout;
	cq->sel = sel;
	cq->rd.buf_size = CLI_BUF_SIZE;
	cq->cmd.limit = CLI_BUF_SIZE;
	cq->cli = cli;

	TAILQ_INSERT_TAIL(cli, cq, queue);

	TAILQ_INIT(&cq->history.queue);

	if (cq->fdout < 0) /* if not stdout */
		cq->fdout = fd;

	if (cli_start(cq) < 0) {
		cli_remove(cli, cq);
		return -1;
	}

	selector_set_raw(cq->sel, fd, 1);

	cli_telnet_negotiate(cq);

	cli_redraw(cq);

	return 0;
}

int cli_disconnect(struct cli *cli, int fd)
{
	struct cliq *cq;

	cq = cli_search(cli, fd);
	if (!cq)
		return -1;

	selector_set_raw(cq->sel, fd, 0);

	cli_remove(cli, cq);

	return 0;
}

/*
 *  destroy cli
 */
void cli_destroy(struct cli *cli)
{
	struct cliq *cq;
	struct cli_cmdq *cmdq;

	if (!cli)
		return;

	/* free supported commands */
	while (!TAILQ_EMPTY(&cli->cmdq)) {
		cmdq = TAILQ_FIRST(&cli->cmdq);
		cli_cmd_remove(cli, cmdq);
	}

	/* free cli queue */
	while (!TAILQ_EMPTY(cli)) {
		cq = TAILQ_FIRST(cli);
		cli_remove(cli, cq);
	}

	if (cli->prompt)
		free(cli->prompt);

	free(cli);
}

/*
 * create cli
 */
struct cli *cli_create(void)
{
	struct cli *cli;

	cli = malloc(sizeof(struct cli));
	memset(cli, 0, sizeof(struct cli));
	TAILQ_INIT(cli);
	TAILQ_INIT(&cli->cmdq);

	cli_cmd_add(cli, "help", cli_cmd_help, cli, NULL);
	cli_cmd_add(cli, "history", cli_cmd_history, cli, "command line history");
	return cli;
}

