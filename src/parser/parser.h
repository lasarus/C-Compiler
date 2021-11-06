#ifndef PARSER_H
#define PARSER_H

#include <ir/ir.h>

enum ir_binary_operator ibo_from_type_and_op(struct type *type, enum operator_type op);
enum operand_type ot_from_type(struct type *type);

void parse_into_ir(void);

struct parser_flags {
	enum {
		DMODEL_ILP64,
		DMODEL_LLP64,
		DMODEL_LP64
	} dmodel;
};

extern struct parser_flags parser_flags;

#endif
