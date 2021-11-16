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

struct reg_info {
	var_id variable;
	int register_idx, is_sse;
	var_id merge_into, merge_pos;
};

struct call_info {
	int regs_size;
	struct reg_info *regs;

	int ret_regs_size;
	struct reg_info *ret_regs;

	int stack_variables_size;
	var_id *stack_variables;

	int returns_address;
	var_id ret_address;

	int gp_offset;

	int rax;
};

static struct call_info get_calling_convention(var_id result, struct type *function_type, int n_args, struct type **argument_types, var_id *args, enum call_abi abi, int calling) {
	struct type *return_type = function_type->children[0];

	static int regs_size = 0, regs_cap = 0;
	regs_size = 0;
	struct reg_info *regs = NULL;

	static int ret_regs_size = 0, ret_regs_cap = 0;
	ret_regs_size = 0;
	struct reg_info *ret_regs = NULL;

	static int stack_variables_size = 0, stack_variables_cap = 0;
	stack_variables_size = 0;
	var_id *stack_variables = NULL;

	int returns_address = 0;
	var_id ret_address;

	int gp_offset = 0;

	int rax = -1;

	switch (abi) {
	case CALL_ABI_MICROSOFT: {
		static const int calling_convention[] = { REG_RCX, REG_RDX, REG_R8, REG_R9 };
		int current_reg = 0;

		if (!type_is_simple(return_type, ST_VOID)) {
			int ret_size = calculate_size(return_type);
			int can_be_reg = ret_size == 1 || ret_size == 2 || ret_size == 4 || ret_size == 8;

			if (can_be_reg) {
				if (type_is_floating(return_type)) {
					ADD_ELEMENT(ret_regs_size, ret_regs_cap, ret_regs) = (struct reg_info) {
						.variable = result,
						.is_sse = 1,
						.register_idx = REG_RAX
					};
				} else {
					ADD_ELEMENT(ret_regs_size, ret_regs_cap, ret_regs) = (struct reg_info) {
						.variable = result,
						.is_sse = 0,
						.register_idx = 0 // xmm0
					};
				}
			} else {
				returns_address = 1;
				ret_address = new_variable_sz(8, 1, 1);
				current_reg = 1;
				ADD_ELEMENT(regs_size, regs_cap, regs) = (struct reg_info) {
					.variable = ret_address,
					.register_idx = calling_convention[0]
				};
			}
		}

		for (int i = 0; i < n_args; i++) {
			struct type *type = argument_types[i];
			var_id v = args[i];
			int size = calculate_size(type);
			int can_be_reg = size == 1 || size == 2 || size == 4 || size == 8;

			var_id reg_to_push = v;
			if (!can_be_reg) {
				var_id copy = new_variable_sz(get_variable_size(v), 1, 1);
				IR_PUSH_COPY(copy, v);
				var_id address = new_variable_sz(8, 1, 1);
				IR_PUSH_ADDRESS_OF(address, copy);
				current_reg = 1;
				reg_to_push = address;
			}

			if (current_reg < 4) {
				ADD_ELEMENT(regs_size, regs_cap, regs) = (struct reg_info) {
					.variable = reg_to_push,
					.register_idx = calling_convention[current_reg++]
				};
			} else {
				ADD_ELEMENT(stack_variables_size, stack_variables_cap, stack_variables) = reg_to_push;
			}
		}
	} break;

	case CALL_ABI_SYSV: {
		static const int calling_convention[] = { REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9 };
		static const int return_convention[] = { REG_RAX, REG_RDX };

		rax = 0;

		int current_gp_reg = 0, current_sse_reg = 0;
		const int max_gp_reg = 6, max_sse_reg = 8;
		if (!type_is_simple(return_type, ST_VOID)) {
			int n_parts;
			enum parameter_class classes[4];
			classify(return_type, &n_parts, classes);

			if (classes[0] == CLASS_MEMORY) {
				returns_address = 1;
				ret_address = new_variable_sz(8, 1, 1);

				current_gp_reg = 1;
				ADD_ELEMENT(regs_size, regs_cap, regs) = (struct reg_info) {
					.variable = ret_address,
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

			int argument_size = get_variable_size(args[i]);
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
				for (int j = 0; j < n_parts; j++) {
					int part_size = MIN(argument_size - j * 8, 8);

					if (calling)
						part_size = 8;

					var_id part = new_variable_sz(part_size, 1, 1);

					if (classes[j] == CLASS_SSE || classes[j] == CLASS_SSEUP) {
						ADD_ELEMENT(regs_size, regs_cap, regs) = (struct reg_info) {
							.variable = part,
							.is_sse = 1,
							.register_idx = current_sse_reg++,
							.merge_into = args[i],
							.merge_pos = j * 8
						};
					} else {
						ADD_ELEMENT(regs_size, regs_cap, regs) = (struct reg_info) {
							.variable = part,
							.is_sse = 0,
							.register_idx = calling_convention[current_gp_reg++],
							.merge_into = args[i],
							.merge_pos = j * 8
						};
					}
				}
			}
		}

		gp_offset = current_gp_reg * 8;
		rax = current_sse_reg;
	} break;
	}

	return (struct call_info) {
		.regs_size = regs_size,
		.regs = regs,

		.ret_regs_size = ret_regs_size,
		.ret_regs = ret_regs,

		.stack_variables_size = stack_variables_size,
		.stack_variables = stack_variables,

		.returns_address = returns_address,
		.ret_address = ret_address,

		.gp_offset = gp_offset,

		.rax = rax
	};
}

void ir_call(var_id result, var_id func_var, struct type *function_type, int n_args, struct type **argument_types, var_id *args, enum call_abi abi) {
	struct call_info c = get_calling_convention(result, function_type, n_args, argument_types, args, abi, 1);

	if (c.returns_address) {
		IR_PUSH_ADDRESS_OF(c.ret_address, result);
	}

	var_id rax_constant;
	if (c.rax != -1) {
		rax_constant = new_variable_sz(8, 1, 1);
		IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
					.data_type = type_simple(ST_ULLONG), .ullong_d = c.rax }),
			rax_constant);
	}

	for (int i = 0; i < c.regs_size; i++) {
		if (!c.regs[i].merge_into)
			continue;

		var_id address = new_variable_sz(8, 1, 1);
		var_id offset_constant = new_variable_sz(8, 1, 1);

		IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
					.data_type = type_simple(ST_ULLONG), .ullong_d = c.regs[i].merge_pos }),
			offset_constant);
		IR_PUSH_ADDRESS_OF(address, c.regs[i].merge_into);
		IR_PUSH_BINARY_OPERATOR(IBO_ADD, address, offset_constant, address);
		IR_PUSH_LOAD(c.regs[i].variable, address);
	}

	int total_mem_needed = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		total_mem_needed += round_up_to_nearest(get_variable_size(c.stack_variables[i]), 8);
	}
	int stack_sub = round_up_to_nearest(total_mem_needed, 16);

	IR_PUSH_MODIFY_STACK_POINTER(-stack_sub);

	int current_mem = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		IR_PUSH_STORE_STACK_RELATIVE(current_mem, c.stack_variables[i]);
		current_mem += round_up_to_nearest(get_variable_size(c.stack_variables[i]), 8);
	}

	for (int i = 0; i < c.regs_size; i++)
		IR_PUSH_SET_REG(c.regs[i].variable, c.regs[i].register_idx, c.regs[i].is_sse);

	if (c.rax != -1)
		IR_PUSH_SET_REG(rax_constant, REG_RAX, 0);

	IR_PUSH_CALL(func_var, REG_RBX);

	for (int i = 0; i < c.ret_regs_size; i++)
		IR_PUSH_GET_REG(c.ret_regs[i].variable, c.ret_regs[i].register_idx, c.ret_regs[i].is_sse);

	if (c.ret_regs_size == 1) {
		IR_PUSH_INT_CAST(result, c.ret_regs[0].variable, 0);
	} else if (c.ret_regs_size == 2) {
		var_id address = new_variable_sz(8, 1, 1);
		var_id eight_constant = new_variable_sz(8, 1, 1);
		IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
					.data_type = type_simple(ST_ULLONG), .ullong_d = 8 }),
			eight_constant);
		IR_PUSH_ADDRESS_OF(address, result);

		IR_PUSH_STORE(c.ret_regs[0].variable, address);
		IR_PUSH_BINARY_OPERATOR(IBO_ADD, address, eight_constant, address);
		IR_PUSH_STORE(c.ret_regs[1].variable, address);
	}

	IR_PUSH_MODIFY_STACK_POINTER(+stack_sub);
}

