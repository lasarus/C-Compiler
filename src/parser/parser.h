#ifndef PARSER_H
#define PARSER_H

#include <ir/ir.h>

enum operand_type ot_from_st(enum simple_type st);
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
