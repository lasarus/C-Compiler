#include "macro_expander.h"
#include "token_list.h"

#include <common.h>

struct token tokenizer_next();
#define NEXT() tokenizer_next();

#define MAP_SIZE 1024
struct define_map {
	struct define **entries;
};

static struct define_map *define_map = NULL;

void define_map_init(void) {
	define_map = malloc(sizeof *define_map);

	define_map->entries = malloc(sizeof *define_map->entries * MAP_SIZE);
	for (size_t i = 0; i < MAP_SIZE; i++) {
		define_map->entries[i] = NULL;
	}
}

struct define **define_map_find(char *name) {
	if (!define_map)
		define_map_init();
	uint32_t hash_idx = hash_str(name) % MAP_SIZE;

	struct define **it = &define_map->entries[hash_idx];

	while (*it && strcmp((*it)->name, name) != 0) {
		it = &(*it)->next;
	}

	return it;
}

void define_map_add(struct define define) {
	struct define **elem = define_map_find(define.name);

	if (*elem) {
		define.next = (*elem)->next;
		**elem = define;
	} else {
		define.next = NULL;
		*elem = malloc(sizeof define);
		**elem = define;
	}
}

struct define *define_map_get(char *str) {
	return *define_map_find(str);
}

void define_map_remove(char *str) {
	struct define **elem = define_map_find(str);
	if (*elem) {
		struct define *next = (*elem)->next;
		free(*elem);
		*elem = next;
		// TODO: Free contents of elem as well.
	}
}

struct define define_init(char *name) {
	return (struct define) {
		.name = name,
	};
}

void define_free(struct define *d) {
	token_list_free(&d->def);
	token_list_free(&d->par);
	d->func = 0;
}

void define_add_def(struct define *d, struct token t) {
	token_list_add(&d->def, t);
}

void define_add_par(struct define *d, struct token t) {
	d->func = 1;
	token_list_add(&d->par, t);
}

struct expander {
	struct token_list stack;
} expander;

struct token expander_take(void) {
	struct token t;

	if (expander.stack.n > 0) {
		t = token_move(token_list_top(&expander.stack));
		token_list_pop(&expander.stack);
	} else {
		t = NEXT();
	}
	return t;
}

void expander_push(struct token t) {
	token_list_add(&expander.stack, t);
}

void expander_push_front(struct token t) {
	token_list_push_front(&expander.stack, t);
}

// Returns true if done.
int expander_parse_argument(struct token_list *tl, int ignore_comma) {
	int depth = 1;
	*tl = (struct token_list) {0};
	struct token t;
	do {
		t = expander_take();
		if(t.type == PP_LPAR) {
			depth++;
		} else if(t.type == PP_RPAR) {
			depth--;
			if(depth == 0)
				continue;
		} else if(!ignore_comma && t.type == PP_COMMA) {
			if(depth == 1)
				continue;
		}

		token_list_add(tl, t);
	} while(
		!(!ignore_comma && (depth == 1 && t.type == PP_COMMA)) &&
		!(depth == 0 && t.type == PP_RPAR));

	return t.type == PP_RPAR;
}

int get_param(struct define *def, struct token tok) {
	if (!def->func || tok.type != PP_IDENT)
		return -1;

	return token_list_index_of(&def->par, tok);
}

struct token glue(struct token a, struct token b) {
	if (a.type != PP_IDENT ||
		b.type != PP_IDENT) {
		printf("a of type %s %d\n", token_to_str(a.type), a.type);
		printf("b of type %s %d\n", token_to_str(b.type), b.type);
		NOTIMP();
	}

	struct token ret = a;
	ret.str = allocate_printf("%s%s", b.str, a.str);
	ret.hs = string_set_intersection(a.hs, b.hs);

	return ret;
}

