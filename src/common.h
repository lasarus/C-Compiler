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
#define ADD_ELEMENT(SIZE, CAP, PTR) (*((void)((SIZE) >= CAP ? (CAP = MAX(CAP * 2, 1)) : 0, PTR = realloc(PTR, sizeof *PTR * CAP)), PTR + (SIZE)++))
#define ADD_ELEMENTS(SIZE, CAP, PTR, N) ((void)((SIZE + (N)) > CAP && (CAP = MAX(CAP * 2, (SIZE) + (N)), PTR = realloc(PTR, sizeof *PTR * CAP))), (SIZE) += (N), PTR + (SIZE) - (N))

uint32_t hash32(uint32_t a);

//uint32_t hash_str(const char *str);
char *allocate_printf(const char *fmt, ...);

int round_up_to_nearest(int num, int div);

char *strdup(const char *s);

uint64_t gen_mask(unsigned char left_pad, unsigned char right_pad);

int character_constant_to_int(struct string_view str);
void character_to_escape_sequence(char character, char *output, int allow_compilcated_escape);

void init_source_character_set(void);

void write_8(uint8_t *data, uint64_t value);
void write_16(uint8_t *data, uint64_t value);
void write_32(uint8_t *data, uint64_t value);
void write_64(uint8_t *data, uint64_t value);

#include <debug.h>

#endif
