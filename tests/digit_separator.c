#include <assert.h>

int main(void) {
	assert(1'000'000 == 1000000);
	assert(1'0.00'000 == 10.0);
}
