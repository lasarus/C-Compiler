#include "abi.h"
#include "arch/x64.h"
#include "ir/ir.h"
#include "parser/expression_to_ir.h"

#include <parser/symbols.h>
#include <codegen/codegen.h>
#include <codegen/registers.h>
#include <arch/calling.h>
#include <common.h>
#include <preprocessor/preprocessor.h>

static const int calling_convention[] = { REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9 };
static const int return_convention[] = { REG_RAX, REG_RDX };

static struct struct_data *builtin_va_list = NULL;

struct sysv_data {
	int overflow_position;
	int gp_offset;
	int fp_offset;

	int is_variadic;

	int returns_address;
	struct instruction *ret_address;

	struct instruction *rbx_store;
};

struct reg_info {
	int register_idx, is_sse;
	int is_merged;
	int merge_into, merge_pos;
	int size;
};

struct call_info {
	int regs_size;
	struct reg_info *regs;

	int ret_regs_size;
	struct reg_info *ret_regs;

	int stack_variables_size;
	int *stack_variables;

	int returns_address;
	int ret_address_index;

	int gp_offset;

	int rax;

	int shadow_space;
};

static struct call_info get_calling_convention(struct type *function_type, int n_args, struct type **argument_types) {
	struct type *return_type = function_type->children[0];

	static int regs_size = 0, regs_cap = 0;
	regs_size = 0;
	struct reg_info *regs = NULL;

	static int ret_regs_size = 0, ret_regs_cap = 0;
	ret_regs_size = 0;
	struct reg_info *ret_regs = NULL;

	static int stack_variables_size = 0, stack_variables_cap = 0;
	stack_variables_size = 0;
	int *stack_variables = NULL;

	int returns_address = 0;
	int ret_address_index = 0;

	int gp_offset = 0;
	int shadow_space = 0;

	int current_gp_reg = 0, current_sse_reg = 0;
	const int max_gp_reg = 6, max_sse_reg = 8;
	if (!type_is_simple(return_type, ST_VOID)) {
		int n_parts;
		enum parameter_class classes[4];
		classify(return_type, &n_parts, classes);

		if (classes[0] == CLASS_MEMORY) {
			returns_address = 1;
			ret_address_index = 0;

			current_gp_reg = 1;
			ADD_ELEMENT(regs_size, regs_cap, regs) = (struct reg_info) {
				.register_idx = calling_convention[0],
				.size = 8,
			};
		} else {
			int gp_idx = 0, ssa_idx = 0;
			for (int j = 0; j < n_parts; j++) {
				if (classes[j] == CLASS_SSE || classes[j] == CLASS_SSEUP) {
					ADD_ELEMENT(ret_regs_size, ret_regs_cap, ret_regs) = (struct reg_info) {
						.is_sse = 1,
						.register_idx = ssa_idx++,
						.size = 8,
					};
				} else {
					ADD_ELEMENT(ret_regs_size, ret_regs_cap, ret_regs) = (struct reg_info) {
						.is_sse = 0,
						.register_idx = return_convention[gp_idx++],
						.size = 8,
					};
				}
			}
		}
	}

	for (int i = 0; i < n_args; i++) {
		struct type *type = argument_types[i];
		int n_parts;
		enum parameter_class classes[4];

		int argument_size = calculate_size(type);
		classify(type, &n_parts, classes);

		int n_gp_regs = 0, n_sse_regs = 0;
		for (int j = 0; j < n_parts; j++) {
			if (classes[j] == CLASS_MEMORY)
				break;
			else if (classes[j] == CLASS_SSE || classes[j] == CLASS_SSEUP)
				n_sse_regs++;
			else if (classes[j] == CLASS_INTEGER)
				n_gp_regs++;
			else
				NOTIMP();
		}

		const int is_memory = classes[0] == CLASS_MEMORY ||
			current_gp_reg + n_gp_regs > max_gp_reg ||
			current_sse_reg + n_sse_regs > max_sse_reg;
		if (is_memory) {
			ADD_ELEMENT(stack_variables_size, stack_variables_cap, stack_variables) = i;
		} else {
			for (int j = 0; j < n_parts; j++) {
				int part_size = MIN(argument_size - j * 8, 8);

				if (classes[j] == CLASS_SSE || classes[j] == CLASS_SSEUP) {
					ADD_ELEMENT(regs_size, regs_cap, regs) = (struct reg_info) {
						.is_sse = 1,
						.register_idx = current_sse_reg++,
						.is_merged = 1,
						.merge_into = i,
						.merge_pos = j * 8,
						.size = part_size,
					};
				} else {
					ADD_ELEMENT(regs_size, regs_cap, regs) = (struct reg_info) {
						.is_sse = 0,
						.register_idx = calling_convention[current_gp_reg++],
						.is_merged = 1,
						.merge_into = i,
						.merge_pos = j * 8,
						.size = part_size,
					};
				}
			}
		}
	}

	gp_offset = current_gp_reg * 8;

