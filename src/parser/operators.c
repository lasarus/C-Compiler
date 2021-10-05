#include "operators.h"

#include <common.h>
#include <string.h>

struct type *operators_get_result_type(enum operator_type op,
									   struct type *lhs_type,
									   struct type *rhs_type) {
	static int returns_int[OP_TYPE_COUNT] = {
		[OP_GREATER] = 1,
		[OP_GREATER_EQ] = 1,
		[OP_LESS] = 1,
		[OP_LESS_EQ] = 1,
		[OP_EQUAL] = 1,
		[OP_NOT_EQUAL] = 1,
	};

	static int supports_pointer[OP_TYPE_COUNT] = {
		[OP_GREATER] = 1,
		[OP_GREATER_EQ] = 1,
		[OP_LESS] = 1,
		[OP_LESS_EQ] = 1,
		[OP_EQUAL] = 1,
		[OP_NOT_EQUAL] = 1,
	};

	if (lhs_type != rhs_type) {
		if (!supports_pointer[op] ||
			type_is_pointer(lhs_type) != type_is_pointer(rhs_type)) {
			ERROR("Can't perform op %d on %s and %s\n", op,
				  strdup(type_to_string(lhs_type)),
				  strdup(type_to_string(rhs_type)));
		}
	}

	return returns_int[op] ? type_simple(ST_INT) : lhs_type;
}

int op_integer(enum operator_type op, int lhs, int rhs) {
	switch (op) {
	case OP_ADD: return lhs + rhs;
	case OP_SUB: return lhs - rhs;
	case OP_MUL: return lhs * rhs;
	case OP_DIV: return lhs / rhs;
	case OP_MOD: return lhs % rhs;
	case OP_BXOR: return lhs ^ rhs;
	case OP_BOR: return lhs | rhs;
	case OP_BAND: return lhs & rhs;
	case OP_LSHIFT: return lhs << rhs;
	case OP_RSHIFT: return lhs >> rhs;
	case OP_LESS: return lhs < rhs;
	case OP_GREATER: return lhs > rhs;
	case OP_LESS_EQ: return lhs <= rhs;
	case OP_GREATER_EQ: return lhs >= rhs;
	case OP_EQUAL: return lhs == rhs;
	case OP_NOT_EQUAL: return lhs != rhs;
	default: NOTIMP();
	}
}

struct constant operators_constant(enum operator_type op,
								   struct constant lhs, struct constant rhs) {
	if (lhs.type != CONSTANT_TYPE ||
		rhs.type != CONSTANT_TYPE)
		NOTIMP();

	struct type *type = lhs.data_type;
	if (type != rhs.data_type)
		ERROR("Can't perform binary operator on constant of differing type");

	if (type->type != TY_SIMPLE)
		NOTIMP();

	switch (type->simple) {
	case ST_INT:
		return (struct constant) {
			.type = CONSTANT_TYPE,
			.data_type = type_simple(ST_INT),
			.i = op_integer(op, lhs.i, rhs.i)
		};
		break;
	default:
		NOTIMP();
	}
}
