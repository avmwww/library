/*
 *
 */

#ifndef _MENU_H_
#define _MENU_H_

typedef struct menu_info {
	char *name;
	char *options;
} menu_info_t;

typedef struct menuq {
	struct {
		struct menuq *next;
		struct menuq *prev;
	} x, y;
	menu_info_t info;
	unsigned int data_len;
	void *data;
} menuq_t;

void menu_elm_destroy(menuq_t *elm);

int menu_elm_add(menuq_t *menu, menuq_t *elm);

menuq_t *menu_elm_create(void *data, unsigned int data_len);

menuq_t *menu_elm_create_and_add(menuq_t *menu, void *data, unsigned int data_len);

int menu_elm_init(menuq_t *elm);

#define menu_create(d, n)			menu_elm_create(d, n)
#define menu_init(e)				menu_elm_init(e)

#endif

