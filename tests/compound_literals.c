#include <assert.h>

int func(int *ptr) {
	return ptr[0] + ptr[1] + ptr[2];
}

int main() {
	assert(func((int[]){1, 2, 3}) == 6);
}
