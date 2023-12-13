#ifndef IR_H
#define IR_H

#include "operators.h"
#include <codegen/rodata.h>

// Extra variables:
// state (memory, registers)

#define IR_MAX 4

struct node {
	enum {
		IR_ADD,
		IR_SUB,
		IR_MUL,
		IR_IMUL,
		IR_DIV,
		IR_IDIV,
		IR_MOD,
		IR_IMOD,
		IR_LSHIFT,
		IR_RSHIFT,
		IR_IRSHIFT,
		IR_BXOR,
		IR_BOR,
		IR_BAND,
		IR_LESS,
		IR_ILESS,
		IR_GREATER,
		IR_IGREATER,
		IR_LESS_EQ,
		IR_ILESS_EQ,
		IR_GREATER_EQ,
		IR_IGREATER_EQ,
		IR_EQUAL,
		IR_NOT_EQUAL,
		IR_FLT_ADD,
		IR_FLT_SUB,
		IR_FLT_MUL,
		IR_FLT_DIV,
		IR_FLT_LESS,
		IR_FLT_GREATER,
		IR_FLT_LESS_EQ,
		IR_FLT_GREATER_EQ,
		IR_FLT_EQUAL,
		IR_FLT_NOT_EQUAL,
		IR_NEGATE_INT,
		IR_NEGATE_FLOAT,
		IR_BINARY_NOT,
		IR_ALLOC,
		IR_LOAD_VOLATILE,
		IR_LOAD,
		IR_STORE,
		IR_CONSTANT,
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
		IR_VA_ARG,
		IR_VLA_ALLOC,
		IR_COPY_MEMORY,
		IR_LOAD_PART_ADDRESS,
		IR_STORE_PART_ADDRESS,
		IR_SET_REG,
		IR_GET_REG,
		IR_ALLOCATE_CALL_STACK,
		IR_STORE_STACK_RELATIVE,
		IR_STORE_STACK_RELATIVE_ADDRESS,
		IR_LOAD_BASE_RELATIVE,
		IR_LOAD_BASE_RELATIVE_ADDRESS,
		IR_PHI,
		IR_REGION,
		IR_PROJECT,
		IR_IF,
		IR_RETURN,
		IR_FUNCTION,
		IR_ZERO,
		IR_DEAD,
		IR_UNDEFINED,

		IR_COUNT
	} type;

	struct node *arguments[IR_MAX];
	struct node *projects[4];

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
		} allocate_call_stack;

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
			int size, stack_location, alignment;
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
			int index;
		} project;

		struct {
			int is_global;
			const char *name;
			int preamble_alloc;

			int uses_va;

			void *abi_data;
		} function;
	};

	int index;

	int size;
	int spans_block;
	struct node *first_block;
	int used;
	int visited;
	struct node *parent_function, *block;

	struct node *scratch;

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
			struct node *end;
			struct node *state, *incomplete_phi;
			int is_sealed;
			label_id label;

			size_t children_size, children_cap;
			struct node **children;

			int post_idx, dom_depth;
			struct node *idom; // Immediate dominator of block.
		} block_info;
	};

	struct node *next, *child;

	size_t use_size, use_cap;
	struct node **uses;
};

int node_is_control(struct node *node);
int node_argument_count(struct node *node);
int node_is_instruction(struct node *node);
int node_is_tuple(struct node *node);

struct node *new_block(void);
struct node *new_function(const char *name, int is_global);

extern struct node *first_function;

void ir_block_start(struct node *block);

void ir_if_selection(struct node *condition, struct node **block_true, struct node **block_false);
void ir_goto(struct node *jump);
void ir_connect(struct node *start, struct node *end);
struct node *ir_region(struct node *a, struct node *b);
void ir_return(struct node *reg_state);

void ir_init_ptr(struct initializer *init, struct type *type, struct node *ptr);

struct node *get_current_function(void);
void set_current_function(struct node *function);
struct node *get_current_block(void);

void ir_reset(void);

void ir_calculate_block_local_variables(void);

// New interface. Below here all variables should be immutable.
void ir_va_start(struct node *address);
void ir_va_arg(struct node *array, struct node *result_address, struct type *type);

void ir_store(struct node *address, struct node *value);
struct node *ir_load(struct node *address, int size);
struct node *ir_phi(struct node *var_a, struct node *var_b);
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

void ir_call(struct node *callee, struct node *reg_state, struct node *call_stack,
			 int non_clobbered_register,
			 struct node **reg_source);
struct node *ir_vla_alloc(struct node *length);

struct node *ir_set_reg(struct node *variable, struct node *reg_state, int register_index, int is_sse);
struct node *ir_get_reg(struct node *source, int size, int register_index, int is_sse);

struct node *ir_allocate(int size, int alignment);
void ir_allocate_preamble(int size);

struct node *ir_allocate_call_stack(int change);
struct node *ir_store_stack_relative(struct node *call_stack, struct node *variable, int offset);
struct node *ir_store_stack_relative_address(struct node *call_stack, struct node *variable, int offset, int size);
struct node *ir_load_base_relative(struct node *call_stack, int offset, int size);
void ir_load_base_relative_address(struct node *call_stack, struct node *address, int offset, int size);

void ir_set_zero_ptr(struct node *address, int size);

struct node *ir_load_part_address(struct node *address, int offset, int size);
void ir_store_part_address(struct node *address, struct node *value, int offset);

void ir_copy_memory(struct node *destination, struct node *source, int size);

void node_set_argument(struct node *node, int index, struct node *argument);
struct node *ir_project(struct node *node, int index, int size);
struct node *ir_zero(int size);

void ir_schedule_blocks(void);
void ir_local_schedule(void);
void ir_seal_blocks(void);

void ir_get_node_list(struct node ***nodes, size_t *size);

void ir_replace_node(struct node *original, struct node *replacement);

struct node *node_get_prev_state(struct node *node);

struct node *ir_new(int type, int size);
struct node *ir_new1(int type, struct node *op, int size);
struct node *ir_new2(int type, struct node *op1, struct node *op2, int size);
struct node *ir_new3(int type, struct node *op1, struct node *op2, struct node *op3, int size);

#endif
