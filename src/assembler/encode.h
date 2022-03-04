#ifndef ENCODE_H
#define ENCODE_H

#include <common.h>
#include "assembler.h"

struct relocation {
	enum {
		REL_INS,
	} type;

	int offset;
	int size;

	int relative;

	label_id label;
	uint64_t imm;
};

void assemble_instruction(uint8_t *output, int *len, const char *mnemonic, struct operand ops[4],
						  struct relocation relocations[], int *n_relocations);

#endif
