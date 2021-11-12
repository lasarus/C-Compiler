#include <assert.h>

struct T {
	int n, arr[];
};

struct T ret_struct() {
	return (struct T) {1};
}

struct a {
	char a, b, c;
};

int sum_a(struct a a) {
	return a.a + a.b + a.c;
}

int main() {
	assert(sum_a((struct a) { 1, 2, 3 }) == 6);
	ret_struct();
}
