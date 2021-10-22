#include "common.h"

#include <stdarg.h>
#include <limits.h>

uint32_t hash32(uint32_t a) {
	a = (a ^ 61) ^ (a >> 16);
	a = a + (a << 3);
	a = a ^ (a >> 4);
	a = a * 0x27d4eb2d;
	a = a ^ (a >> 15);
	return a;
}	

// djb2 hash: http://www.cse.yorku.ca/~oz/hash.html
uint32_t hash_str(const char *str) {
	uint32_t hash = 5381;
	int c;
	
	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;

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

int character_constant_to_int(const char *str) {
	int constant = 0;

	for (; *str; str++) {
		constant <<= 8; // TODO: UB on overflow?
		constant += *str;
	}

	return constant;
}

unsigned char needs_no_escape[CHAR_MAX];

void init_source_character_set(void) {
	const char *str = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#%&()*+,-./:;<=>?[]^_{|}~ ";
	for (; *str; str++) {
		int idx = *str;
		if (idx >= 0 && idx < 128)
			needs_no_escape[idx] = 1;
	}
}

void character_to_escape_sequence(char character, char *output) {
	int octal[3] = { 0 };

	if ((int)character >= 0 && needs_no_escape[(int)character]) {
		output[0] = character;
		output[1] = '\0';
		return;
	}

	octal[2] = character % 8;
	character /= 8;
	octal[1] = character % 8;
	character /= 8;
	octal[0] = character % 8;
	
	output[0] = '\\';
	output[1] = octal[0] + '0';
	output[2] = octal[1] + '0';
	output[3] = octal[2] + '0';
	output[4] = '\0';
}

uint64_t gen_mask(unsigned char left_pad, unsigned char right_pad) {
    uint64_t start = ~0;

    start = (start >> right_pad) << right_pad;
    start = (start << left_pad) >> left_pad;

    return ~start;
}
