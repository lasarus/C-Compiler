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

#define TEST_DEF

  #if defined(NOT_DEFINED)
#error
  #elif defined(TEST_DEF)
//#error
  #elif defined(NOT_DEFINED)
#error
  #else
	#error
  #endif

  #if defined(NOT_DEFINED)
#error
  #elif defined(TEST_DEF)
//#error
  #elif !defined(NOT_DEFINED)
#error
  #endif


#define AAAA

# if defined(AAAA)
#  if defined(BBBB)
#   error
#  else
//#   error
#  endif
# elif !defined(CCCC)
#   error
# endif

#if 0
#!
#endif


	int abcd = 0;
#define join_va_args(x, ...)            x ## __VA_ARGS__

	join_va_args(ab, cd) = 1;
	assert(abcd == 1);

	join_va_args(abcd,) = 2;
	assert(abcd == 2);

	return 0;
}

#if defined(BBBB)
#elif defined(AAAA)
#else
#endif

#if 4294967295 == 4294967295U
#else
#error
#endif
