#include "variables.h"
#include "arch/x64.h"
#include "ir/ir.h"

#include <common.h>
#include <types.h>
#include <parser/expression_to_ir.h>
#include <parser/parser.h>

#include <stdlib.h>
#include <assert.h>

struct instruction **variable_instructions;
static size_t variables_size, variables_cap;

void variables_reset(void) {
	variables_size = variables_cap = 0;
	free(variable_instructions);
	variable_instructions = NULL;
}

var_id allocate_vla(struct type *type) {
	struct expr *size_expr = type_sizeof(type);
	var_id size = expression_to_size_t(size_expr);
	var_id ptr = ir_vla_alloc(size);
	return ptr;
}

var_id new_variable(struct instruction *instruction, int size) {
	if (size > 8 || size == 0)
		ICE("Invalid register size: %d", size);

	var_id id = variables_size;
	ADD_ELEMENT(variables_size, variables_cap, variable_instructions) = instruction;

	instruction->result = id;
	instruction->size = size;
	instruction->first_block = -1;
	instruction->spans_block = 0;

	return id;
}

int get_variable_size(var_id variable) {
	return variable_instructions[variable]->size;
}

void init_variables(void) {
	variables_size = 0;
	// VOID_VAR
	ADD_ELEMENT(variables_size, variables_cap, variable_instructions) = NULL;
}

struct instruction *var_get_instruction(var_id variable) {
	return variable_instructions[variable];
}
