#include "string_set.h"

#include <common.h>

#include <stdlib.h>
#include <string.h>

static void string_set_append(struct string_set *a, char *str) {
	ADD_ELEMENT(a->size, a->cap, a->strings) = str;
}

struct string_set string_set_intersection(struct string_set a, struct string_set b) {
	struct string_set ret = {0};

	int a_ptr = 0, b_ptr = 0;

	while (a_ptr < a.size && b_ptr < b.size) {
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

	while (a_ptr < a.size && b_ptr < b.size) {
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

	for (; a_ptr < a.size; a_ptr++)
		string_set_append(&ret, a.strings[a_ptr]);
	for (; b_ptr < b.size; b_ptr++)
		string_set_append(&ret, b.strings[b_ptr]);

	return ret;
}

struct string_set string_set_dup(struct string_set a) {
	struct string_set ret = a;
	ret.strings = malloc(sizeof *ret.strings * ret.cap);
	memcpy(ret.strings, a.strings, sizeof *ret.strings * ret.cap);
	return ret;
}

void string_set_free(struct string_set a) {
	free(a.strings);
}

void string_set_insert(struct string_set *a, char *str) {
	if (!a->size) {
		string_set_append(a, str);
		return;
	}

	int s = 0, cmp = 0;
	for (; s < a->size; s++) {
		if ((cmp = strcmp(a->strings[s], str)) <= 0)
			break;
	}

	if (cmp == 0)
		return;

	string_set_append(a, NULL);

	for (int i = a->size - 1; i > s; i--) {
		a->strings[i] = a->strings[i - 1];
	}

	a->strings[s] = str;
}

int string_set_contains(struct string_set a, char *str) {
	for (int i = 0; i < a.size; i++) {
		if (strcmp(a.strings[i], str) == 0)
			return 1;
	}

	return 0;
}
