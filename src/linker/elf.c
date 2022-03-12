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

static void elf_allocate_sections(struct elf_file *elf) {
	uint64_t address = 128;
	elf->header.e_shoff = address;
	address += 64 * elf->section_size;
	for (unsigned i = 1; i < elf->section_size; i++) {
		struct elf_section *section = elf->sections + i;

		section->header.sh_offset = address;
		address += section->size;
	}
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

static void elf_free(struct elf_file *elf) {
	free(elf->shstrings);
	free(elf->strings);
	free(elf->sections);
}

static void write(FILE *fp, const void *ptr, size_t size) {
	if (fwrite(ptr, size, 1, fp) != 1)
		ICE("Could not write to file");
}

static void write_byte(FILE *fp, uint8_t byte) {
	write(fp, &byte, 1);
}

static void write_word(FILE *fp, uint16_t word) {
	write_byte(fp, word);
	write_byte(fp, word >> 8);
}

static void write_long(FILE *fp, uint32_t long_) {
	write_word(fp, long_);
	write_word(fp, long_ >> 16);
}

static void write_quad(FILE *fp, uint64_t quad) {
	write_long(fp, quad);
	write_long(fp, quad >> 32);
}

static void write_zero(FILE *fp, size_t size) {
	for (size_t i = 0; i < size; i++)
		write_byte(fp, 0); // TODO: Make this faster.
}

static void write_skip(FILE *fp, size_t target) {
	long int current_pos = ftell(fp);
	assert(target >= (size_t)current_pos);
	write_zero(fp, target - current_pos);
}

static void write_header(FILE *fp, struct elf_file *elf) {
	static uint8_t magic[4] = {0x7f, 0x45, 0x4c, 0x46};
	write(fp, magic, sizeof magic);

	write_byte(fp, 2); // EI_CLASS = 64 bit
	write_byte(fp, 1); // EI_DATA = little endian
	write_byte(fp, 1); // EI_VARSION = 1
	write_byte(fp, 0); // EI_OSABI = System V
	write_byte(fp, 0); // EI_ABIVERSION = 0

	write_zero(fp, 7); // Padding

	write_word(fp, elf->header.e_type); // e_type = ET_REL
	write_word(fp, 0x3e); // e_machine = AMD x86-64

	write_long(fp, 1); // e_version = 1

	write_quad(fp, elf->header.e_entry); // e_entry = ??
	write_quad(fp, 0); // e_phoff = ??
	write_quad(fp, elf->header.e_shoff); // e_shoff = ??

	write_long(fp, 0); // e_flags = 0
	write_word(fp, 64); // e_ehsize = 64

	write_word(fp, 0); // e_phentsize = 0
	write_word(fp, 0); // e_phnum = 0
	write_word(fp, 64); // e_shentsize = 0
	write_word(fp, elf->section_size); // e_shnum = 0
	write_word(fp, elf->header.e_shstrndx); // e_shstrndx = 0
}

static void write_section_header(FILE *fp, struct elf_section_header *header, size_t size) {
	write_long(fp, header->sh_name);
	write_long(fp, header->sh_type);

	write_quad(fp, header->sh_flags);
	write_quad(fp, header->sh_addr);
	write_quad(fp, header->sh_offset);
	write_quad(fp, size);

	write_long(fp, header->sh_link);
	write_long(fp, header->sh_info);

	write_quad(fp, header->sh_addralign);
	write_quad(fp, header->sh_entsize);
}

static void write_sections(FILE *fp, struct elf_file *elf) {
	write_skip(fp, elf->header.e_shoff);

	for (unsigned i = 0; i < elf->section_size; i++) {
		struct elf_section *section = elf->sections + i;

		write_section_header(fp, &section->header, section->size);
	}

	for (unsigned i = 0; i < elf->section_size; i++) {
		struct elf_section *section = elf->sections + i;

		if (section->size == 0)
			continue;

		write_skip(fp, section->header.sh_offset);
		write(fp, section->data, section->size);
	}
}

void elf_write(const char *path, struct elf_file *elf) {
	FILE *fp = fopen(path, "wb");

	write_header(fp, elf);
	write_sections(fp, elf);

	fclose(fp);
}

void elf_write_object(const char *path, struct object *object) {
	struct elf_file *elf = elf_from_object(object);
	elf_write(path, elf);
	elf_free(elf);
	free(elf);
}

#define PH_OFF 64

static void write_header_exec(FILE *fp, int segment_n, size_t entry) {
	static uint8_t magic[4] = {0x7f, 0x45, 0x4c, 0x46};
	write(fp, magic, sizeof magic);

	write_byte(fp, 2); // EI_CLASS = 64 bit
	write_byte(fp, 1); // EI_DATA = little endian
	write_byte(fp, 1); // EI_VERSION = 1
	write_byte(fp, 0); // EI_OSABI = System V
	write_byte(fp, 0); // EI_ABIVERSION = 0

	write_zero(fp, 7); // Padding

	write_word(fp, 2); // e_type = ET_EXEC
	write_word(fp, 0x3e); // e_machine = AMD x86-64

	write_long(fp, 1); // e_version = 1

	write_quad(fp, entry); // e_entry = ??
	write_quad(fp, PH_OFF); // e_phoff = ??
	write_quad(fp, 0); // e_shoff = ??

	write_long(fp, 0); // e_flags = 0
	write_word(fp, 64); // e_ehsize = 64

	write_word(fp, 7 * 8); // e_phentsize = 0
	write_word(fp, segment_n); // e_phnum = 0
	write_word(fp, 0); // e_shentsize = 0
	write_word(fp, 0); // e_shnum = 0
	write_word(fp, 0); // e_shstrndx = SHN_UNDEF
}

static void write_program_header(FILE *fp, struct elf_program_header *header) {
	write_long(fp, header->p_type);
	write_long(fp, header->p_flags);

	write_quad(fp, header->p_offset);
	write_quad(fp, header->p_vaddr);
	write_quad(fp, header->p_paddr);
	write_quad(fp, header->p_filesz);
	write_quad(fp, header->p_memsz);
	write_quad(fp, header->p_align);
}

void elf_write_executable(const char *path, struct executable *executable) {
	FILE *fp = fopen(path, "wb");

	write_header_exec(fp, executable->segment_size, executable->entry);

	size_t *segment_offsets = malloc(sizeof *segment_offsets * executable->segment_size);

	write_skip(fp, PH_OFF);
	unsigned long current_file_offset = PH_OFF + 7 * 8 * executable->segment_size;
	for (unsigned i = 0; i < executable->segment_size; i++) {
		struct segment *segment = &executable->segments[i];

		struct elf_program_header header = { 0 };

		header.p_align = 0x1000;
		header.p_type = PT_LOAD;

		if (segment->executable)
			header.p_flags |= PF_X;
		if (segment->readable)
			header.p_flags |= PF_R;
		if (segment->writable)
			header.p_flags |= PF_W;

		current_file_offset =
			round_up_to_nearest(current_file_offset, header.p_align);

		header.p_offset = current_file_offset;

		current_file_offset += segment->size;

		header.p_vaddr = segment->load_address;
		header.p_paddr = segment->load_address;
		header.p_filesz = segment->size;
		header.p_memsz = segment->load_size;

		write_program_header(fp, &header);

		segment_offsets[i] = header.p_offset;
	}

	current_file_offset = PH_OFF + 7 * 8 * executable->segment_size;
	assert(ftell(fp) == (long)current_file_offset);

	for (unsigned i = 0; i < executable->segment_size; i++) {
		write_skip(fp, segment_offsets[i]);
		struct segment *segment = &executable->segments[i];
		write(fp, segment->data, segment->size);
	}

	fclose(fp);

	free(segment_offsets);
}
