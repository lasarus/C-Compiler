#include "ir.h"

#include <common.h>
#include <parser/declaration.h>
#include <arch/calling.h>
#include <codegen/registers.h>

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
	uint64_t mask = gen_mask(64 - offset - length, offset);
	var_id mask_var = new_variable_sz(8, 1, 1);
	var_id shift_var = new_variable_sz(8, 1, 1);

	var_id value_large = new_variable_sz(8, 1, 1);
	var_id field_large = new_variable_sz(8, 1, 1);
	var_id result_large = new_variable_sz(8, 1, 1);

	IR_PUSH_INT_CAST(value_large, value, 0);
	IR_PUSH_INT_CAST(field_large, field, 0);

	IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
				.data_type = type_simple(ST_UCHAR), .ullong_d = offset }),
				shift_var);
	IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
				.data_type = type_simple(ST_ULLONG), .ullong_d = mask }),
				mask_var);
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

	IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
				.data_type = type_simple(ST_ULLONG), .ullong_d = 64 - offset - length }),
				shift_var);

	IR_PUSH_BINARY_OPERATOR(IBO_LSHIFT, field_large, shift_var, field_large);

	IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
				.data_type = type_simple(ST_ULLONG), .ullong_d = 64 - length }),
				shift_var);

	if (sign_extend) {
		IR_PUSH_BINARY_OPERATOR(IBO_IRSHIFT, field_large, shift_var, field_large);
	} else {
		IR_PUSH_BINARY_OPERATOR(IBO_RSHIFT, field_large, shift_var, field_large);
	}

	IR_PUSH_INT_CAST(result, field_large, 0);
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

