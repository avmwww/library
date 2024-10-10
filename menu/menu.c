/*
 *
 */

#include <stdlib.h>
#include <string.h>

#include "menu.h"
#include "zalloc.h"

/*
 * first entry: prev == NULL
 * last entry : next == NULL
 */
#define menu_elm_first(e)	((e)->prev == NULL)
#define menu_elm_last(e)	((e)->next == NULL)

static menuq_t *menu_last_x(menuq_t *m)
{
	while (m->x.next)
		m = m->x.next;

	return m;
}

/*
 *
 */
int menu_elm_add(menuq_t *menu, menuq_t *elm)
{
	menuq_t *y, *x;

	if (!menu || !elm)
		return -1;

	if (elm->y.prev || elm->y.next)
		return -1;

	y = menu->y.next;

	if (!y) {
		menu->y.next = elm;
	} else {
		x = menu_last_x(y);
		x->x.next = elm;
	}

	return 0;
}

void menu_elm_destroy(menuq_t *elm)
{
	menuq_t *y;

	if (!elm)
		return;

	/* goto to last vertical entry */
	y = elm;
	while (y->y.next)
		y = y->y.next;

	/* link vertical parent to this friend on horizontal,
	 * if elm is last there was only one left in the line: prev and next == NULL,
	 * so link to next vertical elm on prev is set to NULL */
	if (elm->y.prev->y.next == elm)
		elm->y.prev->y.next = elm->x.next ? elm->x.next : elm->x.prev;

	/* relink next horizontal friend to this prev friend */
	if (!menu_elm_last(&elm->x))
		elm->x.next->x.prev = elm->x.prev;

	/* relink prev horizontal friend to this next friend */
	if (!menu_elm_first(&elm->x))
		elm->x.prev->x.next = elm->x.next;

	if (elm->info.name)
		free(elm->info.name);

	if (elm->data && elm->data_len)
		free(elm->data);

	/* remove this entry */
	free(elm);
}

menuq_t *menu_elm_create(void *data, unsigned int data_len)
{
	menuq_t *elm;

	elm = zalloc(sizeof(menuq_t));
	if (!elm)
		return NULL;

	if (!data && data_len != 0) {
		data = zalloc(data_len);
		if (!data) {
			free(elm);
			return NULL;
		}
	} else
		data_len = 0;

	elm->data = data;
	elm->data_len = data_len;

	return elm;
}

menuq_t *menu_elm_create_and_add(menuq_t *menu, void *data, unsigned int data_len)
{
	menuq_t *elm;
	
	elm = menu_elm_create(data, data_len);
	if (!elm)
		return NULL;

	if (menu_elm_add(menu, elm) < 0) {
		menu_elm_destroy(elm);
		return NULL;
	}
	return elm;
}

int menu_elm_init(menuq_t *elm)
{
	if (!elm)
		return -1;

	memset(elm, 0, sizeof(menuq_t));

	return 0;
}

