#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <time.h>

int main() {
	assert(strlen(__DATE__) == 11);
	assert(strlen(__TIME__) == 8);
	printf("Current date: %s and time %s\n", __DATE__, __TIME__);
	return 0;
}
