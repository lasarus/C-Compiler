#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stdio.h>
#include "codegen/rodata.h"

enum {
	R_X86_64_NONE = 0, /* No reloc */
	R_X86_64_64 = 1, /* Direct 64 bit  */
	R_X86_64_PC32 = 2, /* PC relative 32 bit signed */
	R_X86_64_GOT32 = 3, /* 32 bit GOT entry */
	R_X86_64_PLT32 = 4, /* 32 bit PLT address */
	R_X86_64_COPY = 5, /* Copy symbol at runtime */
	R_X86_64_GLOB_DAT = 6, /* Create GOT entry */
	R_X86_64_JUMP_SLOT = 7, /* Create PLT entry */
	R_X86_64_RELATIVE = 8, /* Adjust by program base */
	R_X86_64_GOTPCREL = 9, /* 32 bit signed PC relative */
	R_X86_64_32 = 10, /* Direct 32 bit zero extended */
	R_X86_64_32S = 11, /* Direct 32 bit sign extended */
	R_X86_64_16 = 12, /* Direct 16 bit zero extended */
	R_X86_64_PC16 = 13, /* 16 bit sign extended pc relative */
	R_X86_64_8 = 14, /* Direct 8 bit sign extended  */
};

void elf_init(void);
void elf_set_section(const char *section);
void elf_write(uint8_t *data, int len);
void elf_write_quad(uint64_t imm);
void elf_write_byte(uint8_t imm);
void elf_write_zero(int len);
void elf_finish(const char *path);

void elf_symbol_relocate(label_id label, int64_t offset, int64_t add, int type);
void elf_symbol_set(label_id label, int global);

#endif
