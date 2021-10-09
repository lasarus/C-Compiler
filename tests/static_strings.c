#include <stdio.h>
#include <string.h>
#include <assert.h>

void function(const void *data, unsigned int len) {
	(void)len, (void)data;
}

int main() {
	static const char *str = "Hello world";
	static const char str_arr[] = "Hello world";
	char str_arr_local[] = "Hello world";
	assert(strcmp(str, "Hello world") == 0);
	assert(sizeof str_arr == 12);
	assert(strcmp(str_arr, "Hello world") == 0);
	assert(strcmp(str_arr_local, "Hello world") == 0);

	function("string", 6);
}
