#include "directives.h"
#include "macro_expander.h"
#include "tokenizer.h"
#include "string_concat.h"

#include <common.h>
#include <precedence.h>
#include <arch/x64.h>

#include <assert.h>

struct tokenized_file {
	int token_idx;
	struct token_list tokens;

	int pushed_idx;
	struct token pushed[3];

	const char *path;
	struct tokenized_file *parent;
};

static struct tokenized_file *current_file;

static const char *new_path = NULL;
static int line_diff = 0;

struct macro_stack {
	size_t size, cap;
	struct define *defines;
};

static size_t macro_stack_size, macro_stack_cap;
static struct macro_stack *macro_stacks;

static int write_dependencies = 0;
static size_t dep_size, dep_cap;
static char **deps;

void directiver_write_dependencies(void) {
	write_dependencies = 1;
}

void directiver_finish_writing_dependencies(const char *mt, const char *mf) {
	write_dependencies = 1;

	FILE *fp = fopen(mf, "w");

	fprintf(fp, "%s:", mt);
	for (unsigned i = 0; i < dep_size; i++)
		fprintf(fp, " %s", deps[i]);
	fprintf(fp, "\n");

	fclose(fp);
}

// Resets all global state. Not very elegant.
void directiver_reset(void) {
	new_path = NULL;
	line_diff = 0;
	current_file = NULL;

	macro_stack_size = macro_stack_cap = 0;
	free(macro_stacks);
	macro_stacks = NULL;

	for (unsigned i = 0; i < dep_size; i++)
		free(deps[i]);
	free(deps);
	dep_size = dep_cap = 0;
	deps = NULL;
}

void directiver_push_input(const char *path, int system) {
	const char *parent_path = current_file ? current_file->path : ".";
	struct input new_input = input_open(parent_path, path, system);

	if (!new_input.path)
		return;

	if (write_dependencies)
		ADD_ELEMENT(dep_size, dep_cap, deps) = strdup(new_input.path);

	current_file = ALLOC((struct tokenized_file) {
			.parent = current_file,
			.token_idx = 0,
			.tokens = tokenize_input(new_input.contents, new_input.path),
			.path = strdup(new_input.path),
		});
}

static struct token next(void);

static struct token next_from_stack(void) {
	if (current_file->token_idx == current_file->tokens.size) {
		if (current_file->parent) {
			token_list_free(&current_file->tokens);
			current_file = current_file->parent;
			return next();
		}

		return (struct token ) { .type = T_EOI };
	}

	return current_file->tokens.list[current_file->token_idx++];
}

static void push(struct token t) {
	if (current_file->pushed_idx > 2)
		ICE("Pushed too many directive tokens.");
	current_file->pushed[current_file->pushed_idx++] = t;
}

static struct token next(void) {
	struct token t;
	if (current_file->pushed_idx) {
		current_file->pushed_idx--;
		t = current_file->pushed[current_file->pushed_idx];
	} else {
		t = next_from_stack();
	}
	t.pos.line += line_diff;
	if (new_path)
		t.pos.path = new_path;
	return t;
}

static void directiver_define(void) {
	struct token name = next();

	struct define def = define_init(name.str);

	struct token t = next();
	if(t.type == T_LPAR && !t.whitespace) {
		int idx = 0;
		do {
			t = next();
			if(idx == 0 && t.type == T_RPAR) {
				def.func = 1;
				break;
			}

			if (t.type == T_ELLIPSIS) {
				t = next();
				EXPECT(&t, T_RPAR);
				def.vararg = 1;
				def.func = 1;
				break;
			}
			EXPECT(&t, T_IDENT);
			define_add_par(&def, t);

			t = next();
			if (t.type != T_RPAR)
				EXPECT(&t, T_COMMA);

			idx++;
		} while(t.type == T_COMMA);

		t = next();
	}

	while(!t.first_of_line) {
		define_add_def(&def, t);
		t = next();
	}

	push(t);

	define_map_add(def);
}

static struct token_list buffer;
static int buffer_pos;

