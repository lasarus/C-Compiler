#include "expression_to_ir.h"
#include "arch/x64.h"
#include "debug.h"
#include "ir/ir.h"
#include "ir/operators.h"
#include "parser/expression.h"
#include "parser/parser.h"
#include "types.h"

#include <abi/abi.h>
#include <common.h>

#include <assert.h>

static int ir_from_type_and_op(struct type *type, enum operator_type op) {
	int sign = 0;
	if (type_is_floating(type)) {
		switch (op) {
		case OP_ADD: return IR_FLT_ADD;
		case OP_SUB: return IR_FLT_SUB;
		case OP_MUL: return IR_FLT_MUL;
		case OP_DIV: return IR_FLT_DIV;
		case OP_LESS: return IR_FLT_LESS;
		case OP_GREATER: return IR_FLT_GREATER;
		case OP_LESS_EQ: return IR_FLT_LESS_EQ;
		case OP_GREATER_EQ: return IR_FLT_GREATER_EQ;
		case OP_EQUAL: return IR_FLT_EQUAL;
		case OP_NOT_EQUAL: return IR_FLT_NOT_EQUAL;
		default: NOTIMP();
		}
	} else if (type->type == TY_SIMPLE) {
		sign = is_signed(type->simple);
	} else if (type->type == TY_POINTER) {
		sign = 0;
	} else {
		NOTIMP();
	}

	switch (op) {
	case OP_ADD: return IR_ADD;
	case OP_SUB: return IR_SUB;
	case OP_MUL: return sign ? IR_IMUL : IR_MUL;
	case OP_DIV: return sign ? IR_IDIV : IR_DIV;
	case OP_MOD: return sign ? IR_IMOD : IR_MOD;
	case OP_BXOR: return IR_BXOR;
	case OP_BOR: return IR_BOR;
	case OP_BAND: return IR_BAND;
	case OP_LSHIFT: return IR_LSHIFT;
	case OP_RSHIFT: return sign ? IR_IRSHIFT : IR_RSHIFT;
	case OP_LESS: return sign ? IR_ILESS : IR_LESS;
	case OP_GREATER: return sign ? IR_IGREATER : IR_GREATER;
	case OP_LESS_EQ: return sign ? IR_ILESS_EQ : IR_LESS_EQ;
	case OP_GREATER_EQ: return sign ? IR_IGREATER_EQ : IR_GREATER_EQ;
	case OP_EQUAL: return IR_EQUAL;
	case OP_NOT_EQUAL: return IR_NOT_EQUAL;
	default: NOTIMP();
	}
}

struct instruction *evaluated_expression_to_address(struct evaluated_expression *evaluated_expression) {
	switch (evaluated_expression->type) {
	case EE_BITFIELD_POINTER: {
		struct instruction *bitfield_var = evaluated_expression_to_var(evaluated_expression);
		struct instruction *address = ir_allocate(calculate_size(evaluated_expression->data_type));
		ir_store(address, bitfield_var);
		return address;
	}

	case EE_CONSTANT: {
		struct constant constant = evaluated_expression->constant;
		if (constant.type == CONSTANT_LABEL || constant.type == CONSTANT_TYPE_POINTER) {
			if (constant.type == CONSTANT_TYPE_POINTER) {
				constant.type = CONSTANT_TYPE;
			}

			return ir_constant(constant);
		} else {
			// Allocate memory, and push the constant to it.
			struct instruction *address = ir_allocate(calculate_size(evaluated_expression->data_type));
			ir_write_constant_to_address(constant, address);
			return address;
		}
	}

	case EE_POINTER:
		return evaluated_expression->pointer;

	case EE_VARIABLE: {
		struct instruction *address = ir_allocate(calculate_size(evaluated_expression->data_type));
		ir_store(address, evaluated_expression->variable);
		return address;
	}

	case EE_VOID:
		NOTIMP();
		break;

	default:
		NOTIMP();
	}
}

