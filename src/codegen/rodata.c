#include "rodata.h"
#include "codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct entry {
	const char *name;
	label_id id;
};

struct entry *entries = NULL;
int n_entries = 0;

label_id rodata_register(const char *str) {
	for (int i = 0; i < n_entries; i++) {
		if (strcmp(entries[i].name, str) == 0) {
			return entries[i].id;
		}
	}

	n_entries++;
	entries = realloc(entries, n_entries * sizeof *entries);
	entries[n_entries - 1].name = str;
	entries[n_entries - 1].id = n_entries - 1;

	return n_entries - 1;
}

const char *rodata_get_label_string(label_id id) {
	static char *buffer = NULL;

	if (!buffer)
		buffer = malloc(128);

	sprintf(buffer, ".L_rodata%d", id);
	return buffer;
}

void rodata_codegen(void) {
	for (int i = 0; i < n_entries; i++) {
		emit("%s:", rodata_get_label_string(entries[i].id));
		emit(".string \"%s\"", entries[i].name);
	}
}
