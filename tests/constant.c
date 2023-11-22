#include <assert.h>
#include <stdio.h>

float f1 = 1.5;
//float f2 = (1.0, 3.4);
float f3 = 0.0 ? 1.0 : 2.2;

int main(void) {
	_Static_assert(sizeof(sizeof(int)) == 8, "");

	int a = 0 ? 10 * (0 / 0) : 1;

	static int xx=3;
	int z = 1 ? &xx : &xx;

	static unsigned char prec[256];
	static int tok = '*';
	assert(prec + tok == prec + (int)'*');
}