void ir_call(var_id result, var_id func_var, struct type *function_type, int n_args, struct type **argument_types, var_id *args, enum call_abi abi) {
	struct type *return_type = function_type->children[0];

	static int regs_size = 0, regs_cap = 0;
	regs_size = 0;
	struct reg_info {
		var_id variable;
		int register_idx, is_sse;
	} *regs = NULL;

	static int ret_regs_size = 0, ret_regs_cap = 0;
	ret_regs_size = 0;
	struct reg_info *ret_regs = NULL;

	static int stack_variables_size = 0, stack_variables_cap = 0;
	stack_variables_size = 0;
	var_id *stack_variables = NULL;

	switch (abi) {
	case CALL_ABI_MICROSOFT:
		NOTIMP();
		break;
	case CALL_ABI_SYSV: {
		static const int calling_convention[] = {REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9};
		static const int return_convention[] = { REG_RAX, REG_RDX };

		int current_gp_reg = 0, current_sse_reg = 0;
		const int max_gp_reg = 6, max_sse_reg = 8;
		if (!type_is_simple(return_type, ST_VOID)) {
			int n_parts;
			enum parameter_class classes[4];
			classify(return_type, &n_parts, classes);

			if (classes[0] == CLASS_MEMORY) {
				var_id address = new_variable_sz(8, 1, 1);
				IR_PUSH_ADDRESS_OF(address, result);
				current_gp_reg = 1;
				ADD_ELEMENT(regs_size, regs_cap, regs) = (struct reg_info) {
					.variable = address,
					.register_idx = calling_convention[0]
				};
			} else {
				int gp_idx = 0, ssa_idx = 0;
				for (int j = 0; j < n_parts; j++) {
					var_id part = new_variable_sz(8, 1, 1);
					if (classes[j] == CLASS_SSE || classes[j] == CLASS_SSEUP) {
						ADD_ELEMENT(ret_regs_size, ret_regs_cap, ret_regs) = (struct reg_info) {
							.variable = part,
							.is_sse = 1,
							.register_idx = ssa_idx++
						};
					} else {
						ADD_ELEMENT(ret_regs_size, ret_regs_cap, ret_regs) = (struct reg_info) {
							.variable = part,
							.is_sse = 0,
							.register_idx = return_convention[gp_idx++]
						};
					}
				}
			}
		}

		for (int i = 0; i < n_args; i++) {
			struct type *type = argument_types[i];
			int n_parts;
			enum parameter_class classes[4];

			classify(type, &n_parts, classes);

			int is_memory = 0;

			int n_gp_regs = 0, n_sse_regs = 0;
			for (int j = 0; j < n_parts; j++) {
				if (classes[j] == CLASS_MEMORY)
					is_memory = 1;
				else if (classes[j] == CLASS_SSE || classes[j] == CLASS_SSEUP)
					n_sse_regs++;
				else if (classes[j] == CLASS_INTEGER)
					n_gp_regs++;
				else
					NOTIMP();
			}

			is_memory = classes[0] == CLASS_MEMORY ||
				current_gp_reg + n_gp_regs > max_gp_reg ||
				current_sse_reg + n_sse_regs > max_sse_reg;
			if (is_memory) {
				ADD_ELEMENT(stack_variables_size, stack_variables_cap, stack_variables) = args[i];
			} else {
				var_id address = new_variable_sz(8, 1, 1);
				var_id eight_constant = new_variable_sz(8, 1, 1);
				IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
							.data_type = type_simple(ST_ULLONG), .ullong_d = 8 }),
					eight_constant);
				IR_PUSH_ADDRESS_OF(address, args[i]);
				for (int j = 0; j < n_parts; j++) {
					var_id part = new_variable_sz(8, 1, 1);
					IR_PUSH_LOAD(part, address);
					IR_PUSH_BINARY_OPERATOR(IBO_ADD, address, eight_constant, address);

					if (classes[j] == CLASS_SSE || classes[j] == CLASS_SSEUP) {
						ADD_ELEMENT(regs_size, regs_cap, regs) = (struct reg_info) {
							.variable = part,
							.is_sse = 1,
							.register_idx = current_sse_reg++
						};
					} else {
						ADD_ELEMENT(regs_size, regs_cap, regs) = (struct reg_info) {
							.variable = part,
							.is_sse = 0,
							.register_idx = calling_convention[current_gp_reg++]
						};
					}
				}
			}
		}
	} break;
	}

	int total_mem_needed = 0;
	for (int i = 0; i < stack_variables_size; i++) {
		total_mem_needed += round_up_to_nearest(get_variable_size(stack_variables[i]), 8);
	}
	int stack_sub = round_up_to_nearest(total_mem_needed, 16);

	IR_PUSH_MODIFY_STACK_POINTER(-stack_sub);

	int current_mem = 0;
	for (int i = 0; i < stack_variables_size; i++) {
		IR_PUSH_STORE_STACK_RELATIVE(current_mem, stack_variables[i]);
		current_mem += round_up_to_nearest(get_variable_size(stack_variables[i]), 8);
	}

	for (int i = 0; i < regs_size; i++)
		IR_PUSH_SET_REG(regs[i].variable, regs[i].register_idx, regs[i].is_sse);

	IR_PUSH_CALL(func_var, REG_RBX);

	for (int i = 0; i < ret_regs_size; i++)
		IR_PUSH_GET_REG(ret_regs[i].variable, ret_regs[i].register_idx, ret_regs[i].is_sse);

	if (ret_regs_size == 1) {
		IR_PUSH_INT_CAST(result, ret_regs[0].variable, 0);
	} else if (ret_regs_size == 2) {
		var_id address = new_variable_sz(8, 1, 1);
		var_id eight_constant = new_variable_sz(8, 1, 1);
		IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
					.data_type = type_simple(ST_ULLONG), .ullong_d = 8 }),
			eight_constant);
		IR_PUSH_ADDRESS_OF(address, result);

		IR_PUSH_STORE(ret_regs[0].variable, address);
		IR_PUSH_BINARY_OPERATOR(IBO_ADD, address, eight_constant, address);
		IR_PUSH_STORE(ret_regs[1].variable, address);
	}

	IR_PUSH_MODIFY_STACK_POINTER(+stack_sub);
}
