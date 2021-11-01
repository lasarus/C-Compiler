#include <assert.h>

// This is UB, but should be supported anyways.
#define A 1
#define A 2
#define A 3

#define COMBINE(X) START_ ## X

#if '\xff' > 0
#else
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
}

#if defined(BBBB)
#elif defined(AAAA)
#else
#endif
