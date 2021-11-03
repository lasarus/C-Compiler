#include "tokenizer.h"
#include "syntax.h"
#include "search_path.h"
#include "input.h"

#include <common.h>

#include <string.h>
#include <stdlib.h>

static struct tokenizer {
	struct input *top;
	int header;
} tok;

#define C0 (tok.top->c[0])
#define C1 (tok.top->c[1])
#define C2 (tok.top->c[2])
#define CNEXT() input_next(tok.top)

size_t disallowed_size, disallowed_cap;
char **disallowed = NULL;

static void push_input(struct file file) {
	struct input *n_top = malloc(sizeof *n_top);
	*n_top = input_create(file);
	n_top->next = tok.top;
	tok.top = n_top;
}

void tokenizer_push_input_absolute(const char *path) {
	struct file file;
	if (!try_open_file(path, &file))
		ERROR("No such file as %s exists", path);

	push_input(file);
}

void tokenizer_push_input(const char *rel_path) {
	struct file file = search_include(&tok.top->file, rel_path);

	for (unsigned i = 0; i < disallowed_size; i++) {
		if (strcmp(disallowed[i], file.full) == 0)
			return; // Do not open file. (Related to #pragma once.)
	}

	push_input(file);
}

void tokenizer_disable_current_path(void) {
	ADD_ELEMENT(disallowed_size, disallowed_cap, disallowed) = strdup(tok.top->file.full);
}

static void tokenizer_pop_input(void) {
	struct input *prev = tok.top;
	tok.top = prev->next;
	input_free(prev);
	free(prev);
}

void flush_whitespace(int *whitespace, int *first_of_line) {
	for (;;) {
		if (is_space(C0)) {
			if(C0 == '\n')
				*first_of_line = 1;

			CNEXT();
		} else if (C0 == '/' &&
				   C1 == '*') {
			while (!(C0 == '*' &&
					 C1 == '/')) {
				CNEXT();

				if (C0 == '\0')
					ERROR("Comment reached end of file");
			}
			CNEXT();
			CNEXT();
		} else if (C0 == '/' &&
				   C1 == '/') {
			while (!(C0 == '\n')) {
				CNEXT();

				if (C0 == '\0')
					ERROR("Comment reached end of file");
			}
		} else {
			break;
		}
		*whitespace = 1;
	}
}

char *buffer = NULL;
size_t buffer_size = 0, buffer_cap = 0;

void buffer_start(void) {
	buffer_size = 0;
}

void buffer_write(char c) {
	ADD_ELEMENT(buffer_size, buffer_cap, buffer) = c;
}

char *buffer_get(void) {
	return strdup(buffer);
}

int parse_pp_token(enum ttype type, struct token *t,
				   int (*is_token)(char c, char nc, int initial)) {
	if (!is_token(C0, C1, 1))
		return 0;

	buffer_start();

	int advance = 0, initial = 1;
	while((advance = is_token(C0, C1, initial)) > 0) {
		initial = 0;
		for (int i = 0; i < advance; i++) {
			buffer_write(C0);
			CNEXT();
		}
	}

	buffer_write('\0');

	t->type = type;
	t->str = buffer_get();
	return 1;
}

int parse_pp_header_name(struct token *t) {
	int hchar;
	if (C0 == '"')
		hchar = 0;
	else if (C0 == '<')
		hchar = 1;
	else
		return 0;

	buffer_start();

	int initial = 1;
	while (initial || (hchar && is_hchar(C0)) ||
		   (!hchar && is_qchar(C0))) {
		initial = 0;
		buffer_write(C0);
		CNEXT();
	}

	buffer_write(C0);
	buffer_write('\0');
	t->type = PP_HEADER_NAME;
	t->str = buffer_get();

	CNEXT();

	return 1;
}

int parse_escape_sequence(int *character) {
	if (C0 != '\\')
		return 0;

	CNEXT();

	if (is_octal_digit(C0)) {
		int result = 0;
		for (int i = 0; i < 3 && is_octal_digit(C0); i++) {
			result *= 8;
			result += C0 - '0';
			CNEXT();
		}

		*character = result;

		return 1;
	} else if (C0 == 'x') {
		CNEXT();
		int result = 0;

		for (; is_hexadecimal_digit(C0); CNEXT()) {
			result *= 16;
			char digit = C0;
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

	switch (C0) {
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
		ERROR("Invalid escape sequence \\%c", C0);
	}

	CNEXT();

	return 1;
}

int parse_cs_char(int *character, int is_schar) {
	char c = C0;
	if (parse_escape_sequence(character)) {
		return 1;
	} else if (c == '\n' || c == (is_schar ? '\"' : '\'')) {
		return 0;
	} else {
		CNEXT();
		*character = c;
		return 1;
	}
}

int parse_string(struct token *next) {
	if (C0 == 'u' &&
		C1 == '8' &&
		C2 == '"') {
		NOTIMP();
	} else if (C0 == 'u' &&
		C1 == '"') {
		NOTIMP();
	} else if (C0 == 'U' &&
		C1 == '"') {
		NOTIMP();
	} else if (C0 == 'L' &&
		C1 == '"') {
		NOTIMP();
	} else if (C0 != '"') {
		return 0;
	}
	buffer_start();

	CNEXT();

	int character;
	while (parse_cs_char(&character, 1))
		buffer_write((char)character);

	if (C0 != '"')
		ERROR("Expected \"");

	buffer_write('\0');

	CNEXT();

	next->type = PP_STRING;
	next->str = buffer_get();

	return 1;
}

int parse_character_constant(struct token *next) {
	if (C0 != '\'')
		return 0;

	CNEXT();

	buffer_start();

	int character;
	while (parse_cs_char(&character, 0))
		buffer_write((char)character);

	if (C0 != '\'')
		ERROR("Expected \'");

	buffer_write('\0');

	CNEXT();

	next->type = PP_CHARACTER_CONSTANT;
	next->str = buffer_get();

	return 1;
}

int parse_punctuator(struct token *next) {
	int count = 0;
#define SYM(A, B) else if (												\
		(sizeof(B) <= 1 || B[0] == C0) &&					\
		(sizeof(B) <= 2 || B[1] == C1) &&					\
		(sizeof(B) <= 3 || B[2] == C2)) {					\
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
		buffer_write(C0);
		CNEXT();
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
#define IFSTR(S, TOK)	(C0 == S[0] &&						\
						 (sizeof(S) == 2 || C1 == S[1])) &&	\
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
	} else if(C0 == '\0') {
		if (tok.top->next) {
			// Retry on popped source.
			tokenizer_pop_input();
			return tokenizer_next();
		}

		next.first_of_line = 1;
		next.type = T_EOI;
	} else {
		PRINT_POS(next.pos);
		ERROR("Unrecognized preprocessing token! Starting with '%c', %d\n", C0, C0);
	}
#undef IFSTR

	for(; advance > 0; advance--)
		CNEXT();

	return next;
}

void set_header(int i) {
	tok.header = i;
}

void set_line(int line) {
	int diff = line - tok.top->pos[0].line;
	for (int i = 0; i < N_BUFF; i++)
		tok.top->pos[i].line += diff;
	for (int i = 0; i < INT_BUFF; i++)
		tok.top->ipos[i].line += diff;
	tok.top->iline = tok.top->ipos[INT_BUFF - 1].line;
}

void set_filename(char *name) {
	for (int i = 0; i < N_BUFF; i++)
		tok.top->pos[i].path = name;
	for (int i = 0; i < INT_BUFF; i++)
		tok.top->ipos[i].path = name;
	tok.top->file.full = name;
}

