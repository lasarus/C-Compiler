#include "common.h"

#include <stdarg.h>
#include <limits.h>
#include <assert.h>

uint32_t hash32(uint32_t a) {
	a = (a ^ 61) ^ (a >> 16);
	a = a + (a << 3);
	a = a ^ (a >> 4);
	a = a * 0x27d4eb2d;
	a = a ^ (a >> 15);
	return a;
}	

char *allocate_printf(const char *fmt, ...) {
	va_list args1, args2;
	va_start(args1, fmt);
	va_copy(args2, args1);
	int len = vsnprintf(NULL, 0, fmt, args1);
	char *str = cc_malloc(len + 1);
	vsprintf(str, fmt, args2);
	va_end(args1);
	va_end(args2);
	return str;
}

void expand_printf(char **buffer, size_t *capacity, const char *fmt, ...) {
	va_list args1, args2;
	va_start(args1, fmt);
	va_copy(args2, args1);
	size_t size = vsnprintf(NULL, 0, fmt, args1) + 1;

	if (size > *capacity) {
		*buffer = cc_realloc(*buffer, size);
		*capacity = size;
	}

	vsprintf(*buffer, fmt, args2);
	va_end(args1);
	va_end(args2);
}

int round_up_to_nearest(int num, int div) {
	int r = num % div;
	if (r)
		num += div - r;
	return num;
}

intmax_t character_constant_to_int(struct string_view str) {
	intmax_t constant = 0;

	for (int i = 0; i < str.len; i++) {
		constant <<= 8; // TODO: UB on overflow?
		constant |= str.str[i];
	}

	return constant;
}

intmax_t character_constant_wchar_to_int(struct string_view str) {
	assert(str.len % 4 == 0);
	intmax_t constant = 0;

	for (int i = 0; i < str.len; i += 4) {
		constant <<= 32; // TODO: UB on overflow?
		constant |= (str.str[i] & 0xff) |
			((str.str[i + 1] & 0xff) << 8) |
			((str.str[i + 2] & 0xff) << 16) |
			((str.str[i + 3] & 0xff) << 24);
	}

	return constant;
}

intmax_t character_constant_char16_to_int(struct string_view str) {
	assert(str.len % 2 == 0);
	intmax_t constant = 0;

	for (int i = 0; i < str.len; i += 2) {
		constant <<= 16; // TODO: UB on overflow?
		constant |= (str.str[i] & 0xff) |
			((str.str[i + 1] & 0xff) << 8);
	}

	return constant;
}

intmax_t character_constant_char32_to_int(struct string_view str) {
	return character_constant_wchar_to_int(str);
}

uint64_t gen_mask(unsigned char left_pad, unsigned char right_pad) {
    uint64_t start = ~0;

    start = (start >> right_pad) << right_pad;
    start = (start << left_pad) >> left_pad;

    return ~start;
}

void write_8(uint8_t *data, uint64_t value) {
	data[0] = value;
}

void write_16(uint8_t *data, uint64_t value) {
	data[0] = value;
	data[1] = value >> 8;
}

void write_32(uint8_t *data, uint64_t value) {
	write_16(data, value);
	write_16(data + 2, value >> 16);
}

void write_64(uint8_t *data, uint64_t value) {
	write_32(data, value);
	write_32(data + 4, value >> 32);
}

uint8_t read_8(uint8_t *data) {
	return data[0];
}

uint16_t read_16(uint8_t *data) {
	return read_8(data) | ((uint16_t)read_8(data + 1) << 8);
}

uint32_t read_32(uint8_t *data) {
	return read_16(data) | ((uint32_t)read_16(data + 2) << 16);
}

uint64_t read_64(uint8_t *data) {
	return read_32(data) | ((uint64_t)read_32(data + 4) << 32);
}

void file_write(FILE *fp, const void *ptr, size_t size) {
	if (size == 0)
		return;

	if (fwrite(ptr, size, 1, fp) != 1)
		ICE("Could not write %zu bytes to file.", size);
}

void file_write_byte(FILE *fp, uint8_t byte) {
	file_write(fp, &byte, 1);
}

void file_write_word(FILE *fp, uint16_t word) {
	file_write_byte(fp, word);
	file_write_byte(fp, word >> 8);
}

void file_write_long(FILE *fp, uint32_t long_) {
	file_write_word(fp, long_);
	file_write_word(fp, long_ >> 16);
}

void file_write_quad(FILE *fp, uint64_t quad) {
	file_write_long(fp, quad);
	file_write_long(fp, quad >> 32);
}

void file_write_zero(FILE *fp, size_t size) {
	for (size_t i = 0; i < size; i++)
		file_write_byte(fp, 0); // TODO: Make this faster.
}

void file_write_skip(FILE *fp, size_t target) {
	long int current_pos = ftell(fp);
	assert(target >= (size_t)current_pos);
	file_write_zero(fp, target - current_pos);
}

void *cc_malloc(size_t size) {
#undef malloc
	void *ret = malloc(size);

	if (!ret)
		ICE("Allocation error! Probably out of memory.\n");

	return ret;
}

void *cc_realloc(void *ptr, size_t size) {
#undef realloc
	void *ret = realloc(ptr, size);

	if (!ret)
		ICE("Allocation error! Probably out of memory.\n");

	return ret;
}

_Noreturn void impl_error_ice(const char *file, int line, const char *fmt, ...) {
	va_list args1;
	va_start(args1, fmt);
	printf("\nInternal compiler error, %s:%d of compiler source: \n\t", file, line);
	vprintf(fmt, args1);
	printf("\n");

	va_end(args1);
	exit(EXIT_FAILURE);
}

_Noreturn void impl_error(struct position pos, const char *file, int line, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	printf("\nError: %s:%d:%d: ", pos.path, pos.line, pos.column);
	vprintf(fmt, va);
	printf("\n");
	printf("Compiler source: %s:%d\n", file, line);

	va_end(va);
	exit(EXIT_FAILURE);
}

_Noreturn void impl_error_no_pos(const char *file, int line, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	printf("\nError: ");
	vprintf(fmt, va);
	printf("\n");
	printf("Compiler source: %s:%d\n", file, line);

	va_end(va);
	exit(EXIT_FAILURE);
}

_Noreturn void impl_error_notimp(const char *file, int line) {
	printf("Not implemented: %s:%d\n", file, line);
	exit(EXIT_FAILURE);
}

void impl_warning(struct position pos, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);

	printf("\nWarning, %s:%d:%d: ", pos.path, pos.line, pos.column);
	vprintf(fmt, va);
	printf("\n");

	va_end(va);
}

int char_to_int(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	ICE("%c is not a digit.", c);
}
