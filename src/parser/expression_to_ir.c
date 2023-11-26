#include "expression_to_ir.h"
#include "arch/x64.h"
#include "ir/ir.h"
#include "ir/operators.h"
#include "ir/variables.h"
#include "parser/expression.h"
#include "parser/parser.h"
#include "types.h"

#include <abi/abi.h>
#include <common.h>

#include <assert.h>

var_id evaluated_expression_to_address(struct evaluated_expression *evaluated_expression) {
	switch (evaluated_expression->type) {
	case EE_BITFIELD_POINTER: {
		var_id bitfield_var = evaluated_expression_to_var(evaluated_expression);
		var_id address = new_variable_sz(8, 1, 1);
		IR_PUSH_ALLOC(address, calculate_size(evaluated_expression->data_type));
		ir_push2(IR_STORE, bitfield_var, address);
		return address;
	}

	case EE_CONSTANT: {
		struct constant constant = evaluated_expression->constant;
		if (constant.type == CONSTANT_LABEL || constant.type == CONSTANT_TYPE_POINTER) {
			var_id var = new_variable_sz(8, 1, 1);

			if (constant.type == CONSTANT_TYPE_POINTER) {
				constant.type = CONSTANT_TYPE;
			}

			IR_PUSH_CONSTANT(constant, var);
			return var;
		} else {
			// Allocate memory, and push the constant to it.
			var_id address = new_variable_sz(8, 1, 1);
			IR_PUSH_ALLOC(address, calculate_size(evaluated_expression->data_type));
			IR_PUSH_CONSTANT_ADDRESS(constant, address);
			return address;
		}
	}

	case EE_POINTER:
		return evaluated_expression->pointer;

	case EE_VARIABLE: {
		var_id address = new_variable_sz(8, 1, 1);
		IR_PUSH_ALLOC(address, calculate_size(evaluated_expression->data_type));
		ir_push2(IR_STORE, evaluated_expression->variable, address);
		return address;
	}

	case EE_VOID:
		NOTIMP();
		break;

	default:
		NOTIMP();
	}
}

var_id evaluated_expression_to_var(struct evaluated_expression *evaluated_expression) {
	switch (evaluated_expression->type) {
	case EE_BITFIELD_POINTER: {
		var_id var = new_variable(evaluated_expression->data_type, 1, 1);
		ir_push2(IR_LOAD, var, evaluated_expression->bitfield_pointer.pointer);
		ir_get_bits(var, var, evaluated_expression->bitfield_pointer.offset,
					evaluated_expression->bitfield_pointer.bitfield,
					evaluated_expression->bitfield_pointer.sign_extend);
		return var;
	}

	case EE_CONSTANT: {
		var_id var = new_variable(evaluated_expression->data_type, 1, 1);
		IR_PUSH_CONSTANT(evaluated_expression->constant, var);
		return var;
	}

	case EE_POINTER: {
		var_id var = new_variable(evaluated_expression->data_type, 1, 1);
		ir_push2(IR_LOAD, var, evaluated_expression->pointer);
		return var;
	}

	case EE_VARIABLE:
		return evaluated_expression->variable;

	case EE_VOID: { // This should really never be evaluated.
		var_id var = new_variable_sz(0, 1, 1);
		return var;
	}

	default:
		NOTIMP();
	}
}

