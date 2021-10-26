#include <assert.h>

int main() {
	int var = 1;
	goto label;
	var = 4;
label:
	var *= 2;
unused:
	assert(var == 2);
}
