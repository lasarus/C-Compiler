#include "ir.h"
#include "dominator_tree.h"
#include "global_code_motion.h"

#include <common.h>
#include <abi/abi.h>

#include <assert.h>

struct node *first_function;

static struct node *current_function;
static struct node *current_block;

static int has_written_state = 0;

static size_t seal_size, seal_cap;
struct node **seals;

static size_t nodes_size, nodes_cap;
struct node **nodes;

struct node *ir_new(int type, int size) {
	struct node *next = ALLOC((struct node) { .type = type });

	static int counter;
	next->index = ++counter;
	next->size = size;

	next->parent_function = current_function;

	ADD_ELEMENT(nodes_size, nodes_cap, nodes) = next;

	return next;
}

struct node *ir_new4(int type, struct node *op1, struct node *op2, struct node *op3, struct node *op4, int size) {
	struct node *ins = ir_new(type, size);

	node_set_argument(ins, 0, op1);
	node_set_argument(ins, 1, op2);
	node_set_argument(ins, 2, op3);
	node_set_argument(ins, 3, op4);

	return ins;
}

struct node *ir_new3(int type, struct node *op1, struct node *op2, struct node *op3, int size) {
	struct node *ins = ir_new(type, size);

	node_set_argument(ins, 0, op1);
	node_set_argument(ins, 1, op2);
	node_set_argument(ins, 2, op3);

	return ins;
}

struct node *ir_new2(int type, struct node *op1, struct node *op2, int size) {
	struct node *ins = ir_new(type, size);

	node_set_argument(ins, 0, op1);
	node_set_argument(ins, 1, op2);

	return ins;
}

struct node *ir_new1(int type, struct node *op, int size) {
	return ir_new2(type, op, NULL, size);
}

void ir_get_node_list(struct node ***ret_nodes, size_t *ret_size) {
	*ret_nodes = nodes;
	*ret_size = nodes_size;
}

void ir_reset(void) {
	current_function = NULL;
	first_function = NULL;
	current_block = NULL;
	has_written_state = 0;

	free(seals);
	seal_size = seal_cap = 0;
	seals = NULL;
}

static void set_state(struct node *node);
static struct node *get_state(void);
static void seal_block(struct node *node);

void node_set_argument(struct node *node, int index, struct node *argument) {
	struct node *prev = node->arguments[index];

	if (prev == argument)
		return;

	if (prev) {
		unsigned use_index = 0;
		for (; use_index < prev->use_size; use_index++)
			if (prev->uses[use_index] == node)
				break;

		assert(use_index < prev->use_size);

		for (; use_index < prev->use_size - 1; use_index++) {
			prev->uses[use_index] = prev->uses[use_index + 1];
		}

		if (node->type == IR_PROJECT) {
			prev->projects[node->project.index] = NULL;
		}

		prev->use_size--;
	}

	if (argument)
		ADD_ELEMENT(argument->use_size, argument->use_cap, argument->uses) = node;
	node->arguments[index] = argument;

	if (node->type == IR_PROJECT) {
		argument->projects[node->project.index] = node;
	}

	if (node_is_control(node)) {
		if (node->type == IR_PROJECT) {
			node->block_info.is_sealed = 1;
		} else if (node->type == IR_REGION) {
			if (index == 1)
				seal_block(node);
		}
	}
}

int node_argument_count(struct node *node) {
	int count = 0;
	for (int i = 0; i < IR_MAX; i++) {
		if (node->arguments[i])
			count++;
	}
	return count;
}

struct node *new_block(void) {
	struct node *block = ir_new(IR_REGION, 0);

	ADD_ELEMENT(seal_size, seal_cap, seals) = block;

	return block;
}

struct node *ir_region(struct node *a, struct node *b) {
	struct node *block = new_block();

	node_set_argument(block, 0, a);
	node_set_argument(block, 1, b);

	return block;
}

struct node *new_function(const char *name, int is_global) {
	struct node *next = ir_new(IR_FUNCTION, 0);

	next->function.name = name;
	next->function.is_global = is_global;

