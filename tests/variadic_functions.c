#include <stdio.h>
#include <assert.h>

int sum(int n, ...) {
	__builtin_va_list va;
	__builtin_va_start(va, n);

	int sum = 0;
	for (int i = 0; i < n; i++) {
		sum += __builtin_va_arg(va, int);
	}
	return sum;
}

int sum_8p(int n, int a, int b, int c, int d, int e, int f, int g, int h, ...) {
	__builtin_va_list va;
	__builtin_va_start(va, h);

	int sum = a + b + c + d + e + f + g + h;
	for (int i = 0; i < n - 8; i++) {
		sum += __builtin_va_arg(va, int);
	}
	return sum;
}

struct large_struct {
	int members[10];
};

int sum_structs(int n, ...) {
	__builtin_va_list va;
	__builtin_va_start(va, n);

	int sum = 0;
	for (int i = 0; i < n; i++) {
		struct large_struct s = __builtin_va_arg(va, struct large_struct);
		for (int j = 0; j < 10; j++) {
			sum += s.members[j];
		}
	}
	return sum;
}

int main() {
	assert(sum(2, 100, 200) == 300);
	assert(sum(3, 100, 200, 300) == 600);
	assert(sum(6, 100, 200, 300, 400, 500, 600) == 2100);
	assert(sum_8p(9, 1, 2, 3, 4, 5, 6, 7, 8, -36) == 0);
	assert(sum_structs(2, (struct large_struct) {
				{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
			}, (struct large_struct) {
				{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
			}) == 110);
}
