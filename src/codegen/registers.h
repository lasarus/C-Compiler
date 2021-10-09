#ifndef REGISTERS_H
#define REGISTERS_H

#include <parser/parser.h>

enum {
	REG_RAX,
	REG_RBX,
	REG_RCX,
	REG_RDX,
	REG_RSI,
	REG_RDI,
	REG_RBP,
	REG_RSP,
	REG_R8,
	REG_R9,
	REG_R10,
	REG_R11,
	REG_R12,
	REG_R13,
	REG_R14,
	REG_R15,
};

void scalar_to_reg(var_id scalar, int reg);
void reg_to_scalar(int reg, var_id scalar);

char size_to_suffix(int size);
const char *get_reg_name(int id, int size);

// address in rdi.
void load_address(struct type *type, var_id result);
void store_address(struct type *type, var_id value);

#endif
