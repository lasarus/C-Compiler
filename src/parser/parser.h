#ifndef PARSER_H
#define PARSER_H

#include "variables.h"
#include "operators.h"

#include <types.h>

typedef int block_id;
block_id new_block();

enum operand_type {
	OT_INT,
	OT_UINT,
	OT_LONG,
	OT_ULONG,
	OT_LLONG,
	OT_ULLONG,
	OT_FLOAT,
	OT_DOUBLE,
	OT_PTR,

	OT_TYPE_COUNT
};

enum operand_type ot_from_st(enum simple_type st);
enum operand_type ot_from_type(struct type *type);

struct instruction;
void push_ir(struct instruction instruction);
#define IR_PUSH(...) do { push_ir((struct instruction) { __VA_ARGS__ }); } while(0)

struct instruction {
	enum {
		IR_BINARY_OPERATOR,
		IR_UNARY_OPERATOR,
		IR_POINTER_INCREMENT,
		IR_POINTER_DIFF,
		IR_LOAD,
		IR_STORE,
		IR_ADDRESS_OF,
		IR_CONSTANT,
		IR_SWITCH_SELECTION,
		IR_CALL_VARIABLE,
		IR_COPY,
		IR_CAST,
		IR_GET_MEMBER,
		IR_ARRAY_PTR_DECAY,
		IR_SET_ZERO,
		IR_VA_START,
		IR_VA_END,
		IR_VA_ARG, //IR_VA_COPY,
		IR_STACK_ALLOC,
		IR_POP_STACK_ALLOC,
		IR_GET_SYMBOL_PTR,
		IR_TRUNCATE,
		IR_SET_BITS,
		IR_GET_BITS,

		IR_TYPE_COUNT
	} type;

	var_id result;

	union {
		struct {
			enum operator_type type;
			enum operand_type operand_type;
			var_id lhs, rhs;
		} binary_operator;
#define IR_PUSH_BINARY_OPERATOR(TYPE, OPERAND_TYPE, LHS, RHS, RESULT) IR_PUSH(.type = IR_BINARY_OPERATOR, .result = (RESULT), .binary_operator = {(TYPE), (OPERAND_TYPE), (LHS), (RHS)})
		struct {
			enum unary_operator_type type;
			enum operand_type operand_type;
			var_id operand;
		} unary_operator;
#define IR_PUSH_UNARY_OPERATOR(TYPE, OPERAND_TYPE, OPERAND, RESULT) IR_PUSH(.type = IR_UNARY_OPERATOR, .result = (RESULT), .unary_operator = { (TYPE), (OPERAND_TYPE), (OPERAND)})
		struct {
			var_id pointer, index;
			int decrement;
			struct type *ptr_type;
			enum simple_type index_type;
		} pointer_increment;
#define IR_PUSH_POINTER_INCREMENT(RESULT, POINTER, INDEX, DECREMENT, PTR_TYPE, INDEX_TYPE) IR_PUSH(.type = IR_POINTER_INCREMENT, .result = (RESULT), .pointer_increment = {(POINTER), (INDEX), (DECREMENT), (PTR_TYPE), (INDEX_TYPE)})
		struct {
			var_id lhs, rhs;
			struct type *ptr_type;
		} pointer_diff;
#define IR_PUSH_POINTER_DIFF(RESULT, LHS, RHS, PTR_TYPE) IR_PUSH(.type = IR_POINTER_DIFF, .result=(RESULT), .pointer_diff = {(LHS), (RHS), (PTR_TYPE)})
		struct {
			var_id pointer;
		} load;
#define IR_PUSH_LOAD(RESULT, POINTER) IR_PUSH(.type = IR_LOAD, .result = (RESULT), .load = {(POINTER)})
		struct {
			var_id source;
		} copy;
#define IR_PUSH_COPY(RESULT, SOURCE) IR_PUSH(.type = IR_COPY, .result=(RESULT), .copy = {(SOURCE)})
		struct {
			var_id value, pointer;
		} store;
#define IR_PUSH_STORE(VALUE, POINTER) IR_PUSH(.type = IR_STORE, .store = {(VALUE), (POINTER)})
		struct {
			var_id variable;
		} address_of;
#define IR_PUSH_ADDRESS_OF(RESULT, VARIABLE) IR_PUSH(.type = IR_ADDRESS_OF, .result=(RESULT), .address_of = {(VARIABLE)})
		struct {
			var_id array;
		} array_ptr_decay;
#define IR_PUSH_ARRAY_PTR_DECAY(RESULT, ARRAY) IR_PUSH(.type = IR_ARRAY_PTR_DECAY, .result=(RESULT), .address_of = {(ARRAY)})
		struct {
			var_id pointer;
			int offset;
		} get_member;
#define IR_PUSH_GET_OFFSET(RESULT, POINTER, OFFSET) IR_PUSH(.type = IR_GET_MEMBER, .result = (RESULT), .get_member = {(POINTER), (OFFSET)})

#define IR_PUSH_SET_ZERO(RESULT) IR_PUSH(.type = IR_SET_ZERO, .result = (RESULT))
		struct {
			struct constant constant;
		} constant;
#define IR_PUSH_CONSTANT(CONSTANT, RESULT) IR_PUSH(.type = IR_CONSTANT, .result=(RESULT), .constant = {(CONSTANT)})
		struct {
			const char *label;
			int64_t offset;
		} get_symbol_ptr;
#define IR_PUSH_GET_SYMBOL_PTR(STR, OFFSET, RESULT) IR_PUSH(.type = IR_GET_SYMBOL_PTR, .result=(RESULT), .get_symbol_ptr = {(STR), (OFFSET)})
		struct {
			var_id function;
			struct type *function_type;
			struct type **argument_types;
			int n_args;
			var_id *args;
		} call_variable;
#define IR_PUSH_CALL_VARIABLE(VARIABLE, FUNCTION_TYPE, ARGUMENT_TYPES, N_ARGS, ARGS, RESULT) IR_PUSH(.type = IR_CALL_VARIABLE, .result = (RESULT), .call_variable = {(VARIABLE), (FUNCTION_TYPE), (ARGUMENT_TYPES), (N_ARGS), (ARGS)})
		struct {
			var_id rhs;
			struct type *result_type, *rhs_type;
		} cast;
#define IR_PUSH_CAST(RESULT, RESULT_TYPE, RHS, RHS_TYPE) IR_PUSH(.type = IR_CAST, .result = (RESULT), .cast = {(RHS), (RESULT_TYPE), (RHS_TYPE)})
		struct {
			var_id rhs;
			int sign_extend;
		} truncate;
#define IR_PUSH_TRUNCATE(RESULT, RHS, SIGN_EXTEND) IR_PUSH(.type = IR_TRUNCATE, .result = (RESULT), .truncate = {(RHS), (SIGN_EXTEND)})

#define IR_PUSH_VA_START(RESULT) IR_PUSH(.type = IR_VA_START, .result = (RESULT))

