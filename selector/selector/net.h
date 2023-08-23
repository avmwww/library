/*
 *
 */

#ifndef _SELECTOR_NET_H_
#define _SELECTOR_NET_H_

#include "selector.h"

#define SELECTOR_NET_ANY		0
#define SELECTOR_NET_TCP		1
#define SELECTOR_NET_UDP		2

#define SELECTOR_NET_CLIENT		0
#define SELECTOR_NET_SERVER		1

struct selector_net;

void selector_net_free(struct selector_net *sn);

struct selector_net *selector_net_accept(struct selector_net *sm,
					 int (*callback)(int, int, void *),
					 void *context);

struct selector_net *selector_net_create(struct selector *sel,
					 const char *host,
					 int port, int proto, int server,
					 int (*callback)(int, int, void *),
					 void *context);

void selector_net_destroy(struct selector_net *sn);

int selector_net_wait(struct selector_net *sn, long timeout);

int selector_net_write(struct selector_net *sn, const void *buf, size_t size);

int selector_net_read(struct selector_net *sn, void *buf, size_t size);

int selector_net_fd(struct selector_net *sn);

int selector_net_set_raw(struct selector_net *sn, int raw);

int selector_net_connect(struct selector_net *sn);

int selector_net_disconnect(struct selector_net *sn);

#endif
