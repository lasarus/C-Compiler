#include "string_concat.h"
#include "directives.h"

#include <common.h>

static int has_prev = 0;
static struct token prev = { 0 };

struct token string_concat_next(void) {
	if (has_prev && prev.type != T_STRING) {
		has_prev = 0;
		return prev;
	}

	struct token t = directiver_next();

	while (t.type == T_STRING) {
		struct token nt = directiver_next();
		if (nt.type != T_STRING) {
			has_prev = 1;
			prev = nt;
			return t;
		} else {
			char *concat = malloc(strlen(t.str) + strlen(nt.str) + 1);
			sprintf(concat, "%s%s", t.str, nt.str);
			t = token_init(T_STRING, concat, t.pos);
		}
	}

	return t;
}
