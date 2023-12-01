#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "preprocessor/input.h" // For struct position.

#include <string_view.h>

_Noreturn void impl_error_ice(const char *file, int line, const char *fmt, ...);
_Noreturn void impl_error(struct position pos, const char *file, int line, const char *fmt, ...);
_Noreturn void impl_error_no_pos(const char *file, int line, const char *fmt, ...);
_Noreturn void impl_error_notimp(const char *file, int line);
void impl_warning(struct position pos, const char *fmt, ...);

#define ICE(...) impl_error_ice(__FILE__, __LINE__, __VA_ARGS__)
#define ERROR(POS, ...) impl_error((POS), __FILE__, __LINE__, __VA_ARGS__)
#define WARNING(POS, ...) impl_warning((POS), __VA_ARGS__)
#define ERROR_NO_POS(...) impl_error_no_pos(__FILE__, __LINE__, __VA_ARGS__)
#define NOTIMP() impl_error_notimp(__FILE__, __LINE__)

#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#define MIN(A, B) (((A) < (B)) ? (A) : (B))

// Add element to a dynamic array, with size and capacity.
// Doubling is better than 1.5, or any other factor.
#define ADD_ELEMENT(SIZE, CAP, PTR) (*((void)((SIZE) >= CAP ? (CAP = MAX(CAP * 2, 1)) : 0, PTR = cc_realloc(PTR, sizeof *PTR * CAP)), PTR + (SIZE)++))
#define ADD_ELEMENTS(SIZE, CAP, PTR, N) ((void)((SIZE + (N)) > CAP && (CAP = MAX(CAP * 2, (SIZE) + (N)), PTR = cc_realloc(PTR, sizeof *PTR * CAP))), (SIZE) += (N), PTR + (SIZE) - (N))
#define REMOVE_ELEMENT(SIZE, PTR, IDX) (memmove(PTR + (IDX), PTR + (IDX) + 1, ((SIZE) - (IDX) - 1) * sizeof *(PTR)), (SIZE)--)

#define ALLOC(...) memcpy(cc_malloc(sizeof (__VA_ARGS__)), &(__VA_ARGS__), sizeof (__VA_ARGS__))

uint32_t hash32(uint32_t a);

char *allocate_printf(const char *fmt, ...);
void expand_printf(char **buffer, size_t *capacity, const char *fmt, ...);

int round_up_to_nearest(int num, int div);

char *strdup(const char *s);

uint64_t gen_mask(unsigned char left_pad, unsigned char right_pad);

intmax_t character_constant_to_int(struct string_view str);
intmax_t character_constant_wchar_to_int(struct string_view str);
intmax_t character_constant_char16_to_int(struct string_view str);
intmax_t character_constant_char32_to_int(struct string_view str);

void write_8(uint8_t *data, uint64_t value);
void write_16(uint8_t *data, uint64_t value);
void write_32(uint8_t *data, uint64_t value);
void write_64(uint8_t *data, uint64_t value);

uint8_t read_8(uint8_t *data);
uint16_t read_16(uint8_t *data);
uint32_t read_32(uint8_t *data);
uint64_t read_64(uint8_t *data);

void file_write(FILE *fp, const void *ptr, size_t size);
void file_write_byte(FILE *fp, uint8_t byte);
void file_write_word(FILE *fp, uint16_t word);
void file_write_long(FILE *fp, uint32_t long_);
void file_write_quad(FILE *fp, uint64_t quad);
void file_write_zero(FILE *fp, size_t size);
void file_write_skip(FILE *fp, size_t target);

#define malloc(...) _Static_assert(0, "Use cc_malloc instead.")
#define realloc(...) _Static_assert(0, "Use cc_realloc instead.")

void *cc_malloc(size_t size);
void *cc_realloc(void *ptr, size_t size);

int char_to_int(char c);

#include <debug.h>

#endif
