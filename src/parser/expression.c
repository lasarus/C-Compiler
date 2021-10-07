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

struct expr *do_integer_promotion(struct expr *expr) {
	struct type *current_type = expr->data_type;
	
	if (current_type->type != TY_SIMPLE) {
		return expr;
	}

	enum simple_type simple_type = current_type->simple;

	switch(simple_type) {
	case ST_BOOL:
	case ST_CHAR:
	case ST_SCHAR:
	case ST_UCHAR:
	case ST_SHORT:
	case ST_USHORT:
		return expression_cast(expr, type_simple(ST_INT));
	default:
		return expr;
	}
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

	case E_SYMBOL:
		return expr->symbol.type;

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
			ERROR("Can't call type %s\n", type_to_string(callee_type));
		}
	}

	case E_VARIABLE:
		return get_variable_type(expr->variable.id);

	case E_INDIRECTION:
		return type_deref(expr->args[0]->data_type);

	case E_ADDRESS_OF:
		return type_pointer(expr->args[0]->data_type);

	case E_ARRAY_PTR_DECAY:
		return type_pointer(expr->args[0]->data_type->children[0]);

	case E_POINTER_ADD:
	case E_ASSIGNMENT:
	case E_ASSIGNMENT_OP:
	case E_ASSIGNMENT_POINTER_ADD:
	case E_POSTFIX_INC:
	case E_POSTFIX_DEC:
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

int dont_decay_ptr[E_NUM_TYPES] = {
	[E_ADDRESS_OF] = 1,
	[E_ARRAY_PTR_DECAY] = 1,
};

int num_args[E_NUM_TYPES] = {
	[E_POSTFIX_INC] = 1,
	[E_POSTFIX_DEC] = 1,
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

int does_integer_conversion[E_NUM_TYPES] = {
	[E_UNARY_OP] = 1,
	[E_BINARY_OP] = 1,
	[E_CONDITIONAL] = 1,
};

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
				  strdup(type_to_string(a)),
				  strdup(type_to_string(b)));
		}
	} else if (type_is_arithmetic(a) &&
			   type_is_arithmetic(b)) {
		convert_arithmetic(&expr->args[1], &expr->args[2]);
	} else if (a != b) {
		ERROR("Invalid combination of data types:\n%s and %s\n",
			  strdup(type_to_string(a)),
			  strdup(type_to_string(b)));
	}
}

// This applies all the necessary transformations to binary operators
void fix_binary_operator(struct expr *expr) {
	if (expr->type != E_BINARY_OP)
		return;

	convert_arithmetic(&expr->args[0], &expr->args[1]);

	int lhs_ptr = type_is_pointer(expr->args[0]->data_type),
		rhs_ptr = type_is_pointer(expr->args[1]->data_type);

	switch (expr->binary_op) {
	case OP_SUB:
		if (lhs_ptr && rhs_ptr) {
			expr->type = E_POINTER_DIFF;
		} else if (lhs_ptr) {
			NOTIMP();
		} else if (rhs_ptr) {
			NOTIMP();
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
	default: // Do nothing.
		break;
	}
}

void fix_assignment_operators(struct expr *expr) {
	if (expr->type != E_ASSIGNMENT_OP)
		return;

	convert_arithmetic(&expr->args[0], &expr->args[1]);
	int lhs_ptr = type_is_pointer(expr->args[0]->data_type);

	switch (expr->binary_op) {
	case OP_ADD:
		if (lhs_ptr)
			expr->type = E_ASSIGNMENT_POINTER_ADD;
		break;
	case OP_SUB:
		if (lhs_ptr)
			NOTIMP();
		break;
	default:
		break;
	}
}

int evaluate_constant_expression(struct expr *expr,
 								 struct constant *constant);

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
	
	cast_conditional(&expr);
	fix_assignment_operators(&expr);
	fix_binary_operator(&expr);

	int integer_promotion = does_integer_conversion[expr.type];
	if (integer_promotion) {
		for (int i = 0; i < num_args[expr.type]; i++) {
			expr.args[i] = do_integer_promotion(expr.args[i]);
		}
	}

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

	struct expr *ret = malloc(sizeof *ret);
	*ret = expr;

	return ret;
}

