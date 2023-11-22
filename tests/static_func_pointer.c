#include <assert.h>

int function(int i) {
	return i;
}

typedef int (*func_type)(int);

func_type new_func = function;

int arr[] = {1, 2, 3};

void *table[] = {
	(void *)&function,
	(void *)&arr[0],
	(void *)&arr[2],
	&arr[3],
};

int main(void) {
	assert(new_func(10) == 10);
	assert(((func_type)table[0])(10) == 10);
}
