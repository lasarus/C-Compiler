#include "abi.h"

struct abi_info abi_info;

struct evaluated_expression (*abi_expr_call)(struct evaluated_expression *callee,
											 int n, struct evaluated_expression arguments[n]);
void (*abi_expr_function)(struct node *function, struct type *type, struct symbol_identifier **args);
void (*abi_expr_return)(struct node *func, struct evaluated_expression *value, struct node **reg_state);

void (*abi_emit_function_preamble)(struct node *func);
void (*abi_emit_va_start)(struct node *result, struct node *func);
void (*abi_emit_va_arg)(struct node *result, struct node *va_list, struct type *type);

int (*abi_sizeof_simple)(enum simple_type type);
