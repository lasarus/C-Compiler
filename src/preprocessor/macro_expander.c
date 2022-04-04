#include "macro_expander.h"
#include "token_list.h"
#include "directives.h"
#include "tokenizer.h"

#include <common.h>

#include <assert.h>

#define MAP_SIZE 1024
struct define_map {
	struct define **entries;
};

static struct define_map *define_map = NULL;

void macro_expander_reset(void) {
	free(define_map->entries);
	free(define_map);
	define_map = NULL;
}

void expand_buffer(int input, int return_output, struct token *t);

static void define_map_init(void) {
	define_map = cc_malloc(sizeof *define_map);

	define_map->entries = cc_malloc(sizeof *define_map->entries * MAP_SIZE);
	for (size_t i = 0; i < MAP_SIZE; i++) {
		define_map->entries[i] = NULL;
	}
}

static struct define **define_map_find(struct string_view name) {
	if (!define_map)
		define_map_init();

	uint32_t hash_idx = sv_hash(name) % MAP_SIZE;

	struct define **it = &define_map->entries[hash_idx];

	while (*it && !sv_cmp((*it)->name, name)) {
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
		*elem = cc_malloc(sizeof define);
		**elem = define;
	}
}

struct define *define_map_get(struct string_view str) {
	return *define_map_find(str);
}

void define_map_remove(struct string_view str) {
	struct define **elem = define_map_find(str);
	if (*elem) {
		struct define *next = (*elem)->next;
		free(*elem);
		*elem = next;
		// TODO: Free contents of elem as well.
	}
}

struct define define_init(struct string_view name) {
	return (struct define) {
		.name = name,
	};
}

void define_add_def(struct define *d, struct token t) {
	token_list_add(&d->def, t);
}

void define_add_par(struct define *d, struct token t) {
	d->func = 1;
	token_list_add(&d->par, t);
}

static int get_param(struct define *def, struct token tok) {
	if (!def->func || tok.type != T_IDENT)
		return -1;

	return token_list_index_of(&def->par, tok);
}

void define_string(char *name, char *value) {
	struct define def = define_init(sv_from_str(strdup(name)));
	struct input input = input_open_string(value);
	def.def = tokenize_input(&input);
	define_map_add(def);
}

// Hopefully sufficiant table for
// combining two token types to
// get another. The strings are simply
// concatenated in all these cases.
enum ttype paste_table[][3] = {
	{T_IDENT, T_IDENT, T_IDENT},
	{T_IDENT, T_NUM, T_IDENT},
	{T_IDENT, T_IDENT, T_IDENT},
	{T_LSHIFT, T_A, T_LSHIFTA},
	{T_RSHIFT, T_A, T_RSHIFTA},
	{T_NUM, T_NUM, T_NUM},
	{T_NUM, T_IDENT, T_NUM},
	{T_ADD, T_ADD, T_INC},
	{T_ADD, T_A, T_ADDA},
	{T_SUB, T_SUB, T_DEC},
	{T_SUB, T_G, T_ARROW},
	{T_SUB, T_A, T_SUBA},
	{T_DIV, T_A, T_DIVA},
	{T_MOD, T_A, T_MODA},
	{T_G, T_GEQ, T_RSHIFTA},
	{T_G, T_G, T_RSHIFT},
	{T_G, T_A, T_GEQ},
	{T_L, T_LEQ, T_LSHIFTA},
	{T_L, T_L, T_LSHIFT},
	{T_L, T_A, T_LEQ},
	{T_NOT, T_A, T_NEQ},
	{T_BOR, T_BOR, T_OR},
	{T_BOR, T_A, T_BORA},
	{T_XOR, T_A, T_XORA},
	{T_A, T_A, T_EQ},
	{T_STAR, T_A, T_MULA},
	{T_AMP, T_A, T_BANDA},
	{T_AMP, T_AMP, T_AND},
};

