#ifndef PARSER_EXPRESSION_H
#define PARSER_EXPRESSION_H

#include "preprocessor/preprocessor.h"
#include <types.h>
#include "parser.h"
#include "declaration.h"
#include "operators.h"

struct expr *expr_new(struct expr expr);

#define EXPR_ARGS(TYPE, ...) expr_new((struct expr) {	\
			.type = (TYPE),								\
			.args = {__VA_ARGS__}						\
		})

#define EXPR_BINARY(TYPE, LHS, RHS) expr_new((struct expr) {	\
			.type = (TYPE),										\
			.args = {(LHS), (RHS)}								\
		})

#define EXPR_BINARY_OP(TYPE, LHS, RHS) expr_new((struct expr) {	\
			.type = E_BINARY_OP,								\
			.binary_op.op = (TYPE),								\
			.binary_op.type = E_BINARY_OP_UNASIGNED,			\
			.args = {(LHS), (RHS)}								\
		})

#define EXPR_CAST(TYPE, EXPR) expr_new((struct expr) {	\
			.type = (E_CAST),							\
			.cast = {(EXPR), (TYPE)}					\
		})

#define EXPR_INT(I) expr_new((struct expr) {							\
			.type = E_CONSTANT,											\
			.constant = {.type = CONSTANT_TYPE, .data_type = type_simple(ST_INT), .i = (I)} \
		})
#define EXPR_VAR(V) expr_new((struct expr) {	\
			.type = E_VARIABLE,								\
			.variable = {(V)}			\
		})

struct expr {
	enum {
		E_INVALID,
		E_IDENTIFIER,
		E_VARIABLE,
		E_SYMBOL,
		E_CALL,
		E_CONSTANT,
		E_STRING_LITERAL,
		E_GENERIC_SELECTION,
		E_FUNCTION_CALL,
		E_DOT_OPERATOR,
		E_POSTFIX_INC,
		E_POSTFIX_DEC,
		E_COMPOUND_LITERAL,
		E_PREFIX_INC,
		E_PREFIX_DEC,
		E_ADDRESS_OF,
		E_INDIRECTION,
		E_UNARY_PLUS,
		E_UNARY_MINUS,
		E_BITWISE_NOT,
		E_ALIGNOF,
		E_CAST,
		E_POINTER_ADD,
		E_POINTER_DIFF,
		E_ASSIGNMENT,
		E_ASSIGNMENT_ADD,
		E_ASSIGNMENT_POINTER_ADD,
		E_ASSIGNMENT_SUBTRACT,
		E_ASSIGNMENT_MULTIPLY,
		E_ASSIGNMENT_DIVIDE,
		E_ASSIGNMENT_MODULUS,
		E_ASSIGNMENT_LEFT_SHIFT,
		E_ASSIGNMENT_RIGHT_SHIFT,
		E_ASSIGNMENT_XOR,
		E_ASSIGNMENT_BINARY_OR,
		E_ASSIGNMENT_BINARY_AND,
		E_CONDITIONAL,
		E_COMMA,
		E_ARRAY_PTR_DECAY,
		E_ENUM_TO_INT,
		E_BUILTIN_VA_START,
		E_BUILTIN_VA_END,
		E_BUILTIN_VA_ARG,
		E_BUILTIN_VA_COPY,
		E_BINARY_OP,

		E_NUM_TYPES
	} type;

	union {
		struct {
			const char *name;
		} identifier;

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
		const char *string_literal;

		struct {
			enum operator_type op;
			enum {
				E_BINARY_OP_UNASIGNED,
				E_BINARY_OP_POINTER,
				E_BINARY_OP_ARITHMETIC,
			} type;
		} binary_op;
	};
	
	struct expr *args[3];

	struct position pos;
	struct type *data_type;
};

struct expr *parse_assignment_expression();
struct expr *parse_expression();
struct expr *expression_cast(struct expr *expr, struct type *type);

int evaluate_constant_expression(struct expr *expr,
								 struct constant *constant);
var_id expression_to_ir(struct expr *expr);

#endif
