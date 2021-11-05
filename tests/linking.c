#include <assert.h>

int f(void)
{
	extern int a;
	return a;
}

int a;

int main() {
	a = 10;
	assert(f() == 10);
}
