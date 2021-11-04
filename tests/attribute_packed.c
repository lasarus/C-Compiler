#include <assert.h>

struct T_aligned {
	char a;
	int b;
	long c;
};

#undef __attribute__

struct T_packed {
	char a;
	int b;
	long c;
} __attribute__((packed));

struct __attribute__((packed)) T_packed2 {
	char a;
	int b;
	long c;
};

int main() {
	struct T_packed t;
	t.a = 10;
	t.b = 20;
	t.c = 30;
	assert(t.a == 10);
	assert(t.b == 20);
	assert(t.c == 30);
	assert(sizeof(struct T_aligned) == 16);
	assert(sizeof(struct T_packed) == 1 + 4 + 8);
	assert(sizeof(struct T_packed2) == 1 + 4 + 8);
}
