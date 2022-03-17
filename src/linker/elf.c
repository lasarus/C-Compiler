#include "elf.h"
#include "object.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

enum {
	R_X86_64_NONE = 0,
	R_X86_64_64 = 1,
	R_X86_64_PC32 = 2,
	R_X86_64_GOT32 = 3,
	R_X86_64_PLT32 = 4,
	R_X86_64_COPY = 5,
	R_X86_64_GLOB_DAT = 6,
	R_X86_64_JUMP_SLOT = 7,
	R_X86_64_RELATIVE = 8,
	R_X86_64_GOTPCREL = 9,
	R_X86_64_32 = 10,
	R_X86_64_32S = 11,
	R_X86_64_16 = 12,
	R_X86_64_PC16 = 13,
	R_X86_64_8 = 14,
};

enum {
	SHT_NULL = 0,
	SHT_PROGBITS = 1,
	SHT_SYMTAB = 2,
	SHT_STRTAB = 3,
	SHT_RELA = 4,
	SHT_NOBITS = 8,
};

enum {
	STB_LOCAL = 0,
	STB_GLOBAL = 1,
};

enum {
	SHF_WRITE = 1 << 0,
	SHF_ALLOC = 1 << 1,
	SHF_EXECINSTR = 1 << 2,
	SHF_MERGE = 1 << 4,
	SHF_STRINGS = 1 << 5,
	SHF_INFO_LINK = 1 << 6,
	SHF_LINK_ORDER = 1 << 7
};

enum {
	STT_NOTYPE = 0,
	STT_SECTION = 3
};

enum {
	PT_NULL = 0,
	PT_LOAD = 1,
};

enum {
	PF_X = 1 << 0,
	PF_W = 1 << 1,
	PF_R = 1 << 2
};

enum {
	ET_REL = 1,
	ET_EXEC = 2
};

struct elf_section_header {
	uint32_t sh_name, sh_type;
	uint64_t sh_flags, sh_addr, sh_offset, sh_size;
	uint32_t sh_link, sh_info;
	uint64_t sh_addralign, sh_entsize;
};

struct elf_header {
	uint16_t e_type;
	uint64_t e_entry;
	uint64_t e_shoff;
	uint64_t e_phoff;

	uint16_t e_shstrndx;
};

struct elf_section {
	struct elf_section_header header;
	size_t size;
	uint8_t *data;
};

struct elf_program_header {
	uint32_t p_type, p_flags;
	uint64_t p_offset, p_vaddr, p_paddr;
	uint64_t p_filesz, p_memsz, p_align;
};

struct elf_segment {
	struct elf_program_header header;
	size_t size;
	uint8_t *data;
};

struct rela {
	int symb_idx;
	uint64_t offset;
	uint64_t type;
	uint64_t add;
};

struct elf_file {
	struct elf_header header;

	size_t shstring_size, shstring_cap;
	char *shstrings;

	size_t string_size, string_cap;
	char *strings;

	size_t section_size, section_cap;
	struct elf_section *sections;

	size_t segment_size, segment_cap;
	struct elf_segment *segments;
};

static int elf_register_shstring(struct elf_file *elf, const char *str) {
	char *space = ADD_ELEMENTS(elf->shstring_size, elf->shstring_cap, elf->shstrings, strlen(str) + 1);

	strcpy(space, str);

	return space - elf->shstrings;
}

static int elf_register_string(struct elf_file *elf, const char *str) {
	char *space = ADD_ELEMENTS(elf->string_size, elf->string_cap, elf->strings, strlen(str) + 1);

	strcpy(space, str);

	return space - elf->strings;
}

static int elf_add_section(struct elf_file *elf, uint32_t name, uint32_t type) {
	struct elf_section *section =
		&ADD_ELEMENT(elf->section_size, elf->section_cap, elf->sections);

	section->header = (struct elf_section_header) { .sh_name = name, .sh_type = type };
	section->size = 0;
	section->data = NULL;

	return section - elf->sections;
}

static int elf_add_segment(struct elf_file *elf, uint32_t type) {
	struct elf_segment *segment =
		&ADD_ELEMENT(elf->segment_size, elf->segment_cap, elf->segments);

	segment->header = (struct elf_program_header) { .p_type = type };
	segment->size = 0;
	segment->data = NULL;

	return segment - elf->segments;
}

