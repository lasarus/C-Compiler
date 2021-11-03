#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "input.h"
#include "string_set.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum ttype {
#define X(A, B) A,
#define KEY(A, B) A,
#define SYM(A, B) A,
    #include "tokens.h"
    #undef X
    #undef KEY
    #undef SYM
	T_COUNT
};

int int_max(int a, int b);

struct token {
    enum ttype type;

    char *str;

    int first_of_line;
    int whitespace;

	struct position pos;

    struct string_set hs; // Hide set. Only unsed internally.
};

struct token token_init(enum ttype type, char *str, struct position pos);
struct token token_move(struct token *from);
struct token token_dup(struct token *from);
struct token token_dup_from_hs(struct token *from, struct string_set hs);

#define ASSERT_TYPE2(TOKEN, TYPE1, TYPE2) do { if(TOKEN.type != TYPE1 && TOKEN.type != TYPE2) { ERROR("Unexpected token %i %s\n", TOKEN.type, TOKEN.str); } } while(0)
#define ASSERT_TYPE(TOKEN, TYPE) ASSERT_TYPE2(TOKEN, TYPE, T_NONE)

// Interface:

#define TNEXT() do {							\
		t_next();								\
	} while (0)
#define TACCEPT(T) T_accept(T)
#define TEXPECT(TYPE) do {												\
		if (TPEEK(0)->type == TYPE) {									\
			TNEXT();													\
		} else {														\
			struct token *t = TPEEK(0);									\
			printf("On line %d col %d file %s\n", t->pos.line, t->pos.column, t->pos.path); \
			printf("Got %s, expected %s\n", strdup(dbg_token(t)), dbg_token_type(TYPE)); \
			if (t->str)													\
				printf("Token string has value %s\n", t->str);			\
			ERROR("Expected other token");								\
		}																\
	} while (0)
#define TPEEK(N) T_peek(N)

#define T0 (TPEEK(0))
#define T_ISNEXT(TYPE) (T0->type == (TYPE))

void t_next(void);
void t_push(struct token t);
int T_accept(enum ttype type);
void T_expect(enum ttype type);
struct token *T_peek(int n);
void preprocessor_create(const char *path);

#endif
