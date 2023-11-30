#ifndef IR_H
#define IR_H

#include "operators.h"
#include <codegen/rodata.h>

struct node;

struct node {
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
		IR_BLOCK,
		IR_IF,

		IR_COUNT
	} type;

	struct node *arguments[2];

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
			struct node *block_a, *block_b;
		} phi;

		struct {
			label_id label;
		} block;
	};

	int index;

	int size;
	int spans_block;
	struct node *first_block;
	int used;

	union {
		struct {
			enum {
				VAR_STOR_NONE,
				VAR_STOR_STACK
			} storage;

			int stack_location;
		} cg_info;

		struct {
			struct node *block_true, *block_false;
		} if_info;

		struct {
			int stack_counter; // Used in codegen for allocating variables local to block.
			struct node *jump_to;
			int return_;
		} block_info;
	};

	struct node *next, *child;
};

struct node *new_block(void);
struct function *new_function(void);

extern struct function *first_function;

struct function {
	int is_global;
	const char *name;

	int uses_va;

	struct node *first;

	void *abi_data;

	struct function *next;
};

void ir_block_start(struct node *block);

void ir_if_selection(struct node *condition, struct node *block_true, struct node *block_false);
void ir_goto(struct node *jump);
void ir_connect(struct node *start, struct node *end);
void ir_return(void);

void ir_init_ptr(struct initializer *init, struct type *type, struct node *ptr);

struct function *get_current_function(void);
struct node *get_current_block(void);

void ir_reset(void);

void ir_calculate_block_local_variables(void);

// New interface. Below here all variables should be immutable.
void ir_va_start(struct node *address);
void ir_va_arg(struct node *array, struct node *result_address, struct type *type);

void ir_store(struct node *address, struct node *value);
struct node *ir_load(struct node *address, int size);
struct node *ir_phi(struct node *var_a, struct node *var_b, struct node *block_a, struct node *block_b);
struct node *ir_bool_cast(struct node *operand);
struct node *ir_cast_int(struct node *operand, int target_size, int sign_extend);
struct node *ir_cast_float(struct node *operand, int target_size);
struct node *ir_cast_float_to_int(struct node *operand, int target_size);
struct node *ir_cast_int_to_float(struct node *operand, int target_size, int is_signed);

struct node *ir_binary_not(struct node *operand);
struct node *ir_negate_int(struct node *operand);
struct node *ir_negate_float(struct node *operand);

struct node *ir_binary_and(struct node *lhs, struct node *rhs);
struct node *ir_left_shift(struct node *lhs, struct node *rhs);
struct node *ir_right_shift(struct node *lhs, struct node *rhs, int arithmetic);
struct node *ir_binary_or(struct node *lhs, struct node *rhs);
struct node *ir_add(struct node *lhs, struct node *rhs);
struct node *ir_sub(struct node *lhs, struct node *rhs);
struct node *ir_mul(struct node *lhs, struct node *rhs);
struct node *ir_imul(struct node *lhs, struct node *rhs);
struct node *ir_div(struct node *lhs, struct node *rhs);
struct node *ir_idiv(struct node *lhs, struct node *rhs);
struct node *ir_equal(struct node *lhs, struct node *rhs);
struct node *ir_binary_op(int type, struct node *lhs, struct node *rhs);

struct node *ir_set_bits(struct node *field, struct node *value, int offset, int length);
struct node *ir_get_bits(struct node *field, int offset, int length, int sign_extend);

struct node *ir_get_offset(struct node *base_address, int offset);

struct node *ir_constant(struct constant constant);
void ir_write_constant_to_address(struct constant constant, struct node *address);

void ir_call(struct node *callee, int non_clobbered_register);
struct node *ir_vla_alloc(struct node *length);

void ir_set_reg(struct node *variable, int register_index, int is_sse);
struct node *ir_get_reg(int size, int register_index, int is_sse);

struct node *ir_allocate(int size);
struct node *ir_allocate_preamble(int size);

void ir_modify_stack_pointer(int change);
void ir_store_stack_relative(struct node *variable, int offset);
void ir_store_stack_relative_address(struct node *variable, int offset, int size);
struct node *ir_load_base_relative(int offset, int size);
void ir_load_base_relative_address(struct node *address, int offset, int size);

void ir_set_zero_ptr(struct node *address, int size);

struct node *ir_load_part_address(struct node *address, int offset, int size);
void ir_store_part_address(struct node *address, struct node *value, int offset);

void ir_copy_memory(struct node *destination, struct node *source, int size);

#endif
