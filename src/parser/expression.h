#ifndef PARSER_EXPRESSION_H
#define PARSER_EXPRESSION_H

#include "preprocessor/preprocessor.h"
#include "parser.h"
#include "declaration.h"
#include "operators.h"

#include <types.h>

struct expr *expr_new(struct expr expr);

#define EXPR_ARGS(TYPE, ...) expr_new((struct expr) {	\
			.type = (TYPE),								\
			.args = {__VA_ARGS__}						\
		})

#define EXPR_ASSIGNMENT_OP(TYPE, LHS, RHS) expr_new((struct expr) {	\
			.type = E_ASSIGNMENT_OP,								\
			.binary_op = (TYPE),								\
			.args = {(LHS), (RHS)}								\
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

#define EXPR_STR(STR) expr_new((struct expr) {							\
			.type = E_CONSTANT,											\
			.constant = {												\
				.type = CONSTANT_TYPE,									\
				.data_type = type_array(type_simple(ST_CHAR), strlen(STR) + 1),	\
				.str_d = (strdup(STR))									\
			}});

#define EXPR_INT(I) expr_new((struct expr) {							\
			.type = E_CONSTANT,											\
			.constant = {.type = CONSTANT_TYPE, .data_type = type_simple(ST_INT), .int_d = (I)} \
		})
#define EXPR_VAR(V, TYPE) expr_new((struct expr) {				\
			.type = E_VARIABLE,								\
			.variable = {(V), (TYPE)}						\
		})

struct expr {
	enum {
		E_INVALID,
		E_VARIABLE,
		E_SYMBOL,
		E_CALL,
		E_CONSTANT,
		E_GENERIC_SELECTION,
		E_FUNCTION_CALL,
		E_DOT_OPERATOR,
		E_POSTFIX_INC,
		E_POSTFIX_DEC,
		E_COMPOUND_LITERAL,
		E_ADDRESS_OF,
		E_INDIRECTION,
		E_UNARY_OP,
		E_ALIGNOF,
		E_CAST,
		E_POINTER_ADD,
		E_POINTER_DIFF,
		E_ASSIGNMENT,
		E_ASSIGNMENT_POINTER_ADD,
		E_ASSIGNMENT_POINTER_SUB,
		E_ASSIGNMENT_OP,
		E_CONDITIONAL,
		E_COMMA,
		E_ARRAY_PTR_DECAY,
		E_BUILTIN_VA_START,
		E_BUILTIN_VA_END,
		E_BUILTIN_VA_ARG,
		E_BUILTIN_VA_COPY,

		E_BINARY_OP,

		E_NUM_TYPES
	} type;

	union {
		struct {
			struct type *type;
			struct initializer *init;
		} compound_literal;

		struct {
			struct expr *callee;
			int n_args;
			struct expr **args;
		} call;

		struct {
			const char *name;
			struct type *type;
		} symbol;

		struct {
			struct expr *arg;
			struct type *target;
		} cast;

		struct {
			var_id id;
			struct type *type;
		} variable;

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
		//const char *string_literal;

		enum operator_type binary_op;
		enum unary_operator_type unary_op;
	};
	
	struct expr *args[3];

	struct position pos;
	struct type *data_type;
};

struct expr *parse_assignment_expression();
struct expr *parse_expression();
struct expr *expression_cast(struct expr *expr, struct type *type);

struct constant *expression_to_constant(struct expr *expr);
var_id expression_to_ir(struct expr *expr);

#endif
