#include "coff.h"

#include "common.h"

#include <inttypes.h>

struct coff_file_header {
  uint16_t machine;
  uint16_t number_of_sections;
  uint32_t time_date_stamp;
  uint32_t pointer_to_symbol_table;
  uint32_t number_of_symbols;
  uint16_t size_of_optional_header;
  uint16_t characteristics;
};

enum {
	IMAGE_FILE_RELOCS_STRIPPED = 0x0001,
	IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002,
	IMAGE_FILE_LINE_NUMS_STRIPPED = 0x0004,
	IMAGE_FILE_LOCAL_SYMS_STRIPPED = 0x0008,
	IMAGE_FILE_AGGRESIVE_WS_TRIM = 0x0010,
	IMAGE_FILE_LARGE_ADDRESS_AWARE = 0x0020,
	IMAGE_FILE_BYTES_REVERSED_LO = 0x0080,
	IMAGE_FILE_32BIT_MACHINE = 0x0100,
	IMAGE_FILE_DEBUG_STRIPPED = 0x0200,
	IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP = 0x0400,
	IMAGE_FILE_NET_RUN_FROM_SWAP = 0x0800,
	IMAGE_FILE_SYSTEM = 0x1000,
	IMAGE_FILE_DLL = 0x2000,
	IMAGE_FILE_UP_SYSTEM_ONLY = 0x4000,
	IMAGE_FILE_BYTES_REVERSED_HI = 0x8000,
};

struct coff_section_header {
	uint8_t name[8];
	uint32_t virtual_size;
	uint32_t virtual_address;
	uint32_t size_of_raw_data;
	uint32_t pointer_to_raw_data;
	uint32_t pointer_to_relocations;
	uint32_t pointer_to_linenumbers;
	uint16_t number_of_relocations;
	uint16_t number_of_linenumbers;
	uint32_t characteristics;
};

enum {
	IMAGE_SCN_CNT_CODE = 0x00000020,
	IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040,
	IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080,
	IMAGE_SCN_LNK_INFO = 0x00000200,
	IMAGE_SCN_LNK_REMOVE = 0x00000800,
	IMAGE_SCN_LNK_COMDAT = 0x00001000,
	IMAGE_SCN_GPREL = 0x00008000,
	IMAGE_SCN_ALIGN_1BYTES = 0x00100000,
	IMAGE_SCN_ALIGN_2BYTES = 0x00200000,
	IMAGE_SCN_ALIGN_4BYTES = 0x00300000,
	IMAGE_SCN_ALIGN_8BYTES = 0x00400000,
	IMAGE_SCN_ALIGN_16BYTES = 0x00500000,
	IMAGE_SCN_ALIGN_32BYTES = 0x00600000,
	IMAGE_SCN_ALIGN_64BYTES = 0x00700000,
	IMAGE_SCN_ALIGN_128BYTES = 0x00800000,
	IMAGE_SCN_ALIGN_256BYTES = 0x00900000,
	IMAGE_SCN_ALIGN_512BYTES = 0x00A00000,
	IMAGE_SCN_ALIGN_1024BYTES = 0x00B00000,
	IMAGE_SCN_ALIGN_2048BYTES = 0x00C00000,
	IMAGE_SCN_ALIGN_4096BYTES = 0x00D00000,
	IMAGE_SCN_ALIGN_8192BYTES = 0x00E00000,
	IMAGE_SCN_LNK_NRELOC_OVFL = 0x01000000,
	IMAGE_SCN_MEM_DISCARDABLE = 0x02000000,
	IMAGE_SCN_MEM_NOT_CACHED = 0x04000000,
	IMAGE_SCN_MEM_NOT_PAGED = 0x08000000,
	IMAGE_SCN_MEM_SHARED = 0x10000000,
	IMAGE_SCN_MEM_EXECUTE = 0x20000000,
	IMAGE_SCN_MEM_READ = 0x40000000,
#define IMAGE_SCN_MEM_WRITE (0x80000000) // Too large to fit into enum.
};

struct coff_relocation {
	uint32_t virtual_address;
	uint32_t symbol_table_index;
	uint16_t type;
};

enum {
	IMAGE_REL_AMD64_ADDR64 = 0x0001,
	IMAGE_REL_AMD64_ADDR32 = 0x0002,
	IMAGE_REL_AMD64_REL32 = 0x0004
};

struct coff_symbol {
	uint8_t name[8];
	uint32_t value;
	uint16_t section_number;
	uint16_t type;
	uint8_t storage_class;
	uint8_t number_of_aux_symbols;
};

