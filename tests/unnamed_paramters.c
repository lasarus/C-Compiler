#include <assert.h>

int func(int, int b, _Bool);

int func(int, int b, _Bool) {
	return b;
}

int func2(int, int, _Bool) {
	return 10;
}

int main(void) {
	assert(func(3, 4, 1) == 4);
	assert(func2(1, 1, 0) == 10);
}
