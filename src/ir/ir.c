#include "ir.h"

#include <common.h>
#include <abi/abi.h>

#include <assert.h>

struct function *first_function;

static struct function *current_function;
static struct node *current_block, *last_block;
static struct node *current_instruction;

void ir_reset(void) {
	current_function = NULL;
	first_function = NULL;
}

struct node *new_block(void) {
	struct function *func = get_current_function();

	struct node *block = ALLOC((struct node) {
			.type = IR_BLOCK,
			.block.label = register_label(),
		});

	if (!func->first)
		func->first = block;
	else
		last_block->next = block;

	last_block = block;

	return block;
}

struct function *new_function(void) {
	struct function *next = ALLOC((struct function) { 0 });

	if (current_function)
		current_function->next = next;
	else
		first_function = next;

	current_function = next;

	return next;
}

struct function *get_current_function(void) {
	return current_function;
}

struct node *get_current_block(void) {
	return current_block;
}

static struct node *ir_new(int type, int size) {
	struct node *next = ALLOC((struct node) { .type = type });
	struct node *block = get_current_block();

	static int counter = 0;
	next->index = ++counter;
	next->size = size;

	if (!block->child)
		block->child = next;
	else
		current_instruction->next = next;

	current_instruction = next;

	return next;
}

static struct node *ir_new2(int type, struct node *op2, struct node *op3, int size) {
	struct node *ins = ir_new(type, size);

	ins->arguments[0] = op2;
	ins->arguments[1] = op3;

	return ins;
}

static struct node *ir_new1(int type, struct node *op2, int size) {
	return ir_new2(type, op2, NULL, size);
}

void ir_block_start(struct node *block) {
	current_block = block;
}

void ir_if_selection(struct node *condition, struct node *block_true, struct node *block_false) {
	struct node *block = get_current_block();
	assert(block->block.exit.type == BLOCK_EXIT_NONE);

	block->block.exit.type = BLOCK_EXIT_IF;
	block->block.exit.if_.condition = condition;
	block->block.exit.if_.block_true = block_true;
	block->block.exit.if_.block_false = block_false;
}

void ir_goto(struct node *jump) {
	struct node *block = get_current_block();
	assert(block->block.exit.type == BLOCK_EXIT_NONE);

	block->block.exit.type = BLOCK_EXIT_JUMP;
	block->block.exit.jump = jump;
}

void ir_connect(struct node *start, struct node *end) {
	assert(start->block.exit.type == BLOCK_EXIT_NONE);

	start->block.exit.type = BLOCK_EXIT_JUMP;
	start->block.exit.jump = end;
}

void ir_return(void) {
	struct node *block = get_current_block();
	assert(block->block.exit.type == BLOCK_EXIT_NONE);

	block->block.exit.type = BLOCK_EXIT_RETURN;
}

struct node *ir_get_offset(struct node *base_address, int offset) {
	struct node *offset_var = ir_constant(constant_simple_unsigned(abi_info.pointer_type, offset));
	return ir_add(base_address, offset_var);
}

struct node *ir_set_bits(struct node *field, struct node *value, int offset, int length) {
	uint64_t mask = gen_mask(64 - offset - length, offset);

	struct node *value_large = ir_cast_int(value, 8, 0);
	struct node *field_large = ir_cast_int(field, 8, 0);

	struct node *shift_var = ir_constant(constant_simple_unsigned(abi_info.size_type, offset));
	struct node *mask_var = ir_constant(constant_simple_unsigned(abi_info.size_type, mask));

	struct node *result_large = ir_binary_and(mask_var, field_large);

	struct node *mask_inverted = ir_binary_not(mask_var);
	value_large = ir_left_shift(value_large, shift_var);
	mask_inverted = ir_binary_and(mask_inverted, value_large);

	result_large = ir_binary_or(mask_inverted, result_large);

	return ir_cast_int(result_large, field->size, 0);
}

struct node *ir_get_bits(struct node *field, int offset, int length, int sign_extend) {
	struct node *field_large = ir_cast_int(field, 8, 0);
	struct node *lshift_var = ir_constant(constant_simple_unsigned(abi_info.size_type, 64 - offset - length));
	struct node *rshift_var = ir_constant(constant_simple_unsigned(abi_info.size_type, 64 - length));

	field_large = ir_left_shift(field_large, lshift_var);
	field_large = ir_right_shift(field_large, rshift_var, sign_extend);

	return ir_cast_int(field_large, field->size, 0);
}

