#include "macro_expander.h"
#include "token_list.h"

#include <common.h>

#include <time.h>
#include <assert.h>

void expand_buffer(int input, int return_output, struct token *t);

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

int get_param(struct define *def, struct token tok) {
	if (!def->func || tok.type != PP_IDENT)
		return -1;

	return token_list_index_of(&def->par, tok);
}

// TODO: Make this use the tokenizer.
// Strings are not correctly handled in the early stages of the preprocessor.
struct token glue(struct token a, struct token b) {
	struct token ret = a;
	if (a.type == PP_STRING || b.type == PP_STRING) {
		// This ignores cases like u8"string".
		ERROR("Can't paste strings.");
	} else if (a.type == PP_CHARACTER_CONSTANT || b.type == PP_CHARACTER_CONSTANT) {
		// This ignores L'a'.
		ERROR("Can't paste character constants.");
	} else if (b.type == PP_IDENT) {
		ret.type = PP_IDENT;
		if (!(a.type == PP_IDENT || a.type == PP_NUMBER)) {
			PRINT_POS(a.pos);
			ERROR("Can't paste %s and %s", a.str, b.str);
		}
	} else if (b.type == PP_IDENT) {
		ret.type = PP_IDENT;
	} else if (b.type == PP_HASH || b.type == PP_HHASH ||
		b.type == PP_LPAR || b.type == PP_COMMA ||
		b.type == PP_RPAR || b.type == PP_DIRECTIVE ||
		b.type == PP_PUNCT) {
		ret.type = PP_PUNCT;
	}

	ret.str = allocate_printf("%s%s", b.str, a.str);
	ret.hs = string_set_intersection(a.hs, b.hs);

	return ret;
}

static size_t stringify_size, stringify_cap;
static char *stringify_buffer;

void stringify_start() {
	stringify_size = 0;
	ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = '\0';
}

void stringify_add(struct token *t, int start) {
	stringify_size--;
	
	if (!start && t->whitespace)
		ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = ' ';
	switch (t->type) {
	case PP_STRING:
		ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = '"';
		for (int i = 0; t->str[i]; i++) {
			char escape_seq[5];
			character_to_escape_sequence(t->str[i], escape_seq);
			for (int j = 0; escape_seq[j]; j++)
				ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = escape_seq[j];
		}
		ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = '"';
		break;

	default:
		for (int i = 0; t->str[i]; i++)
			ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = t->str[i];
	}
	ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = '\0';
}

