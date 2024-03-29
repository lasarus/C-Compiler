#include "parser.h"
#include "declaration.h"
#include "ir/ir.h"
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
	while (parse_declaration(1) || TACCEPT(T_SEMI_COLON) || parse_handle_pragma());

	TEXPECT(T_EOI);

	generate_tentative_definitions();
	ir_seal_blocks();
}
