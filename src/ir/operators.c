#include "operators.h"

#include <common.h>
#include <preprocessor/preprocessor.h>

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
			ICE("Can't perform op %d on %s and %s\n", op,
				strdup(dbg_type(lhs_type)),
				strdup(dbg_type(rhs_type)));
		}
	}

	return returns_int[op] ? type_simple(ST_INT) : lhs_type;
}

int operators_constant(enum operator_type op,
					   struct constant lhs, struct constant rhs,
					   struct constant *result) {
	if (lhs.type != CONSTANT_TYPE ||
		rhs.type != CONSTANT_TYPE) {
		return 0;
	}

	struct type *type = lhs.data_type;
	if (type != rhs.data_type)
		return 0;

	if (type->type != TY_SIMPLE)
		return 0;

	enum simple_type st = type->simple;

	struct constant res = {
		.type = CONSTANT_TYPE,
		.data_type = operators_get_result_type(op, lhs.data_type, rhs.data_type)
	};

	if (type_is_simple(type, ST_FLOAT)) {
		switch (op) {
		case OP_ADD: res.float_d = lhs.float_d + rhs.float_d; break;
		case OP_SUB: res.float_d = lhs.float_d - rhs.float_d; break;
		case OP_MUL: res.float_d = lhs.float_d * rhs.float_d; break;
		case OP_DIV: res.float_d = lhs.float_d / rhs.float_d; break;
		case OP_LESS: res.int_d = lhs.float_d < rhs.float_d; break;
		case OP_GREATER: res.int_d = lhs.float_d > rhs.float_d; break;
		case OP_LESS_EQ: res.int_d = lhs.float_d <= rhs.float_d; break;
		case OP_GREATER_EQ: res.int_d = lhs.float_d >= rhs.float_d; break;
		case OP_EQUAL: res.int_d = lhs.float_d == rhs.float_d; break;
		case OP_NOT_EQUAL: res.int_d = lhs.float_d != rhs.float_d; break;
		default: NOTIMP();
		}
	} else if (type_is_simple(type, ST_DOUBLE)) {
		switch (op) {
		case OP_ADD: res.double_d = lhs.double_d + rhs.double_d; break;
		case OP_SUB: res.double_d = lhs.double_d - rhs.double_d; break;
		case OP_MUL: res.double_d = lhs.double_d * rhs.double_d; break;
		case OP_DIV: res.double_d = lhs.double_d / rhs.double_d; break;
		case OP_LESS: res.int_d = lhs.double_d < rhs.double_d; break;
		case OP_GREATER: res.int_d = lhs.double_d > rhs.double_d; break;
		case OP_LESS_EQ: res.int_d = lhs.double_d <= rhs.double_d; break;
		case OP_GREATER_EQ: res.int_d = lhs.double_d >= rhs.double_d; break;
		case OP_EQUAL: res.int_d = lhs.double_d == rhs.double_d; break;
		case OP_NOT_EQUAL: res.int_d = lhs.double_d != rhs.double_d; break;
		default: NOTIMP();
		}
	} else if (type_is_integer(type) && is_signed(st)) {
		// Overflow is undefined, so we can just ignore truncation completely.
		if ((op == OP_DIV || op == OP_DIV) && rhs.int_d == 0)
			return 0;

		switch (op) {
		case OP_ADD: res.int_d = lhs.int_d + rhs.int_d; break;
		case OP_SUB: res.int_d = lhs.int_d - rhs.int_d; break;
		case OP_MUL: res.int_d = lhs.int_d * rhs.int_d; break;
		case OP_DIV: res.int_d = lhs.int_d / rhs.int_d; break;
		case OP_MOD: res.int_d = lhs.int_d % rhs.int_d; break;
		case OP_BXOR: res.int_d = lhs.int_d ^ rhs.int_d; break;
		case OP_BOR: res.int_d = lhs.int_d | rhs.int_d; break;
		case OP_BAND: res.int_d = lhs.int_d & rhs.int_d; break;
		case OP_LSHIFT: res.int_d = lhs.int_d << rhs.int_d; break;
		case OP_RSHIFT: res.int_d = lhs.int_d >> rhs.int_d; break;
		case OP_LESS: res.int_d = lhs.int_d < rhs.int_d; break;
		case OP_GREATER: res.int_d = lhs.int_d > rhs.int_d; break;
		case OP_LESS_EQ: res.int_d = lhs.int_d <= rhs.int_d; break;
		case OP_GREATER_EQ: res.int_d = lhs.int_d >= rhs.int_d; break;
		case OP_EQUAL: res.int_d = lhs.int_d == rhs.int_d; break;
		case OP_NOT_EQUAL: res.int_d = lhs.int_d != rhs.int_d; break;
		default: NOTIMP();
		}
	} else if (type_is_integer(type) && !is_signed(st)) {
		int sign = 0;
		switch (op) {
		case OP_ADD: res.uint_d = lhs.uint_d + rhs.uint_d; break;
		case OP_SUB: res.uint_d = lhs.uint_d - rhs.uint_d; break;
		case OP_MUL: res.uint_d = lhs.uint_d * rhs.uint_d; break;
		case OP_DIV: res.uint_d = lhs.uint_d / rhs.uint_d; break;
		case OP_MOD: res.uint_d = lhs.uint_d % rhs.uint_d; break;
		case OP_BXOR: res.uint_d = lhs.uint_d ^ rhs.uint_d; break;
		case OP_BOR: res.uint_d = lhs.uint_d | rhs.uint_d; break;
		case OP_BAND: res.uint_d = lhs.uint_d & rhs.uint_d; break;
		case OP_LSHIFT: res.uint_d = lhs.uint_d << rhs.uint_d; break;
		case OP_RSHIFT: res.uint_d = lhs.uint_d >> rhs.uint_d; break;
		case OP_LESS: res.int_d = lhs.uint_d < rhs.uint_d; sign = 1; break;
		case OP_GREATER: res.int_d = lhs.uint_d > rhs.uint_d; sign = 1; break;
		case OP_LESS_EQ: res.int_d = lhs.uint_d <= rhs.uint_d; sign = 1; break;
		case OP_GREATER_EQ: res.int_d = lhs.uint_d >= rhs.uint_d; sign = 1; break;
		case OP_EQUAL: res.int_d = lhs.uint_d == rhs.uint_d; sign = 1; break;
		case OP_NOT_EQUAL: res.int_d = lhs.uint_d != rhs.uint_d; sign = 1; break;
		default: NOTIMP();
		}

		if (!sign)
			constant_normalize(&res);
	} else {
		NOTIMP();
	}

	*result = res;

	return 1;
}

