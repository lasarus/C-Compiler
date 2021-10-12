#include <assert.h>

int function(int i) {
	return i;
}

typedef int (*func_type)(int);

func_type new_func = function;

int main() {
	assert(new_func(10) == 10);
}
