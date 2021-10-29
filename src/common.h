#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define ERROR(STR, ...) do { printf("Error on line %d file %s: \"" STR "\"\n", __LINE__, __FILE__, ##__VA_ARGS__); exit(1); } while(0)
#define NOTIMP() ERROR("Not implemented");

#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#define MIN(A, B) (((A) < (B)) ? (A) : (B))

// Add element to a dynamic array, with size and capacity.
// Doubling is better than 1.5, or any other factor.
#define ADD_ELEMENT(SIZE, CAP, PTR) (*((void)(SIZE >= CAP ? (CAP = MAX(CAP * 2, 1)) : 0, PTR = realloc(PTR, sizeof *PTR * CAP)), PTR + SIZE++))

uint32_t hash32(uint32_t a);

uint32_t hash_str(const char *str);
char *allocate_printf(const char *fmt, ...);

int str_contains(const char *str, char c);

int round_up_to_nearest(int num, int div);

char *strdup(const char *s);

uint64_t gen_mask(unsigned char left_pad, unsigned char right_pad);

int character_constant_to_int(const char *str);
void character_to_escape_sequence(char character, char *output);

void init_source_character_set(void);

#include <debug.h>

#endif
