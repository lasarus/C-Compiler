#ifndef ABI_H
#define ABI_H

#include "parser/symbols.h"
#include <ir/ir.h>
#include <parser/expression_to_ir.h>

struct abi_info {
	int va_list_is_reference;
	enum simple_type pointer_type,
		ptrdiff_type,
		size_type,
		wchar_type;
};

extern struct abi_info abi_info;

void abi_init_sysv(void);
void abi_init_microsoft(void);
void abi_init_mingw_workarounds(void);

extern struct evaluated_expression (*abi_expr_call)(struct evaluated_expression *callee,
													int n, struct evaluated_expression arguments[static n]);

extern void (*abi_expr_function)(struct node *function, struct type *type, struct symbol_identifier **args);
extern void (*abi_expr_return)(struct node *func, struct evaluated_expression *value, struct node **reg_state);

extern void (*abi_emit_function_preamble)(struct node *func);
extern void (*abi_emit_va_start)(struct node *result, struct node *func);
extern void (*abi_emit_va_arg)(struct node *address, struct node *va_list, struct type *type);

extern int (*abi_sizeof_simple)(enum simple_type type);

#endif
