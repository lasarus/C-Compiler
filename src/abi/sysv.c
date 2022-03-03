#include "abi.h"

#include <parser/symbols.h>
#include <codegen/codegen.h>
#include <codegen/registers.h>
#include <arch/calling.h>
#include <common.h>
#include <preprocessor/macro_expander.h>

static const int calling_convention[] = { REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9 };
static const int return_convention[] = { REG_RAX, REG_RDX };

static struct struct_data *builtin_va_list = NULL;

struct sysv_data {
	var_id reg_save_area;
	int overflow_position;
	int gp_offset;
	int fp_offset;

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

	int gp_offset;

	int rax;

	int shadow_space;
};

static struct call_info get_calling_convention(struct type *function_type, int n_args, struct type **argument_types, var_id *args, int calling) {
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
	var_id ret_address = 0;

	int gp_offset = 0;

	int rax = -1;

	int shadow_space = 0;

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

		.rax = rax,

		.shadow_space = shadow_space
	};
}

void split_variable(var_id variable, int n_parts, var_id *parts) {
	var_id address = new_variable_sz(8, 1, 1);
	var_id offset_constant = new_variable_sz(8, 1, 1);
	IR_PUSH_ADDRESS_OF(address, variable);

	IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.size_type, 8), offset_constant);

	for (int i = 0; i < n_parts; i++) {
		parts[i] = new_variable_sz(8, 1, 1);
		IR_PUSH_LOAD(parts[i], address);
		IR_PUSH_BINARY_OPERATOR(IBO_ADD, address, offset_constant, address);
	}
}

static void sysv_ir_function_call(var_id result, var_id func_var, struct type *function_type, int n_args, struct type **argument_types, var_id *args) {
	struct call_info c = get_calling_convention(function_type, n_args, argument_types, args, 1);

	if (c.returns_address) {
		IR_PUSH_ADDRESS_OF(c.ret_address, result);
	}

	var_id rax_constant;
	if (c.rax != -1) {
		rax_constant = new_variable_sz(8, 1, 1);
		IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.size_type, c.rax), rax_constant);
	}

	for (int i = 0; i < c.regs_size; i++) {
		if (!c.regs[i].merge_into)
			continue;

		var_id address = new_variable_sz(8, 1, 1);
		var_id offset_constant = new_variable_sz(8, 1, 1);

		IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.size_type, c.regs[i].merge_pos), offset_constant);
		IR_PUSH_ADDRESS_OF(address, c.regs[i].merge_into);
		IR_PUSH_BINARY_OPERATOR(IBO_ADD, address, offset_constant, address);
		IR_PUSH_LOAD(c.regs[i].variable, address);
	}

	int total_mem_needed = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		total_mem_needed += round_up_to_nearest(get_variable_size(c.stack_variables[i]), 8);
	}
	int stack_sub = round_up_to_nearest(total_mem_needed, 16);

	IR_PUSH_MODIFY_STACK_POINTER(-stack_sub - c.shadow_space);

	int current_mem = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		IR_PUSH_STORE_STACK_RELATIVE(current_mem + c.shadow_space, c.stack_variables[i]);
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

		IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.size_type, 8), eight_constant);
		IR_PUSH_ADDRESS_OF(address, result);

		IR_PUSH_STORE(c.ret_regs[0].variable, address);
		IR_PUSH_BINARY_OPERATOR(IBO_ADD, address, eight_constant, address);
		IR_PUSH_STORE(c.ret_regs[1].variable, address);
	}

	IR_PUSH_MODIFY_STACK_POINTER(+stack_sub + c.shadow_space);
}

static void sysv_ir_function_new(struct type *type, var_id *args, const char *name, int is_global) {
	int n_args = type->n - 1;

	struct function *func = &ADD_ELEMENT(ir.size, ir.cap, ir.functions);
	*func = (struct function) {
		.name = name,
		.is_global = is_global,
	};

	struct sysv_data abi_data = { 0 };

	ir_block_start(new_block());

	struct call_info c = get_calling_convention(type, n_args, type->children + 1, args, 0);
	
	if (c.returns_address) {
		variable_set_stack_bucket(c.ret_address, 0);
		abi_data.ret_address = c.ret_address;
		abi_data.returns_address = 1;
	}

	int total_mem_needed = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		total_mem_needed += round_up_to_nearest(get_variable_size(c.stack_variables[i]), 8);
	}


	if (type->function.is_variadic) {
		abi_data.is_variadic = 1;
		abi_data.reg_save_area = new_variable_sz(304, 1, 0); // According to Figure 3.33 in sysV AMD64 ABI.
		abi_data.gp_offset = c.gp_offset;
		abi_data.overflow_position = total_mem_needed + 16;
	}

	for (int i = 0; i < c.regs_size; i++)
		IR_PUSH_GET_REG(c.regs[i].variable, c.regs[i].register_idx, c.regs[i].is_sse);

	int current_mem = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		IR_PUSH_LOAD_BASE_RELATIVE(c.stack_variables[i], current_mem + 16 + c.shadow_space);
		current_mem += round_up_to_nearest(get_variable_size(c.stack_variables[i]), 8);
	}

	for (int i = 0; i < c.regs_size; i++) {
		if (!c.regs[i].merge_into)
			continue;

		var_id address = new_variable_sz(8, 1, 1);
		var_id offset_constant = new_variable_sz(8, 1, 1);

		IR_PUSH_CONSTANT(constant_simple_unsigned(abi_info.size_type, c.regs[i].merge_pos), offset_constant);
		IR_PUSH_ADDRESS_OF(address, c.regs[i].merge_into);
		IR_PUSH_BINARY_OPERATOR(IBO_ADD, address, offset_constant, address);
		IR_PUSH_STORE(c.regs[i].variable, address);
	}

	func->abi_data = malloc(sizeof (struct sysv_data));
	*(struct sysv_data *)func->abi_data = abi_data;
}

