#include "rodata.h"
#include "codegen.h"

#include <common.h>
#include <arch/x64.h>
#include <parser/expression.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

struct entry {
	enum entry_type {
		ENTRY_STR,
		ENTRY_LABEL_NAME
	} type;
	const char *name;
	label_id id;
};

static struct entry *entries = NULL;
static int entries_size = 0, entries_cap = 0;

label_id label_register(enum entry_type type, const char *str) {
	if (type == ENTRY_STR) {
			for (int i = 0; i < entries_size; i++) {
				if (strcmp(entries[i].name, str) == 0) {
					return entries[i].id;
				}
			}
	}

	int id = entries_size;
	ADD_ELEMENT(entries_size, entries_cap, entries) = (struct entry) {
		.type = type,
		.name = str,
		.id = id
	};

	return id;
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
	for (int i = 0; i < entries_size; i++) {
		if (entries[i].type != ENTRY_STR)
			continue;
		emit("%s:", rodata_get_label_string(entries[i].id));

		emit_no_newline("\t.string \"", entries[i].name);
		const char *str = entries[i].name;
		for (; *str; str++) {
			char buffer[5];
			character_to_escape_sequence(*str, buffer);
			emit_no_newline("%s", buffer);
		}
		emit_no_newline("\"\n");
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
static int static_vars_size, static_vars_cap;

void data_register_static_var(const char *label, struct type *type, struct initializer *init, int global) {
	ADD_ELEMENT(static_vars_size, static_vars_cap, static_vars) = (struct static_var) {
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

	uint8_t *buffer = malloc(sizeof *buffer * size);
	label_id *labels = malloc(sizeof *labels * size);
	int64_t *label_offsets = malloc(sizeof *label_offsets * size);
	int *is_label = malloc(sizeof *is_label * size);

	for (int i = 0; i < size; i++) {
		buffer[i] = 0;
		is_label[i] = 0;
		labels[i] = 0;
		label_offsets[i] = 0;
	}

	for (int i = 0; i < init->size; i++) {
		struct init_pair *pair = init->pairs + i;
		int offset = pair->offset;

		if (offset >= size)
			ERROR("Internal compiler error");

		struct constant *c = expression_to_constant(pair->expr);
		if (!c)
			ERROR("Static initialization can't contain non constant expressions! %d", pair->expr->type);

		switch (c->type) {
		case CONSTANT_TYPE:
			constant_to_buffer(buffer + offset, *c);
			break;

		case CONSTANT_LABEL_POINTER:
			is_label[offset] = 1;
			labels[offset] = c->label.label;
			label_offsets[offset] = c->label.offset;
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
			if (label_offsets[i] == 0)
				emit(".quad %s", rodata_get_label_string(labels[i]));
			else
				emit(".quad %s+%lld", rodata_get_label_string(labels[i]),
					label_offsets[i]);
			i += 7;
		} else {
			int how_long = 0;
			for (; how_long < 8 && i + how_long < size; how_long++) {
				if (is_label[i + how_long])
					break;
			}
			//TODO: This shouldn't need an integer cast.
			// But right now I can't be bothered to implement
			// implicit integer casts for variadic functions.
			if (how_long == 8) {
				emit(".quad %" PRIu64, *(uint64_t *)(buffer + i));
				i += how_long - 1;
			} else {
				emit(".byte %d", (int)buffer[i]);
			}
		}
	}

	free(buffer);
	free(labels);
	free(label_offsets);
	free(is_label);
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
	for (int i = 0; i < static_vars_size; i++) {
		codegen_static_var(static_vars + i);
	}
}
