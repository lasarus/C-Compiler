#include <assert.h>

int main() {
	int a = 77, b = 077;
	assert(a != b);
	assert(b == 0x3f);
}