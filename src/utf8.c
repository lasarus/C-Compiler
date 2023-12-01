#include "utf8.h"

#include "common.h"

// See https://en.wikipedia.org/wiki/UTF-8 for more details.

void utf8_encode(uint32_t codepoint, char output[4]) {
	output[0] = output[1] = output[2] = output[3] = '\0';

	if (codepoint <= 0x7f) {
		output[0] = codepoint & 0xff;
	} else if (codepoint <= 0x7ff) {
		output[0] = 0xC0 | ((codepoint >> (6 * 1)) & ~0xE0);
		output[1] = 0x80 | ((codepoint >> (6 * 0)) & ~0xC0);
	} else if (codepoint <= 0xffff) {
		output[0] = 0xE0 | ((codepoint >> (6 * 2)) & ~0xF0);
		output[1] = 0x80 | ((codepoint >> (6 * 1)) & ~0xC0);
		output[2] = 0x80 | ((codepoint >> (6 * 0)) & ~0xC0);
	} else if (codepoint <= 0x10ffff) {
		output[0] = 0xF0 | ((codepoint >> (6 * 3)) & ~0xF8);
		output[1] = 0x80 | ((codepoint >> (6 * 2)) & ~0xC0);
		output[2] = 0x80 | ((codepoint >> (6 * 1)) & ~0xC0);
		output[3] = 0x80 | ((codepoint >> (6 * 0)) & ~0xC0);
	} else {
		ICE("Codepoint %ju too large to encode.", codepoint);
	}
}

int utf8_code_point_length(unsigned char s) {
	if ((s & 0x80) == 0) {
		return 1;
	} else if ((s & 0xE0) == 0xC0) {
		return 2;
	} else if ((s & 0xF0) == 0xE0) {
		return 3;
	} else if ((s & 0xF8) == 0xF0) {
		return 4;
	} else {
		ICE("Incorrectly encoded UTF-8, starting with %c\n", s);
	}
}

uint32_t utf8_decode(const char **start) {
	const unsigned char *s = (unsigned char *)*start;
	int advance = 0;
	uint32_t codepoint = 0;
	if ((s[0] & 0x80) == 0) {
		codepoint |= (s[0] & ~0x80) << (6 * 0);
		advance = 1;
	} else if ((s[0] & 0xE0) == 0xC0) {
		codepoint |= (s[0] & ~0xE0) << (6 * 1);
		codepoint |= (s[1] & ~0xC0) << (6 * 0);
		advance = 2;
	} else if ((s[0] & 0xF0) == 0xE0) {
		codepoint |= (s[0] & ~0xE0) << (6 * 2);
		codepoint |= (s[1] & ~0xC0) << (6 * 1);
		codepoint |= (s[2] & ~0xC0) << (6 * 0);
		advance = 3;
	} else if ((s[0] & 0xF8) == 0xF0) {
		codepoint |= (s[0] & ~0xF8) << (6 * 3);
		codepoint |= (s[1] & ~0xC0) << (6 * 2);
		codepoint |= (s[2] & ~0xC0) << (6 * 1);
		codepoint |= (s[3] & ~0xC0) << (6 * 0);
		advance = 4;
	} else {
		ICE("Incorrectly encoded UTF-8, starting with %.16s, %X\n", *start);
	}

	*start += advance;

	return codepoint;
}