static struct token buffer_next(void) {
	if (buffer_pos >= buffer.size)
		return (struct token) { .type = T_EOI, .str = { 0 } };
	struct token *t = buffer.list + buffer_pos;
	if (t->type == T_IDENT) {
		buffer_pos++;
		return (struct token) { .type = T_NUM, .str = sv_from_str("0") };
	} else {
		return buffer.list[buffer_pos++];
	}
}

struct result {
	int is_signed;
	union {
		intmax_t i;
		uintmax_t u;
	};
};

static struct result result_signed(intmax_t val) {
	return (struct result) { .is_signed = 1, .i = val };
}

static struct result result_unsigned(intmax_t val) {
	return (struct result) { .is_signed = 0, .u = val };
}

int result_is_zero(struct result result) {
	return result.is_signed ? (result.i == 0) : (result.u == 0);
}

void check_div_overflow(struct token *t,
						struct result lhs, struct result rhs) {
	if (result_is_zero(rhs))
		ERROR(t->pos, "Division by zero");
	if (lhs.is_signed && lhs.i == INTMAX_MIN && rhs.i == -1)
		ERROR(t->pos, "Division will overflow");
}

#define RESULT_UNARY(OP, EXPR) ((EXPR).is_signed				\
								? result_signed(OP (EXPR).i)	\
								: result_unsigned(OP (EXPR).u))

#define RESULT_BINARY(OP, LHS, RHS) ((LHS).is_signed					\
									 ? result_signed((LHS).i OP (RHS).i) \
									 : result_unsigned((LHS).u OP (RHS).u))

// Conditionals always returns signed integers.
#define RESULT_BINARY_COND(OP, LHS, RHS) ((LHS).is_signed				\
										  ? result_signed((LHS).i OP (RHS).i) \
										  : result_signed((LHS).u OP (RHS).u))

// TODO: Ensure no UB. Don't allow operators to overflow.
static struct result evaluate_expression(int prec, int evaluate) {
	struct result expr = result_signed(0);
	struct token t = buffer_next();

	if (t.type == T_ADD) {
		expr = evaluate_expression(PREFIX_PREC, evaluate);
	} else if (t.type == T_SUB) {
		struct result rhs = evaluate_expression(PREFIX_PREC, evaluate);
		expr = RESULT_UNARY(-, rhs);
	} else if (t.type == T_NOT) {
		struct result rhs = evaluate_expression(PREFIX_PREC, evaluate);
		expr = RESULT_UNARY(!, rhs);
	} else if (t.type == T_LPAR) {
		expr = evaluate_expression(0, evaluate);
		struct token rpar = buffer_next();
		if (rpar.type != T_RPAR)
			ERROR(rpar.pos, "Expected ), got %s", dbg_token_type(rpar.type));
	} else if (t.type == T_NUM) {
		struct constant c = constant_from_string(t.str);
		assert(c.type == CONSTANT_TYPE);
		if (type_is_floating(c.data_type))
			ERROR(t.pos, "Floating point arithmetic in the preprocessor is not allowed.");
		if (!type_is_integer(c.data_type))
			ERROR(t.pos, "Preprocessor variables must be of integer type.");
		if (is_signed(c.data_type->simple))
			expr = result_signed(c.int_d);
		else
			expr = result_unsigned(c.uint_d);
	} else if (t.type == T_CHARACTER_CONSTANT) {
		expr = result_signed(escaped_character_constant_to_int(t));
	} else {
		ERROR(t.pos, "Invalid token in preprocessor expression. %s", dbg_token(&t));
	}

	t = buffer_next();