struct instruction *evaluated_expression_to_var(struct evaluated_expression *evaluated_expression) {
	switch (evaluated_expression->type) {
	case EE_BITFIELD_POINTER: {
		struct instruction *var = ir_load(evaluated_expression->bitfield_pointer.pointer,
							 calculate_size(evaluated_expression->data_type));

		return ir_get_bits(var, evaluated_expression->bitfield_pointer.offset,
						   evaluated_expression->bitfield_pointer.bitfield,
						   evaluated_expression->bitfield_pointer.sign_extend);
	}

	case EE_CONSTANT:
		return ir_constant(evaluated_expression->constant);

	case EE_POINTER:
		return ir_load(evaluated_expression->pointer,
					   calculate_size(evaluated_expression->data_type));

	case EE_VARIABLE:
		return evaluated_expression->variable;

	default:
		NOTIMP();
	}
}

static struct instruction *variable_cast(struct instruction *operand_var, struct type *operand_type, struct type *resulting_type) {
	struct instruction *res = operand_var;

	if (operand_type == resulting_type)
		return operand_var;

	if (type_is_simple(resulting_type, ST_BOOL)) {
		res = ir_bool_cast(operand_var);
	} else if ((type_is_integer(resulting_type) || type_is_pointer(resulting_type)) &&
			   (type_is_integer(operand_type) || type_is_pointer(operand_type))) {
		if (calculate_size(resulting_type) != calculate_size(operand_type)) {
			int sign_extend = type_is_integer(operand_type) && is_signed(operand_type->simple);
			res = ir_cast_int(operand_var, calculate_size(resulting_type), sign_extend);
		}
	} else if(type_is_floating(resulting_type) && type_is_floating(operand_type)) {
		res = ir_cast_float(operand_var, calculate_size(resulting_type));
	} else if(type_is_floating(resulting_type) && type_is_integer(operand_type)) {
		res = ir_cast_int_to_float(operand_var, calculate_size(resulting_type), is_signed(operand_type->simple));
	} else if(type_is_integer(resulting_type) && type_is_floating(operand_type)) {
		res = ir_cast_float_to_int(operand_var, calculate_size(resulting_type));
	}

	return res;
}

struct evaluated_expression evaluated_expression_cast(struct evaluated_expression *operand, struct type *resulting_type) {
	if (operand->data_type == resulting_type)
		return *operand;
	if (type_is_simple(resulting_type, ST_VOID)) {
		return (struct evaluated_expression) {
			.type = EE_VOID
		};
	}

	struct instruction *operand_var = evaluated_expression_to_var(operand);
	struct instruction *res = variable_cast(operand_var, operand->data_type, resulting_type);

	return (struct evaluated_expression) {
		.type = EE_VARIABLE,
		.data_type = resulting_type,
		.variable = res
	};
}

static void assign_to_ee(struct evaluated_expression *lhs, struct instruction *rhs_var) {
	switch (lhs->type) {
	case EE_POINTER:
		ir_store(lhs->pointer, rhs_var);
		break;

	case EE_BITFIELD_POINTER: {
		struct instruction *prev = ir_load(lhs->bitfield_pointer.pointer, rhs_var->size);

		struct instruction *new = ir_set_bits(prev, rhs_var, lhs->bitfield_pointer.offset, lhs->bitfield_pointer.bitfield);

		ir_store(lhs->bitfield_pointer.pointer, new);
	} break;
		
	default: NOTIMP();
	}
}

static void assign_ee_to_ee(struct evaluated_expression *lhs, struct evaluated_expression *rhs) {
	assert(lhs->data_type == rhs->data_type);
	if (calculate_size(rhs->data_type) <= 8) { // If fits into variable, take shortcut.
		struct instruction *rhs_var = evaluated_expression_to_var(rhs);
		assign_to_ee(lhs, rhs_var);
	} else {
		// Otherwise copy address.
		struct instruction *rhs_adress = evaluated_expression_to_address(rhs);
		switch (lhs->type) {
		case EE_POINTER:
			ir_copy_memory(lhs->pointer, rhs_adress, calculate_size(lhs->data_type));
			break;
		default: NOTIMP();
		}
	}
}

