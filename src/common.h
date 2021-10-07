#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#define ERROR(STR, ...) do { printf("Error on line %d file %s: \"" STR "\"\n", __LINE__, __FILE__, ##__VA_ARGS__); exit(1); } while(0)
#define NOTIMP() ERROR("Not implemented");

#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#define MIN(A, B) (((A) < (B)) ? (A) : (B))

static inline uint32_t hash32(uint32_t a) {
	a = (a ^ 61) ^ (a >> 16);
	a = a + (a << 3);
	a = a ^ (a >> 4);
	a = a * 0x27d4eb2d;
	a = a ^ (a >> 15);
	return a;
}

// TODO: This is not a good hash function.
static inline uint32_t hash_str(char *str) {
	uint32_t hash = 0;
	for (; *str; str++)
		hash ^= hash32(*str);
	return hash;
}

static inline uint32_t hash64(uint64_t a) {
	return hash32(a) ^ hash32(a >> 32);
}

static inline char *allocate_printf(const char *fmt, ...) {
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

static inline int str_contains(const char *str, char c) {
	for (; *str && (*str != c); str++);
	return *str ? 1 : 0;
}

static inline int round_up_to_nearest(int num, int div) {
	int r = num % div;
	if (r)
		num += div - r;
	return num;
}

static inline int div_round_up(int num, int div) {
	return (num + (div - 1)) / div;
}

// This is just to show of my bit manipulation skillz.
static inline int round_up16(int num) {
    return (num & ~15) + !!(num & 15) * 16;
}

int escaped_to_str(const char *str);

#endif
