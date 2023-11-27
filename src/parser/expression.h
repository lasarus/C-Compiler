#ifndef PARSER_EXPRESSION_H
#define PARSER_EXPRESSION_H

#include "parser/symbols.h"
#include "preprocessor/preprocessor.h"
#include "parser.h"
#include "declaration.h"

#include <ir/operators.h>

#include <types.h>

struct expr *expr_new(struct expr expr);

#define EXPR_ARGS(TYPE, ...) expr_new((struct expr) {	\
			.type = (TYPE),								\
			.args = {__VA_ARGS__}						\
		})

#define EXPR_ASSIGNMENT_OP(TYPE, LHS, RHS, POSTFIX) expr_new((struct expr) {	\
			.type = E_ASSIGNMENT_OP,									\
			.assignment_op = {.op = (TYPE), .postfix = (POSTFIX)},			\
			.args = {(LHS), (RHS)}										\
		})

#define EXPR_BINARY_OP(TYPE, LHS, RHS) expr_new((struct expr) {	\
			.type = E_BINARY_OP,								\
			.binary_op = (TYPE),								\
			.args = {(LHS), (RHS)}								\
		})

#define EXPR_UNARY_OP(TYPE, RHS) expr_new((struct expr) {	\
			.type = E_UNARY_OP,								\
			.unary_op = (TYPE),								\
			.args = {(RHS)}									\
		})

#define EXPR_STR(STR, CHAR_TYPE) expr_new((struct expr) {				\
			.type = E_CONSTANT,											\
			.constant = {												\
				.type = CONSTANT_LABEL,									\
				.data_type = type_array(type_simple(CHAR_TYPE), (STR).len / calculate_size(type_simple(CHAR_TYPE))), \
				.label = {rodata_register(STR), .offset = 0}			\
			}})

#define EXPR_INT(I) expr_new((struct expr) {							\
			.type = E_CONSTANT,											\
			.constant = constant_simple_signed(ST_INT, I)				\
		})

#define EXPR_INTEGER(TYPE, I) expr_new((struct expr) {					\
			.type = E_CONSTANT,											\
			.constant = constant_simple_signed((TYPE), I)				\
		})

#define EXPR_VAR(V, TYPE, IS_REG) expr_new((struct expr) {	\
			.type = E_VARIABLE,								\
			.variable = {(V), (TYPE), (IS_REG)}				\
		})

struct expr {
	enum {
		E_INVALID,
		E_SYMBOL,
		E_VARIABLE,
		E_CALL,
		E_CONSTANT,
		E_GENERIC_SELECTION,
		E_DOT_OPERATOR,
		E_COMPOUND_LITERAL,
		E_ADDRESS_OF,
		E_INDIRECTION,
		E_UNARY_OP,
		E_ALIGNOF,
		E_CAST,
		E_POINTER_ADD,
		E_POINTER_SUB,
		E_POINTER_DIFF,
		E_ASSIGNMENT,
		E_ASSIGNMENT_POINTER,
		E_ASSIGNMENT_OP,
		E_CONDITIONAL,
		E_COMMA,
		E_ARRAY_PTR_DECAY,
		E_BUILTIN_VA_START,
		E_BUILTIN_VA_END,
		E_BUILTIN_VA_ARG,
		E_BUILTIN_VA_COPY,
		E_CONST_REMOVE,

		E_BINARY_OP,

		E_NUM_TYPES
	} type;

	union {
		struct {
			struct type *type;
			struct initializer init;
		} compound_literal;

		struct {
			struct expr *callee;
			int n_args;
			struct expr **args;
		} call;

		struct {
			struct expr *arg;
			struct type *target;
		} cast;

		struct {
			var_id id;
			struct type *type;
			int is_register;
		} variable;

		struct {
			var_id ptr;
			struct type *type;
			int is_register;
		} variable_ptr;

		struct symbol_identifier *symbol;

		struct {
			struct expr *lhs;
			int member_idx;
		} member;

		struct {
			struct expr *array, *last_param; // TODO: last_param should not be an expression.
		} va_start_;
		struct {
			struct expr *v;
		} va_end_;
		struct {
			struct expr *v;
			struct type *t;
		} va_arg_;
		struct {
			struct expr *d, *s;
		} va_copy_;

		struct constant constant;

		struct {
			int postfix;
			enum operator_type op;
		} assignment_op;

		struct {
			int postfix, sub;
		} assignment_pointer;

		enum operator_type binary_op;
		enum unary_operator_type unary_op;
	};
	
	struct expr *args[3];

	struct type *data_type;

	struct position pos;
};

struct expr *parse_assignment_expression(void);
struct expr *parse_expression(void);
struct expr *expression_cast(struct expr *expr, struct type *type);

struct constant *expression_to_constant(struct expr *expr);
int evaluate_constant_expression(struct expr *expr, struct constant *constant);

int constant_is_zero(struct constant *c);

#endif