static void elf_allocate_sections(struct elf_file *elf) {
	uint64_t address = 0;

	address += 64; // e_hsize.

	if (elf->section_size) {
		elf->header.e_shoff = address;
		address += 64 * elf->section_size;
		for (unsigned i = 1; i < elf->section_size; i++) {
			struct elf_section *section = &elf->sections[i];

			section->header.sh_offset = address;
			address += section->size;
		}
	}

	if (elf->segment_size) {
		elf->header.e_phoff = address;
		address += (7 * 8) * elf->segment_size;
		for (unsigned i = 0; i < elf->segment_size; i++) {
			struct elf_segment *segment = &elf->segments[i];

			address = round_up_to_nearest(address, segment->header.p_align);

			segment->header.p_offset = address;
			address += segment->size;
		}
	}
}

static struct object *object_from_elf(struct elf_file *elf) {
	struct object object = { 0 };

	int *section_mapping = malloc(sizeof *section_mapping * elf->section_size);
	for (unsigned i = 0; i < elf->section_size; i++)
		section_mapping[i] = -1;

	// Add progbits sections.
	for (unsigned i = 0; i < elf->section_size; i++) {
		struct elf_section *elf_section = &elf->sections[i];
		if (elf_section->header.sh_type != SHT_PROGBITS)
			continue;

		struct section *section =
			&ADD_ELEMENT(object.section_size, object.section_cap, object.sections);

		section_mapping[i] = section - object.sections;

		*section = (struct section) {
			.size = elf_section->size,
			.cap = elf_section->size,
			.data = elf_section->data,
		};

		if (elf_section->header.sh_name)
			section->name = strdup(elf->shstrings + elf_section->header.sh_name);
	}

	int *symbol_mapping = NULL;

	for (unsigned i = 0; i < elf->section_size; i++) {
		struct elf_section *symtab_section = &elf->sections[i];
		if (symtab_section->header.sh_type != SHT_SYMTAB)
			continue;

		size_t ent_size = symtab_section->header.sh_entsize;
		size_t ent_count = symtab_section->header.sh_size / ent_size;

		symbol_mapping = malloc(sizeof *symbol_mapping * ent_count);

		for (unsigned j = 1; j < ent_count; j++) {
			uint8_t *symbol_data = symtab_section->data + j * ent_size;

			uint32_t st_name = read_32(symbol_data + 0);
			uint8_t st_info = read_8(symbol_data + 4);
			/*uint8_t st_other =*/ read_8(symbol_data + 5);
			uint16_t st_shndx = read_16(symbol_data + 6);
			uint64_t st_value = read_64(symbol_data + 8);
			uint64_t st_size = read_64(symbol_data + 16);

			if ((st_info & 0xf) != STT_NOTYPE)
				continue;

			struct symbol *symbol = &ADD_ELEMENT(object.symbol_size, object.symbol_cap, object.symbols);

			*symbol = (struct symbol) {
				.global = st_info & (STB_GLOBAL << 4),
				.name = st_name ? strdup(elf->strings + st_name) : NULL,
				.section = section_mapping[st_shndx],
				.size = st_size,
				.value = st_value,
			};

			symbol_mapping[j] = symbol - object.symbols;
		}
	}

	for (unsigned i = 0; i < elf->section_size; i++) {
		struct elf_section *elf_section = &elf->sections[i];
		if (elf_section->header.sh_type != SHT_RELA)
			continue;

		struct section *target_section = &object.sections[section_mapping[elf_section->header.sh_info]];

		size_t ent_size = elf_section->header.sh_entsize;
		size_t ent_count = elf_section->header.sh_size / ent_size;

		for (unsigned j = 0; j < ent_count; j++) {
			uint8_t *rela_data = elf_section->data + ent_size * j;

			uint64_t r_offset = read_64(rela_data);
			uint64_t r_info = read_64(rela_data + 8);
			uint64_t r_addend = read_64(rela_data + 16);

			int type = 0;
			switch (r_info & 0xffff) {
			case R_X86_64_64: type = RELOCATE_64; break;
			case R_X86_64_32S: type = RELOCATE_32; break;
			case R_X86_64_PC32: type = RELOCATE_32_RELATIVE; break;
			default:
				ICE("Relocation %lu not implemented.", r_info & 0xffff);
			}

			struct object_relocation *relocation =
				&ADD_ELEMENT(target_section->relocation_size, target_section->relocation_cap, target_section->relocations);

			*relocation = (struct object_relocation) {
				.add = r_addend,
				.offset = r_offset,
				.idx = symbol_mapping[(r_info >> 32) & 0xffff],
				.type = type,
			};
		}
	}

