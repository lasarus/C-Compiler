#include "common.h"

#include <stdarg.h>

uint32_t hash32(uint32_t a) {
	a = (a ^ 61) ^ (a >> 16);
	a = a + (a << 3);
	a = a ^ (a >> 4);
	a = a * 0x27d4eb2d;
	a = a ^ (a >> 15);
	return a;
}	

// TODO: This is not a good hash function.
uint32_t hash_str(const char *str) {
	uint32_t hash = 0;
	for (; *str; str++)
		hash ^= hash32(*str);
	return hash;
}

char *allocate_printf(const char *fmt, ...) {
	va_list args1, args2;
	va_start(args1, fmt);
	va_copy(args2, args1);
	int len = vsnprintf(NULL, 0, fmt, args1);
	char *str = malloc(len + 1);
	vsprintf(str, fmt, args2);
	va_end(args1);
	va_end(args2);
	return str;
}

int str_contains(const char *str, char c) {
	for (; *str && (*str != c); str++);
	return *str ? 1 : 0;
}

int round_up_to_nearest(int num, int div) {
	int r = num % div;
	if (r)
		num += div - r;
	return num;
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

int take_character(const char **str) {
	if (**str != '\\')
		return *(*str)++;
	else {
		(*str)++;
		if (**str == 'x') {
			int number = 0;
			for (;**str; (*str)++) {
				char c = **str;
				int decimal_digit = (c >= '0' && c <= '9');
				int low_hex_digit = (c >= 'a' && c <= 'f');
				int high_hex_digit = (c >= 'A' && c <= 'F');

				if (!(decimal_digit || low_hex_digit || high_hex_digit))
					break;

				number *= 16;
				if (decimal_digit)
					number += c - '0';
				else if (low_hex_digit)
					number += c - 'a' + 10;
				else if (high_hex_digit)
					number += c - 'A' + 10;
			}
			return number;
		} else
			return get_simple_escape_sequence(*(*str)++);
	}
}

int escaped_to_str(const char *str) {
	return take_character(&str);
}

uint64_t gen_mask(unsigned char left_pad, unsigned char right_pad) {
    uint64_t start = ~0;

    start = (start >> right_pad) << right_pad;
    start = (start << left_pad) >> left_pad;

    return ~start;
}
