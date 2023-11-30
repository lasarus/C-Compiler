#include "abi.h"

struct abi_info abi_info;

struct evaluated_expression (*abi_expr_call)(struct evaluated_expression *callee,
											 int n, struct evaluated_expression arguments[n]);
void (*abi_expr_function)(struct type *type, struct symbol_identifier **args, const char *name, int is_global);
void (*abi_expr_return)(struct function *func, struct evaluated_expression *value);

void (*abi_emit_function_preamble)(struct function *func);
void (*abi_emit_va_start)(struct node *result, struct function *func);
void (*abi_emit_va_arg)(struct node *result, struct node *va_list, struct type *type);

int (*abi_sizeof_simple)(enum simple_type type);
