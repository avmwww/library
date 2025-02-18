/*
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <fcntl.h>

#include "selector.h"

struct selector_elm {
	TAILQ_ENTRY(selector_elm) queue;
	int fd;
	size_t read;
	struct timeval time;
	struct termios tty;
	long timeout;
	int (*callback)(int, int, void *);
	int (*rd_cb)(int, void *, int, void *);
	void *context;
	selector_buf_t rd, wr;
};

/* define struct selector */
TAILQ_HEAD(selector, selector_elm);

static int selector_buf_resize(selector_buf_t *sb, size_t size)
{
	uint8_t *buf;

	if (size > sb->limit) /* buffer limit reached */
		return -1;

	buf = realloc(sb->buf, size);
	if (!buf)
		return -1;

	sb->buf = buf;
	sb->buf_size = size;
	return 0;
}

static int set_descriptor_block(int fd, int block)
{
	int flags;

	if (fd < 0)
		return -1;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;

	if (block)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;

	return fcntl(fd, F_SETFL, flags);
}

static struct selector_elm *selector_search(struct selector *sel, int fd)
{
	struct selector_elm *elm;

	TAILQ_FOREACH(elm, sel, queue) {
		if (elm->fd == fd)
			return elm;
	}
	return NULL;
}

static int selector_write_cb(struct selector_elm *elm)
{
	int n = elm->wr.size - elm->wr.offt;

	if (n == 0) {
		/* Write complete */
		elm->wr.offt = elm->wr.size = 0;
		return 0;
	}

	n = write(elm->fd, elm->wr.buf + elm->wr.offt, n);
	if (n <= 0) {
		elm->wr.offt = elm->wr.size = 0;
		return -1;
	}

	elm->wr.offt += n;
	elm->wr.bytes += n;

	return elm->wr.offt;
}

static int selector_read_cb(struct selector_elm *elm)
{
	int n;
	uint8_t *buf;

	if (!elm->rd.buf)
		return -1;

	n = elm->rd.size - elm->rd.offt;
	buf = elm->rd.buf + elm->rd.offt;

	if (n == 0)
		return elm->rd.offt;

	if (elm->rd_cb)
		n = elm->rd_cb(elm->fd, buf, n, elm->context);
	else
		n = read(elm->fd, buf, n);

	if (n <= 0) /* Close connection */
		return n;

	elm->rd.bytes += n;
	elm->rd.offt = 0;

	return n;
}

static void selector_elm_remove(struct selector *sel, struct selector_elm *elm)
{
	TAILQ_REMOVE(sel, elm, queue);
	if (elm->wr.buf)
		free(elm->wr.buf);
	free(elm);
}

/*
 * set file descriptor to raw tty mode
 * if \@raw = 0, then restore old settings
 */
int selector_set_raw(struct selector *sel, int fd, int raw)
{
	struct termios tty;
	struct selector_elm *elm = selector_search(sel, fd);

	if (!elm)
		return -1;

	if (raw) {
		/* set the terminal to raw mode */
		tcgetattr(fd, &tty);
		/* store orig settings */
		elm->tty = tty;
#if 0
		/* input modes - clear indicated ones giving: no break, no CR to NL, 
		   no parity check, no strip char, no start/stop output (sic) control */
		tty.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

		/* output modes - clear giving: no post processing such as NL to CR+NL */
		tty.c_oflag &= ~(OPOST);

		/* control modes - set 8 bit chars */
		tty.c_cflag |= (CS8);

		/* local modes - clear giving: echoing off, canonical off (no erase with 
		   backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
		tty.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

		/* control chars - set return condition: min number of bytes and timer */
		tty.c_cc[VMIN] = 5; tty.c_cc[VTIME] = 8; /* after 5 bytes or .8 seconds
							    after first byte seen      */
		tty.c_cc[VMIN] = 0; tty.c_cc[VTIME] = 0; /* immediate - anything       */
		tty.c_cc[VMIN] = 2; tty.c_cc[VTIME] = 0; /* after two bytes, no timer  */
		tty.c_cc[VMIN] = 0; tty.c_cc[VTIME] = 8; /* after a byte or .8 seconds */
#else
		/* local modes - clear giving: echoing off, canonical off (no erase with 
		   backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
		tty.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
#endif
	} else {
		/* restore old settings */
		tty = elm->tty;
	}

	return tcsetattr(fd, TCSANOW, &tty);
}

/*
 *  write buffer \@buf data by \@size len to internal structure for write callback
 */
int selector_write(struct selector *sel, int fd, const void *buf, size_t size)
{
	struct selector_elm *elm = selector_search(sel, fd);
	size_t nsize;

	if (!elm)
		return -1;

	if (size == 0)
		return 0;

	nsize = size + elm->wr.size;

	if (nsize > elm->wr.buf_size) {
		/* Write buffer resize */
		if (selector_buf_resize(&elm->wr, nsize) < 0)
			return -1;
	}

	memcpy(elm->wr.buf + elm->wr.size, buf, size);

	elm->wr.size += size;

	return size;
}

int selector_write_arm(struct selector *sel, int fd)
{
	struct selector_elm *elm = selector_search(sel, fd);

	if (!elm)
		return -1;

	elm->wr.size = elm->wr.offt = 1;
	return 0;
}