	return (struct call_info) {
		.regs_size = regs_size,
		.regs = regs,

		.ret_regs_size = ret_regs_size,
		.ret_regs = ret_regs,

		.stack_variables_size = stack_variables_size,
		.stack_variables = stack_variables,

		.returns_address = returns_address,
		.ret_address_index = ret_address_index,

		.gp_offset = gp_offset,

		.rax = current_sse_reg,

		.shadow_space = shadow_space
	};
}

static void split_variable(struct instruction *variable_address, int n_parts, struct instruction *parts[n_parts]) {
	for (int i = 0; i < n_parts; i++)
		parts[i] = ir_load_part_address(variable_address, i * 8, 8);
}

static struct evaluated_expression sysv_expr_call(struct evaluated_expression *callee,
										   int n, struct evaluated_expression arguments[static n]) {
	struct type *argument_types[128];
	struct instruction *arg_addresses[128];
	struct instruction *reg_variables[sizeof calling_convention / sizeof *calling_convention];
	struct instruction *ret_reg_variables[sizeof return_convention / sizeof *return_convention];
	struct type *callee_type = type_deref(callee->data_type);
	struct type *return_type = callee_type->children[0];

	for (int i = 0; i < n; i++) {
		arg_addresses[i] = evaluated_expression_to_address(&arguments[i]);
		argument_types[i] = arguments[i].data_type;
	}

	struct call_info c = get_calling_convention(callee_type, n, argument_types);

	struct instruction *callee_var = evaluated_expression_to_var(callee);
	struct instruction *ret_address;

	if (c.returns_address) {
		ret_address = ir_allocate(calculate_size(return_type));
		reg_variables[c.ret_address_index] = ret_address;
	}

	struct instruction *rax_constant = NULL;
	if (c.rax != -1) {
		rax_constant = ir_constant(constant_simple_unsigned(abi_info.size_type, c.rax));
	}

	for (int i = 0; i < c.regs_size; i++) {
		if (!c.regs[i].is_merged)
			continue;

		reg_variables[i] = ir_load_part_address(arg_addresses[c.regs[i].merge_into], c.regs[i].merge_pos, c.regs[i].size);
	}

	int total_mem_needed = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		total_mem_needed += round_up_to_nearest(calculate_size(argument_types[c.stack_variables[i]]), 8);
	}
	int stack_sub = round_up_to_nearest(total_mem_needed, 16);

	ir_modify_stack_pointer(-stack_sub - c.shadow_space);

	int current_mem = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		int size = calculate_size(argument_types[c.stack_variables[i]]);
		ir_store_stack_relative_address(arg_addresses[c.stack_variables[i]], current_mem + c.shadow_space, size);
		current_mem += round_up_to_nearest(size, 8);
	}

	for (int i = 0; i < c.regs_size; i++)
		ir_set_reg(reg_variables[i], c.regs[i].register_idx, c.regs[i].is_sse);

	if (c.rax != -1)
		ir_set_reg(rax_constant, REG_RAX, 0);

	ir_call(callee_var, REG_RBX);

	for (int i = 0; i < c.ret_regs_size; i++)
		ret_reg_variables[i] = ir_get_reg(c.ret_regs[i].size, c.ret_regs[i].register_idx, c.ret_regs[i].is_sse);

	struct evaluated_expression result;

	if (c.returns_address) {
		result = (struct evaluated_expression) {
			.type = EE_POINTER,
			.data_type = return_type,
			.pointer = ret_address,
		};
	} else if (c.ret_regs_size == 1) {
		struct instruction *variable = ir_cast_int(ret_reg_variables[0], calculate_size(return_type), 0);
		result = (struct evaluated_expression) {
			.type = EE_VARIABLE,
			.data_type = return_type,
			.variable = variable,
		};
	} else if (c.ret_regs_size == 2) {
		struct instruction *address = ir_allocate(calculate_size(return_type));
		ir_store_part_address(address, ret_reg_variables[0], 0);
		ir_store_part_address(address, ret_reg_variables[1], 8);
		result = (struct evaluated_expression) {
			.type = EE_POINTER,
			.data_type = return_type,
			.variable = address,
		};
	} else {
		result = (struct evaluated_expression) {
			.type = EE_VOID,
		};
	}

	ir_modify_stack_pointer(+stack_sub + c.shadow_space);

	return result;
}