// The following functions are called in expression_evaluate.
static struct evaluated_expression evaluate_indirection(struct expr *expr) {
	struct evaluated_expression rhs = expression_evaluate(expr->args[0]);

	switch (rhs.type) {
	case EE_VARIABLE:
		return (struct evaluated_expression) {
			.type = EE_POINTER,
			.is_lvalue = 1,
			.pointer = rhs.variable
		};

	case EE_POINTER: {
		struct instruction *value = ir_load(rhs.pointer, 8);

		return (struct evaluated_expression) {
			.type = EE_POINTER,
			.is_lvalue = 1,
			.pointer = value
		};
	}

	case EE_CONSTANT: // For example &((type *)0)->member)
		if (rhs.constant.type == CONSTANT_TYPE) {
			rhs.constant.type = CONSTANT_TYPE_POINTER;
		}

		return (struct evaluated_expression) {
			.type = EE_CONSTANT,
			.is_lvalue = 1,
			.constant = rhs.constant
		};
		break;

	default:
		ICE("Invalid evaluated type %d %s:%d", rhs.type, expr->pos.path, expr->pos.line);
	}
}

static struct evaluated_expression evaluate_symbol(struct expr *expr) {
	assert(expr->symbol->type == IDENT_VARIABLE);
	return (struct evaluated_expression) {
		.type = EE_POINTER,
		.is_lvalue = 1,
		.pointer = expr->symbol->variable.ptr
	};
}

static struct evaluated_expression evaluate_variable(struct expr *expr) {
	return (struct evaluated_expression) {
		.type = EE_VARIABLE,
		.is_lvalue = 1,
		.pointer = expr->variable.id
	};
}

static struct evaluated_expression evaluate_call(struct expr *expr) {
	struct evaluated_expression callee = expression_evaluate(expr->call.callee);

	struct type *signature = type_deref(callee.data_type);

	struct evaluated_expression arguments[128];
	for (int i = 0; i < expr->call.n_args; i++) {
		struct evaluated_expression argument = expression_evaluate(expr->call.args[i]);

		if (i + 1 < signature->n)
			argument = evaluated_expression_cast(&argument, signature->children[i + 1]);

		arguments[i] = argument;
	}

	return abi_expr_call(&callee, expr->call.n_args, arguments);
}

static struct evaluated_expression evaluate_constant(struct expr *expr) {
	struct constant *constant = &expr->constant;
	// I don't fully understand the logic of this either.
	switch (constant->type) {
	case CONSTANT_TYPE:
	case CONSTANT_LABEL_POINTER:
		return (struct evaluated_expression) {
			.type = EE_CONSTANT,
			.constant = *constant,
		};

	case CONSTANT_LABEL: {
		constant->type = CONSTANT_LABEL_POINTER;

		struct instruction *pointer = ir_constant(*constant);

		return (struct evaluated_expression) {
			.type = EE_POINTER,
			.is_lvalue = 1,
			.pointer = pointer,
		};
	}

	default: NOTIMP();
	}
}

static struct evaluated_expression evaluate_pointer_arithmetic(struct expr *expr) {
	struct evaluated_expression pointer = expression_evaluate(expr->args[0]),
		index = expression_evaluate(expr->args[1]),
		size = expression_evaluate(type_sizeof(type_deref(expr->args[0]->data_type)));

	index = evaluated_expression_cast(&index, type_simple(abi_info.pointer_type));

	struct instruction *pointer_var = evaluated_expression_to_var(&pointer),
		*index_var = evaluated_expression_to_var(&index),
		*size_var = evaluated_expression_to_var(&size);

	index_var = ir_mul(index_var, size_var);

