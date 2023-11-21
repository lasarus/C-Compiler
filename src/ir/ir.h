#ifndef IR_H
#define IR_H

#include "variables.h"
#include "operators.h"
#include <codegen/rodata.h>

typedef int block_id;
block_id new_block(void);

struct instruction;
void ir_push(struct instruction instruction);
#define IR_PUSH(...) do { ir_push((struct instruction) { __VA_ARGS__ }); } while(0)

void ir_push1(int type, var_id op1);
void ir_push2(int type, var_id op1, var_id op2);
void ir_push3(int type, var_id op1, var_id op2, var_id op3);

struct instruction {
	enum {
		IR_NULL,
		IR_ADD, IR_SUB,
		IR_MUL, IR_IMUL,
		IR_DIV, IR_IDIV,
		IR_MOD, IR_IMOD,
		IR_LSHIFT, IR_RSHIFT, IR_IRSHIFT,
		IR_BXOR, IR_BOR, IR_BAND,
		IR_LESS, IR_ILESS,
		IR_GREATER, IR_IGREATER,
		IR_LESS_EQ, IR_ILESS_EQ,
		IR_GREATER_EQ, IR_IGREATER_EQ,
		IR_EQUAL, IR_NOT_EQUAL,

		IR_FLT_ADD, IR_FLT_SUB,
		IR_FLT_MUL, IR_FLT_DIV,
		IR_FLT_LESS, IR_FLT_GREATER,
		IR_FLT_LESS_EQ,
		IR_FLT_GREATER_EQ,
		IR_FLT_EQUAL,
		IR_FLT_NOT_EQUAL,

		IR_NEGATE_INT,
		IR_NEGATE_FLOAT,
		IR_BINARY_NOT,
		IR_LOAD,
		IR_STORE,
		IR_ADDRESS_OF,
		IR_CONSTANT,
		IR_CALL,
		IR_COPY,
		IR_BOOL_CAST,
		IR_INT_CAST_ZERO,
		IR_INT_CAST_SIGN,
		IR_FLOAT_CAST,
		IR_INT_FLOAT_CAST,
		IR_FLOAT_INT_CAST,
		IR_UINT_FLOAT_CAST,
		IR_SET_ZERO,
		IR_VA_START,
		IR_VA_ARG, //IR_VA_COPY,
		IR_STACK_ALLOC,
		IR_ADD_TEMPORARY,
		IR_CLEAR_STACK_BUCKET,

		// You should be careful with these instructions.
		// They are here to allow for easier implementation
		// of different calling conventions.
		// Registers might be overwritten by other instructions.
		IR_SET_REG,
		IR_GET_REG,
		IR_MODIFY_STACK_POINTER,
		IR_STORE_STACK_RELATIVE,
		IR_LOAD_BASE_RELATIVE,

		IR_COUNT
	} type;

	var_id operands[3];

	union {
		struct {
			struct constant constant;
		} constant;
#define IR_PUSH_CONSTANT(CONSTANT, RESULT) IR_PUSH(.type = IR_CONSTANT, .operands={(RESULT)}, .constant = {(CONSTANT)})
		struct {
			int non_clobbered_register;
		} call;
#define IR_PUSH_CALL(VARIABLE, NON_CLOBBERED_REGISTER) IR_PUSH(.type = IR_CALL, .operands = {(VARIABLE)}, .call = {(NON_CLOBBERED_REGISTER)})

		struct {
			var_id array;
			struct type *type;
		} va_arg_;
#define IR_PUSH_VA_ARG(ARRAY, RESULT, TYPE) IR_PUSH(.type = IR_VA_ARG, .operands = {(RESULT)}, .va_arg_ = {(ARRAY), (TYPE)})

		struct {
			int dominance;
		} stack_alloc;
#define IR_PUSH_STACK_ALLOC(RESULT, LENGTH, SLOT, DOMINANCE) IR_PUSH(.type = IR_STACK_ALLOC, .operands = {(RESULT), (LENGTH), (SLOT)}, .stack_alloc = {(DOMINANCE)})

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
