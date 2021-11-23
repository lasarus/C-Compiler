#include <assert.h>

int if_goto(int val) {
	if (val == 1)
		goto label;

	if (val == 2)
	label: return 1;
	return 0;
}

int main() {
	int var = 1;
	goto label;
	var = 4;
label:
	var *= 2;
unused:
	assert(var == 2);

	assert(if_goto(1) == 1);
	assert(if_goto(2) == 1);
	assert(if_goto(3) == 0);
}
