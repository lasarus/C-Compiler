#include "preprocessor.h"
#include "tokenizer.h"
#include "string_concat.h"

#include <common.h>

struct token_stream {
	struct token buffer[3], pushed;
} ts;

struct token token_init(enum ttype type, char *str, struct position pos) {
	return (struct token) { .type = type, .str = str, .pos = pos };
}

void t_next() {
	ts.buffer[0] = ts.buffer[1];
	ts.buffer[1] = ts.buffer[2];
	ts.buffer[2] = ts.pushed.type ? ts.pushed : string_concat_next();
	ts.pushed = (struct token) {0};
}

void t_push(struct token t) {
	if (ts.pushed.type != T_NONE)
		ERROR("Internal compiler error.");
	ts.pushed = ts.buffer[2];
	ts.buffer[2] = ts.buffer[1];
	ts.buffer[1] = ts.buffer[0];
	ts.buffer[0] = t;
}

void preprocessor_init(const char *path) {
	tokenizer_push_input_absolute(path);

	for (unsigned i = 0; i < sizeof ts.buffer / sizeof *ts.buffer; i++)
		t_next();
}

struct token *t_peek(int n) {
	if (n > 2)
		ERROR("Can't peek that far");
	return &ts.buffer[n];
}
