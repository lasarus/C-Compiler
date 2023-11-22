#include <assert.h>

int f(void)
{
	extern int a;
	return a;
}

int a;

int main(void) {
	a = 10;
	assert(f() == 10);
}
