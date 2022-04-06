#include <stdio.h>
#include <assert.h>
#include <string.h>

#define STR(X) #X
#define GLUE(X, Y) X ## Y

#define VA_STR(...) VA_XSTR(__VA_ARGS__)
#define VA_XSTR(...) #__VA_ARGS__
int main() {
	// Testing strcmp first, just to be sure.
	assert(strcmp("Hello", "Hello") == 0);
	assert(strcmp("Fr5tlPS93T", "Fr5tlPS93T") == 0);
	// Testing STR(X)
	assert(strcmp(STR(HELLO THERE!), "HELLO THERE!") == 0);
	assert(strcmp(STR(GLUE(AAA, BBB)), "GLUE(AAA, BBB)") == 0);
	// Testing stringification of strings.
	assert(strcmp(STR("Hello World!\n"), "\"Hello World!\\n\"") == 0);

	// Bug with stringify overwriting previous stringification.
	assert(strcmp(STR(A) STR(70), "A70") == 0);

	assert(strcmp(VA_STR(A, B), "A, B") == 0);
	assert(strcmp(VA_STR(), "") == 0);
}