static void sysv_expr_function(struct type *type, struct symbol_identifier **args, const char *name, int is_global) {
	int n_args = type->n - 1;
	struct instruction *reg_variables[sizeof calling_convention / sizeof *calling_convention];

	struct function *func = &ADD_ELEMENT(ir.size, ir.cap, ir.functions);
	*func = (struct function) {
		.name = name,
		.is_global = is_global,
	};

	struct sysv_data abi_data = { 0 };

	ir_block_start(new_block());

	struct call_info c = get_calling_convention(type, n_args, type->children + 1);

	abi_data.rbx_store = ir_get_reg(8, REG_RBX, 0);

	for (int i = 0; i < c.regs_size; i++)
		reg_variables[i] = ir_get_reg(c.regs[i].size, c.regs[i].register_idx, c.regs[i].is_sse);
	
	if (c.returns_address) {
		abi_data.ret_address = reg_variables[c.ret_address_index];
		abi_data.returns_address = 1;
	}

	struct instruction *arg_addresses[128];
	for (int i = 0; i < type->n - 1; i++) {
		struct symbol_identifier *symbol = args[i];

		struct type *type = symbol->parameter.type;
		int size = calculate_size(type);
		struct instruction *address = ir_allocate(size);

		symbol->type = IDENT_VARIABLE;
		symbol->variable.type = type;
		symbol->variable.ptr = address;

		arg_addresses[i] = address;
	}

	int current_mem = 0;
	int total_mem_needed = 0;
	for (int i = 0; i < c.stack_variables_size; i++) {
		int idx = c.stack_variables[i];
		struct instruction *address = arg_addresses[idx];
		struct type *arg_type = type->children[idx + 1];
		int size = calculate_size(arg_type);

		ir_load_base_relative_address(address, current_mem + 16 + c.shadow_space, size);
		current_mem += round_up_to_nearest(size, 8);
		total_mem_needed += round_up_to_nearest(size, 8);
	}

	for (int i = 0; i < c.regs_size; i++) {
		if (!c.regs[i].is_merged)
			continue;

		struct instruction *address = arg_addresses[c.regs[i].merge_into];

		ir_store_part_address(address, reg_variables[i], c.regs[i].merge_pos);
	}

	if (type->function.is_variadic) {
		abi_data.is_variadic = 1;
        // Sized according to Figure 3.33 in sysV AMD64 ABI:
		ir_allocate_preamble(304);
		abi_data.gp_offset = c.gp_offset;
		abi_data.overflow_position = total_mem_needed + 16;
	}

	func->abi_data = ALLOC(abi_data);
}

static void sysv_expr_return(struct function *func, struct evaluated_expression *value) {
	enum parameter_class classes[4];
	struct sysv_data *abi_data = func->abi_data;
	int n_parts = 0;

	if (value->type == EE_VOID)
		goto unclobber;

	classify(value->data_type, &n_parts, classes);

	if (n_parts == 1 && classes[0] == CLASS_MEMORY) {
		struct instruction *value_address = evaluated_expression_to_address(value);
		ir_copy_memory(abi_data->ret_address, value_address, calculate_size(value->data_type));
	} else if (n_parts == 1 && classes[0] == CLASS_INTEGER) {
		struct instruction *value_var = evaluated_expression_to_var(value);
		ir_set_reg(value_var, return_convention[0], 0);
	} else if (n_parts == 2 && classes[0] == CLASS_INTEGER) {
		struct instruction *value_address = evaluated_expression_to_address(value);
		struct instruction *parts[2];
		split_variable(value_address, 2, parts);
		
		ir_set_reg(parts[0], return_convention[0], 0);
		ir_set_reg(parts[1], return_convention[1], 0);
	} else if (n_parts == 1 && classes[0] == CLASS_SSE) {
		struct instruction *value_var = evaluated_expression_to_var(value);
		ir_set_reg(value_var, 0, 1);
	} else {
		NOTIMP();
	}

unclobber:
	ir_set_reg(abi_data->rbx_store, REG_RBX, 0);
}

static void sysv_emit_function_preamble(struct function *func) {
	struct sysv_data *abi_data = func->abi_data;

	if (!abi_data->is_variadic)
		return;

	int position = codegen_get_alloc_preamble();
	asm_ins2("movq", R8(REG_RDI), MEM(0 - position, REG_RBP));
	asm_ins2("movq", R8(REG_RSI), MEM(8 - position, REG_RBP));
	asm_ins2("movq", R8(REG_RDX), MEM(16 - position, REG_RBP));
	asm_ins2("movq", R8(REG_RCX), MEM(24 - position, REG_RBP));
	asm_ins2("movq", R8(REG_R8), MEM(32 - position, REG_RBP));
	asm_ins2("movq", R8(REG_R9), MEM(40 - position, REG_RBP));
}

static void sysv_emit_va_start(struct instruction *result, struct function *func) {
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

	int preamble_ptr = codegen_get_alloc_preamble();
	asm_ins2("leaq", MEM(-preamble_ptr, REG_RBP), R8(REG_RDI));
	asm_ins2("movq", R8(REG_RDI), MEM(reg_save_area_offset, REG_RAX));
}

static void sysv_emit_va_arg(struct instruction *address, struct instruction *va_list, struct type *type) {
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
	scalar_to_reg(address, REG_RSI);

	codegen_memcpy(calculate_size(type));
}

static int sysv_sizeof_simple(enum simple_type type) {
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
	abi_info.ptrdiff_type = ST_LONG;

	abi_expr_call = sysv_expr_call;
	abi_expr_function= sysv_expr_function;

	abi_expr_return = sysv_expr_return;
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

	struct field *fields = cc_malloc(sizeof *fields * 4);
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
