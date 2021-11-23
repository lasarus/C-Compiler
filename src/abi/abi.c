#include "abi.h"

struct abi_info abi_info;

void (*abi_ir_function_call)(var_id result, var_id func_var, struct type *function_type, int n_args, struct type **argument_types, var_id *args);
void (*abi_ir_function_new)(struct type *function_type, var_id *args, const char *name, int is_global);
void (*abi_ir_function_return)(struct function *func, var_id value, struct type *type);

void (*abi_emit_function_preamble)(struct function *func);
void (*abi_emit_va_start)(var_id result, struct function *func);
void (*abi_emit_va_arg)(var_id result, var_id va_list, struct type *type);
