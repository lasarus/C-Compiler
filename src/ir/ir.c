#include "ir.h"
#include "arch/x64.h"
#include "debug.h"
#include "ir/variables.h"
#include "types.h"

#include <common.h>
#include <parser/declaration.h>
#include <parser/expression_to_ir.h>
#include <arch/calling.h>
#include <codegen/registers.h>
#include <abi/abi.h>

#include <assert.h>

static size_t block_size, block_cap;
static struct block *blocks;

struct ir ir;

void ir_reset(void) {
	block_size = block_cap = 0;
	free(blocks);
	blocks = NULL;

	ir = (struct ir) { 0 };

	variables_reset();
}

block_id new_block(void) {
	int id = (int)block_size;
	ADD_ELEMENT(block_size, block_cap, blocks) = (struct block) {
		.id = id,
		.label = register_label(),
		.exit.type = BLOCK_EXIT_NONE,

		.stack_counter = 0,
	};

	return id;
}

struct block *get_block(block_id id) {
	return blocks + id;
}

struct function *get_current_function(void) {
	return &ir.functions[ir.size - 1];
}

struct block *get_current_block(void) {
	struct function *func = get_current_function();
	return get_block(func->blocks[func->size - 1]);
}

void ir_push(struct instruction instruction) {
	struct block *block = get_current_block();

	ADD_ELEMENT(block->size, block->cap, block->instructions) = instruction;
}

void ir_push3(int type, var_id op1, var_id op2, var_id op3) {
	struct block *block = get_current_block();

	ADD_ELEMENT(block->size, block->cap, block->instructions) = (struct instruction) {
		.type = type,
		.operands = { op1, op2, op3 }
	};
}

static void ir_push2(int type, var_id op1, var_id op2) {
	ir_push3(type, op1, op2, VOID_VAR);
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

void ir_return(void) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_RETURN;
}

var_id ir_phi(var_id var_a, var_id var_b, block_id block_a, block_id block_b) {
	struct block *block = get_current_block();
	var_id result = new_variable(get_variable_size(var_a));

	ADD_ELEMENT(block->phi_size, block->phi_cap, block->phi_nodes) = (struct phi_node) {
		.block_a = block_a,
		.block_b = block_b,
		.var_a = var_a,
		.var_b = var_b,
		.result = result,
	};

	return result;
}

var_id ir_get_offset(var_id base_address, int offset) {
	var_id offset_var = ir_constant(constant_simple_unsigned(abi_info.pointer_type, offset));
	return ir_add(base_address, offset_var);
}

var_id ir_set_bits(var_id field, var_id value, int offset, int length) {
	uint64_t mask = gen_mask(64 - offset - length, offset);

	var_id value_large = ir_cast_int(value, 8, 0);
	var_id field_large = ir_cast_int(field, 8, 0);

	var_id shift_var = ir_constant(constant_simple_unsigned(abi_info.size_type, offset));
	var_id mask_var = ir_constant(constant_simple_unsigned(abi_info.size_type, mask));

	var_id result_large = ir_binary_and(mask_var, field_large);

	var_id mask_inverted = ir_binary_not(mask_var);
	value_large = ir_left_shift(value_large, shift_var);
	mask_inverted = ir_binary_and(mask_inverted, value_large);

	result_large = ir_binary_or(mask_inverted, result_large);

	return ir_cast_int(result_large, get_variable_size(field), 0);
}

var_id ir_get_bits(var_id field, int offset, int length, int sign_extend) {
	var_id field_large = ir_cast_int(field, 8, 0);
	var_id lshift_var = ir_constant(constant_simple_unsigned(abi_info.size_type, 64 - offset - length));
	var_id rshift_var = ir_constant(constant_simple_unsigned(abi_info.size_type, 64 - length));

	field_large = ir_left_shift(field_large, lshift_var);
	field_large = ir_right_shift(field_large, rshift_var, sign_extend);

	return ir_cast_int(field_large, get_variable_size(field), 0);
}

