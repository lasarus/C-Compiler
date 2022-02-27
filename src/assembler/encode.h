#ifndef ENCODE_H
#define ENCODE_H

#include <common.h>
#include "assembler.h"

void assemble_instruction(uint8_t *output, int *len, const char *mnemonic,
						  struct operand ops[4]);

#endif
