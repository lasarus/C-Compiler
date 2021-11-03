#include "expression.h"
#include "declaration.h"
#include "function_parser.h"
#include "symbols.h"

#include <common.h>
#include <codegen/rodata.h>
#include <precedence.h>

#include <assert.h>

// Type conversions
enum simple_type get_arithmetic_type(enum simple_type a,
									 enum simple_type b) {
	if (a == ST_LDOUBLE || b == ST_LDOUBLE)
		return ST_LDOUBLE;
    else if (a == ST_DOUBLE || b == ST_DOUBLE)
		return ST_DOUBLE;
	else if (a == ST_FLOAT || b == ST_FLOAT)
		return ST_FLOAT;

	else if (a == b) {
		return a;

	} else if (is_signed(a) == is_signed(b)) {
		return (type_rank(a) > type_rank(b)) ? a : b;
	} else if (!is_signed(a) &&
			   type_rank(a) >= type_rank(b)) {
		return a;
	} else if (!is_signed(b) &&
			   type_rank(b) >= type_rank(a)) {
		return b;

	} else if (is_signed(a) &&
		is_contained_in(a, b)) {
		return a;
	} else if (is_signed(b) &&
		is_contained_in(b, a)) {
		return b;

	} else if (is_signed(b)) {
		return to_unsigned(b);

	} else if (is_signed(a)) {
		return to_unsigned(a);
	} else {
		ERROR("Internal compiler error!");
	}
}

void convert_arithmetic(struct expr **a,
						struct expr **b) {
	struct type *a_type = (*a)->data_type,
		*b_type = (*b)->data_type;

	if (a_type->type != TY_SIMPLE ||
		b_type->type != TY_SIMPLE)
		return;

	enum simple_type target_type =
		get_arithmetic_type(a_type->simple, b_type->simple);

	*a = expression_cast(*a, type_simple(target_type));
	*b = expression_cast(*b, type_simple(target_type));
}

void decay_array(struct expr **expr) {
	struct type *type = (*expr)->data_type;
	if (type->type == TY_ARRAY ||
		type->type == TY_INCOMPLETE_ARRAY ||
		type->type == TY_VARIABLE_LENGTH_ARRAY) {
		*expr = EXPR_ARGS(E_ARRAY_PTR_DECAY, *expr);
	} else if (type->type == TY_FUNCTION) {
		*expr = EXPR_ARGS(E_ADDRESS_OF, *expr);
	}
}

void do_integer_promotion(struct expr **expr) {
	struct type *current_type = (*expr)->data_type;
	
	if (current_type->type != TY_SIMPLE)
		return;

	enum simple_type simple_type = current_type->simple;

	switch(simple_type) {
	case ST_BOOL:
	case ST_CHAR:
	case ST_SCHAR:
	case ST_UCHAR:
	case ST_SHORT:
	case ST_USHORT:
		*expr = expression_cast(*expr, type_simple(ST_INT));
	default: break;
	}
}

void do_default_argument_promotion(struct expr **expr) {
	do_integer_promotion(expr);

	if (type_is_simple((*expr)->data_type, ST_FLOAT))
		*expr = expression_cast(*expr, type_simple(ST_DOUBLE));
}

struct type *calculate_type(struct expr *expr) {
	switch (expr->type) {
	case E_CONSTANT:
		return expr->constant.data_type;

	case E_BINARY_OP:
		return operators_get_result_type(expr->binary_op, expr->args[0]->data_type,
										 expr->args[1]->data_type);

	case E_UNARY_OP:
		return expr->args[0]->data_type;

	case E_CALL: {
		struct type *callee_type = expr->call.callee->data_type;
		if (callee_type->type == TY_FUNCTION)
			return callee_type->children[0];
		else if (callee_type->type == TY_POINTER)
			return type_deref(callee_type)->children[0];
		else {
			printf("POS: ");
			PRINT_POS(expr->pos);
			printf("\n");
			ERROR("Can't call type %s\n", dbg_type(callee_type));
		}
	}

	case E_VARIABLE:
		return expr->variable.type;

	case E_VARIABLE_LENGTH_ARRAY:
		return expr->variable.type;

	case E_INDIRECTION:
		return type_deref(expr->args[0]->data_type);

	case E_ADDRESS_OF:
		return type_pointer(expr->args[0]->data_type);

	case E_ARRAY_PTR_DECAY:
		return type_pointer(expr->args[0]->data_type->children[0]);

	case E_POINTER_ADD:
	case E_POINTER_SUB:
	case E_ASSIGNMENT:
	case E_ASSIGNMENT_OP:
	case E_ASSIGNMENT_POINTER:
		return expr->args[0]->data_type;

	case E_CAST:
		return expr->cast.target;

	case E_DOT_OPERATOR: {
		struct type *type;
		int offset;
		type_select(expr->member.lhs->data_type, expr->member.member_idx, &offset, &type);
		return type;
	}

	case E_CONDITIONAL:
		assert(expr->args[1]->data_type == expr->args[2]->data_type);
		return expr->args[1]->data_type;

	case E_BUILTIN_VA_ARG:
		return expr->va_arg_.t;
		
	case E_BUILTIN_VA_START:
	case E_BUILTIN_VA_END:
	case E_BUILTIN_VA_COPY:
		return type_simple(ST_VOID);

	case E_COMPOUND_LITERAL:
		return expr->compound_literal.type;

	case E_POINTER_DIFF:
		return type_simple(ST_INT);

	case E_COMMA:
		return expr->args[1]->data_type;

	default:
		printf("%d\n", expr->type);
		NOTIMP();
	}
}

static const int dont_decay_ptr[E_NUM_TYPES] = {
	[E_ADDRESS_OF] = 1,
	[E_ARRAY_PTR_DECAY] = 1,
};

static const int num_args[E_NUM_TYPES] = {
	[E_ADDRESS_OF] = 1,
	[E_INDIRECTION] = 1,
	[E_UNARY_OP] = 1,
	[E_ALIGNOF] = 1,
	[E_BINARY_OP] = 2,
	[E_ASSIGNMENT] = 2,
	[E_CONDITIONAL] = 3,
	[E_COMMA] = 2,
	[E_ASSIGNMENT_OP] = 2,
};

