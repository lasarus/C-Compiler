#include "token_list.h"

#include <common.h>

void token_list_free(struct token_list *list) {
	free(list->list);
}

void token_list_add(struct token_list *list, struct token t) {
	ADD_ELEMENT(list->size, list->cap, list->list) = t;
}

int token_list_index_of(struct token_list *list, struct token t) {
	for (int i = 0; i < list->size; i++) {
		if (sv_cmp(list->list[i].str, t.str)) return i;
	}
	return -1;
}

struct token token_list_take_first(struct token_list *list) {
	if (!list->size)
		ICE("Can't take first token from empty list");
	struct token ret = list->list[0];
	REMOVE_ELEMENT(list->size, list->list, 0);
	return ret;
}
