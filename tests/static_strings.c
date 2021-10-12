#include <stdio.h>
#include <string.h>
#include <assert.h>

void function(const void *data, unsigned int len) {
	(void)len, (void)data;
}

int main() {
	static const char *str = "Hello world";
	static const char str_arr[] = "Hello world";
	static const char str_arr2[40] = "Hello world";
	struct {
		char arr1[10];
	} aggregate = { "Hmmm" };
	char str_arr_local[] = "Hello world";
	assert(strcmp(str, "Hello world") == 0);
	assert(sizeof str_arr == 12);
	assert(sizeof str_arr2 == 40);
	assert(strcmp(str_arr, "Hello world") == 0);
	assert(strcmp(str_arr2, "Hello world") == 0);
	assert(strcmp(str_arr_local, "Hello world") == 0);
	assert(strcmp(aggregate.arr1, "Hmmm") == 0);

	function("string", 6);
}
