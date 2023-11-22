#include <assert.h>

int main(void) {
	char c = 1;
	c += 123l;
	assert(c == 124);
	assert(sizeof(++c) == 1);
	assert(sizeof(c += 1) == 1);
	assert(sizeof(c++) == 1);

	int i = 10;
	assert(i++ == 10);

	double d = 1;
	assert(sizeof(d++) == 8);
	d++;
	assert(d >= 1.9 && d <= 2.1);
}