static const int does_integer_conversion[E_NUM_TYPES] = {
	[E_UNARY_OP] = 1,
	[E_BINARY_OP] = 1,
	[E_ASSIGNMENT_OP] = 1,
	[E_CONDITIONAL] = 1,
};

int is_null_pointer_constant(struct expr *expr) {
	struct constant *c = expression_to_constant(expr);

	return c && c->type == CONSTANT_TYPE && constant_is_zero(c);
}

void cast_conditional(struct expr *expr) {
	if (expr->type != E_CONDITIONAL)
		return;

	struct type *a = expr->args[1]->data_type,
		*b = expr->args[2]->data_type;

	if (a == b)
		return;

	if (type_is_pointer(a) && type_is_pointer(b)) {
		if (type_deref(a) == type_simple(ST_VOID))
			expr->args[1] = expression_cast(expr->args[1], b);
		else if (type_deref(b) == type_simple(ST_VOID))
			expr->args[2] = expression_cast(expr->args[2], a);
		else {
			ERROR("Invalid combination of data types:\n%s and %s\n",
				  strdup(dbg_type(a)),
				  strdup(dbg_type(b)));
		}
	} else if (type_is_pointer(a) && is_null_pointer_constant(expr->args[2])) {
		expr->args[2] = expression_cast(expr->args[2], a);
	} else if (type_is_pointer(b) && is_null_pointer_constant(expr->args[1])) {
		expr->args[1] = expression_cast(expr->args[1], b);
	} else if (type_is_arithmetic(a) &&
			   type_is_arithmetic(b)) {
		convert_arithmetic(&expr->args[1], &expr->args[2]);
	} else if (a != b) {
		ERROR("Invalid combination of data types:\n%s and %s\n",
			  strdup(dbg_type(a)),
			  strdup(dbg_type(b)));
	}
}

// This applies all the necessary transformations to binary operators
void fix_binary_operator(struct expr *expr) {
	if (expr->type != E_BINARY_OP)
		return;

	if (expr->binary_op == OP_LSHIFT ||
		expr->binary_op == OP_RSHIFT) {
		// Cast the right to be the same as left.
		// See 6.5.7.
		expr->args[1] = expression_cast(expr->args[1], expr->args[0]->data_type);
	} else {
		convert_arithmetic(&expr->args[0], &expr->args[1]);
	}

	int lhs_ptr = type_is_pointer(expr->args[0]->data_type),
		rhs_ptr = type_is_pointer(expr->args[1]->data_type);

	switch (expr->binary_op) {
	case OP_SUB:
		if (lhs_ptr && rhs_ptr) {
			expr->type = E_POINTER_DIFF;
		} else if (lhs_ptr) {
			expr->type = E_POINTER_SUB;
		} else if (rhs_ptr) {
			ERROR("Can't subtract with pointer as rhs.");
		}
		break;

	case OP_ADD:
		if (!(lhs_ptr || rhs_ptr))
			return;

		if (lhs_ptr && rhs_ptr) {
			ERROR("Invalid");
		}

		if (!lhs_ptr && rhs_ptr) {
			struct expr *tmp = expr->args[0];
			expr->args[0] = expr->args[1];
			expr->args[1] = tmp;
		}

		// Left hand side is now a pointer.
		expr->type = E_POINTER_ADD;
		break;

	case OP_EQUAL:
	case OP_NOT_EQUAL: {
		// Expressions of the form ptr == 0 and ptr != 0 are allowed.
		// But the right hand side must be a null pointer constant.
		if (lhs_ptr != rhs_ptr) {
			struct constant *non_ptr = NULL;
			if (!rhs_ptr)
				non_ptr = expression_to_constant(expr->args[1]);
			if (!lhs_ptr)
				non_ptr = expression_to_constant(expr->args[0]);

			if (!non_ptr || non_ptr->type != CONSTANT_TYPE ||
				!constant_is_zero(non_ptr)) {
				ERROR("can only compare pointers with pointers or null pointer constant");
			}

			if (!rhs_ptr)
				expr->args[1] = expression_cast(expr->args[1], type_pointer(type_simple(ST_VOID)));
			if (!lhs_ptr)
				expr->args[0] = expression_cast(expr->args[0], type_pointer(type_simple(ST_VOID)));
		}
	} break;

	default: // Do nothing.
		break;
	}
}

void fix_assignment_operators(struct expr *expr) {
	if (expr->type != E_ASSIGNMENT_OP)
		return;

	convert_arithmetic(&expr->args[0], &expr->args[1]);
	// Reduce double cast into single cast. Always correct in this case.
	if (expr->args[0]->type == E_CAST) {
		if (expr->args[0]->cast.arg->type == E_CAST)
			expr->args[0]->cast.arg = expr->args[0]->cast.arg->cast.arg;

		expr->assignment_op.cast = expr->args[0]->cast.target;
		expr->args[0] = expr->args[0]->cast.arg;
	}
	int lhs_ptr = type_is_pointer(expr->args[0]->data_type);

	if (lhs_ptr && (expr->assignment_op.op == OP_ADD || expr->assignment_op.op == OP_SUB)) {
		expr->type = E_ASSIGNMENT_POINTER;
		expr->assignment_pointer.postfix = expr->assignment_op.postfix;
		expr->assignment_pointer.sub = expr->assignment_op.op == OP_SUB;
	}
}

int evaluate_constant_expression(struct expr *expr,
 								 struct constant *constant);

void check_const_correctness(struct expr *expr) {
	switch (expr->type) {
	case E_ASSIGNMENT:
	case E_ASSIGNMENT_OP:
	case E_ASSIGNMENT_POINTER:
		if (expr->args[0]->data_type->is_const) {
			ERROR("Can't modify constant variable");
		}
		break;
	default:
		break;
	}
}

