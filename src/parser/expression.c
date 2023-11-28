#include "expression.h"
#include "declaration.h"
#include "function_parser.h"
#include "symbols.h"
#include "expression_to_ir.h"

#include <common.h>
#include <codegen/rodata.h>
#include <precedence.h>
#include <abi/abi.h>

#include <assert.h>
#include <stddef.h>

// Type conversions
static enum simple_type get_arithmetic_type(enum simple_type a, int bitfield_a,
											enum simple_type b, int bitfield_b) {
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
			   is_contained_in(a, bitfield_a, b, bitfield_b)) {
		return a;
	} else if (is_signed(b) &&
			   is_contained_in(b, bitfield_b, a, bitfield_a)) {
		return b;

	} else if (is_signed(b)) {
		return to_unsigned(b);

	} else if (is_signed(a)) {
		return to_unsigned(a);
	} else {
		ICE("Error in arithmetic conversion.");
	}
}

static int get_expression_bitfield(struct expr *expr) {
	if (expr->type == E_DOT_OPERATOR) {
		assert(expr->member.lhs->data_type->type == TY_STRUCT);
		struct struct_data *data = expr->member.lhs->data_type->struct_data;

		return data->fields[expr->member.member_idx].bitfield;
	}
	return -1;
}

static void convert_arithmetic(struct expr **a,
						struct expr **b) {
	struct type *a_type = (*a)->data_type,
		*b_type = (*b)->data_type;

	if (a_type->type != TY_SIMPLE ||
		b_type->type != TY_SIMPLE)
		return;

	int bitfield_a = get_expression_bitfield(*a),
		bitfield_b = get_expression_bitfield(*b);

	enum simple_type target_type =
		get_arithmetic_type(a_type->simple, bitfield_a, b_type->simple, bitfield_b);

	*a = expression_cast(*a, type_simple(target_type));
	*b = expression_cast(*b, type_simple(target_type));
}

static void decay_array(struct expr **expr) {
	struct type *type = (*expr)->data_type;
	if (type->type == TY_ARRAY ||
		type->type == TY_INCOMPLETE_ARRAY ||
		type->type == TY_VARIABLE_LENGTH_ARRAY) {
		*expr = EXPR_ARGS(E_ARRAY_PTR_DECAY, *expr);
	} else if (type->type == TY_FUNCTION) {
		*expr = EXPR_ARGS(E_ADDRESS_OF, *expr);
	}

	if ((*expr)->data_type->is_const)
		*expr = EXPR_ARGS(E_CONST_REMOVE, *expr);
}

static void do_integer_promotion(struct expr **expr) {
	struct type *current_type = (*expr)->data_type;
	int bitfield = get_expression_bitfield(*expr);

	if (type_is_integer(current_type) &&
		is_contained_in(ST_INT, -1, current_type->simple, bitfield)) {
		*expr = expression_cast(*expr, type_simple(ST_INT));
		return;
	}
}

static void do_default_argument_promotion(struct expr **expr) {
	do_integer_promotion(expr);

	if (type_is_simple((*expr)->data_type, ST_FLOAT))
		*expr = expression_cast(*expr, type_simple(ST_DOUBLE));
}

static struct type *calculate_type(struct expr *expr) {
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
		if (callee_type->type == TY_POINTER &&
			callee_type->children[0]->type == TY_FUNCTION)
			return type_deref(callee_type)->children[0];
		else
			ICE("Can't call type %s\n", dbg_type(callee_type));
	}

	case E_VARIABLE:
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

	case E_DOT_OPERATOR:
		return type_select(expr->member.lhs->data_type, expr->member.member_idx);

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
		return type_simple(abi_info.ptrdiff_type);

	case E_COMMA:
		return expr->args[1]->data_type;

	case E_CONST_REMOVE:
		return type_make_const(expr->args[0]->data_type, 0);

	case E_SYMBOL:
		switch (expr->symbol->type) {
		case IDENT_PARAMETER:
			return expr->symbol->parameter.type;
		case IDENT_VARIABLE:
			return expr->symbol->variable.type;
		default: NOTIMP();
		}

	default:
		printf("%d\n", expr->type);
		NOTIMP();
	}
}

