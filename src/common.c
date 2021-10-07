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

int escaped_to_str(const char *str) {
	if (str[0] != '\\')
		return str[0];
	else {
		switch (str[1]) {
		case 'n':
			return '\n';
		case '\'':
			return '\'';
		case '\"':
			return '\"';
		case 't':
			return '\t';
		case '0':
			return '\0';
		case '\\':
			return '\\';
		default:
			ERROR("Invalid escape sequence %c%c", str[0], str[1]);
		}
	}
}
