#include <assert.h>

#define COMBINE(X) START_ ## X

#if '\xff' > 0
#else
#endif

int main() {
	int COMBINE(HELLO) = 10;
	COMBINE(HELLO) = 30;
	assert(COMBINE(HELLO) == 30);
}
