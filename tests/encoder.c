#include <assert.h>
#include <stdint.h>

int main() {
	unsigned int a = -(unsigned int)4000000000;
	assert(a == (unsigned int)-4000000000);
}
