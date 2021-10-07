#include "preprocessor.h"
#include "macro_expander.h"
#include "tokenizer.h"
#include "splitter.h"
#include "directives.h"
#include "string_concat.h"


#include <common.h>

#include <precedence.h>
#include <list.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TOK_EQ(T1, T2) (T1.type == T2.type && strcmp(T1.str, T2.str) == 0)
LIST_FREE_EQ(token_list, struct token, NULL_FREE, TOK_EQ);

struct token_stream {
    struct token t, nt;
} ts;

// Token functions.

struct token token_init(enum ttype type, char *str, struct position pos) {
	struct token t = {.type = type, .str = str, .pos = pos};
	t.hs = NULL;
	return t;
}

void token_delete(struct token *from) {
	from->str = 0;
	from->type = T_NONE;
}

struct token token_move(struct token *from) {
	struct token to = *from;
	*from = token_init(T_NONE, NULL, (struct position) {
			.path = "invalid",
			.line = 0,
			.column = 0
		});
	return to;
}

struct token token_dup(struct token *from) {
	struct token t = *from;
	t.str = strdup(from->str);
	t.hs = str_list_dup(from->hs);
	return t;
}

struct token token_dup_from_hs(struct token *from, struct str_list *hs) {
	struct token t = *from;
	t.str = strdup(from->str);
	t.hs = hs;
	return t;
}

void token_free(struct token *from) {
	free(from->str);
	str_list_free(from->hs);
	token_delete(from);
}

void t_next() {
	ts.t = ts.nt;
	ts.nt = string_concat_next();
}

void preprocessor_create(const char *path) {
	tokenizer_push_input_absolute(path);

	t_next();
	t_next();
}

// TODO: These should be replaced by macros.
int T_accept(enum ttype type) {
	if (ts.t.type == type) {
		t_next();
		return 1;
	}
	return 0;
}

void T_expect(enum ttype type) {
	if (ts.t.type == type) {
		t_next();
		return;
	}
	ERROR("Expected token %s got %s", token_to_str(type), token_to_str(ts.t.type));
}

struct token *T_peek(int n) {
	if (n == 0)
		return &ts.t;
	if (n == 1)
		return &ts.nt;
	ERROR("Can't peek that far");
}

const char *token_to_string(struct token *t) {
	static int char_buffer_size = 100;
	static char *buffer = NULL;

	if (!buffer) {
		buffer = malloc(char_buffer_size);
	}

	int curr_pos = 0;
	
#define PPRINT(STR, ...) do {											\
		int print_size = snprintf(buffer + curr_pos, char_buffer_size - 1 - curr_pos, STR, ##__VA_ARGS__); \
		int req_size = print_size + 1 + curr_pos; \
		if (req_size > char_buffer_size) {								\
			char_buffer_size = req_size;								\
			buffer = realloc(buffer, char_buffer_size);					\
			snprintf(buffer + curr_pos, char_buffer_size - 1 - curr_pos, STR, ##__VA_ARGS__); \
		}																\
		curr_pos += print_size;\
	} while (0)

	if (t->type == T_IDENT) {
		PPRINT("%s", t->str);
	} else {
		switch(t->type) {
#define PRINT(A, B) case A: PPRINT("%s", B); break;
#define X(A, B) PRINT(A, B)
#define SYM(A, B) PRINT(A, B)
#define KEY(A, B) PRINT(A, B)
#include "tokens.h"
#undef KEY
#undef X
#undef SYM
		default:
			return "";
		}
	}

	return buffer;
}
