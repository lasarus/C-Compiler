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

// This also modifies the values inside each object file.
static struct object *combine_objects(int n_objects, struct object *objects) {
	struct object object = { 0 };

	for (int i = 0; i < n_objects; i++) {
		object.section_size += objects[i].section_size;
		object.symbol_size += objects[i].symbol_size;
	}

	object.sections = cc_malloc(sizeof *object.sections * object.section_size);
	object.symbols = cc_malloc(sizeof *object.symbols * object.symbol_size);

	size_t current_symbol_offset = 0, current_section_offset = 0;
	for (int i = 0; i < n_objects; i++) {
		for (unsigned j = 0; j < objects[i].section_size; j++) {
			struct section section = objects[i].sections[j];

			for (unsigned k = 0; k < section.relocation_size; k++) {
				struct object_relocation *rel = &section.relocations[k];

				rel->idx += current_symbol_offset;
			}

			object.sections[current_section_offset + j] = section;
		}

		for (unsigned j = 0; j < objects[i].symbol_size; j++) {
			struct symbol symbol = objects[i].symbols[j];

			if (symbol.section != -1)
				symbol.section += current_section_offset;

			object.symbols[current_symbol_offset + j] = symbol;
		}

		current_symbol_offset += objects[i].symbol_size;
		current_section_offset += objects[i].section_size;
	}

	// Link symbols.
	for (unsigned i = 0; i < object.symbol_size; i++) {
		struct symbol *symbol = &object.symbols[i];

		if (symbol->section == -1) {
			if (!symbol->name) {
				ICE("Invalid local relocation.\n");
			}
			int found_section = -1;
			for (unsigned j = 0; j < object.symbol_size; j++) {
				if (object.symbols[j].name && object.symbols[j].global && object.symbols[j].section != -1) {
					if (strcmp(object.symbols[j].name, symbol->name) == 0) {
						if (object.symbols[j].section != -1)
							found_section = object.symbols[j].section;
					}
				}
			}

			symbol->section = found_section;
		}
	}

	struct object *ret = cc_malloc(sizeof *ret);
	*ret = object;

	return ret;
}

struct executable *linker_link(int n_objects, struct object *_objects) {
	struct executable executable = { 0 };

	struct object *object = n_objects == 1 ? _objects : combine_objects(n_objects, _objects);

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
		uint64_t offset;
		int segment_idx;
	};

	struct section_info *section_infos = cc_malloc(sizeof(*section_infos) * object->section_size);

	// Move sections into segment.
	for (unsigned i = 0; i < object->section_size; i++) {
		struct section *section = &object->sections[i];
		uint8_t *dest = segment->data;
		if (section->size) {
			dest = ADD_ELEMENTS(segment->size, segment->cap, segment->data, section->size);
			memcpy(dest, section->data, section->size);
		}

		size_t offset = dest - segment->data;

		section_infos[i] = (struct section_info) {
			.offset = offset,
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
				ICE("Linking error: \"%s\" is undefined.", symbol->name);

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

	struct executable *ret = cc_malloc(sizeof *ret);
	*ret = executable;
	return ret;
}
