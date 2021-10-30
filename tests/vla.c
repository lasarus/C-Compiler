#include <assert.h>
#include <stdlib.h>
#include <string.h>

int global = 10;
void func(char arr[][global]) {
	assert(sizeof *arr == global);
}

int main() {
	int a = 10 * 10;
	int arr[a];
	arr[20] = 100;
	arr[99] = 99;
	int arr2[a];
	arr2[0] = 20;
	assert(arr[20] == 100);
	assert(arr[99] == 99);

	assert(sizeof (char [a]) == a);
	assert(sizeof (char [a][a * 2]) == a * a * 2);

	int n = 5;
	assert(sizeof (char[2][n]) == 2 * n);

	char arr3[n][n];
	assert(sizeof arr3 == n * n);

	char *ptrs[1000];
	for (int i = 0; i < 1000; i++) {
		char arr[i + 100];
		memset(arr, 0, i + 100);
		ptrs[i] = arr;
	}
	assert(ptrs[0] - ptrs[999] < 1100);
}
