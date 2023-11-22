#include <assert.h>

int main(void) {
	char a = 10, b = 20;

	a ^= b;

	assert(a == (10 ^ 20));
}
