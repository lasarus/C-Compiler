#include "tokenizer.h"
#include "syntax.h"
#include "search_path.h"
#include "input.h"

#include <common.h>

#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_INCLUDE_DEPTH 16

struct tokenizer {
	int input_n;
	struct input *stack, *top;

	int header;
} tok;
// Tokenizer, abbreviated tok.

unsigned disallowed_size, disallowed_cap;
char **disallowed = NULL;

void tokenizer_push_input_absolute(const char *path) {
	struct file file;
	if (!try_open_file(path, &file))
		ERROR("No such file as %s exists", path);

	if (!tok.stack)
		tok.stack = malloc(sizeof *tok.stack * MAX_INCLUDE_DEPTH);

	tok.input_n++;
	tok.stack[tok.input_n - 1] = input_create(file);
	tok.top = &tok.stack[tok.input_n - 1];
}

void tokenizer_push_input(const char *rel_path) {
	struct file file = search_include(&tok.top->file, rel_path);

	for (unsigned i = 0; i < disallowed_size; i++) {
		if (strcmp(disallowed[i], file.full) == 0)
			return; // Do not open file. (Related to #pragma once.)
	}

	if (!tok.stack)
		tok.stack = malloc(sizeof *tok.stack * MAX_INCLUDE_DEPTH);

	tok.input_n++;
	tok.stack[tok.input_n - 1] = input_create(file);
	tok.top = &tok.stack[tok.input_n - 1];
}

void tokenizer_disable_current_path(void) {
	if (disallowed_size >= disallowed_cap) {
		disallowed_cap = MAX(disallowed_cap * 2, 4);
		disallowed = realloc(disallowed, sizeof *disallowed * disallowed_cap);
	}

	disallowed[disallowed_size++] = strdup(tok.top->file.full);
}

static void tokenizer_pop_input(void) {
	input_free(tok.top);
	tok.input_n--;
	tok.top = &tok.stack[tok.input_n - 1];
}

int flush_whitespace(int *whitespace,
					 int *first_of_line) {
	int any_change = 0;
	while (isspace(tok.top->c[0])) {
		*whitespace = 1;

		if(tok.top->c[0] == '\n')
			*first_of_line = 1;

		input_next(tok.top);
		any_change = 1;
	}

	return any_change;
}

char *buffer = NULL;
size_t buffer_cap = 0, buffer_pos = 0;

void buffer_start(void) {
	buffer_pos = 0;
}

void buffer_write(char c) {
	if (buffer_cap == 0) {
		buffer_cap = 4;
		buffer = malloc(buffer_cap);
	}

	if (buffer_pos >= buffer_cap) {
		buffer_cap = (buffer_cap > 0) ?
			(buffer_cap * 2) :
			16;

		buffer = realloc(buffer, buffer_cap);
	}

	buffer[buffer_pos] = c;
	buffer_pos++;
}

char *buffer_get(void) {
	return strdup(buffer);
}

int parse_pp_token(enum ttype type, struct token *t,
				   int (*is_token)(char c, char nc, int initial)) {
	if (!is_token(tok.top->c[0], tok.top->c[1], 1))
		return 0;

	buffer_start();

	int advance = 0, initial = 1;
	while((advance = is_token(tok.top->c[0], tok.top->c[1], initial)) > 0) {
		initial = 0;
		for (int i = 0; i < advance; i++) {
			buffer_write(tok.top->c[0]);
			input_next(tok.top);
		}
	}

	buffer_write('\0');

	t->type = type;
	t->str = buffer_get();
	return 1;
}

int parse_pp_header_name(struct token *t) {
	int hchar;
	if (tok.top->c[0] == '"')
		hchar = 0;
	else if (tok.top->c[0] == '<')
		hchar = 1;
	else
		return 0;

	buffer_start();

	int initial = 1;
	while (initial || (hchar && is_hchar(tok.top->c[0])) ||
		   (!hchar && is_qchar(tok.top->c[0]))) {
		initial = 0;
		buffer_write(tok.top->c[0]);
		input_next(tok.top);
	}

	buffer_write(tok.top->c[0]);
	buffer_write('\0');
	t->type = PP_HEADER_NAME;
	t->str = buffer_get();

	input_next(tok.top);

	return 1;
}

int get_simple_escape_sequence(char nc) {
	switch (nc) {
	case '\'':
		return '\'';
	case '\"':
		return '\"';
	case '?':
		return '\?';
	case '\\':
		return '\\';
	case 'a':
		return '\a';
	case 'b':
		return '\b';
	case 'f':
		return '\f';
	case 'n':
		return '\n';
	case 'r':
		return '\r';
	case 't':
		return '\t';
	case 'v':
		return '\v';

	case '0': // This is an octal-escape-sequence, and should be handled seperately.
		return '\0';

	default:
		ERROR("Invalid escape sequence \\%c", nc);
	}
}