struct expr *expr_new(struct expr expr) {
	for (int i = 0; i < num_args[expr.type]; i++) {
		if (!expr.args[i]) {
			PRINT_POS(T0->pos);
			ERROR("Wrongly constructed expression of type %d", expr.type);
		}
	}

	if (!dont_decay_ptr[expr.type]) {
		for (int i = 0; i < num_args[expr.type]; i++)
			decay_array(&expr.args[i]);

		if (expr.type == E_BUILTIN_VA_ARG) {
			decay_array(&expr.va_arg_.v);
		} else if (expr.type == E_BUILTIN_VA_START) {
			decay_array(&expr.va_start_.array);
		} else if (expr.type == E_BUILTIN_VA_COPY) {
			decay_array(&expr.va_copy_.d);
			decay_array(&expr.va_copy_.s);
		} else if (expr.type == E_CALL) {
			for (int i = 0; i < expr.call.n_args; i++)
				decay_array(&expr.call.args[i]);
			decay_array(&expr.call.callee);
		}
	}

	int integer_promotion = does_integer_conversion[expr.type];
	if (integer_promotion) {
		for (int i = 0; i < num_args[expr.type]; i++) {
			do_integer_promotion(&expr.args[i]);
		}
	}

	if (expr.type == E_CALL) {
		assert(type_is_pointer(expr.call.callee->data_type));
		struct type *signature = type_deref(expr.call.callee->data_type);

		for (int i = signature->n - 1; i < expr.call.n_args; i++) {
			do_default_argument_promotion(&expr.call.args[i]);
		}
	}
	
	cast_conditional(&expr);
	fix_assignment_operators(&expr);
	fix_binary_operator(&expr);

	expr.data_type = calculate_type(&expr);

	if (expr.type != E_CONSTANT) {
		struct constant c;
		if (evaluate_constant_expression(&expr, &c)) {
			return expr_new((struct expr) {
					.type = E_CONSTANT,
					.constant = c
				});
		}
	}

	check_const_correctness(&expr);

	struct expr *ret = malloc(sizeof *ret);
	*ret = expr;

	return ret;
}

void pointer_increment(var_id result, var_id pointer, struct expr *index, int decrement, struct type *type) {
	var_id size = expression_to_ir(type_sizeof(type_deref(type)));
	var_id index_var = expression_to_ir(expression_cast(index, type_simple(ST_ULONG)));
	IR_PUSH_BINARY_OPERATOR(OP_MUL, OT_ULONG, index_var, size, index_var);
	IR_PUSH_BINARY_OPERATOR(decrement ? OP_SUB : OP_ADD, OT_ULONG, pointer, index_var, result);
}

// Loads pointer into return value.
int try_expression_to_address(struct expr *expr, var_id *var) {
	switch (expr->type) {
	case E_INDIRECTION:
		*var = expression_to_ir(expr->args[0]);
		return 1;

	case E_VARIABLE: {
		var_id address = new_variable(type_pointer(expr->data_type), 1, 1);
		IR_PUSH_ADDRESS_OF(address, expr->variable.id);
		*var = address;
		return 1;
	}

	case E_VARIABLE_LENGTH_ARRAY: {
		var_id address = new_variable(type_pointer(expr->data_type), 1, 1);
		IR_PUSH_COPY(address, expr->variable_length_array.ptr);
		*var = address;
		return 1;
	}

	case E_DOT_OPERATOR: {
		struct type *lhs_type = expr->member.lhs->data_type;
		assert(lhs_type->type == TY_STRUCT);

		struct struct_data *data = lhs_type->struct_data;

		int field_offset = data->fields[expr->member.member_idx].offset;

		if (data->fields[expr->member.member_idx].bitfield != -1)
			return 0;

		var_id address;

		if (!try_expression_to_address(expr->member.lhs, &address))
			return 0;

		var_id member_address = new_variable(type_pointer(expr->data_type), 1, 1);
		IR_PUSH_GET_OFFSET(member_address, address, field_offset);

		*var = member_address;
		return 1;
	}

	case E_CONSTANT: {
		struct constant *c = &expr->constant;
		assert(c->type == CONSTANT_LABEL);

		var_id ptr_result = new_variable(type_pointer(expr->data_type), 1, 1);
		IR_PUSH_GET_SYMBOL_PTR(rodata_get_label_string(c->label.label), c->label.offset, ptr_result);

		*var = ptr_result;
		return 1;
	}

	case E_CAST:
		return 0;

	case E_COMPOUND_LITERAL: {
		var_id compound = expression_to_ir(expr);
		var_id address = new_variable(type_pointer(expr->data_type), 1, 1);
		IR_PUSH_ADDRESS_OF(address, compound);
		*var = address;
		return 1;
	}

	default:
		return 0;
	}
}

var_id expression_to_address(struct expr *expr) {
	var_id res;
	if (!try_expression_to_address(expr, &res))
		ERROR("Can't take expression as lvalue");
	return res;
}

struct bitfield_address {
	var_id address;
	int bitfield, offset, sign_extend;
};

struct bitfield_address expression_to_bitfield_address(struct expr *expr) {
	struct bitfield_address out = { .bitfield = -1 };
	if (expr->type != E_DOT_OPERATOR) {
		out.address = expression_to_address(expr);
	} else {
		struct struct_data *data = expr->member.lhs->data_type->struct_data;
		int idx = expr->member.member_idx;

		if (data->fields[idx].bitfield == -1) {
			out.address = expression_to_address(expr);
		} else {
			out.address = expression_to_address(expr->member.lhs);
			int offset = data->fields[idx].offset;

			IR_PUSH_GET_OFFSET(out.address, out.address, offset);

			out.bitfield = data->fields[idx].bitfield;
			out.offset = data->fields[idx].bit_offset;

			assert(data->fields[idx].type->type == TY_SIMPLE);

			if (is_signed(data->fields[idx].type->simple))
				out.sign_extend = 1;
		}
	}

	return out;
}

var_id address_load(var_id address, struct type *type) {
	var_id ret = new_variable(type, 1, 1);

	IR_PUSH_LOAD(ret, address);

	return ret;
}

void address_store(var_id address, var_id value) {
	IR_PUSH_STORE(value, address);
}