/*
 *  set file descriptor buffer pointer and size for read callback
 */
int selector_read(struct selector *sel, int fd, void *buf, size_t size)
{
	struct selector_elm *elm = selector_search(sel, fd);

	if (!elm)
		return -1;

	elm->rd.buf = buf;
	elm->rd.size = size;
	elm->rd.offt = 0;
	return size;
}

int selector_set_read_cb(struct selector *sel, int fd, int (*cb)(int, void *, int, void *))
{
	struct selector_elm *elm = selector_search(sel, fd);

	if (!elm)
		return -1;

	elm->rd_cb = cb;
	return 0;
}

/*
 * set file descriptor wait timeout
 */
int selector_wait(struct selector *sel, int fd, long timeout)
{
	struct selector_elm *elm = selector_search(sel, fd);

	if (!elm)
		return -1;

	elm->timeout = timeout;
	gettimeofday(&elm->time, NULL);
	return 0;
}

/*
 * add file descriptor to selector
 */
int selector_add(struct selector *sel, int fd,
		 int (*callback)(int, int, void *), void *context)
{
	struct selector_elm *elm = selector_search(sel, fd);

	if (elm) /* already in queue */
		return -1;

	elm = malloc(sizeof(struct selector_elm));
	if (!elm)
		return -1;

	memset(elm, 0, sizeof(struct selector_elm));

	/* Non blocking io */
	set_descriptor_block(fd, 0);

	elm->fd = fd;
	elm->callback = callback;
	elm->context = context;
	elm->rd.limit = elm->wr.limit = SELECTOR_BUFFER_LIMIT;

	TAILQ_INSERT_TAIL(sel, elm, queue);

	return 0;
}

/*
 * remove file descriptor from selector
 */
int selector_remove(struct selector *sel, int fd)
{
	struct selector_elm *elm = selector_search(sel, fd);

	if (!elm)
		return -1;

	set_descriptor_block(elm->fd, 1);
	selector_elm_remove(sel, elm);
	return 0;
}

/*
 *  stv, current time
 *  tv, set timeout timer
 */
static int selector_timeout(struct selector_elm *elm,
			    struct timeval *stv, struct timeval *tv)
{
	struct timeval arm, prd;
	int err = 0;

	prd.tv_sec = elm->timeout / 1000000UL;
	prd.tv_usec = elm->timeout % 1000000UL;
	/* Add to last armed time timer period */
	timeradd(&elm->time, &prd, &arm);

	if (timercmp(&arm, stv, >)) {
		/* Timeout not reached  */
		timersub(&arm, stv, &arm);
	} else {
		/* Timeout callback */
		err = elm->callback(SELECTOR_TIMEOUT, 0, elm->context);
		elm->time = *stv;
		arm = prd;
	}

	if (!timerisset(tv))
		*tv = arm;

	/* Set minimum time value */
	if (timercmp(&arm, tv, <))
		*tv = arm;

	return err;
}

/*
 * execute selector for one time
 */
int selector_exec(struct selector *sel)
{
	fd_set rfds, wfds;
	struct timeval tv, stv;
	int fd_max = -1;
	int err;
	struct selector_elm *elm;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

	timerclear(&tv);
	gettimeofday(&stv, NULL);

	TAILQ_FOREACH(elm, sel, queue) {
		if (elm->callback == NULL)
			continue;

		if (elm->fd > fd_max)
			fd_max = elm->fd;

		if (elm->rd.buf)
			FD_SET(elm->fd, &rfds);
		if (elm->wr.size)
			FD_SET(elm->fd, &wfds);
		if (elm->timeout)
			selector_timeout(elm, &stv, &tv);
	}

	if (fd_max < 0)
		return 0;

	err = select(fd_max + 1, &rfds, &wfds, NULL, timerisset(&tv) ? &tv : NULL);
	if (err < 0) {
		if (errno != EINTR)
			return -1;

		return 0;
	}

	timerclear(&tv);
	gettimeofday(&stv, NULL);

	TAILQ_FOREACH(elm, sel, queue) {
		if (elm->callback == NULL)
			continue;

		if (FD_ISSET(elm->fd, &rfds)) {
			err = selector_read_cb(elm);
			err = elm->callback(SELECTOR_READ, err, elm->context);
			if (err < 0)
				return -1;
		}

		if (FD_ISSET(elm->fd, &wfds)) {
			err = selector_write_cb(elm);
			err = elm->callback(SELECTOR_WRITE, err, elm->context);
			if (err < 0)
				return -1;
		}

		if (elm->timeout)
			selector_timeout(elm, &stv, &tv);
	}

	return 1;
}

/*
 * create selector
 */
struct selector *selector_create(void)
{
	struct selector *sel;

	sel = malloc(sizeof(struct selector));
	if (!sel)
		return NULL;

	memset(sel, 0, sizeof(struct selector));

	TAILQ_INIT(sel);

	return sel;
}

/*
 * destroy selector
 */
void selector_destroy(struct selector *sel)
{
	struct selector_elm *elm;

	while (!TAILQ_EMPTY(sel)) {
		elm = TAILQ_FIRST(sel);
		selector_elm_remove(sel, elm);
	}
	free(sel);
}