	if (current_function)
		current_function->next = next;
	else
		first_function = next;

	current_function = next;

	has_written_state = 0;

	return next;
}

struct node *get_current_function(void) {
	return current_function;
}

void set_current_function(struct node *function) {
	current_function = function;
}

struct node *get_current_block(void) {
	return current_block;
}

// Algorithm taken from the article
// "Simple and Efficient Construction of Static Single Assignment Form"
static struct node *read_state(struct node *block);

static void write_state(struct node *block, struct node *value) {
	block->block_info.state = value;
}

static struct node *add_phi_operands(struct node *phi) {
	struct node *block = phi->arguments[0];
	assert(block->type == IR_REGION);

	int count = node_argument_count(block);

	for (int i = 0; i < count; i++) {
		struct node *pred = block->arguments[i];

		node_set_argument(phi, i + 1, read_state(pred));
	}

	return phi;
}

static struct node *read_state_recursive(struct node *block) {
	struct node *val = NULL;
	if (!block->block_info.is_sealed) {
		val = ir_new1(IR_PHI, block, 0);
		block->block_info.incomplete_phi = val;
	} else if (block->type == IR_PROJECT) {
		val = read_state(block->arguments[0]->arguments[0]);
	} else {
		val = ir_new1(IR_PHI, block, 0);
		write_state(block, val);
		val = add_phi_operands(val);
	}

	write_state(block, val);
	return val;
}

static struct node *read_state(struct node *block) {
	if (block->block_info.state) {
		return block->block_info.state;
	}

	return read_state_recursive(block);
}

static void seal_block(struct node *node) {
	current_function = node->parent_function;
	if (node->block_info.is_sealed)
		return;
	if (node->block_info.incomplete_phi)
		add_phi_operands(node->block_info.incomplete_phi);
	node->block_info.is_sealed = 1;
}

static void set_state(struct node *node) {
	write_state(current_block, node);
}

static struct node *get_state(void) {
	return read_state(current_block);
}

struct node *ir_project(struct node *node, int index, int size) {
	struct node *ret = ir_new(IR_PROJECT, size);
	ret->project.index = index;
	node_set_argument(ret, 0, node);
	return ret;
}

void ir_block_start(struct node *block) {
	current_block = block;
	if (!has_written_state) {
		set_state(ir_project(current_function, 3, 0));
		has_written_state = 1;
	}
}

void ir_if_selection(struct node *condition, struct node **block_true, struct node **block_false) {
	struct node *block = get_current_block();
	struct node *if_node = ir_new2(IR_IF, block, condition, 0);
	*block_false = ir_project(if_node, 1, 0);
	*block_true = ir_project(if_node, 0, 0);
	/* *block_false = new_block(); */
	/* *block_true = new_block(); */

	/* (*block_false)->type = IR_PROJECT; */
	/* node_set_argument(*block_false, 0, if_node); */
	/* (*block_false)->project.index = 1; */
	/* (*block_true)->type = IR_PROJECT; */
	/* node_set_argument(*block_true, 0, if_node); */
	/* (*block_true)->project.index = 0; */
}

void ir_goto(struct node *jump) {
	struct node *block = get_current_block();
	assert(jump->type == IR_REGION);
	assert(node_argument_count(jump) < 2);
	ir_connect(block, jump);
}

void ir_connect(struct node *start, struct node *end) {
	if (!end->arguments[0]) {
		node_set_argument(end, 0, start);
	} else if (!end->arguments[1]) {
		node_set_argument(end, 1, start);
	} else {
		ICE("Too many predecessors to block.");
	}
}

void ir_return(struct node *reg_state) {
	struct node *block = get_current_block();
	ir_new3(IR_RETURN, block, reg_state, get_state(), 0);
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
	for (struct node *f = first_function; f; f = f->next) {
		for (struct node *b = f->child; b; b = b->next) {
			for (struct node *ins = b->child; ins; ins = ins->next) {
				register_usage(b, ins);
				register_usage(b, ins->arguments[0]);
				register_usage(b, ins->arguments[1]);
				register_usage(b, ins->arguments[2]);
			}
		}
	}
}

