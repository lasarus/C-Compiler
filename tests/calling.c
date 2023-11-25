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

struct T2 { char a[10]; };
struct T2 ret(void) {
  return (struct T2){10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
}

struct T3 {
	long a, b;
};

struct T3 ret3(void) {
	return (struct T3) { 10, 20 };
}

int main(void) {
	assert(sum_a((struct a) { 1, 2, 3 }) == 6);
	ret_struct();

	assert(ret3().a == 10);
	assert(ret3().b == 20);
}
