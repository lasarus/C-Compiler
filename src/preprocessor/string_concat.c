#include "string_concat.h"
#include "macro_expander.h"
#include "tokenizer.h"

#include <common.h>
#include <utf8.h>
#include <escape_sequence.h>

#include <assert.h>

static ttype get_ident(struct string_view str) {
#define X(A, B)
#define SYM(A, B)
#define KEY(A, B) if(sv_string_cmp(str, B)) { return A; }
#include "tokens.h"
#undef KEY
#undef X
#undef SYM
	return T_IDENT;
}

enum string_type {
	STRING_DEFAULT, STRING_U_SMALL, STRING_U_LARGE, STRING_L, STRING_U8
};

static char *buffer = NULL;
static size_t buffer_size = 0, buffer_cap = 0;
static enum string_type buffer_type = STRING_DEFAULT;

static void buffer_start(void) {
	buffer_size = 0;
	buffer_type = STRING_DEFAULT;
}

static void buffer_write(char c) {
	ADD_ELEMENT(buffer_size, buffer_cap, buffer) = c;
}

static struct string_view buffer_get(void) {
	struct string_view ret = { .len = buffer_size };
	ret.str = cc_malloc(buffer_size);
	memcpy(ret.str, buffer, buffer_size);
	return ret;
}

static uint32_t take_utf8(struct string_view *input) {
	assert(input->len);

	const char *advance = input->str;
	uint32_t codepoint = utf8_decode(&advance);
	sv_tail(input, advance - input->str);

	return codepoint;
}

static int escape_char_to_buffer(struct string_view *input,
								 char end_char,
								 enum string_type type) {
	if (input->len == 0 || input->str[0] == end_char)
		return 0;

	uint32_t character;
	const char *advance = input->str;
	if (escape_sequence_read(&character, &advance, input->len)) {
		sv_tail(input, advance - input->str);
	} else {
		if (type == STRING_L || type == STRING_U_LARGE ||
			type == STRING_U_SMALL) {
			character = take_utf8(input);
		} else {
			character = input->str[0];
			sv_tail(input, 1);
		}
	}

	switch (type) {
	case STRING_DEFAULT:
	case STRING_U8:
		buffer_write(character);
		break;
	case STRING_L:
	case STRING_U_LARGE:
		// TODO: Make this actually decode character
		buffer_write(character & 0xff);
		buffer_write((character >> 8) & 0xff);
		buffer_write((character >> 16) & 0xff);
		buffer_write((character >> 24) & 0xff);
		break;
	case STRING_U_SMALL:
		buffer_write(character & 0xff);
		buffer_write((character >> 8) & 0xff);
		break;
	}

	return 1;
}

static enum string_type take_string_prefix(struct string_view *input) {
	if (input->len > 1 && input->str[0] == 'u' &&
		input->str[1] == '8') {
		sv_tail(input, 2);
		return STRING_U8;
	} else if (input->len > 0 && input->str[0] == 'u') {
		sv_tail(input, 1);
		return STRING_U_SMALL;
	} else if (input->len > 0 && input->str[0] == 'U') {
		sv_tail(input, 1);
		return STRING_U_LARGE;
	} else if (input->len > 0 && input->str[0] == 'L') {
		sv_tail(input, 1);
		return STRING_L;
	}
	return STRING_DEFAULT;
}

static void escape_string_to_buffer(struct string_view input, enum string_type type) {
	assert(input.len);

	char end_char = input.str[0];
	if (input.str[0] == '<')
		end_char = '>';

	sv_tail(&input, 1);

	while (escape_char_to_buffer(&input, end_char, type));

	if (!(input.len && input.str[0] == end_char)) {
		printf("Left over: \"%.*s\"\n", input.len, input.str);
	}
	assert(input.len && input.str[0] == end_char);
}

struct token string_concat_next(void) {
	static struct token prev = { 0 };

	if (prev.type) {
		struct token ret = prev;
		prev = (struct token) { 0 };
		return ret;
	}

	struct token t = expander_next();
	static size_t string_tokens_size = 0, string_tokens_cap = 0;
	static struct token *string_tokens = NULL;

	string_tokens_size = 0;

	while (t.type == T_STRING) {
		ADD_ELEMENT(string_tokens_size, string_tokens_cap, string_tokens) = t;
		t = expander_next();
	}

	if (string_tokens_size) {
		prev = t;

		enum string_type combined_type = STRING_DEFAULT;
		for (unsigned i = 0; i < string_tokens_size; i++) {
			enum string_type type = take_string_prefix(&string_tokens[i].str);

			if (type == STRING_DEFAULT ||
				type == combined_type)
				continue;

			if (combined_type == STRING_DEFAULT)
				combined_type = type;
			else
				ERROR(string_tokens[i].pos, "Invalid combination of strings with different prefix.");
		}

		buffer_start();
		for (unsigned i = 0; i < string_tokens_size; i++)
			escape_string_to_buffer(string_tokens[i].str, combined_type);
		struct token ret = string_tokens[0];

		switch (combined_type) {
		case STRING_DEFAULT:
		case STRING_U8:
			ret.type = T_STRING;
			buffer_write('\0');
			break;
		case STRING_U_LARGE:
			ret.type = T_STRING_CHAR32;
			buffer_write('\0');
			buffer_write('\0');
			buffer_write('\0');
			buffer_write('\0');
			break;
		case STRING_U_SMALL:
			ret.type = T_STRING_CHAR16;
			buffer_write('\0');
			buffer_write('\0');
			break;
		case STRING_L:
			ret.type = T_STRING_WCHAR;
			buffer_write('\0');
			buffer_write('\0');
			buffer_write('\0');
			buffer_write('\0');
			break;
		}

		ret.str = buffer_get();
		return ret;
	}

	if (t.type == T_IDENT) {
		t.type = get_ident(t.str);
	} else if (t.type == T_CHARACTER_CONSTANT) {
		enum string_type type = take_string_prefix(&t.str);

		if (type == STRING_U8)
			ERROR(t.pos, "Can't have character constant with u8 prefix.");

		buffer_start();
		escape_string_to_buffer(t.str, type);
		t.str = buffer_get();

		switch (type) {
		case STRING_DEFAULT:
			t.type = T_CHARACTER_CONSTANT;
			break;
		case STRING_U_LARGE:
			t.type = T_CHARACTER_CONSTANT_CHAR32;
			break;
		case STRING_U_SMALL:
			t.type = T_CHARACTER_CONSTANT_CHAR16;
			break;
		case STRING_L:
			t.type = T_CHARACTER_CONSTANT_WCHAR;
			break;
		default: NOTIMP();
		}
	}

	return t;
}

intmax_t escaped_character_constant_to_int(struct token t) {
	enum string_type type = take_string_prefix(&t.str);
	buffer_start();
	escape_string_to_buffer(t.str, type);

	// No need to call buffer_get(), since we do not need
	// a permanent string_view.
	struct string_view tmp_view = {
		.len = buffer_size,
		.str = buffer
	};

	return character_constant_to_int(tmp_view);
}