// Loads pointer into return value.
var_id expression_to_address(struct expr *expr) {
	switch (expr->type) {
	case E_INDIRECTION:
		return expression_to_ir(expr->args[0]);

	case E_VARIABLE: {
		var_id address = new_variable(type_pointer(expr->data_type), 1);
		IR_PUSH_ADDRESS_OF(address, expr->variable.id);
		return address;
	}

	case E_DOT_OPERATOR: {
		var_id address = expression_to_address(expr->member.lhs);
		var_id member_address = new_variable(type_pointer(expr->data_type), 1);
		IR_PUSH_GET_MEMBER(member_address, address, expr->member.member_idx);
		return member_address;
	} break;

	case E_SYMBOL: {
		var_id ptr_result = new_variable(type_pointer(expr->data_type), 1);
		IR_PUSH_GET_SYMBOL_PTR(expr->symbol.name, ptr_result);
		return ptr_result;
	} break;

	default:
		ERROR("NOt imp %d\n", expr->type);
		NOTIMP();
	}
}

struct type *get_address_type(var_id address) {
	return type_deref(get_variable_type(address));
}

var_id address_load(var_id address) {
	struct type *type = get_address_type(address);
	var_id ret = new_variable(type, 1);

	IR_PUSH_LOAD(ret, address);

	return ret;
}

void address_store(var_id address, var_id value) {
	IR_PUSH_STORE(value, address);
}