var_id bitfield_load(struct bitfield_address address, struct type *type) {
	var_id ret = new_variable(type, 1, 1);
	if (address.bitfield == -1) {
		IR_PUSH_LOAD(ret, address.address);
		return ret;
	} else {
		IR_PUSH_LOAD(ret, address.address);
		IR_PUSH_GET_BITS(ret, ret, address.offset, address.bitfield, address.sign_extend);
		return ret;
	}
}

void bitfield_store(struct bitfield_address address, var_id value) {
	if (address.bitfield == -1) {
		IR_PUSH_STORE(value, address.address);
	} else {
		var_id prev = new_variable_sz(get_variable_size(value), 1, 1);
		IR_PUSH_LOAD(prev, address.address);

		IR_PUSH_SET_BITS(prev, prev, value, address.offset, address.bitfield);

		IR_PUSH_STORE(prev, address.address);
	}
}

var_id expression_to_ir_result(struct expr *expr, var_id res) {
	if (!res)
		res = new_variable(expr->data_type, 1, 1);

	switch(expr->type) {
	case E_BINARY_OP: {
		enum operand_type ot;
		if (type_is_pointer(expr->args[0]->data_type) && type_is_pointer(expr->args[1]->data_type)) {
			ot = OT_PTR;
		} else {
			assert(expr->args[0]->data_type->type == TY_SIMPLE &&
				   expr->args[1]->data_type->type == TY_SIMPLE);
			ot = ot_from_st(expr->args[0]->data_type->simple);
		}
		IR_PUSH_BINARY_OPERATOR(expr->binary_op, ot,
								expression_to_ir(expr->args[0]),
								expression_to_ir(expr->args[1]), res);
		} break;

	case E_UNARY_OP:
		IR_PUSH_UNARY_OPERATOR(expr->unary_op, ot_from_type(expr->args[0]->data_type), expression_to_ir(expr->args[0]), res);
		break;

	case E_CONSTANT:
		IR_PUSH_CONSTANT(expr->constant, res);
		break;

	case E_CALL: {
		struct expr *callee = expr->call.callee;

		assert(type_is_pointer(callee->data_type));
		struct type *signature = type_deref(callee->data_type);

		var_id *args = malloc(sizeof *args * expr->call.n_args);
		struct type **arg_types = malloc(sizeof *arg_types * expr->call.n_args);
		for (int i = 0; i < expr->call.n_args; i++) {
			if (i + 1 < signature->n) {
				args[i] = expression_to_ir(expression_cast(expr->call.args[i],
														   signature->children[i + 1]));
				arg_types[i] = signature->children[i + 1];
			} else {
				args[i] = expression_to_ir(expr->call.args[i]);
				arg_types[i] = expr->call.args[i]->data_type;
			}
		}

		var_id func_var = expression_to_ir(callee);
		struct type *func_type = callee->data_type;

		if (func_type->type != TY_POINTER) {
			ERROR("Can't call type %s", dbg_type(func_type));
		}

		IR_PUSH_CALL_VARIABLE(func_var, type_deref(func_type), arg_types, expr->call.n_args, args, res);
		return res;
	}

	case E_VARIABLE:
		// TODO: Remove some of these unnecessary copies in an optimization pass.
		IR_PUSH_COPY(res, expr->variable.id);
		break;

	case E_INDIRECTION:
		IR_PUSH_LOAD(res, expression_to_ir(expr->args[0]));
		break;

	case E_ADDRESS_OF:
		return expression_to_address(expr->args[0]);

	case E_ARRAY_PTR_DECAY:
		IR_PUSH_CAST(res, expr->data_type,
					 expression_to_address(expr->args[0]), type_pointer(expr->args[0]->data_type));
		break;

	case E_POINTER_ADD: {
		pointer_increment(res, expression_to_ir(expr->args[0]),
						  expr->args[1], 0,
						  expr->args[0]->data_type);
	} break;

	case E_POINTER_SUB:
		pointer_increment(res, expression_to_ir(expr->args[0]),
						  expr->args[1], 1,
						  expr->args[0]->data_type);
		break;

	case E_POINTER_DIFF: {
		var_id lhs = expression_to_ir(expr->args[0]),
			rhs = expression_to_ir(expr->args[1]);
		var_id size = expression_to_ir(type_sizeof(type_deref(expr->args[0]->data_type)));

		IR_PUSH_BINARY_OPERATOR(OP_SUB, OT_ULONG, lhs, rhs, res);
		IR_PUSH_BINARY_OPERATOR(OP_DIV, OT_ULONG, res, size, res);
	} break;

	case E_ASSIGNMENT: {
		var_id rhs = expression_to_ir(expression_cast(expr->args[1], expr->args[0]->data_type));
		bitfield_store(expression_to_bitfield_address(expr->args[0]), rhs);
		return rhs;
	}

	case E_ASSIGNMENT_OP: {
		struct expr *a_expr = expr->args[0];

		struct bitfield_address address = expression_to_bitfield_address(a_expr);

		var_id a = bitfield_load(address, a_expr->data_type);
		var_id b = expression_to_ir(expr->args[1]);

		if (expr->assignment_op.postfix)
			IR_PUSH_COPY(res, a);

		if (expr->assignment_op.cast) {
			var_id ac = new_variable(expr->assignment_op.cast, 1, 1);
			IR_PUSH_CAST(ac, expr->assignment_op.cast, a, a_expr->data_type);
			IR_PUSH_BINARY_OPERATOR(expr->assignment_op.op,
									ot_from_type(expr->args[1]->data_type),
									ac, b, ac);
			IR_PUSH_CAST(a, a_expr->data_type, ac, expr->assignment_op.cast);
		} else {
			IR_PUSH_BINARY_OPERATOR(expr->assignment_op.op,
									ot_from_type(expr->args[1]->data_type),
									a, b, a);
		}

		bitfield_store(address, a);
		return expr->assignment_op.postfix ? res : a;
	}

	case E_ASSIGNMENT_POINTER: {
		var_id address = expression_to_address(expr->args[0]);

		var_id prev_val = address_load(address, expr->args[0]->data_type);

		if (expr->assignment_pointer.postfix)
			IR_PUSH_COPY(res, prev_val);

		assert(type_is_pointer(expr->args[0]->data_type));

		pointer_increment(prev_val, prev_val, expr->args[1], expr->assignment_pointer.sub, expr->args[0]->data_type);

		address_store(address, prev_val);
		return expr->assignment_pointer.postfix ? res : prev_val;
	}

	case E_CAST:
		IR_PUSH_CAST(res, expr->cast.target, expression_to_ir(expr->cast.arg), expr->cast.arg->data_type);
		break;

	case E_DOT_OPERATOR: {
		struct type *lhs_type = expr->member.lhs->data_type;
		assert(lhs_type->type == TY_STRUCT);

		struct struct_data *data = lhs_type->struct_data;

		int field_offset = data->fields[expr->member.member_idx].offset;
		int field_bitfield = data->fields[expr->member.member_idx].bitfield;


		var_id address;
		if (!try_expression_to_address(expr->member.lhs, &address)) {
			var_id lhs = expression_to_ir(expr->member.lhs);
			address = new_variable(type_pointer(expr->member.lhs->data_type), 1, 1);
			IR_PUSH_ADDRESS_OF(address, lhs);
		}

		var_id member_address = new_variable(type_pointer(expr->data_type), 1, 1);

		IR_PUSH_GET_OFFSET(member_address, address, field_offset);
		IR_PUSH_LOAD(res, member_address);

		if (field_bitfield != -1) {
			int sign_extend = is_signed(data->fields[expr->member.member_idx].type->simple);
			IR_PUSH_GET_BITS(res, res, data->fields[expr->member.member_idx].bit_offset, field_bitfield, sign_extend);
		}
	} break;

	case E_CONDITIONAL: {
		var_id condition = expression_to_ir(expr->args[0]);
		int is_void = type_is_simple(expr->data_type, ST_VOID);

		block_id block_true = new_block(),
			block_false = new_block(),
			block_end = new_block();

		ir_if_selection(condition, block_true, block_false);

		ir_block_start(block_true);
		var_id true_val = expression_to_ir(expr->args[1]);
		if (!is_void)
			IR_PUSH_COPY(res, true_val);
		ir_goto(block_end);

		ir_block_start(block_false);
		var_id false_val = expression_to_ir(expr->args[2]);
		if (!is_void)
			IR_PUSH_COPY(res, false_val);
		ir_goto(block_end);

		ir_block_start(block_end);
	} break;

	case E_BUILTIN_VA_END:
		break;

	case E_BUILTIN_VA_START: {
		var_id ptr = expression_to_ir(expr->va_start_.array);
		get_current_function()->uses_va = 1;
		IR_PUSH_VA_START(ptr);
	} break;

	case E_BUILTIN_VA_ARG: {
		var_id ptr = expression_to_ir(expr->va_arg_.v);
		IR_PUSH_VA_ARG(ptr, res, expr->va_arg_.t);
	} break;

	case E_BUILTIN_VA_COPY: {
		var_id dest = expression_to_ir(expr->va_copy_.d);
		var_id source = expression_to_ir(expr->va_copy_.s);

		var_id tmp = new_variable(type_deref(expr->va_copy_.d->data_type), 1, 1);

		IR_PUSH_LOAD(tmp, source);
		IR_PUSH_STORE(tmp, dest);
	} break;

	case E_COMPOUND_LITERAL: {
		struct initializer *init = expr->compound_literal.init;
		ir_init_var(init, res);
		variable_set_stack_bucket(res, 0); // compound literals live until end of block.
	} break;

	case E_COMMA:
		expression_to_ir(expr->args[0]);
		expression_to_ir_result(expr->args[1], res);
		break;

	default:
		printf("%d\n", expr->type);
		NOTIMP();
	}
	return res;
}

