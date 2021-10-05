#ifndef MACRO_EXPANDER_H
#define MACRO_EXPANDER_H

#include "preprocessor.h"

struct define {
	char *name;
	int func;
	int vararg;

	struct token_list *def, *par;
};

struct define define_init(char *name);
void define_add_def(struct define *d, struct token t);
void define_add_par(struct define *d, struct token t);

void define_map_add(struct define def);
int define_map_is_defined(char *name);
void define_map_remove(char *name);

struct token expander_next(void);
struct token expander_next_unexpanded(void);
void expander_push_front(struct token t);

#endif