	struct instruction *result;
	if (expr->type == E_POINTER_ADD) {
		result = ir_add(pointer_var, index_var);
	} else if (expr->type == E_POINTER_SUB) {
		result = ir_sub(pointer_var, index_var);
	} else {
		NOTIMP();
	}

	return (struct evaluated_expression) {
		.type = EE_VARIABLE,
		.variable = result,
	};
}

static struct evaluated_expression evaluate_pointer_diff(struct expr *expr) {
	struct evaluated_expression lhs = expression_evaluate(expr->args[0]),
		rhs = expression_evaluate(expr->args[1]),
		size = expression_evaluate(type_sizeof(type_deref(expr->args[0]->data_type)));

	struct instruction *lhs_var = evaluated_expression_to_var(&lhs),
		*rhs_var = evaluated_expression_to_var(&rhs),
		*size_var = evaluated_expression_to_var(&size);

	struct instruction *res = ir_idiv(ir_sub(lhs_var, rhs_var), size_var);

	return (struct evaluated_expression) {
		.type = EE_VARIABLE,
		.variable = res,
	};
}

static struct evaluated_expression evaluate_dot_operator(struct expr *expr) {
	struct evaluated_expression lhs = expression_evaluate(expr->member.lhs);
	assert(lhs.data_type->type == TY_STRUCT);

	struct struct_data *data = lhs.data_type->struct_data;

	int idx = expr->member.member_idx;
	int field_offset = data->fields[idx].offset;

	struct instruction *address = evaluated_expression_to_address(&lhs);

	struct instruction *member_address = ir_get_offset(address, field_offset);

	if (data->fields[idx].bitfield != -1) {
		assert(data->fields[idx].type->type == TY_SIMPLE);

		return (struct evaluated_expression) {
			.type = EE_BITFIELD_POINTER,
			.is_lvalue = lhs.is_lvalue,
			.bitfield_pointer = {
				.pointer = member_address,
				.bitfield = data->fields[idx].bitfield,
				.offset = data->fields[idx].bit_offset,
				.sign_extend = is_signed(data->fields[idx].type->simple),
			}
		};
	}

	return (struct evaluated_expression) {
		.type = EE_POINTER,
		.is_lvalue = lhs.is_lvalue,
		.pointer = member_address,
	};
}

static struct evaluated_expression evaluate_cast(struct expr *expr) {
	struct evaluated_expression rhs = expression_evaluate(expr->cast.arg);
	return evaluated_expression_cast(&rhs, expr->cast.target);
}

static struct evaluated_expression evaluate_address_of(struct expr *expr) {
	struct evaluated_expression rhs = expression_evaluate(expr->args[0]);

	switch (rhs.type) {
	case EE_POINTER:
		return (struct evaluated_expression) {
			.type = EE_VARIABLE,
			.variable = rhs.pointer,
		};

	default: NOTIMP();
	}
}

static struct evaluated_expression evaluate_array_ptr_decay(struct expr *expr) {
	struct evaluated_expression rhs = expression_evaluate(expr->args[0]);

	switch (rhs.type) {
	case EE_POINTER:
		return (struct evaluated_expression) {
			.type = EE_VARIABLE,
			.variable = rhs.pointer,
		};

	default: NOTIMP();
	}
}

static struct evaluated_expression evaluate_compound_literal(struct expr *expr) {
	struct instruction *address = ir_allocate(calculate_size(expr->compound_literal.type));
	ir_init_ptr(&expr->compound_literal.init, expr->compound_literal.type, address);

	return (struct evaluated_expression) {
		.type = EE_POINTER,
		.pointer = address,
	};
}

static struct evaluated_expression evaluate_binary_operator(struct expr *expr) {
	struct evaluated_expression lhs = expression_evaluate(expr->args[0]),
		rhs = expression_evaluate(expr->args[1]);

	struct instruction *lhs_var = evaluated_expression_to_var(&lhs),
		*rhs_var = evaluated_expression_to_var(&rhs);

