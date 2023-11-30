#ifndef EXPRESSION_TO_IR
#define EXPRESSION_TO_IR

#include "expression.h"

#include <ir/ir.h>

struct instruction *expression_to_ir(struct expr *expr);
struct instruction *expression_to_ir_clear_temp(struct expr *expr);

struct instruction *expression_to_size_t(struct expr *expr);
struct instruction *expression_to_int(struct expr *expr);
void expression_to_void(struct expr *expr);
void expression_to_address(struct expr *expr, struct instruction *address);

struct evaluated_expression {
	enum {
		EE_VOID,
		EE_CONSTANT,
		EE_VARIABLE,
		EE_POINTER,
		EE_BITFIELD_POINTER,
	} type;

	int is_lvalue; // only for EE_POINTER and EE_BITFIELD_POINTER.

	struct type *data_type;

	union {
		struct constant constant;
		struct instruction *variable;
		struct instruction *pointer;

		struct {
			struct instruction *pointer;
			int bitfield, offset, sign_extend;
		} bitfield_pointer;
	};
};

struct evaluated_expression expression_evaluate(struct expr *expr);
struct evaluated_expression evaluated_expression_cast(struct evaluated_expression *operand, struct type *resulting_type);
struct instruction *evaluated_expression_to_var(struct evaluated_expression *evaluated_expression);
struct instruction *evaluated_expression_to_address(struct evaluated_expression *evaluated_expression);

#endif
