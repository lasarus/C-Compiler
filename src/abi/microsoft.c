#include "abi.h"

#include <parser/symbols.h>
#include <codegen/codegen.h>
#include <codegen/registers.h>
#include <arch/calling.h>
#include <common.h>
#include <preprocessor/macro_expander.h>

static const int calling_convention[] = { REG_RCX, REG_RDX, REG_R8, REG_R9 };
//static const int return_convention[] = { REG_RAX };
static const int shadow_space = 32;

struct ms_data {
	int n_args;

	int is_variadic;

	int returns_address;
	var_id ret_address;

	var_id rbx_store, rdi_store, rsi_store;
};

static int fits_into_reg(struct type *type) {
	int size = calculate_size(type);
	return size == 1 || size == 2 || size == 4 || size == 8;
}

static void ms_ir_function_call(var_id result, var_id func_var, struct type *type, int n_args, struct type **argument_types, var_id *args) {
	struct type *return_type = type->children[0];

	var_id registers[4];
	int is_floating[4] = { 0 };
	int register_idx = 0;
	int ret_in_register = 0;

	if (!type_is_simple(return_type, ST_VOID)) {
		if (fits_into_reg(return_type)) {
			ret_in_register = 1;
		} else {
			registers[0] = new_variable_sz(8, 1, 0);
			IR_PUSH_ADDRESS_OF(registers[0], result);

			register_idx++;
		}
	}

	int stack_sub = MAX(32, round_up_to_nearest(n_args * 8, 16));

	IR_PUSH_MODIFY_STACK_POINTER(-stack_sub - shadow_space);
	int current_mem = 0;
	for (int i = 0; i < n_args; i++) {
		var_id reg_to_push = args[i];

		if (!fits_into_reg(argument_types[i])) {
			reg_to_push = new_variable_sz(8, 1, 1);
			IR_PUSH_ADDRESS_OF(reg_to_push, args[i]);
		}

		if (register_idx < 4) {
			is_floating[register_idx] = (i < type->n - 1) ? type_is_floating(argument_types[i]) : 0;
			registers[register_idx++] = reg_to_push;
		} else {
			IR_PUSH_STORE_STACK_RELATIVE(current_mem + shadow_space, reg_to_push);
			current_mem += 8;
		}
	}

	for (int i = 0; i < register_idx; i++) {
		if (is_floating[i])
			IR_PUSH_SET_REG(registers[i], i, 1);
		else
			IR_PUSH_SET_REG(registers[i], calling_convention[i], 0);
	}

	IR_PUSH_CALL(func_var, REG_RBX);

	if (ret_in_register)
		IR_PUSH_GET_REG(result, REG_RAX, type_is_floating(return_type));

	IR_PUSH_MODIFY_STACK_POINTER(+stack_sub + shadow_space);
}

