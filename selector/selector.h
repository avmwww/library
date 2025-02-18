/*
 *
 */

#ifndef _SELECTOR_H_
#define _SELECTOR_H_

#include <stdlib.h>

struct selector;

typedef struct selector_buf {
	void *buf;       /* buffer */
	size_t buf_size; /* current buffer size */
	size_t limit;    /* buffer size limit */
	size_t size;     /* current size of transfer */
	size_t offt;     /* current transfer offset */
	size_t bytes;    /* processed bytes, for statistic */
} selector_buf_t;

#define SELECTOR_BUFFER_LIMIT	(128 * 1024)

#define SELECTOR_READ		0
#define SELECTOR_WRITE		1
#define SELECTOR_TIMEOUT	3

int selector_add(struct selector *sel, int fd,
		 int (*callback)(int, int, void *), void *context);

int selector_remove(struct selector *sel, int fd);

int selector_wait(struct selector *sel, int fd, long timeout);

int selector_read(struct selector *sel, int fd, void *buf, size_t size);

int selector_write(struct selector *sel, int fd, const void *buf, size_t size);

int selector_write_arm(struct selector *sel, int fd);

int selector_set_raw(struct selector *sel, int fd, int raw);

int selector_set_read_cb(struct selector *sel, int fd, int (*cb)(int, void *, int, void *));

struct selector *selector_create(void);

void selector_destroy(struct selector *sel);

int selector_exec(struct selector *sel);

#endif
