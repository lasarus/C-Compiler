#include "preprocessor.h"
#include "directives.h"
#include "tokenizer.h"
#include "string_concat.h"
#include "macro_expander.h"

#include <common.h>
#include <assert.h>

static struct token_stream {
	struct token buffer[3], pushed;
} ts;

void t_next(void) {
	ts.buffer[0] = ts.buffer[1];
	ts.buffer[1] = ts.buffer[2];
	ts.buffer[2] = ts.pushed.type ? ts.pushed : string_concat_next();
	ts.pushed = (struct token) {0};
}

void t_push(struct token t) {
	if (ts.pushed.type != T_NONE)
		ICE("Pushed buffer overfull.");
	ts.pushed = ts.buffer[2];
	ts.buffer[2] = ts.buffer[1];
	ts.buffer[1] = ts.buffer[0];
	ts.buffer[0] = t;
}

void preprocessor_init(const char *path) {
	directiver_push_input(path, 0);

	for (unsigned i = 0; i < sizeof ts.buffer / sizeof *ts.buffer; i++)
		t_next();
}

struct token *t_peek(int n) {
	assert(n <= 2);
	return &ts.buffer[n];
}

void preprocessor_reset(void) {
	directiver_reset();
	input_reset();
	macro_expander_reset();
}