void ir_new_function(struct type *function_type, var_id *args, const char *name, int is_global,
					 enum call_abi abi) {
	int n_args = function_type->n - 1;

	struct function *func = &ADD_ELEMENT(ir.size, ir.cap, ir.functions);
	*func = (struct function) {
		.name = name,
		.is_global = is_global,
		.abi = abi,
	};

	ir_block_start(new_block());

	struct call_info c = get_calling_convention(0, function_type, n_args, function_type->children + 1, args, abi, 0);
	
	if (c.returns_address) {
		variable_set_stack_bucket(c.ret_address, 0);
		func->ret_ptr = c.ret_address;
	}

	int total_mem_needed = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		total_mem_needed += round_up_to_nearest(get_variable_size(c.stack_variables[i]), 8);
	}

	func->gp_offset = c.gp_offset;
	func->overflow_position = total_mem_needed + 16;

	for (int i = 0; i < c.regs_size; i++)
		IR_PUSH_GET_REG(c.regs[i].variable, c.regs[i].register_idx, c.regs[i].is_sse);

	int current_mem = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		IR_PUSH_LOAD_BASE_RELATIVE(c.stack_variables[i], current_mem + 16);
		current_mem += round_up_to_nearest(get_variable_size(c.stack_variables[i]), 8);
	}

	for (int i = 0; i < c.regs_size; i++) {
		if (!c.regs[i].merge_into)
			continue;

		var_id address = new_variable_sz(8, 1, 1);
		var_id offset_constant = new_variable_sz(8, 1, 1);

		IR_PUSH_CONSTANT(((struct constant) { .type = CONSTANT_TYPE,
					.data_type = type_simple(ST_ULLONG), .ullong_d = c.regs[i].merge_pos }),
			offset_constant);
		IR_PUSH_ADDRESS_OF(address, c.regs[i].merge_into);
		IR_PUSH_BINARY_OPERATOR(IBO_ADD, address, offset_constant, address);
		IR_PUSH_STORE(c.regs[i].variable, address);
	}
}