	while (prec < precedence_get(t.type, 1)) {
		int new_prec = precedence_get(t.type, 0);

		if (t.type == T_QUEST) {
			struct result mid = evaluate_expression(0, !result_is_zero(expr) ? evaluate : 0);
			struct token colon = buffer_next();
			assert(colon.type == T_COLON);
			struct result rhs = evaluate_expression(new_prec, result_is_zero(expr) ? evaluate : 0);
			expr = !result_is_zero(expr) ? mid : rhs;
		} else if (t.type == T_AND) {
			struct result rhs = evaluate_expression(new_prec, !result_is_zero(expr) ? evaluate : 0);
			expr = result_signed(!result_is_zero(expr) && !result_is_zero(rhs));
		} else if (t.type == T_OR) {
			struct result rhs = evaluate_expression(new_prec, result_is_zero(expr) ? evaluate : 0);
			expr = result_signed(!result_is_zero(expr) || !result_is_zero(rhs));
		} else {
			// Standard binary operator.
			struct result rhs = evaluate_expression(new_prec, evaluate);

			// Integer -> Unsigned integer promotion.
			if (rhs.is_signed && !expr.is_signed)
				rhs = result_unsigned(rhs.i);
			else if (!rhs.is_signed && expr.is_signed)
				expr = result_unsigned(expr.i);

			if (evaluate) {
				switch (t.type) {
				case T_BOR: expr = RESULT_BINARY(|, expr, rhs); break;
				case T_XOR: expr = RESULT_BINARY(^, expr, rhs); break;
				case T_AMP: expr = RESULT_BINARY(&, expr, rhs); break;
				case T_EQ: expr = RESULT_BINARY_COND(==, expr, rhs); break;
				case T_NEQ: expr = RESULT_BINARY_COND(!=, expr, rhs); break;
				case T_LEQ: expr = RESULT_BINARY_COND(<=, expr, rhs); break;
				case T_GEQ: expr = RESULT_BINARY_COND(>=, expr, rhs); break;
				case T_L: expr = RESULT_BINARY_COND(<, expr, rhs); break;
				case T_G: expr = RESULT_BINARY_COND(>, expr, rhs); break;
				case T_LSHIFT: expr = RESULT_BINARY(<<, expr, rhs); break;
				case T_RSHIFT: expr = RESULT_BINARY(>>, expr, rhs); break;
				case T_ADD: expr = RESULT_BINARY(+, expr, rhs); break;
				case T_SUB: expr = RESULT_BINARY(-, expr, rhs); break;
				case T_STAR: expr = RESULT_BINARY(*, expr, rhs); break;
				case T_DIV:
					check_div_overflow(&t, expr, rhs);
					expr = RESULT_BINARY(/, expr, rhs);
					break;
				case T_MOD:
					check_div_overflow(&t, expr, rhs);
					expr = RESULT_BINARY(%, expr, rhs);
					break;
				default:
					ERROR(t.pos, "Invalid infix %s", dbg_token(&t));
				}
			}
		}

		t = buffer_next();
	}

	if (buffer_pos == 0)
		ICE("Buffer should not be empty.");
	buffer.list[--buffer_pos] = t;

	return expr;
}

static struct result evaluate_until_newline(void) {
	buffer.size = 0;
	struct token t = next();
	while (!t.first_of_line) {
		if (sv_string_cmp(t.str, "defined")) {
			t = next();
			int has_lpar = t.type == T_LPAR;
			if (has_lpar)
				t = next();

			int is_defined = define_map_get(t.str) != NULL;
			token_list_add(&buffer, (struct token) {.type = T_NUM, .str = is_defined ? sv_from_str("1") :
					sv_from_str("0")});

			t = next();

			if (has_lpar) {
				EXPECT(&t, T_RPAR);
				t = next();
			}
		} else {
			token_list_add(&buffer, t);

			t = next();
		}
	}
	push(t);

	expand_token_list(&buffer);
	token_list_add(&buffer, (struct token) { .type = T_EOI });

	buffer_pos = 0;
	return evaluate_expression(0, 1);
}

static struct string_view get_include_path(struct token dir, struct token t, int *system) {
	buffer.size = 0;
	while (!t.first_of_line) {
		token_list_add(&buffer, t);
		t = next();
	}
	push(t);

	expand_token_list(&buffer);

	if (buffer.size != 1 || buffer.list[0].type != T_STRING)
		ERROR(dir.pos, "Invalidly formatted path to #include directive.");

	*system = 0;

	struct string_view path = buffer.list[0].str;
	path.len -= 2;
	path.str++;

	buffer_pos = 0;
	return path;
}

