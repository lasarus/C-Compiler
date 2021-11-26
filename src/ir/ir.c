#include "ir.h"

#include <common.h>
#include <parser/declaration.h>
#include <parser/expression.h>
#include <arch/calling.h>
#include <codegen/registers.h>
#include <abi/abi.h>

#include <assert.h>

static size_t block_size, block_cap;
static struct block *blocks;

block_id new_block() {
	int id = (int)block_size;
	ADD_ELEMENT(block_size, block_cap, blocks) = (struct block) {
		.id = id,
		.exit.type = BLOCK_EXIT_NONE
	};

	return id;
}

struct block *get_block(block_id id) {
	return blocks + id;
}

struct ir ir;

struct function *get_current_function(void) {
	return &ir.functions[ir.size - 1];
}

struct block *get_current_block(void) {
	struct function *func = get_current_function();
	return get_block(func->blocks[func->size - 1]);
}

void push_ir(struct instruction instruction) {
	struct block *block = get_current_block();

	ADD_ELEMENT(block->size, block->cap, block->instructions) = instruction;
}

void ir_block_start(block_id id) {
	struct function *func = get_current_function();

	ADD_ELEMENT(func->size, func->cap, func->blocks) = id;
}

void allocate_var(var_id var) {
	struct function *func = &ir.functions[ir.size - 1];
	ADD_ELEMENT(func->var_size, func->var_cap, func->vars) = var;
}

void ir_if_selection(var_id condition, block_id block_true, block_id block_false) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_IF;
	block->exit.if_.condition = condition;
	block->exit.if_.block_true = block_true;
	block->exit.if_.block_false = block_false;
}

void ir_switch_selection(var_id condition, struct case_labels labels) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_SWITCH;
	block->exit.switch_.condition = condition;
	block->exit.switch_.labels = labels;
}

void ir_goto(block_id jump) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_JUMP;
	block->exit.jump = jump;
}

void ir_return(var_id value, struct type *type) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_RETURN;
	block->exit.return_.type = type;
	block->exit.return_.value = value;

	abi_ir_function_return(get_current_function(), value, type);
}

void ir_return_void(void) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_RETURN;
	block->exit.return_.type = type_simple(ST_VOID);
	block->exit.return_.value = 0;
}

void ir_get_offset(var_id member_address, var_id base_address, var_id offset_var, int offset) {
	if (!offset_var)
		offset_var = new_variable(type_pointer(type_simple(ST_VOID)), 1, 1);
	IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.pointer_type, offset), offset_var);
	IR_PUSH_BINARY_OPERATOR(IBO_ADD, base_address, offset_var, member_address);
}

void ir_set_bits(var_id result, var_id field, var_id value, int offset, int length) {
	uint64_t mask = gen_mask(64 - offset - length, offset);
	var_id mask_var = new_variable_sz(8, 1, 1);
	var_id shift_var = new_variable_sz(8, 1, 1);

	var_id value_large = new_variable_sz(8, 1, 1);
	var_id field_large = new_variable_sz(8, 1, 1);
	var_id result_large = new_variable_sz(8, 1, 1);

	IR_PUSH_INT_CAST(value_large, value, 0);
	IR_PUSH_INT_CAST(field_large, field, 0);

	IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.size_type, offset), shift_var);
	IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.size_type, mask), mask_var);
	IR_PUSH_BINARY_OPERATOR(IBO_BAND, mask_var, field_large, result_large);
	IR_PUSH_BINARY_NOT(mask_var, mask_var);
	IR_PUSH_BINARY_OPERATOR(IBO_LSHIFT, value_large, shift_var, value_large);
	IR_PUSH_BINARY_OPERATOR(IBO_BAND, mask_var, value_large, mask_var);
	IR_PUSH_BINARY_OPERATOR(IBO_BOR, mask_var, result_large, result_large);

	IR_PUSH_INT_CAST(result, result_large, 0);
}

void ir_get_bits(var_id result, var_id field, int offset, int length, int sign_extend) {
	var_id field_large = new_variable_sz(8, 1, 1);
	var_id shift_var = new_variable_sz(8, 1, 1);

	IR_PUSH_INT_CAST(field_large, field, 0);

	IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.size_type, 64 - offset - length), shift_var);
	IR_PUSH_BINARY_OPERATOR(IBO_LSHIFT, field_large, shift_var, field_large);

	IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.size_type, 64 - length), shift_var);

	if (sign_extend) {
		IR_PUSH_BINARY_OPERATOR(IBO_IRSHIFT, field_large, shift_var, field_large);
	} else {
		IR_PUSH_BINARY_OPERATOR(IBO_RSHIFT, field_large, shift_var, field_large);
	}

	IR_PUSH_INT_CAST(result, field_large, 0);
}

static void ir_init_var_recursive(struct initializer *init, struct type *type, var_id offset,
								  int bit_offset, int bit_size) {
	switch (init->type) {
	case INIT_BRACE: {
		var_id child_offset_var = new_variable(type_pointer(type_simple(ST_VOID)), 1, 1);
		for (int i = 0; i < init->brace.size; i++) {
			int child_offset = calculate_offset(type, i);
			struct type *child_type = type_select(type, i);
			IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.size_type, child_offset), child_offset_var);
			IR_PUSH_BINARY_OPERATOR(IBO_ADD, offset, child_offset_var, child_offset_var);
			int bit_offset = -1, bit_size = -1;

			if (type->type == TY_STRUCT) {
				struct field f = type->struct_data->fields[i];
				bit_offset = f.bit_offset;
				bit_size = f.bitfield;
			}

			ir_init_var_recursive(init->brace.entries + i, child_type, child_offset_var,
								  bit_offset, bit_size);
		}
	} break;

	case INIT_EXPRESSION:
		if (bit_size == -1) {
			IR_PUSH_STORE(expression_to_ir(init->expr), offset);
		} else {
			var_id value = expression_to_ir(init->expr);
			var_id prev = new_variable_sz(get_variable_size(value), 1, 1);
			IR_PUSH_LOAD(prev, offset);

			ir_set_bits(prev, prev, value, bit_offset, bit_size);

			IR_PUSH_STORE(prev, offset);
		}
		break;

	case INIT_STRING: {
		var_id offset_var = new_variable(type_pointer(type_simple(ST_VOID)), 1, 1);
		var_id char_var = new_variable_sz(1, 1, 1);
		for (int j = 0; j < init->string.len; j++) {
			IR_PUSH_CONSTANT(constant_simple_unsigned(ST_CHAR, init->string.str[j]), char_var);
			IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.size_type, j), offset_var);
			IR_PUSH_BINARY_OPERATOR(IBO_ADD, offset, offset_var, offset_var);

			IR_PUSH_STORE(char_var, offset_var);
		}
	} break;
		
	case INIT_EMPTY: break;
	}
}

void ir_init_var(struct initializer *init, struct type *type, var_id result) {
	IR_PUSH_SET_ZERO(result);
	var_id base_address = new_variable(type_pointer(type_simple(ST_VOID)), 1, 1);
	IR_PUSH_ADDRESS_OF(base_address, result);

	ir_init_var_recursive(init, type, base_address, -1, -1);
}

void ir_call(var_id result, var_id func_var, struct type *function_type, int n_args, struct type **argument_types, var_id *args) {
	abi_ir_function_call(result, func_var, function_type, n_args, argument_types, args);
}

void ir_new_function(struct type *function_type, var_id *args, const char *name, int is_global) {
	abi_ir_function_new(function_type, args, name, is_global);
}
