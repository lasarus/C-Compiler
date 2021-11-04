#include "token_list.h"

#include <common.h>

void token_list_free(struct token_list *list) {
	free(list->list);
}

void token_list_add(struct token_list *list, struct token t) {
	ADD_ELEMENT(list->size, list->cap, list->list) = t;
}

void token_list_pop(struct token_list *list) {
	list->size--;
}

int token_list_index_of(struct token_list *list, struct token t) {
	for (int i = 0; i < list->size; i++) {
		if (strcmp(list->list[i].str, t.str) == 0) return i;
	}
	return -1;
}

