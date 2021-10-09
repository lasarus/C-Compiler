#include <assert.h>

int main() {
	char a = 10, b = 20;

	a ^= b;

	assert(a == (10 ^ 20));
}
