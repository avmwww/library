/*
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "selector.h"
#include "selector/net.h"

struct selector_net {
	struct selector *sel;
	int fd;
	int server;
	char *host;
	int port;
	int proto;
	socklen_t addrlen;
	struct sockaddr_storage addr;
	long timeout;
	int (*callback)(int, int, void *);
	void *context;
};

static int selector_net_timeout_cb(struct selector_net *sn)
{
	return 0;
}

static int selector_net_read_cb(struct selector_net *sn, int size)
{
	/* If socket is listen, send read event is connection ready */
	return size;
}

static int selector_net_write_cb(struct selector_net *sn, int size)
{
	return 0;
}

static int selector_net_callback(int ev, int size, void *arg)
{
	struct selector_net *sn = arg;
	int err;

	if (ev == SELECTOR_READ) {
		err = selector_net_read_cb(sn, size);
	} else if (ev == SELECTOR_WRITE) {
		err = selector_net_write_cb(sn, size);
	} else if (ev == SELECTOR_TIMEOUT) {
		err = selector_net_timeout_cb(sn);
	}

	if (sn->callback)
		err = sn->callback(ev, size, sn->context);

	return err;
}

int selector_net_disconnect(struct selector_net *sn)
{
	if (!sn)
		return -1;

	if (sn->fd == -1)
		return -1;

	if (sn->callback)
		selector_remove(sn->sel, sn->fd);

	close(sn->fd);
	sn->fd = -1;
	return 0;
}

int selector_net_connect(struct selector_net *sn)
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	char sport[64];
	int s;
	int type;

	snprintf(sport, sizeof(sport), "%d", sn->port);

	if (sn->proto == SELECTOR_NET_UDP)
		type = SOCK_DGRAM;
	else if (sn->proto == SELECTOR_NET_TCP)
		type = SOCK_STREAM;
	else
		type = SELECTOR_NET_ANY;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = type;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	s = getaddrinfo(sn->host, sport, &hints, &result);
	if (s < 0)
		return s;

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (s == -1)
			continue;

		if (sn->server == SELECTOR_NET_CLIENT) {
			/* TODO: make connect asynchronous */
			if (connect(s, rp->ai_addr, rp->ai_addrlen) != -1)
				break;
		} else {
			int opt = 1;
			setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
			if (bind(s, rp->ai_addr, rp->ai_addrlen) != -1) {
				if (type == SOCK_DGRAM)
					break;

				if (listen(s, 5) != -1)
					break;
			}
		}

		close(s);
	}

	freeaddrinfo(result);

	if (rp == NULL)
		return -1;

	if (selector_add(sn->sel, s, selector_net_callback, sn) < 0) {
		close(s);
		return -1;
	}
	sn->fd = s;

	return s;
}

/*
 *
 */
static struct selector_net *selector_net_alloc(struct selector *sel,
					       const char *host,
					       int port, int proto, int server,
					       int (*callback)(int, int, void *),
					       void *context)
{
	struct selector_net *sn;
	char *h = NULL;

	sn = malloc(sizeof(struct selector_net));
	if (!sn)
		return NULL;

	if (host) {
		h = strdup(host);
		if (!h) {
			free(sn);
			return NULL;
		}
	}

	memset(sn, 0, sizeof(struct selector_net));

	sn->sel = sel;
	sn->fd = -1;
	sn->server = server;
	sn->port = port;
	sn->proto = proto;
	sn->host = h;
	sn->callback = callback;
	sn->context = context;

	return sn;
}

/*
 *
 */
int selector_net_wait(struct selector_net *sn, long timeout)
{
	sn->timeout = timeout;
	return selector_wait(sn->sel, sn->fd, timeout);
}

int selector_net_write(struct selector_net *sn, const void *buf, size_t size)
{
	return selector_write(sn->sel, sn->fd, buf, size);
}

int selector_net_read(struct selector_net *sn, void *buf, size_t size)
{
	return selector_read(sn->sel, sn->fd, buf, size);
}

int selector_net_set_raw(struct selector_net *sn, int raw)
{
	return selector_set_raw(sn->sel, sn->fd, raw);
}

void selector_net_free(struct selector_net *sn)
{
	if (!sn)
		return;

	close(sn->fd);

	if (sn->host)
		free(sn->host);

	free(sn);
}

struct selector_net *selector_net_accept(struct selector_net *sm,
					 int (*callback)(int, int, void *),
					 void *context)
{
	int s;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	struct selector_net *sn;
	char host[NI_MAXHOST], service[NI_MAXSERV];

	addrlen = sizeof(addr);
	s = accept(sm->fd, (struct sockaddr *)&addr, &addrlen);
	if (s < 0)
		return NULL;

	sn = selector_net_alloc(sm->sel, NULL, 0, sm->proto,
				SELECTOR_NET_CLIENT,
				callback, context);
	if (!sn) {
		close(s);
		return NULL;
	}

	sn->addrlen = addrlen;
	sn->addr = addr;

	if (callback) {
		if (selector_add(sn->sel, s, selector_net_callback, sn) < 0) {
			close(s);
			return NULL;
		}
	}
	sn->fd = s;

	host[0] = service[0] = '\0';
	if ((s = getnameinfo((struct sockaddr *)&sn->addr, sn->addrlen,
		    host, NI_MAXHOST, service,
		    NI_MAXSERV, NI_NUMERICSERV)) < 0) {
		//printf("getnameinfo: %s\n", gai_strerror(s));
	}

	sn->host = strdup(host);
	sn->port = atoi(service);

	return sn;
}

int selector_net_fd(struct selector_net *sn)
{
	if (!sn)
		return -1;

	return sn->fd;
}

struct selector_net *selector_net_create(struct selector *sel,
					const char *host,
					int port, int proto, int server,
					int (*callback)(int, int, void *),
					void *context)
{
	struct selector_net *sn;

	sn = selector_net_alloc(sel, host, port, proto, server,
				callback, context);

	if (!sn)
		return NULL;

	return sn;
}

void selector_net_destroy(struct selector_net *sn)
{
	if (!sn)
		return;

	selector_remove(sn->sel, sn->fd);
	selector_net_free(sn);
}

