#include "rodata.h"
#include "codegen.h"

#include <common.h>
#include <arch/x64.h>
#include <parser/expression.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct entry {
	enum entry_type {
		ENTRY_STR,
		ENTRY_LABEL_NAME
	} type;
	const char *name;
	label_id id;
};

static struct entry *entries = NULL;
static int n_entries = 0;

label_id label_register(enum entry_type type, const char *str) {
	if (type == ENTRY_STR) {
			for (int i = 0; i < n_entries; i++) {
				if (strcmp(entries[i].name, str) == 0) {
					return entries[i].id;
				}
			}
	}

	n_entries++;
	entries = realloc(entries, n_entries * sizeof *entries);
	entries[n_entries - 1].name = str;
	entries[n_entries - 1].id = n_entries - 1;
	entries[n_entries - 1].type = type;

	return n_entries - 1;
}

label_id rodata_register(const char *str) {
	return label_register(ENTRY_STR, str);
}

const char *rodata_get_label_string(label_id id) {
	if (entries[id].type == ENTRY_STR) {
		static char *buffer = NULL;

		if (!buffer)
			buffer = malloc(128);

		sprintf(buffer, ".L_rodata%d", id);
		return buffer;
	} else if (entries[id].type == ENTRY_LABEL_NAME) {
		return entries[id].name;
	} else {
		NOTIMP();
	}
}

void rodata_codegen(void) {
	for (int i = 0; i < n_entries; i++) {
		if (entries[i].type == ENTRY_STR) {
			emit("%s:", rodata_get_label_string(entries[i].id));
			emit(".string \"%s\"", entries[i].name);
		}
	}
}

label_id register_label_name(const char *str) {
	return label_register(ENTRY_LABEL_NAME, str);
}

struct static_var {
	const char *label;
	struct type *type;
	struct initializer *init;
	int global;
};

static struct static_var *static_vars = NULL;
static int static_vars_n, static_vars_cap;

void data_register_static_var(const char *label, struct type *type, struct initializer *init, int global) {
	if (static_vars_n >= static_vars_cap) {
		static_vars_cap = MAX(static_vars_cap * 2, 4);
		static_vars = realloc(static_vars,
							  sizeof *static_vars * static_vars_cap);
	}

	static_vars[static_vars_n++] = (struct static_var) {
		.label = label,
		.type = type,
		.init = init,
		.global = global
	};
}

void codegen_initializer(struct type *type,
						 struct initializer *init) {
	// TODO: Make this more portable.
	int size = calculate_size(type);
	if (size > 4096) {
		printf("Size: %d\n", size);
		NOTIMP();
	}
	uint8_t buffer[size];
	label_id labels[size];
	int is_label[size];
	for (int i = 0; i < size; i++) {
		buffer[i] = 0;
		is_label[i] = 0;
		labels[i] = 1337;
	}

	for (int i = 0; i < init->n; i++) {
		struct init_pair *pair = init->pairs + i;
		int offset = pair->offset;

		struct constant *c = expression_to_constant(pair->expr);
		if (!c)
			ERROR("Static initialization can't contain non constant expressions! %d", pair->expr->type);

		switch (c->type) {
		case CONSTANT_TYPE:
			constant_to_buffer(buffer + offset, *c);
			break;

		case CONSTANT_LABEL_POINTER:
			is_label[offset] = 1;
			labels[offset] = c->label;
			break;

		case CONSTANT_LABEL:
			NOTIMP();
			break;

		default:
			NOTIMP();
		}
	}

	for (int i = 0; i < size; i++) {
		if (is_label[i]) {
			emit(".quad %s", rodata_get_label_string(labels[i]));
			i += 7;
		} else {
			//TODO: This shouldn't need an integer cast.
			// But right now I can't be bothered to implement
			// implicit integer casts for variadic functions.
			emit(".byte %d", (int)buffer[i]);
		}
	}
}

void codegen_static_var(struct static_var *static_var) {
	set_section(".data");
	if (static_var->global)
		emit(".global %s", static_var->label);
	if (static_var->init) {
		emit("%s:", static_var->label);

		codegen_initializer(static_var->type,
							static_var->init);
	} else {
		emit("%s:", static_var->label);

		emit(".zero %d", calculate_size(static_var->type));
	}
	set_section(".text");
}

void data_codegen(void) {
	for (int i = 0; i < static_vars_n; i++) {
		codegen_static_var(static_vars + i);
	}
}
