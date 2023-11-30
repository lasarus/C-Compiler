#ifndef IR_H
#define IR_H

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

		IR_PHI,

		IR_COUNT
	} type;

	struct instruction *arguments[2];

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

		struct {
			block_id block_a, block_b;
		} phi;
	};

	int index;

	int size;
	int spans_block, first_block, used;

	struct codegen_info {
		enum {
			VAR_STOR_NONE,
			VAR_STOR_STACK
		} storage;

		int stack_location;
	} cg_info;

	struct instruction *next;
};

struct ir {
	size_t size, cap;
	struct function *functions;
};

extern struct ir ir;

struct function {
	int is_global;
	const char *name;

	int uses_va;

	size_t size, cap;
	block_id *blocks;

	void *abi_data;
};

struct block *get_block(block_id id);

struct case_labels {
	size_t size, cap;

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
			struct instruction *condition;
			struct case_labels labels;
		} switch_;

		struct {
			struct instruction *condition;
			block_id block_true, block_false;
		} if_;

		block_id jump;
	};
};

struct block {
	block_id id;
	label_id label;

	struct instruction *first, *last;

	struct block_exit exit;

	int stack_counter; // Used in codegen for allocating variables local to block.
};

void ir_block_start(block_id id);

void ir_if_selection(struct instruction *condition, block_id block_true, block_id block_false);
void ir_switch_selection(struct instruction *condition, struct case_labels labels);
void ir_goto(block_id jump);
void ir_return(void);

void ir_init_ptr(struct initializer *init, struct type *type, struct instruction *ptr);

struct function *get_current_function(void);
struct block *get_current_block(void);

void ir_reset(void);

void ir_calculate_block_local_variables(void);

// New interface. Below here all variables should be immutable.
void ir_va_start(struct instruction *address);
void ir_va_arg(struct instruction *array, struct instruction *result_address, struct type *type);

void ir_store(struct instruction *address, struct instruction *value);
struct instruction *ir_load(struct instruction *address, int size);
struct instruction *ir_phi(struct instruction *var_a, struct instruction *var_b, block_id block_a, block_id block_b);
struct instruction *ir_bool_cast(struct instruction *operand);
struct instruction *ir_cast_int(struct instruction *operand, int target_size, int sign_extend);
struct instruction *ir_cast_float(struct instruction *operand, int target_size);
struct instruction *ir_cast_float_to_int(struct instruction *operand, int target_size);
struct instruction *ir_cast_int_to_float(struct instruction *operand, int target_size, int is_signed);

struct instruction *ir_binary_not(struct instruction *operand);
struct instruction *ir_negate_int(struct instruction *operand);
struct instruction *ir_negate_float(struct instruction *operand);

struct instruction *ir_binary_and(struct instruction *lhs, struct instruction *rhs);
struct instruction *ir_left_shift(struct instruction *lhs, struct instruction *rhs);
struct instruction *ir_right_shift(struct instruction *lhs, struct instruction *rhs, int arithmetic);
struct instruction *ir_binary_or(struct instruction *lhs, struct instruction *rhs);
struct instruction *ir_add(struct instruction *lhs, struct instruction *rhs);
struct instruction *ir_sub(struct instruction *lhs, struct instruction *rhs);
struct instruction *ir_mul(struct instruction *lhs, struct instruction *rhs);
struct instruction *ir_imul(struct instruction *lhs, struct instruction *rhs);
struct instruction *ir_div(struct instruction *lhs, struct instruction *rhs);
struct instruction *ir_idiv(struct instruction *lhs, struct instruction *rhs);
struct instruction *ir_binary_op(int type, struct instruction *lhs, struct instruction *rhs);

struct instruction *ir_set_bits(struct instruction *field, struct instruction *value, int offset, int length);
struct instruction *ir_get_bits(struct instruction *field, int offset, int length, int sign_extend);

struct instruction *ir_get_offset(struct instruction *base_address, int offset);

struct instruction *ir_constant(struct constant constant);
void ir_write_constant_to_address(struct constant constant, struct instruction *address);

void ir_call(struct instruction *callee, int non_clobbered_register);
struct instruction *ir_vla_alloc(struct instruction *length);

void ir_set_reg(struct instruction *variable, int register_index, int is_sse);
struct instruction *ir_get_reg(int size, int register_index, int is_sse);

struct instruction *ir_allocate(int size);
struct instruction *ir_allocate_preamble(int size);

void ir_modify_stack_pointer(int change);
void ir_store_stack_relative(struct instruction *variable, int offset);
void ir_store_stack_relative_address(struct instruction *variable, int offset, int size);
struct instruction *ir_load_base_relative(int offset, int size);
void ir_load_base_relative_address(struct instruction *address, int offset, int size);

void ir_set_zero_ptr(struct instruction *address, int size);

struct instruction *ir_load_part_address(struct instruction *address, int offset, int size);
void ir_store_part_address(struct instruction *address, struct instruction *value, int offset);

void ir_copy_memory(struct instruction *destination, struct instruction *source, int size);

#endif
