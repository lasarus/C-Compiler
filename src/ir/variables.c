#include "variables.h"
#include "arch/x64.h"
#include "ir/ir.h"

#include <common.h>
#include <types.h>
#include <parser/expression_to_ir.h>
#include <parser/parser.h>

#include <stdlib.h>
#include <assert.h>

struct variable_data *variable_data;
static size_t variables_size, variables_cap;

void variables_reset(void) {
	variables_size = variables_cap = 0;
	free(variable_data);
	variable_data = NULL;
}

var_id new_ptr(void) {
	return new_variable(8);
}

var_id allocate_vla(struct type *type) {
	struct expr *size_expr = type_sizeof(type);
	var_id size = expression_to_size_t(size_expr);
	var_id ptr = ir_vla_alloc(size);
	return ptr;
}

var_id new_variable(int size) {
	if (size > 8) {
		ICE("Too large register");
	}
	// A bit of a shortcut.
	if (size == 0)
		return VOID_VAR;

	var_id id = variables_size;
	ADD_ELEMENT(variables_size, variables_cap, variable_data) = (struct variable_data) {
		.size = size,
		.first_block = -1,
		.spans_block = 0,
	};

	allocate_var(id);

	return id;
}

int get_n_vars(void) {
	return variables_size;
}

int get_variable_size(var_id variable) {
	return variable_data[variable].size;
}

void init_variables(void) {
	variables_size = 0;
	// VOID_VAR
	ADD_ELEMENT(variables_size, variables_cap, variable_data) = (struct variable_data) { 0 };
}

struct variable_data *var_get_data(var_id variable) {
	return &variable_data[variable];
}
