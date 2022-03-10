#include "linker.h"

#include "common.h"

#include <stdlib.h>

struct combined_relocation {
	enum relocation_type type;

	int object_idx, section_idx;
	uint64_t offset;

	int segment_idx;
	uint64_t segment_offset;

	int64_t add;

	int idx;
};

struct combined_symbol {
	char *name; // Can be NULL.

	uint64_t value, size;

	int object_idx, section_idx;
};

static void write_16(uint8_t *data, uint64_t value) {
	data[0] = value;
	data[1] = value >> 8;
}

static void write_32(uint8_t *data, uint64_t value) {
	write_16(data, value);
	write_16(data + 2, value >> 16);
}

static void write_64(uint8_t *data, uint64_t value) {
	write_32(data, value);
	write_32(data + 4, value >> 32);
}

struct executable *linker_link(int n_objects, struct object *objects) {
	struct executable executable = { 0 };

	if (n_objects != 1)
		NOTIMP();

	struct object *object = objects;

	// To begin with we put everything into one large segment.
	// This will be executable, writable, and readable.

	struct segment *segment = &ADD_ELEMENT(executable.segment_size, executable.segment_cap, executable.segments);
	*segment = (struct segment) {
		.load_address = 0x10000,
		.executable = 1,
		.readable = 1,
		.writable = 1
	};

	struct section_info {
		uint64_t offset;//, v_addr;
		int segment_idx;
	};

	struct section_info *section_infos = malloc(sizeof(*section_infos) * objects->section_size);

	// Move sections into segment.
	for (unsigned i = 0; i < object->section_size; i++) {
		struct section *section = &object->sections[i];
		uint8_t *dest = ADD_ELEMENTS(segment->size, segment->cap, segment->data, section->size);
		memcpy(dest, section->data, section->size);

		size_t offset = dest - segment->data;

		section_infos[i] = (struct section_info) {
			.offset = offset,
			//.v_addr = offset + segment->load_address,
			.segment_idx = 0
		};
	}

	for (unsigned i = 0; i < object->section_size; i++) {
		struct section *section = &object->sections[i];
		struct section_info *relocation_section_info = &section_infos[i];
		struct segment *relocation_segment = &executable.segments[relocation_section_info->segment_idx];

		for (unsigned j = 0; j < section->relocation_size; j++) {
			struct object_relocation *relocation = &section->relocations[j];
			struct symbol *symbol = &object->symbols[relocation->idx];

			if (symbol->section == -1)
				NOTIMP();

			struct section_info *symbol_section_info = &section_infos[symbol->section];
			struct segment *symbol_segment = &executable.segments[symbol_section_info->segment_idx];

			uint64_t symbol_value = symbol->value + symbol_section_info->offset + symbol_segment->load_address;
			uint64_t relocation_virtual_pos = relocation->offset + relocation_section_info->offset + relocation_segment->load_address;

			uint64_t segment_relative_pos = relocation->offset + relocation_section_info->offset;

			switch (relocation->type) {
			case RELOCATE_64:
				write_64(relocation_segment->data + segment_relative_pos, symbol_value + relocation->add);
				break;

			case RELOCATE_32:
				write_32(relocation_segment->data + segment_relative_pos, symbol_value + relocation->add);
				break;

			case RELOCATE_32_RELATIVE: {
				int64_t relative = symbol_value - relocation_virtual_pos;
				write_32(relocation_segment->data + segment_relative_pos, relative + relocation->add);
			} break;
			}
		}
	}

	for (unsigned i = 0; i < object->symbol_size; i++) {
		struct symbol *symbol = &object->symbols[i];

		struct section_info *symbol_section_info = &section_infos[symbol->section];
		struct segment *symbol_segment = &executable.segments[symbol_section_info->segment_idx];

		uint64_t symbol_value = symbol->value + symbol_section_info->offset + symbol_segment->load_address;

		if (symbol->name && strcmp(symbol->name, "_start") == 0) {
			executable.entry = symbol_value;
		}
	}

	segment->load_size = segment->size;

	struct executable *ret = malloc(sizeof *ret);
	*ret = executable;
	return ret;
}
