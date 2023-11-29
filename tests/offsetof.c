#include <assert.h>
#include <stdlib.h>

#define offsetof_macro(type, member) ((size_t)( (char *)&(((type *)0)->member) - (char *)0 ))
#define offsetof_builtin(type, member) __builtin_offsetof(type, member)

struct T {
	int a, b, c;
	char d;
	long e;
};

struct T2 {
	struct T3 {
		int a;
		int arr[12];
	} inner;
};

enum {
	CONST = offsetof_builtin(struct T, e),
	CONST2 = offsetof_builtin(struct T2, inner.arr[1 << 1])
};

int main(void) {
	_Static_assert(CONST == 16, "");
	assert(offsetof_macro(struct T, e) == 16);
	assert(offsetof_builtin(struct T, e) == 16);
	assert(offsetof_builtin(struct T2, inner.a) == 0);
	assert(offsetof_builtin(struct T2, inner.arr[0]) == 4);
	int idx = 2;
	assert(offsetof_builtin(struct T2, inner.arr[idx]) == 4 + idx * 4);
}