static const int dont_decay_ptr[E_NUM_TYPES] = {
	[E_ADDRESS_OF] = 1,
	[E_ARRAY_PTR_DECAY] = 1,
	[E_ASSIGNMENT_OP] = 1,
	[E_ASSIGNMENT] = 1,
	[E_ASSIGNMENT_POINTER] = 1,
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

static int is_null_pointer_constant(struct expr *expr) {
	struct constant *c = expression_to_constant(expr);

	return c && c->type == CONSTANT_TYPE && constant_is_zero(c);
}

static void cast_conditional(struct expr *expr) {
	if (expr->type != E_CONDITIONAL)
		return;

	struct type *a = expr->args[1]->data_type,
		*b = expr->args[2]->data_type;

	if (a == b)
		return;

	if (type_is_pointer(a) && type_is_pointer(b)) {
		struct type *composite;
		if (type_remove_qualifications(type_deref(a)) == type_simple(ST_VOID))
			expr->args[1] = expression_cast(expr->args[1], b);
		else if (type_remove_qualifications(type_deref(b)) == type_simple(ST_VOID))
			expr->args[2] = expression_cast(expr->args[2], a);
		else if ((composite = type_make_composite(type_remove_qualifications(type_deref(a)),
												  type_remove_qualifications(type_deref(b))))) {
			composite = type_pointer(composite);
			expr->args[2] = expression_cast(expr->args[2], composite);
			expr->args[1] = expression_cast(expr->args[1], composite);
		} else {
			ERROR(T0->pos, "Invalid combination of data types:\n%s and %s\n",
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
		ERROR(T0->pos, "Invalid combination of data types:\n%s and %s\n",
			  strdup(dbg_type(a)),
			  strdup(dbg_type(b)));
	}
}

// This applies all the necessary transformations to binary operators
static void fix_binary_operator(struct expr *expr) {
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
			ICE("Can't subtract with pointer as rhs.");
		}
		break;

	case OP_ADD:
		if (!(lhs_ptr || rhs_ptr))
			return;

		if (lhs_ptr && rhs_ptr) {
			ICE("Invalid addition of pointers.");
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
				ICE("can only compare pointers with pointers or null pointer constant");
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

static void fix_assignment_operators(struct expr *expr) {
	if (expr->type != E_ASSIGNMENT_OP)
		return;

	convert_arithmetic(&expr->args[0], &expr->args[1]);
	// Reduce double cast into single cast. Always correct in this case.
	if (expr->args[0]->type == E_CAST) {
		if (expr->args[0]->cast.arg->type == E_CAST)
			expr->args[0]->cast.arg = expr->args[0]->cast.arg->cast.arg;

		//expr->assignment_op.cast = expr->args[0]->cast.target;
		expr->args[0] = expr->args[0]->cast.arg;
	}
	int lhs_ptr = type_is_pointer(expr->args[0]->data_type);

	if (lhs_ptr && (expr->assignment_op.op == OP_ADD || expr->assignment_op.op == OP_SUB)) {
		int is_postfix = expr->assignment_op.postfix;
		int is_sub = expr->assignment_op.op == OP_SUB;

		expr->type = E_ASSIGNMENT_POINTER;

		expr->assignment_pointer.postfix = is_postfix;
		expr->assignment_pointer.sub = is_sub;
	}
}

int evaluate_constant_expression(struct expr *expr,
 								 struct constant *constant);

static void check_const_correctness(struct expr *expr) {
	switch (expr->type) {
	case E_ASSIGNMENT:
	case E_ASSIGNMENT_OP:
	case E_ASSIGNMENT_POINTER:
		if (expr->args[0]->data_type->is_const) {
			ICE("Can't modify constant variable");
		}
		break;
	default:
		break;
	}
}

static struct expr *expr_dot_operator(struct expr *lhs, struct token *name) {
	int n = 0, *indices;
	if (!type_search_member(lhs->data_type, name->str, &n, &indices))
		return NULL;

	for (int i = n - 1; i >= 0; i--) {
		lhs = expr_new((struct expr) {
				.type = E_DOT_OPERATOR,
				.member = { lhs, indices[i] }
			});
	}

	return lhs;
}

struct expr *expr_new(struct expr expr) {
	for (int i = 0; i < num_args[expr.type]; i++) {
		if (!expr.args[i])
			ICE("Wrongly constructed expression of type %d", expr.type);
	}

	for (int i = dont_decay_ptr[expr.type];
		 i < num_args[expr.type]; i++) {
		decay_array(&expr.args[i]);
	}

	if (expr.type == E_BUILTIN_VA_COPY) {
		decay_array(&expr.va_copy_.d);
		decay_array(&expr.va_copy_.s);
	} else if (expr.type == E_BUILTIN_VA_ARG) {
		decay_array(&expr.va_arg_.v);
	} else if (expr.type == E_CALL) {
		for (int i = 0; i < expr.call.n_args; i++)
			decay_array(&expr.call.args[i]);
		decay_array(&expr.call.callee);
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

		int named_arguments_count = signature->n - 1;
		if (!(signature->function.is_variadic ?
			  named_arguments_count <= expr.call.n_args :
			  named_arguments_count == expr.call.n_args)) {
			ERROR(T0->pos, "Wrong number of arguments.");
		}

		for (int i = named_arguments_count; i < expr.call.n_args; i++) {
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

	if (!expr.pos.path)
		expr.pos = T0->pos; // If no position is supplied, at least take something close to it.

	return ALLOC(expr);
}

// Parsing.

static void parse_call_parameters(struct expr ***args, int *n_args) {
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
			ERROR(T0->pos, "Expected expression, got %s", dbg_token(T0));

		buffer[pos] = expr;
	}

	if (pos == MAX_ARGUMENTS)
		ERROR(T0->pos, "Too many arguments passed to function (maximum: %d)", MAX_ARGUMENTS);

	*args = NULL;
	if (pos) {
		*args = cc_malloc(sizeof **args * pos);
		memcpy(*args, buffer, sizeof **args * pos);
	}

	*n_args = pos;
}

struct expr *parse_pratt(int precedence);
struct expr *parse_pratt_with_lhs(int precedence, struct expr *lhs);

static struct expr *parse_prefix(void) {
	if (TACCEPT(T_LPAR)) {
		struct type *cast_type = parse_type_name();

		if (cast_type) {
			TEXPECT(T_RPAR);
			if (T0->type == T_LBRACE) {
				struct initializer init = parse_initializer(&cast_type);
				return expr_new((struct expr) {
						.type = E_COMPOUND_LITERAL,
						.compound_literal = { cast_type, init }
					});
			} else {
				struct expr *rhs = parse_pratt(PREFIX_PREC);
				if (!rhs)
					ERROR(T0->pos, "Expected expression");
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
		struct position pos = T0->pos;
		struct expr *rhs = parse_pratt(PREFIX_PREC);
		if ((rhs->type == E_VARIABLE && rhs->variable.is_register) ||
			(rhs->type == E_SYMBOL && rhs->symbol->is_register))
			ERROR(pos, "Taking address of register variable is not allowed.");
		return EXPR_ARGS(E_ADDRESS_OF, rhs);
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

		return type_alignof(type);
	} else if (T0->type == T_IDENT) {
		struct symbol_identifier *sym = symbols_get_identifier(T0->str);

		if (!sym)
			ERROR(T0->pos, "Could not find identifier %.*s", T0->str.len, T0->str.str);

		TNEXT();
		switch (sym->type) {
		case IDENT_LABEL:
			return expr_new((struct expr) {
					.type = E_CONSTANT,
					.constant = {
						.type = CONSTANT_LABEL,
						.data_type = sym->label.type,
						.label.label = register_label_name(sym->label.name),
						.label.offset = 0
					}
				});

		case IDENT_PARAMETER:
		case IDENT_VARIABLE:
			return expr_new((struct expr) {
					.type = E_SYMBOL,
					.symbol = sym
				});

		case IDENT_CONSTANT:
			return expr_new((struct expr) {
					.type = E_CONSTANT,
					.constant = sym->constant
				});
		}
	} else if (T0->type == T_STRING) {
		struct string_view str = T0->str;
		TNEXT();
		return EXPR_STR(str, ST_CHAR);
	} else if (T0->type == T_STRING_CHAR16) {
		struct string_view str = T0->str;
		TNEXT();
		return EXPR_STR(str, CHAR16_TYPE);
	} else if (T0->type == T_STRING_CHAR32) {
		struct string_view str = T0->str;
		TNEXT();
		return EXPR_STR(str, CHAR32_TYPE);
	} else if (T0->type == T_STRING_WCHAR) {
		struct string_view str = T0->str;
		TNEXT();
		return EXPR_STR(str, abi_info.wchar_type);
	} else if (T0->type == T_NUM) {
		struct constant c = constant_from_string(T0->str);
		TNEXT();
		return expr_new((struct expr) {
				.type = E_CONSTANT,
				.constant = c
			});
	} else if (T0->type == T_CHARACTER_CONSTANT) {
		struct string_view str = T0->str;
		TNEXT();
		return EXPR_INT(character_constant_to_int(str));
	} else if (T0->type == T_CHARACTER_CONSTANT_WCHAR) {
		struct string_view str = T0->str;
		TNEXT();
		return EXPR_INTEGER(abi_info.wchar_type, character_constant_wchar_to_int(str));
	} else if (T0->type == T_CHARACTER_CONSTANT_CHAR16) {
		struct string_view str = T0->str;
		TNEXT();
		return EXPR_INTEGER(CHAR16_TYPE, character_constant_char16_to_int(str));
	} else if (T0->type == T_CHARACTER_CONSTANT_CHAR32) {
		struct string_view str = T0->str;
		TNEXT();
		return EXPR_INTEGER(CHAR32_TYPE, character_constant_char32_to_int(str));
	} else if (TACCEPT(T_KGENERIC)) {
		TEXPECT(T_LPAR);
		struct expr *expr = parse_assignment_expression();
		if (!expr)
			ERROR(T0->pos, "Expected expression.");

		decay_array(&expr);
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
						ERROR(T0->pos, "More than one compatible type in _Generic association list, %s",
							  strdup(dbg_type(type)));
					res = rhs;
				}
			}
		}

		if (!res)
			ERROR(T0->pos, "No type matched the expresison in _Generic");

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
			ERROR(T0->pos, "Expected typename in var_arg");
		TEXPECT(T_RPAR);
		return expr_new((struct expr) {
				.type = E_BUILTIN_VA_ARG,
				.va_arg_ = {v, t}
			});
	} else if (TACCEPT(T_KOFFSETOF)) {
		TEXPECT(T_LPAR);
		
		struct type *type = parse_type_name();
		if (!type)
			ERROR(T0->pos, "Expectected type-name.");
		TEXPECT(T_COMMA);

		struct expr *expr = expr_new((struct expr) {
				.type = E_CONSTANT,
				.constant = {
					.type = CONSTANT_LABEL,
					.data_type = type,
					.label.label = -1, // null label?
					.label.offset = 0
				}
			});
		expr = expr_dot_operator(expr, T0);
		TNEXT();

		expr = parse_pratt_with_lhs(0, expr);
		expr = EXPR_ARGS(E_ADDRESS_OF, expr);

		TEXPECT(T_RPAR);

		return expression_cast(expr, type_simple(ST_ULONG));
	} else if (TACCEPT(T_KFUNC)) {
 		return EXPR_STR(get_current_function_name(), ST_CHAR);
	}
	return NULL;
}

struct expr *parse_pratt_with_lhs(int precedence, struct expr *lhs) {
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
			lhs = expr_dot_operator(lhs, T0);
			if (!lhs)
				ERROR(T0->pos, "Could not find member of name %s", dbg_token(T0));
			TNEXT();
		} else if (TACCEPT(T_ARROW)) {
			lhs = expr_dot_operator(EXPR_ARGS(E_INDIRECTION, lhs), T0);
			if (!lhs)
				ERROR(T0->pos, "Could not find member of name %s", dbg_token(T0));
			TNEXT();
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

struct expr *parse_pratt(int precedence) {
	struct expr *lhs = parse_prefix();

	if (!lhs)
		return NULL;

	return parse_pratt_with_lhs(precedence, lhs);
}

struct expr *parse_assignment_expression(void) {
	return parse_pratt(ASSIGNMENT_PREC);
}

struct expr *parse_expression(void) {
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
			!type_is_pointer(expr->cast.target)) {
			if (rhs.type == CONSTANT_LABEL_POINTER &&
				rhs.label.label == -1) {
				constant->type = CONSTANT_TYPE;
				constant->data_type = expr->cast.target;
				constant->uint_d = rhs.label.offset;
			} else {
				return 0;
			}
		} else {
			*constant = constant_cast(rhs, expr->cast.target);
		}
	} break;

	case E_POINTER_SUB:
	case E_POINTER_ADD: {
		struct constant lhs, rhs;
		if (!evaluate_constant_expression(expr->args[0], &lhs))
			return 0;
		if (!evaluate_constant_expression(expr->args[1], &rhs))
			return 0;

		if (lhs.type == CONSTANT_LABEL)
			return 0;

		if (lhs.type == CONSTANT_LABEL_POINTER &&
			type_is_simple(rhs.data_type, ST_INT) &&
			rhs.type == CONSTANT_TYPE) {
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

	case E_DOT_OPERATOR: {
		struct constant lhs;
		if (!evaluate_constant_expression(expr->member.lhs, &lhs))
			return 0;

		if (lhs.type == CONSTANT_LABEL) {
			int offset, bit_offset, bit_size;
			type_get_offsets(lhs.data_type, expr->member.member_idx,
							 &offset, &bit_offset, &bit_size);

			if (bit_size != -1)
				return 0;

			struct type *child_type = type_select(lhs.data_type, expr->member.member_idx);

			*constant = lhs;
			constant->label.offset += offset;
			constant->data_type = child_type;

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
		if (!operators_constant(expr->binary_op, lhs, rhs, constant))
			return 0;
	} break;

	case E_UNARY_OP: {
		struct constant rhs;
		if (!evaluate_constant_expression(expr->args[0], &rhs))
			return 0;
		if (rhs.type == CONSTANT_LABEL)
			return 0;
		if (!operators_constant_unary(expr->unary_op, rhs, constant))
			return 0;
	} break;

	case E_ARRAY_PTR_DECAY: {
		if (expr->args[0]->type != E_CONSTANT)
			return 0;

		struct constant c = expr->args[0]->constant;

		assert(c.type == CONSTANT_LABEL);

		*constant = c;
		constant->type = CONSTANT_LABEL_POINTER;
		constant->data_type = type_pointer(expr->args[0]->data_type->children[0]);
	} break;

	case E_CONDITIONAL: {
		struct constant lhs, mid, rhs;
		if (!evaluate_constant_expression(expr->args[0], &lhs))
			return 0;
		if (!evaluate_constant_expression(expr->args[1], &mid))
			return 0;
		if (!evaluate_constant_expression(expr->args[2], &rhs))
			return 0;

		if (lhs.type != CONSTANT_TYPE ||
			mid.type != CONSTANT_TYPE ||
			rhs.type != CONSTANT_TYPE)
			return 0;

		if (constant_is_zero(&lhs)) {
			*constant = rhs;
		} else {
			*constant = mid;
		}
	} break;

	case E_ADDRESS_OF: {
		struct constant operand;
		if (!evaluate_constant_expression(expr->args[0], &operand))
			return 0;

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
		if (!evaluate_constant_expression(expr->args[0], &operand))
			return 0;

		if (operand.type == CONSTANT_LABEL_POINTER) {
			*constant = operand;
			constant->type = CONSTANT_LABEL;
			constant->data_type = expr->data_type;
		} else {
			return 0;
		}
	} break;

	case E_COMMA: {
		struct constant lhs, rhs;
		if (!evaluate_constant_expression(expr->args[0], &lhs))
			return 0;
		if (!evaluate_constant_expression(expr->args[1], &rhs))
			return 0;

		*constant = rhs;
	} break;

	case E_CONST_REMOVE: {
		struct constant c;
		if (!evaluate_constant_expression(expr->args[0], &c))
			return 0;
		*constant = c;
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

	if (type_is_simple(c->data_type, ST_FLOAT)) {
		return c->double_d == 0;
	} else if (type_is_simple(c->data_type, ST_DOUBLE)) {
		return c->float_d == 0;
	} else if (type_is_integer(c->data_type) && is_signed(c->data_type->simple)) {
		return c->int_d == 0;
	} else if (type_is_integer(c->data_type) && !is_signed(c->data_type->simple)) {
		return c->uint_d == 0;
	} else if (c->data_type->type == TY_POINTER) {
		return c->uint_d == 0;
	}

	return 0;
}
