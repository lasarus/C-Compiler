#ifndef OPERATORS_H
#define OPERATORS_H

#include <types.h>
#include <arch/x64.h>

enum operator_type {
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_BXOR,
	OP_BOR,
	OP_BAND,
	OP_LSHIFT,
	OP_RSHIFT,
	OP_LESS,
	OP_GREATER,
	OP_LESS_EQ,
	OP_GREATER_EQ,
	OP_EQUAL,
	OP_NOT_EQUAL,

	OP_TYPE_COUNT
};

struct type *operators_get_result_type(enum operator_type op,
									   struct type *lhs_type,
									   struct type *rhs_type);
struct constant operators_constant(enum operator_type op,
								   struct constant lhs, struct constant rhs);

enum unary_operator_type {
	UOP_PLUS, UOP_NEG, UOP_BNOT,
	UOP_TYPE_COUNT
};

#endif
