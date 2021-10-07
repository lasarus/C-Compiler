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
		return EXPR_BINARY_OP(OP_MUL, EXPR_VAR(type->variable_length_array.length),
						   construct_size_expression(type->children[0]));
	case TY_ARRAY:
		return EXPR_BINARY_OP(OP_MUL, EXPR_INT(type->array.length),
						   construct_size_expression(type->children[0]));
	default:
		return EXPR_INT(calculate_size(type));
	}
}

struct type **variable_types = NULL;
static int variable_types_n = 0;
static int variable_types_cap = 0;

void variable_set_type(var_id var, struct type *type, int allocate) {
	if (has_variable_size(type)) {
		if (type->type != TY_VARIABLE_LENGTH_ARRAY)
			NOTIMP();
		variable_types[var] = type_pointer(type->children[0]);

		if (!allocate)
			ERROR("Must allocate variable length");

		allocate_var(var);

		struct expr *size_expr = construct_size_expression(type);
		var_id size = expression_to_ir(size_expr);
		IR_PUSH_STACK_ALLOC(var, size);
		// Should be represented as a pointer.
	} else {
		variable_types[var] = type;

		if (allocate)
			allocate_var(var);
	}
}

var_id new_variable(struct type *type, int allocate) {
	// A bit of a shortcut.
	if (variable_types_n && type->type == TY_SIMPLE && type->simple == ST_VOID)
		return VOID_VAR;

	if (variable_types_n >= variable_types_cap) {
		variable_types_cap *= 2;
		if (variable_types_cap == 0)
			variable_types_cap = 4;
		variable_types = realloc(variable_types, sizeof (*variable_types) * variable_types_cap);
	}


	var_id new_id = variable_types_n++;
	variable_set_type(new_id, type, allocate);

	return new_id;
}

int get_n_vars(void) {
	return variable_types_n;
}

void allocate_var(var_id var) {
	struct type *type = get_variable_type(var);

	if (has_variable_size(type)) {
		ERROR("Can't statically allocate storage of variable size");
	} else {
		IR_PUSH(.type = IR_ALLOCA, .alloca = {var});
	}
}

struct type *get_variable_type(var_id variable) {
	return variable_types[variable];
}

void init_variables(void) {
	variable_types_n = 0;
	new_variable(type_simple(ST_VOID), 0);
}

void change_variable_type(var_id var, struct type *type) {
	variable_set_type(var, type, 0);
}
