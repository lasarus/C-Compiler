#ifndef IR_H
#define IR_H

#include "variables.h"
#include "operators.h"
#include <codegen/rodata.h>

typedef int block_id;
block_id new_block(void);

enum ir_binary_operator {
	IBO_ADD, IBO_SUB,
	IBO_MUL, IBO_IMUL,
	IBO_DIV, IBO_IDIV,
	IBO_MOD, IBO_IMOD,
	IBO_LSHIFT, IBO_RSHIFT, IBO_IRSHIFT,
	IBO_BXOR, IBO_BOR, IBO_BAND,
	IBO_LESS, IBO_ILESS,
	IBO_GREATER, IBO_IGREATER,
	IBO_LESS_EQ, IBO_ILESS_EQ,
	IBO_GREATER_EQ, IBO_IGREATER_EQ,
	IBO_EQUAL, IBO_NOT_EQUAL,

	IBO_FLT_ADD, IBO_FLT_SUB,
	IBO_FLT_MUL, IBO_FLT_DIV,
	IBO_FLT_LESS, IBO_FLT_GREATER,
	IBO_FLT_LESS_EQ,
	IBO_FLT_GREATER_EQ,
	IBO_FLT_EQUAL,
	IBO_FLT_NOT_EQUAL,

	IBO_COUNT
};

struct instruction;
void push_ir(struct instruction instruction);
#define IR_PUSH(...) do { push_ir((struct instruction) { __VA_ARGS__ }); } while(0)

struct instruction {
	enum {
		IR_BINARY_OPERATOR,
		IR_NEGATE_INT,
		IR_NEGATE_FLOAT,
		IR_BINARY_NOT,
		IR_LOAD,
		IR_STORE,
		IR_ADDRESS_OF,
		IR_CONSTANT,
		IR_SWITCH_SELECTION,
		IR_CALL,
		IR_COPY,
		IR_BOOL_CAST,
		IR_INT_CAST,
		IR_FLOAT_CAST,
		IR_INT_FLOAT_CAST,
		IR_FROM_FLOAT,
		IR_SET_ZERO,
		IR_VA_START,
		IR_VA_ARG, //IR_VA_COPY,
		IR_STACK_ALLOC,
		IR_ADD_TEMPORARY,
		IR_CLEAR_STACK_BUCKET,
		IR_RESIZE,

		// You should be careful with these instructions.
		// They are here to allow for easier implementation
		// of different calling conventions.
		// Registers might be overwritten by other instructions.
		IR_SET_REG,
		IR_GET_REG,
		IR_MODIFY_STACK_POINTER,
		IR_STORE_STACK_RELATIVE,
		IR_LOAD_BASE_RELATIVE,

		IR_TYPE_COUNT
	} type;

	var_id operands[3];

	union {
		struct {
			enum ir_binary_operator type;
		} binary_operator;
#define IR_PUSH_BINARY_OPERATOR(TYPE, LHS, RHS, RESULT) IR_PUSH(.type = IR_BINARY_OPERATOR, .operands = {(RESULT), (LHS), (RHS)}, .binary_operator = {(TYPE)})
#define IR_PUSH_BINARY_NOT(RESULT, OPERAND) IR_PUSH(.type = IR_BINARY_NOT, .operands = {(RESULT), (OPERAND)})
#define IR_PUSH_NEGATE_INT(RESULT, OPERAND) IR_PUSH(.type = IR_NEGATE_INT, .operands = {(RESULT), (OPERAND)})
#define IR_PUSH_NEGATE_FLOAT(RESULT, OPERAND) IR_PUSH(.type = IR_NEGATE_FLOAT, .operands = {(RESULT), (OPERAND)})
#define IR_PUSH_LOAD(RESULT, POINTER) IR_PUSH(.type = IR_LOAD, .operands = {(RESULT), (POINTER)})
#define IR_PUSH_COPY(RESULT, SOURCE) IR_PUSH(.type = IR_COPY, .operands = {(RESULT), (SOURCE)})
#define IR_PUSH_STORE(VALUE, POINTER) IR_PUSH(.type = IR_STORE, .operands = {(VALUE), (POINTER)})
#define IR_PUSH_ADDRESS_OF(RESULT, VARIABLE) IR_PUSH(.type = IR_ADDRESS_OF, .operands = {(RESULT), (VARIABLE)})
#define IR_PUSH_SET_ZERO(RESULT) IR_PUSH(.type = IR_SET_ZERO, .operands = {(RESULT)})
		struct {
			struct constant constant;
		} constant;
#define IR_PUSH_CONSTANT(CONSTANT, RESULT) IR_PUSH(.type = IR_CONSTANT, .operands={(RESULT)}, .constant = {(CONSTANT)})
		struct {
			int non_clobbered_register;
		} call;
#define IR_PUSH_CALL(VARIABLE, NON_CLOBBERED_REGISTER) IR_PUSH(.type = IR_CALL, .operands = {(VARIABLE)}, .call = {(NON_CLOBBERED_REGISTER)})

