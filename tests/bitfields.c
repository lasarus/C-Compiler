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

	{
		struct {
			unsigned x : 1, y : 1;
		} t = {.y = 1};

		assert(t.y == 1);
	}
	{
		static struct {
			unsigned x : 1, y : 1;
		} t = {.y = 1};

		assert(t.y == 1);
	}
	{
		struct T {
			int a : 1, : 0, b : 1;
		};
		_Static_assert(sizeof (struct T) == 8, "");
	}
	{
		static struct T {
			int a : 2;
			int :1;
			int b : 2;
		} a = {1, 1};
		assert(a.a == 1 && a.b == 1);
	}
	{
		static struct T {
			char c : 2, c2 : 4;
		} t;
		t.c = 1;
		t.c2 = 5;
		assert(t.c == 1);
		assert(t.c2 == 5);
	}
	{
		// This tests for a bug where reading a bitfield
		// caused a neighbouring variable to be overwritten.
		struct T {
			int a:4;
		} t = { 2 };

		char buffer[256];
		int len = sprintf(buffer, "%d\n", t.a);
		assert(len > 0);
	}
	{
		struct {
			unsigned a : 23;
		} t;

		assert(t.a > -1);
	}

	{
		struct {
			unsigned short a : 11;
			unsigned char b : 4;
			unsigned char c : 3;
		} t;

		assert(sizeof t == 4);
	}

	{
		struct {
			unsigned short a : 11;
			unsigned char b : 4;
			unsigned char c : 3;

			unsigned short d : 15;

			unsigned char padding[8];
		} t = { 1, 1, 1, 1, {0} };

		assert(*(uint64_t *)&t == 0x100010801);
	}

	{
		struct T {
			struct T2 {
				int : 0;
			};
		};

		_Static_assert(_Alignof(struct T) == 1, "");
		_Static_assert(sizeof(struct T) == 0, "");
	}
}
