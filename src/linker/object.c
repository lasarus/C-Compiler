#include "object.h"

#include "common.h"

#include <stdlib.h>
#include <string.h>

static struct object current_object;
static int current_section = -1;

static int get_symbol(label_id label) {
	char buffer[64];

	rodata_get_label(label, sizeof buffer, buffer);

	for (unsigned i = 0; i < current_object.symbol_size; i++) {
		if (current_object.symbols[i].name &&
			strcmp(current_object.symbols[i].name, buffer) == 0)
			return i;
	}

	struct symbol symbol = {
		.section = -1,
	};

	if (label != -1) {
		char buffer[64];

		rodata_get_label(label, sizeof buffer, buffer);

		symbol.name = strdup(buffer);
	}

	ADD_ELEMENT(current_object.symbol_size, current_object.symbol_cap, current_object.symbols) = symbol;

	return current_object.symbol_size - 1;
}

void object_start() {
	current_object = (struct object) { 0 };

	object_set_section(".text");
}

struct object *object_finish() {
	struct object *object = cc_malloc(sizeof *object);

	*object = current_object;

	for (unsigned j = 0; j < object->symbol_size; j++) {
		if (object->symbols[j].section == -1)
			object->symbols[j].global = 1;
	}

	current_object = (struct object) { 0 };

	return object;
}

void object_set_section(const char *section_name) {
	for (unsigned i = 0; i < current_object.section_size; i++) {
		if (strcmp(current_object.sections[i].name, section_name) == 0) {
			current_section = i;
			return;
		}
	}

	struct section *section =
		&ADD_ELEMENT(current_object.section_size, current_object.section_cap, current_object.sections);

	*section = (struct section) {
		.name = strdup(section_name)
	};

	current_section = current_object.section_size - 1;
}

void object_write(uint8_t *data, size_t size) {
	struct section *section = &current_object.sections[current_section];
	uint8_t *dest = ADD_ELEMENTS(section->size, section->cap, section->data, size);

	memcpy(dest, data, size);
}

void object_write_byte(uint8_t imm) {
	struct section *section = &current_object.sections[current_section];
	ADD_ELEMENT(section->size, section->cap, section->data) = imm;
}

void object_write_quad(uint64_t imm) {
	object_write_byte(imm);
	object_write_byte(imm >> 8);
	object_write_byte(imm >> 16);
	object_write_byte(imm >> 24);
	object_write_byte(imm >> 32);
	object_write_byte(imm >> 40);
	object_write_byte(imm >> 48);
	object_write_byte(imm >> 56);
}

void object_write_zero(size_t size) {
	struct section *section = &current_object.sections[current_section];
	memset(ADD_ELEMENTS(section->size, section->cap, section->data, size), 0, size);
}

void object_symbol_relocate(label_id label, int64_t offset, int64_t add, enum relocation_type type) {
	struct section *section = &current_object.sections[current_section];
	struct object_relocation *relocation = &ADD_ELEMENT(section->relocation_size,
														section->relocation_cap,
														section->relocations);

	int idx = get_symbol(label);

	*relocation = (struct object_relocation) {
		.idx = idx,
		.offset = section->size + offset,
		.add = add,
		.type = type
	};
}

void object_symbol_set(label_id label, int global) {
	struct section *section = &current_object.sections[current_section];

	int idx = get_symbol(label);

	struct symbol *symbol = &current_object.symbols[idx];

	symbol->section = current_section;
	symbol->value = section->size;
	symbol->global = global;
}
