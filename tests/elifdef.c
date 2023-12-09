#include <assert.h>

#ifdef A
#define VAL 123 
#elifndef B
#define VAL 234
#else
#define VAL 456
#endif

#define B

#ifdef A
#define VAL2 123 
#elifndef B
#define VAL2 234
#else
#define VAL2 456
#endif

#ifdef A
#define VAL3 123 
#elifdef B
#define VAL3 234
#else
#define VAL3 456
#endif

int main(void) {
	assert(VAL == 234);
	assert(VAL2 == 456);
	assert(VAL3 == 234);
}