static void sysv_ir_function_return(struct function *func, var_id value, struct type *type) {
	if (type == type_simple(ST_VOID))
		return;
	enum parameter_class classes[4];
	struct sysv_data *abi_data = func->abi_data;
	int n_parts = 0;

	classify(type, &n_parts, classes);

	if (n_parts == 1 && classes[0] == CLASS_MEMORY) {
		IR_PUSH_STORE(value, abi_data->ret_address);
	} else if (n_parts == 1 && classes[0] == CLASS_INTEGER) {
		IR_PUSH_SET_REG(value, return_convention[0], 0);
	} else if (n_parts == 2 && classes[0] == CLASS_INTEGER) {
		var_id parts[2];
		split_variable(value, 2, parts);
		
		IR_PUSH_SET_REG(parts[0], return_convention[0], 0);
		IR_PUSH_SET_REG(parts[1], return_convention[1], 0);
	} else if (n_parts == 1 && classes[0] == CLASS_SSE) {
		IR_PUSH_SET_REG(value, 0, 1);
	} else {
		NOTIMP();
	}
}

static void sysv_emit_function_preamble(struct function *func) {
	struct sysv_data *abi_data = func->abi_data;

	if (!abi_data->is_variadic)
		return;

	int position = variable_info[abi_data->reg_save_area].stack_location;
	asm_ins2("movq", R8(REG_RDI), MEM(0 - position, REG_RBP));
	asm_ins2("movq", R8(REG_RSI), MEM(8 - position, REG_RBP));
	asm_ins2("movq", R8(REG_RDX), MEM(16 - position, REG_RBP));
	asm_ins2("movq", R8(REG_RCX), MEM(24 - position, REG_RBP));
	asm_ins2("movq", R8(REG_R8), MEM(32 - position, REG_RBP));
	asm_ins2("movq", R8(REG_R9), MEM(40 - position, REG_RBP));
}

static void sysv_emit_va_start(var_id result, struct function *func) {
	struct sysv_data *abi_data = func->abi_data;
	int gp_offset_offset = builtin_va_list->fields[0].offset;
	int fp_offset_offset = builtin_va_list->fields[1].offset;
	int overflow_arg_area_offset = builtin_va_list->fields[2].offset;
	int reg_save_area_offset = builtin_va_list->fields[3].offset;
	scalar_to_reg(result, REG_RAX);
	asm_ins2("movl", IMM(abi_data->gp_offset), MEM(gp_offset_offset, REG_RAX));
	asm_ins2("movl", IMM(0), MEM(fp_offset_offset, REG_RAX));
	asm_ins2("leaq", MEM(abi_data->overflow_position, REG_RBP), R8(REG_RDI));
	asm_ins2("movq", R8(REG_RDI), MEM(overflow_arg_area_offset, REG_RAX));

	asm_ins2("leaq", MEM(-variable_info[abi_data->reg_save_area].stack_location, REG_RBP), R8(REG_RDI));
	asm_ins2("movq", R8(REG_RDI), MEM(reg_save_area_offset, REG_RAX));
}

