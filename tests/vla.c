#include <assert.h>
#include <stdlib.h>
#include <string.h>

int global = 10;
void func1(char arr[][global]) {
	assert(sizeof *arr == global);
}

void func2(int n, char arr[][n * 2]) {
	assert(sizeof *arr == n * 2);
}

void test(int n, int x[n]);
void test(int n, int x[*]);

void func3(int func3) {
	assert(func3 == 10);
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

	func1(NULL);
	func2(100, NULL);
	func3(10);
}