var_id expression_to_ir(struct expr *expr) {
	var_id res = expression_to_ir_result(expr, 0);
	return res;
}

var_id expression_to_ir_clear_temp(struct expr *expr) {
	var_id res = expression_to_ir(expr);
	variable_set_stack_bucket(res, 0);
	IR_PUSH_CLEAR_STACK_BUCKET(1);
	return res;
}

// Parsing.

void parse_call_parameters(struct expr ***args, int *n_args) {
	#define MAX_ARGUMENTS 128
	struct expr *buffer[MAX_ARGUMENTS];

	int pos = 0;

	for (; pos < MAX_ARGUMENTS; pos++) {
		if (TACCEPT(T_RPAR))
			break;

		if (pos != 0)
			TEXPECT(T_COMMA);

		struct expr *expr = parse_assignment_expression();

		if (!expr)
			ERROR("Expected expression");

		buffer[pos] = expr;
	}

	if (pos == MAX_ARGUMENTS)
		ERROR("Too many arguments passed to function");

	*args = NULL;
	if (pos) {
		*args = malloc(sizeof **args * pos);
		memcpy(*args, buffer, sizeof **args * pos);
	}

	*n_args = pos;
}

struct expr *parse_pratt(int precedence);

struct expr *parse_prefix() {
	if (TACCEPT(T_LPAR)) {
		struct type *cast_type = parse_type_name();

		if (cast_type) {
			TEXPECT(T_RPAR);
			if (T0->type == T_LBRACE) {
				struct initializer *init = parse_initializer(&cast_type);
				return expr_new((struct expr) {
						.type = E_COMPOUND_LITERAL,
						.compound_literal = { cast_type, init }
					});
			} else {
				struct expr *rhs = parse_pratt(PREFIX_PREC);
				if (!rhs)
					ERROR("Expected expression");
				return expression_cast(rhs, cast_type);
			}
		} else {
			struct expr *expr = parse_pratt(0);
			TEXPECT(T_RPAR);
			return expr;
		}
	} else if (TACCEPT(T_INC)) {
 		return EXPR_ASSIGNMENT_OP(OP_ADD, parse_pratt(PREFIX_PREC), EXPR_INT(1), 0);
	} else if (TACCEPT(T_DEC)) {
 		return EXPR_ASSIGNMENT_OP(OP_SUB, parse_pratt(PREFIX_PREC), EXPR_INT(1), 0);
 	} else if (TACCEPT(T_STAR)) {
 		return EXPR_ARGS(E_INDIRECTION, parse_pratt(PREFIX_PREC));
	} else if (TACCEPT(T_ADD)) {
		return EXPR_UNARY_OP(UOP_PLUS, parse_pratt(PREFIX_PREC));
	} else if (TACCEPT(T_SUB)) {
		return EXPR_UNARY_OP(UOP_NEG, parse_pratt(PREFIX_PREC));
	} else if (TACCEPT(T_NOT)) {
		struct expr *rhs = parse_pratt(PREFIX_PREC);
		return EXPR_ARGS(E_CONDITIONAL, rhs,
						 EXPR_INT(0),
						 EXPR_INT(1));
	} else if (TACCEPT(T_BNOT)) {
		return EXPR_UNARY_OP(UOP_BNOT, parse_pratt(PREFIX_PREC));
	} else if (TACCEPT(T_AMP)) {
 		return EXPR_ARGS(E_ADDRESS_OF, parse_pratt(PREFIX_PREC));
	} else if (TACCEPT(T_KSIZEOF)) {
		struct type *type = NULL;
		struct token t = *T0;
		if (TACCEPT(T_LPAR)) {
			type = parse_type_name();
			if (type) {
				TEXPECT(T_RPAR);
			} else {
				t_push(t);
				type = parse_pratt(PREFIX_PREC)->data_type;
			} 
		} else {
			type = parse_pratt(PREFIX_PREC)->data_type;
		}

		return type_sizeof(type);
	} else if (TACCEPT(T_KALIGNOF)) {
		NOTIMP();
	} else if (T0->type == T_IDENT) {
		struct symbol_identifier *sym = symbols_get_identifier(T0->str);

		if (!sym) {
			PRINT_POS(T0->pos);
			ERROR("Could not find identifier %s", T0->str);
		}

		switch (sym->type) {
		case IDENT_LABEL:
			TNEXT();
			return expr_new((struct expr) {
					.type = E_CONSTANT,
					.constant = {
						.type = CONSTANT_LABEL,
						.data_type = sym->label.type,
						.label.label = register_label_name(sym->label.name),
						.label.offset = 0
					}
				});

		case IDENT_VARIABLE:
			TNEXT();
			return expr_new((struct expr) {
					.type = E_VARIABLE,
					.variable = { sym->variable.id, sym->variable.type }
				});

		case IDENT_VARIABLE_LENGTH_ARRAY:
			TNEXT();
			return expr_new((struct expr) {
					.type = E_VARIABLE_LENGTH_ARRAY,
					.variable_length_array = { sym->variable.id, sym->variable.type }
				});

		case IDENT_CONSTANT:
			TNEXT();
			return expr_new((struct expr) {
					.type = E_CONSTANT,
					.constant = sym->constant
				});

		default:
			printf("%s\n", T0->str);
			NOTIMP();
		}
	} else if (T0->type == T_STRING) {
		const char *str = T0->str;
		TNEXT();
		return EXPR_STR(str);
	} else if (T0->type == T_NUM) {
		struct constant c = constant_from_string(T0->str);
		TNEXT();
		return expr_new((struct expr) {
				.type = E_CONSTANT,
				.constant = c
			});
	} else if (T0->type == T_CHARACTER_CONSTANT) {
		const char *str = T0->str;
		TNEXT();
		return EXPR_INT(character_constant_to_int(str));
	} else if (TACCEPT(T_CHAR)) {
		NOTIMP();
	} else if (TACCEPT(T_KGENERIC)) {
		TEXPECT(T_LPAR);
		struct expr *expr = parse_assignment_expression();
		if (!expr)
			ERROR("Expected expression.");

		decay_array(&expr);
		// Remove all constness. This is a part of lvalue conversions.
		// TODO: Make removing constness a bit more robust.
		struct type *match_type = type_make_const(expr->data_type, 0);

		struct expr *res = NULL;
		int res_is_default = 0;

		while (TACCEPT(T_COMMA)) {
			int default_ = 0;
			struct type *type = NULL;
			if (TACCEPT(T_KDEFAULT)) {
				res_is_default = 1;
				default_ = 1;
			} else {
				type = parse_type_name();
			}
			TEXPECT(T_COLON);
			struct expr *rhs = parse_assignment_expression();
			if (default_) {
				if (!res)
					res = rhs;
			} else {
				if (type == match_type) {
					if (res && !res_is_default)
						ERROR("More than one compatible type in _Generic association list, %s",
							  strdup(dbg_type(type)));
					res = rhs;
				}
			}
		}

		if (!res)
			ERROR("No type matched the expresison in _Generic");

		TEXPECT(T_RPAR);
		return res;
	} else if (TACCEPT(T_KVA_START)) {
		TEXPECT(T_LPAR);
		struct expr *v = parse_assignment_expression();
		TEXPECT(T_COMMA);
		struct expr *l = parse_assignment_expression();
		TEXPECT(T_RPAR);

		return expr_new((struct expr) {
				.type = E_BUILTIN_VA_START,
				.va_start_ = {v, l}
			});
	} else if (TACCEPT(T_KVA_COPY)) {
		TEXPECT(T_LPAR);
		struct expr *d = parse_assignment_expression();
		TEXPECT(T_COMMA);
		struct expr *s = parse_assignment_expression();
		TEXPECT(T_RPAR);
		return expr_new((struct expr) {
				.type = E_BUILTIN_VA_COPY,
				.va_copy_ = {d, s}
			});
	} else if (TACCEPT(T_KVA_END)) {
		TEXPECT(T_LPAR);
		struct expr *v = parse_assignment_expression();
		TEXPECT(T_RPAR);
		return expr_new((struct expr) {
				.type = E_BUILTIN_VA_END,
				.va_end_ = {v}
		 	});
	} else if (TACCEPT(T_KVA_ARG)) {
		TEXPECT(T_LPAR);
		struct expr *v = parse_assignment_expression();
		TEXPECT(T_COMMA);
		struct type *t = parse_type_name();
		if (!t)
			ERROR("Expected typename in var_arg");
		TEXPECT(T_RPAR);
		return expr_new((struct expr) {
				.type = E_BUILTIN_VA_ARG,
				.va_arg_ = {v, t}
			});
	} else if (TACCEPT(T_KFUNC)) {
 		return EXPR_STR(get_current_function_name());
	}
	return NULL;
}

