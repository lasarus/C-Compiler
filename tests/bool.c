#include <assert.h>
#include <stdbool.h>

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
}
