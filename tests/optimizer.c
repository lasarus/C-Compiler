// This file contains tests that the optimizer/scheduler
// might fail on.
#include <assert.h>

struct T {
	int data;
};

void func(void) {
}

// Error that first occurred in the parse_struct function.
int parse_struct(void) {
	struct T *def = NULL;

	if (1)
		func();
	else
		return def->data;

	return 0;
}

int test2(void) {
	int i = 0;
	long next_arg = 0;

loop:
	i++;
	if (next_arg || i < 0) {
		goto loop;
	}
	return next_arg;
}


static void type_search_member2(int **indices) {
	*indices = (void *)10;
}

void test3(void) {
	int *indices;

	type_search_member2(&indices);

	assert((void *)indices == (void *)10);
}

// Test4
// Bug occured when phi node has two identical arguments.
// The code scheduler would only take one into account
// when doing late scheduling.
static int *get_ptr(void) {
	static int def = 10;
	return &def;
}

int define_map_add2(int define) {
	int *elem = get_ptr();

	if (define) {
	}

	return *elem;
}

void test4(void) {
	int def = 0;
	assert(define_map_add2(def) == 10);
}

// Something to do with scheduling nodes to blocks when they are unused.
void escape_sequence_read2(int *out) {
	const char *start = "\\";
	int result = 0;
	while (result < 1) {
		result = result * 1 + *start;
	}

	*out = result;
}

void test5(void) {
	int out;
	escape_sequence_read2(&out);
}

// Dispatcher
int main(void) {
	parse_struct();
	assert(test2() == 0);
	test3();
	test4();
	test5();
}
