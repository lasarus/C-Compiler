#ifndef REGISTERS_H
#define REGISTERS_H

#include <parser/parser.h>

void scalar_to_reg(struct instruction *scalar, int reg);
void reg_to_scalar(int reg, struct instruction *scalar);
void reg_to_memory(int reg, int size);

const char *get_reg_name(int id, int size);

#endif
