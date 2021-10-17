#include <assert.h>
#include <stdio.h>
#include <stdint.h>

struct S {
	int a : 2, b : 2;
	int : 1, k : 3,:2,:4;
	unsigned int val : 2;
	int l : 3, z : 12;
	int c;
	union {
		unsigned int shared_1 : 2;
		unsigned int shared_2 : 2;
		unsigned int shared_3 : 2;
	};
};

int main() {
	struct S s;
	s.val = 0;

	for (int i = 0; i < 10; i++) {
		assert(i % 4 == s.val);
		s.val += 1;
	}

	s.val = 0;

	for (int i = 0; i < 10; i++) {
		assert(i % 4 == s.val);
		s.val++;
	}

	s.a = 4 + 1;
	assert(s.a == 1);

	s.shared_1 = 3;
	assert(s.shared_2 == s.shared_3);
	assert(s.shared_2 == 3);
}