static void sysv_emit_va_arg(var_id result, var_id va_list, struct type *type) {
	int n_parts;
	enum parameter_class classes[4];
	classify(type, &n_parts, classes);

	int gp_offset_offset = builtin_va_list->fields[0].offset;
	int reg_save_area_offset = builtin_va_list->fields[3].offset;
	int overflow_arg_area_offset = builtin_va_list->fields[2].offset;
	// 1. Determine whether type may be passed in the registers. If not go to step 7
	scalar_to_reg(va_list, REG_RDI);

	label_id stack_label = register_label(),
		fetch_label = register_label();
	if (classes[0] != CLASS_MEMORY) {
		// 2. Compute num_gp to hold the number of general purpose registers needed
		// to pass type and num_fp to hold the number of floating point registers needed.

		int num_gp = 0;
		//int num_fp = 0;
		for (int i = 0; i < n_parts; i++) {
			if (classes[i] == CLASS_INTEGER)
				num_gp++;
			else
				NOTIMP();
		}

		// 3. Verify whether arguments fit into registers. In the case:
		//     l->gp_offset > 48 − num_gp ∗ 8
		// or
		//     l->fp_offset > 304 − num_fp ∗ 16
		// go to step 7.

		asm_ins2("movl", MEM(gp_offset_offset, REG_RDI), R4(REG_RAX));
		asm_ins2("cmpl", IMM(48 - 8 * num_gp), R4(REG_RAX));
		asm_ins1("ja", IMML_ABS(stack_label, 0));

		// 4. Fetch type from l->reg_save_area with an offset of l->gp_offset
		// and/or l->fp_offset. This may require copying to a temporary loca-
		// tion in case the parameter is passed in different register classes or requires
		// an alignment greater than 8 for general purpose registers and 16 for XMM
		// registers. [Note: Alignment is largely ignored in this implementation]

		// 5. Set:
		// l->gp_offset = l->gp_offset + num_gp ∗ 8
		// l->fp_offset = l->fp_offset + num_fp ∗ 16.
		// 6. Return the fetched type.

		asm_ins2("leal", MEM(num_gp * 8, REG_RAX), R4(REG_RDX));
		asm_ins2("addq", MEM(reg_save_area_offset, REG_RDI), R8(REG_RAX));
		asm_ins2("movl", R4(REG_RDX), MEM(gp_offset_offset, REG_RDI));
		asm_ins1("jmp", IMML_ABS(fetch_label, 0));
	}
	// 7. Align l->overflow_arg_area upwards to a 16 byte boundary if align-
	//ment needed by type exceeds 8 byte boundary. [This is ignored.]
	asm_label(0, stack_label);

	// 8. Fetch type from l->overflow_arg_area.

	asm_ins2("movq", MEM(overflow_arg_area_offset, REG_RDI), R8(REG_RAX));

	// 9. Set l->overflow_arg_area to:
	// l->overflow_arg_area + sizeof(type)
	// 10. Align l->overflow_arg_area upwards to an 8 byte boundary.
	asm_ins2("leaq", MEM(round_up_to_nearest(calculate_size(type), 8), REG_RAX), R8(REG_RDX));
	asm_ins2("movq", R8(REG_RDX), MEM(overflow_arg_area_offset, REG_RDI));

	// 11. Return the fetched type.
	asm_label(0, fetch_label);

	// Address is now in %%rax.
	asm_ins2("movq", R8(REG_RAX), R8(REG_RDI));
	asm_ins2("leaq", MEM(-variable_info[result].stack_location, REG_RBP), R8(REG_RSI));

	codegen_memcpy(calculate_size(type));
}

int sysv_sizeof_simple(enum simple_type type) {
	static const int sizes[ST_COUNT] = {
		[ST_BOOL] = 1,
		[ST_CHAR] = 1, [ST_SCHAR] = 1, [ST_UCHAR] = 1,
		[ST_SHORT] = 2, [ST_USHORT] = 2,
		[ST_INT] = 4, [ST_UINT] = 4,
		[ST_LONG] = 8, [ST_ULONG] = 8,
		[ST_LLONG] = 8, [ST_ULLONG] = 8,
		[ST_FLOAT] = 4,
		[ST_DOUBLE] = 8,
		[ST_LDOUBLE] = 16,
		[ST_VOID] = 0,
	};

	return sizes[type];
}

void abi_init_sysv(void) {
	abi_info.va_list_is_reference = 0;

	abi_info.pointer_type = ST_ULONG;
	abi_info.size_type = ST_ULONG;
	abi_info.wchar_type = ST_INT;

	abi_ir_function_call = sysv_ir_function_call;
	abi_ir_function_new = sysv_ir_function_new;
	abi_ir_function_return = sysv_ir_function_return;
	abi_emit_function_preamble = sysv_emit_function_preamble;
	abi_emit_va_start = sysv_emit_va_start;
	abi_emit_va_start = sysv_emit_va_start;
	abi_emit_va_arg = sysv_emit_va_arg;
	abi_sizeof_simple = sysv_sizeof_simple;

	// Initialize the __builtin_va_list typedef.
	struct symbol_typedef *sym =
		symbols_add_typedef(sv_from_str("__builtin_va_list"));

	struct type *uint = type_simple(ST_UINT);
	struct type *vptr = type_pointer(type_simple(ST_VOID));

	struct field *fields = malloc(sizeof *fields * 4);
	for (int i = 0; i < 4; i++)
		fields[i].bitfield = -1;
	fields[0].type = uint;
	fields[0].name = sv_from_str("gp_offset");
	fields[1].type = uint;
	fields[1].name = sv_from_str("fp_offset");
	fields[2].type = vptr;
	fields[2].name = sv_from_str("overflow_arg_area");
	fields[3].type = vptr;
	fields[3].name = sv_from_str("reg_save_area");

	struct struct_data *struct_data = register_struct();
	*struct_data = (struct struct_data) {
		.name = sv_from_str("<__builtin_va_list_struct>"),
		.is_complete = 1,
		.n = 4,
		.fields = fields
	};

	calculate_offsets(struct_data);

	builtin_va_list = struct_data;

	struct type *struct_type = type_struct(struct_data);

	struct type final_params = {
		.type = TY_ARRAY,
		.array.length = 1,
		.n = 1
	};
	struct type *final = type_create(&final_params, &struct_type);

	sym->data_type = final;

	define_string("__LP64__", "1");
}
