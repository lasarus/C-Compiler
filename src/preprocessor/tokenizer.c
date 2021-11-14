#include "tokenizer.h"
#include "input.h"

#include <common.h>

#include <string.h>
#include <limits.h>

static struct input *input;

#define C0 (input->c[0])
#define C1 (input->c[1])
#define C2 (input->c[2])
#define CNEXT() input_next(input)

enum {
	C_DIGIT = 0x1,
	C_OCTAL_DIGIT = 0x2,
	C_HEXADECIMAL_DIGIT = 0x4,
	C_SPACE = 0x10,
	C_IDENTIFIER_NONDIGIT = 0x20,
};

// Sorry for this hardcoded table.
// These are the constants above, and ored together to build a lookup table.
// E.g.: char_props['c'] = C_HEXADECIMAL_DIGIT | C_IDENTIFIER_NONDIGIT
static const unsigned char char_props[UCHAR_MAX] = {
	['\t'] = 0x10, 0x10, 0x10, 0x10, 
	[' '] = 0x10, ['$'] = 0x20, 
	['_'] = 0x20, 
	['0'] = 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x5, 0x5, 
	['A'] = 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
	['a'] = 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};

#define HAS_PROP(C, PROP) (char_props[(unsigned char)(C)] & (PROP))

void tokenizer_push_input(const char *path, int system) {
	input_open(&input, path, system); // <- TODO: System.
}

void tokenizer_disable_current_path(void) {
	input_disable_path(input);
}

static void flush_whitespace(int *whitespace, int *first_of_line) {
	for (;;) {
		if (HAS_PROP(C0, C_SPACE)) {
			if(C0 == '\n')
				*first_of_line = 1;

			CNEXT();
		} else if (C0 == '/' && C1 == '*') {
			while (!(C0 == '*' && C1 == '/')) {
				CNEXT();

				if (C0 == '\0')
					ERROR(input->pos[0], "Comment reached end of file");
			}
			CNEXT();
			CNEXT();
		} else if (C0 == '/' && C1 == '/') {
			while (!(C0 == '\n')) {
				CNEXT();

				if (C0 == '\0')
					ERROR(input->pos[0], "Comment reached end of file");
			}
		} else {
			break;
		}
		*whitespace = 1;
	}
}

static char *buffer = NULL;
static size_t buffer_size = 0, buffer_cap = 0;

static void buffer_start(void) {
	buffer_size = 0;
}

static void buffer_eat() {
	ADD_ELEMENT(buffer_size, buffer_cap, buffer) = C0;
	CNEXT();
}

static struct string_view buffer_get(void) {
	struct string_view ret = { .len = buffer_size };
	ret.str = malloc(buffer_size);
	memcpy(ret.str, buffer, buffer_size);
	return ret;
}

static int parse_identifier(struct token *next) {
	if (!HAS_PROP(C0, C_IDENTIFIER_NONDIGIT))
		return 0;

	buffer_start();

	while(HAS_PROP(C0, C_IDENTIFIER_NONDIGIT | C_DIGIT))
		buffer_eat();

	next->type = T_IDENT;
	next->str = buffer_get();
	return 1;
}

static int parse_pp_number(struct token *next) {
	if (!(HAS_PROP(C0, C_DIGIT) ||
		  (C0 == '.' && HAS_PROP(C1, C_DIGIT))))
		return 0;

	buffer_start();

	for (;;) {
		if ((C0 == 'e' || C0 == 'E' ||
			 C0 == 'p' || C0 == 'P') &&
			(C1 == '+' || C1 == '-')) {
			buffer_eat();
			buffer_eat();
		} else if (HAS_PROP(C0, C_DIGIT | C_IDENTIFIER_NONDIGIT) || C0 == '.') {
			buffer_eat();
		} else {
			break;
		}
	}

	next->type = T_NUM;
	next->str = buffer_get();
	return 1;
}

int parse_escape_sequence(struct string_view *string, uint32_t *character, struct position pos) {
	if (string->len == 0 || string->str[0] != '\\')
		return 0;

	sv_tail(string, 1);

	if (HAS_PROP(string->str[0], C_OCTAL_DIGIT)) {
		int result = 0;
		for (int i = 0; i < 3 && string->len &&
				 HAS_PROP(string->str[0], C_OCTAL_DIGIT); i++) {
			result *= 8;
			result += string->str[0] - '0';
			sv_tail(string, 1);
		}

		*character = result;

		return 1;
	} else if (string->str[0] == 'x') {
		sv_tail(string, 1);
		int result = 0;

		while (string->len && HAS_PROP(string->str[0], C_HEXADECIMAL_DIGIT)) {
			result *= 16;
			char digit = string->str[0];
			if (HAS_PROP(digit, C_DIGIT)) {
				result += digit - '0';
			} else if (digit >= 'a' && digit <= 'f') {
				result += digit - 'a' + 10;
			} else if (digit >= 'A' && digit <= 'F') {
				result += digit - 'A' + 10;
			}

			sv_tail(string, 1);
		}

		*character = result;
		return 1;
	}

	switch (string->str[0]) {
	case '\'': *character = '\''; break;
	case '\"': *character = '\"'; break;
	case '\?': *character = '?'; break; // Trigraphs are stupid.
	case '\\': *character = '\\'; break;
	case 'a': *character = '\a'; break;
	case 'b': *character = '\b'; break;
	case 'f': *character = '\f'; break;
	case 'n': *character = '\n'; break;
	case 'r': *character = '\r'; break;
	case 't': *character = '\t'; break;
	case 'v': *character = '\v'; break;

	default:
		ERROR(pos, "Invalid escape sequence \\%c", string->str[0]);
	}

	sv_tail(string, 1);

	return 1;
}