var_id expression_to_ir_result(struct expr *expr, var_id res) {
	if (!res)
		res = new_variable(expr->data_type, 1);

	switch(expr->type) {
	case E_BINARY_OP:
		IR_PUSH_BINARY_OPERATOR(expr->binary_op,
								expression_to_ir(expr->args[0]),
								expression_to_ir(expr->args[1]), res);
		break;

	case E_UNARY_OP:
		IR_PUSH_UNARY_OPERATOR(expr->unary_op, expression_to_ir(expr->args[0]), res);
		break;

	case E_CONSTANT:
		IR_PUSH_CONSTANT(expr->constant, res);
		break;

	case E_CALL: {
		struct expr *callee = expr->call.callee;

		struct type *signature = NULL;
		switch (callee->type) {
		case E_SYMBOL:
			signature = callee->symbol.type;
			break;
		default:
			assert(type_is_pointer(callee->data_type));
			signature = type_deref(callee->data_type);
			break;
		}

		var_id *args = malloc(sizeof *args * expr->call.n_args);
		for (int i = 0; i < expr->call.n_args; i++) {
			if (i + 1 < signature->n) {
				args[i] = expression_to_ir(expression_cast(expr->call.args[i],
														   signature->children[i + 1]));
			} else {
				args[i] = expression_to_ir(expr->call.args[i]);
			}
		}

		var_id func_var = expression_to_ir(callee);
		struct type *func_type = get_variable_type(func_var);

		if (func_type->type != TY_POINTER) {
			ERROR("Can't call type %s", type_to_string(func_type));
		}
		assert(type_is_pointer(func_type));

		IR_PUSH_CALL_VARIABLE(func_var, func_type, expr->call.n_args, args, res);
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
		IR_PUSH_CAST(res, expression_to_address(expr->args[0]), expr->data_type);
		break;

	case E_POINTER_ADD:
		IR_PUSH_POINTER_INCREMENT(res, expression_to_ir(expr->args[0]),
								  expression_to_ir(expr->args[1]));
		break;

	case E_POINTER_DIFF:
		// TODO: Make this work on variable length objects.
		IR_PUSH_POINTER_DIFF(res, expression_to_ir(expr->args[0]),
							 expression_to_ir(expr->args[1]));
		break;

	case E_POSTFIX_DEC:
	case E_POSTFIX_INC: {
		struct type *type = get_variable_type(res);

		var_id address = expression_to_address(expr->args[0]);
		var_id value = address_load(address);
		IR_PUSH_COPY(res, value);

		if (type->type == TY_POINTER) {
			var_id constant_one = expression_to_ir(EXPR_INT(1));
			if (expr->type == E_POSTFIX_DEC)
				NOTIMP();
			IR_PUSH_POINTER_INCREMENT(value, value, constant_one);
		} else {
			var_id constant_one = expression_to_ir(expression_cast(EXPR_INT(1), type));
			if (expr->type == E_POSTFIX_DEC)
				IR_PUSH_BINARY_OPERATOR(OP_SUB, value, constant_one, value);
			else
				IR_PUSH_BINARY_OPERATOR(OP_ADD, value, constant_one, value);
		}
		address_store(address, value);
		return res;
	} break;

	case E_ASSIGNMENT: {
		var_id address = expression_to_address(expr->args[0]);
		var_id rhs = expression_to_ir(expression_cast(expr->args[1], expr->args[0]->data_type));

		address_store(address, rhs);
		return rhs;
	}

	case E_ASSIGNMENT_OP: {
		struct expr *lhs = expr->args[0];
		if (lhs->type == E_CAST)
			NOTIMP();
		var_id address = expression_to_address(expr->args[0]);
		var_id rhs = expression_to_ir(expr->args[1]);

		var_id prev_val = address_load(address);
		enum operator_type ot = expr->binary_op;

		if (type_is_pointer(expr->args[0]->data_type) ||
			type_is_pointer(expr->args[1]->data_type)) {
			PRINT_POS(T0->pos);
			ERROR("OP: %d\n", ot);
		}

		IR_PUSH_BINARY_OPERATOR(ot, prev_val, rhs, prev_val);

		address_store(address, prev_val);
		return prev_val;
	}

	case E_ASSIGNMENT_POINTER_ADD: {
		var_id address = expression_to_address(expr->args[0]);
		var_id rhs = expression_to_ir(expr->args[1]);

		var_id prev_val = address_load(address);

		assert(type_is_pointer(get_variable_type(prev_val)));

		IR_PUSH_POINTER_INCREMENT(prev_val, prev_val, rhs);

		address_store(address, prev_val);
		return prev_val;
	}

	case E_CAST:
		IR_PUSH_CAST(res, expression_to_ir(expr->cast.arg), expr->cast.target);
		break;

	case E_DOT_OPERATOR: {
		var_id lhs = expression_to_ir(expr->member.lhs);
		var_id address = new_variable(type_pointer(expr->member.lhs->data_type), 1);
		var_id member_address = new_variable(type_pointer(expr->data_type), 1);
		IR_PUSH_ADDRESS_OF(address, lhs);
		IR_PUSH_GET_MEMBER(member_address, address, expr->member.member_idx);
		IR_PUSH_LOAD(res, member_address);
	} break;

	case E_CONDITIONAL: {
		var_id condition = expression_to_ir(expr->args[0]);

		block_id block_true = new_block(),
			block_false = new_block(),
			block_end = new_block();

		IR_PUSH(.type = IR_IF_SELECTION,
				.if_selection = { condition, block_true, block_false });

		IR_PUSH_START_BLOCK(block_true);
		var_id true_val = expression_to_ir(expr->args[1]);
		IR_PUSH_COPY(res, true_val);
		IR_PUSH_GOTO(block_end);

		IR_PUSH_START_BLOCK(block_false);
		var_id false_val = expression_to_ir(expr->args[2]);
		IR_PUSH_COPY(res, false_val);
		IR_PUSH_GOTO(block_end);

		IR_PUSH_START_BLOCK(block_end);
	} break;

	case E_BUILTIN_VA_END:
		break;

	case E_BUILTIN_VA_START: {
		var_id ptr = expression_to_ir(expr->va_start_.array);
		IR_PUSH_VA_START(ptr);
	} break;

	case E_BUILTIN_VA_ARG: {
		var_id ptr = expression_to_ir(expr->va_arg_.v);
		IR_PUSH_VA_ARG(ptr, res, expr->va_arg_.t);
	} break;

	case E_BUILTIN_VA_COPY: {
		var_id dest = expression_to_ir(expr->va_copy_.d);
		var_id source = expression_to_ir(expr->va_copy_.s);

		var_id tmp = new_variable(type_deref(get_variable_type(dest)), 1);

		IR_PUSH_LOAD(tmp, source);
		IR_PUSH_STORE(tmp, dest);
	} break;

	case E_COMPOUND_LITERAL: {
		struct initializer *init = expr->compound_literal.init;
		IR_PUSH_SET_ZERO(res);

		for (int i = 0; i < init->n; i++) {
			IR_PUSH_ASSIGN_CONSTANT_OFFSET(res, expression_to_ir(init->pairs[i].expr), init->pairs[i].offset);
		}
	} break;

	case E_SYMBOL: {
		var_id ptr = new_variable(type_pointer(expr->symbol.type), 1);
		IR_PUSH_GET_SYMBOL_PTR(expr->symbol.name, ptr);
		IR_PUSH_LOAD(res, ptr);
		break;
	}

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
	return expression_to_ir_result(expr, 0);
}

// Parsing.

struct expr *parse_builtins(void) {
	if (TACCEPT(T_KVA_START)) {
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
		return EXPR_STR(get_current_function());
	} else {
		return NULL;
	}
}

struct expr *parse_primary_expression(int starts_with_lpar) {
	if (starts_with_lpar || TACCEPT(T_LPAR)) {
		struct expr *expr = parse_expression();
		TEXPECT(T_RPAR);
		return expr;
	} else if (T_ISNEXT(T_IDENT)) {
		struct symbol_identifier *sym = symbols_get_identifier(T0->str);

		if (!sym) {
			PRINT_POS(T0->pos);
			ERROR("Could not find identifier %s", T0->str);
		}

		switch (sym->type) {
		case IDENT_LABEL:
			TNEXT();
			return expr_new((struct expr) {
					.type = E_SYMBOL,
					.symbol = { sym->label.name, sym->label.type }
				});
			break;

		case IDENT_VARIABLE:
			TNEXT();
			return expr_new((struct expr) {
					.type = E_VARIABLE,
					.variable = { sym->variable }
				});
			break;

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

		NOTIMP();
	} else if (T_ISNEXT(T_NUM)) {
		struct constant c = constant_from_string(T0->str);
		TNEXT();
		return expr_new((struct expr) {
				.type = E_CONSTANT,
				.constant = c
			});
	} else if (T_ISNEXT(T_STRING)) {
		const char *str = T0->str;
		TNEXT();
		return EXPR_STR(str);
	} else if (T_ISNEXT(T_CHARACTER_CONSTANT)) {
		const char *str = T0->str;
		TNEXT();
		return EXPR_INT(escaped_to_str(str));
	} else if (T_ISNEXT(T_CHAR)) {
		NOTIMP();
	} else if (TACCEPT(T_KGENERIC)) {
		NOTIMP();
	} else {

	}
	return parse_builtins();
}

struct expr *parse_postfix_expression(int starts_with_lpar, struct expr *starting_lhs) {
	struct expr *lhs = starting_lhs ? starting_lhs : parse_primary_expression(starts_with_lpar);

	do {
		if (TACCEPT(T_LBRACK)) {
			struct expr *index = parse_expression();
			TEXPECT(T_RBRACK);
			lhs = expr_new((struct expr) {
					.type = E_INDIRECTION,
					.args = {
						EXPR_BINARY_OP(OP_ADD, lhs, index)
					}
				});
		} else if (TACCEPT(T_LPAR)) {
			struct position position = T0->pos;
			struct expr *buffer[128];

			int pos = 0;

			for (; pos < 128; pos++) {
				if (TACCEPT(T_RPAR))
					break;

				if (pos != 0)
					TEXPECT(T_COMMA);

				struct expr *expr = parse_assignment_expression();

				if (!expr)
					ERROR("Expected expression");

				buffer[pos] = expr;
			}

			if (pos == 128)
				NOTIMP();

			struct expr **args = malloc(sizeof *args * pos);
			memcpy(args, buffer, sizeof *args * pos);

			lhs = expr_new((struct expr) {
					.type = E_CALL,
					.call = { lhs, pos, args },
					.pos = position
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
			struct type *lhs_type = type_deref(lhs->data_type);
			int idx = type_member_idx(lhs_type, identifier);

			lhs = expr_new((struct expr) {
					.type = E_DOT_OPERATOR,
					.member = {
						EXPR_ARGS(E_INDIRECTION, lhs), idx
					}
				});
		} else if (TACCEPT(T_INC)) {
			lhs = EXPR_ARGS(E_POSTFIX_INC, lhs);
		} else if (TACCEPT(T_DEC)) {
			lhs = EXPR_ARGS(E_POSTFIX_DEC, lhs);
		} else {
			break;
		}
	} while (1);

	return lhs;
}

struct expr *parse_cast_expression(void);
struct expr *parse_unary_expression() {
	if (TACCEPT(T_INC)) {
		return EXPR_ASSIGNMENT_OP(OP_ADD, parse_unary_expression(), EXPR_INT(1));
	} else if (TACCEPT(T_DEC)) {
		return EXPR_ASSIGNMENT_OP(OP_SUB, parse_unary_expression(), EXPR_INT(1));
	} else if (TACCEPT(T_STAR)) {
		return EXPR_ARGS(E_INDIRECTION, parse_cast_expression());
	} else if (TACCEPT(T_AMP)) {
		return EXPR_ARGS(E_ADDRESS_OF, parse_cast_expression());
	} else if (TACCEPT(T_ADD)) {
		return EXPR_UNARY_OP(UOP_PLUS, parse_cast_expression());
	} else if (TACCEPT(T_SUB)) {
		return EXPR_UNARY_OP(UOP_NEG, parse_cast_expression());
	} else if (TACCEPT(T_BNOT)) {
		return EXPR_UNARY_OP(UOP_BNOT, parse_cast_expression());
	} else if (TACCEPT(T_NOT)) {
		struct expr *rhs = parse_cast_expression();
		return EXPR_ARGS(E_CONDITIONAL, rhs,
						 EXPR_INT(0),
						 EXPR_INT(1));
	} else if (TACCEPT(T_KSIZEOF)) {
		struct type *type = NULL;
		if (TACCEPT(T_LPAR)) {
			type = parse_type_name();
			if (!type)
				type = parse_expression()->data_type;
			TEXPECT(T_RPAR);
		} else {
			type = parse_unary_expression()->data_type;
		}
		// TODO: Size should perhaps not be an integer.
		struct constant c = {.type = CONSTANT_TYPE, .data_type = type_simple(ST_INT), .int_d = calculate_size(type) };

		return expr_new((struct expr) {
				.type = E_CONSTANT,
				.constant = c
			});
	} else if (TACCEPT(T_KALIGNOF)) {
		NOTIMP();
	} else {
		return parse_postfix_expression(0, NULL);
	}
}

struct expr *parse_paren_or_cast_expression() {
	if (!TACCEPT(T_LPAR))
		return NULL;

	struct type *cast_type = parse_type_name();
	if (cast_type) {
		TEXPECT(T_RPAR);
		if (T0->type == T_LBRACE) {
			struct initializer *init = parse_initializer(&cast_type);

			struct expr *compound = expr_new((struct expr) {
					.type = E_COMPOUND_LITERAL,
					.compound_literal = { cast_type, init }
				});

			return parse_postfix_expression(0, compound);
		} else {
			struct expr *rhs = parse_cast_expression();
			if (!rhs)
				ERROR("Expected expression");
			return expression_cast(rhs, cast_type);
		}
	} else {
		return parse_postfix_expression(1, NULL);
	}
}

struct expr *parse_cast_expression(void) {
	struct expr *ret = parse_paren_or_cast_expression();
	if (ret)
		return ret;

	return parse_unary_expression();
}

struct expr *parse_pratt(int precedence) {
	struct expr *lhs = parse_cast_expression();

	if (!lhs)
		return NULL;

	while (precedence < precedence_get(T0->type, PREC_INFIX, 1)) {
		int new_prec = precedence_get(T0->type, PREC_INFIX, 0);

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
			lhs = EXPR_ASSIGNMENT_OP(ops[tok_type][1], lhs, parse_pratt(new_prec));
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
	}

	return lhs;
}

struct expr *parse_assignment_expression() {
	return parse_pratt(5);
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
		*constant = constant_cast(rhs, expr->cast.target);
	} break;

	case E_BINARY_OP: {
		struct constant lhs, rhs;
		if (!evaluate_constant_expression(expr->args[0], &lhs))
			return 0;
		if (!evaluate_constant_expression(expr->args[1], &rhs))
			return 0;
		*constant = operators_constant(expr->binary_op, lhs, rhs);
	} break;

	case E_UNARY_OP: {
		struct constant rhs;
		if (!evaluate_constant_expression(expr->args[0], &rhs))
			return 0;
		*constant = operators_constant_unary(expr->unary_op, rhs);
	} break;

	case E_ARRAY_PTR_DECAY: {
		if (expr->args[0]->type != E_CONSTANT)
			return 0;

		struct constant c = expr->args[0]->constant;

		if (c.type == CONSTANT_LABEL) {
			*constant = c;
			constant->data_type = type_pointer(expr->args[0]->data_type->children[0]);
		} else {
			*constant = c;
			constant->data_type = type_pointer(expr->args[0]->data_type->children[0]);
			constant->type = CONSTANT_LABEL;
			constant->label = rodata_register(c.str_d);
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
