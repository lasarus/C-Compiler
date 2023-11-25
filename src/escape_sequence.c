#include "escape_sequence.h"
#include "character_types.h"

#include "common.h"

#include <assert.h>
#include <limits.h>

int escape_sequence_read(uint32_t *out, const char **start, int n) {
	const char *s = *start;

	if (n == 0 || *s != '\\')
		return 0;

	s++;

	if (is_octal(*s)) {
		uint32_t result = 0;
		for (int i = 0; i < 3 && i < n && is_octal(*s); i++, s++) {
			result *= 8;
			result += char_to_int(*s);
		}
		*out = result;
	} else if (*s == 'x') {
		s++;
		uint32_t result = 0;
		for (int i = 0; i < n && is_hexadecimal(*s); i++, s++) {
			result *= 8;
			result += char_to_int(*s);
		}
		*out = result;
	} else {
		switch (*s) {
		case '\'': *out = '\''; break;
		case '\"': *out = '\"'; break;
		case '\?': *out = '?'; break;
		case '\\': *out = '\\'; break;
		case 'a': *out = '\a'; break;
		case 'b': *out = '\b'; break;
		case 'f': *out = '\f'; break;
		case 'n': *out = '\n'; break;
		case 'r': *out = '\r'; break;
		case 't': *out = '\t'; break;
		case 'v': *out = '\v'; break;

		default: ICE("Invalid escape sequence \\%c", *s);
		}

		s++;
	}

	*start = s;
	return 1;
}

static const unsigned char needs_no_escape[CHAR_MAX] = {
	['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1,
	['F'] = 1, ['G'] = 1, ['H'] = 1, ['I'] = 1, ['J'] = 1,
	['K'] = 1, ['L'] = 1, ['M'] = 1, ['N'] = 1, ['O'] = 1,
	['P'] = 1, ['Q'] = 1, ['R'] = 1, ['S'] = 1, ['T'] = 1,
	['U'] = 1, ['V'] = 1, ['W'] = 1, ['X'] = 1, ['Y'] = 1,
	['Z'] = 1, ['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1,
	['e'] = 1, ['f'] = 1, ['g'] = 1, ['h'] = 1, ['i'] = 1,
	['j'] = 1, ['k'] = 1, ['l'] = 1, ['m'] = 1, ['n'] = 1,
	['o'] = 1, ['p'] = 1, ['q'] = 1, ['r'] = 1, ['s'] = 1,
	['t'] = 1, ['u'] = 1, ['v'] = 1, ['w'] = 1, ['x'] = 1,
	['y'] = 1, ['z'] = 1, ['0'] = 1, ['1'] = 1, ['2'] = 1,
	['3'] = 1, ['4'] = 1, ['5'] = 1, ['6'] = 1, ['7'] = 1,
	['8'] = 1, ['9'] = 1, ['!'] = 1, ['#'] = 1, ['%'] = 1,
	['&'] = 1, ['('] = 1, [')'] = 1, ['*'] = 1, ['+'] = 1,
	[','] = 1, ['-'] = 1, ['.'] = 1, ['/'] = 1, [':'] = 1,
	[';'] = 1, ['<'] = 1, ['='] = 1, ['>'] = 1, ['['] = 1,
	[']'] = 1, ['^'] = 1, ['_'] = 1, ['{'] = 1, ['|'] = 1,
	['}'] = 1, ['~'] = 1, [' '] = 1,
};

static const unsigned char has_simple_escape[CHAR_MAX] = {
	['\n'] = 'n',
	['\t'] = 't',
	['\"'] = '\"',
	['\''] = '\'',
	['\\'] = '\\',
};

// Some assemblers don't support escape codes like \a,
// these are here referred to as "complicated".
static const unsigned char has_complicated_escape[CHAR_MAX] = {
	['?'] = '?',
	['\a'] = 'a',
	['\b'] = 'b',
	['\f'] = 'f',
	['\r'] = 'r',
	['\v'] = 'v',
};

void character_to_escape_sequence(char character, char output[static 5], int allow_complicated_escape) {
	if (character == '"') {
		output[0] = '\\';
		output[1] = '"';
		output[2] = '\0';
	} else if (character == '\'') {
		output[0] = '\'';
		output[1] = '\0';
	} else if ((int)character >= 0 && needs_no_escape[(int)character]) {
		output[0] = character;
		output[1] = '\0';
	} else if ((int)character >= 0 && has_simple_escape[(int)character]) {
		output[0] = '\\';
		output[1] = has_simple_escape[(int)character];
		output[2] = '\0';
	} else if (allow_complicated_escape && (int)character >= 0 &&
			   has_complicated_escape[(int)character]) {
		output[0] = '\\';
		output[1] = has_complicated_escape[(int)character];
		output[2] = '\0';
	} else {
		output[0] = '\\';

		unsigned char uchar = character;
		output[3] = uchar % 8 + '0';
		uchar /= 8;
		output[2] = uchar % 8 + '0';
		uchar /= 8;
		output[1] = uchar % 8 + '0';

		output[4] = '\0';
	}
}
