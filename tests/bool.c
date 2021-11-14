#include <assert.h>
#include <stdbool.h>

struct {
	_Bool b;
} S = { 1 };

struct C {
	_Bool a : 1;
	unsigned b : 20;
	_Bool c : 1;
};

_Static_assert(sizeof(struct C) == 4, "");

bool ret_true(void) {
	return true;
}

bool ret_false(void) {
	return false;
}

int main() {
	assert(ret_true() || ret_false());
	assert(!ret_false());
	assert(!(ret_true() && ret_false()));
	assert(!(ret_true() && ret_false()));

	int x = 2;
	_Bool b = x;
	assert(b == 1);
}