	struct instruction *res = ir_binary_op(ir_from_type_and_op(expr->args[0]->data_type, expr->binary_op), lhs_var, rhs_var);

	return (struct evaluated_expression) {
		.type = EE_VARIABLE,
		.variable = res
	};
}

static struct evaluated_expression evaluate_unary_operator(struct expr *expr) {
	struct evaluated_expression rhs = expression_evaluate(expr->args[0]);
	struct type *type = expr->args[0]->data_type;
	enum unary_operator_type uop = expr->unary_op;

	if (uop == UOP_PLUS)
		return rhs;

	struct instruction *res;
	struct instruction *rhs_var = evaluated_expression_to_var(&rhs);

	if (type_is_integer(type)) {
		if (uop == UOP_BNOT) {
			res = ir_binary_not(rhs_var);
		} else if (uop == UOP_NEG) {
			res = ir_negate_int(rhs_var);
		} else {
			NOTIMP();
		}
	} else if (type_is_floating(type)) {
		if (uop == UOP_NEG) {
			res = ir_negate_float(rhs_var);
		} else {
			NOTIMP();
		}
	} else {
		NOTIMP();
	}

	return (struct evaluated_expression) {
		.type = EE_VARIABLE,
		.variable = res
	};
}

static struct evaluated_expression evaluate_assignment(struct expr *expr) {
	struct evaluated_expression lhs = expression_evaluate(expr->args[0]);
	struct evaluated_expression rhs = expression_evaluate(expr->args[1]);

	rhs = evaluated_expression_cast(&rhs, expr->args[0]->data_type);

	assert(lhs.is_lvalue);

	assign_ee_to_ee(&lhs, &rhs);

	return rhs;
}

static struct evaluated_expression evaluate_assignment_op(struct expr *expr) {
	struct evaluated_expression lhs = expression_evaluate(expr->args[0]),
		rhs = expression_evaluate(expr->args[1]);

	struct type *operator_type = expr->args[1]->data_type;
	int ir_operator = ir_from_type_and_op(operator_type, expr->assignment_op.op);

	struct instruction *lhs_var = evaluated_expression_to_var(&lhs);
	struct instruction *rhs_var = evaluated_expression_to_var(&rhs);

	struct instruction *res_var = lhs_var;

	//     a += b
	// should be interpreted as
	//     a = a + b
	// If implicit conversions are included this becomes
	//     a = (typeof(a))(   (typeof(a + b))a + (typeof(a + b))b   )
	// b is implicitly converted in the parsing stage,
	// therefore typeof(a + b) == expr->args[1]->data_type.
	// For example if a is a unsigned char, a++ is converted into a += (int)1,
	// which then becomes
	// a = (unsigned char)((int)a + (int)1).

	lhs_var = variable_cast(lhs_var, lhs.data_type, operator_type);
	lhs_var = ir_binary_op(ir_operator, lhs_var, rhs_var);
	lhs_var = variable_cast(lhs_var, operator_type, lhs.data_type);

	if (!expr->assignment_op.postfix)
		res_var = lhs_var;

	struct evaluated_expression res = {
		.type = EE_VARIABLE,
		.data_type = operator_type,
		.variable = res_var
	};

	assign_to_ee(&lhs, lhs_var);

	return res;
}

static struct evaluated_expression evaluate_conditional(struct expr *expr) {
	struct evaluated_expression condition = expression_evaluate(expr->args[0]);
	struct instruction *condition_var = evaluated_expression_to_var(&condition);

	int is_void = type_is_simple(expr->data_type, ST_VOID);

