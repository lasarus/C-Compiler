#ifndef PARSER_H
#define PARSER_H

#include <ir/ir.h>

enum ir_binary_operator ibo_from_type_and_op(struct type *type, enum operator_type op);

void parse_into_ir(void);
void parser_reset(void);

extern struct parser_flags parser_flags;

int get_current_packing(void);

int parse_handle_pragma(void);

#endif
