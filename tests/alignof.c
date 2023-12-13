#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static _Alignas(256) char sc2;
static _Alignas(int) char sc1;

int align_int = 10;

int main(void) {
	int i;
	assert(_Alignof(i) == 4);
	assert(_Alignof(int) == 4);

	_Alignas(int) char c1;
	_Alignas(256) char c2;
	_Alignas(1024) char c3;

	assert((uint64_t)(&c1) % _Alignof(int) == 0);
	assert((uint64_t)(&c2) % 256 == 0);
	assert((uint64_t)(&c3) % 1024 == 0);

	assert((uint64_t)(&sc1) % _Alignof(int) == 0);
	assert((uint64_t)(&sc2) % 256 == 0);
	assert((uint64_t)(&align_int) % _Alignof(int) == 0);

	_Alignas(0) int a;
	assert((uint64_t)&a % _Alignof(int) == 0);

	return 0;
}
