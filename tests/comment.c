#include <assert.h>
#include <string.h>

char *str = "a\"/*b";

int main(void) {
	assert(strcmp(str, "a\"/" "*b") == 0);
}