static int directiver_evaluate_conditional(struct token dir) {
	if (sv_string_cmp(dir.str, "ifdef")) {
		return (define_map_get(next().str) != NULL);
	} else if (sv_string_cmp(dir.str, "ifndef")) {
		return !(define_map_get(next().str) != NULL);
	} else if (sv_string_cmp(dir.str, "if") ||
			   sv_string_cmp(dir.str, "elif")) {
		return !result_is_zero(evaluate_until_newline());
	} else if (sv_string_cmp(dir.str, "else")) {
		return 1;
	}

	ERROR(dir.pos, "Invalid conditional directive");
}

static void push_macro(struct string_view name) {
	struct macro_stack *stack = NULL;
	for (unsigned i = 0; i < macro_stack_size; i++) {
		if (macro_stacks[i].size &&
			sv_cmp(macro_stacks[i].defines[0].name, name)) {
			stack = &macro_stacks[i];
		}
	}

	if (!stack) {
		stack = &ADD_ELEMENT(macro_stack_size, macro_stack_cap, macro_stacks);
		*stack = (struct macro_stack) { 0 };
	}

	struct define *current_define = define_map_get(name);
	if (current_define)
		ADD_ELEMENT(stack->size, stack->cap, stack->defines) = *current_define;
}

static void pop_macro(struct string_view name) {
	struct macro_stack *stack = NULL;
	for (unsigned i = 0; i < macro_stack_size; i++) {
		if (macro_stacks[i].size &&
			sv_cmp(macro_stacks[i].defines[0].name, name)) {
			stack = &macro_stacks[i];
		}
	}

	if (!stack)
		return;

	define_map_add(stack->defines[--stack->size]);

	if (!stack->size)
		REMOVE_ELEMENT(macro_stack_size, macro_stacks, stack - macro_stacks);
}

static int directiver_handle_pragma(void) {
	struct token command = next();

	if (sv_string_cmp(command.str, "once")) {
		input_disable_path(current_file->path);
	} else if (sv_string_cmp(command.str, "push_macro")) {
		struct token lpar = next();
		if (lpar.type != T_LPAR)
			ERROR(lpar.pos, "Expected (, got %s", dbg_token_type(lpar.type));
		struct token name_token = next();
		if (name_token.type != T_STRING)
			ERROR(name_token.pos, "Expected string got %s", dbg_token_type(lpar.type));
		struct token rpar = next();
		if (rpar.type != T_RPAR)
			ERROR(rpar.pos, "Expected ), got %s", dbg_token_type(lpar.type));

		struct string_view name = name_token.str;
		name.str++;
		name.len -= 2;

		push_macro(name);
	} else if (sv_string_cmp(command.str, "pop_macro")) {
		struct token lpar = next();
		if (lpar.type != T_LPAR)
			ERROR(lpar.pos, "Expected (, got %s", dbg_token_type(lpar.type));
		struct token name_token = next();
		if (name_token.type != T_STRING)
			ERROR(name_token.pos, "Expected string got %s", dbg_token_type(lpar.type));
		struct token rpar = next();
		if (rpar.type != T_RPAR)
			ERROR(rpar.pos, "Expected ), got %s", dbg_token_type(lpar.type));

		struct string_view name = name_token.str;
		name.str++;
		name.len -= 2;

		pop_macro(name);
	} else {
		push(command);
		return 1;
	}

	return 0;
}

struct token directiver_next(void) {
	static int cond_stack_n = 0, cond_stack_cap = 0;
	static int *cond_stack = NULL;

	if (cond_stack_n == 0)
		ADD_ELEMENT(cond_stack_n, cond_stack_cap, cond_stack) = 1;

