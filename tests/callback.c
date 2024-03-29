#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

// This only failed when linking against glibc.
// Because codegen didn't take into account that
// the rbx register is callee saved.

void atexit_func(void) {
    printf("");
}

int compare_strings(const void *p, const void *q) {
    return strcmp(*(const char **)p, *(const char **)q);
}

int main(void) {
    atexit(atexit_func);

	const char *strings[] = {
		"Tok",
		"oAj",
		"yOa",
		"byX",
		"EL0",
	};

	qsort(strings, sizeof strings / sizeof *strings, sizeof(*strings), compare_strings);

	assert(strcmp(strings[0], "EL0") == 0);
}
