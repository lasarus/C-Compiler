#ifndef TOKEN_LIST_H
#define TOKEN_LIST_H

#include "preprocessor.h"

struct token_list {
	int size, cap;
	struct token *list;
};

void token_list_free(struct token_list *list);
void token_list_add(struct token_list *list, struct token t);
int token_list_index_of(struct token_list *list, struct token t);

#endif