struct expr *parse_pratt(int precedence) {
	struct expr *lhs = parse_prefix();

	if (!lhs)
		return NULL;

	while (precedence < precedence_get(T0->type, 1)) {
		int new_prec = precedence_get(T0->type, 0);

		static int ops[T_COUNT][2] = {
			[T_COMMA] = {1, E_COMMA},
			[T_A] = {1, E_ASSIGNMENT},

			[T_MULA] = {2, OP_MUL},
			[T_DIVA] = {2, OP_DIV},
			[T_MODA] = {2, OP_MOD},
			[T_ADDA] = {2, OP_ADD},
			[T_SUBA] = {2, OP_SUB},
			[T_LSHIFTA] = {2, OP_LSHIFT},
			[T_RSHIFTA] = {2, OP_RSHIFT},
			[T_BANDA] = {2, OP_BAND},
			[T_XORA] = {2, OP_BXOR},
			[T_BORA] = {2, OP_BOR},

			[T_BOR] = {3, OP_BOR},
			[T_XOR] = {3, OP_BXOR},
			[T_AMP] = {3, OP_BAND},
			[T_EQ] = {3, OP_EQUAL},
			[T_NEQ] = {3, OP_NOT_EQUAL},
			[T_LEQ] = {3, OP_LESS_EQ},
			[T_GEQ] = {3, OP_GREATER_EQ},
			[T_L] = {3, OP_LESS},
			[T_G] = {3, OP_GREATER},
			[T_LSHIFT] = {3, OP_LSHIFT},
			[T_RSHIFT] = {3, OP_RSHIFT},
			[T_ADD] = {3, OP_ADD},
			[T_SUB] = {3, OP_SUB},
			[T_STAR] = {3, OP_MUL},
			[T_DIV] = {3, OP_DIV},
			[T_MOD] = {3, OP_MOD},
		};

		int tok_type = T0->type;
		if (ops[tok_type][0] == 1) {
			TNEXT();
			lhs = EXPR_ARGS(ops[tok_type][1], lhs, parse_pratt(new_prec));
		} else if (ops[tok_type][0] == 2) {
			TNEXT();
			lhs = EXPR_ASSIGNMENT_OP(ops[tok_type][1], lhs, parse_pratt(new_prec), 0);
		} else if (ops[tok_type][0] == 3) {
			TNEXT();
			lhs = EXPR_BINARY_OP(ops[tok_type][1], lhs, parse_pratt(new_prec));
		} else if (TACCEPT(T_QUEST)) {
			struct expr *mid = parse_expression();
			TEXPECT(T_COLON);
			struct expr *rhs = parse_pratt(new_prec);
			lhs = EXPR_ARGS(E_CONDITIONAL, lhs, mid, rhs);
		} else if (TACCEPT(T_OR)) {
			// A || B -> A ? 1 : (B ? 1 : 0)
			struct expr *rhs = parse_pratt(new_prec);
			lhs = EXPR_ARGS(E_CONDITIONAL, lhs, EXPR_INT(1),
							EXPR_ARGS(E_CONDITIONAL, rhs, EXPR_INT(1), EXPR_INT(0)));
		} else if (TACCEPT(T_AND)) {
			// A && B -> A ? (B ? 1 : 0) : 0
			struct expr *rhs = parse_pratt(new_prec);
			lhs = EXPR_ARGS(E_CONDITIONAL, lhs,
							EXPR_ARGS(E_CONDITIONAL, rhs, EXPR_INT(1), EXPR_INT(0)),
							EXPR_INT(0));
		}
		// Postfix operators
		else if (TACCEPT(T_LBRACK)) {
			struct expr *index = parse_expression();
			TEXPECT(T_RBRACK);
			lhs = expr_new((struct expr) {
					.type = E_INDIRECTION,
					.args = {
						EXPR_BINARY_OP(OP_ADD, lhs, index)
					}
				});
		} else if (TACCEPT(T_LPAR)) {
			struct expr **args = NULL;
			int n_args;

			parse_call_parameters(&args, &n_args);

			lhs = expr_new((struct expr) {
					.type = E_CALL,
					.call = { lhs, n_args, args }
				});

		} else if (TACCEPT(T_DOT)) {
			const char *identifier = T0->str;
			TNEXT();
			struct type *lhs_type = lhs->data_type;
			int idx = type_member_idx(lhs_type, identifier);

			lhs = expr_new((struct expr) {
					.type = E_DOT_OPERATOR,
					.member = { lhs, idx }
				});
		} else if (TACCEPT(T_ARROW)) {
			const char *identifier = T0->str;
			TNEXT();
			decay_array(&lhs);
			struct type *lhs_type = type_deref(lhs->data_type);
			int idx = type_member_idx(lhs_type, identifier);

			lhs = expr_new((struct expr) {
					.type = E_DOT_OPERATOR,
					.member = {
						EXPR_ARGS(E_INDIRECTION, lhs), idx
					}
				});
		} else if (TACCEPT(T_INC)) {
			lhs = EXPR_ASSIGNMENT_OP(OP_ADD, lhs, EXPR_INT(1), 1);
		} else if (TACCEPT(T_DEC)) {
			lhs = EXPR_ASSIGNMENT_OP(OP_SUB, lhs, EXPR_INT(1), 1);
		} else {
			break;
		}
	}