		struct {
			var_id array;
			struct type *type;
		} va_arg_;
#define IR_PUSH_VA_ARG(ARRAY, RESULT, TYPE) IR_PUSH(.type = IR_VA_ARG, .result = (RESULT), .va_arg_ = {(ARRAY), (TYPE)})

		struct {
			var_id length;
		} stack_alloc;
#define IR_PUSH_STACK_ALLOC(RESULT, LENGTH) IR_PUSH(.type = IR_STACK_ALLOC, .result = (RESULT), .stack_alloc = {(LENGTH)})
#define IR_PUSH_POP_STACK_ALLOC() IR_PUSH(.type = IR_POP_STACK_ALLOC)

		struct {
			var_id field, value;
			int offset, length;
		} set_bits;
#define IR_PUSH_SET_BITS(RESULT, FIELD, VALUE, OFFSET, LENGTH) IR_PUSH(.type = IR_SET_BITS, .result = (RESULT), .set_bits = {(FIELD), (VALUE), (OFFSET), (LENGTH)})

		struct {
			var_id field;
			int offset, length, sign_extend;
		} get_bits;
#define IR_PUSH_GET_BITS(RESULT, FIELD, OFFSET, LENGTH, SIGN_EXTEND) IR_PUSH(.type = IR_GET_BITS, .result = (RESULT), .get_bits = {(FIELD), (OFFSET), (LENGTH), (SIGN_EXTEND)})
	};
};

const char *instruction_to_str(struct instruction ins);

void parse_into_ir();
void print_parser_ir();

struct program {
	int n, cap;
	struct function *functions;
};

struct function {
	int is_global;
	struct type *signature;
	var_id *named_arguments;
	const char *name;

	int var_n, var_cap;
	var_id *vars;

	int uses_va;

	int n, cap;
	struct block *blocks;
};

struct block_exit {
	enum {
		BLOCK_EXIT_NONE,
		BLOCK_EXIT_RETURN,
		BLOCK_EXIT_JUMP,
		BLOCK_EXIT_IF,
		BLOCK_EXIT_RETURN_ZERO,
		BLOCK_EXIT_SWITCH
	} type;

	union {
		struct {
			var_id condition;
			int n;
			struct constant *values;
			block_id *blocks;
			block_id default_;
		} switch_;

		struct {
			var_id condition;
			block_id block_true, block_false;
		} if_;

		block_id jump;

		struct {
			struct type *type;
			var_id value;
		} return_;
	};
};

struct block {
	block_id id;
	int n, cap;
	struct instruction *instructions;

	struct block_exit exit;
};

void ir_new_function(struct type *signature, var_id *arguments, const char *name, int is_global);

struct program *get_program(void);

void ir_block_start(block_id id);

void ir_if_selection(var_id condition, block_id block_true, block_id block_false);
void ir_switch_selection(var_id condition, int n, struct constant *values, block_id *blocks, block_id default_);
void ir_goto(block_id jump);
void ir_return(var_id value, struct type *type);
void ir_return_void(void);

void ir_init_var(struct initializer *init, var_id result);

struct function *get_current_function(void);
struct block *get_current_block(void);

#endif
