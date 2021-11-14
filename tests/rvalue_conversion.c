#include <assert.h>

int main() {
	const unsigned long a = {1};
	assert(a << 2 == 4);
}
