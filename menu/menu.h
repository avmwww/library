/*
 *
 */

#ifndef _MENU_H_
#define _MENU_H_

typedef struct menu_info {
	char *name;
} menu_info_t;

typedef struct menuq {
	struct {
		struct menuq *next;
		struct menuq *prev;
	} x, y;
	unsigned int data_len;
	menu_info_t info;
	long data[0];
} menuq_t;

menuq_t *menu_elm_create(unsigned int data_len);

void menu_elm_destroy(menuq_t *elm);


#endif
