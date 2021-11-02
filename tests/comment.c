#include <assert.h>
#include <string.h>

char *str = "a\"/*b";

int main() {
	assert(strcmp(str, "a\"/" "*b") == 0);
}
