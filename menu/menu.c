/*
 *
 */

#include <stdlib.h>

#include "menu.h"
#include "zalloc.h"

/*
 * first entry: prev == NULL
 * last entry : next == NULL
 */
#define menu_elm_first(e)	((e)->prev == NULL)
#define menu_elm_last(e)	((e)->next == NULL)

menuq_t *menu_elm_create(unsigned int data_len)
{
	menuq_t *elm;

	elm = zalloc(sizeof(menuq_t) + data_len);
	if (!elm)
		return NULL;

	elm->data_len = data_len;

	return elm;
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

	/* remove this entry */
	free(elm);
}


