#include "splitter.h"
#include "macro_expander.h"
#include "token_list.h"

#include <common.h>

#define NEXT_U() expander_next_unexpanded()
#define NEXT() expander_next()

static char *str_move(char **from) {
	char *str = *from;
	*from = NULL;
	return str;
}

struct splitter {
	struct token_list stack;
} splitter;

enum ttype get_ident(char *str) {
#define X(A, B)
#define SYM(A, B)
#define KEY(A, B) if(strcmp(str, B) == 0) { return A; }
#include "tokens.h"
#undef KEY
#undef X
#undef SYM
	return T_IDENT;
}

char *token_to_str(enum ttype type) {
	switch(type) {
#define PRINT(A, B) case A: return B;
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

enum ttype get_punct(char **in_str) {
	char *str = *in_str;

	enum ttype type;

#define SYM(A, B) else if(B[0] == *str && (sizeof(B) <= 2 || B[1] == *(str + 1)) && (sizeof(B) <= 3 || B[2] == *(str + 2))) { type = A; str += sizeof(B) - 1; }
#define KEY(A, B)
#define X(A, B)

	if(0) {
	}
#include "tokens.h"
	else {
		ERROR("Unrecognized punctuation: %s\n", str);
	}

#undef SYM
#undef KEY
#undef X


	*in_str = str;
	return type;
}

struct token splitter_next(void) {
	if (splitter.stack.n) {
		struct token t = token_move(token_list_top(&splitter.stack));
		token_list_pop(&splitter.stack);
		return t;
	}

	struct token in = NEXT();

	if(in.type == PP_STRING) {
		return token_init(T_STRING, str_move(&in.str), in.pos);
	} else if(in.type == PP_NUMBER) {
		return token_init(T_NUM, str_move(&in.str), in.pos);
	} else if(in.type == PP_PUNCT ||
			  in.type == PP_LPAR || in.type == PP_RPAR ||
			  in.type == PP_COMMA) {
		char *symb = in.str, *start = in.str;
		while (*symb) {
			enum ttype type = get_punct(&symb);

			struct position pos = in.pos;
			pos.column += symb - start;
			struct token t = token_init(type, NULL, pos);
			token_list_push_front(&splitter.stack, t);
		}

		return splitter_next();
	} else {
		return in;
	}
}

struct token splitter_next_unexpanded() {
	if (splitter.stack.n) {
		struct token t = token_move(token_list_top(&splitter.stack));
		token_list_pop(&splitter.stack);
		return t;
	} else {
		return NEXT_U();
	}
}

struct token splitter_next_translate(void) {
	struct token in = splitter_next();

	if(in.type == PP_IDENT) {
		struct token t = token_init(get_ident(in.str), NULL, in.pos);
		if(t.type == T_IDENT)
			t.str = str_move(&in.str);
		return t;
	} else if(in.type == PP_STRING) {
		return token_init(T_STRING, str_move(&in.str), in.pos);
	} else if(in.type == PP_CHARACTER_CONSTANT) {
		return token_init(T_CHARACTER_CONSTANT, str_move(&in.str), in.pos);
	} else if(in.type == PP_NUMBER) {
		return token_init(T_NUM, str_move(&in.str), in.pos);
	} else if(in.type == T_EOI) {
		return token_init(T_EOI, NULL, in.pos);
	} else if (in.type == PP_DIRECTIVE) {
		return in;
	} else {
		return in;
	}

	ERROR("Can't process token %s\n", token_to_str(in.type));
}
