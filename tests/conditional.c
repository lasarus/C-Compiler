#include <string.h>

const void *f(const void *a, char *b) {
	return a ? a : b;
}

int main(void) {
	char *str1 = "Hello";
	const char *str2 = "Hello";
	const char *str3 = 0 ? str1 : str2;
}
