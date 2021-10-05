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

label_id register_string(const char *str) {
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

const char *get_label_name(label_id id) {
	static char *buffer = NULL;

	if (!buffer)
		buffer = malloc(128);

	sprintf(buffer, ".L_rodata%d", id);
	return buffer;
}

void codegen_rodata(void) {
	for (int i = 0; i < n_entries; i++) {
		EMIT("%s:", get_label_name(entries[i].id));
		EMIT(".string \"%s\"", entries[i].name);
	}
}