	free(section_mapping);
	free(symbol_mapping);

	struct object *ret = malloc(sizeof *ret);
	*ret = object;
	return ret;
}

static struct elf_file *elf_from_object(struct object *object) {
	struct elf_file elf = { 0 };

	elf.header.e_type = ET_REL;
	elf.header.e_entry = 0; // No entry for relocatable object.

	// Write null section.
	elf_register_string(&elf, "");
	elf_add_section(&elf, elf_register_shstring(&elf, ""), SHT_NULL);

	// Write sections found in object.
	for (unsigned i = 0; i < object->section_size; i++) {
		struct section *section = &object->sections[i];

		int section_idx = elf_add_section(&elf, elf_register_shstring(&elf, section->name), SHT_PROGBITS);
		struct elf_section *elf_section = &elf.sections[section_idx];

		elf_section->size = section->size;
		if (elf_section->size) {
			elf_section->data = malloc(elf_section->size);
			memcpy(elf_section->data, section->data, elf_section->size);
		}

		elf_section->header.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	}

	// Write symbol section.
	int *symbol_mapping = malloc(sizeof *symbol_mapping * object->symbol_size);

	int sym_idx = elf_add_section(&elf, elf_register_shstring(&elf, ".symtab"), SHT_SYMTAB);
	struct elf_section *sym_section = &elf.sections[sym_idx];
	sym_section->header.sh_entsize = 24;
	sym_section->size = (object->section_size + object->symbol_size + 1) * sym_section->header.sh_entsize;
	sym_section->data = malloc(sym_section->size);
	int n_local_symb = 1;

	for (unsigned i = 0; i < object->symbol_size; i++) {
		struct symbol *symbol = &object->symbols[i];

		if (!symbol->global)
			n_local_symb++;
	}

	memset(sym_section->data, 0, sym_section->header.sh_entsize); // Set first symbol to NULL.

	// Each section has a corresponding symbol.
	for (unsigned i = 0; i < object->section_size; i++) {
		struct section *section = &object->sections[i];

		uint8_t *symbol_data =
			sym_section->data + sym_section->header.sh_entsize * (i + 1);

		write_32(symbol_data + 0, 0);
		write_8(symbol_data + 4, STT_SECTION);
		write_8(symbol_data + 5, 0);
		write_16(symbol_data + 6, i + 1);
		write_64(symbol_data + 8, 0);
		write_64(symbol_data + 16, section->size);

		n_local_symb++;
	}

	unsigned current_local_symbol = current_local_symbol = 1 + object->section_size,
		current_global_symbol = n_local_symb;

	sym_section->header.sh_info = n_local_symb;

	for (unsigned i = 0; i < object->symbol_size; i++) {
		struct symbol *symbol = &object->symbols[i];

		uint8_t *symbol_data;
		if (symbol->global) {
			symbol_mapping[i] = current_global_symbol;
			symbol_data = sym_section->data + sym_section->header.sh_entsize * current_global_symbol;
			current_global_symbol++;
		} else {
			symbol_mapping[i] = current_local_symbol;
			symbol_data = sym_section->data + sym_section->header.sh_entsize * current_local_symbol;
			current_local_symbol++;
		}

		uint32_t st_name = elf_register_string(&elf, symbol->name);
		uint8_t st_info = 0; // ???

		if (symbol->global)
			st_info |= STB_GLOBAL << 4;

		uint8_t st_other = 0;
		uint16_t st_shndx = 0;
		uint64_t st_value = symbol->value;
		uint64_t st_size = symbol->size;

		if (symbol->section != -1)
			st_shndx = 1 + symbol->section;

		write_32(symbol_data + 0, st_name);
		write_8(symbol_data + 4, st_info);
		write_8(symbol_data + 5, st_other);
		write_16(symbol_data + 6, st_shndx);
		write_64(symbol_data + 8, st_value);
		write_64(symbol_data + 16, st_size);
	}

