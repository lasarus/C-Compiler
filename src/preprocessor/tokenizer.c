#include "tokenizer.h"
#include "string_view.h"

#include <common.h>
#include <utf8.h>

#include <limits.h>

static char c;
static struct position pos;
static const char *str;
static int needs_escape_sequences_removed;

enum {
	EQ_UTF8 = 0,
	EQ_NULL = 1,
	EQ_EXPONENT = 3,
	EQ_DECIMAL = 5,
	EQ_IDENT = 6,
	EQ_SPACE = 7,
	EQ_ALPHA = 9,
};

static char eq_table[UCHAR_MAX + 1] = {
	1, 2,  2,  2,  2, 2,   2,  2,  2,  7,  10, 2,   2,   7,   2,   2,
	2, 2,  2,  2,  2, 2,   2,  2,  2,  2,  2,  2,   2,   2,   2,   2,
	7, 33, 34, 35, 9, 37,  38, 39, 40, 41, 42, 43,  44,  45,  46,  47,
	5, 5,  5,  5,  5, 5,   5,  5,  56, 5,  58, 59,  60,  61,  62,  63,
	2, 9,  9,  9,  9, 3,   9,  9,  9,  9,  9,  9,   76,  9,   9,   9,
	3, 9,  9,  9,  9, 85,  9,  9,  9,  9,  9,  91,  92,  93,  94,  9,
	2, 9,  9,  9,  9, 3,   9,  9,  9,  9,  9,  9,   9,   9,   9,   9,
	3, 9,  9,  9,  9, 117, 9,  9,  9,  9,  9,  123, 124, 125, 126, 2,
};

static void next_char(void) {
	// Remove "\\\n" and "\\\r\n".
	if (*str == '\\' &&
	    (str[1] == '\n' || (str[1] == '\r' && str[2] == '\n'))) {
		str += str[1] == '\n' ? 2 : 3;
		pos.line++;
		pos.column = 0;
		needs_escape_sequences_removed = 1;
		next_char();
		return;
	}

	if (*str == '\n') {
		pos.line++;
		pos.column = 0;
	}

	pos.column++;

	c = eq_table[(unsigned char)*str++];
}

static struct string_view remove_escape_sequences(const char *initial_pos) {
	size_t len = str - initial_pos - 1;
	char *ret_str = cc_malloc(len + 1);
	memcpy(ret_str, initial_pos, len);
	ret_str[len] = '\0';

	char *read, *write;
	// Remove '\\n':
	for (write = read = ret_str; *read; read++) {
		if (read[0] == '\\' && read[1] == '\n')
			read += 1;
		else
			*write++ = *read;
	}
	*write = '\0';

	// Replace \Uxxxxxxxx and \uxxxx with decoded unicode.
	for (write = read = ret_str; *read; read++) {
		if (read[0] == '\\' && (read[1] == 'u' || read[1] == 'U')) {
			int n_digits = *++read == 'u' ? 4 : 8;

			uint32_t codepoint = 0;
			for (int i = 0; i < n_digits; i++) {
				codepoint <<= 4;
				char c = *++read;
				if (c >= '0' && c <= '9')
					codepoint |= c - '0';
				else if (c >= 'a' && c <= 'f')
					codepoint |= c - 'a' + 10;
				else if (c >= 'A' && c <= 'F')
					codepoint |= c - 'A' + 10;
				else
					ERROR(pos, "Invalid universal character name in %.*s", len,
					      initial_pos);
			}

			char encoded[4];
			utf8_encode(codepoint, encoded);
			for (int i = 0; i < 4 && encoded[i]; i++)
				*write++ = encoded[i];
		} else {
			*write++ = *read;
		}
	}
	*write = '\0';

	return sv_from_str(ret_str);
}

static void parse_number(void) {
	for (;;) {
		if (c == EQ_EXPONENT) {
			next_char();
			if ((c == '+' || c == '-'))
				next_char();
		} else if (c == EQ_DECIMAL || c == '8' || c == EQ_ALPHA || c == 'u' ||
		           c == 'U' || c == EQ_EXPONENT || c == 'L' || c == '.') {
			next_char();
		} else {
			break;
		}
	}
}

