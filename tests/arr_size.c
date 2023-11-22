#include <assert.h>

#define SIZE 10

extern int arr[SIZE];
int arr[] = { 11, 34, };

int main(void) {
	assert(sizeof arr == SIZE * sizeof (int));
	return 0;
}
