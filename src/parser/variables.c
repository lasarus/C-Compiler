#include "variables.h"
#include "expression.h"
#include "parser.h"

#include <common.h>
#include <types.h>

#include <stdlib.h>
#include <assert.h>

struct expr *construct_size_expression(struct type *type) {
	switch (type->type) {
	case TY_VARIABLE_LENGTH_ARRAY:
		return EXPR_BINARY_OP(OP_MUL, EXPR_VAR(type->variable_length_array.length, type_simple(ST_INT)),
							  construct_size_expression(type->children[0]));
	case TY_ARRAY:
		return EXPR_BINARY_OP(OP_MUL, EXPR_INT(type->array.length),
							  construct_size_expression(type->children[0]));
	default:
		return EXPR_INT(calculate_size(type));
	}
}

struct variable_data {
	int size;
} *variables = NULL;

static int variables_n, variables_cap = 0;


var_id new_variable(struct type *type, int allocate) {
	return new_variable_sz(calculate_size(type), allocate);
}

var_id allocate_vla(struct type **type) {
	struct type *n_type = type_pointer((*type)->children[0]);
	struct expr *size_expr = construct_size_expression(*type);
	var_id size = expression_to_ir(size_expr);
	var_id ptr = new_variable(n_type, 1);
	IR_PUSH_STACK_ALLOC(ptr, size);
	*type = n_type;
	return ptr;
}

var_id new_variable_sz(int size, int allocate) {
	// A bit of a shortcut.
	if (variables_n && size == 0)
		return VOID_VAR;

	if (variables_n >= variables_cap) {
		variables_cap *= 2;
		if (variables_cap == 0)
			variables_cap = 4;
		variables = realloc(variables, sizeof (*variables) * variables_cap);
	}

	var_id new_id = variables_n++;

	variables[new_id].size = size;

	if (allocate)
		allocate_var(new_id);
	//variable_set_type(new_id, type, allocate);

	return new_id;
}

int get_n_vars(void) {
	return variables_n;
}

int get_variable_size(var_id variable) {
	return variables[variable].size;
}

void init_variables(void) {
	variables_n = 0;
	new_variable(type_simple(ST_VOID), 0);
}

void change_variable_size(var_id var, int size) {
	variables[var].size = size;
}