	if (is_void) {
		struct block *block_true = new_block(),
			*block_false = new_block(),
			*block_end = new_block();

		ir_if_selection(condition_var, block_true, block_false);

		ir_block_start(block_true);
		expression_evaluate(expr->args[1]);
		ir_goto(block_end);

		ir_block_start(block_false);
		expression_evaluate(expr->args[2]);
		ir_goto(block_end);

		ir_block_start(block_end);

		return (struct evaluated_expression) {
			.type = EE_VOID
		};
	} else {

		struct block *block_true = new_block(),
			*block_false = new_block(),
			*block_end = new_block();

		ir_if_selection(condition_var, block_true, block_false);

		ir_block_start(block_true);
		struct evaluated_expression true_ = expression_evaluate(expr->args[1]);
		struct instruction *true_address = evaluated_expression_to_address(&true_);

		block_true = get_current_block(); // Current block might have changed.
		ir_goto(block_end);

		ir_block_start(block_false);
		struct evaluated_expression false_ = expression_evaluate(expr->args[2]);
		struct instruction *false_address = evaluated_expression_to_address(&false_);

		block_false = get_current_block();
		ir_goto(block_end);

		ir_block_start(block_end);

		struct instruction *res_address = ir_phi(true_address, false_address, block_true, block_false);

		return (struct evaluated_expression) {
			.type = EE_POINTER,
			.pointer = res_address,
		};
	}
}

static struct evaluated_expression evaluate_assignment_pointer(struct expr *expr) {
	struct evaluated_expression pointer = expression_evaluate(expr->args[0]),
		index = expression_evaluate(expr->args[1]),
		size = expression_evaluate(type_sizeof(type_deref(expr->args[0]->data_type)));

	index = evaluated_expression_cast(&index, type_simple(abi_info.pointer_type));

	struct instruction *pointer_var = evaluated_expression_to_var(&pointer),
		*index_var = evaluated_expression_to_var(&index),
		*size_var = evaluated_expression_to_var(&size);

	struct instruction *res = pointer_var;

	index_var = ir_mul(index_var, size_var);

	if (expr->assignment_pointer.sub) {
		pointer_var = ir_sub(pointer_var, index_var);
	} else {
		pointer_var = ir_add(pointer_var, index_var);
	}

	assign_to_ee(&pointer, pointer_var);

	if (!expr->assignment_pointer.postfix)
		res = pointer_var;

	return (struct evaluated_expression) {
		.type = EE_VARIABLE,
		.variable = res,
	};
}

static struct evaluated_expression evaluate_va_start(struct expr *expr) {
	get_current_function()->uses_va = 1;
	struct evaluated_expression arr = expression_evaluate(expr->va_start_.array);
	struct instruction *address = evaluated_expression_to_address(&arr);

	ir_va_start(address);

	return (struct evaluated_expression) {
		.type = EE_VOID,
	};
}

static struct evaluated_expression evaluate_va_arg(struct expr *expr) {
	struct evaluated_expression v = expression_evaluate(expr->va_arg_.v);
	struct instruction *result_address = ir_allocate(calculate_size(expr->data_type));
	if (abi_info.va_list_is_reference) {
		ir_va_arg(evaluated_expression_to_address(&v), result_address, expr->va_arg_.t);
	} else {
		ir_va_arg(evaluated_expression_to_var(&v), result_address, expr->va_arg_.t);
	}

	return (struct evaluated_expression) {
		.type = EE_POINTER,
		.variable = result_address
	};
}

static struct evaluated_expression evaluate_va_copy(struct expr *expr) {
	struct evaluated_expression dest = expression_evaluate(expr->va_copy_.d);
	struct evaluated_expression src = expression_evaluate(expr->va_copy_.s);

	struct instruction *dest_var, *src_var;
	int size;

	if (abi_info.va_list_is_reference) {
		dest_var = evaluated_expression_to_address(&dest);
		src_var = evaluated_expression_to_address(&src);

		size = calculate_size(expr->va_copy_.d->data_type);
	} else {
		dest_var = evaluated_expression_to_var(&dest);
		src_var = evaluated_expression_to_var(&src);

		size = calculate_size(type_deref(expr->va_copy_.d->data_type));
	}

	ir_copy_memory(dest_var, src_var, size);

	return (struct evaluated_expression) {
		.type = EE_VOID,
	};
}

