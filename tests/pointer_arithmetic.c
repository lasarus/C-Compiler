#include <assert.h>
#include <stdlib.h>
#include <stddef.h>

void func(int *ptr) {
	ptr--;
	ptr++;

	ptr += (char)10;
	ptr += (short)10;
	ptr -= (char)10;
}

int main(void) {
	int arr[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	int *ptr = arr + 5;
	*(ptr - 5) = 20;

	assert(arr[0] == 20);

	int *null_ptr = NULL;
	assert(null_ptr == 0);

	assert(&arr[5] - &arr[3] == 2);
	assert(&arr[3] - &arr[5] == -2);

	func(NULL);

	assert(sizeof(&arr[0] - &arr[3]) == sizeof(ptrdiff_t));
}
