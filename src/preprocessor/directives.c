#include "directives.h"
#include "macro_expander.h"
#include "tokenizer.h"
#include "splitter.h"

#include <common.h>
#include <precedence.h>
#include <arch/x64.h>

#include <assert.h>

void unescape(char *str) {
	char *escaped = str;
	int header = *str == '<';

	str++;

	for(;*str; str++) {
		if (header && *str == '>')
			break;
		else if (*str == '"')
			break;
		else if (*str == '\\') {
			str++;
			*(escaped++) = *str;
		} else {
			*(escaped++) = *str;
		}
	}
	*escaped = '\0';
}

struct token tmp(void);
void p(struct token t);

#define NEXT_U() splitter_next_unexpanded()

#define NEXT_E() splitter_next()
#define NEXT_T() splitter_next_translate()
#define PUSH(T) expander_push_front(T)

void directiver_define(void) {

	struct token name = NEXT_U();

	struct define def = define_init(name.str);

	struct token t = NEXT_U();
	if(t.type == PP_LPAR && !t.whitespace) {
		int idx = 0;
		do {
			t = NEXT_U();
			if(idx == 0 && t.type == PP_RPAR) {
				def.func = 1;
				break;
			}

			if (t.type == PP_PUNCT &&
				strcmp(t.str, "...") == 0) {
				t = NEXT_U();
				ASSERT_TYPE(t, PP_RPAR);
				def.vararg = 1;
				def.func = 1;
				break;
			}
			ASSERT_TYPE(t, PP_IDENT);
			define_add_par(&def, token_move(&t));

			t = NEXT_U();
			ASSERT_TYPE2(t, PP_COMMA, PP_RPAR);

			idx++;
		} while(t.type == PP_COMMA);

		t = NEXT_U();
	}

	while(!t.first_of_line) {
		define_add_def(&def, token_move(&t));
		t = NEXT_U();
	}

	PUSH(t);

	define_map_add(def);
}

void directiver_undef(void) {
	struct token name = NEXT_U();

	ASSERT_TYPE(name, PP_IDENT);
	define_map_remove(name.str);
}

intmax_t evaluate_expression(int prec) {
	intmax_t expr = 0;
	struct token t = NEXT_E();

	if (t.type == T_ADD) {
		expr = evaluate_expression(PREFIX_PREC);
	} else if (t.type == T_SUB) {
		expr = -evaluate_expression(PREFIX_PREC);
	} else if (t.type == T_NOT) {
		expr = !evaluate_expression(PREFIX_PREC);
	} else if (t.type == PP_IDENT &&
			   strcmp(t.str, "defined") == 0) {
		struct token name;
		struct token nt = NEXT_U();

		if (strcmp(nt.str, "(") == 0) {
			name = NEXT_U();
			struct token rpar = NEXT_U();
			if (strcmp(rpar.str, ")") != 0)
				ERROR("Expected )");
		} else {
			name = nt;
		}

		expr = (define_map_get(name.str) != NULL);
	} else if (t.type == PP_IDENT) {
		expr = 0;
	} else if (t.type == T_LPAR) {
		expr = evaluate_expression(0);
		struct token rpar = NEXT_E();
		if (rpar.type != T_RPAR) {
			PRINT_POS(rpar.pos);
			ERROR("Expected )");
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

	t = NEXT_E();

	while (prec < precedence_get(t.type, 1)) {
		int new_prec = precedence_get(t.type, 0);

		if (t.type == T_QUEST) {
			int mid = evaluate_expression(0);
			struct token colon = NEXT_E();
			assert(colon.type == T_COLON);
			int rhs = evaluate_expression(new_prec);
			expr = expr ? mid : rhs;
		} else {
			// Standard binary operator.
			int rhs = evaluate_expression(new_prec);
			switch (t.type) {
			case T_AND: expr = expr && rhs; break;
			case T_OR: expr = expr || rhs; break;
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

		t = NEXT_E();
	}

	PUSH(t);

	return expr;
}

int directiver_evaluate_conditional(struct token dir) {
	if (strcmp(dir.str, "ifdef") == 0) {
		return (define_map_get(NEXT_U().str) != NULL);
	} else if (strcmp(dir.str, "ifndef") == 0) {
		return !(define_map_get(NEXT_U().str) != NULL);
	} else if (strcmp(dir.str, "if") == 0 ||
			   strcmp(dir.str, "elif") == 0) {
		intmax_t eval = evaluate_expression(0);
		return eval;
	}

	ERROR("Invalid conditional directive");
}

void directiver_flush_if(void) {
	int nest_level = 1;
	while (nest_level > 0) {
		struct token t;
		do {
			t = NEXT_U();
			if (t.type == T_EOI)
				ERROR("Reading past end of file");
		} while (t.type != PP_DIRECTIVE);

		struct token dir = NEXT_U();
		char *name = dir.str;
		if (strcmp(name, "if") == 0 ||
			strcmp(name, "ifdef") == 0 ||
			strcmp(name, "ifndef") == 0) {
			nest_level++;
		} else if (strcmp(name, "elif") == 0) {
			if (nest_level == 1) {
				PUSH(t);
				PUSH(dir);
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
	struct token command = NEXT_U();

	if (strcmp(command.str, "once") == 0) {
		tokenizer_disable_current_path();
	} else {
		ERROR("\"#pragma %s\" not supported", command.str);
	}
}

struct token directiver_next(void) {
	struct token t = NEXT_T();
	if (t.type == PP_DIRECTIVE) {
		struct token directive = NEXT_U();

		char *name = directive.str;
		assert(directive.type == PP_IDENT);

		if (strcmp(name, "define") == 0) {
			directiver_define();
		} else if (strcmp(name, "undef") == 0) {
			define_map_remove(NEXT_U().str);
		} else if (strcmp(name, "error") == 0) {
			ERROR("#error directive was invoked on %s:%d", directive.pos.path, directive.pos.line);
		} else if (strcmp(name, "include") == 0) {
			set_header(1);
			struct token path_tok = NEXT_U();
			set_header(0);
			char *path = path_tok.str;
			unescape(path);
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
			struct token digit_seq = NEXT_E();
			if (digit_seq.first_of_line || digit_seq.type != T_NUM)
				ERROR("Expected digit sequence after #line");
			set_line(atoi(digit_seq.str));

			// TODO: Make this also use NEXT_E(), currently a bit buggy.
			struct token s_char_seq = NEXT_U();

			if (s_char_seq.first_of_line) {
				PUSH(s_char_seq);
			} else if (s_char_seq.type == PP_STRING) {
				set_filename(s_char_seq.str);
			} else {
				ERROR("Expected s char sequence as second argument to #line");
			}
		} else {
			ERROR("#%s not implemented", name);
		}
		return directiver_next();
	} else {
		return t;
	}
}
