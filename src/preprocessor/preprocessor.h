#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <stdio.h>
#include "input.h"

enum ttype {
#define X(A, B) A,
#define KEY(A, B) A,
#define SYM(A, B) A,
    #include "tokens.h"
    #undef X
    #undef KEY
    #undef SYM
};

int int_max(int a, int b);

#include <stdlib.h>
#include <string.h>

#include "../list.h"

#define STR_FREE(A) free(A)
#define STR_EQUALS(A, B) (strcmp(A, B) == 0)
LIST_FREE_EQ(str_list, char *, STR_FREE, STR_EQUALS);

struct token {
    enum ttype type;

    char *str;

    int first_of_line;
    int whitespace;

	struct position pos;

    struct str_list *hs; // Hide set. Only unsed internally.
};

char *token_to_str(enum ttype type);

struct token token_init(enum ttype type, char *str, struct position pos);
void token_delete(struct token *from);
struct token token_move(struct token *from);
struct token token_dup(struct token *from);
struct token token_dup_from_hs(struct token *from, struct str_list *hs);

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
			printf("Got %s (%d), expected %s (%d)\n", token_to_str(t->type), t->type, token_to_str(TYPE), TYPE); \
			if (t->str)													\
				printf("Token string has value %s\n", t->str);			\
			ERROR("Expected other token");								\
		}																\
	} while (0)
#define TPEEK(N) T_peek(N)

#define T0 (TPEEK(0))
#define T_ISNEXT(TYPE) (T0->type == (TYPE))

void t_next(void);
int T_accept(enum ttype type);
void T_expect(enum ttype type);
struct token *T_peek(int n);
void preprocessor_create(const char *path);

const char *token_to_string(struct token *t);

#endif
