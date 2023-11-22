#include <stdio.h>
#include <assert.h>

unsigned long byteswap(unsigned long x) {
#define __bswap_constant_64(x)					\
	((((x) & 0xff00000000000000ull) >> 56)		\
	 | (((x) & 0x00ff000000000000ull) >> 40)	\
	 | (((x) & 0x0000ff0000000000ull) >> 24)	\
	 | (((x) & 0x000000ff00000000ull) >> 8)		\
	 | (((x) & 0x00000000ff000000ull) << 8)		\
	 | (((x) & 0x0000000000ff0000ull) << 24)	\
	 | (((x) & 0x000000000000ff00ull) << 40)	\
	 | (((x) & 0x00000000000000ffull) << 56))
	return __bswap_constant_64(x);
}

unsigned long l = 0Xff00000000000000ull;

int main(void) {
#ifdef __LP64__
	assert(__bswap_constant_64(0x1490d7d3f094a6e3ull) == 0xe3a694f0d3d79014ull);
	assert(byteswap(0x1490d7d3f094a6e3ull) == 0xe3a694f0d3d79014ull);
#else
	return 0;
#endif
}
