#include <stdio.h>
#include <assert.h>

double add(double a, double b) {
	return a + b;
}

int main() {
	assert(add(2.3, 4.5) < 6.9);
	assert(add(2.3, 4.5) > 6.7);

	double d = 4.5;
	d++;
	d--;
	d++;
	assert(d >= 5.4 && d <= 5.6);

	long double ld = 4.5; // Only 64-bit, as allowed by the standard.
	assert(ld >= 4.4 && ld <= 4.6);

	int integer = 88;
	float f = integer;

	assert(f >= 87 && f <= 89);

	integer = f / 2;
	assert(integer >= 43 && integer <= 45);

	d = 999999999;
	unsigned long ul = d;
	assert(ul >= 999999998 && ul <= 1000000000);

	{
		double f = 100.0;
		assert(-f == -100.0);
	}

	assert(1 && 1);
	assert(1e+6 > 999999 && 1e+6 < 1000001);
	assert(1e-6 > 0.0000009 && 1e-6 < 0.0000011);
	assert(1e6 > 999999 && 1e6 < 1000001);

	{
		float f = .1f;
	}

	return 0;
}
