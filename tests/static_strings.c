#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
	static const char *str = "Hello world";
	static const char str_arr[] = "Hello world";
	char str_arr_local[] = "Hello world";
	assert(strcmp(str, "Hello world") == 0);
	assert(sizeof str_arr == 12);
	assert(strcmp(str_arr, "Hello world") == 0);
	assert(strcmp(str_arr_local, "Hello world") == 0);
}
