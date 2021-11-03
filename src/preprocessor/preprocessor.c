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
	int has_pushed;
	struct token pushed;
} ts;

// Token functions.

struct token token_init(enum ttype type, char *str, struct position pos) {
	return (struct token) { .type = type, .str = str, .pos = pos };
}

struct token token_dup(struct token *from) {
	struct token t = *from;
	t.str = strdup(from->str);
	t.hs = string_set_dup(from->hs);
	return t;
}

void t_next() {
	ts.t = ts.nt;
	ts.nt = ts.nnt;
	if (ts.has_pushed) {
		ts.nnt = ts.pushed;
		ts.has_pushed = 0;
	} else {
		ts.nnt = string_concat_next();
	}
}

void t_push(struct token t) {
	if (ts.has_pushed)
		ERROR("Internal compiler error.");
	ts.has_pushed = 1;
	ts.pushed = ts.nnt;
	ts.nnt = ts.nt;
	ts.nt = ts.t;
	ts.t = t;
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

struct token *T_peek(int n) {
	if (n == 0)
		return &ts.t;
	if (n == 1)
		return &ts.nt;
	if (n == 2)
		return &ts.nnt;
	ERROR("Can't peek that far");
}
