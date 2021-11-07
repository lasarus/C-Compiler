#include "ir.h"

#include <common.h>
#include <parser/declaration.h>

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

void ir_new_function(struct type *signature, var_id *arguments, const char *name, int is_global) {
	ADD_ELEMENT(ir.size, ir.cap, ir.functions) = (struct function) {
		.signature = signature,
		.named_arguments = arguments,
		.name = name,
		.is_global = is_global
	};
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
	IR_PUSH_CONSTANT(((struct constant) {
				.type = CONSTANT_TYPE,
				.data_type = type_simple(ST_ULLONG),
				.ullong_d = offset
			}), offset_var);
	IR_PUSH_BINARY_OPERATOR(IBO_ADD, base_address, offset_var, member_address);
}

void ir_set_bits(var_id result, var_id field, var_id value, int offset, int length) {
	// TODO: Unaligned loads? Out of bounds? Different sizes of value?
	uint64_t mask = gen_mask(64 - offset - length, offset);
	var_id mask_var = new_variable_sz(8, 1, 1);
	var_id shift_var = new_variable_sz(8, 1, 1);

	var_id value_large = new_variable_sz(8, 1, 1);
	var_id field_large = new_variable_sz(8, 1, 1);
	var_id result_large = new_variable_sz(8, 1, 1);

	IR_PUSH_COPY(value_large, value);
	IR_PUSH_COPY(field_large, field);

	IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
				.data_type = type_simple(ST_UCHAR), .ullong_d = offset }),
				shift_var);
	IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
				.data_type = type_simple(ST_ULLONG), .ullong_d = mask }),
				mask_var);
	IR_PUSH_BINARY_OPERATOR(IBO_BAND, mask_var, field_large, result_large);
	IR_PUSH_UNARY_OPERATOR(UOP_BNOT, OT_ULLONG, mask_var, mask_var);
	IR_PUSH_BINARY_OPERATOR(IBO_LSHIFT, value_large, shift_var, value_large);
	IR_PUSH_BINARY_OPERATOR(IBO_BAND, mask_var, value_large, mask_var);
	IR_PUSH_BINARY_OPERATOR(IBO_BOR, mask_var, result_large, result_large);

	IR_PUSH_COPY(result, result_large);
}

void ir_get_bits(var_id result, var_id field, int offset, int length, int sign_extend) {
	IR_PUSH_GET_BITS(result, field, offset, length, sign_extend);
}

void ir_init_var(struct initializer *init, var_id result) {
	IR_PUSH_SET_ZERO(result);
	var_id base_address = new_variable(type_pointer(type_simple(ST_VOID)), 1, 1);
	IR_PUSH_ADDRESS_OF(base_address, result);
	var_id member_address = new_variable(type_pointer(type_simple(ST_VOID)), 1, 1);
	var_id offset_var = new_variable(type_pointer(type_simple(ST_VOID)), 1, 1);

	for (int i = 0; i < init->size; i++) {
		struct init_pair *pair = init->pairs + i;
		switch (pair->type) {
		case IP_EXPRESSION: {
			ir_get_offset(member_address, base_address, offset_var, pair->offset);
			if (init->pairs[i].bit_offset) {
				var_id value = expression_to_ir(pair->u.expr);
				var_id prev = new_variable_sz(get_variable_size(value), 1, 1);
				IR_PUSH_LOAD(prev, member_address);

				ir_set_bits(prev, prev, value, init->pairs[i].bit_offset, init->pairs[i].bit_size);

				IR_PUSH_STORE(prev, member_address);
			} else {
				IR_PUSH_STORE(expression_to_ir(pair->u.expr), member_address);
			}
		} break;

		case IP_STRING: {
			var_id char_val = new_variable_sz(1, 1, 1);
			struct string_view str = pair->u.str;
			for (int j = 0; j < str.len; j++) { 
				ir_get_offset(member_address, base_address, offset_var, pair->offset + j);
				IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
							.data_type = type_simple(ST_CHAR),
							.char_d = str.str[j]}), char_val);
				IR_PUSH_STORE(char_val, member_address);
			}
		} break;
		}
	}
}