	// Write relocation sections.
	for (unsigned i = 0; i < object->section_size; i++) {
		struct section *section = &object->sections[i];

		char name_buffer[256];
		int len = snprintf(name_buffer, sizeof name_buffer, ".rela%s", section->name);
		if (len < 0 || len >= (int)sizeof name_buffer)
			ICE("Could not write \"%s\" to buffer\n", section->name);

		int rela_idx = elf_add_section(&elf, elf_register_shstring(&elf, name_buffer), SHT_RELA);
		struct elf_section *elf_section = &elf.sections[rela_idx];

		elf_section->header.sh_entsize = 24;
		elf_section->size = elf_section->header.sh_entsize * section->relocation_size;
		elf_section->data = malloc(elf_section->size);
		elf_section->header.sh_link = sym_idx;
		elf_section->header.sh_info = i + 1;
		elf_section->header.sh_flags = SHF_INFO_LINK;

		for (unsigned j = 0; j < section->relocation_size; j++) {
			struct object_relocation *relocation = &section->relocations[j];

			uint8_t *rela_data = elf_section->data + elf_section->header.sh_entsize * j;

			int sym_idx = symbol_mapping[relocation->idx];
			int type = 0;

			switch (relocation->type) {
			case RELOCATE_64: type = R_X86_64_64; break;
			case RELOCATE_32: type = R_X86_64_32S; break;
			case RELOCATE_32_RELATIVE: type = R_X86_64_PC32; break;
			}

			write_64(rela_data, relocation->offset);
			write_64(rela_data + 8, ((uint64_t)sym_idx << 32) + type); // r_info
			write_64(rela_data + 16, relocation->add); // r_info
		}
	}

	// Write strtab and shstrtab sections.
	int strtab_section = elf_add_section(&elf, elf_register_shstring(&elf, ".strtab"), SHT_STRTAB);
	int shstrtab_section = elf_add_section(&elf, elf_register_shstring(&elf, ".shstrtab"), SHT_STRTAB);

	elf.sections[sym_idx].header.sh_link = strtab_section;

	elf.sections[shstrtab_section].size = elf.shstring_size;
	elf.sections[shstrtab_section].data = (uint8_t *)elf.shstrings;

	elf.sections[strtab_section].size = elf.string_size;
	elf.sections[strtab_section].data = (uint8_t *)elf.strings;

	elf.header.e_shstrndx = shstrtab_section;

	free(symbol_mapping);

	elf_allocate_sections(&elf);

	struct elf_file *ret = malloc(sizeof *ret);
	*ret = elf;
	return ret;
}

static struct elf_file *elf_from_executable(struct executable *executable) {
	struct elf_file elf = { 0 };

	elf.header.e_type = ET_EXEC;
	elf.header.e_entry = executable->entry; // No entry for relocatable object.

	for (unsigned i = 0; i < executable->segment_size; i++) {
		struct segment *segment = &executable->segments[i];

		int segment_idx = elf_add_segment(&elf, PT_LOAD);
		struct elf_segment *elf_segment = &elf.segments[segment_idx];

		elf_segment->data = segment->data;
		elf_segment->size = segment->size;

		elf_segment->header.p_align = 0x1000;

		if (segment->executable)
			elf_segment->header.p_flags |= PF_X;
		if (segment->readable)
			elf_segment->header.p_flags |= PF_R;
		if (segment->writable)
			elf_segment->header.p_flags |= PF_W;

		elf_segment->header.p_vaddr = segment->load_address;
		elf_segment->header.p_paddr = segment->load_address;
		elf_segment->header.p_filesz = segment->size;
		elf_segment->header.p_memsz = segment->load_size;
	}

	elf_allocate_sections(&elf);

	struct elf_file *ret = malloc(sizeof *ret);
	*ret = elf;
	return ret;
}

static void elf_free(struct elf_file *elf) {
	free(elf->shstrings);
	free(elf->strings);
	free(elf->sections);
}

static void read(FILE *fp, void *ptr, size_t size) {
	if (fread(ptr, size, 1, fp) != 1)
		ICE("Could not read from file");
}

static uint8_t read_byte(FILE *fp) {
	uint8_t byte;
	read(fp, &byte, 1);
	return byte;
}

static uint16_t read_word(FILE *fp) {
	return read_byte(fp) | ((uint16_t)read_byte(fp) << 8);
}

static uint32_t read_long(FILE *fp) {
	return read_word(fp) | ((uint32_t)read_word(fp) << 16);
}

static uint64_t read_quad(FILE *fp) {
	return read_long(fp) | ((uint64_t)read_long(fp) << 32);
}

static void read_zero(FILE *fp, size_t size) {
	for (size_t i = 0; i < size; i++)
		read_byte(fp); // TODO: Make this faster.
}

static void read_skip(FILE *fp, unsigned long pos) {
	fseek(fp, pos, SEEK_SET);
}