static void ir_init_var_recursive(struct initializer *init, struct type *type, var_id offset,
								  int bit_offset, int bit_size) {
	switch (init->type) {
	case INIT_BRACE: {
		for (int i = 0; i < init->brace.size; i++) {
			int child_offset = calculate_offset(type, i);
			struct type *child_type = type_select(type, i);
			var_id child_offset_var = ir_add(offset,
											 ir_constant(constant_simple_unsigned(abi_info.size_type, child_offset)));
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
			expression_to_address(init->expr, offset);
		} else {
			var_id value = expression_to_ir(init->expr);

			var_id prev = ir_load(offset, get_variable_size(value));

			var_id new = ir_set_bits(prev, value, bit_offset, bit_size);

			ir_store(offset, new);
		}
		break;

	case INIT_STRING: {
		for (int j = 0; j < init->string.len; j++) {
			var_id char_var = ir_constant(constant_simple_unsigned(ST_CHAR, init->string.str[j]));
			var_id offset_var = ir_constant(constant_simple_unsigned(abi_info.size_type, j));

			offset_var = ir_add(offset_var, offset);

			ir_store(offset_var, char_var);
		}
	} break;
		
	case INIT_EMPTY: break;
	}
}

void ir_init_ptr(struct initializer *init, struct type *type, var_id ptr) {
	ir_set_zero_ptr(ptr, calculate_size(type));

	ir_init_var_recursive(init, type, ptr, -1, -1);
}

static void register_usage(struct block *block, var_id var) {
	if (var == 0)
		return;

	struct variable_data *data = var_get_data(var);

	if (data->first_block == -1) {
		data->first_block = block->id;
	} else if (data->first_block != block->id) {
		data->spans_block = 1;
	}

	data->used = 1;
}

void ir_calculate_block_local_variables(void) {
	for (int i = 0; i < ir.size; i++) {
		struct function *f = &ir.functions[i];

		for (int j = 0; j < f->size; j++) {
			struct block *b = get_block(f->blocks[j]);

			for (int k = 0; k < b->size; k++) {
				struct instruction *ins = &b->instructions[k];

				for (int l = 0; l < 3; l++) {
					var_id usage = ins->operands[l];
					register_usage(b, usage);
				}
			}

			switch (b->exit.type) {
			case BLOCK_EXIT_SWITCH:
				register_usage(b, b->exit.switch_.condition);
				break;
			case BLOCK_EXIT_IF:
				register_usage(b, b->exit.if_.condition);
				break;
			case BLOCK_EXIT_RETURN:
			case BLOCK_EXIT_JUMP:
			case BLOCK_EXIT_RETURN_ZERO:
			case BLOCK_EXIT_NONE:
				break;
			}

			for (unsigned k = 0; k < b->phi_size; k++) {
				struct phi_node *phi = &b->phi_nodes[k];
				register_usage(b, phi->var_a);
				register_usage(b, phi->var_b);
				register_usage(b, phi->result);
			}
		}
	}
}

#define IR_PUSH(...) do { ir_push((struct instruction) { __VA_ARGS__ }); } while(0)

void ir_va_start(var_id address) {
	ir_push3(IR_VA_START, address, VOID_VAR, VOID_VAR);
}

void ir_va_arg(var_id array, var_id result_address, struct type *type) {
#define IR_PUSH_VA_ARG(ARRAY, RESULT, TYPE) IR_PUSH(.type = IR_VA_ARG, .operands = {(RESULT), (ARRAY)}, .va_arg_ = {(TYPE)})
	IR_PUSH_VA_ARG(array, result_address, type);
}

void ir_store(var_id address, var_id value) {
	ir_push2(IR_STORE, value, address);
}

var_id ir_load(var_id address, int size) {
	var_id var = new_variable(size);
	ir_push2(IR_LOAD, var, address);
	return var;
}

var_id ir_copy(var_id var) {
	var_id ret = new_variable(get_variable_size(var));
	ir_push2(IR_COPY, ret, var);
	return ret;
}

var_id ir_bool_cast(var_id operand) {
	var_id res = new_variable(calculate_size(type_simple(ST_BOOL)));
	ir_push2(IR_BOOL_CAST, res, operand);
	return res;
}

var_id ir_cast_int(var_id operand, int target_size, int sign_extend) {
	var_id res = new_variable(target_size);
	ir_push2(sign_extend ? IR_INT_CAST_SIGN : IR_INT_CAST_ZERO, res, operand);
	return res;
}

var_id ir_cast_float(var_id operand, int target_size) {
	var_id res = new_variable(target_size);
	ir_push2(IR_FLOAT_CAST, res, operand);
	return res;
}

var_id ir_cast_int_to_float(var_id operand, int target_size, int is_signed) {
	var_id res = new_variable(target_size);
	ir_push2(is_signed ? IR_INT_FLOAT_CAST : IR_UINT_FLOAT_CAST, res, operand);
	return res;
}

var_id ir_cast_float_to_int(var_id operand, int target_size) {
	var_id res = new_variable(target_size);
	ir_push2(IR_FLOAT_INT_CAST, res, operand);
	return res;
}

var_id ir_binary_not(var_id operand) {
	var_id ret = new_variable(get_variable_size(operand));
	ir_push2(IR_BINARY_NOT, ret, operand);
	return ret;
}

var_id ir_negate_int(var_id operand) {
	var_id ret = new_variable(get_variable_size(operand));
	ir_push2(IR_NEGATE_INT, ret, operand);
	return ret;
}

var_id ir_negate_float(var_id operand) {
	var_id ret = new_variable(get_variable_size(operand));
	ir_push2(IR_NEGATE_FLOAT, ret, operand);
	return ret;
}

var_id ir_constant(struct constant constant) {
	int size = calculate_size(constant.data_type);
	if (constant.type == CONSTANT_LABEL_POINTER)
		size = 8;
	var_id var = new_variable(size);
#define IR_PUSH_CONSTANT(CONSTANT, RESULT) IR_PUSH(.type = IR_CONSTANT, .operands={(RESULT)}, .constant = {(CONSTANT)})
	IR_PUSH_CONSTANT(constant, var);
	return var;
}

void ir_write_constant_to_address(struct constant constant, var_id address) {
#define IR_PUSH_CONSTANT_ADDRESS(CONSTANT, ADDRESS) IR_PUSH(.type = IR_CONSTANT_ADDRESS, .operands={(ADDRESS)}, .constant = {(CONSTANT)})
	IR_PUSH_CONSTANT_ADDRESS(constant, address);
}

var_id ir_binary_and(var_id lhs, var_id rhs) {
	var_id var = new_variable(get_variable_size(lhs));
	ir_push3(IR_BAND, var, lhs, rhs);
	return var;
}

var_id ir_left_shift(var_id lhs, var_id rhs) {
	var_id var = new_variable(get_variable_size(lhs));
	ir_push3(IR_LSHIFT, var, lhs, rhs);
	return var;
}

var_id ir_right_shift(var_id lhs, var_id rhs, int arithmetic) {
	var_id var = new_variable(get_variable_size(lhs));
	ir_push3(arithmetic ? IR_IRSHIFT : IR_RSHIFT, var, lhs, rhs);
	return var;
}

var_id ir_binary_or(var_id lhs, var_id rhs) {
	var_id var = new_variable(get_variable_size(lhs));
	ir_push3(IR_BOR, var, lhs, rhs);
	return var;
}

var_id ir_add(var_id lhs, var_id rhs) {
	var_id var = new_variable(get_variable_size(lhs));
	ir_push3(IR_ADD, var, lhs, rhs);
	return var;
}

var_id ir_sub(var_id lhs, var_id rhs) {
	var_id var = new_variable(get_variable_size(lhs));
	ir_push3(IR_SUB, var, lhs, rhs);
	return var;
}

var_id ir_mul(var_id lhs, var_id rhs) {
	var_id var = new_variable(get_variable_size(lhs));
	ir_push3(IR_MUL, var, lhs, rhs);
	return var;
}

var_id ir_imul(var_id lhs, var_id rhs) {
	var_id var = new_variable(get_variable_size(lhs));
	ir_push3(IR_IMUL, var, lhs, rhs);
	return var;
}

var_id ir_div(var_id lhs, var_id rhs) {
	var_id var = new_variable(get_variable_size(lhs));
	ir_push3(IR_DIV, var, lhs, rhs);
	return var;
}

var_id ir_idiv(var_id lhs, var_id rhs) {
	var_id var = new_variable(get_variable_size(lhs));
	ir_push3(IR_IDIV, var, lhs, rhs);
	return var;
}

var_id ir_binary_op(int type, var_id lhs, var_id rhs) {
	var_id var = new_variable(get_variable_size(lhs));
	ir_push3(type, var, lhs, rhs);
	return var;
}

void ir_call(var_id callee, int non_clobbered_register) {
#define IR_PUSH_CALL(VARIABLE, NON_CLOBBERED_REGISTER) IR_PUSH(.type = IR_CALL, .operands = {(VARIABLE)}, .call = {(NON_CLOBBERED_REGISTER)})
	IR_PUSH_CALL(callee, non_clobbered_register);
}

var_id ir_vla_alloc(var_id length) {
#define IR_PUSH_VLA_ALLOC(RESULT, LENGTH) IR_PUSH(.type = IR_VLA_ALLOC, .operands = {(RESULT), (LENGTH)}, .vla_alloc = {0})
	var_id result = new_ptr();
	IR_PUSH_VLA_ALLOC(result, length);
	return result;
}

void ir_set_reg(var_id variable, int register_index, int is_sse) {
#define IR_PUSH_SET_REG(VARIABLE, REGISTER_INDEX, IS_SSE) IR_PUSH(.type = IR_SET_REG, .operands = {(VARIABLE)}, .set_reg = {(REGISTER_INDEX), (IS_SSE)})
	IR_PUSH_SET_REG(variable, register_index, is_sse);
}

var_id ir_get_reg(int size, int register_index, int is_sse) {
#define IR_PUSH_GET_REG(RESULT, REGISTER_INDEX, IS_SSE) IR_PUSH(.type = IR_GET_REG, .operands = {(RESULT)}, .get_reg = {(REGISTER_INDEX), (IS_SSE)})
	var_id reg = new_variable(size);
	IR_PUSH_GET_REG(reg, register_index, is_sse);
	return reg;
}

#define IR_PUSH_ALLOC(RESULT, SIZE) IR_PUSH(.type = IR_ALLOC, .operands = {(RESULT)}, .alloc = {(SIZE), -1, 0})
#define IR_PUSH_ALLOC_PREAMBLE(RESULT, SIZE) IR_PUSH(.type = IR_ALLOC, .operands = {(RESULT)}, .alloc = {(SIZE), -1, 1})

var_id ir_allocate(int size) {
	var_id res = new_ptr();

	IR_PUSH_ALLOC(res, size);

	return res;
}

var_id ir_allocate_preamble(int size) {
	var_id res = new_ptr();

	IR_PUSH_ALLOC_PREAMBLE(res, size);

	return res;
}

void ir_modify_stack_pointer(int change) {
#define IR_PUSH_MODIFY_STACK_POINTER(CHANGE) IR_PUSH(.type = IR_MODIFY_STACK_POINTER, .modify_stack_pointer = {(CHANGE)})
	IR_PUSH_MODIFY_STACK_POINTER(change);
}

void ir_store_stack_relative(var_id variable, int offset) {
#define IR_PUSH_STORE_STACK_RELATIVE(OFFSET, VARIABLE) IR_PUSH(.type = IR_STORE_STACK_RELATIVE, .operands = {(VARIABLE)}, .store_stack_relative = {(OFFSET)})
	IR_PUSH_STORE_STACK_RELATIVE(offset, variable);
}

void ir_store_stack_relative_address(var_id variable, int offset, int size) {
#define IR_PUSH_STORE_STACK_RELATIVE_ADDRESS(VARIABLE, OFFSET, SIZE) IR_PUSH(.type = IR_STORE_STACK_RELATIVE_ADDRESS, .operands = {(VARIABLE)}, .store_stack_relative_address = {(OFFSET), (SIZE)})
	IR_PUSH_STORE_STACK_RELATIVE_ADDRESS(variable, offset, size);
}

var_id ir_load_base_relative(int offset, int size) {
#define IR_PUSH_LOAD_BASE_RELATIVE(RESULT, OFFSET) IR_PUSH(.type = IR_LOAD_BASE_RELATIVE, .operands = {(RESULT)}, .load_base_relative = {(OFFSET)})
	var_id res = new_variable(size);
	IR_PUSH_LOAD_BASE_RELATIVE(res, offset);
	return res;
}

void ir_load_base_relative_address(var_id address, int offset, int size) {
#define IR_PUSH_LOAD_BASE_RELATIVE_ADDRESS(RESULT, OFFSET, SIZE) IR_PUSH(.type = IR_LOAD_BASE_RELATIVE_ADDRESS, .operands = {(RESULT)}, .load_base_relative_address = {(OFFSET), (SIZE)})
	IR_PUSH_LOAD_BASE_RELATIVE_ADDRESS(address, offset, size);
}

void ir_set_zero_ptr(var_id address, int size) {
#define IR_PUSH_SET_ZERO_PTR(RESULT, SIZE) IR_PUSH(.type = IR_SET_ZERO_PTR, .operands = {(RESULT)}, .set_zero_ptr = {(SIZE)})
	IR_PUSH_SET_ZERO_PTR(address, size);
}

var_id ir_load_part_address(var_id address, int offset, int size) {
#define IR_PUSH_LOAD_PART_ADDRESS(RESULT, VAR, OFFSET) IR_PUSH(.type = IR_LOAD_PART_ADDRESS, .operands = {(RESULT), (VAR)}, .load_part = {(OFFSET)})
	var_id res = new_variable(size);
	IR_PUSH_LOAD_PART_ADDRESS(res, address, offset);
	return res;
}

void ir_store_part_address(var_id address, var_id value, int offset) {
#define IR_PUSH_STORE_PART_ADDRESS(RESULT, VAR, OFFSET) IR_PUSH(.type = IR_STORE_PART_ADDRESS, .operands = {(RESULT), (VAR)}, .store_part = {(OFFSET)})
	IR_PUSH_STORE_PART_ADDRESS(address, value, offset);
}

void ir_copy_memory(var_id destination, var_id source, int size) {
#define IR_PUSH_COPY_MEMORY(DESTINATION, SOURCE, SIZE) IR_PUSH(.type = IR_COPY_MEMORY, .operands = {(DESTINATION), (SOURCE)}, .copy_memory = {(SIZE)})
	IR_PUSH_COPY_MEMORY(destination, source, size);
}
