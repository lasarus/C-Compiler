#include <assert.h>

int main(void) {
	int a = 1, b = 2;
	a += (a += 1, b = 1);
	assert(a == 3);
}
