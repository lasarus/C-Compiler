#include "abi.h"
#include "arch/x64.h"
#include "ir/ir.h"
#include "parser/expression_to_ir.h"
#include "types.h"

#include <parser/symbols.h>
#include <codegen/codegen.h>
#include <codegen/registers.h>
#include <arch/calling.h>
#include <common.h>
#include <preprocessor/preprocessor.h>

static const int calling_convention[] = { REG_RCX, REG_RDX, REG_R8, REG_R9 };
static const int shadow_space = 32;

struct ms_data {
	int n_args;

	int is_variadic;

	int returns_address;
	struct node *ret_address;

	struct node *rbx_store, *rdi_store, *rsi_store;
};

static int fits_into_reg(struct type *type) {
	int size = calculate_size(type);
	return size == 1 || size == 2 || size == 4 || size == 8;
}

static struct evaluated_expression ms_expr_call(struct evaluated_expression *callee,
										   int n, struct evaluated_expression arguments[static n]) {
	struct type *argument_types[128];
	struct node *arg_addresses[128];
	struct type *callee_type = type_deref(callee->data_type);
	struct type *return_type = callee_type->children[0];

	for (int i = 0; i < n; i++) {
		arg_addresses[i] = evaluated_expression_to_address(&arguments[i]);
		argument_types[i] = arguments[i].data_type;
	}

	struct node *registers[4];
	int is_floating[4] = { 0 };
	int register_idx = 0;
	int ret_in_register = 0, ret_in_address = 0;
	
	struct node *callee_var = evaluated_expression_to_var(callee);

	if (!type_is_simple(return_type, ST_VOID)) {
		if (fits_into_reg(return_type)) {
			ret_in_register = 1;
		} else {
			registers[0] = ir_allocate(calculate_size(return_type));

			register_idx++;

			ret_in_address = 1;
		}
	}

	int stack_sub = MAX(32, round_up_to_nearest(n * 8, 16));

	struct node *call_stack = ir_allocate_call_stack(-stack_sub - shadow_space);
	int current_mem = 0;
	for (int i = 0; i < n; i++) {
		struct node *reg_to_push = arg_addresses[i];

		if (fits_into_reg(argument_types[i])) {
			reg_to_push = ir_load(arg_addresses[i],
								  calculate_size(argument_types[i]));
		}

		if (register_idx < 4) {
			is_floating[register_idx] = (i < callee_type->n - 1) ? type_is_floating(argument_types[i]) : 0;
			registers[register_idx++] = reg_to_push;
		} else {
			call_stack = ir_store_stack_relative(call_stack, reg_to_push, current_mem + shadow_space);
			current_mem += 8;
		}
	}

	struct node *reg_state = NULL;
	for (int i = 0; i < register_idx; i++) {
		if (is_floating[i])
			reg_state = ir_set_reg(registers[i], reg_state, i, 1);
		else
			reg_state = ir_set_reg(registers[i], reg_state, calling_convention[i], 0);
	}

	struct node *reg_source = NULL;
	ir_call(callee_var, reg_state, call_stack, REG_RBX, &reg_source);

	struct evaluated_expression result;
	if (ret_in_register) {
		struct node *variable = ir_get_reg(reg_source, calculate_size(return_type), REG_RAX, type_is_floating(return_type));
		result = (struct evaluated_expression) {
			.type = EE_VARIABLE,
			.data_type = return_type,
			.variable = variable,
		};
	} else if (ret_in_address) {
		result = (struct evaluated_expression) {
			.type = EE_POINTER,
			.data_type = return_type,
			.pointer = registers[0],
		};
	} else {
		result = (struct evaluated_expression) {
			.type = EE_VOID,
		};
	}

	ir_allocate_call_stack(+stack_sub + shadow_space);

	return result;
}

