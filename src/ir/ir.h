#ifndef IR_H
#define IR_H

#include "variables.h"
#include "operators.h"
#include <codegen/rodata.h>

typedef int block_id;
block_id new_block(void);

struct instruction {
	enum {
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
		IR_ALLOC,
		IR_LOAD,
		IR_STORE,
		IR_CONSTANT,
		IR_CONSTANT_ADDRESS,
		IR_CALL,
		IR_COPY,
		IR_BOOL_CAST,
		IR_INT_CAST_ZERO,
		IR_INT_CAST_SIGN,
		IR_FLOAT_CAST,
		IR_INT_FLOAT_CAST,
		IR_FLOAT_INT_CAST,
		IR_UINT_FLOAT_CAST,
		IR_SET_ZERO_PTR,
		IR_VA_START,
		IR_VA_ARG, //IR_VA_COPY,
		IR_VLA_ALLOC,
		IR_COPY_MEMORY,

		IR_LOAD_PART_ADDRESS,
		IR_STORE_PART_ADDRESS,

		// You should be careful with these instructions.
		// They are here to allow for easier implementation
		// of different calling conventions.
		// Registers might be overwritten by other instructions.
		IR_SET_REG,
		IR_GET_REG,
		IR_MODIFY_STACK_POINTER,
		IR_STORE_STACK_RELATIVE,
		IR_LOAD_BASE_RELATIVE,

		IR_STORE_STACK_RELATIVE_ADDRESS,
		IR_LOAD_BASE_RELATIVE_ADDRESS,

		IR_COUNT
	} type;

	var_id operands[3];

	union {
		struct {
			struct constant constant;
		} constant;

		struct {
			int non_clobbered_register;
		} call;

		struct {
			struct type *type;
		} va_arg_;

		struct {
			int dominance;
		} vla_alloc;

		struct {
			int register_index, is_sse;
		} set_reg;

		struct {
			int register_index, is_sse;
		} get_reg;

		struct {
			int change;
		} modify_stack_pointer;

		struct {
			int offset;
		} store_stack_relative;

		struct {
			int offset, size;
		} store_stack_relative_address;

		struct {
			int offset;
		} load_base_relative;

		struct {
			int offset, size;
		} load_base_relative_address;

		struct {
			int size, stack_location, save_to_preamble;
		} alloc;

		struct {
			int size;
		} set_zero_ptr;

		struct {
			int offset;
		} load_part;

		struct {
			int offset;
		} store_part;

		struct {
			int size;
		} copy_memory;
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
	};
};

struct phi_node {
	var_id var_a, var_b;
	block_id block_a, block_b;

	var_id result;
};

struct block {
	block_id id;
	label_id label;

	int size, cap;
	struct instruction *instructions;

	size_t phi_size, phi_cap;
	struct phi_node *phi_nodes;

	struct block_exit exit;

	int stack_counter; // Used in codegen for allocating variables local to block.
};

void ir_block_start(block_id id);

void ir_if_selection(var_id condition, block_id block_true, block_id block_false);
void ir_switch_selection(var_id condition, struct case_labels labels);
void ir_goto(block_id jump);
void ir_return(void);

void ir_init_ptr(struct initializer *init, struct type *type, var_id ptr);

struct function *get_current_function(void);
struct block *get_current_block(void);

void ir_reset(void);

void ir_calculate_block_local_variables(void);

// New interface. Below here all variables should be immutable.
void ir_va_start(var_id address);
void ir_va_arg(var_id array, var_id result_address, struct type *type);

void ir_store(var_id address, var_id value);
var_id ir_load(var_id address, int size);
var_id ir_copy(var_id var); // TODO: Remove.
var_id ir_phi(var_id var_a, var_id var_b, block_id block_a, block_id block_b);
var_id ir_bool_cast(var_id operand);
var_id ir_cast_int(var_id operand, int target_size, int sign_extend);
var_id ir_cast_float(var_id operand, int target_size);
var_id ir_cast_float_to_int(var_id operand, int target_size);
var_id ir_cast_int_to_float(var_id operand, int target_size, int is_signed);

var_id ir_binary_not(var_id operand);
var_id ir_negate_int(var_id operand);
var_id ir_negate_float(var_id operand);

var_id ir_binary_and(var_id lhs, var_id rhs);
var_id ir_left_shift(var_id lhs, var_id rhs);
var_id ir_right_shift(var_id lhs, var_id rhs, int arithmetic);
var_id ir_binary_or(var_id lhs, var_id rhs);
var_id ir_add(var_id lhs, var_id rhs);
var_id ir_sub(var_id lhs, var_id rhs);
var_id ir_mul(var_id lhs, var_id rhs);
var_id ir_imul(var_id lhs, var_id rhs);
var_id ir_div(var_id lhs, var_id rhs);
var_id ir_idiv(var_id lhs, var_id rhs);
var_id ir_binary_op(int type, var_id lhs, var_id rhs);

var_id ir_set_bits(var_id field, var_id value, int offset, int length);
var_id ir_get_bits(var_id field, int offset, int length, int sign_extend);

var_id ir_get_offset(var_id base_address, int offset);

var_id ir_constant(struct constant constant);
void ir_write_constant_to_address(struct constant constant, var_id address);

void ir_call(var_id callee, int non_clobbered_register);
var_id ir_vla_alloc(var_id length);

void ir_set_reg(var_id variable, int register_index, int is_sse);
var_id ir_get_reg(int size, int register_index, int is_sse);

var_id ir_allocate(int size);
var_id ir_allocate_preamble(int size);

void ir_modify_stack_pointer(int change);
void ir_store_stack_relative(var_id variable, int offset);
void ir_store_stack_relative_address(var_id variable, int offset, int size);
var_id ir_load_base_relative(int offset, int size);
void ir_load_base_relative_address(var_id address, int offset, int size);

void ir_set_zero_ptr(var_id address, int size);

var_id ir_load_part_address(var_id address, int offset, int size);
void ir_store_part_address(var_id address, var_id value, int offset);

void ir_copy_memory(var_id destination, var_id source, int size);

#endif
