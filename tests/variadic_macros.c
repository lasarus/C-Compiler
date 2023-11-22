#include <string.h>
#include <assert.h>
#include <stdio.h>

#define F(fmt, ...) snprintf(NULL, 0, fmt, ##__VA_ARGS__)

int main(void) {
	F("Test");
	F("Test %s", "Test2");
}
