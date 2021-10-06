#ifndef PARSER_H
#define PARSER_H

#include <types.h>

#include "variables.h"
#include "operators.h"

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

struct instruction;
void push_ir(struct instruction instruction);
#define IR_PUSH(...) do { push_ir((struct instruction) { __VA_ARGS__ }); } while(0)

struct instruction {
	enum {
		IR_FUNCTION,
		IR_BINARY_OPERATOR,
		IR_UNARY_OPERATOR,
		IR_POINTER_INCREMENT,
		IR_POINTER_DIFF,
		IR_LOAD,
		IR_STORE,
		IR_ADDRESS_OF,
		IR_CONSTANT,
		IR_GOTO,
		IR_RETURN,
		IR_START_BLOCK,
		IR_ALLOCA,
		IR_SWITCH_SELECTION,
		IR_IF_SELECTION,
		IR_CALL_LABEL,
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
		IR_STATIC_VAR,

		IR_TYPE_COUNT
	} type;

	union {
		struct {
			int global;
			struct type *signature;
			var_id *named_arguments;
			const char *name;
		} function;
#define IR_PUSH_FUNCTION(SIGNATURE, ARGS, NAME, GLOBAL) IR_PUSH(.type = IR_FUNCTION, .function.global = (GLOBAL), .function.signature = (SIGNATURE), .function.named_arguments = (ARGS), .function.name = (NAME))
		struct {
			enum operator_type type;
			var_id lhs, rhs, result;
		} binary_operator;
#define IR_PUSH_BINARY_OPERATOR(TYPE, LHS, RHS, RESULT) IR_PUSH(.type = IR_BINARY_OPERATOR, .binary_operator = { .type = (TYPE), .lhs = (LHS), .rhs = (RHS), .result = (RESULT)})
		struct {
			enum unary_operator_type type;
			var_id operand, result;
		} unary_operator;
#define IR_PUSH_UNARY_OPERATOR(TYPE, OPERAND, RESULT) IR_PUSH(.type = IR_UNARY_OPERATOR, .unary_operator = { (TYPE), (OPERAND), (RESULT)})
		struct {
			var_id result, pointer, index;
		} pointer_increment;
#define IR_PUSH_POINTER_INCREMENT(RESULT, POINTER, INDEX) IR_PUSH(.type = IR_POINTER_INCREMENT, .pointer_increment = {(RESULT), (POINTER), (INDEX)})
		struct {
			var_id result, lhs, rhs;
		} pointer_diff;
#define IR_PUSH_POINTER_DIFF(RESULT, LHS, RHS) IR_PUSH(.type = IR_POINTER_DIFF, .pointer_diff = {(RESULT), (LHS), (RHS)})
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
			int index;
		} get_member;
#define IR_PUSH_GET_MEMBER(RESULT, POINTER, INDEX) IR_PUSH(.type = IR_GET_MEMBER, .get_member = {(RESULT), (POINTER), (INDEX)})

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
			block_id block;
		} goto_;
#define IR_PUSH_GOTO(BLOCK) IR_PUSH(.type = IR_GOTO, .goto_.block = (BLOCK))
		struct {
			block_id block;
		} start_block;
#define IR_PUSH_START_BLOCK(BLOCK) IR_PUSH(.type = IR_START_BLOCK, .start_block.block = (BLOCK))
		struct {
			int is_void;
			var_id value;
		} return_;
#define IR_PUSH_RETURN_VALUE(VALUE) IR_PUSH(.type = IR_RETURN, .return_ = {.value = (VALUE), .is_void = 0} )
#define IR_PUSH_RETURN_VOID() IR_PUSH(.type = IR_RETURN, .return_ = {.value = 0, .is_void = 1} )
		struct {
			var_id variable;
		} alloca;
		struct {
			var_id condition;
			int n;
			struct constant *values;
			block_id *blocks;
			block_id default_;
		} switch_selection;
		struct {
			var_id condition;
			block_id block_true, block_false;
		} if_selection;
		struct {
			const char *label;
			struct type *function_type;
			int n_args;
			var_id *args;
			var_id result;
		} call_label;
#define IR_PUSH_CALL_LABEL(LABEL, FUNCTION_TYPE, N_ARGS, ARGS, RESULT) IR_PUSH(.type = IR_CALL_LABEL, .call_label = {(LABEL), (FUNCTION_TYPE), (N_ARGS), (ARGS), (RESULT)})
		struct {
			var_id function;
			struct type *function_type;
			int n_args;
			var_id *args;
			var_id result;
		} call_variable;
#define IR_PUSH_CALL_VARIABLE(VARIABLE, FUNCTION_TYPE, N_ARGS, ARGS, RESULT) IR_PUSH(.type = IR_CALL_VARIABLE, .call_variable = {(VARIABLE), (FUNCTION_TYPE), (N_ARGS), (ARGS), (RESULT)})
		struct {
			var_id result;
			var_id rhs;
			struct type *type;
		} cast;
#define IR_PUSH_CAST(RESULT, RHS, TYPE) IR_PUSH(.type = IR_CAST, .cast = {(RESULT), (RHS), (TYPE)})

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

		struct {
			const char *label;
			struct type *type;
			struct initializer *init;
			int global;
		} static_var;
#define IR_PUSH_STATIC_VAR(LABEL, TYPE, INIT, GLOBAL) IR_PUSH(.type = IR_STATIC_VAR, .static_var = {(LABEL), (TYPE), (INIT), (GLOBAL)})
	};
};

const char *instruction_to_str(struct instruction ins);

void parse_into_ir();
void print_parser_ir();

struct program {
	int instructions_n;
	struct instruction *instructions;
};

struct program *get_program(void);

#endif
