#include "parser.h"
#include "declaration.h"
#include "symbols.h"

#include <common.h>
#include <preprocessor/preprocessor.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

void parser_reset(void) {
	symbols_reset();
}

void parse_into_ir(void) {
	init_variables();

	while (parse_declaration(1) || TACCEPT(T_SEMI_COLON));

	TEXPECT(T_EOI);

	generate_tentative_definitions();
}

enum ir_binary_operator ibo_from_type_and_op(struct type *type, enum operator_type op) {
	int sign = 0;
	if (type_is_floating(type)) {
		switch (op) {
		case OP_ADD: return IBO_FLT_ADD;
		case OP_SUB: return IBO_FLT_SUB;
		case OP_MUL: return IBO_FLT_MUL;
		case OP_DIV: return IBO_FLT_DIV;
		case OP_LESS: return IBO_FLT_LESS;
		case OP_GREATER: return IBO_FLT_GREATER;
		case OP_LESS_EQ: return IBO_FLT_LESS_EQ;
		case OP_GREATER_EQ: return IBO_FLT_GREATER_EQ;
		case OP_EQUAL: return IBO_FLT_EQUAL;
		case OP_NOT_EQUAL: return IBO_FLT_NOT_EQUAL;
		default: NOTIMP();
		}
	} else if (type->type == TY_SIMPLE) {
		sign = is_signed(type->simple);
	} else if (type->type == TY_POINTER) {
		sign = 0;
	} else {
		NOTIMP();
	}

	switch (op) {
	case OP_ADD: return IBO_ADD;
	case OP_SUB: return IBO_SUB;
	case OP_MUL: return sign ? IBO_IMUL : IBO_MUL;
	case OP_DIV: return sign ? IBO_IDIV : IBO_DIV;
	case OP_MOD: return sign ? IBO_IMOD : IBO_MOD;
	case OP_BXOR: return IBO_BXOR;
	case OP_BOR: return IBO_BOR;
	case OP_BAND: return IBO_BAND;
	case OP_LSHIFT: return IBO_LSHIFT;
	case OP_RSHIFT: return sign ? IBO_IRSHIFT : IBO_RSHIFT;
	case OP_LESS: return sign ? IBO_ILESS : IBO_LESS;
	case OP_GREATER: return sign ? IBO_IGREATER : IBO_GREATER;
	case OP_LESS_EQ: return sign ? IBO_ILESS_EQ : IBO_LESS_EQ;
	case OP_GREATER_EQ: return sign ? IBO_IGREATER_EQ : IBO_GREATER_EQ;
	case OP_EQUAL: return IBO_EQUAL;
	case OP_NOT_EQUAL: return IBO_NOT_EQUAL;
	default: NOTIMP();
	}
}