struct evaluated_expression expression_evaluate(struct expr *expr) {
	struct evaluated_expression ret;

	switch (expr->type) {
		// All these follow the same pattern.
	case E_SYMBOL: ret = evaluate_symbol(expr); break;
	case E_VARIABLE: ret = evaluate_variable(expr); break;
	case E_INDIRECTION: ret = evaluate_indirection(expr); break;
	case E_CALL: ret = evaluate_call(expr); break;
	case E_CONSTANT: ret = evaluate_constant(expr); break;
	case E_POINTER_ADD: // fallthrough.
	case E_POINTER_SUB: ret = evaluate_pointer_arithmetic(expr); break;
	case E_POINTER_DIFF: ret = evaluate_pointer_diff(expr); break;
	case E_DOT_OPERATOR: ret = evaluate_dot_operator(expr); break;
	case E_CAST: ret = evaluate_cast(expr); break;
	case E_ADDRESS_OF: ret = evaluate_address_of(expr); break;
	case E_COMPOUND_LITERAL: ret = evaluate_compound_literal(expr); break;
	case E_BINARY_OP: ret = evaluate_binary_operator(expr); break;
	case E_UNARY_OP: ret = evaluate_unary_operator(expr); break;
	case E_ARRAY_PTR_DECAY: ret = evaluate_array_ptr_decay(expr); break;
	case E_ASSIGNMENT: ret = evaluate_assignment(expr); break;
	case E_ASSIGNMENT_OP: ret = evaluate_assignment_op(expr); break;
	case E_CONDITIONAL: ret = evaluate_conditional(expr); break;
	case E_ASSIGNMENT_POINTER: ret = evaluate_assignment_pointer(expr); break;
	case E_BUILTIN_VA_START: ret = evaluate_va_start(expr); break;
	case E_BUILTIN_VA_ARG: ret = evaluate_va_arg(expr); break;
	case E_BUILTIN_VA_COPY: ret = evaluate_va_copy(expr); break;

		// These do not.
	case E_CONST_REMOVE:
		ret = expression_evaluate(expr->args[0]);
		break;

	case E_BUILTIN_VA_END:
		ret = (struct evaluated_expression) {
			.type = EE_VOID,
		};
		break;

	case E_COMMA:
		expression_evaluate(expr->args[0]);
		ret = expression_evaluate(expr->args[1]);
		break;

	default:
		printf("%d\n", expr->type);
		NOTIMP();
	}

	ret.data_type = expr->data_type;

	return ret;
}

// Helper functions.
struct instruction *expression_to_ir(struct expr *expr) {
	struct evaluated_expression ee = expression_evaluate(expr);
	return evaluated_expression_to_var(&ee);
}

struct instruction *expression_to_ir_clear_temp(struct expr *expr) {
	struct instruction *res = expression_to_ir(expr);
	return res;
}

struct instruction *expression_to_size_t(struct expr *expr) {
	struct evaluated_expression ee = expression_evaluate(expr);
	ee = evaluated_expression_cast(&ee, type_simple(ST_ULLONG));
	struct instruction *res = evaluated_expression_to_var(&ee);
	return res;
}

struct instruction *expression_to_int(struct expr *expr) {
	struct evaluated_expression ee = expression_evaluate(expr);
	ee = evaluated_expression_cast(&ee, type_simple(ST_INT));
	struct instruction *res = evaluated_expression_to_var(&ee);
	return res;
}

void expression_to_void(struct expr *expr) {
	expression_evaluate(expr);
}

void expression_to_address(struct expr *expr, struct instruction *address) {
	struct evaluated_expression ee = expression_evaluate(expr);

	switch (ee.type) {
	case EE_POINTER:
		ir_copy_memory(address, ee.pointer, calculate_size(ee.data_type));
		break;

	default: {
		struct instruction *var = evaluated_expression_to_var(&ee);
		ir_store(address, var);
	} break;
	}
}