	struct token t = next();
	int pass_directive = 0;
	while ((t.type == PP_DIRECTIVE || cond_stack[cond_stack_n - 1] != 1) &&
		!pass_directive) {
		if (t.type != PP_DIRECTIVE) {
			t = next();
			continue;
		}
		struct token directive = next();

		if (directive.first_of_line) {
			t = directive;
			continue;
		}

		if (directive.type != T_IDENT &&
			cond_stack[cond_stack_n - 1] != 1) {
			t = directive;
			continue;
		}

		struct string_view name = directive.str;

		assert(directive.type == T_IDENT);

		if (sv_string_cmp(name, "ifndef") ||
			sv_string_cmp(name, "ifdef") ||
			sv_string_cmp(name, "if")) {
			if (cond_stack[cond_stack_n - 1] == 1) {
				int result = directiver_evaluate_conditional(directive);
				ADD_ELEMENT(cond_stack_n, cond_stack_cap, cond_stack) = result ? 1 : 0;
			} else {
				ADD_ELEMENT(cond_stack_n, cond_stack_cap, cond_stack) = -1;
			}
		} else if (sv_string_cmp(name, "elif") ||
				   sv_string_cmp(name, "else")) {
			if (cond_stack[cond_stack_n - 1] == 0) {
				int result = directiver_evaluate_conditional(directive);
				cond_stack[cond_stack_n - 1] = result;
			} else {
				cond_stack[cond_stack_n - 1] = -1;
			}
		} else if (sv_string_cmp(name, "endif")) {
			cond_stack_n--;
		} else if (cond_stack[cond_stack_n - 1] == 1) {
			if (sv_string_cmp(name, "define")) {
				directiver_define();
			} else if (sv_string_cmp(name, "undef")) {
				define_map_remove(next().str);
			} else if (sv_string_cmp(name, "error")) {
				struct token msg = next();
				if (msg.type == T_STRING)
					ERROR(directive.pos, "#error directive was invoked with message: \"%.*s\".", msg.str.len, msg.str.str);
				else
					ERROR(directive.pos, "#error directive was invoked.");
			} else if (sv_string_cmp(name, "include")) {
				// There is an issue with just resetting after include. But
				// I'm interpreting the standard liberally to allow for this.
				new_path = NULL;
				line_diff = 0;
				struct string_view path;
				struct token path_tok = next();
				int system;
				if (path_tok.type == PP_HEADER_NAME_H ||
					path_tok.type == PP_HEADER_NAME_Q) {
					path = path_tok.str;
					system = path_tok.type == PP_HEADER_NAME_H;
					path.len -= 2;
					path.str++;
				} else {
					path = get_include_path(directive, path_tok, &system);
				}

				directiver_push_input(sv_to_str(path), system);
			} else if (sv_string_cmp(name, "endif")) {
				// Do nothing.
			} else if (sv_string_cmp(name, "pragma")) {
				if (directiver_handle_pragma()) {
					push(directive);
					push(t);

					pass_directive = 1;
				}
			} else if (sv_string_cmp(name, "line")) {
				// 6.10.4
				struct token digit_seq = next(), s_char_seq;

				if (digit_seq.first_of_line)
					ERROR(digit_seq.pos, "Expected digit sequence after #line");

				int has_s_char_seq = 0;
				if (digit_seq.type != T_NUM) {
					buffer.size = 0;
					struct token t = digit_seq;
					while (!t.first_of_line) {
						token_list_add(&buffer, t);
						t = next();
					}

					push(t);

					expand_token_list(&buffer);

					if (buffer.size == 0) {
						ERROR(digit_seq.pos, "Invalid #line macro expansion");
					} else if (buffer.size >= 1) {
						digit_seq = buffer.list[0];
					} else if (buffer.size >= 2) {
						s_char_seq = buffer.list[1];
						has_s_char_seq = 1;
					}
				} else {
					s_char_seq = next();
					if (s_char_seq.first_of_line) {
						push(s_char_seq);
					} else {
						has_s_char_seq = 1;
					}
				}

				if (digit_seq.first_of_line || digit_seq.type != T_NUM)
					ERROR(digit_seq.pos, "Expected digit sequence after #line");

				line_diff += atoi(sv_to_str(digit_seq.str)) - directive.pos.line - 1;

				if (has_s_char_seq) {
					if (s_char_seq.type != T_STRING)
						ERROR(s_char_seq.pos, "Expected s char sequence as second argument to #line");
					s_char_seq.str.len -= 2;
					s_char_seq.str.str++;
					new_path = sv_to_str(s_char_seq.str);
				}
			} else {
				ERROR(directive.pos, "#%s not implemented", dbg_token(&directive));
			}
		}

		t = next();
	}

	return t;
}