	return lhs;
}

struct expr *parse_assignment_expression() {
	return parse_pratt(ASSIGNMENT_PREC);
}

struct expr *parse_expression() {
	return parse_pratt(0);
}

// Constant expressions.
int evaluate_constant_expression(struct expr *expr,
								 struct constant *constant) {
	switch (expr->type) {
	case E_CONSTANT:
		*constant = expr->constant;
		return 1;

	case E_CAST: {
		struct constant rhs;
		if (!evaluate_constant_expression(expr->cast.arg, &rhs))
			return 0;
		if (rhs.type == CONSTANT_LABEL)
			return 0;
		if (type_is_pointer(rhs.data_type) &&
			!type_is_pointer(expr->cast.target))
			return 0;
		*constant = constant_cast(rhs, expr->cast.target);
	} break;

	case E_POINTER_SUB:
	case E_POINTER_ADD: {
		struct constant lhs, rhs;
		if (!evaluate_constant_expression(expr->args[0], &lhs))
			return 0;
		if (!evaluate_constant_expression(expr->args[1], &rhs))
			return 0;

		if (lhs.type == CONSTANT_LABEL) {
			return 0;
		}

		if (lhs.type == CONSTANT_LABEL_POINTER &&
			type_is_simple(rhs.data_type, ST_INT)) {
			*constant = lhs;
			int size = calculate_size(type_deref(lhs.data_type));

			if (expr->type == E_POINTER_ADD) {
				constant->label.offset += rhs.int_d * size;
			} else if (expr->type == E_POINTER_SUB) {
				constant->label.offset -= rhs.int_d * size;
			}

			return 1;
		}

		return 0;
	} break;

	case E_BINARY_OP: {
		struct constant lhs, rhs;
		if (!evaluate_constant_expression(expr->args[0], &lhs))
			return 0;
		if (!evaluate_constant_expression(expr->args[1], &rhs))
			return 0;
		if (lhs.type != CONSTANT_TYPE ||
			rhs.type != CONSTANT_TYPE) {
			return 0;
		}
		*constant = operators_constant(expr->binary_op, lhs, rhs);
	} break;

	case E_UNARY_OP: {
		struct constant rhs;
		if (!evaluate_constant_expression(expr->args[0], &rhs))
			return 0;
		if (rhs.type == CONSTANT_LABEL)
			return 0;
		*constant = operators_constant_unary(expr->unary_op, rhs);
	} break;

	case E_ARRAY_PTR_DECAY: {
		if (expr->args[0]->type != E_CONSTANT)
			return 0;

		struct constant c = expr->args[0]->constant;

		if (c.type == CONSTANT_LABEL) {
			*constant = c;
			constant->type = CONSTANT_LABEL_POINTER;
			constant->data_type = type_pointer(expr->args[0]->data_type->children[0]);
		} else {
			*constant = c;
			constant->data_type = type_pointer(expr->args[0]->data_type->children[0]);
			constant->type = CONSTANT_LABEL_POINTER;
			constant->label.label = rodata_register(c.str_d);
			constant->label.offset = 0;
		}
	} break;

	case E_CONDITIONAL: {
		struct constant lhs, mid, rhs;
		if (!evaluate_constant_expression(expr->args[0], &lhs))
			return 0;
		if (!evaluate_constant_expression(expr->args[1], &mid))
			return 0;
		if (!evaluate_constant_expression(expr->args[2], &rhs))
			return 0;

		if (lhs.type == CONSTANT_LABEL ||
			mid.type == CONSTANT_LABEL ||
			rhs.type == CONSTANT_LABEL)
			return 0;

		assert(lhs.type == CONSTANT_TYPE);
		assert(mid.type == CONSTANT_TYPE);
		assert(rhs.type == CONSTANT_TYPE);

		assert(lhs.data_type == type_simple(ST_INT));
		if (lhs.int_d) {
			*constant = mid;
		} else {
			*constant = rhs;
		}
	} break;

	case E_ADDRESS_OF: {
		struct constant operand;
		if (!evaluate_constant_expression(expr->args[0], &operand)) {
			return 0;
		}

		if (operand.type == CONSTANT_LABEL) {
			*constant = operand;
			constant->type = CONSTANT_LABEL_POINTER;
			constant->data_type = expr->data_type;
		} else {
			return 0;
		}
	} break;

	case E_INDIRECTION: {
		struct constant operand;
		if (!evaluate_constant_expression(expr->args[0], &operand)) {
			return 0;
		}

		if (operand.type == CONSTANT_LABEL_POINTER) {
			*constant = operand;
			constant->type = CONSTANT_LABEL;
			constant->data_type = expr->data_type;
		} else {
			return 0;
		}
	} break;

	default:
		return 0;
	}
	return 1;
}

