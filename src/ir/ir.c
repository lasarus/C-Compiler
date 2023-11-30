#include "ir.h"
#include "arch/x64.h"
#include "debug.h"
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

static struct instruction *ir_push(struct instruction instruction) {
	struct block *block = get_current_block();

	ADD_ELEMENT(block->size, block->cap, block->instructions) = ALLOC(instruction);

	return block->instructions[block->size - 1];
}

static struct instruction *ir_push3(int type, struct instruction *op2, struct instruction *op3) {
	struct block *block = get_current_block();

	ADD_ELEMENT(block->size, block->cap, block->instructions) = ALLOC((struct instruction) {
		.type = type,
		.arguments = { op2, op3 }
		});

	return block->instructions[block->size - 1];
}

static struct instruction *ir_push2(int type, struct instruction *op2) {
	return ir_push3(type, op2, NULL);
}

void ir_block_start(block_id id) {
	struct function *func = get_current_function();

	ADD_ELEMENT(func->size, func->cap, func->blocks) = id;
}

void ir_if_selection(struct instruction *condition, block_id block_true, block_id block_false) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_IF;
	block->exit.if_.condition = condition;
	block->exit.if_.block_true = block_true;
	block->exit.if_.block_false = block_false;
}

void ir_switch_selection(struct instruction *condition, struct case_labels labels) {
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

struct instruction *ir_get_offset(struct instruction *base_address, int offset) {
	struct instruction *offset_var = ir_constant(constant_simple_unsigned(abi_info.pointer_type, offset));
	return ir_add(base_address, offset_var);
}

struct instruction *ir_set_bits(struct instruction *field, struct instruction *value, int offset, int length) {
	uint64_t mask = gen_mask(64 - offset - length, offset);

	struct instruction *value_large = ir_cast_int(value, 8, 0);
	struct instruction *field_large = ir_cast_int(field, 8, 0);

	struct instruction *shift_var = ir_constant(constant_simple_unsigned(abi_info.size_type, offset));
	struct instruction *mask_var = ir_constant(constant_simple_unsigned(abi_info.size_type, mask));

	struct instruction *result_large = ir_binary_and(mask_var, field_large);

	struct instruction *mask_inverted = ir_binary_not(mask_var);
	value_large = ir_left_shift(value_large, shift_var);
	mask_inverted = ir_binary_and(mask_inverted, value_large);

	result_large = ir_binary_or(mask_inverted, result_large);

	return ir_cast_int(result_large, field->size, 0);
}

struct instruction *ir_get_bits(struct instruction *field, int offset, int length, int sign_extend) {
	struct instruction *field_large = ir_cast_int(field, 8, 0);
	struct instruction *lshift_var = ir_constant(constant_simple_unsigned(abi_info.size_type, 64 - offset - length));
	struct instruction *rshift_var = ir_constant(constant_simple_unsigned(abi_info.size_type, 64 - length));

	field_large = ir_left_shift(field_large, lshift_var);
	field_large = ir_right_shift(field_large, rshift_var, sign_extend);

	return ir_cast_int(field_large, field->size, 0);
}

static void ir_init_var_recursive(struct initializer *init, struct type *type, struct instruction *offset,
								  int bit_offset, int bit_size) {
	switch (init->type) {
	case INIT_BRACE: {
		for (int i = 0; i < init->brace.size; i++) {
			int child_offset = calculate_offset(type, i);
			struct type *child_type = type_select(type, i);
			struct instruction *child_offset_var = ir_add(offset,
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
			struct instruction *value = expression_to_ir(init->expr);
			struct instruction *prev = ir_load(offset, value->size);
			struct instruction *new = ir_set_bits(prev, value, bit_offset, bit_size);

			ir_store(offset, new);
		}
		break;

	case INIT_STRING: {
		for (int j = 0; j < init->string.len; j++) {
			struct instruction *char_var = ir_constant(constant_simple_unsigned(ST_CHAR, init->string.str[j]));
			struct instruction *offset_var = ir_constant(constant_simple_unsigned(abi_info.size_type, j));

			offset_var = ir_add(offset_var, offset);

			ir_store(offset_var, char_var);
		}
	} break;
		
	case INIT_EMPTY: break;
	}
}

void ir_init_ptr(struct initializer *init, struct type *type, struct instruction *ptr) {
	ir_set_zero_ptr(ptr, calculate_size(type));

	ir_init_var_recursive(init, type, ptr, -1, -1);
}

static void register_usage(struct block *block, struct instruction *var) {
	if (!var)
		return;

	if (var->first_block == -1) {
		var->first_block = block->id;
	} else if (var->first_block != block->id) {
		var->spans_block = 1;
	}

	var->used = 1;
}

void ir_calculate_block_local_variables(void) {
	for (unsigned i = 0; i < ir.size; i++) {
		struct function *f = &ir.functions[i];

		for (unsigned j = 0; j < f->size; j++) {
			struct block *b = get_block(f->blocks[j]);

			for (unsigned k = 0; k < b->size; k++) {
				struct instruction *ins = b->instructions[k];

				register_usage(b, ins);
				register_usage(b, ins->arguments[0]);
				register_usage(b, ins->arguments[1]);
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
		}
	}
}

#define IR_PUSH(...) ir_push((struct instruction) { __VA_ARGS__ })

struct instruction *new_variable(struct instruction *instruction, int size) {
	if (size > 8 || size == 0)
		ICE("Invalid register size: %d", size);

	static int counter = 0;

	instruction->index = ++counter;
	instruction->size = size;
	instruction->first_block = -1;
	instruction->spans_block = 0;

	return instruction;
}

void ir_va_start(struct instruction *address) {
	ir_push3(IR_VA_START, address, NULL);
}

void ir_va_arg(struct instruction *array, struct instruction *result_address, struct type *type) {
	IR_PUSH(.type = IR_VA_ARG, .arguments = {result_address, array}, .va_arg_ = {type});
}

void ir_store(struct instruction *address, struct instruction *value) {
	ir_push3(IR_STORE, address, value);
}

struct instruction *ir_load(struct instruction *address, int size) {
	return new_variable(ir_push2(IR_LOAD, address), size);
}

struct instruction *ir_bool_cast(struct instruction *operand) {
	return new_variable(ir_push2(IR_BOOL_CAST, operand), calculate_size(type_simple(ST_BOOL)));
}

struct instruction *ir_cast_int(struct instruction *operand, int target_size, int sign_extend) {
	return new_variable(ir_push2(sign_extend ? IR_INT_CAST_SIGN : IR_INT_CAST_ZERO, operand), target_size);
}

struct instruction *ir_cast_float(struct instruction *operand, int target_size) {
	return new_variable(ir_push2(IR_FLOAT_CAST, operand), target_size);
}

struct instruction *ir_cast_int_to_float(struct instruction *operand, int target_size, int is_signed) {
	return new_variable(ir_push2(is_signed ? IR_INT_FLOAT_CAST : IR_UINT_FLOAT_CAST, operand), target_size);
}

struct instruction *ir_cast_float_to_int(struct instruction *operand, int target_size) {
	return new_variable(ir_push2(IR_FLOAT_INT_CAST, operand), target_size);
}

struct instruction *ir_binary_not(struct instruction *operand) {
	return new_variable(ir_push2(IR_BINARY_NOT, operand), operand->size);
}

struct instruction *ir_negate_int(struct instruction *operand) {
	return new_variable(ir_push2(IR_NEGATE_INT, operand), operand->size);
}

struct instruction *ir_negate_float(struct instruction *operand) {
	return new_variable(ir_push2(IR_NEGATE_FLOAT, operand), operand->size);
}

struct instruction *ir_constant(struct constant constant) {
	int size = calculate_size(constant.data_type);
	if (constant.type == CONSTANT_LABEL_POINTER)
		size = 8;
	return new_variable(IR_PUSH(.type = IR_CONSTANT, .constant = {constant}), size);
}

void ir_write_constant_to_address(struct constant constant, struct instruction *address) {
	IR_PUSH(.type = IR_CONSTANT_ADDRESS, .arguments={address}, .constant = {constant});
}

struct instruction *ir_binary_and(struct instruction *lhs, struct instruction *rhs) {
	return new_variable(ir_push3(IR_BAND, lhs, rhs), lhs->size);
}

struct instruction *ir_left_shift(struct instruction *lhs, struct instruction *rhs) {
	return new_variable(ir_push3(IR_LSHIFT, lhs, rhs), lhs->size);
}

struct instruction *ir_right_shift(struct instruction *lhs, struct instruction *rhs, int arithmetic) {
	return new_variable(ir_push3(arithmetic ? IR_IRSHIFT : IR_RSHIFT, lhs, rhs), lhs->size);
}

struct instruction *ir_binary_or(struct instruction *lhs, struct instruction *rhs) {
	return new_variable(ir_push3(IR_BOR, lhs, rhs), lhs->size);
}

struct instruction *ir_add(struct instruction *lhs, struct instruction *rhs) {
	return new_variable(ir_push3(IR_ADD, lhs, rhs), lhs->size);
}

struct instruction *ir_sub(struct instruction *lhs, struct instruction *rhs) {
	return new_variable(ir_push3(IR_SUB, lhs, rhs), lhs->size);
}

struct instruction *ir_mul(struct instruction *lhs, struct instruction *rhs) {
	return new_variable(ir_push3(IR_MUL, lhs, rhs), lhs->size);
}

struct instruction *ir_imul(struct instruction *lhs, struct instruction *rhs) {
	return new_variable(ir_push3(IR_IMUL, lhs, rhs), lhs->size);
}

struct instruction *ir_div(struct instruction *lhs, struct instruction *rhs) {
	return new_variable(ir_push3(IR_DIV, lhs, rhs), lhs->size);
}

struct instruction *ir_idiv(struct instruction *lhs, struct instruction *rhs) {
	return new_variable(ir_push3(IR_IDIV, lhs, rhs), lhs->size);
}

struct instruction *ir_binary_op(int type, struct instruction *lhs, struct instruction *rhs) {
	return new_variable(ir_push3(type, lhs, rhs), lhs->size);
}

void ir_call(struct instruction *callee, int non_clobbered_register) {
	IR_PUSH(.type = IR_CALL, .arguments = {callee}, .call = {non_clobbered_register});
}

struct instruction *ir_vla_alloc(struct instruction *length) {
	return new_variable(IR_PUSH(.type = IR_VLA_ALLOC, .arguments = {length}, .vla_alloc = {0}), 8);
}

void ir_set_reg(struct instruction *variable, int register_index, int is_sse) {
	IR_PUSH(.type = IR_SET_REG, .arguments = {variable}, .set_reg = {register_index, is_sse});
}

struct instruction *ir_get_reg(int size, int register_index, int is_sse) {
	return new_variable(IR_PUSH(.type = IR_GET_REG, .get_reg = {register_index, is_sse}), size);
}

struct instruction *ir_allocate(int size) {
	return new_variable(IR_PUSH(.type = IR_ALLOC, .alloc = {size, -1, 0}), 8);
}

struct instruction *ir_allocate_preamble(int size) {
	return new_variable(IR_PUSH(.type = IR_ALLOC, .alloc = {size, -1, 1}), 8);
}

void ir_modify_stack_pointer(int change) {
	IR_PUSH(.type = IR_MODIFY_STACK_POINTER, .modify_stack_pointer = {change});
}

void ir_store_stack_relative(struct instruction *variable, int offset) {
	IR_PUSH(.type = IR_STORE_STACK_RELATIVE, .arguments = {variable}, .store_stack_relative = {offset});
}

void ir_store_stack_relative_address(struct instruction *variable, int offset, int size) {
	IR_PUSH(.type = IR_STORE_STACK_RELATIVE_ADDRESS, .arguments = {variable}, .store_stack_relative_address = {offset, size});
}

struct instruction *ir_load_base_relative(int offset, int size) {
	return new_variable(IR_PUSH(.type = IR_LOAD_BASE_RELATIVE, .load_base_relative = {offset}), size);
}

void ir_load_base_relative_address(struct instruction *address, int offset, int size) {
	IR_PUSH(.type = IR_LOAD_BASE_RELATIVE_ADDRESS, .arguments = {address}, .load_base_relative_address = {offset, size});
}

void ir_set_zero_ptr(struct instruction *address, int size) {
	IR_PUSH(.type = IR_SET_ZERO_PTR, .arguments = {address}, .set_zero_ptr = {size});
}

struct instruction *ir_load_part_address(struct instruction *address, int offset, int size) {
	return new_variable(IR_PUSH(.type = IR_LOAD_PART_ADDRESS, .arguments = {address}, .load_part = {offset}), size);
}

void ir_store_part_address(struct instruction *address, struct instruction *value, int offset) {
	IR_PUSH(.type = IR_STORE_PART_ADDRESS, .arguments = {address, value}, .store_part = {offset});
}

void ir_copy_memory(struct instruction *destination, struct instruction *source, int size) {
	IR_PUSH(.type = IR_COPY_MEMORY, .arguments = {destination, source}, .copy_memory = {size});
}

struct instruction *ir_phi(struct instruction *var_a, struct instruction *var_b, block_id block_a, block_id block_b) {
	return new_variable(IR_PUSH(.type = IR_PHI, .arguments = {var_a, var_b}, .phi = {block_a, block_b}), var_a->size);
}