int operators_constant_unary(enum unary_operator_type op,
							 struct constant rhs, struct constant *result) {
	if (rhs.type != CONSTANT_TYPE)
		return 0;

	struct type *type = rhs.data_type;

	if (type->type != TY_SIMPLE)
		return 0;

	enum simple_type st = type->simple;

	struct constant res = rhs;

	if (type_is_simple(type, ST_FLOAT)) {
		switch (op) {
		case UOP_PLUS: res.float_d = +rhs.float_d; break;
		case UOP_NEG: res.float_d = -rhs.float_d; break;
		default: NOTIMP();
		}
	} else if (type_is_simple(type, ST_DOUBLE)) {
		switch (op) {
		case UOP_PLUS: res.double_d = +rhs.double_d; break;
		case UOP_NEG: res.double_d = -rhs.double_d; break;
		default: NOTIMP();
		}
	} else if (type_is_integer(type) && is_signed(st)) {
		switch (op) {
		case UOP_PLUS: res.int_d = +rhs.int_d; break;
		case UOP_NEG: res.int_d = -rhs.int_d; break;
		case UOP_BNOT: res.int_d = ~rhs.int_d; break;
		default: NOTIMP();
		}
	} else if (type_is_integer(type) && !is_signed(st)) {
		switch (op) {
		case UOP_PLUS: res.uint_d = +rhs.uint_d; break;
		case UOP_NEG: res.uint_d = -rhs.uint_d; break;
		case UOP_BNOT: res.uint_d = ~rhs.uint_d; break;
		default: NOTIMP();
		}

		constant_normalize(&res);
	} else {
		NOTIMP();
	}

	*result = res;

	return 1;
}
