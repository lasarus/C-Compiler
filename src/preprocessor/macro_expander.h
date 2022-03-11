#ifndef MACRO_EXPANDER_H
#define MACRO_EXPANDER_H

#include "preprocessor.h"
#include "token_list.h"

struct define {
	struct define *next;
	struct string_view name;
	int func;
	int vararg;

	struct token_list def, par;
};

void define_string(char *name, char *value);
struct define define_init(struct string_view name);
void define_add_def(struct define *d, struct token t);
void define_add_par(struct define *d, struct token t);

void define_map_add(struct define def);
struct define *define_map_get(struct string_view str);
void define_map_remove(struct string_view name);

struct token expander_next(void);

void expand_token_list(struct token_list *ts);

void macro_expander_reset(void);

#endif
