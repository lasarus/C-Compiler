#include "builtins.h"
#include <stdlib.h>
#include <common.h>

#include <parser/symbols.h>

struct struct_data *builtin_va_list = NULL;

void builtins_init(void) {
	struct symbol_typedef *sym =
		symbols_add_typedef("__builtin_va_list");

	struct type *uint = type_simple(ST_UINT);
	struct type *vptr = type_pointer(type_simple(ST_VOID));
	struct type **members = malloc(sizeof(*members) * 4);
	char **names = malloc(sizeof(*names) * 4);
	members[0] = uint;
	names[0] = "gp_offset";
	members[1] = uint;
	names[1] = "fp_offset";
	members[2] = vptr;
	names[2] = "overflow_arg_area";
	members[3] = vptr;
	names[3] = "reg_save_area";

	struct struct_data *struct_data = register_struct();
	*struct_data = (struct struct_data) {
		.name = "<__builtin_va_list_struct>",
		.is_complete = 1,
		.n = 4,
		.types = members,
		.names = names
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
