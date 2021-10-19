#include <assert.h>
#include <stdbool.h>

bool ret(void) {
	return true;
}

int main() {
	assert(ret() == true);
}