static const uint8_t magic[4] = {0x7f, 0x45, 0x4c, 0x46};

static void read_header(FILE *fp, struct elf_file *elf) {
	static uint8_t magic_buffer[4];
	read(fp, magic_buffer, sizeof magic_buffer);

	for (unsigned i = 0; i < sizeof magic_buffer; i++)
		if (magic_buffer[i] != magic[i])
			ICE("Not an elf-file.");

	assert(read_byte(fp) == 2); // EI_CLASS = 64 bit
	assert(read_byte(fp) == 1); // EI_DATA = little endian
	assert(read_byte(fp) == 1); // EI_VARSION = 1
	assert(read_byte(fp) == 0); // EI_OSABI = System V
	assert(read_byte(fp) == 0); // EI_ABIVERSION = 0

	read_zero(fp, 7);

	elf->header.e_type = read_word(fp);
	assert(read_word(fp) == 0x3e); // e_machine = AMD x86-64

	assert(read_long(fp) == 1); // e_version = 1

	elf->header.e_entry = read_quad(fp); // e_entry
	elf->header.e_phoff = read_quad(fp); // e_phoff
	elf->header.e_shoff = read_quad(fp); // e_shoff

	assert(read_long(fp) == 0); // e_flags = 0
	assert(read_word(fp) == 64); // e_ehsize = 64

	uint16_t e_phentsize = read_word(fp); // e_phentsize
	elf->segment_size = read_word(fp); // e_phnum

	assert(elf->segment_size == 0 || e_phentsize == 7 * 8);

	uint16_t e_shentsize = read_word(fp); // e_shentsize
	elf->section_size = read_word(fp); // e_shnum

	assert(elf->segment_size == 0 || e_phentsize == 7 * 8);
	assert(elf->section_size == 0 || e_shentsize == 8 * 8);

	elf->header.e_shstrndx = read_word(fp); // e_shstrndx
}

static void write_header(FILE *fp, struct elf_file *elf) {
	file_write(fp, magic, sizeof magic);

	file_write_byte(fp, 2); // EI_CLASS = 64 bit
	file_write_byte(fp, 1); // EI_DATA = little endian
	file_write_byte(fp, 1); // EI_VARSION = 1
	file_write_byte(fp, 0); // EI_OSABI = System V
	file_write_byte(fp, 0); // EI_ABIVERSION = 0

	file_write_zero(fp, 7); // Padding

	file_write_word(fp, elf->header.e_type); // e_type
	file_write_word(fp, 0x3e); // e_machine = AMD x86-64

	file_write_long(fp, 1); // e_version = 1

	file_write_quad(fp, elf->header.e_entry); // e_entry
	file_write_quad(fp, elf->header.e_phoff); // e_phoff
	file_write_quad(fp, elf->header.e_shoff); // e_shoff

	file_write_long(fp, 0); // e_flags = 0
	file_write_word(fp, 64); // e_ehsize = 64

	file_write_word(fp, elf->segment_size ? 7 * 8 : 0); // e_phentsize
	file_write_word(fp, elf->segment_size); // e_phnum
	file_write_word(fp, elf->section_size ? 64 : 0); // e_shentsize
	file_write_word(fp, elf->section_size); // e_shnum
	file_write_word(fp, elf->header.e_shstrndx); // e_shstrndx
}

static void read_section_header(FILE *fp, struct elf_section_header *header, size_t *size) {
	header->sh_name = read_long(fp);
	header->sh_type = read_long(fp);

	header->sh_flags = read_quad(fp);
	header->sh_addr = read_quad(fp);
	header->sh_offset = read_quad(fp);
	header->sh_size = *size = read_quad(fp);

	header->sh_link = read_long(fp);
	header->sh_info = read_long(fp);

	header->sh_addralign = read_quad(fp);
	header->sh_entsize = read_quad(fp);
}

static void write_section_header(FILE *fp, struct elf_section_header *header, size_t size) {
	file_write_long(fp, header->sh_name);
	file_write_long(fp, header->sh_type);

	file_write_quad(fp, header->sh_flags);
	file_write_quad(fp, header->sh_addr);
	file_write_quad(fp, header->sh_offset);
	file_write_quad(fp, size);

	file_write_long(fp, header->sh_link);
	file_write_long(fp, header->sh_info);

	file_write_quad(fp, header->sh_addralign);
	file_write_quad(fp, header->sh_entsize);
}

