#include <assert.h>
#include <stdlib.h>

#define offsetof_macro(type, member) ((size_t)( (char *)&(((type *)0)->member) - (char *)0 ))
#define offsetof_builtin(type, member) __builtin_offsetof(type, member)

struct T {
	int a, b, c;
	char d;
	long e;
};

enum {
	CONST = offsetof_builtin(struct T, e),
};

int main(void) {
	_Static_assert(CONST == 16, "");
	assert(offsetof_macro(struct T, e) == 16);
	assert(offsetof_builtin(struct T, e) == 16);
}
