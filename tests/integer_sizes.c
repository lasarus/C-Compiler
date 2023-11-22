#include <assert.h>

#define __LONG_MAX 0x7fffffffffffffffL
#define CHAR_BIT 8
#define SCHAR_MIN (-128)
#define SCHAR_MAX 127
#define UCHAR_MAX 255
#define SHRT_MIN  (-1-0x7fff)
#define SHRT_MAX  0x7fff
#define USHRT_MAX 0xffff
#define INT_MIN  (-1-0x7fffffff)
#define INT_MAX  0x7fffffff
#define UINT_MAX 0xffffffffU
#define LONG_MIN (-LONG_MAX-1)
#define LONG_MAX __LONG_MAX
#define ULONG_MAX (2UL*LONG_MAX+1)
#define LLONG_MIN (-LLONG_MAX-1)
#define LLONG_MAX  0x7fffffffffffffffLL
#define ULLONG_MAX (2ULL*LLONG_MAX+1)

int main(void) {
	assert(CHAR_BIT == 8);
	assert(SCHAR_MIN == -128);
	assert(SCHAR_MAX == 127);
	assert(UCHAR_MAX == 255);
	assert(SHRT_MIN == -32768);
	assert(SHRT_MAX == 32767);
	assert(USHRT_MAX == 65535);
	assert(INT_MIN == -2147483648);
	assert(INT_MAX == 2147483647);
	assert(UINT_MAX == -1);
	assert(LONG_MIN == -9223372036854775807ll - 1);
	assert(LONG_MAX == 9223372036854775807);
	assert(ULONG_MAX == 18446744073709551615ul);
	assert(LLONG_MIN == -9223372036854775807l - 1);
	assert(LLONG_MAX == 9223372036854775807);
	assert(ULLONG_MAX == 18446744073709551615ull);
}
