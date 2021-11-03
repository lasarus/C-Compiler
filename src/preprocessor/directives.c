#include "directives.h"
#include "macro_expander.h"
#include "tokenizer.h"

#include <common.h>
#include <precedence.h>
#include <arch/x64.h>

#include <assert.h>

static int pushed_idx = 0;
static struct token pushed[2];

void push(struct token t) {
	if (pushed_idx > 1)
		ERROR("Internal compiler error");
	pushed[pushed_idx++] = t;
}

struct token next() {
	struct token t = pushed_idx ? pushed[--pushed_idx] : tokenizer_next();
	return t;
}

#define NEXT() next()
#define PUSH(T) push(T)

void directiver_define(void) {

	struct token name = NEXT();

	struct define def = define_init(name.str);

	struct token t = NEXT();
	if(t.type == PP_LPAR && !t.whitespace) {
		int idx = 0;
		do {
			t = NEXT();
			if(idx == 0 && t.type == PP_RPAR) {
				def.func = 1;
				break;
			}

			if (t.type == PP_PUNCT &&
				strcmp(t.str, "...") == 0) {
				t = NEXT();
				EXPECT(&t, PP_RPAR);
				def.vararg = 1;
				def.func = 1;
				break;
			}
			EXPECT(&t, PP_IDENT);
			define_add_par(&def, t);

			t = NEXT();
			if (t.type != PP_RPAR)
				EXPECT(&t, PP_COMMA);

			idx++;
		} while(t.type == PP_COMMA);

		t = NEXT();
	}

	while(!t.first_of_line) {
		define_add_def(&def, t);
		t = NEXT();
	}

	PUSH(t);

	define_map_add(def);
}

void directiver_undef(void) {
	struct token name = NEXT();

	EXPECT(&name, PP_IDENT);
	define_map_remove(name.str);
}

static enum ttype get_punct(char **in_str) {
	char *str = *in_str;

	enum ttype type;

#define SYM(A, B) else if(B[0] == *str && (sizeof(B) <= 2 || B[1] == *(str + 1)) && (sizeof(B) <= 3 || B[2] == *(str + 2))) { type = A; str += sizeof(B) - 1; }
#define KEY(A, B)
#define X(A, B)

	if(0) {
	}
#include "tokens.h"
	else {
		ERROR("Unrecognized punctuation: %s %d", str, *str);
	}

#undef SYM
#undef KEY
#undef X


	*in_str = str;
	return type;
}

static struct token_list buffer;
static int buffer_pos;

struct token buffer_next() {
	if (buffer_pos >= buffer.size)
		return (struct token) { .type = T_EOI, .str = "" };
	struct token *t = buffer.list + buffer_pos;
	if (t->type == PP_PUNCT) {
		enum ttype next_type = get_punct(&t->str);

		if (*t->str == '\0')
			buffer_pos++;

		return (struct token) { .type = next_type, .str = "" };
	} else if (t->type == PP_IDENT) {
		buffer_pos++;
		return (struct token) { .type = T_NUM, .str = "0" };
	} else if (t->type == PP_NUMBER) {
		buffer_pos++;
		return (struct token) { .type = T_NUM, .str = t->str };
	} else if (t->type == PP_LPAR) {
		buffer_pos++;
		return (struct token) { .type = T_LPAR };
	} else if (t->type == PP_RPAR) {
		buffer_pos++;
		return (struct token) { .type = T_RPAR };
	} else if (t->type == PP_COMMA) {
		buffer_pos++;
		return (struct token) { .type = T_COMMA };
	} else {
		return buffer.list[buffer_pos++];
	}
}

