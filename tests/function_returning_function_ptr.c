#include <assert.h>

int f1() {
	return 10;
}

int (*f2())() {
	return f1;
}

int main(void) {
	assert(f2()() == 10);
}
