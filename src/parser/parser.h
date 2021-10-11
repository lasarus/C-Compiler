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
		IR_ASSIGN_CONSTANT_OFFSET,
		IR_SET_ZERO,
		IR_VA_START,
		IR_VA_END,
		IR_VA_ARG, //IR_VA_COPY,
		IR_STACK_ALLOC,
		IR_POP_STACK_ALLOC,
		IR_GET_SYMBOL_PTR,

		IR_TYPE_COUNT
	} type;

	union {
		struct {
			enum operator_type type;
			enum operand_type operand_type;
			var_id lhs, rhs, result;
		} binary_operator;
#define IR_PUSH_BINARY_OPERATOR(TYPE, OPERAND_TYPE, LHS, RHS, RESULT) IR_PUSH(.type = IR_BINARY_OPERATOR, .binary_operator = {(TYPE), (OPERAND_TYPE), (LHS), (RHS), (RESULT)})
		struct {
			enum unary_operator_type type;
			enum operand_type operand_type;
			var_id operand, result;
		} unary_operator;
#define IR_PUSH_UNARY_OPERATOR(TYPE, OPERAND_TYPE, OPERAND, RESULT) IR_PUSH(.type = IR_UNARY_OPERATOR, .unary_operator = { (TYPE), (OPERAND_TYPE), (OPERAND), (RESULT)})
		struct {
			var_id result, pointer, index;
			int decrement;
			struct type *ptr_type;
			enum simple_type index_type;
		} pointer_increment;
#define IR_PUSH_POINTER_INCREMENT(RESULT, POINTER, INDEX, DECREMENT, PTR_TYPE, INDEX_TYPE) IR_PUSH(.type = IR_POINTER_INCREMENT, .pointer_increment = {(RESULT), (POINTER), (INDEX), (DECREMENT), (PTR_TYPE), (INDEX_TYPE)})
		struct {
			var_id result, lhs, rhs;
			struct type *ptr_type;
		} pointer_diff;
#define IR_PUSH_POINTER_DIFF(RESULT, LHS, RHS, PTR_TYPE) IR_PUSH(.type = IR_POINTER_DIFF, .pointer_diff = {(RESULT), (LHS), (RHS), (PTR_TYPE)})
		struct {
			var_id result, pointer;
		} load;
#define IR_PUSH_LOAD(RESULT, POINTER) IR_PUSH(.type = IR_LOAD, .load = {(RESULT), (POINTER)})
		struct {
			var_id result, source;
		} copy;
#define IR_PUSH_COPY(RESULT, SOURCE) IR_PUSH(.type = IR_COPY, .copy = {(RESULT), (SOURCE)})
		struct {
			var_id value, pointer;
		} store;
#define IR_PUSH_STORE(VALUE, POINTER) IR_PUSH(.type = IR_STORE, .store = {(VALUE), (POINTER)})
		struct {
			var_id result, variable;
		} address_of;
#define IR_PUSH_ADDRESS_OF(RESULT, VARIABLE) IR_PUSH(.type = IR_ADDRESS_OF, .address_of = {(RESULT), (VARIABLE)})
		struct {
			var_id result, array;
		} array_ptr_decay;
#define IR_PUSH_ARRAY_PTR_DECAY(RESULT, ARRAY) IR_PUSH(.type = IR_ARRAY_PTR_DECAY, .address_of = {(RESULT), (ARRAY)})
		struct {
			var_id result, pointer;
			int offset;
		} get_member;
#define IR_PUSH_GET_OFFSET(RESULT, POINTER, OFFSET) IR_PUSH(.type = IR_GET_MEMBER, .get_member = {(RESULT), (POINTER), (OFFSET)})

		struct {
			var_id variable;
			var_id value;
			int offset;
		} assign_constant_offset;
#define IR_PUSH_ASSIGN_CONSTANT_OFFSET(VARIABLE, VALUE, OFFSET) IR_PUSH(.type = IR_ASSIGN_CONSTANT_OFFSET, .assign_constant_offset = { (VARIABLE), (VALUE), (OFFSET) })

		struct {
			var_id variable;
		} set_zero;
#define IR_PUSH_SET_ZERO(VARIABLE) IR_PUSH(.type = IR_SET_ZERO, .set_zero = { (VARIABLE) })
		struct {
			struct constant constant;
			var_id result;
		} constant;
#define IR_PUSH_CONSTANT(CONSTANT, RESULT) IR_PUSH(.type = IR_CONSTANT, .constant = {.constant = (CONSTANT), .result = (RESULT)})
		struct {
			const char *label;
			var_id result;
		} get_symbol_ptr;
#define IR_PUSH_GET_SYMBOL_PTR(STR, RESULT) IR_PUSH(.type = IR_GET_SYMBOL_PTR, .get_symbol_ptr = {(STR), (RESULT)})
		struct {
			var_id function;
			struct type *function_type;
			struct type **argument_types;
			int n_args;
			var_id *args;
			var_id result;
		} call_variable;
#define IR_PUSH_CALL_VARIABLE(VARIABLE, FUNCTION_TYPE, ARGUMENT_TYPES, N_ARGS, ARGS, RESULT) IR_PUSH(.type = IR_CALL_VARIABLE, .call_variable = {(VARIABLE), (FUNCTION_TYPE), (ARGUMENT_TYPES), (N_ARGS), (ARGS), (RESULT)})
		struct {
			var_id result;
			var_id rhs;
			struct type *result_type, *rhs_type;
		} cast;
#define IR_PUSH_CAST(RESULT, RESULT_TYPE, RHS, RHS_TYPE) IR_PUSH(.type = IR_CAST, .cast = {(RESULT), (RHS), (RESULT_TYPE), (RHS_TYPE)})

		struct {
			var_id result;
		} va_start_;
#define IR_PUSH_VA_START(RESULT) IR_PUSH(.type = IR_VA_START, .va_start_ = {(RESULT)})

		struct {
			var_id array, result;
			struct type *type;
		} va_arg_;
#define IR_PUSH_VA_ARG(ARRAY, RESULT, TYPE) IR_PUSH(.type = IR_VA_ARG, .va_arg_ = {(ARRAY), (RESULT), (TYPE)})

		struct {
			var_id pointer, length;
		} stack_alloc;
#define IR_PUSH_STACK_ALLOC(POINTER, LENGTH) IR_PUSH(.type = IR_STACK_ALLOC, .stack_alloc = {(POINTER), (LENGTH)})
#define IR_PUSH_POP_STACK_ALLOC() IR_PUSH(.type = IR_POP_STACK_ALLOC)
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

struct function *get_current_function(void);
struct block *get_current_block(void);

#endif
