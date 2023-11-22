#include <assert.h>

int *a = &((int []) {1, 2})[1] - 1;

int func(int *ptr) {
	return ptr[0] + ptr[1] + ptr[2];
}

int main(void) {
	assert(func((int[]){1, 2, 3}) == 6);
	assert(*a == 1);
}
