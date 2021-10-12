#include <assert.h>
#include <stdlib.h>

void func(int *ptr) {
	ptr--;
	ptr++;

	ptr += (char)10;
	ptr += (short)10;
	ptr -= (char)10;
}

int main() {
	int arr[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	int *ptr = arr + 5;
	*(ptr - 5) = 20;

	assert(arr[0] == 20);

	func(NULL);
}
