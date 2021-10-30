#include <assert.h>

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
}
