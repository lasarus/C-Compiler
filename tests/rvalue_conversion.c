#include <assert.h>

int *ptr = (int *const)&ptr;

int main(void) {
	const unsigned long a = {1};
	assert(a << 2 == 4);
}