static void read_sections(FILE *fp, struct elf_file *elf) {
	if (!elf->section_size)
		return;

	read_skip(fp, elf->header.e_shoff);

	elf->section_cap = elf->section_size;
	elf->sections = malloc(sizeof *elf->sections * elf->section_cap);
	for (unsigned i = 0; i < elf->section_size; i++) {
		struct elf_section *section = &elf->sections[i];

		read_section_header(fp, &section->header, &section->size);
	}

	for (unsigned i = 0; i < elf->section_size; i++) {
		struct elf_section *section = &elf->sections[i];

		if (section->size == 0)
			continue;

		read_skip(fp, section->header.sh_offset);

		section->data = malloc(section->size);
		read(fp, section->data, section->size);
	}
}

static void write_sections(FILE *fp, struct elf_file *elf) {
	if (!elf->section_size)
		return;

	file_write_skip(fp, elf->header.e_shoff);

	for (unsigned i = 0; i < elf->section_size; i++) {
		struct elf_section *section = &elf->sections[i];

		write_section_header(fp, &section->header, section->size);
	}

	for (unsigned i = 0; i < elf->section_size; i++) {
		struct elf_section *section = elf->sections + i;

		if (section->size == 0)
			continue;

		file_write_skip(fp, section->header.sh_offset);
		file_write(fp, section->data, section->size);
	}
}

static void write_program_header(FILE *fp, struct elf_program_header *header, size_t size) {
	file_write_long(fp, header->p_type);
	file_write_long(fp, header->p_flags);

	file_write_quad(fp, header->p_offset);
	file_write_quad(fp, header->p_vaddr);
	file_write_quad(fp, header->p_paddr);

	file_write_quad(fp, size);
	file_write_quad(fp, header->p_memsz);
	file_write_quad(fp, header->p_align);
}

static void write_segments(FILE *fp, struct elf_file *elf) {
	if (!elf->segment_size)
		return;

	file_write_skip(fp, elf->header.e_phoff);

	for (unsigned i = 0; i < elf->segment_size; i++) {
		struct elf_segment *segment = &elf->segments[i];

		write_program_header(fp, &segment->header, segment->size);
	}

	for (unsigned i = 0; i < elf->segment_size; i++) {
		struct elf_segment *segment = &elf->segments[i];

		if (segment->size == 0)
			continue;

		file_write_skip(fp, segment->header.p_offset);
		file_write(fp, segment->data, segment->size);
	}
}

static void elf_write(const char *path, struct elf_file *elf) {
	FILE *fp = fopen(path, "wb");

	write_header(fp, elf);
	write_sections(fp, elf);
	write_segments(fp, elf);

	fclose(fp);
}

struct elf_file *elf_read(const char *path) {
	FILE *fp = fopen(path, "rb");
	struct elf_file elf = { 0 };

	read_header(fp, &elf);

	if (elf.segment_size)
		NOTIMP();

	read_sections(fp, &elf);

	// Add strtab and shstrtab if they exist.
	if (elf.header.e_shstrndx) {
		struct elf_section *shstrtab_section = &elf.sections[elf.header.e_shstrndx];
		assert(shstrtab_section->header.sh_type == SHT_STRTAB);

		elf.shstring_cap = elf.shstring_size = shstrtab_section->size;
		elf.shstrings = (char *)shstrtab_section->data;
	}

	for (unsigned i = 0; i < elf.section_size; i++) {
		struct elf_section *symtab_section = &elf.sections[i];
		if (symtab_section->header.sh_type != SHT_SYMTAB)
			continue;

		if (elf.strings)
			ICE("Multiple .strtab not supported");

		struct elf_section *strtab_section = &elf.sections[symtab_section->header.sh_link];
		assert(strtab_section->header.sh_type == SHT_STRTAB);

		elf.string_cap = elf.string_size = strtab_section->size;
		elf.strings = (char *)strtab_section->data;
	}

	fclose(fp);

	struct elf_file *ret = malloc(sizeof *ret);
	*ret = elf;
	return ret;
}

void elf_write_object(const char *path, struct object *object) {
	struct elf_file *elf = elf_from_object(object);
	elf_write(path, elf);
	elf_free(elf);
	free(elf);
}

void elf_write_executable(const char *path, struct executable *executable) {
	struct elf_file *elf = elf_from_executable(executable);
	elf_write(path, elf);
	elf_free(elf);
	free(elf);
}

struct object *elf_read_object(const char *path) {
	return object_from_elf(elf_read(path));
}