void ir_va_start(struct node *address) {
	set_state(ir_new2(IR_VA_START, address, get_state(), 0));
}

void ir_va_arg(struct node *array, struct node *result_address, struct type *type) {
	struct node *ins = ir_new3(IR_VA_ARG, result_address, array, get_state(), 0);
	ins->va_arg_.type = type;
	set_state(ins);
}

void ir_store(struct node *address, struct node *value) {
	set_state(ir_new3(IR_STORE, address, value, get_state(), 0));
}

struct node *ir_load(struct node *address, int size) {
	if (address->type == IR_ALLOC) {
		return ir_new2(IR_LOAD, address, get_state(), size);
	} else {
		struct node *ins = ir_new2(IR_LOAD_VOLATILE, address, get_state(), 0);
		set_state(ir_project(ins, 0, 0));
		return ir_project(ins, 1, size);
	}
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

struct node *ir_zero(int size) {
	return ir_new(IR_ZERO, size);
}

struct node *ir_constant(struct constant constant) {
	int size = calculate_size(constant.data_type);
	if (constant.type == CONSTANT_LABEL_POINTER)
		size = 8;

	struct node *ins = ir_new(IR_CONSTANT, size);
	ins->constant.constant = constant;
	return ins;
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
	struct node *ins = ir_new3(IR_DIV, lhs, rhs, get_state(), 0);
	set_state(ir_project(ins, 1, 0));
	return ir_project(ins, 0, lhs->size);
}

struct node *ir_idiv(struct node *lhs, struct node *rhs) {
	struct node *ins = ir_new3(IR_IDIV, lhs, rhs, get_state(), 0);
	set_state(ir_project(ins, 1, 0));
	return ir_project(ins, 0, lhs->size);
}

struct node *ir_mod(struct node *lhs, struct node *rhs) {
	struct node *ins = ir_new3(IR_MOD, lhs, rhs, get_state(), 0);
	set_state(ir_project(ins, 1, 0));
	return ir_project(ins, 0, lhs->size);
}

struct node *ir_imod(struct node *lhs, struct node *rhs) {
	struct node *ins = ir_new3(IR_IMOD, lhs, rhs, get_state(), 0);
	set_state(ir_project(ins, 1, 0));
	return ir_project(ins, 0, lhs->size);
}

struct node *ir_equal(struct node *lhs, struct node *rhs) {
	return ir_new2(IR_EQUAL, lhs, rhs, lhs->size);
}

struct node *ir_binary_op(int type, struct node *lhs, struct node *rhs) {
	if (type == IR_IDIV)
		return ir_idiv(lhs, rhs);
	if (type == IR_DIV)
		return ir_idiv(lhs, rhs);
	if (type == IR_MOD)
		return ir_mod(lhs, rhs);
	if (type == IR_IMOD)
		return ir_imod(lhs, rhs);
	return ir_new2(type, lhs, rhs, lhs->size);
}

void ir_call(struct node *callee, struct node *reg_state, struct node *call_stack,
			 int non_clobbered_register,
			 struct node **reg_source) {
	struct node *ins = ir_new4(IR_CALL, callee, get_state(), reg_state, call_stack, 0);
	ins->call.non_clobbered_register = non_clobbered_register;
	set_state(ir_project(ins, 0, 0));
	*reg_source = ir_project(ins, 1, 0);
}

struct node *ir_vla_alloc(struct node *length) {
	return ir_new1(IR_VLA_ALLOC, length, 8);
}

struct node *ir_set_reg(struct node *variable, struct node *reg_state, int register_index, int is_sse) {
	struct node *ins = ir_new2(IR_SET_REG, variable, reg_state, 8);
	ins->set_reg.register_index = register_index;
	ins->set_reg.is_sse = is_sse;
	return ins;
}

struct node *ir_get_reg(struct node *source, int size, int register_index, int is_sse) {
	struct node *ins = ir_new1(IR_GET_REG, source, size);
	ins->get_reg.register_index = register_index;
	ins->get_reg.is_sse = is_sse;
	return ins;
}

struct node *ir_allocate(int size, int alignment) {
	struct node *ins = ir_new(IR_ALLOC, 8);
	ins->alloc.size = size;
	ins->alloc.stack_location = -1;
	ins->alloc.alignment = alignment;
	return ins;
}

void ir_allocate_preamble(int size) {
	current_function->function.preamble_alloc = size;
}

struct node *ir_allocate_call_stack(int change) {
	struct node *ins = ir_new(IR_ALLOCATE_CALL_STACK, 0);
	ins->allocate_call_stack.change = change;
	return ins;
}

struct node *ir_store_stack_relative(struct node *call_stack, struct node *variable, int offset) {
	struct node *ins = ir_new2(IR_STORE_STACK_RELATIVE, variable, call_stack, 0);
	ins->store_stack_relative.offset = offset;
	return ins;
}

struct node *ir_store_stack_relative_address(struct node *call_stack, struct node *variable, int offset, int size) {
	struct node *ins = ir_new2(IR_STORE_STACK_RELATIVE_ADDRESS, variable, call_stack, 0);
	ins->store_stack_relative_address.offset = offset;
	ins->store_stack_relative_address.size = size;
	return ins;
}

struct node *ir_load_base_relative(struct node *call_stack, int offset, int size) {
	struct node *ins = ir_new1(IR_LOAD_BASE_RELATIVE, call_stack, size);
	ins->load_base_relative.offset = offset;
	return ins;
}

void ir_load_base_relative_address(struct node *call_stack, struct node *address, int offset, int size) {
	struct node *ins = ir_new3(IR_LOAD_BASE_RELATIVE_ADDRESS, address, get_state(), call_stack, 0);
	ins->load_base_relative_address.offset = offset;
	ins->load_base_relative_address.size = size;
	set_state(ins);
}

void ir_set_zero_ptr(struct node *address, int size) {
	struct node *ins = ir_new2(IR_SET_ZERO_PTR, address, get_state(), 0);
	ins->set_zero_ptr.size = size;
	set_state(ins);
}

struct node *ir_load_part_address(struct node *address, int offset, int size) {
	struct node *ins = ir_new2(IR_LOAD_PART_ADDRESS, address, get_state(), 0);
	ins->load_part.offset = offset;
	set_state(ir_project(ins, 0, 0));
	return ir_project(ins, 1, size);
}

void ir_store_part_address(struct node *address, struct node *value, int offset) {
	struct node *ins = ir_new3(IR_STORE_PART_ADDRESS, address, value, get_state(), 0);
	ins->store_part.offset = offset;
	set_state(ins);
}

void ir_copy_memory(struct node *destination, struct node *source, int size) {
	struct node *ins = ir_new3(IR_COPY_MEMORY, destination, source, get_state(), 0);
	ins->copy_memory.size = size;
	set_state(ins);
}

struct node *ir_phi(struct node *var_a, struct node *var_b) {
	struct node *block = get_current_block();
	return ir_new3(IR_PHI, block, var_a, var_b, var_a->size);
}

// Iterate through all blocks and order them.
void ir_schedule_blocks(void) {
	ir_post_order_blocks();
	ir_calculate_dominator_tree();

	for (struct node *f = first_function; f; f = f->next) {
		for (struct node *b = f->child; b; b = b->next) {
			for (unsigned i = 0; i < b->use_size; i++) {
				// Either phi, or block-end node.
				struct node *end = b->uses[i];

				if (end->type == IR_PHI)
					continue;

				b->block_info.end = end;

				if (end->type == IR_IF) {
					b->block_info.end = end;
					for (unsigned j = 0; j < end->use_size; j++) {
						struct node *proj = end->uses[j];
						assert(proj->type == IR_PROJECT);

						if (proj->project.index == 0) {
							end->if_info.block_true = proj;
						} else if (proj->project.index == 1) {
							end->if_info.block_false = proj;
						}
					}
				}
			}
		}
	}
}

int node_is_instruction(struct node *node) {
	// If not function or control.

	return node->type != IR_FUNCTION &&
		!node_is_control(node);
}

struct node *node_get_prev_state(struct node *node) {
	if (node->type == IR_CALL)
		return node->arguments[1];
	else if (node->type == IR_STORE)
		return node->arguments[2];
	else if (node->type == IR_VA_START)
		return node->arguments[1];
	else if (node->type == IR_VA_ARG)
		return node->arguments[2];
	else if (node->type == IR_LOAD_VOLATILE)
		return node->arguments[1];
	else if (node->type == IR_LOAD_BASE_RELATIVE_ADDRESS)
		return node->arguments[1];
	else if (node->type == IR_SET_ZERO_PTR)
		return node->arguments[1];
	else if (node->type == IR_STORE_PART_ADDRESS)
		return node->arguments[2];
	else if (node->type == IR_LOAD_PART_ADDRESS)
		return node->arguments[1];
	else if (node->type == IR_COPY_MEMORY)
		return node->arguments[2];
	else if (node->type == IR_DIV ||
			 node->type == IR_IDIV ||
			 node->type == IR_MOD ||
			 node->type == IR_IMOD)
		return node->arguments[2];
	return NULL;
}

void ir_local_schedule_recursive(struct node *node, struct node **end, struct node **first) {
	// Iterate through all child controls and turn them into a list.
	// Return last node in the list.
	if (node->visited == 5 || !node_is_instruction(node) || !node->block)
		return;

	node->visited = 5;

	for (unsigned i = 0; i < IR_MAX; i++) {
		struct node *arg = node->arguments[i];

		if (arg && arg->block == node->block)
			ir_local_schedule_recursive(arg, end, first);
	}

	// If node consumes a state, it needs to come after the
	// uses of the state that is consumed.

	struct node *prev_state = node_get_prev_state(node);
	if (prev_state && prev_state->block && prev_state->block == node->block) {
		for (unsigned i = 0; i < prev_state->use_size; i++) {
			struct node *use = prev_state->uses[i];

			ir_local_schedule_recursive(use, end, first);
		}
	}

	if (*end) {
		(*end)->next = node;
		*end = node;
	} else {
		*first = *end = node;
	}
}

static void ir_add_instructions_to_block_children(void) {
	for (unsigned i = 0; i < nodes_size; i++) {
		struct node *node = nodes[i];
		struct node *block = node->block;
		if (node_is_instruction(node) && block) {
			ADD_ELEMENT(block->block_info.children_size,
						block->block_info.children_cap,
						block->block_info.children) = node;
		}
	}
}

void ir_local_schedule(void) {
	ir_schedule_instructions_to_blocks();
	ir_add_instructions_to_block_children();

	for (struct node *f = first_function; f; f = f->next) {
		for (struct node *b = f->child; b; b = b->next) {
			struct node *end = NULL, *first = NULL;

			for (unsigned i = 0; i < b->block_info.children_size; i++) {
				struct node *node = b->block_info.children[i];
				ir_local_schedule_recursive(node, &end, &first);
			}

			b->child = first;
		}
	}
}

void ir_seal_blocks(void) {
	for (size_t i = 0; i < seal_size; i++) {
		seal_block(seals[i]);
	}
}

int node_is_control(struct node *node) {
	if (node->type == IR_REGION) {
		return 1;
	} else if (node->type == IR_PROJECT) {
		if (node->arguments[0]->type == IR_IF) {
			return 1;
		}

		if (node->project.index == 0 &&
			node->arguments[0]->type == IR_FUNCTION)
			return 1;
	}
	return 0;
}

void ir_replace_node(struct node *original, struct node *replacement) {
	for (int i = original->use_size - 1; i >= 0; i--) {
		struct node *use = original->uses[i];
		for (int j = 0; j < IR_MAX; j++) {
			if (use->arguments[j] == original) {
				node_set_argument(use, j, replacement);
			}
		}
	}

	/* original->use_cap = original->use_size = 0; */
	/* original->uses = NULL; */
}
