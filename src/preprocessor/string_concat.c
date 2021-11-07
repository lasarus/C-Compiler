#include "string_concat.h"
#include "macro_expander.h"

#include <common.h>

static enum ttype get_ident(struct string_view str) {
#define X(A, B)
#define SYM(A, B)
#define KEY(A, B) if(sv_string_cmp(str, B)) { return A; }
#include "tokens.h"
#undef KEY
#undef X
#undef SYM
	return T_IDENT;
}

struct token string_concat_next(void) {
	static int has_prev = 0;
	static struct token prev = { 0 };

	if (has_prev && prev.type != T_STRING) {
		has_prev = 0;
		return prev;
	}

	struct token t = expander_next();

	while (t.type == T_STRING) {
		struct token nt = expander_next();
		if (nt.type != T_STRING) {
			has_prev = 1;
			prev = nt;
			break;
		} else {
			t.str.len--;
			struct string_view concat = sv_concat(t.str, nt.str);
			t.str = concat;
		}
	}

	if (t.type == T_IDENT)
		t.type = get_ident(t.str);

	return t;
}