enum {
	IMAGE_SYM_CLASS_EXTERNAL = 2,
	IMAGE_SYM_CLASS_STATIC = 3,
	IMAGE_SYM_CLASS_LABEL = 6
};

static void coff_write_header(FILE *fp, struct coff_file_header *header) {
	file_write_word(fp, header->machine);
	file_write_word(fp, header->number_of_sections);
	file_write_long(fp, header->time_date_stamp);
	file_write_long(fp, header->pointer_to_symbol_table);
	file_write_long(fp, header->number_of_symbols);
	file_write_word(fp, header->size_of_optional_header);
	file_write_word(fp, header->characteristics);
}

static void coff_write_section_header(FILE *fp, struct coff_section_header *header) {
	file_write(fp, header->name, 8);
	file_write_long(fp, header->virtual_size);
	file_write_long(fp, header->virtual_address);
	file_write_long(fp, header->size_of_raw_data);
	file_write_long(fp, header->pointer_to_raw_data);
	file_write_long(fp, header->pointer_to_relocations);
	file_write_long(fp, header->pointer_to_linenumbers);
	file_write_word(fp, header->number_of_relocations);
	file_write_word(fp, header->number_of_linenumbers);
	file_write_long(fp, header->characteristics);
}

static void coff_write_symbol(FILE *fp, struct coff_symbol *symbol) {
	file_write(fp, symbol->name, 8);
	file_write_long(fp, symbol->value);
	file_write_word(fp, symbol->section_number);
	file_write_word(fp, symbol->type);
	file_write_byte(fp, symbol->storage_class);
	file_write_byte(fp, symbol->number_of_aux_symbols);
}

static void coff_write_relocation(FILE *fp, struct coff_relocation *relocation) {
	file_write_long(fp, relocation->virtual_address);
	file_write_long(fp, relocation->symbol_table_index);
	file_write_word(fp, relocation->type);
}

struct coff_section {
	struct coff_section_header header;

	uint8_t *data; // Size determined by header.
	struct coff_relocation *relocations; // -||-
};

struct coff_file {
	struct coff_file_header header;

	struct coff_section *sections; // Length determined by header.
	struct coff_symbol *symbols; // -||-

	size_t string_size, string_cap;
	char *strings;
};

static int coff_register_string(struct coff_file *coff, const char *str) {
	char *space = ADD_ELEMENTS(coff->string_size, coff->string_cap, coff->strings, strlen(str) + 1);

	strcpy(space, str);

	return space - coff->strings;
}

static struct coff_file *coff_from_object(struct object *object) {
	struct coff_file file = { 0 };

	coff_register_string(&file, "");

	file.header = (struct coff_file_header) {
		.machine = 0x8664,
		//.characteristics = IMAGE_FILE_LINE_NUMS_STRIPPED | IMAGE_FILE_LARGE_ADDRESS_AWARE, // | IMAGE_FILE_DEBUG_STRIPPED
		.characteristics = IMAGE_FILE_LINE_NUMS_STRIPPED,
		.number_of_sections = object->section_size,
		.number_of_symbols = object->symbol_size
	};

	file.sections = cc_malloc(sizeof *file.sections * file.header.number_of_sections);
	for (unsigned i = 0; i < file.header.number_of_sections; i++) {
		struct section *section = &object->sections[i];
		struct coff_section *coff_section = &file.sections[i];

		coff_section->data = section->data;

		uint32_t characteristics = 0;

		// For now, make everything executable, writable, and readable.
		characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE;

		coff_section->header = (struct coff_section_header) {
			.virtual_size = section->size,
			.virtual_address = 0,
			.size_of_raw_data = section->size,
			.pointer_to_raw_data = 0, // Will be set during allocation phase.
			.pointer_to_relocations = 0, // -||-
			.pointer_to_linenumbers = 0,
			.number_of_relocations = section->relocation_size,
			.number_of_linenumbers = 0,
			.characteristics = characteristics
		};

		if (section->name) {
			if (strlen(section->name) > 8) {
				int idx = coff_register_string(&file, section->name);
				// This is just stupid:
				sprintf((char *)coff_section->header.name, "/%d", idx);
			} else {
				strncpy((char *)coff_section->header.name, section->name, 8);
			}
		}

		coff_section->relocations = cc_malloc(sizeof *coff_section->relocations *
											  coff_section->header.number_of_relocations);
		for (unsigned j = 0; j < coff_section->header.number_of_relocations; j++) {
			struct object_relocation *relocation = &section->relocations[j];
			struct coff_relocation *coff_relocation = &coff_section->relocations[j];

			// Since COFF relocations contain no addend,
			// this will have to be written to the data of the section instead.

			if (relocation->add < INT32_MIN || relocation->add > INT32_MAX)
				NOTIMP();
			if (relocation->offset > INT32_MAX)
				NOTIMP();

			int32_t addend = relocation->add;
			uint32_t offset = relocation->offset;
			uint16_t type = 0;
			int size = 0;

			switch (relocation->type) {
			case RELOCATE_32: type = IMAGE_REL_AMD64_ADDR32; size = 4; break;
			case RELOCATE_64: type = IMAGE_REL_AMD64_ADDR64; size = 8; break;
			case RELOCATE_32_RELATIVE:
				type = IMAGE_REL_AMD64_REL32;
				addend += 4; // Different definition of relative.
				size = 4;
				break;
			default: NOTIMP();
			}

			if (addend) {
				switch (size) {
				case 4: write_32(coff_section->data + offset, addend); break;
				case 8: write_64(coff_section->data + offset, addend); break;
				default: NOTIMP();
				}
			}

			*coff_relocation = (struct coff_relocation) {
				.symbol_table_index = relocation->idx,
				.type = type,
				.virtual_address = offset
			};
		}
	}

