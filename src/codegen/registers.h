#ifndef REGISTERS_H
#define REGISTERS_H

#include <parser/parser.h>

void scalar_to_reg(var_id scalar, int reg);
void reg_to_scalar(int reg, var_id scalar);

char size_to_suffix(int size);
const char *get_reg_name(int id, int size);

// address in rdi.
void load_address(struct type *type, var_id result);
void store_address(struct type *type, var_id value);

#endif
