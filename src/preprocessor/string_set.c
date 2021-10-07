#include "string_set.h"

#include <common.h>

#include <stdlib.h>
#include <string.h>

static void string_set_append(struct string_set *a, char *str) {
	if (a->n >= a->capacity) {
		a->capacity = MAX(a->capacity * 2, 4);
		a->strings = realloc(a->strings, sizeof *a->strings * a->capacity);
	}

	a->strings[a->n++] = str;
}

struct string_set string_set_intersection(struct string_set a, struct string_set b) {
	struct string_set ret = {0};

	int a_ptr = 0, b_ptr = 0;

	while (a_ptr < a.n && b_ptr < b.n) {
		int cmp = strcmp(a.strings[a_ptr], b.strings[b_ptr]);

		if (cmp == 0) {
			string_set_append(&ret, a.strings[a_ptr]);
			a_ptr++;
			b_ptr++;
		} else if (cmp < 0) {
			a_ptr++;
		} else {
			b_ptr++;
		}
	}

	return ret;
}

struct string_set string_set_union(struct string_set a, struct string_set b) {
	struct string_set ret = {0};

	int a_ptr = 0, b_ptr = 0;

	while (a_ptr < a.n && b_ptr < b.n) {
		int cmp = strcmp(a.strings[a_ptr], b.strings[b_ptr]);

		if (cmp == 0) {
			string_set_append(&ret, a.strings[a_ptr]);
			a_ptr++;
			b_ptr++;
		} else if (cmp < 0) {
			string_set_append(&ret, a.strings[a_ptr]);
			a_ptr++;
		} else {
			string_set_append(&ret, b.strings[b_ptr]);
			b_ptr++;
		}
	}

	for (; a_ptr < a.n; a_ptr++)
		string_set_append(&ret, a.strings[a_ptr]);
	for (; b_ptr < b.n; b_ptr++)
		string_set_append(&ret, b.strings[b_ptr]);

	return ret;
}

struct string_set string_set_dup(struct string_set a) {
	struct string_set ret = a;
	ret.strings = malloc(sizeof *ret.strings * ret.capacity);
	memcpy(ret.strings, a.strings, sizeof *ret.strings * ret.capacity);
	return ret;
}

void string_set_free(struct string_set a) {
	free(a.strings);
}

void string_set_insert(struct string_set *a, char *str) {
	int s = 0, cmp = 0;
	for (; s < a->n; s++) {
		if ((cmp = strcmp(a->strings[s], str)) <= 0)
			break;
	}

	if (cmp == 0)
		return;

	string_set_append(a, NULL);
	/* if (a->n >= a->capacity) { */
	/* 	a->capacity = MAX(a->capacity * 2, 4); */
	/* 	a->strings = realloc(a->strings, sizeof *a->strings * a->capacity); */
	/* } */

	/* a->n++; */
	for (int i = a->n - 1; i > s; i--) {
		a->strings[i] = a->strings[i - 1];
	}

	a->strings[s] = str;
}

int string_set_contains(struct string_set a, char *str) {
	for (int i = 0; i < a.n; i++) {
		if (strcmp(a.strings[i], str) == 0)
			return 1;
	}

	return 0;
}