void expander_subs(struct define *def, struct string_set *hs,
				   struct position new_pos) {
	string_set_insert(hs, strdup(def->name));

	// Parse function macro.
	int n_args = def->par.n;
	if (n_args > 16)
		ERROR("Unsupported number of elements");
	struct token_list arguments[16] = {0};
	struct token_list vararg = {0};
	int vararg_included = 0;
	if(def->func) {
		struct token lpar = expander_take();
		if (lpar.type != PP_LPAR) {
			PRINT_POS(lpar.pos);
			ERROR("Expected ( in macro expension of %s", def->name);
		}
		ASSERT_TYPE(lpar, PP_LPAR);

		int finished = 0;
		for (int i = 0; i < n_args; i++) {
			if (expander_parse_argument(&arguments[i], 0)) {
				finished = 1;
				if (i != n_args - 1)
					ERROR("Wrong number of arguments to macro");
			}
		}

		if (n_args == 0 && !def->vararg) {
			struct token rpar = expander_take();
			ASSERT_TYPE(rpar, PP_RPAR);
			finished = 1;
		}

		if (def->vararg && !finished) {
			vararg_included = 1;
			if (!expander_parse_argument(&vararg, 1)) {
				ERROR("__VA_ARGS__ Not end of input");
			}
		}
	}

	int concat_with_prev = 0;
	for(int i = def->def.n - 1; i >= 0; i--) {
		struct token t = def->def.list[i];

		int concat = 0;
		int stringify = 0;

		if (i != 0) {
			concat = (def->def.list[i - 1].type == PP_HHASH);
			stringify = (def->def.list[i - 1].type == PP_HASH);
		}

		if (t.type == PP_HHASH)
			ERROR("Concat token at edge of macro expansion.");

		int idx;

		int is_va_args = strcmp(t.str, "__VA_ARGS__") == 0;

		if (!is_va_args)
			idx = get_param(def, t);

		if (is_va_args) {
			if (concat && i - 2 >= 0 &&
				def->def.list[i - 2].type == PP_COMMA &&
				!vararg_included) {
				i--; // There is an additional i-- at the end of the loop.
			} else if (vararg_included) {
				struct token_list tl = vararg;

				int start_it = tl.n - 1;

				if (concat_with_prev && tl.n) {
					struct token *end = &expander.stack.list[
						expander.stack.n - 1
						];
					*end = glue(*end, tl.list[start_it--]);
				}

				for(int i = start_it; i >= 0; i--) {
					struct token t = tl.list[i];
					t.pos = new_pos;
					expander_push(t);
				}
			} else {
				PRINT_POS(T0->pos);
				ERROR("Not implemented, %s (%d)", def->name, def->func);
				NOTIMP();
			}
		} else if (idx >= 0 && stringify) {
			struct token_list tl = arguments[idx];
			char *str = "";

			int start_it = tl.n - 1;
			for(int i = start_it; i >= 0; i--) {
				struct token t = tl.list[i];
				t.pos = new_pos;

				str = allocate_printf("%s%s", t.str, str);
				if (i && t.whitespace)
					str = allocate_printf(" %s", str);
			}

			struct token t = token_init(PP_STRING, str, (struct position) { 0 });
			expander_push(t);
		} else if(idx >= 0) {
			struct token_list tl = arguments[idx];

			int start_it = tl.n - 1;

			if (concat_with_prev && tl.n) {
				struct token *end = &expander.stack.list[
					expander.stack.n - 1
					];
				*end = glue(*end, tl.list[start_it--]);
			}

			for(int i = start_it; i >= 0; i--) {
				struct token t = tl.list[i];
				t.pos = new_pos;
				expander_push(t);
			}

			if (tl.n)
				concat_with_prev = concat;
		} else {
			if (concat_with_prev) {
				struct token *end = &expander.stack.list[
					expander.stack.n - 1
					];
				*end = glue(*end, t);
			} else if (stringify) {
				ERROR("# Should be followed by macro parameter");
			} else {
				t.hs = string_set_union(*hs, t.hs);
				expander_push(t);

				if (concat)
					concat_with_prev = 1;
			}
		}

		if (concat || stringify)
			i--;
	}

	for(int i = 0; i < n_args; i++) {
		token_list_free(&arguments[i]);
	}
}

int builtin_macros(struct token *t) {
	// These are only single tokens.
	if (strcmp(t->str, "__LINE__") == 0) {
		*t = token_init(PP_NUMBER, allocate_printf("%d", t->pos.line), t->pos);
	} else if (strcmp(t->str, "__FILE__") == 0) {
		*t = token_init(PP_STRING, allocate_printf("%s", t->pos.path), t->pos);
	} else if (strcmp(t->str, "__STDC__") == 0) {
		NOTIMP();
	} else if (strcmp(t->str, "__STDC_HOSTED__") == 0) {
		NOTIMP();
	} else if (strcmp(t->str, "__STDC_VERSION__") == 0) {
		char *version_string = "201710L";
		*t = token_init(PP_NUMBER, version_string, t->pos);
	} else if (strcmp(t->str, "__STDC_TIME__") == 0) {
		NOTIMP();
	} else if (strcmp(t->str, "__WORDSIZE") == 0) {
		*t = token_init(PP_NUMBER, "64", t->pos);
	} else
		return 0;
	return 1;
}

struct token expander_next(void) {
	struct token t = expander_take();

	if (t.type != PP_IDENT)
		return t;

	if (string_set_contains(t.hs, t.str))
		return t;

	struct define *def = NULL;
	if (builtin_macros(&t)) {
		return t;
	} else if ((def = define_map_get(t.str))) {
		expander_subs(def, &t.hs, t.pos);
		return expander_next();
	}

	return t;
}

struct token expander_next_unexpanded(void) {
	struct token t = expander_take();
	return t;
}
