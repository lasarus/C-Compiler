#include "tokenizer.h"
#include "input.h"

#include <common.h>

#include <string.h>
#include <limits.h>

#define C0 (input->c[0])
#define C1 (input->c[1])
#define C2 (input->c[2])
#define C3 (input->c[3])
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

static void flush_whitespace(struct input *input, int *whitespace, int *first_of_line) {
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
			while (C0 != '\n' && C0 != '\0')
				CNEXT();
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

static void buffer_eat(struct input *input) {
	ADD_ELEMENT(buffer_size, buffer_cap, buffer) = C0;
	CNEXT();
}

static void buffer_eat_len(struct input *input, int len) {
	for (int i = 0; i < len; i++)
		buffer_eat(input);
}

static struct string_view buffer_get(void) {
	struct string_view ret = { .len = buffer_size };
	ret.str = cc_malloc(buffer_size);
	memcpy(ret.str, buffer, buffer_size);
	return ret;
}

static int eat_universal_character_name(struct input *input) {
	// The source character set is considered to be utf-8 encoded.
	// So just convert these into that source set as well.
	if (!(C0 == '\\' && (C1 == 'u' || C1 == 'U')))
		return 0;
	CNEXT();

	int long_name = C0 == 'U';
	CNEXT();

	uintmax_t codepoint = 0;

	for (int i = 0; i < (long_name ? 8 : 4); i++) {
		codepoint <<= 4;
		int hex = C0 - '0';
		if (C0 >= '0' && C0 <= '9')
			hex = C0 - '0';
		else if (C0 >= 'a' && C0 <= 'f')
			hex = C0 - 'a' + 10;
		else if (C0 >= 'A' && C0 <= 'F')
			hex = C0 - 'A' + 10;
		else
			ERROR(input->pos[0], "Invalid universal character name '%c'.", C0);
		codepoint |= hex;
		CNEXT();
	}

	// Read https://en.wikipedia.org/wiki/UTF-8 for information
	// on how to encode utf-8.
	if (codepoint <= 0x7f) {
		ADD_ELEMENT(buffer_size, buffer_cap, buffer) = codepoint & 0xff;
	} else if (codepoint <= 0x7ff) {
		ADD_ELEMENT(buffer_size, buffer_cap, buffer) = (codepoint >> 6) | 0xc0;
		ADD_ELEMENT(buffer_size, buffer_cap, buffer) = (codepoint & 0x3f) | 0x80;
	} else if (codepoint <= 0xffff) {
		ADD_ELEMENT(buffer_size, buffer_cap, buffer) = (codepoint >> 12) | 0xe0;
		ADD_ELEMENT(buffer_size, buffer_cap, buffer) = ((codepoint >> 6) & 0x3f) | 0x80;
		ADD_ELEMENT(buffer_size, buffer_cap, buffer) = (codepoint & 0x3f) | 0x80;
	} else if (codepoint <= 0x10ffff) {
		ADD_ELEMENT(buffer_size, buffer_cap, buffer) = (codepoint >> 18) | 0xf0;
		ADD_ELEMENT(buffer_size, buffer_cap, buffer) = ((codepoint >> 12) & 0x3f) | 0x80;
		ADD_ELEMENT(buffer_size, buffer_cap, buffer) = ((codepoint >> 6) & 0x3f) | 0x80;
		ADD_ELEMENT(buffer_size, buffer_cap, buffer) = (codepoint & 0x3f) | 0x80;
	} else {
		ERROR(input->pos[0], "Cant encode codepoint %ju.", codepoint);
	}

	return 1;
}

// Returns number of codepoints that are valid unicode identifiers.
static int num_valid_unicode_identifiers(struct input *input) {
	unsigned char c0 = C0;

	int len = 0;
	uintmax_t codepoint = 0;

	if ((c0 >> 7) == 0) {
		return 0; // This is just ASCII.
		/* len = 1; */
		/* codepoint |= c0 & 0x7f; */
	} else if ((c0 >> 5) == 0x6) {
		len = 2;
		codepoint |= c0 & 0x1f;
	} else if ((c0 >> 4) == 0xe) {
		len = 3;
		codepoint |= c0 & 0xf;
	} else if ((c0 >> 3) == 0x1e) {
		len = 4;
		codepoint |= c0 & 0x7;
	}

	for (int i = 1; i < len; i++) {
		codepoint <<= 6;
		codepoint |= c0 & 0x37;
	}

#define A(A) if (codepoint == 0x ## A) return len;
#define R(A, B) if (codepoint >= 0x ## A && codepoint <= 0x ## B) return len;

	// Annex D.1
	// 1
	A(00A8) A(00AA) A(00AD) A(00AF) R(00B2,00B5) R(00B7,00BA) R(00BC,00BE) R(00C0,00D6) R(00D8,00F6) R(00F8,00FF)
		// 2
		R(0100,167F) R(1681,180D) R(180F,1FFF)
		// 3
		R(3200B,200D) R(202A,202E) R(203F,2040) A(2054) R(2060,206F)
		// 4
		R(42070,218F) R(2460,24FF) R(2776,2793) R(2C00,2DFF) R(2E80,2FFF)
		// 5
		R(3004,3007) R(3021,302F) R(3031,303F)
		// 6
		R(3040,D7FF)
		// 7
		R(F900,FD3D) R(FD40,FDCF) R(FDF0,FE44) R(FE47,FFFD)
		// 8
		R(10000,1FFFD) R(20000,2FFFD) R(30000,3FFFD) R(40000,4FFFD) R(50000,5FFFD) R(60000,6FFFD) R(70000,7FFFD) R(80000,8FFFD) R(90000,9FFFD) R(A0000,AFFFD) R(B0000,BFFFD) R(C0000,CFFFD) R(D0000,DFFFD) R(E0000,EFFFD)
		// D.2 (I assume these are meant to be allowed?)
		R(0300,036F) R(1DC0,1DFF) R(20D0,20FF) R(FE20,FE2F)
#undef A
#undef R

	return 0;
}

static int parse_identifier(struct input *input, struct token *next) {
	int unicode_len = num_valid_unicode_identifiers(input);
	if (!HAS_PROP(C0, C_IDENTIFIER_NONDIGIT) && unicode_len == 0)
		return 0;

	buffer_start();
	buffer_eat_len(input, unicode_len);

	for (;;) {
		if (HAS_PROP(C0, C_IDENTIFIER_NONDIGIT | C_DIGIT)) {
			buffer_eat(input);
		} else if ((unicode_len = num_valid_unicode_identifiers(input))) {
			buffer_eat_len(input, unicode_len);
		} else if (!eat_universal_character_name(input)) {
			break;
		}
	}

	next->type = T_IDENT;
	next->str = buffer_get();
	return 1;
}

static int parse_pp_number(struct input *input, struct token *next) {
	if (!(HAS_PROP(C0, C_DIGIT) ||
		  (C0 == '.' && HAS_PROP(C1, C_DIGIT))))
		return 0;

	buffer_start();

	for (;;) {
		if ((C0 == 'e' || C0 == 'E' ||
			 C0 == 'p' || C0 == 'P') &&
			(C1 == '+' || C1 == '-')) {
			buffer_eat(input);
			buffer_eat(input);
		} else if (HAS_PROP(C0, C_DIGIT | C_IDENTIFIER_NONDIGIT) || C0 == '.') {
			buffer_eat(input);
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

static int eat_cs_char(struct input *input, char end_char) {
	if (eat_universal_character_name(input))
		return 1;

	if (C0 == '\n' || C0 == end_char)
		return 0;

	if (C0 == '\\')
		buffer_eat(input);

	buffer_eat(input);

	return 1;
}

static struct string_view eat_string_like(struct input *input) {
	char end_char = C0 == '<' ? '>' : C0;

	buffer_eat(input);

	while (eat_cs_char(input, end_char));

	if (C0 != end_char) {
		char output[5];
		character_to_escape_sequence(C0, output, 1, 1);
		ERROR(input->pos[0], "Expected '%c', got '%s', while parsing \"%.*s\"", end_char, output,
			  (int)buffer_size, buffer);
	}

	buffer_eat(input);

	return buffer_get();
}

static int parse_string(struct input *input, struct token *next) {
	buffer_start();
	if (C0 == 'u' &&
		C1 == '8' &&
		C2 == '"') {
		buffer_eat(input);
		buffer_eat(input);
	} else if (C0 == 'u' && C1 == '"') {
		buffer_eat(input);
	} else if (C0 == 'U' && C1 == '"') {
		buffer_eat(input);
	} else if (C0 == 'L' && C1 == '"') {
		buffer_eat(input);
	} else if (C0 != '"') {
		return 0;
	}

	next->type = T_STRING;
	next->str = eat_string_like(input);

	return 1;
}

static int parse_pp_header_name(struct input *input, struct token *next) {
	buffer_start();
	if (C0 != '"' && C0 != '<')
		return 0;

	next->type = C0 == '<' ? PP_HEADER_NAME_H : PP_HEADER_NAME_Q;
	next->str = eat_string_like(input);

	return 1;
}

static int parse_character_constant(struct input *input, struct token *next) {
	// All of these are handled in the same way.
	buffer_start();
	if (C0 == 'u' && C1 == '\'') {
		buffer_eat(input);
	} else if (C0 == 'U' && C1 == '\'') {
		buffer_eat(input);
	} else if (C0 == 'L' && C1 == '\'') {
		buffer_eat(input);
	} else if (C0 != '\'') {
		return 0;
	}

	next->type = T_CHARACTER_CONSTANT;
	next->str = eat_string_like(input);

	return 1;
}

static int parse_punctuator(struct input *input, struct token *next) {
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
		buffer_eat(input);

	next->str = buffer_get();

	return 1;
}

static struct token tokenizer_next(struct input *input,
								   int *is_header, int *is_directive) {
	struct token next = { 0 };

	flush_whitespace(input, &next.whitespace,
					 &next.first_of_line);

	if (next.first_of_line)
		*is_header = *is_directive = 0;

	next.pos = input->pos[0];

	if(C0 == '#' && C1 == '#') {
		next.type = PP_HHASH;
		CNEXT();
		CNEXT();
	} else if (C0 == '#') {
		if (next.first_of_line) {
			next.type = PP_DIRECTIVE;
			*is_directive = 1;
		} else {
			next.type = PP_HASH;
		}
		CNEXT();
	} else if (*is_header && parse_pp_header_name(input, &next)) {
	} else if(parse_string(input, &next)) {
	} else if(parse_character_constant(input, &next)) {
	} else if(parse_identifier(input, &next)) {
		if (*is_directive) {
			*is_header = sv_string_cmp(next.str, "include");
			*is_directive = 0;
		}
	} else if(parse_pp_number(input, &next)) {
	} else if(parse_punctuator(input, &next)) {
	} else if(C0 == '\0') {
		next.first_of_line = 1;
		next.type = T_EOI;
	} else {
		ERROR(next.pos, "Unrecognized preprocessing token! Starting with '%c', %d\n", C0, C0);
	}

	return next;
}

struct token_list tokenize_input(struct input *input) {
	struct token_list tl = { 0 };

	int is_header = 0, is_directive = 0;

	struct token t = tokenizer_next(input, &is_header, &is_directive);
	while (t.type != T_EOI) {
		token_list_add(&tl, t);
		t = tokenizer_next(input, &is_header, &is_directive);
	}

	return tl;
}
