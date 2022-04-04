#include <stdio.h>
#include <assert.h>
#include <string.h>

int main() {
	assert('\n' == 10);
	assert('\0' == 0);
	assert('\064' == 064);

	assert("\a"[0] == 7);

	assert(strcmp("str\12345", "strS45") == 0);

#define STR(x) #x
	assert(strcmp(STR(L'\x123'), "L'\\x123'") == 0);
}