struct expr *expression_cast(struct expr *expr, struct type *type) {
	decay_array(&expr);
	return (expr->data_type == type) ? expr :
		expr_new((struct expr) {
				.type = E_CAST,
				.cast = {expr, type}				   
			});
}

struct constant *expression_to_constant(struct expr *expr) {
	return expr->type == E_CONSTANT ? &expr->constant : NULL;
}

int constant_is_zero(struct constant *c) {
	if (c->type != CONSTANT_TYPE)
		return 0;

	if (c->data_type->type == TY_SIMPLE) {
		switch (c->data_type->simple) {
		case ST_INT:
			return c->int_d == 0;
		case ST_UINT:
			return c->uint_d == 0;
		case ST_LONG:
			return c->long_d == 0;
		case ST_ULONG:
			return c->ulong_d == 0;
		case ST_LLONG:
			return c->llong_d == 0;
		case ST_ULLONG:
			return c->ullong_d == 0;
		case ST_CHAR:
			return c->char_d == 0;
		case ST_UCHAR:
			return c->uchar_d == 0;
		case ST_SCHAR:
			return c->schar_d == 0;
		case ST_SHORT:
			return c->short_d == 0;
		case ST_USHORT:
			return c->ushort_d == 0;
		default:
			return 0;
		}
	} else if (c->data_type->type == TY_POINTER) {
		return c->ulong_d == 0;
	} else {
		return 0;
	}
}

int expression_is_zero(struct expr *expr) {
	struct constant *c = expression_to_constant(expr);

	return c && constant_is_zero(c);
}