static int eat_cs_char(char end_char) {
	if (C0 == '\n' || C0 == end_char)
		return 0;

	if (C0 == '\\')
		buffer_eat();
	buffer_eat();

	return 1;
}

struct string_view eat_string_like() {
	char end_char = C0 == '<' ? '>' : C0;

	buffer_eat();

	while (eat_cs_char(end_char));

	if (C0 != end_char) {
		char output[5];
		character_to_escape_sequence(C0, output, 1);
		ERROR(input->pos[0], "Expected '%c', got '%s', while parsing \"%.*s\"", end_char, output,
			  (int)buffer_size, buffer);
	}

	buffer_eat();

	return buffer_get();
}

static int parse_string(struct token *next) {
	buffer_start();
	if (C0 == 'u' &&
		C1 == '8' &&
		C2 == '"') {
		buffer_eat();
		buffer_eat();
	} else if (C0 == 'u' && C1 == '"') {
		buffer_eat();
	} else if (C0 == 'U' && C1 == '"') {
		buffer_eat();
	} else if (C0 == 'L' && C1 == '"') {
		buffer_eat();
	} else if (C0 != '"') {
		return 0;
	}

	next->type = T_STRING;
	next->str = eat_string_like();

	return 1;
}

static int parse_pp_header_name(struct token *next) {
	buffer_start();
	if (C0 != '"' && C0 != '<')
		return 0;

	next->type = C0 == '<' ? PP_HEADER_NAME_H : PP_HEADER_NAME_Q;
	next->str = eat_string_like();

	return 1;
}

static int parse_character_constant(struct token *next) {
	// All of these are handled in the same way.
	buffer_start();
	if (C0 == 'u' && C1 == '\'') {
		buffer_eat();
	} else if (C0 == 'U' && C1 == '\'') {
		buffer_eat();
	} else if (C0 == 'L' && C1 == '\'') {
		buffer_eat();
	} else if (C0 != '\'') {
		return 0;
	}

	next->type = T_CHARACTER_CONSTANT;
	next->str = eat_string_like();

	return 1;
}

static int parse_punctuator(struct token *next) {
	int count = 0;
#define SYM(A, B) else if (						\
		(sizeof(B) <= 1 || B[0] == C0) &&		\
		(sizeof(B) <= 2 || B[1] == C1) &&		\
		(sizeof(B) <= 3 || B[2] == C2)) {		\
		count = sizeof(B) - 1;					\
		next->type = A;							\
	}
#define KEY(A, B)
#define X(A, B)

	if (0) {}
	#include "tokens.h"
	else
		return 0;

	buffer_start();

	for (int i = 0; i < count; i++)
		buffer_eat();

	next->str = buffer_get();

	return 1;
}

static int is_header, is_directive;

struct token tokenizer_next(void) {
	struct token next = { 0 };

	flush_whitespace(&next.whitespace,
					 &next.first_of_line);

	if (next.first_of_line)
		is_header = is_directive = 0;

#define IFSTR(S, TOK)	(C0 == S[0] &&						\
						 (sizeof(S) == 2 || C1 == S[1])) &&	\
		(next.type = TOK, next.str = sv_from_str(S),		\
		 CNEXT(), (sizeof(S) == 3) && (CNEXT(), 1), 1)		\

	next.pos = input->pos[0];

	if(IFSTR("##", PP_HHASH)) {
	} else if(next.first_of_line && IFSTR("#", PP_DIRECTIVE)) {
		is_directive = 1;
	} else if(IFSTR("#", PP_HASH)) {
	} else if (is_header && parse_pp_header_name(&next)) {
	} else if(parse_string(&next)) {
	} else if(parse_character_constant(&next)) {
	} else if(parse_identifier(&next)) {
		if (is_directive) {
			is_header = sv_string_cmp(next.str, "include");
			is_directive = 0;
		}
	} else if(parse_punctuator(&next)) {
	} else if(parse_pp_number(&next)) {
	} else if(C0 == '\0') {
		if (input->next) {
			// Retry on popped source.
			input_close(&input);
			return tokenizer_next();
		}

		next.first_of_line = 1;
		next.type = T_EOI;
	} else {
		ERROR(next.pos, "Unrecognized preprocessing token! Starting with '%c', %d\n", C0, C0);
	}
#undef IFSTR

	return next;
}

struct token_list tokenizer_whole(struct input *new_input) {
	struct input *prev_input = input;
	input = new_input;

	struct token_list tl = { 0 };

	struct token t = tokenizer_next();
	while (t.type != T_EOI) {
		token_list_add(&tl, t);
		t = tokenizer_next();
	}

	input = prev_input;

	return tl;
}