static var_id variable_cast(var_id operand_var, struct type *operand_type, struct type *resulting_type) {
	var_id res = operand_var;

	if (type_is_simple(resulting_type, ST_BOOL)) {
		res = new_variable(resulting_type, 1, 1);
		ir_push2(IR_BOOL_CAST, res, operand_var);
	} else if ((type_is_integer(resulting_type) || type_is_pointer(resulting_type)) &&
			   (type_is_integer(operand_type) || type_is_pointer(operand_type))) {
		if (calculate_size(resulting_type) != calculate_size(operand_type)) {
			int sign_extend = type_is_integer(operand_type) && is_signed(operand_type->simple);
			res = new_variable(resulting_type, 1, 1);
			ir_push2(sign_extend ? IR_INT_CAST_SIGN : IR_INT_CAST_ZERO, res, operand_var);
		}
	} else if(type_is_floating(resulting_type) && type_is_floating(operand_type)) {
		res = new_variable(resulting_type, 1, 1);
		ir_push2(IR_FLOAT_CAST, res, operand_var);
	} else if(type_is_floating(resulting_type) && type_is_integer(operand_type)) {
		res = new_variable(resulting_type, 1, 1);
		ir_push2(is_signed(operand_type->simple) ? IR_INT_FLOAT_CAST : IR_UINT_FLOAT_CAST, res, operand_var);
	} else if(type_is_integer(resulting_type) && type_is_floating(operand_type)) {
		res = new_variable(resulting_type, 1, 1);
		ir_push2(IR_FLOAT_INT_CAST, res, operand_var);
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

	var_id operand_var = evaluated_expression_to_var(operand);
	var_id res = variable_cast(operand_var, operand->data_type, resulting_type);

	return (struct evaluated_expression) {
		.type = EE_VARIABLE,
		.data_type = resulting_type,
		.variable = res
	};
}

static void assign_to_ee(struct evaluated_expression *lhs, var_id rhs_var) {
	switch (lhs->type) {
	case EE_POINTER:
		ir_push2(IR_STORE, rhs_var, lhs->pointer);
		break;

	case EE_BITFIELD_POINTER: {
		var_id prev = new_variable_sz(get_variable_size(rhs_var), 1, 1);
		ir_push2(IR_LOAD, prev, lhs->bitfield_pointer.pointer);

		ir_set_bits(prev, prev, rhs_var, lhs->bitfield_pointer.offset, lhs->bitfield_pointer.bitfield);

		ir_push2(IR_STORE, prev, lhs->bitfield_pointer.pointer);
	} break;
		
	default: NOTIMP();
	}
}

static void assign_ee_to_ee(struct evaluated_expression *lhs, struct evaluated_expression *rhs) {
	assert(lhs->data_type == rhs->data_type);
	if (calculate_size(rhs->data_type) <= 8) { // If fits into variable, take shortcut.
		var_id rhs_var = evaluated_expression_to_var(rhs);
		assign_to_ee(lhs, rhs_var);
	} else {
		// Otherwise copy address.
		var_id rhs_adress = evaluated_expression_to_address(rhs);
		switch (lhs->type) {
		case EE_POINTER:
			IR_PUSH_COPY_MEMORY(lhs->pointer, rhs_adress, calculate_size(lhs->data_type));
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
		var_id value = new_variable_sz(8, 1, 1);
		ir_push2(IR_LOAD, value, rhs.pointer);
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

static struct evaluated_expression evaluate_variable_ptr(struct expr *expr) {
	return (struct evaluated_expression) {
		.type = EE_POINTER,
		.is_lvalue = 1,
		.pointer = expr->variable_ptr.ptr
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

		var_id pointer = new_variable_sz(8, 1, 1);
		IR_PUSH_CONSTANT(*constant, pointer);

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

	var_id pointer_var = evaluated_expression_to_var(&pointer),
		index_var = evaluated_expression_to_var(&index),
		size_var = evaluated_expression_to_var(&size);

	var_id result_pointer = new_variable_sz(8, 1, 1);
	ir_push3(IR_MUL, index_var, index_var, size_var);

	if (expr->type == E_POINTER_ADD) {
		ir_push3(IR_ADD, result_pointer, pointer_var, index_var);
	} else if (expr->type == E_POINTER_SUB) {
		ir_push3(IR_SUB, result_pointer, pointer_var, index_var);
	}

	return (struct evaluated_expression) {
		.type = EE_VARIABLE,
		.variable = result_pointer,
	};
}

static struct evaluated_expression evaluate_pointer_diff(struct expr *expr) {
	struct evaluated_expression lhs = expression_evaluate(expr->args[0]),
		rhs = expression_evaluate(expr->args[1]),
		size = expression_evaluate(type_sizeof(type_deref(expr->args[0]->data_type)));

	var_id lhs_var = evaluated_expression_to_var(&lhs),
		rhs_var = evaluated_expression_to_var(&rhs),
		size_var = evaluated_expression_to_var(&size);

	var_id res = new_variable(expr->data_type, 1, 1);

	ir_push3(IR_SUB, res, lhs_var, rhs_var);
	ir_push3(IR_IDIV, res, res, size_var);

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

	var_id address = evaluated_expression_to_address(&lhs);

	var_id member_address = new_variable_sz(8, 1, 1);
	ir_get_offset(member_address, address, 0, field_offset);

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
	var_id address = new_variable_sz(8, 1, 1);
	IR_PUSH_ALLOC(address, calculate_size(expr->compound_literal.type));
	ir_init_ptr(&expr->compound_literal.init, expr->compound_literal.type, address);

	return (struct evaluated_expression) {
		.type = EE_POINTER,
		.pointer = address,
	};
}

static struct evaluated_expression evaluate_binary_operator(struct expr *expr) {
	struct evaluated_expression lhs = expression_evaluate(expr->args[0]),
		rhs = expression_evaluate(expr->args[1]);

	var_id lhs_var = evaluated_expression_to_var(&lhs),
		rhs_var = evaluated_expression_to_var(&rhs);

	var_id res = new_variable(expr->data_type, 1, 1);

	ir_push3(ir_from_type_and_op(expr->args[0]->data_type, expr->binary_op), res, lhs_var, rhs_var);

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

	var_id res = new_variable(expr->data_type, 1, 1);
	if (type_is_integer(type)) {
		if (uop == UOP_BNOT) {
			ir_push2(IR_BINARY_NOT, res, evaluated_expression_to_var(&rhs));
		} else if (uop == UOP_NEG) {
			ir_push2(IR_NEGATE_INT, res, evaluated_expression_to_var(&rhs));
		} else {
			NOTIMP();
		}
	} else if (type_is_floating(type)) {
		if (uop == UOP_NEG) {
			ir_push2(IR_NEGATE_FLOAT, res, evaluated_expression_to_var(&rhs));
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

	/* var_id rhs_address = evaluated_expression_to_address(&rhs); */
	assert(lhs.is_lvalue);

	assign_ee_to_ee(&lhs, &rhs);

	return rhs;
}

static struct evaluated_expression evaluate_assignment_op(struct expr *expr) {
	struct evaluated_expression lhs = expression_evaluate(expr->args[0]),
		rhs = expression_evaluate(expr->args[1]);

	struct type *operator_type = expr->args[1]->data_type;
	int ir_operator = ir_from_type_and_op(operator_type, expr->assignment_op.op);

	var_id lhs_var = evaluated_expression_to_var(&lhs);
	var_id rhs_var = evaluated_expression_to_var(&rhs);

	var_id res_var = lhs_var;

	if (expr->assignment_op.postfix) {
		res_var = new_variable_sz(get_variable_size(lhs_var), 1, 1);
		ir_push2(IR_COPY, res_var, lhs_var);
	}

	if (operator_type != lhs.data_type)
		lhs_var = variable_cast(lhs_var, lhs.data_type, operator_type);

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

	ir_push3(ir_operator, lhs_var, lhs_var, rhs_var);

	if (operator_type != lhs.data_type)
		lhs_var = variable_cast(lhs_var, operator_type, lhs.data_type);

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
	var_id condition_var = evaluated_expression_to_var(&condition);

	int is_void = type_is_simple(expr->data_type, ST_VOID);

	if (is_void) {
		block_id block_true = new_block(),
			block_false = new_block(),
			block_end = new_block();

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
		var_id res_address = new_variable_sz(8, 1, 1);

		block_id block_true = new_block(),
			block_false = new_block(),
			block_end = new_block();

		ir_if_selection(condition_var, block_true, block_false);

		ir_block_start(block_true);
		struct evaluated_expression true_ = expression_evaluate(expr->args[1]);
		var_id true_address = evaluated_expression_to_address(&true_);
		if (!is_void)
			ir_push2(IR_COPY, res_address, true_address);
		ir_goto(block_end);

		ir_block_start(block_false);
		struct evaluated_expression false_ = expression_evaluate(expr->args[2]);
		var_id false_address = evaluated_expression_to_address(&false_);
		if (!is_void)
			ir_push2(IR_COPY, res_address, false_address);
		ir_goto(block_end);

		ir_block_start(block_end);

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

	var_id pointer_var = evaluated_expression_to_var(&pointer),
		index_var = evaluated_expression_to_var(&index),
		size_var = evaluated_expression_to_var(&size);

	var_id res = pointer_var;
	if (expr->assignment_pointer.postfix) {
		res = new_variable_sz(get_variable_size(pointer_var), 1, 1);
		ir_push2(IR_COPY, res, pointer_var);
	}

	ir_push3(IR_MUL, index_var, index_var, size_var);

	if (expr->assignment_pointer.sub) {
		ir_push3(IR_SUB, pointer_var, pointer_var, index_var);
	} else {
		ir_push3(IR_ADD, pointer_var, pointer_var, index_var);
	}

	assign_to_ee(&pointer, pointer_var);

	return (struct evaluated_expression) {
		.type = EE_VARIABLE,
		.variable = res,
	};
}

static struct evaluated_expression evaluate_va_start(struct expr *expr) {
	get_current_function()->uses_va = 1;
	struct evaluated_expression arr = expression_evaluate(expr->va_start_.array);
	var_id address = evaluated_expression_to_address(&arr);
	ir_push1(IR_VA_START, address);

	return (struct evaluated_expression) {
		.type = EE_VOID,
	};
}

static struct evaluated_expression evaluate_va_arg(struct expr *expr) {
	struct evaluated_expression v = expression_evaluate(expr->va_arg_.v);
	var_id result_address = new_variable_sz(8, 1, 1);
	IR_PUSH_ALLOC(result_address, calculate_size(expr->data_type));
	if (abi_info.va_list_is_reference) {
		IR_PUSH_VA_ARG(evaluated_expression_to_address(&v), result_address, expr->va_arg_.t);
	} else {
		IR_PUSH_VA_ARG(evaluated_expression_to_var(&v), result_address, expr->va_arg_.t);
	}

	return (struct evaluated_expression) {
		.type = EE_POINTER,
		.variable = result_address
	};
}

static struct evaluated_expression evaluate_va_copy(struct expr *expr) {
	struct evaluated_expression dest = expression_evaluate(expr->va_copy_.d);
	struct evaluated_expression src = expression_evaluate(expr->va_copy_.s);

	var_id dest_var, src_var;
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

	IR_PUSH_COPY_MEMORY(dest_var, src_var, size);

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
	case E_VARIABLE_PTR: ret = evaluate_variable_ptr(expr); break;
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
var_id expression_to_ir(struct expr *expr) {
	struct evaluated_expression ee = expression_evaluate(expr);
	return evaluated_expression_to_var(&ee);
}

var_id expression_to_ir_clear_temp(struct expr *expr) {
	var_id res = expression_to_ir(expr);
	variable_set_stack_bucket(res, 0);
	ir_push0(IR_CLEAR_STACK_BUCKET);
	return res;
}

var_id expression_to_size_t(struct expr *expr) {
	struct evaluated_expression ee = expression_evaluate(expr);
	ee = evaluated_expression_cast(&ee, type_simple(ST_ULLONG));
	var_id res = evaluated_expression_to_var(&ee);
	variable_set_stack_bucket(res, 0);
	ir_push0(IR_CLEAR_STACK_BUCKET);
	return res;
}

var_id expression_to_int(struct expr *expr) {
	struct evaluated_expression ee = expression_evaluate(expr);
	ee = evaluated_expression_cast(&ee, type_simple(ST_INT));
	var_id res = evaluated_expression_to_var(&ee);
	variable_set_stack_bucket(res, 0);
	ir_push0(IR_CLEAR_STACK_BUCKET);
	return res;
}

void expression_to_void(struct expr *expr) {
	struct evaluated_expression ee = expression_evaluate(expr);
	(void)ee;
	/* var_id res = expression_to_ir(expr); */
	/* variable_set_stack_bucket(res, 0); */
	ir_push0(IR_CLEAR_STACK_BUCKET);
}

void expression_to_address(struct expr *expr, var_id address) {
	struct evaluated_expression ee = expression_evaluate(expr);

	switch (ee.type) {
	case EE_POINTER:
		IR_PUSH_COPY_MEMORY(address, ee.pointer, calculate_size(ee.data_type));
		break;

	default: {
		var_id var = evaluated_expression_to_var(&ee);
		ir_push2(IR_STORE, var, address);
	} break;
	}
}