	file.symbols = cc_malloc(sizeof *file.symbols * file.header.number_of_symbols);
	for (unsigned i = 0; i < file.header.number_of_symbols; i++) {
		struct symbol *symbol = &object->symbols[i];
		struct coff_symbol *coff_symbol = &file.symbols[i];

		uint8_t storage_class =
			symbol->global ? IMAGE_SYM_CLASS_EXTERNAL : IMAGE_SYM_CLASS_STATIC;
		uint16_t section_number = symbol->section == -1 ? 0 : symbol->section + 1; // Section table starts at 1.

		if (symbol->value > INT32_MAX) {
			printf("Warning: Value of symbol %s truncated from %" PRIu64 " to %" PRIu32 ".\n",
				   symbol->name ? "(null)" : symbol->name, symbol->value, (uint32_t)symbol->value);
		}

		*coff_symbol = (struct coff_symbol) {
			.value = symbol->value,
			.section_number = section_number,
			.type = 0, // ???
			.storage_class = storage_class,
			.number_of_aux_symbols = 0
		};

		if (symbol->name) {
			if (strlen(symbol->name) > 8) {
				int idx = coff_register_string(&file, symbol->name);
				write_32(coff_symbol->name + 4, idx + 4);
			} else {
				strncpy((char *)coff_symbol->name, symbol->name, 8);
			}
		}
	}

	struct coff_file *ret = cc_malloc(sizeof *ret);
	*ret = file;
	return ret;
}

static void coff_allocate(struct coff_file *coff) {
	uint32_t address = 0;
	address += 5 * 4; // Size of COFF file header.
	address += (8 + 8 * 4) * coff->header.number_of_sections; // Size of section header.

	coff->header.pointer_to_symbol_table = address;

	address += (8 + 4 + 2 + 2 + 1 + 1) * coff->header.number_of_symbols;
	address += 4 + coff->string_size; // String table allocation.

	for (unsigned i = 0; i < coff->header.number_of_sections; i++) {
		struct coff_section *section = &coff->sections[i];
		section->header.pointer_to_raw_data = address;
		address += section->header.size_of_raw_data;
		section->header.pointer_to_relocations = address;
		address += section->header.number_of_relocations * (4 + 4 + 2);
	}
}

static void coff_write(const char *path, struct coff_file *coff) {
	FILE *fp = fopen(path, "wb");
	coff_write_header(fp, &coff->header);

	for (unsigned i = 0; i < coff->header.number_of_sections; i++) {
		struct coff_section *section = &coff->sections[i];
		coff_write_section_header(fp, &section->header);
	}

	for (unsigned i = 0; i < coff->header.number_of_symbols; i++)
		coff_write_symbol(fp, &coff->symbols[i]);

	file_write_long(fp, coff->string_size + 4);
	file_write(fp, coff->strings, coff->string_size);

	for (unsigned i = 0; i < coff->header.number_of_sections; i++) {
		struct coff_section *section = &coff->sections[i];
		file_write(fp, section->data, section->header.size_of_raw_data);
		for (unsigned j = 0; j < section->header.number_of_relocations; j++) {
			coff_write_relocation(fp, &section->relocations[j]);
		}
	}

	fclose(fp);
}

void coff_write_object(const char *path, struct object *object) {
	struct coff_file *coff = coff_from_object(object);
	coff_allocate(coff);
	coff_write(path, coff);
}
