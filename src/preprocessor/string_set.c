#include "string_set.h"

#include <common.h>

#include <stdlib.h>
#include <string.h>

struct string_set string_set_intersection(struct string_set a, struct string_set b) {
	struct string_set ret = {0};

	for (int i = 0; i < a.n; i++) {
		if (string_set_index_of(b, a.strings[i]) != -1)
			string_set_insert(&ret, a.strings[i]);
	}

	return ret;
}

struct string_set string_set_union(struct string_set a, struct string_set b) {
	struct string_set ret = {0};

	for (int i = 0; i < a.n; i++) {
		string_set_insert(&ret, a.strings[i]);
	}
	
	for (int i = 0; i < b.n; i++) {
		if (string_set_index_of(a, b.strings[i]) != -1)
			string_set_insert(&ret, b.strings[i]);
	}

	return ret;
}

struct string_set string_set_dup(struct string_set a) {
	struct string_set ret = {0};

	for (int i = 0; i < a.n; i++) {
		string_set_insert(&ret, a.strings[i]);
	}

	return ret;
}

void string_set_free(struct string_set a) {
	free(a.strings);
}

void string_set_insert(struct string_set *a, char *str) {
	if (a->n >= a->capacity) {
		a->capacity = MAX(a->capacity * 2, 4);
		a->strings = realloc(a->strings, sizeof *a->strings * a->capacity);
	}

	a->n++;

	a->strings[a->n - 1] = str;
}

int string_set_index_of(struct string_set a, char *str) {
	for (int i = 0; i < a.n; i++) {
		if (strcmp(a.strings[i], str) == 0)
			return i;
	}

	return -1;
}