intmax_t evaluate_expression(int prec, int evaluate) {
	intmax_t expr = 0;
	struct token t = buffer_next();

	if (t.type == T_ADD) {
		expr = evaluate_expression(PREFIX_PREC, evaluate);
	} else if (t.type == T_SUB) {
		expr = -evaluate_expression(PREFIX_PREC, evaluate);
	} else if (t.type == T_NOT) {
		expr = !evaluate_expression(PREFIX_PREC, evaluate);
	} else if (t.type == T_LPAR) {
		expr = evaluate_expression(0, evaluate);
		struct token rpar = buffer_next();
		if (rpar.type != T_RPAR) {
			PRINT_POS(rpar.pos);
			ERROR("Expected ), got %s", dbg_token_type(rpar.type));
		}
	} else if (t.type == T_NUM) {
		struct constant c = constant_from_string(t.str);
		if (c.type != CONSTANT_TYPE)
			ERROR("Internal compiler error.");
		if (type_is_floating(c.data_type))
			ERROR("Floating point arithmetic in the preprocessor is not allowed.");
		if (!type_is_integer(c.data_type))
			ERROR("Preprocessor variables must be of integer type.");
		switch (c.data_type->simple) {
		case ST_INT:
			expr = c.int_d;
			break;
		case ST_UINT:
			expr = c.uint_d;
			break;
		case ST_LONG:
			expr = c.long_d;
			break;
		case ST_ULONG:
			expr = c.ulong_d;
			break;
		case ST_LLONG:
			expr = c.llong_d;
			break;
		case ST_ULLONG:
			expr = c.ullong_d;
			break;
		default: ERROR("Internal compiler error.");
		}
	} else if (t.type == PP_CHARACTER_CONSTANT) {
		expr = character_constant_to_int(t.str);
	} else {
		ERROR("Invalid token in preprocessor expression. %s, in %s:%d", dbg_token(&t), t.pos.path, t.pos.line);
	}

	t = buffer_next();

	while (prec < precedence_get(t.type, 1)) {
		int new_prec = precedence_get(t.type, 0);

		if (t.type == T_QUEST) {
			int mid = evaluate_expression(0, expr ? evaluate : 0);
			struct token colon = buffer_next();
			assert(colon.type == T_COLON);
			int rhs = evaluate_expression(new_prec, expr ? 0 : evaluate);
			expr = expr ? mid : rhs;
		} else if (t.type == T_AND) {
			int rhs = evaluate_expression(new_prec, expr ? evaluate : 0);
			expr = expr && rhs;
		} else if (t.type == T_OR) {
			int rhs = evaluate_expression(new_prec, expr ? 0 : evaluate);
			expr = expr || rhs;
		} else {
			// Standard binary operator.
			int rhs = evaluate_expression(new_prec, evaluate);
			if (evaluate) {
				switch (t.type) {
				case T_BOR: expr = expr | rhs; break;
				case T_XOR: expr = expr ^ rhs; break;
				case T_AMP: expr = expr & rhs; break;
				case T_EQ: expr = expr == rhs; break;
				case T_NEQ: expr = expr != rhs; break;
				case T_LEQ: expr = expr <= rhs; break;
				case T_GEQ: expr = expr >= rhs; break;
				case T_L: expr = expr < rhs; break;
				case T_G: expr = expr > rhs; break;
				case T_LSHIFT: expr = expr << rhs; break;
				case T_RSHIFT: expr = expr >> rhs; break;
				case T_ADD: expr = expr + rhs; break;
				case T_SUB: expr = expr - rhs; break;
				case T_STAR: expr = expr * rhs; break;
				case T_DIV:
					if (rhs)
						expr = expr / rhs;
					else
						ERROR("Division by zero");
					break;
				case T_MOD:
					if (rhs)
						expr = expr % rhs;
					else
						ERROR("Modulo by zero");
					break;
				default:
					ERROR("Invalid infix %s", dbg_token(&t));
				}
			}
		}

		t = buffer_next();
	}

	if (buffer_pos == 0)
		ERROR("Internal compiler error.");
	buffer.list[--buffer_pos] = t;
	// Push t.

	return expr;
}

intmax_t evaluate_until_newline() {
	buffer.size = 0;
	struct token t = NEXT();
	while (!t.first_of_line) {
		if (strcmp(t.str, "defined") == 0) {
			t = NEXT();
			int has_lpar = t.type == PP_LPAR;
			if (has_lpar)
				t = NEXT();

			int is_defined = define_map_get(t.str) != NULL;
			token_list_add(&buffer, (struct token) {.type = PP_NUMBER, .str = is_defined ? "1" : "0"});

			t = NEXT();

			if (has_lpar) {
				EXPECT(&t, PP_RPAR);
				t = NEXT();
			}
		} else {
			token_list_add(&buffer, t);

			t = NEXT();
		}
	}
	PUSH(t);

	expand_token_list(&buffer);
	token_list_add(&buffer, (struct token) { .type = T_EOI, .str = "" });

	buffer_pos = 0;
	intmax_t result = evaluate_expression(0, 1);

	return result;
}

