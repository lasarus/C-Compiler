#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "input.h"
#include "string_set.h"

#include <string_view.h>

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

struct token {
    enum ttype type;

	struct string_view str;

    int first_of_line, first_of_line_after;
    int whitespace, whitespace_after;

	struct position pos;

    struct string_set hs; // Hide set. Only unsed internally.
};

#define EXPECT(T0, ETYPE) do {											\
	if ((T0)->type != ETYPE) {											\
		ERROR((T0)->pos, "Got %s expected %s\n", strdup(dbg_token((T0))), dbg_token_type(ETYPE)); \
			  }															\
	} while (0)

#define TNEXT() t_next()
#define TACCEPT(T) (T0->type == T ? (t_next(), 1) : 0)
#define TEXPECT(TYPE) do { EXPECT(T0, TYPE); TNEXT(); } while (0)

#define T0 (t_peek(0))
#define T1 (t_peek(1))
#define T2 (t_peek(2))

void t_next(void);
void t_push(struct token t);
struct token *t_peek(int n);

void preprocessor_init(const char *path);
void preprocessor_reset(void);

void define_string(char *name, char *value); // Defined in macro_expander.c
void define_remove(const char *name);

void preprocessor_write_dependencies(void);
void preprocessor_finish_writing_dependencies(const char *mt, const char *mf);

#endif
