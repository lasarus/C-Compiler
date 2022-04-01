#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <string_view.h>

#define ICE(STR, ...) do { printf("\nInternal compiler error on line %d file %s of compiler source: \"" STR "\"\n", __LINE__, __FILE__, ##__VA_ARGS__); exit(1); } while(0)
#define ERROR(POS, STR, ...) do { printf("\nError: %s:%d:%d: ", (POS).path, (POS).line, (POS).column); printf(STR, ##__VA_ARGS__); ICE("DEBUG"); exit(1); } while(0)
#define WARNING(POS, STR, ...) do { printf("\nWarning, %s:%d:%d: ", (POS).path, (POS).line, (POS).column); printf(STR "\n", ##__VA_ARGS__); } while(0)
#define ARG_ERROR(IDX, STR, ...) do { printf("\nArgument error: %s (idx %d) ", argv[(IDX)], (IDX)); printf(STR, ##__VA_ARGS__); exit(1); } while(0)
#define NOTIMP() ICE("Not implemented");

#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#define MIN(A, B) (((A) < (B)) ? (A) : (B))

// Add element to a dynamic array, with size and capacity.
// Doubling is better than 1.5, or any other factor.
#define ADD_ELEMENT(SIZE, CAP, PTR) (*((void)((SIZE) >= CAP ? (CAP = MAX(CAP * 2, 1)) : 0, PTR = cc_realloc(PTR, sizeof *PTR * CAP)), PTR + (SIZE)++))
#define ADD_ELEMENTS(SIZE, CAP, PTR, N) ((void)((SIZE + (N)) > CAP && (CAP = MAX(CAP * 2, (SIZE) + (N)), PTR = cc_realloc(PTR, sizeof *PTR * CAP))), (SIZE) += (N), PTR + (SIZE) - (N))
#define REMOVE_ELEMENT(SIZE, PTR, IDX) (memmove(PTR + (IDX), PTR + (IDX) + 1, ((SIZE) - (IDX) - 1) * sizeof *(PTR)), (SIZE)--)

uint32_t hash32(uint32_t a);

//uint32_t hash_str(const char *str);
char *allocate_printf(const char *fmt, ...);

int round_up_to_nearest(int num, int div);

char *strdup(const char *s);

uint64_t gen_mask(unsigned char left_pad, unsigned char right_pad);

int character_constant_to_int(struct string_view str);
void character_to_escape_sequence(char character, char *output, int allow_compilcated_escape, int string);

void init_source_character_set(void);

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

#include <debug.h>

#endif
