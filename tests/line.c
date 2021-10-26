#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MACRO __LINE__
#define FUNC_MACRO(A, B) A + B

int main() {
	#line 6
	assert(__LINE__ == 6);
	assert(MACRO == 7);

	assert(FUNC_MACRO(MACRO, MACRO) == 18);
	assert(FUNC_MACRO(MACRO,
					  MACRO) == 21);
	#line 2

	assert(__LINE__ == 3);

	#line 4 "notline.c"
	assert(strcmp(__FILE__, "notline.c") == 0);
}