static void ms_expr_function(struct node *func, struct type *function_type, struct symbol_identifier **args) {
	struct type *return_type = function_type->children[0];
	int n_args = function_type->n - 1;

	struct ms_data abi_data = { 0 };

	struct node *first_block = new_block();
	first_block->type = IR_PROJECT;
	first_block->index = 0;
	node_set_argument(first_block, 0, func);
	ir_block_start(first_block);
	struct node *reg_source = ir_project(func, 1, 0);
	struct node *call_stack = ir_project(func, 2, 0);

	int register_idx = 0;
	if (!type_is_simple(return_type, ST_VOID) && !fits_into_reg(return_type)) {
		abi_data.returns_address = 1;
		abi_data.ret_address = ir_get_reg(reg_source, 8, calling_convention[register_idx++], type_is_floating(return_type));
	}

	if (function_type->function.is_variadic) {
		abi_data.is_variadic = 1;
		abi_data.n_args = n_args;
	}

	abi_data.rbx_store = ir_get_reg(reg_source, 8, REG_RBX, 0);
	abi_data.rdi_store = ir_get_reg(reg_source, 8, REG_RDI, 0);
	abi_data.rsi_store = ir_get_reg(reg_source, 8, REG_RSI, 0);

	int current_mem = 0;

	struct node *inputs[128];

	for (int i = 0; i < n_args; i++) {
		int size = calculate_size(args[i]->parameter.type);
		if (!fits_into_reg(args[i]->parameter.type))
			size = 8;

		if (register_idx < 4) {
			if (type_is_floating(function_type->children[i + 1])) {
				inputs[i] = ir_get_reg(reg_source, size, register_idx++, 1);
			} else {
				inputs[i] = ir_get_reg(reg_source, size, calling_convention[register_idx++], 0);
			}
		} else {
			inputs[i] = ir_load_base_relative(call_stack, current_mem + 16 + shadow_space, size);
			current_mem += 8;
		}
	}

	for (int i = 0; i < n_args; i++) {
		struct symbol_identifier *symbol = args[i];

		struct type *type = symbol->parameter.type;
		struct node *address = ir_allocate(calculate_size(type));

		symbol->type = IDENT_VARIABLE;
		symbol->variable.type = type;
		symbol->variable.ptr = address;

		if (fits_into_reg(function_type->children[i + 1])) {
			ir_store(address, inputs[i]);
		} else {
			ir_copy_memory(address, inputs[i], calculate_size(type));
		}
	}

	func->function.abi_data = ALLOC(abi_data);
}

static void ms_expr_return(struct node *func, struct evaluated_expression *value, struct node **reg_state) {
	struct ms_data *abi_data = func->function.abi_data;

	*reg_state = NULL;
	if (value->type != EE_VOID) {
		if (abi_data->returns_address) {
			struct node *value_address = evaluated_expression_to_address(value);
			ir_copy_memory(abi_data->ret_address, value_address, calculate_size(value->data_type));
		} else {
			struct node *value_var = evaluated_expression_to_var(value);
			*reg_state = ir_set_reg(value_var, *reg_state, 0, type_is_floating(value->data_type));
		}
	}

	*reg_state = ir_set_reg(abi_data->rbx_store, *reg_state, REG_RBX, 0);
	*reg_state = ir_set_reg(abi_data->rdi_store, *reg_state, REG_RDI, 0);
	*reg_state = ir_set_reg(abi_data->rsi_store, *reg_state, REG_RSI, 0);
}

static void ms_emit_function_preamble(struct node *func) {
	struct ms_data *abi_data = func->function.abi_data;

	if (!abi_data->is_variadic)
		return;

	asm_ins2("movq", R8(REG_RCX), MEM(16, REG_RBP));
	asm_ins2("movq", R8(REG_RDX), MEM(24, REG_RBP));
	asm_ins2("movq", R8(REG_R8), MEM(32, REG_RBP));
	asm_ins2("movq", R8(REG_R9), MEM(40, REG_RBP));
}

static void ms_emit_va_start(struct node *result, struct node *func) {
	struct ms_data *abi_data = func->function.abi_data;

	asm_ins2("leaq", MEM(abi_data->n_args * 8 + 16, REG_RBP), R8(REG_RAX));
	scalar_to_reg(result, REG_RDX);
	asm_ins2("movq", R8(REG_RAX), MEM(0, REG_RDX));
}