static void ms_ir_function_new(struct type *type, var_id *args, const char *name, int is_global) {
	struct type *return_type = type->children[0];
	int n_args = type->n - 1;

	struct function *func = &ADD_ELEMENT(ir.size, ir.cap, ir.functions);
	*func = (struct function) {
		.name = name,
		.is_global = is_global,
	};

	struct ms_data abi_data = { 0 };

	ir_block_start(new_block());

	int register_idx = 0;
	if (!type_is_simple(return_type, ST_VOID) && !fits_into_reg(return_type)) {
		abi_data.returns_address = 1;
		abi_data.ret_address = new_variable_sz(8, 1, 0);

		IR_PUSH_GET_REG(abi_data.ret_address, calling_convention[register_idx++], 0);
	}

	if (type->function.is_variadic) {
		abi_data.is_variadic = 1;
		abi_data.n_args = n_args;
	}

	abi_data.rbx_store = new_variable_sz(8, 1, 0);
	abi_data.rdi_store = new_variable_sz(8, 1, 0);
	abi_data.rsi_store = new_variable_sz(8, 1, 0);
	IR_PUSH_GET_REG(abi_data.rbx_store, REG_RBX, 0);
	IR_PUSH_GET_REG(abi_data.rdi_store, REG_RDI, 0);
	IR_PUSH_GET_REG(abi_data.rsi_store, REG_RSI, 0);

	static int loads_cap = 0;
	struct load_pair {
		var_id from, to;
	};
	int loads_size = 0;
	struct load_pair *loads = NULL;
	
	int current_mem = 0;

	for (int i = 0; i < n_args; i++) {
		var_id reg_to_push = args[i];

		if (!fits_into_reg(type->children[i + 1])) {
			reg_to_push = new_variable_sz(8, 1, 1);

			ADD_ELEMENT(loads_size, loads_cap, loads) = (struct load_pair) {
				reg_to_push, args[i]
			};
		}

		if (register_idx < 4) {
			if (type_is_floating(type->children[i + 1]))
				IR_PUSH_GET_REG(reg_to_push, register_idx++, 1);
			else
				IR_PUSH_GET_REG(reg_to_push, calling_convention[register_idx++], 0);
		} else {
			IR_PUSH_LOAD_BASE_RELATIVE(reg_to_push, current_mem + 16 + shadow_space);
			current_mem += 8;
		}
	}

	for (int i = 0; i < loads_size; i++) {
		IR_PUSH_LOAD(loads[i].to, loads[i].from);
	}

	func->abi_data = cc_malloc(sizeof (struct ms_data));
	*(struct ms_data *)func->abi_data = abi_data;
}

static void ms_ir_function_return(struct function *func, var_id value, struct type *type) {
	struct ms_data *abi_data = func->abi_data;

	if (type == type_simple(ST_VOID))
		goto unclobber;

	if (abi_data->returns_address) {
		IR_PUSH_STORE(value, abi_data->ret_address);
	} else {
		IR_PUSH_SET_REG(value, 0, type_is_floating(type));
	}

unclobber:
	IR_PUSH_SET_REG(abi_data->rbx_store, REG_RBX, 0);
	IR_PUSH_SET_REG(abi_data->rdi_store, REG_RDI, 0);
	IR_PUSH_SET_REG(abi_data->rsi_store, REG_RSI, 0);
}

static void ms_emit_function_preamble(struct function *func) {
	struct ms_data *abi_data = func->abi_data;

	if (!abi_data->is_variadic)
		return;

	asm_ins2("movq", R8(REG_RCX), MEM(16, REG_RBP));
	asm_ins2("movq", R8(REG_RDX), MEM(24, REG_RBP));
	asm_ins2("movq", R8(REG_R8), MEM(32, REG_RBP));
	asm_ins2("movq", R8(REG_R9), MEM(40, REG_RBP));
}

static void ms_emit_va_start(var_id result, struct function *func) {
	struct ms_data *abi_data = func->abi_data;

	asm_ins2("leaq", MEM(abi_data->n_args * 8 + 16, REG_RBP), R8(REG_RAX));
	scalar_to_reg(result, REG_RDX);
	asm_ins2("movq", R8(REG_RAX), MEM(0, REG_RDX));
}

static void ms_emit_va_arg(var_id result, var_id va_list, struct type *type) {
	(void)result, (void)va_list, (void)type;

	scalar_to_reg(va_list, REG_RBX); // va_list is a pointer to the actual va_list.
	asm_ins2("movq", MEM(0, REG_RBX), R8(REG_RAX));
	asm_ins2("leaq", MEM(8, REG_RAX), R8(REG_RDX));
	asm_ins2("movq", R8(REG_RDX), MEM(0, REG_RBX));
	asm_ins2("movq", MEM(0, REG_RAX), R8(REG_RAX));
	if (fits_into_reg(type)) {
		reg_to_scalar(REG_RAX, result);
	} else {
		asm_ins2("movq", R8(REG_RAX), R8(REG_RDI));
		asm_ins2("leaq", MEM(-variable_info[result].stack_location, REG_RBP), R8(REG_RSI));
		codegen_memcpy(get_variable_size(result));
	}
}

int ms_sizeof_simple(enum simple_type type) {
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

	abi_ir_function_call = ms_ir_function_call;
	abi_ir_function_new = ms_ir_function_new;
	abi_ir_function_return = ms_ir_function_return;
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
}
