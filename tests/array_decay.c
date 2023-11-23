#include <assert.h>

struct T {
	int a[1];
};

struct T func(void) {
	return (struct T){{1}};
}

int main(void) {
	assert(func().a[0] == 1);
	assert(++func().a[0] == 2);
}
