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
			PRINT_POS(T0->pos);
			ERROR("Can't perform op %d on %s and %s\n", op,
				  strdup(dbg_type(lhs_type)),
				  strdup(dbg_type(rhs_type)));
		}
	}

	return returns_int[op] ? type_simple(ST_INT) : lhs_type;
}

struct constant operators_constant(enum operator_type op,
								   struct constant lhs, struct constant rhs) {
	if (lhs.type != CONSTANT_TYPE ||
		rhs.type != CONSTANT_TYPE) {
		printf("Trying op %d between %d and %d\n", op, lhs.type, rhs.type);
		NOTIMP();
	}

	struct type *type = lhs.data_type;
	if (type != rhs.data_type)
		ERROR("Can't perform binary operator on constant of differing type");

	if (type->type != TY_SIMPLE)
		NOTIMP();

#define OP(TYPE, FIELD) case TYPE:										\
		switch (op) {													\
		case OP_ADD: res.FIELD = lhs.FIELD + rhs.FIELD; break;			\
		case OP_SUB: res.FIELD = lhs.FIELD - rhs.FIELD; break;			\
		case OP_MUL: res.FIELD = lhs.FIELD * rhs.FIELD; break;			\
		case OP_DIV: res.FIELD = lhs.FIELD / rhs.FIELD; break;			\
		case OP_MOD: res.FIELD = lhs.FIELD % rhs.FIELD; break;			\
		case OP_BXOR: res.FIELD = lhs.FIELD ^ rhs.FIELD; break;			\
		case OP_BOR: res.FIELD = lhs.FIELD | rhs.FIELD; break;			\
		case OP_BAND: res.FIELD = lhs.FIELD & rhs.FIELD; break;			\
		case OP_LSHIFT: res.FIELD = lhs.FIELD << rhs.FIELD; break;		\
		case OP_RSHIFT: res.FIELD = lhs.FIELD >> rhs.FIELD; break;		\
		case OP_LESS: res.int_d = lhs.FIELD < rhs.FIELD; break;			\
		case OP_GREATER: res.int_d = lhs.FIELD > rhs.FIELD; break;		\
		case OP_LESS_EQ: res.int_d = lhs.FIELD <= rhs.FIELD; break;		\
		case OP_GREATER_EQ: res.int_d = lhs.FIELD >= rhs.FIELD; break;	\
		case OP_EQUAL: res.int_d = lhs.FIELD == rhs.FIELD; break;		\
		case OP_NOT_EQUAL: res.int_d = lhs.FIELD != rhs.FIELD; break;	\
		default: NOTIMP();									\
		} break;											

#define OPF(TYPE, FIELD) case TYPE:										\
		switch (op) {													\
		case OP_ADD: res.FIELD = lhs.FIELD + rhs.FIELD; break;			\
		case OP_SUB: res.FIELD = lhs.FIELD - rhs.FIELD; break;			\
		case OP_MUL: res.FIELD = lhs.FIELD * rhs.FIELD; break;			\
		case OP_DIV: res.FIELD = lhs.FIELD / rhs.FIELD; break;			\
		case OP_LESS: res.int_d = lhs.FIELD < rhs.FIELD; break;			\
		case OP_GREATER: res.int_d = lhs.FIELD > rhs.FIELD; break;		\
		case OP_LESS_EQ: res.int_d = lhs.FIELD <= rhs.FIELD; break;		\
		case OP_GREATER_EQ: res.int_d = lhs.FIELD >= rhs.FIELD; break;	\
		case OP_EQUAL: res.int_d = lhs.FIELD == rhs.FIELD; break;		\
		case OP_NOT_EQUAL: res.int_d = lhs.FIELD != rhs.FIELD; break;	\
		default: NOTIMP();									\
		} break;											

	struct constant res = {
		.type = CONSTANT_TYPE,
		.data_type = operators_get_result_type(op, lhs.data_type, rhs.data_type)
	};
	switch (type->simple) {
		OP(ST_INT, int_d);
		OP(ST_UINT, uint_d);
		OP(ST_LONG, long_d);
		OP(ST_ULONG, ulong_d);
		OP(ST_LLONG, llong_d);
		OP(ST_ULLONG, ullong_d);
		OPF(ST_FLOAT, float_d);
		OPF(ST_DOUBLE, double_d);
	default:
		NOTIMP();
	}

	return res;
}

int uop_integer(enum unary_operator_type op, int rhs) {
	switch (op) {
	case UOP_PLUS: return +rhs;
	case UOP_NEG: return -rhs;
	case UOP_BNOT: return ~rhs;
	default: NOTIMP();
	}
}

struct constant operators_constant_unary(enum unary_operator_type op,
										 struct constant rhs) {
	if (rhs.type != CONSTANT_TYPE)
		NOTIMP();

	struct type *type = rhs.data_type;

	if (type->type != TY_SIMPLE)
		NOTIMP();

	struct constant res = { 0 };
#define UOP(TYPE, FIELD) case TYPE: res.type = CONSTANT_TYPE; res.data_type = type_simple(TYPE); \
	switch (op) {														\
	case UOP_PLUS: res.FIELD = +rhs.FIELD; break;						\
	case UOP_NEG: res.FIELD = -rhs.FIELD; break;						\
	case UOP_BNOT: res.FIELD = ~rhs.FIELD; break;						\
	default: NOTIMP();													\
	} break;															\

#define UOPF(TYPE, FIELD) case TYPE: res.type = CONSTANT_TYPE; res.data_type = type_simple(TYPE); \
	switch (op) {														\
	case UOP_PLUS: res.FIELD = +rhs.FIELD; break;						\
	case UOP_NEG: res.FIELD = -rhs.FIELD; break;						\
	default: NOTIMP();													\
	} break;															\

	switch (type->simple) {
		UOP(ST_INT, int_d);
		UOP(ST_UINT, uint_d);
		UOP(ST_LONG, long_d);
		UOP(ST_ULONG, ulong_d);
		UOP(ST_LLONG, llong_d);
		UOP(ST_ULLONG, ullong_d);
		UOPF(ST_FLOAT, float_d);
		UOPF(ST_DOUBLE, double_d);
	default:
		NOTIMP();
	}

	return res;
}
