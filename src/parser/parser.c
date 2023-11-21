#include "parser.h"
#include "declaration.h"
#include "symbols.h"

#include <common.h>
#include <preprocessor/preprocessor.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static size_t pack_size, pack_cap;
static int *packs;
static int current_packing;

int get_current_packing(void) {
	return current_packing;
}

void parser_reset(void) {
	pack_size = pack_cap = 0;
	current_packing = 0;
	free(packs);
	symbols_reset();
}

int parse_handle_pragma(void) {
	if (!TACCEPT(PP_DIRECTIVE))
		return 0;
	assert(sv_string_cmp(T0->str, "pragma"));

	TNEXT();

	if (T0->type != T_IDENT)
		ERROR(T0->pos, "Expected identifier\n");

	struct string_view name = T0->str;

	if (sv_string_cmp(name, "pack")) {
		int new_packing = 0;
		TNEXT();
		TEXPECT(T_LPAR);

		if (T0->type == T_IDENT) {
			if (sv_string_cmp(T0->str, "pop")) {
				if (pack_size)
					pack_size--;
				TNEXT();
				TEXPECT(T_RPAR);
				return 1;
			} else if (sv_string_cmp(T0->str, "push")) {
				ADD_ELEMENT(pack_size, pack_cap, packs) = current_packing;
				TNEXT();

				if (!TACCEPT(T_COMMA)) {
					TEXPECT(T_RPAR);
					return 1;
				}
			} else {
				NOTIMP();
			}
		}

		if (T0->type == T_NUM) {
			struct constant c = constant_from_string(T0->str);
			if (!type_is_integer(c.data_type))
				ERROR(T0->pos, "Packing must be integer.");

			new_packing = is_signed(c.data_type->simple) ? (intmax_t)c.int_d : (intmax_t)c.uint_d;
			TNEXT();
		}

		TEXPECT(T_RPAR);

		current_packing = new_packing;
	} else {
		WARNING(T0->pos, "\"#pragma %.*s\" not supported", name.len, name.str);

		// Continue until newline or EOI.
		while (T0->type != T_EOI &&
			   !T0->first_of_line)
			TNEXT();
	}

	return 1;
}

void parse_into_ir(void) {
	init_variables();

	while (parse_declaration(1) || TACCEPT(T_SEMI_COLON) || parse_handle_pragma());

	TEXPECT(T_EOI);

	generate_tentative_definitions();
}

int ir_from_type_and_op(struct type *type, enum operator_type op) {
	int sign = 0;
	if (type_is_floating(type)) {
		switch (op) {
		case OP_ADD: return IR_FLT_ADD;
		case OP_SUB: return IR_FLT_SUB;
		case OP_MUL: return IR_FLT_MUL;
		case OP_DIV: return IR_FLT_DIV;
		case OP_LESS: return IR_FLT_LESS;
		case OP_GREATER: return IR_FLT_GREATER;
		case OP_LESS_EQ: return IR_FLT_LESS_EQ;
		case OP_GREATER_EQ: return IR_FLT_GREATER_EQ;
		case OP_EQUAL: return IR_FLT_EQUAL;
		case OP_NOT_EQUAL: return IR_FLT_NOT_EQUAL;
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
	case OP_ADD: return IR_ADD;
	case OP_SUB: return IR_SUB;
	case OP_MUL: return sign ? IR_IMUL : IR_MUL;
	case OP_DIV: return sign ? IR_IDIV : IR_DIV;
	case OP_MOD: return sign ? IR_IMOD : IR_MOD;
	case OP_BXOR: return IR_BXOR;
	case OP_BOR: return IR_BOR;
	case OP_BAND: return IR_BAND;
	case OP_LSHIFT: return IR_LSHIFT;
	case OP_RSHIFT: return sign ? IR_IRSHIFT : IR_RSHIFT;
	case OP_LESS: return sign ? IR_ILESS : IR_LESS;
	case OP_GREATER: return sign ? IR_IGREATER : IR_GREATER;
	case OP_LESS_EQ: return sign ? IR_ILESS_EQ : IR_LESS_EQ;
	case OP_GREATER_EQ: return sign ? IR_IGREATER_EQ : IR_GREATER_EQ;
	case OP_EQUAL: return IR_EQUAL;
	case OP_NOT_EQUAL: return IR_NOT_EQUAL;
	default: NOTIMP();
	}
}
