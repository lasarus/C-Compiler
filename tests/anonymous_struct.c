#include <assert.h>

struct T {
    struct {
        int a, b;
    };
	int c, d;
} t = {.c = 1, .d = 2};

int main(void) {
	assert(t.c == 1 && t.d == 2);
}
