#include "abi.h"

#include <parser/symbols.h>
#include <codegen/codegen.h>
#include <codegen/registers.h>
#include <arch/calling.h>
#include <common.h>

static const int calling_convention[] = { REG_RCX, REG_RDX, REG_R8, REG_R9 };
//static const int return_convention[] = { REG_RAX };
static const int shadow_space = 32;

struct ms_data {
	int overflow_position;

	int is_variadic;

	int returns_address;
	var_id ret_address;
};

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
};

static struct call_info get_calling_convention(var_id result, struct type *function_type, int n_args, struct type **argument_types, var_id *args) {
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

	return (struct call_info) {
		.regs_size = regs_size,
		.regs = regs,

		.ret_regs_size = ret_regs_size,
		.ret_regs = ret_regs,

		.stack_variables_size = stack_variables_size,
		.stack_variables = stack_variables,

		.returns_address = returns_address,
		.ret_address = ret_address,
	};
}

static void ms_ir_function_call(var_id result, var_id func_var, struct type *function_type, int n_args, struct type **argument_types, var_id *args) {
	struct call_info c = get_calling_convention(result, function_type, n_args, argument_types, args);

	if (c.returns_address) {
		IR_PUSH_ADDRESS_OF(c.ret_address, result);
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

	IR_PUSH_MODIFY_STACK_POINTER(-stack_sub - shadow_space);

	int current_mem = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		IR_PUSH_STORE_STACK_RELATIVE(current_mem + shadow_space, c.stack_variables[i]);
		current_mem += round_up_to_nearest(get_variable_size(c.stack_variables[i]), 8);
	}

	for (int i = 0; i < c.regs_size; i++)
		IR_PUSH_SET_REG(c.regs[i].variable, c.regs[i].register_idx, c.regs[i].is_sse);

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

	IR_PUSH_MODIFY_STACK_POINTER(+stack_sub + shadow_space);
}

static void ms_ir_function_new(struct type *type, var_id *args, const char *name, int is_global) {
	int n_args = type->n - 1;

	struct function *func = &ADD_ELEMENT(ir.size, ir.cap, ir.functions);
	*func = (struct function) {
		.name = name,
		.is_global = is_global,
	};

	struct ms_data abi_data = { 0 };

	ir_block_start(new_block());

	struct call_info c = get_calling_convention(0, type, n_args, type->children + 1, args);
	
	if (c.returns_address) {
		variable_set_stack_bucket(c.ret_address, 0);
		abi_data.ret_address = c.ret_address;
	}

	int total_mem_needed = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		total_mem_needed += round_up_to_nearest(get_variable_size(c.stack_variables[i]), 8);
	}


	if (type->function.is_variadic) {
		abi_data.is_variadic = 1;
		abi_data.overflow_position = total_mem_needed + 16;
	}

	for (int i = 0; i < c.regs_size; i++)
		IR_PUSH_GET_REG(c.regs[i].variable, c.regs[i].register_idx, c.regs[i].is_sse);

	int current_mem = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		IR_PUSH_LOAD_BASE_RELATIVE(c.stack_variables[i], current_mem + 16 + shadow_space);
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

	func->abi_data = malloc(sizeof (struct ms_data));
	*(struct ms_data *)func->abi_data = abi_data;
}

static void ms_ir_function_return(struct function *func, var_id value, struct type *type) {
	(void)func, (void)value;
	if (type == type_simple(ST_VOID))
		return;

	struct ms_data *abi_data = func->abi_data;

	if (abi_data->returns_address) {
		IR_PUSH_STORE(value, abi_data->ret_address);
	} else {
		IR_PUSH_SET_REG(value, 0, 0);
	}
}

static void ms_emit_function_preamble(struct function *func) {
	struct ms_data *abi_data = func->abi_data;

	if (!abi_data->is_variadic)
		return;

	emit("movq %%rcx, 16(%%rbp)");
	emit("movq %%rdx, 24(%%rbp)");
	emit("movq %%r8, 32(%%rbp)");
	emit("movq %%r9, 40(%%rbp)");
}

static void ms_emit_va_start(var_id result, struct function *func) {
	struct ms_data *abi_data = func->abi_data;

	(void)result, (void)abi_data;
	NOTIMP();
}

static void ms_emit_va_arg(var_id result, var_id va_list, struct type *type) {
	(void)result, (void)va_list, (void)type;
	NOTIMP();
}

void abi_init_microsoft(void) {
	abi_ir_function_call = ms_ir_function_call;
	abi_ir_function_new = ms_ir_function_new;
	abi_ir_function_return = ms_ir_function_return;
	abi_emit_function_preamble = ms_emit_function_preamble;
	abi_emit_va_start = ms_emit_va_start;
	abi_emit_va_start = ms_emit_va_start;
	abi_emit_va_arg = ms_emit_va_arg;

	// Initialize the __builtin_va_list typedef.
	struct symbol_typedef *sym =
		symbols_add_typedef(sv_from_str("__builtin_va_list"));

	sym->data_type = type_pointer(type_simple(ST_VOID));
}
