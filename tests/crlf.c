#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#define typeid(x) _Generic(x,							\
						   _Bool: 0,					\
						   unsigned char: 1,			\
						   char: 2,                     \
						   signed char: 3,				\
						   short int: 4,				\
						   unsigned short int: 5,		\
						   int: 6,						\
						   unsigned int: 7,				\
						   long int: 8,					\
						   unsigned long int: 9,		\
						   long long int: 10,			\
						   unsigned long long int: 11,	\
						   float: 12,					\
						   double: 13,					\
						   long double: 14,				\
						   char *: 15,					\
						   void *: 16,					\
						   int *: 17,					\
						   default: 18)

int main(void) {
	size_t s;
	ptrdiff_t p;
	intmax_t i;
	int ai[3] = {0};
	assert(typeid('0') == 6);
#ifdef __LP64__
	assert(typeid(i) == 8);
	assert(typeid(p) == 8);
	assert(typeid(s) == 9);
	assert(typeid(0x7FFFFFFF) == 6);
	assert(typeid(0xFFFFFFFF) == 7);
	assert(typeid(0x7FFFFFFFU) == 7);
#endif
	assert(typeid(ai) == 17);
}