		struct {
			int sign_extend;
		} int_cast;
#define IR_PUSH_INT_CAST(RESULT, RHS, SIGN_EXTEND) IR_PUSH(.type = IR_INT_CAST, .operands = {(RESULT), (RHS)}, .int_cast = {(SIGN_EXTEND)})
#define IR_PUSH_BOOL_CAST(RESULT, RHS) IR_PUSH(.type = IR_BOOL_CAST, .operands = {(RESULT), (RHS)})
#define IR_PUSH_FLOAT_CAST(RESULT, RHS) IR_PUSH(.type = IR_FLOAT_CAST, .operands = {(RESULT), (RHS)})

		struct {
			int from_float, sign;
		} int_float_cast;
#define IR_PUSH_INT_FLOAT_CAST(RESULT, RHS, FROM_FLOAT, SIGN) IR_PUSH(.type = IR_INT_FLOAT_CAST, .operands = {(RESULT), (RHS)}, .int_float_cast = {(FROM_FLOAT), (SIGN)})

#define IR_PUSH_VA_START(RESULT) IR_PUSH(.type = IR_VA_START, .operands = {(RESULT)})

		struct {
			var_id array;
			struct type *type;
		} va_arg_;
#define IR_PUSH_VA_ARG(ARRAY, RESULT, TYPE) IR_PUSH(.type = IR_VA_ARG, .operands = {(RESULT)}, .va_arg_ = {(ARRAY), (TYPE)})

		struct {
			int dominance;
		} stack_alloc;
#define IR_PUSH_STACK_ALLOC(RESULT, LENGTH, SLOT, DOMINANCE) IR_PUSH(.type = IR_STACK_ALLOC, .operands = {(RESULT), (LENGTH), (SLOT)}, .stack_alloc = {(DOMINANCE)})

#define IR_PUSH_ADD_TEMPORARY(RESULT) IR_PUSH(.type = IR_ADD_TEMPORARY, .operands = {(RESULT)})

		struct {
			int stack_bucket;
		} clear_stack_bucket;
#define IR_PUSH_CLEAR_STACK_BUCKET(STACK_BUCKET) IR_PUSH(.type = IR_CLEAR_STACK_BUCKET, .clear_stack_bucket = {(STACK_BUCKET)})

		struct {
			int register_index, is_sse;
		} set_reg;
#define IR_PUSH_SET_REG(VARIABLE, REGISTER_INDEX, IS_SSE) IR_PUSH(.type = IR_SET_REG, .operands = {(VARIABLE)}, .set_reg = {(REGISTER_INDEX), (IS_SSE)})
		struct {
			int register_index, is_sse;
		} get_reg;
#define IR_PUSH_GET_REG(RESULT, REGISTER_INDEX, IS_SSE) IR_PUSH(.type = IR_GET_REG, .operands = {(RESULT)}, .get_reg = {(REGISTER_INDEX), (IS_SSE)})

		struct {
			int change;
		} modify_stack_pointer;
#define IR_PUSH_MODIFY_STACK_POINTER(CHANGE) IR_PUSH(.type = IR_MODIFY_STACK_POINTER, .modify_stack_pointer = {(CHANGE)})
		struct {
			int offset;
		} store_stack_relative;
#define IR_PUSH_STORE_STACK_RELATIVE(OFFSET, VARIABLE) IR_PUSH(.type = IR_STORE_STACK_RELATIVE, .operands = {(VARIABLE)}, .store_stack_relative = {(OFFSET)})
		struct {
			int offset;
		} load_base_relative;
#define IR_PUSH_LOAD_BASE_RELATIVE(RESULT, OFFSET) IR_PUSH(.type = IR_LOAD_BASE_RELATIVE, .operands = {(RESULT)}, .load_base_relative = {(OFFSET)})
	};
};

struct ir {
	int size, cap;
	struct function *functions;
};

extern struct ir ir;

struct function {
	int is_global;
	const char *name;

	int var_size, var_cap;
	var_id *vars;

	int uses_va;

	int size, cap;
	block_id *blocks;

	void *abi_data;
};

void ir_new_function(struct type *signature, var_id *arguments, const char *name, int is_global);
void ir_call(var_id result, var_id func_var, struct type *function_type, int n_args, struct type **argument_types, var_id *args);

struct block *get_block(block_id id);

struct case_labels {
	int size, cap;

	struct case_label {
		block_id block;
		struct constant value;
	} *labels;

	block_id default_;
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
			struct case_labels labels;
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
	label_id label;
	int size, cap;
	struct instruction *instructions;

	struct block_exit exit;
};

void ir_block_start(block_id id);

void ir_if_selection(var_id condition, block_id block_true, block_id block_false);
void ir_switch_selection(var_id condition, struct case_labels labels);
void ir_goto(block_id jump);
void ir_return(var_id value, struct type *type);
void ir_return_void(void);

void ir_init_var(struct initializer *init, struct type *type, var_id result);
void ir_get_offset(var_id member_address, var_id base_address, var_id offset_var, int offset);
void ir_set_bits(var_id result, var_id field, var_id value, int offset, int length);
void ir_get_bits(var_id result, var_id field, int offset, int length, int sign_extend);

struct function *get_current_function(void);
struct block *get_current_block(void);

void ir_reset(void);

#endif
