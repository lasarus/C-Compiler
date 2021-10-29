#include "preprocessor.h"
#include "macro_expander.h"
#include "tokenizer.h"
#include "splitter.h"
#include "directives.h"
#include "string_concat.h"
#include "token_list.h"

#include <common.h>

#include <precedence.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct token_stream {
    struct token t, nt, nnt;
} ts;

// Token functions.

struct token token_init(enum ttype type, char *str, struct position pos) {
	return (struct token) { .type = type, .str = str, .pos = pos };
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
	t.hs = string_set_dup(from->hs);
	return t;
}

struct token token_dup_from_hs(struct token *from, struct string_set hs) {
	struct token t = *from;
	t.str = strdup(from->str);
	t.hs = hs;
	return t;
}

void token_free(struct token *from) {
	free(from->str);
	string_set_free(from->hs);
	token_delete(from);
}

void t_next() {
	ts.t = ts.nt;
	ts.nt = ts.nnt;
	ts.nnt = string_concat_next();
}

void preprocessor_create(const char *path) {
	tokenizer_push_input_absolute(path);

	t_next();
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
	if (n == 2)
		return &ts.nnt;
	ERROR("Can't peek that far");
}
