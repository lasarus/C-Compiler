#include "builtins.h"

#include <parser/symbols.h>

#include <stdlib.h>
#include <common.h>

struct struct_data *builtin_va_list = NULL;

void builtins_init(void) {
	struct symbol_typedef *sym =
		symbols_add_typedef("__builtin_va_list");

	struct type *uint = type_simple(ST_UINT);
	struct type *vptr = type_pointer(type_simple(ST_VOID));

	struct field *fields = malloc(sizeof *fields * 4);
	for (int i = 0; i < 4; i++)
		fields[i].bitfield = -1;
	fields[0].type = uint;
	fields[0].name = "gp_offset";
	fields[1].type = uint;
	fields[1].name = "fp_offset";
	fields[2].type = vptr;
	fields[2].name = "overflow_arg_area";
	fields[3].type = vptr;
	fields[3].name = "reg_save_area";

	struct struct_data *struct_data = register_struct();
	*struct_data = (struct struct_data) {
		.name = "<__builtin_va_list_struct>",
		.is_complete = 1,
		.n = 4,
		.fields = fields
	};

	calculate_offsets(struct_data);

	builtin_va_list = struct_data;

	struct type *struct_type = type_struct(struct_data);

	struct type final_params = {
		.type = TY_ARRAY,
		.array.length = 1,
		.n = 1
	};
	struct type *final = type_create(&final_params, &struct_type);

	sym->data_type = final;
}