int parse_escape_sequence(int *character) {
	if (tok.top->c[0] != '\\')
		return 0;

	input_next(tok.top);

	if (is_octal_digit(tok.top->c[0])) {
		int result = 0;
		for (int i = 0; i < 3 && is_octal_digit(tok.top->c[0]); i++) {
			result *= 8;
			result += tok.top->c[0] - '0';
			input_next(tok.top);
		}

		*character = result;

		return 1;
	} else if (tok.top->c[0] == 'x') {
		input_next(tok.top);
		int result = 0;

		for (; is_hexadecimal_digit(tok.top->c[0]); input_next(tok.top)) {
			result *= 16;
			char digit = tok.top->c[0];
			if (is_digit(digit)) {
				result += digit - '0';
			} else if (digit >= 'a' && digit <= 'f') {
				result += digit - 'a' + 10;
			} else if (digit >= 'A' && digit <= 'F') {
				result += digit - 'A' + 10;
			}
		}

		*character = result;
		return 1;
	}

	switch (tok.top->c[0]) {
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
		ERROR("Invalid escape sequence \\%c", tok.top->c[0]);
	}

	input_next(tok.top);

	return 1;
}

int parse_cs_char(int *character, int is_schar) {
	char c = tok.top->c[0];
	if (parse_escape_sequence(character)) {
		return 1;
	} else if (c == '\n' || c == (is_schar ? '\"' : '\'')) {
		return 0;
	} else {
		input_next(tok.top);
		*character = c;
		return 1;
	}
}

int parse_string(struct token *next) {
	if (tok.top->c[0] == 'u' &&
		tok.top->c[1] == '8' &&
		tok.top->c[2] == '"') {
		NOTIMP();
	} else if (tok.top->c[0] == 'u' &&
		tok.top->c[1] == '"') {
		NOTIMP();
	} else if (tok.top->c[0] == 'U' &&
		tok.top->c[1] == '"') {
		NOTIMP();
	} else if (tok.top->c[0] == 'L' &&
		tok.top->c[1] == '"') {
		NOTIMP();
	} else if (tok.top->c[0] != '"') {
		return 0;
	}
	buffer_start();

	input_next(tok.top);

	//while(tok.top->c[0] != '"') {
	int character;
	while (parse_cs_char(&character, 1))
		buffer_write((char)character);

	if (tok.top->c[0] != '"')
		ERROR("Expected \"");

	buffer_write('\0');

	input_next(tok.top);

	next->type = PP_STRING;
	next->str = buffer_get();

	return 1;
}

int parse_character_constant(struct token *next) {
	if (tok.top->c[0] != '\'')
		return 0;

	input_next(tok.top);

	buffer_start();

	int character;
	while (parse_cs_char(&character, 0))
		buffer_write((char)character);

	if (tok.top->c[0] != '\'')
		ERROR("Expected \'");

	buffer_write('\0');

	input_next(tok.top);

	next->type = PP_CHARACTER_CONSTANT;
	next->str = buffer_get();

	return 1;
}

int parse_punctuator(struct token *next) {
	int count = 0;
#define SYM(A, B) else if (												\
		(sizeof(B) <= 1 || B[0] == tok.top->c[0]) &&					\
		(sizeof(B) <= 2 || B[1] == tok.top->c[1]) &&					\
		(sizeof(B) <= 3 || B[2] == tok.top->c[2])) {					\
		count = sizeof(B) - 1;											\
	}
#define KEY(A, B)
#define X(A, B)

	if (0) {}
	#include "tokens.h"
	else
		return 0;

	buffer_start();

	for (int i = 0; i < count; i++) {
		buffer_write(tok.top->c[0]);
		input_next(tok.top);
	}

	buffer_write('\0');

	next->type = PP_PUNCT;
	next->str = buffer_get();

	return 1;
}

struct token tokenizer_next(void) {
	struct token next = token_init(T_NONE, NULL, (struct position){0});

	flush_whitespace(&next.whitespace,
					 &next.first_of_line);

	int advance = 0;
#define IFSTR(S, TOK)	(tok.top->c[0] == S[0] &&						\
						 (sizeof(S) == 2 || tok.top->c[1] == S[1])) &&	\
		(next.type = TOK, next.str = strdup(S),							\
		 advance = sizeof(S) - 1, 1)									\

	next.pos = tok.top->pos[0];

	if(IFSTR("##", PP_HHASH)) {
	} else if(next.first_of_line && IFSTR("#", PP_DIRECTIVE)) {
	} else if(IFSTR("#", PP_HASH)) {
	} else if(IFSTR("(", PP_LPAR)) {
	} else if(IFSTR(")", PP_RPAR)) {
	} else if(IFSTR(",", PP_COMMA)) {
	} else if (tok.header && parse_pp_header_name(&next)) {
	} else if(parse_string(&next)) {
	} else if(parse_pp_token(PP_IDENT, &next,
							 is_identifier)) {
	} else if(parse_punctuator(&next)) {
	} else if(parse_character_constant(&next)) {
	} else if(parse_pp_token(PP_NUMBER, &next,
							 is_pp_number)) {
	} else if(tok.top->c[0] == '\0') {
		if(tok.input_n > 1) {
			// Retry on popped source.
			tokenizer_pop_input();
			return tokenizer_next();
		}

		next.first_of_line = 1;
		next.type = T_EOI;
	} else {
		PRINT_POS(next.pos);
		ERROR("Unrecognized preprocessing token! Starting with %c, %d\n", tok.top->c[0], tok.top->c[0]);
	}
#undef IFSTR

	for(; advance > 0; advance--)
		input_next(tok.top);

	return next;
}

void set_header(int i) {
	tok.header = i;
}