static void ir_init_var_recursive(struct initializer *init, struct type *type, struct node *offset,
								  int bit_offset, int bit_size) {
	switch (init->type) {
	case INIT_BRACE: {
		for (int i = 0; i < init->brace.size; i++) {
			int child_offset = calculate_offset(type, i);
			struct type *child_type = type_select(type, i);
			struct node *child_offset_var = ir_add(offset,
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
			struct node *value = expression_to_ir(init->expr);
			struct node *prev = ir_load(offset, value->size);
			struct node *new = ir_set_bits(prev, value, bit_offset, bit_size);

			ir_store(offset, new);
		}
		break;

	case INIT_STRING: {
		for (int j = 0; j < init->string.len; j++) {
			struct node *char_var = ir_constant(constant_simple_unsigned(ST_CHAR, init->string.str[j]));
			struct node *offset_var = ir_constant(constant_simple_unsigned(abi_info.size_type, j));

			offset_var = ir_add(offset_var, offset);

			ir_store(offset_var, char_var);
		}
	} break;
		
	case INIT_EMPTY: break;
	}
}

void ir_init_ptr(struct initializer *init, struct type *type, struct node *ptr) {
	ir_set_zero_ptr(ptr, calculate_size(type));

	ir_init_var_recursive(init, type, ptr, -1, -1);
}

static void register_usage(struct node *block, struct node *var) {
	if (!var)
		return;

	if (!var->first_block) {
		var->first_block = block;
	} else if (var->first_block != block) {
		var->spans_block = 1;
	}

	var->used = 1;
}

void ir_calculate_block_local_variables(void) {
	for (struct function *f = first_function; f; f = f->next) {
		for (struct node *b = f->first; b; b = b->next) {
			for (struct node *ins = b->child; ins; ins = ins->next) {
				register_usage(b, ins);
				register_usage(b, ins->arguments[0]);
				register_usage(b, ins->arguments[1]);
			}

			switch (b->block.exit.type) {
			case BLOCK_EXIT_IF:
				register_usage(b, b->block.exit.if_.condition);
				break;
			case BLOCK_EXIT_RETURN:
			case BLOCK_EXIT_JUMP:
			case BLOCK_EXIT_NONE:
				break;
			}
		}
	}
}

void ir_va_start(struct node *address) {
	ir_new2(IR_VA_START, address, NULL, 0);
}

void ir_va_arg(struct node *array, struct node *result_address, struct type *type) {
	struct node *ins = ir_new2(IR_VA_ARG, result_address, array, 0);
	ins->va_arg_.type = type;
}

void ir_store(struct node *address, struct node *value) {
	ir_new2(IR_STORE, address, value, 0);
}

struct node *ir_load(struct node *address, int size) {
	return ir_new1(IR_LOAD, address, size);
}

struct node *ir_bool_cast(struct node *operand) {
	return ir_new1(IR_BOOL_CAST, operand, calculate_size(type_simple(ST_BOOL)));
}

struct node *ir_cast_int(struct node *operand, int target_size, int sign_extend) {
	return ir_new1(sign_extend ? IR_INT_CAST_SIGN : IR_INT_CAST_ZERO, operand, target_size);
}

struct node *ir_cast_float(struct node *operand, int target_size) {
	return ir_new1(IR_FLOAT_CAST, operand, target_size);
}

struct node *ir_cast_int_to_float(struct node *operand, int target_size, int is_signed) {
	return ir_new1(is_signed ? IR_INT_FLOAT_CAST : IR_UINT_FLOAT_CAST, operand, target_size);
}

struct node *ir_cast_float_to_int(struct node *operand, int target_size) {
	return ir_new1(IR_FLOAT_INT_CAST, operand, target_size);
}

struct node *ir_binary_not(struct node *operand) {
	return ir_new1(IR_BINARY_NOT, operand, operand->size);
}

struct node *ir_negate_int(struct node *operand) {
	return ir_new1(IR_NEGATE_INT, operand, operand->size);
}

struct node *ir_negate_float(struct node *operand) {
	return ir_new1(IR_NEGATE_FLOAT, operand, operand->size);
}

struct node *ir_constant(struct constant constant) {
	int size = calculate_size(constant.data_type);
	if (constant.type == CONSTANT_LABEL_POINTER)
		size = 8;

	struct node *ins = ir_new(IR_CONSTANT, size);
	ins->constant.constant = constant;
	return ins;
}

void ir_write_constant_to_address(struct constant constant, struct node *address) {
	struct node *ins = ir_new1(IR_CONSTANT_ADDRESS, address, 0);
	ins->constant.constant = constant;
}

struct node *ir_binary_and(struct node *lhs, struct node *rhs) {
	return ir_new2(IR_BAND, lhs, rhs, lhs->size);
}

struct node *ir_left_shift(struct node *lhs, struct node *rhs) {
	return ir_new2(IR_LSHIFT, lhs, rhs, lhs->size);
}

struct node *ir_right_shift(struct node *lhs, struct node *rhs, int arithmetic) {
	return ir_new2(arithmetic ? IR_IRSHIFT : IR_RSHIFT, lhs, rhs, lhs->size);
}

struct node *ir_binary_or(struct node *lhs, struct node *rhs) {
	return ir_new2(IR_BOR, lhs, rhs, lhs->size);
}

struct node *ir_add(struct node *lhs, struct node *rhs) {
	return ir_new2(IR_ADD, lhs, rhs, lhs->size);
}

struct node *ir_sub(struct node *lhs, struct node *rhs) {
	return ir_new2(IR_SUB, lhs, rhs, lhs->size);
}

struct node *ir_mul(struct node *lhs, struct node *rhs) {
	return ir_new2(IR_MUL, lhs, rhs, lhs->size);
}

struct node *ir_imul(struct node *lhs, struct node *rhs) {
	return ir_new2(IR_IMUL, lhs, rhs, lhs->size);
}

struct node *ir_div(struct node *lhs, struct node *rhs) {
	return ir_new2(IR_DIV, lhs, rhs, lhs->size);
}

struct node *ir_idiv(struct node *lhs, struct node *rhs) {
	return ir_new2(IR_IDIV, lhs, rhs, lhs->size);
}

struct node *ir_equal(struct node *lhs, struct node *rhs) {
	return ir_new2(IR_EQUAL, lhs, rhs, lhs->size);
}

struct node *ir_binary_op(int type, struct node *lhs, struct node *rhs) {
	return ir_new2(type, lhs, rhs, lhs->size);
}

void ir_call(struct node *callee, int non_clobbered_register) {
	struct node *ins = ir_new1(IR_CALL, callee, 0);
	ins->call.non_clobbered_register = non_clobbered_register;
}

struct node *ir_vla_alloc(struct node *length) {
	return ir_new1(IR_VLA_ALLOC, length, 8);
}

void ir_set_reg(struct node *variable, int register_index, int is_sse) {
	struct node *ins = ir_new1(IR_SET_REG, variable, 8);
	ins->set_reg.register_index = register_index;
	ins->set_reg.is_sse = is_sse;
}

struct node *ir_get_reg(int size, int register_index, int is_sse) {
	struct node *ins = ir_new(IR_GET_REG, size);
	ins->get_reg.register_index = register_index;
	ins->get_reg.is_sse = is_sse;
	return ins;
}

struct node *ir_allocate(int size) {
	struct node *ins = ir_new(IR_ALLOC, 8);
	ins->alloc.size = size;
	ins->alloc.stack_location = -1;
	return ins;
}

struct node *ir_allocate_preamble(int size) {
	struct node *ins = ir_new(IR_ALLOC, 8);
	ins->alloc.size = size;
	ins->alloc.stack_location = -1;
	ins->alloc.save_to_preamble = 1;
	return ins;
}

void ir_modify_stack_pointer(int change) {
	struct node *ins = ir_new(IR_MODIFY_STACK_POINTER, 0);
	ins->modify_stack_pointer.change = change;
}

void ir_store_stack_relative(struct node *variable, int offset) {
	struct node *ins = ir_new1(IR_STORE_STACK_RELATIVE, variable, 0);
	ins->store_stack_relative.offset = offset;
}

void ir_store_stack_relative_address(struct node *variable, int offset, int size) {
	struct node *ins = ir_new1(IR_STORE_STACK_RELATIVE_ADDRESS, variable, 0);
	ins->store_stack_relative_address.offset = offset;
	ins->store_stack_relative_address.size = size;
}

struct node *ir_load_base_relative(int offset, int size) {
	struct node *ins = ir_new(IR_LOAD_BASE_RELATIVE, size);
	ins->load_base_relative.offset = offset;
	return ins;
}

void ir_load_base_relative_address(struct node *address, int offset, int size) {
	struct node *ins = ir_new1(IR_LOAD_BASE_RELATIVE_ADDRESS, address, 0);
	ins->load_base_relative_address.offset = offset;
	ins->load_base_relative_address.size = size;
}

void ir_set_zero_ptr(struct node *address, int size) {
	struct node *ins = ir_new1(IR_SET_ZERO_PTR, address, 0);
	ins->set_zero_ptr.size = size;
}

struct node *ir_load_part_address(struct node *address, int offset, int size) {
	struct node *ins = ir_new1(IR_LOAD_PART_ADDRESS, address, size);
	ins->load_part.offset = offset;
	return ins;
}

void ir_store_part_address(struct node *address, struct node *value, int offset) {
	struct node *ins = ir_new2(IR_STORE_PART_ADDRESS, address, value, 0);
	ins->store_part.offset = offset;
}

void ir_copy_memory(struct node *destination, struct node *source, int size) {
	struct node *ins = ir_new2(IR_COPY_MEMORY, destination, source, 0);
	ins->copy_memory.size = size;
}

struct node *ir_phi(struct node *var_a, struct node *var_b, struct node *block_a, struct node *block_b) {
	struct node *ins = ir_new2(IR_PHI, var_a, var_b, var_a->size);
	ins->phi.block_a = block_a;
	ins->phi.block_b = block_b;
	return ins;
}
