#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include <stdio.h>
#include "codegen/rodata.h"

struct object_relocation {
	enum relocation_type {
		RELOCATE_64,
		RELOCATE_32,
		RELOCATE_32_RELATIVE
	} type;

	int idx;
	uint64_t offset;
	int64_t add;
};

struct section {
	char *name;

	size_t size, cap;
	uint8_t *data;

	size_t relocation_size, relocation_cap;
	struct object_relocation *relocations;
};

struct symbol {
	char *name;

	uint64_t value;
	uint64_t size;

	int global;
	int section;
};

struct object {
	size_t section_size, section_cap;
	struct section *sections;

	size_t symbol_size, symbol_cap;
	struct symbol *symbols;
};

void object_start(void);
struct object *object_finish(void);

void object_set_section(const char *section_name);

void object_write(uint8_t *data, size_t size);
void object_write_byte(uint8_t byte);
void object_write_quad(uint64_t quad);
void object_write_zero(size_t size);

void object_symbol_relocate(label_id label, int64_t offset, int64_t add, enum relocation_type type);
void object_symbol_set(label_id label, int global);

#endif
