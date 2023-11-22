#include <assert.h>

int main(void) {
	int a = 77, b = 077;
	assert(a != b);
	assert(b == 0x3f);

	int c = 0b101;
	assert(c == 5);

	int x = 0;
	assert (~x == 0xffffffffffffffff);
		
	return 0;

}