int directiver_evaluate_conditional(struct token dir) {
	if (strcmp(dir.str, "ifdef") == 0) {
		return (define_map_get(NEXT().str) != NULL);
	} else if (strcmp(dir.str, "ifndef") == 0) {
		return !(define_map_get(NEXT().str) != NULL);
	} else if (strcmp(dir.str, "if") == 0 ||
			   strcmp(dir.str, "elif") == 0) {
		return evaluate_until_newline();
	}

	ERROR("Invalid conditional directive");
}

void directiver_flush_if(void) {
	int nest_level = 1;
	while (nest_level > 0) {
		struct token t;
		do {
			t = NEXT();
			if (t.type == T_EOI)
				ERROR("Reading past end of file");
		} while (t.type != PP_DIRECTIVE);

		struct token dir = NEXT();
		char *name = dir.str;
		if (strcmp(name, "if") == 0 ||
			strcmp(name, "ifdef") == 0 ||
			strcmp(name, "ifndef") == 0) {
			nest_level++;
		} else if (strcmp(name, "elif") == 0) {
			if (nest_level == 1) {
				PUSH(dir);
				PUSH(t);
				return;
			}
		} else if (strcmp(name, "else") == 0) {
			if (nest_level == 1) {
				return;
			}
		} else if (strcmp(name, "endif") == 0) {
			nest_level--;
		}
	}
}

void directiver_handle_pragma(void) {
	struct token command = NEXT();

	if (strcmp(command.str, "once") == 0) {
		tokenizer_disable_current_path();
	} else {
		ERROR("\"#pragma %s\" not supported", command.str);
	}
}

struct token directiver_next(void) {
	struct token t = NEXT();
	if (t.type == PP_DIRECTIVE) {
		struct token directive = NEXT();

		char *name = directive.str;
		assert(directive.type == PP_IDENT);

		if (strcmp(name, "define") == 0) {
			directiver_define();
		} else if (strcmp(name, "undef") == 0) {
			define_map_remove(NEXT().str);
		} else if (strcmp(name, "error") == 0) {
			ERROR("#error directive was invoked on %s:%d", directive.pos.path, directive.pos.line);
		} else if (strcmp(name, "include") == 0) {
			struct token path_tok = NEXT();
			char *path = path_tok.str;
			tokenizer_push_input(path);
		} else if (strcmp(name, "ifndef") == 0 ||
				   strcmp(name, "ifdef") == 0 ||
				   strcmp(name, "if") == 0 ||
				   strcmp(name, "elif") == 0) {
			int result = directiver_evaluate_conditional(directive);
			if (!result)
				directiver_flush_if();
		} else if (strcmp(name, "else") == 0) {
			directiver_flush_if();
		} else if (strcmp(name, "endif") == 0) {
			// Do nothing.
		} else if (strcmp(name, "pragma") == 0) {
			directiver_handle_pragma();
		} else if (strcmp(name, "line") == 0) {
			// 6.10.4
			struct token digit_seq = NEXT();
			if (digit_seq.first_of_line || digit_seq.type != PP_NUMBER)
				ERROR("Expected digit sequence after #line");
			set_line(atoi(digit_seq.str));

			// TODO: Make this also use NEXT_E(), currently a bit buggy.
			struct token s_char_seq = NEXT();

			if (s_char_seq.first_of_line) {
				PUSH(s_char_seq);
			} else if (s_char_seq.type == PP_STRING) {
				set_filename(s_char_seq.str);
			} else {
				ERROR("Expected s char sequence as second argument to #line");
			}
		} else {
			PRINT_POS(directive.pos);
			ERROR("#%s not implemented", name);
		}
		return directiver_next();
	} else {
		return t;
	}
}