static void parse_string_like(char end_char) {
	while (c != '\n' && c != end_char && c != EQ_NULL) {
		if (c == '\\') {
			next_char();
			if (c == 'u' || c == 'U') {
				next_char();
				needs_escape_sequences_removed = 1;
			}
		}

		next_char();
	}

	if (c != end_char)
		ERROR(pos, "Invalid string");

	next_char();
}

static void parse_identifier(void) {
	for (;;) {
		if (c == EQ_ALPHA || c == 'u' || c == 'U' || c == EQ_EXPONENT ||
		    c == 'L' || c == EQ_DECIMAL || c == '8' || c == EQ_UTF8) {
			next_char();
		} else if (c == '\\') {
			next_char();
			if (c == 'u' || c == 'U') {
				next_char();
				needs_escape_sequences_removed = 1;
			}
		} else {
			break;
		}
	}
}

static struct token tokenizer_next(int *is_header, int *is_directive) {
	struct token next = {0};

#define TYPE(IDX) \
	next_char();  \
	next.type = IDX;

restart:
	next.pos = pos;

	const char *initial_pos = str - 1;
	needs_escape_sequences_removed = 0;

	switch (c) {
	case EQ_SPACE:
		next_char();
		next.whitespace = 1;
		goto restart;

	case '\n':
		next_char();
		*is_header = *is_directive = 0;
		next.first_of_line = next.whitespace = 1;
		goto restart;

	case '/':
		TYPE(T_DIV);
		switch (c) {
		case '/':
			while (c != '\n' && c != EQ_NULL)
				next_char();
			next.whitespace = 1;
			goto restart;

		case '*':
			for (;;) {
				next_char();
				if (c == '*') {
					next_char();
					if (c == '/') {
						next_char();
						break;
					}
				} else if (c == EQ_NULL) {
					ERROR(pos, "Comment reached end of file");
				}
			}
			next.whitespace = 1;
			goto restart;

		case '=': TYPE(T_DIVA); break;
		}
		break;

	case 'u':
		TYPE(T_IDENT);
		switch (c) {
		case '8':
			TYPE(T_IDENT);
			switch (c) {
			case '"':
				TYPE(T_STRING);
				parse_string_like('"');
				break;

			default:
				parse_identifier();
				next.type = T_IDENT;
				break;
			}
			break;

		case '"':
			TYPE(T_STRING);
			parse_string_like('"');
			break;

		case '\'':
			TYPE(T_CHARACTER_CONSTANT);
			parse_string_like('\'');
			break;

		default:
			parse_identifier();
			next.type = T_IDENT;
			break;
		}
		break;

	case 'L':
	case 'U':
		TYPE(T_IDENT);
		switch (c) {
		case '"':
			TYPE(T_STRING);
			parse_string_like('"');
			break;

		case '\'':
			TYPE(T_CHARACTER_CONSTANT);
			parse_string_like('\'');
			break;

		default: parse_identifier(); break;
		}
		break;

	case '"':
		TYPE(T_STRING);
		parse_string_like('"');
		break;

	case '\'':
		TYPE(T_CHARACTER_CONSTANT);
		parse_string_like('\'');
		break;

	case EQ_EXPONENT:
	case EQ_UTF8:
	case EQ_ALPHA:
		TYPE(T_IDENT);
		parse_identifier();
		break;

	case '8':
	case EQ_DECIMAL:
		TYPE(T_NUM);
		parse_number();
		break;

	case '.':
		TYPE(T_DOT);

		switch (c) {
		case EQ_DECIMAL:
		case '8':
			TYPE(T_NUM);
			parse_number();
			break;

		case '.':
			next_char();
			if (c != '.')
				ERROR(pos, "Invalid token");
			TYPE(T_ELLIPSIS);
			break;
		}
		break;

	case '#':
		TYPE(PP_HASH);

		if (c == '#') {
			TYPE(PP_HHASH);
		} else {
			if (next.first_of_line) {
				next.type = PP_DIRECTIVE;
				*is_directive = 1;
			}
		}
		break;

	case '<':
		if (*is_header) {
			TYPE(PP_HEADER_NAME_H);
			parse_string_like('>');
		} else {
			TYPE(T_L);
			switch (c) {
			case '<':
				TYPE(T_LSHIFT);
				switch (c) {
				case '=': TYPE(T_LSHIFTA); break;
				}
				break;

			case '=': TYPE(T_LEQ); break;
			}
		}
		break;

	case '>':
		TYPE(T_G);
		switch (c) {
		case '>':
			TYPE(T_RSHIFT);
			switch (c) {
			case '=': TYPE(T_RSHIFTA); break;
			}
			break;

		case '=': TYPE(T_GEQ); break;
		}
		break;

	case '=':
		TYPE(T_A);
		switch (c) {
		case '=': TYPE(T_EQ); break;
		}
		break;

	case '&':
		TYPE(T_AMP);
		switch (c) {
		case '=': TYPE(T_BANDA); break;
		case '&': TYPE(T_AND); break;
		}
		break;

	case '|':
		TYPE(T_BOR);
		switch (c) {
		case '=': TYPE(T_BORA); break;
		case '|': TYPE(T_OR); break;
		}
		break;

	case '+':
		TYPE(T_ADD);
		switch (c) {
		case '=': TYPE(T_ADDA); break;
		case '+': TYPE(T_INC); break;
		}
		break;

	case '-':
		TYPE(T_SUB);
		switch (c) {
		case '=': TYPE(T_SUBA); break;
		case '-': TYPE(T_DEC); break;
		case '>': TYPE(T_ARROW); break;
		}
		break;

	case '!':
		TYPE(T_NOT);
		switch (c) {
		case '=': TYPE(T_NEQ); break;
		}
		break;

	case '^':
		TYPE(T_XOR);
		switch (c) {
		case '=': TYPE(T_XORA); break;
		}
		break;

	case '*':
		TYPE(T_STAR);
		switch (c) {
		case '=': TYPE(T_MULA); break;
		}
		break;

	case '%':
		TYPE(T_MOD);
		switch (c) {
		case '=': TYPE(T_MODA); break;
		}
		break;

	case '~': TYPE(T_BNOT); break;
	case '?': TYPE(T_QUEST); break;
	case ':': TYPE(T_COLON); break;
	case ';': TYPE(T_SEMI_COLON); break;
	case '(': TYPE(T_LPAR); break;
	case ')': TYPE(T_RPAR); break;
	case '[': TYPE(T_LBRACK); break;
	case ']': TYPE(T_RBRACK); break;
	case ',': TYPE(T_COMMA); break;
	case '{': TYPE(T_LBRACE); break;
	case '}': TYPE(T_RBRACE); break;

	case EQ_NULL:
		next.first_of_line = 1;
		next.type = T_EOI;
		break;

	default: ERROR(next.pos, "Invalid token");
	}