static struct token glue(struct token a, struct token b) {
	struct token ret = { 0 };
	for (unsigned i = 0; i < sizeof paste_table / sizeof *paste_table; i++)
		if (paste_table[i][1] == a.type && paste_table[i][0] == b.type)
			ret.type = paste_table[i][2];
	if (!ret.type)
		ERROR(a.pos, "Invalid paste of %.*s and %.*s", b.str.len, b.str.str, a.str.len, a.str.str);

	ret.str = sv_from_str(allocate_printf("%s%s", sv_to_str(b.str), sv_to_str(a.str))); // TODO: This can be done better.
	ret.hs = string_set_intersection(a.hs, b.hs);
	ret.pos = a.pos;

	return ret;
}

static size_t stringify_size, stringify_cap;
static char *stringify_buffer;

static void stringify_start(void) {
	stringify_size = 0;
	ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = '\"';
}

static struct string_view stringify_end(void) {
	ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = '\"';
	
	struct string_view ret = { .len = stringify_size };
	ret.str = cc_malloc(stringify_size);
	memcpy(ret.str, stringify_buffer, stringify_size);
	return ret;
}

static void stringify_add(struct token *t, int start) {
	if (!start && t->whitespace)
		ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = ' ';
	switch (t->type) {
	case T_STRING:
	case T_CHARACTER_CONSTANT:
		for (int i = 0; i < t->str.len; i++) {
			char escape_seq[5];
			character_to_escape_sequence(t->str.str[i], escape_seq, 1, 1);
			for (int j = 0; escape_seq[j]; j++)
				ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = escape_seq[j];
		}
		break;

	default:
		for (int i = 0; i < t->str.len; i++)
			ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = t->str.str[i];
	}
}

