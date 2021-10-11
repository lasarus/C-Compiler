#include <assert.h>
#include <stdlib.h>

void func(int *ptr) {
	ptr--;
	ptr++;

	ptr += (char)10;
	ptr += (short)10;
	ptr -= (char)10;
}

int main() {
	func(NULL);
}
