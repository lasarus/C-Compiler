#include "variables.h"

#include <common.h>
#include <types.h>
#include <parser/expression_to_ir.h>
#include <parser/parser.h>

#include <stdlib.h>
#include <assert.h>

struct variable_data {
	int size, stack_bucket;
} *variables = NULL;

static int variables_size, variables_cap = 0;

void variables_reset(void) {
	variables_size = variables_cap = 0;
	free(variables);
	variables = NULL;
}

var_id new_variable(struct type *type, int allocate, int stack_bucket) {
	return new_variable_sz(calculate_size(type), allocate, stack_bucket);
}

var_id new_ptr(void) {
	return new_variable_sz(8, 1, 0);
}

var_id allocate_vla(struct type *type) {
	static int dominance = 0;
	struct type *n_type = type_pointer(type->children[0]);
	struct expr *size_expr = type_sizeof(type);
	var_id size = expression_to_size_t(size_expr);
	var_id ptr = new_variable(n_type, 1, 0);
	var_id slot = new_variable(n_type, 1, 0);
	IR_PUSH_STACK_ALLOC(ptr, size, slot, dominance);
	dominance++;
	return ptr;
}

var_id new_variable_sz(int size, int allocate, int stack_bucket) {
	if (size > 8) {
		ICE("Too large register");
	}
	// A bit of a shortcut.
	if (variables_size && size == 0)
		return VOID_VAR;

	var_id id = variables_size;
	ADD_ELEMENT(variables_size, variables_cap, variables) = (struct variable_data) {
		.size = size,
		.stack_bucket = stack_bucket
	};

	if (allocate)
		allocate_var(id);

	if (stack_bucket) {
		ir_push1(IR_ADD_TEMPORARY, id);
	}

	return id;
}

int get_n_vars(void) {
	return variables_size;
}

int get_variable_size(var_id variable) {
	return variables[variable].size;
}

void init_variables(void) {
	variables_size = 0;
	new_variable(type_simple(ST_VOID), 0, 0);
}

void variable_set_stack_bucket(var_id var, int stack_bucket) {
	variables[var].stack_bucket = stack_bucket;
}

int get_variable_stack_bucket(var_id var) {
	return variables[var].stack_bucket;
}
