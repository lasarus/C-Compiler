#ifndef TOKEN_LIST_H
#define TOKEN_LIST_H

#include <list.h>
#include "preprocessor.h"

/* #define TOK_EQ(T1, T2) (T1.type == T2.type && strcmp(T1.str, T2.str) == 0) */
/* LIST_FREE_EQ(token_list, struct token, NULL_FREE, TOK_EQ); */

struct token_list {
	int n, capacity;
	struct token *list;
};

void token_list_free(struct token_list *list);
void token_list_add(struct token_list *list, struct token t);
struct token *token_list_top(struct token_list *list);
void token_list_pop(struct token_list *list);
void token_list_push_front(struct token_list *list, struct token t);
int token_list_index_of(struct token_list *list, struct token t);

#endif