static int builtin_macros(struct token *t) {
	// These are only single tokens.
	if (sv_string_cmp(t->str, "__LINE__")) {
		*t = (struct token) { .type = T_NUM, .str = sv_from_str(allocate_printf("%d", t->pos.line)), .pos = t->pos };
	} else if (sv_string_cmp(t->str, "__FILE__")) {
		*t = (struct token) { .type = T_STRING, .str = sv_from_str(allocate_printf("\"%s\"", t->pos.path)), .pos = t->pos };
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

static void input_buffer_push(struct token *t) {
	struct token n_token = *t;
	n_token.hs = string_set_dup(n_token.hs);
	ADD_ELEMENT(input_buffer.size, input_buffer.cap, input_buffer.tokens) = n_token;
}

static struct token input_buffer_take(int input) {
	if (input_buffer.size)
		return input_buffer.tokens[--input_buffer.size];

	if (input) {
		return directiver_next();
	} else {
		ICE("Reached end of input buffer.");
	}
}

static struct token *input_buffer_top(int input) {
	if (input_buffer.size)
		return &input_buffer.tokens[input_buffer.size - 1];

	if (input) {
		struct token t = directiver_next();
		input_buffer_push(&t);
		return input_buffer_top(input);
	} else {
		ICE("Reached end of input buffer.");
	}
}

// Returns true if done.
static int input_buffer_parse_argument(struct token_list *tl, int ignore_comma, int input) {
	int depth = 1;
	*tl = (struct token_list) {0};
	struct token t;
	do {
		t = input_buffer_take(input);
		if(t.type == T_LPAR) {
			depth++;
		} else if(t.type == T_RPAR) {
			depth--;
			if(depth == 0)
				continue;
		} else if(!ignore_comma && t.type == T_COMMA) {
			if(depth == 1)
				continue;
		}

		token_list_add(tl, t);
	} while(
		!(!ignore_comma && (depth == 1 && t.type == T_COMMA)) &&
		!(depth == 0 && t.type == T_RPAR));

	if (t.type == T_RPAR)
		input_buffer_push(&t);
	return t.type == T_RPAR;
}

static void expand_argument(struct token_list tl, int *concat_with_prev, int concat, int stringify, int input) {
	int start_it = tl.size - 1;

	if (*concat_with_prev && tl.size) {
		struct token *end = input_buffer_top(input);
		*end = glue(*end, tl.list[start_it--]);
	}

	int run_expand_again = 0;
	if (!(concat || stringify)) {
		run_expand_again = 1;
		struct token t = { .type = T_EOI };
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

static void subs_buffer(struct define *def, struct string_set *hs, struct position new_pos, int input) {
	int n_args = def->par.size;
	struct token_list *arguments = cc_malloc(sizeof *arguments * n_args);

	struct token_list vararg = {0};
	int vararg_included = 0;
	if(def->func) {
		struct token lpar = input_buffer_take(input);
		EXPECT(&lpar, T_LPAR);

		int finished = 0;
		for (int i = 0; i < n_args; i++) {
			if (input_buffer_parse_argument(&arguments[i], 0, input)) {
				finished = 1;
				if (i != n_args - 1)
					ERROR(lpar.pos, "Wrong number of arguments to macro");
			}
		}


		if (def->vararg && !finished) {
			vararg_included = 1;
			if (!input_buffer_parse_argument(&vararg, 1, input)) {
				ERROR(lpar.pos, "__VA_ARGS__ Not end of input");
			}
		}
		
		struct token rpar = input_buffer_take(input);
		EXPECT(&rpar, T_RPAR);

		*hs = string_set_intersection(*hs, rpar.hs);
	}

	string_set_insert(hs, sv_to_str(def->name));

	size_t input_start = input_buffer.size;
	int concat_with_prev = 0;
	for(int i = def->def.size - 1; i >= 0; i--) {
		struct token t = def->def.list[i];

		const int concat = (i != 0) && def->def.list[i - 1].type == PP_HHASH;
		const int stringify = (i != 0) && def->def.list[i - 1].type == PP_HASH;

		if (t.type == PP_HHASH)
			ERROR(t.pos, "Concat token at edge of macro expansion.");

		if (sv_string_cmp(t.str, "__VA_ARGS__")) {
			if (concat && i - 2 >= 0 &&
				def->def.list[i - 2].type == T_COMMA &&
				!vararg_included) {
				i--; // There is an additional i-- at the end of the loop.
			} else if (vararg_included) {
				expand_argument(vararg, &concat_with_prev, concat, stringify, input);
			}
		} else {
			const int idx = get_param(def, t);
			if (idx >= 0 && stringify) {
				struct token_list tl = arguments[idx];
				stringify_start();

				for(int i = 0; i < tl.size; i++)
					stringify_add(tl.list + i, i == 0);

				ADD_ELEMENT(stringify_size, stringify_cap, stringify_buffer) = '\"';

				struct token t_new = t;
				t_new.type = T_STRING;
				t_new.str = stringify_end();
				input_buffer_push(&t_new);
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
					ERROR(t.pos, "# Should be followed by macro parameter");
				} else {
					t.pos = new_pos;
					input_buffer_push(&t);
				}

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

	free(arguments);
}

// Expands until empty token.
void expand_buffer(int input, int return_output, struct token *t) { 
	while (input || input_buffer.size) {
		struct token top = input_buffer_take(input);
		if (top.type == T_EOI)
			break;

		struct define *def = NULL;
		if (top.type != T_IDENT || string_set_contains(top.hs, top.str) ||
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

		if ((def->func && (input || input_buffer.size) && input_buffer_top(input)->type == T_LPAR) ||
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
		*t = (struct token) { .type = T_EOI };
}

struct token expander_next(void) {
	struct token t;
	expand_buffer(1, 1, &t);

	return t;
}

void expand_token_list(struct token_list *ts) {
	input_buffer_push(&(struct token) { .type = T_EOI });

	for(int i = ts->size - 1; i >= 0; i--) {
		struct token t = ts->list[i];
		input_buffer_push(&t);
	}

	size_t output_pos = output_buffer.size;
	expand_buffer(0, 0, NULL);

	ts->size = 0;
	for (unsigned i = output_pos; i < output_buffer.size; i++) {
		token_list_add(ts, output_buffer.tokens[i]);
	}

	output_buffer.size = output_pos;
}
