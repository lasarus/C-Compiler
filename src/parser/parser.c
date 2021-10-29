#include "parser.h"
#include "declaration.h"
#include "symbols.h"

#include <common.h>
#include <preprocessor/preprocessor.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct parser_flags parser_flags = {
	.dmodel = DMODEL_LP64
};

void parse_into_ir() {
	init_variables();

	while (parse_declaration(1));
	if (TACCEPT(T_SEMI_COLON)) {
		PRINT_POS(T0->pos);
		ERROR("Extra semicolon outside function.");
	}
	TEXPECT(T_EOI);
}

enum operand_type ot_from_st(enum simple_type st) {
	switch (st) {
	case ST_INT: return OT_INT;
	case ST_UINT: return OT_UINT;
	case ST_LONG: return OT_LONG;
	case ST_ULONG: return OT_ULONG;
	case ST_LLONG: return OT_LLONG;
	case ST_ULLONG: return OT_ULLONG;
	case ST_FLOAT: return OT_FLOAT;
	case ST_DOUBLE: return OT_DOUBLE;
	default: ERROR("Invalid operand type %d", st);
	}
}

enum operand_type ot_from_type(struct type *type) {
	if (type_is_pointer(type)) {
		return OT_PTR;
	} else if (type->type == TY_SIMPLE) {
		return ot_from_st(type->simple);
	} else {
		ERROR("Invalid operand type");
	}
}