static void ms_emit_va_arg(struct node *address, struct node *va_list, struct type *type) {
	scalar_to_reg(va_list, REG_RBX); // va_list is a pointer to the actual va_list.
	asm_ins2("movq", MEM(0, REG_RBX), R8(REG_RAX));
	asm_ins2("leaq", MEM(8, REG_RAX), R8(REG_RDX));
	asm_ins2("movq", R8(REG_RDX), MEM(0, REG_RBX));
	asm_ins2("movq", MEM(0, REG_RAX), R8(REG_RAX));
	if (fits_into_reg(type)) {
		scalar_to_reg(address, REG_RSI);
		reg_to_memory(REG_RAX, calculate_size(type));
	} else {
		asm_ins2("movq", R8(REG_RAX), R8(REG_RDI));
		scalar_to_reg(address, REG_RSI);
		codegen_memcpy(calculate_size(type));
	}
}

static int ms_sizeof_simple(enum simple_type type) {
	static const int sizes[ST_COUNT] = {
		[ST_BOOL] = 1,
		[ST_CHAR] = 1, [ST_SCHAR] = 1, [ST_UCHAR] = 1,
		[ST_SHORT] = 2, [ST_USHORT] = 2,
		[ST_INT] = 4, [ST_UINT] = 4,
		[ST_LONG] = 4, [ST_ULONG] = 4,
		[ST_LLONG] = 8, [ST_ULLONG] = 8,
		[ST_FLOAT] = 4,
		[ST_DOUBLE] = 8,
		[ST_LDOUBLE] = 16,
		[ST_VOID] = 0,
	};

	return sizes[type];
}

void abi_init_microsoft(void) {
	abi_info.va_list_is_reference = 1;
	abi_info.pointer_type = ST_ULLONG;
	abi_info.size_type = ST_ULLONG;
	abi_info.wchar_type = ST_USHORT;
	abi_info.ptrdiff_type = ST_LLONG;

	abi_expr_call = ms_expr_call;
	abi_expr_function = ms_expr_function;
	abi_expr_return = ms_expr_return;
	abi_emit_function_preamble = ms_emit_function_preamble;
	abi_emit_va_start = ms_emit_va_start;
	abi_emit_va_start = ms_emit_va_start;
	abi_emit_va_arg = ms_emit_va_arg;
	abi_sizeof_simple = ms_sizeof_simple;

	// Initialize the __builtin_va_list typedef.
	struct symbol_typedef *sym =
		symbols_add_typedef(sv_from_str("__builtin_va_list"));

	sym->data_type = type_pointer(type_simple(ST_VOID));

	define_string("_WIN32", "1");
	define_string("_WIN64", "1");
	define_string("__LLP64__", "1");
}

void abi_init_mingw_workarounds(void) {
	// These are definitions and typedefs that allow for
	// compilation with the mingw libc headers.
	// It is very annoying that they require va_list to be typedeffed.
	define_string("_VA_LIST_DEFINED", "1");
	symbols_add_typedef(sv_from_str("va_list"))->data_type = type_pointer(type_simple(ST_VOID));
	define_string("_crt_va_start", "__builtin_va_start");
	define_string("_crt_va_end", "__builtin_va_end");
	define_string("_crt_va_arg", "__builtin_va_arg");
	define_string("_crt_va_copy", "__builtin_va_copy");
	define_string("_CRTIMP", "");
	define_string("_SECIMP", "");
	define_string("__MINGW_IMPORT", "");
	define_string("__CRT__NO_INLINE", "1");
	define_string("__cdecl", "");
	define_string("__forceinline", "static inline");
	define_string("__inline", "inline");
	define_string("__int8", "int"); // Not typedef to allow for "unsigned __int8".
	define_string("__int16", "short");
	define_string("__int32", "int");
	define_string("__int64", "long long");
	define_string("__cdecl", "");
	define_string("_MSVCRT_", "1");
	define_string("__x86_64", "1");

	codegen_flags.code_model = CODE_MODEL_LARGE;
}