	next.str = (struct string_view){
		.len = str - initial_pos - 1,
		.str = (char *)initial_pos,
	};

	if (needs_escape_sequences_removed)
		next.str = remove_escape_sequences(initial_pos);

	if (next.type == T_IDENT && *is_directive) {
		*is_header = sv_string_cmp(next.str, "include");
		*is_directive = 0;
	}

	return next;
}

struct token_list tokenize_input(const char *contents, const char *path) {
	struct token_list tl = {0};

	int is_header = 0, is_directive = 0;

	c = '\n'; // Needs to start with newline.
	pos.path = path;
	pos.column = 1;
	pos.line = 1;
	str = contents;

	// Read, and ignore, BOM (byte order mark).
	// BOM signifies that the text file is utf-8.
	// It has the form: 0xef 0xbb 0xbf.
	if ((unsigned char)str[0] == 0xef && (unsigned char)str[1] == 0xbb &&
	    (unsigned char)str[2] == 0xbf)
		str += 3;

	struct token t = tokenizer_next(&is_header, &is_directive);
	while (t.type != T_EOI) {
		token_list_add(&tl, t);
		t = tokenizer_next(&is_header, &is_directive);

		struct token *prev = &tl.list[tl.size - 1];
		prev->whitespace_after = t.whitespace;
		prev->first_of_line_after = t.first_of_line;
	}

	return tl;
}
