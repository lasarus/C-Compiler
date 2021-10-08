#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define ERROR(STR, ...) do { printf("Error on line %d file %s: \"" STR "\"\n", __LINE__, __FILE__, ##__VA_ARGS__); exit(1); } while(0)
#define NOTIMP() ERROR("Not implemented");

#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#define MIN(A, B) (((A) < (B)) ? (A) : (B))

uint32_t hash32(uint32_t a);

uint32_t hash_str(const char *str);
char *allocate_printf(const char *fmt, ...);

int str_contains(const char *str, char c);

int round_up_to_nearest(int num, int div);

int escaped_to_str(const char *str);
int take_character(const char **str);

char *strdup(const char *s);

#endif
