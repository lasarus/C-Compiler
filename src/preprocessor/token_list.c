#include "token_list.h"

#include <common.h>

void token_list_free(struct token_list *list) {
	free(list->list);
}

void token_list_add(struct token_list *list, struct token t) {
	ADD_ELEMENT(list->size, list->cap, list->list) = t;
}

struct token *token_list_top(struct token_list *list) {
	return &list->list[list->size - 1];
}

void token_list_pop(struct token_list *list) {
	list->size--;
}

void token_list_push_front(struct token_list *list, struct token t) {
	token_list_add(list, t);
	for (int i = list->size - 1; i >= 1; i--) {
		list->list[i] = list->list[i - 1];
	}
	list->list[0] = t;
}

int token_list_index_of(struct token_list *list, struct token t) {
	for (int i = 0; i < list->size; i++) {
		if (strcmp(list->list[i].str, t.str) == 0) return i;
	}
	return -1;
}

