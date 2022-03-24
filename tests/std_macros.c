#include <assert.h>
#include <string.h>
#include <stdio.h>

int main() {
	assert(strlen(__DATE__) == 11);
	assert(strlen(__TIME__) == 8);
}
