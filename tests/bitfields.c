#include <assert.h>
#include <stdio.h>
#include <stdint.h>

struct S {
	int a : 2, b : 2;
	int : 1, k : 3,:2,:4;
	int l : 3, z : 12;
	int c;
	union {
		unsigned int shared_1 : 2;
		unsigned int shared_2 : 2;
		unsigned int shared_3 : 2;
	};
};

typedef union _ND_OPERAND_ACCESS
{
	uint8_t Access;
	struct
	{
		uint8_t Read : 2; // The operand is read.
		uint8_t Write : 2; // The operand is written.
		uint8_t CondRead : 2; // The operand is read only under some conditions.
		uint8_t CondWrite : 2; // The operand is written only under some conditions.
		uint8_t Prefetch : 2; // The operand is prefetched.
	};
} ND_OPERAND_ACCESS;

int main() {
	struct S s;
	s.b = 1 + 4;
	s.shared_1 = 3;

	assert (s.b == 1);
	assert (s.shared_2 == 3);
	assert (s.shared_2 == s.shared_3);

	return 0;
}
