#include <assert.h>

double add(double a, double b) {
	return a + b;
}

int main() {
	assert(add(2.3, 4.5) < 6.9);
	assert(add(2.3, 4.5) > 6.7);
	return 0;
}