int builtin_macros(struct token *t) {
	// These are only single tokens.
	if (strcmp(t->str, "__LINE__") == 0) {
		*t = token_init(PP_NUMBER, allocate_printf("%d", t->pos.line), t->pos);
	} else if (strcmp(t->str, "__FILE__") == 0) {
		*t = token_init(PP_STRING, allocate_printf("%s", t->pos.path), t->pos);
	} else if (strcmp(t->str, "__DATE__") == 0) {
		static const char months[][4] = {
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
		};

		time_t ti = time(NULL);
		struct tm tm = *localtime(&ti);
		*t = token_init(PP_STRING, allocate_printf("%s %2d %04d", months[tm.tm_mon], tm.tm_mday, 1900 + tm.tm_year), t->pos);
	} else if (strcmp(t->str, "__TIME__") == 0) {
		time_t ti = time(NULL);
		struct tm tm = *localtime(&ti);
		*t = token_init(PP_STRING, allocate_printf("%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec), t->pos);
	} else if (strcmp(t->str, "__STDC__") == 0) {
		*t = token_init(PP_NUMBER, "1", t->pos);
	} else if (strcmp(t->str, "__STDC_HOSTED__") == 0) {
		*t = token_init(PP_NUMBER, "0", t->pos);
	} else if (strcmp(t->str, "__STDC_VERSION__") == 0) {
		char *version_string = "201710L";
		*t = token_init(PP_NUMBER, version_string, t->pos);
	} else if (strcmp(t->str, "__STDC_TIME__") == 0) {
		NOTIMP(); // Is this actually a macro that is required by anyone?
	} else if (strcmp(t->str, "__WORDSIZE") == 0) {
		*t = token_init(PP_NUMBER, "64", t->pos);
	} else
		return 0;
	return 1;
}

static struct {
	size_t size, cap;
	struct token *tokens;
} input_buffer;

static struct {
	size_t size, cap;
	struct token *tokens;
} output_buffer;

void input_buffer_push(struct token *t) {
	ADD_ELEMENT(input_buffer.size, input_buffer.cap, input_buffer.tokens) = *t;
}

struct token input_buffer_take(int input) {
	if (input_buffer.size)
		return input_buffer.tokens[--input_buffer.size];

	if (input) {
		return NEXT();
	} else {
		ERROR("Reached end of input buffer.");
	}
}

struct token *input_buffer_top(int input) {
	if (input_buffer.size)
		return &input_buffer.tokens[input_buffer.size - 1];

	if (input) {
		struct token t = NEXT();
		input_buffer_push(&t);
		return input_buffer_top(input);
	} else {
		ERROR("Reached end of input buffer.");
	}
}

// Returns true if done.
int input_buffer_parse_argument(struct token_list *tl, int ignore_comma, int input) {
	int depth = 1;
	*tl = (struct token_list) {0};
	struct token t;
	do {
		t = input_buffer_take(input);
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

	if (t.type == PP_RPAR)
		input_buffer_push(&t);
	return t.type == PP_RPAR;
}

void expand_argument(struct token_list tl, int *concat_with_prev, int concat, int stringify, int input) {
	int start_it = tl.size - 1;

	if (*concat_with_prev && tl.size) {
		struct token *end = input_buffer_top(input);
		*end = glue(*end, tl.list[start_it--]);
	}

	int run_expand_again = 0;
	if (!(concat || stringify)) {
		run_expand_again = 1;
		struct token t = token_init(T_EOI, "", (struct position) { 0 });
		input_buffer_push(&t);
	}

	for(int i = start_it; i >= 0; i--) {
		struct token t = tl.list[i];
		input_buffer_push(&t);
	}

	if (run_expand_again) {
		size_t output_pos = output_buffer.size;
		expand_buffer(input, 0, NULL);

		for (int i = output_buffer.size - 1; i >= (int)output_pos; i--) {
			input_buffer_push(&output_buffer.tokens[i]);
		}
		output_buffer.size = output_pos;
		// Expand argument again.
	}
}

void subs_buffer(struct define *def, struct string_set *hs, struct position new_pos, int input) {
	int n_args = def->par.size;
	if (n_args > 16)
		ERROR("Unsupported number of elements");
	struct token_list arguments[16] = {0};
	struct token_list vararg = {0};
	int vararg_included = 0;
	if(def->func) {
		struct token lpar = input_buffer_take(input);
		EXPECT(&lpar, PP_LPAR);

		int finished = 0;
		for (int i = 0; i < n_args; i++) {
			if (input_buffer_parse_argument(&arguments[i], 0, input)) {
				finished = 1;
				if (i != n_args - 1)
					ERROR("Wrong number of arguments to macro");
			}
		}


		if (def->vararg && !finished) {
			vararg_included = 1;
			if (!input_buffer_parse_argument(&vararg, 1, input)) {
				ERROR("__VA_ARGS__ Not end of input");
			}
		}
		
		struct token rpar = input_buffer_take(input);
		EXPECT(&rpar, PP_RPAR);
		finished = 1;

		*hs = string_set_intersection(*hs, rpar.hs);
	}
	(void)vararg_included;

	string_set_insert(hs, strdup(def->name));

	size_t input_start = input_buffer.size;
	int concat_with_prev = 0;
	for(int i = def->def.size - 1; i >= 0; i--) {
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
				expand_argument(vararg, &concat_with_prev, concat, stringify, input);
			} else {
				PRINT_POS(T0->pos);
				ERROR("Not implemented, %s (%d)", def->name, def->func);
				NOTIMP();
			}
		} else if (idx >= 0 && stringify) {
			struct token_list tl = arguments[idx];
			stringify_start();

			for(int i = 0; i < tl.size; i++)
				stringify_add(tl.list + i, i == 0);

			struct token t = token_init(PP_STRING, strdup(stringify_buffer), (struct position) { 0 });
			input_buffer_push(&t);
		} else if(idx >= 0) {
			expand_argument(arguments[idx], &concat_with_prev, concat, stringify, input);

			if (arguments[idx].size)
				concat_with_prev = concat;
		} else {
			if (concat_with_prev) {
				struct token *end = input_buffer_top(input);
				*end = glue(*end, t);
				concat_with_prev = 0;
			} else if (stringify) {
				ERROR("# Should be followed by macro parameter");
			} else {
				t.pos = new_pos;
				input_buffer_push(&t);

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

	for(unsigned i = input_start; i < input_buffer.size; i++) {
		struct token *tok = input_buffer.tokens + i;
		tok->hs = string_set_union(*hs, tok->hs);
	}
}

// Expands until empty token.
void expand_buffer(int input, int return_output, struct token *t) { 
	while (input || input_buffer.size) {
		struct token top = input_buffer_take(input);
		if (top.type == T_EOI)
			break;

		struct define *def = NULL;
		if (top.type != PP_IDENT || string_set_contains(top.hs, top.str) ||
			builtin_macros(&top) || !(def = define_map_get(top.str))) {
			if (return_output) {
				*t = top;
				return;
			} else {
				ADD_ELEMENT(output_buffer.size, output_buffer.cap, output_buffer.tokens) = top;
				continue;
			}
		}

		assert(def);

		if ((def->func && (input || input_buffer.size) && input_buffer_top(input)->type == PP_LPAR) ||
			!def->func) {
			subs_buffer(def, &top.hs, top.pos, input);
		} else {
			if (return_output) {
				*t = top;
				return;
			} else {
				ADD_ELEMENT(output_buffer.size, output_buffer.cap, output_buffer.tokens) = top;
				continue;
			}
		}
	}

	if (return_output)
		*t = token_init(T_EOI, NULL, (struct position) { 0 });
}

struct token expander_next(void) {
	struct token t;
	expand_buffer(1, 1, &t);

	return t;
}

struct token expander_next_unexpanded(void) {
	struct token t = input_buffer_take(1);
	return t;
}

void expander_push_front(struct token t) {
	input_buffer_push(&t);
}
