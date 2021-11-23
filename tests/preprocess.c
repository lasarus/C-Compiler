#include <assert.h>
#include <string.h>

// This is UB, but should be supported anyways.
#define A 1
#define A 2
#define A 3

#define COMBINE(X) START_ ## X
#define CAT(A, B) A ## B

#define CAT3(X) A##_##X

#if '\xff' > 0
#else
#endif

#if 0
// Invalid escape sequences should be ignored.
const char *str = "adfadf\\\\asdfasdf\dd\d\\d\d\asd\s";
#endif

int main() {
	int COMBINE(HELLO) = 10;
	COMBINE(HELLO) = 30;
	assert(COMBINE(HELLO) == 30);

	int a = 0;
#define a (a)
	a = 10;
	#undef a
	assert(a == 10);

	assert(CAT(0x, ff) == 0xff);

	const char *include = NULL;

	int CAT3(HELLO) = 10;
	A_HELLO = 20;

#
	include = "Hello";
assert(strcmp(include, "Hello") == 0);

  #if defined(__LLP64__)
#error
  #elif defined(__LP64__)
//#error
  #elif defined(__ILP32__)
#error
  #else
	#error
  #endif

  #if defined(__LLP64__)
#error
  #elif defined(__LP64__)
//#error
  #elif !defined(__ILP32__)
#error
  #endif

	return 0;
}

#if defined(BBBB)
#elif defined(AAAA)
#else
#endif
