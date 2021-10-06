#include <stdio.h>
#include <string.h>
#include <arch/builtins.h>
#include <arch/calling.h>
#include "codegen.h"
#include "registers.h"

#include <assert.h>

#include "binary_operators.h"
#include "unary_operators.h"
#include "cast_operators.h"

#include <parser/declaration.h>

int calling_convention[] = {REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9};

int ir_pos = 0;
int ir_count = 0;
struct instruction *is = NULL;

struct instruction *ir_next() {
	static int counter = 0;
	struct program *program = get_program();
	if (counter < program->size)
		return &program->instructions[counter++];
	else
		return NULL;
}

struct {
	FILE *out;
	const char *current_section;
	int local_counter;
} data;


void set_section(const char *section) {
	if (strcmp(section, data.current_section) != 0)
		EMIT(".section %s", section);
	data.current_section = section;
}

FILE *get_fp(void) {
	return data.out;
}

struct {
	enum {
		VAR_STOR_NONE,
		VAR_STOR_STACK
	} storage;

	int stack_location;
} *variable_info;

enum operand_type ot_from_st(enum simple_type st) {
	switch (st) {
	case ST_INT: return OT_INT;
	case ST_UINT: return OT_UINT;
	case ST_LONG: return OT_LONG;
	case ST_ULONG: return OT_ULONG;
	case ST_LLONG: return OT_LLONG;
	case ST_ULLONG: return OT_ULLONG;
	default: ERROR("Invalid operand type %d", st);
	}
}

void codegen_binary_operator(int operator_type, var_id out,
							 var_id lhs, var_id rhs) {
	struct type *data_type = get_variable_type(out);
	struct type *lhs_type = get_variable_type(lhs);
	struct type *rhs_type = get_variable_type(rhs);

	enum operand_type ot;

	if (type_is_pointer(lhs_type) && type_is_pointer(rhs_type)) {
		ot = OT_PTR;
	} else {
		if(rhs_type != lhs_type) {
			ERROR("Can't codegen binary operator %d between %s and %s",
				  operator_type, strdup(type_to_string(lhs_type)),
				  strdup(type_to_string(rhs_type)));
		}

		assert(data_type->type == TY_SIMPLE);
		assert(rhs_type->type == TY_SIMPLE);

		ot = ot_from_st(rhs_type->simple);
	}

	scalar_to_reg(variable_info[lhs].stack_location, lhs, REG_RDI);
	scalar_to_reg(variable_info[rhs].stack_location, rhs, REG_RSI);

	EMIT("%s", binary_operator_outputs[ot][operator_type]);

	reg_to_scalar(REG_RAX, variable_info[out].stack_location, out);
}

void codegen_unary_operator(int operator_type, var_id out,
							var_id rhs) {
	struct type *data_type = get_variable_type(out);
	struct type *in_type = get_variable_type(rhs);

	if(data_type->type != TY_SIMPLE || in_type->type != TY_SIMPLE) {
		printf("Invalid types in \"%s\" and out \"%s\"\n",
			   strdup(type_to_string(in_type)),
			   strdup(type_to_string(data_type)));
		NOTIMP();
	}

	enum operand_type ot = ot_from_st(data_type->simple);

	scalar_to_reg(variable_info[rhs].stack_location, rhs, REG_RDI);

	EMIT("%s", unary_operator_outputs[ot][operator_type]);

	reg_to_scalar(REG_RAX, variable_info[out].stack_location, out);
}

void codegen_simple_cast(var_id in, var_id out) {
	struct type *in_type = get_variable_type(in);
	struct type *out_type = get_variable_type(out);

	assert(in_type->type == TY_SIMPLE);
	assert(out_type->type == TY_SIMPLE);

	scalar_to_reg(variable_info[in].stack_location, in, REG_RDI);

	EMIT("%s", cast_operator_outputs[in_type->simple][out_type->simple]);

	reg_to_scalar(REG_RAX, variable_info[out].stack_location, out);
}

struct classification {
	int n_parts;
	enum parameter_class classes[4];
	int pass_in_memory; // TODO: remove
	int arg_size; // TODO: remove this or n_parts
	union {
		int regs[4];
		int mem_pos;
	};
};

void classify_parameters(int n_args, var_id *args, struct type *return_type,
						 struct classification *classifications,
						 struct classification *return_classification,
						 int *total_memory_argument_size,
						 int *current_gp_reg) {
	if (return_type != type_simple(ST_VOID)) {
		classify(return_type, &return_classification->n_parts,
				 return_classification->classes);

		if (return_classification->n_parts == 1 &&
			return_classification->classes[0] == CLASS_MEMORY) {
			return_classification->pass_in_memory = 1;
		} else {
			return_classification->pass_in_memory = 0;
			int return_convention[] = {REG_RAX, REG_RDX};
			for (int j = 0; j < return_classification->n_parts; j++) {
				if (return_classification->classes[j] == CLASS_INTEGER)
					return_classification->regs[j] = return_convention[j];
				else
					NOTIMP();
			}
		}
	} else {
		return_classification->pass_in_memory = 0;
	}

	*total_memory_argument_size = 0;
	*current_gp_reg = 0;
	if (return_classification->pass_in_memory)
		(*current_gp_reg)++;
	for (int i = 0; i < n_args; i++) {
		struct type *type = get_variable_type(args[i]);
		struct classification *classification = classifications + i;

		int size_rounded = round_up_to_nearest(calculate_size(type), 8);

		classification->pass_in_memory = 1;
		classification->arg_size = size_rounded;

		classify(type, &classification->n_parts, classification->classes);

		if (classification->n_parts == 1 &&
			classification->classes[0] == CLASS_MEMORY) {
			classification->mem_pos = *total_memory_argument_size;
			*total_memory_argument_size += size_rounded;
		} else {
			if (classification->n_parts + *current_gp_reg > 6) {
				classification->mem_pos = *total_memory_argument_size;
				*total_memory_argument_size += size_rounded;
			} else {
				for (int j = 0; j < classification->n_parts; j++) {
					classification->regs[j] = calling_convention[(*current_gp_reg)++];
				}
				classification->pass_in_memory = 0;
			}
		}
	}
}

void codegen_call(var_id variable, int n_args, var_id *args, var_id result) {
	int total_memory_argument_size, current_gp_reg;
	struct type *return_type = get_variable_type(result);

	scalar_to_reg(variable_info[variable].stack_location,
				  variable,
				  REG_RBX);

	struct classification classifications[n_args];
	struct classification return_classification;
	classify_parameters(n_args, args, return_type,
						classifications,
						&return_classification,
						&total_memory_argument_size,
						&current_gp_reg);

	if (return_classification.pass_in_memory) {
		EMIT("leaq -%d(%%rbp), %%rdi", variable_info[result].stack_location);
	}

	EMIT("subq $%d, %%rsp", round_up_to_nearest(total_memory_argument_size, 16));

	int current_stack_pos = 0;
	for (int i = 0; i < n_args; i++) {
		struct classification *classification = classifications + i;
		var_id var = args[i];

		if (classification->pass_in_memory) {
			EMIT("#Passed in stack:");
			int stack_loc = variable_info[var].stack_location;
			for (int i = 0; i < classification->arg_size; i += 8) {
				EMIT("movq %d(%%rbp), %%rax", -stack_loc + i);
				EMIT("movq %%rax, %d(%%rsp)", current_stack_pos);
				current_stack_pos += 8;
			}
		} else {
			EMIT("#Passed in register:");
			for (int i = 0; i < classification->n_parts; i++) {
				if (classification->classes[i] == CLASS_INTEGER) {
					EMIT("movq -%d(%%rbp), %s",
						 variable_info[var].stack_location - 8 * i,
						 get_reg_name(classification->regs[i], 8));
				} else {
					NOTIMP();
				}
			}
		}
	}

	EMIT("movl $0, %%eax");
	EMIT("callq *%%rbx");

	if (!return_classification.pass_in_memory && return_type != type_simple(ST_VOID)) {
		int var_size = calculate_size(get_variable_type(result));
		for (int i = 0; i < return_classification.n_parts; i++) {
			int size = var_size - 8 * i;
			if (size > 8) size = 8;
			if (return_classification.classes[i] == CLASS_INTEGER) {
				EMIT("mov%c %s, -%d(%%rbp)",
					 size_to_suffix(size),
					 get_reg_name(return_classification.regs[i], size),
					 variable_info[result].stack_location - 8 * i);
			} else {
				NOTIMP();
			}
		}
	}
}

struct reg_save_info {
	int reg_save_position;
	int gp_offset;
	int fp_offset;
};

// Address in rdi.
void codegen_memzero(int len) {
	int pos = 0;
	for (;pos < (len & ~7); pos += 8) {
		EMIT("movq $0, %d(%%rdi)", pos);
	}
	for (;pos < (len & ~3); pos += 4) {
		EMIT("movl $0, %d(%%rdi)", pos);
	}
	for (;pos < (len & ~1); pos += 2) {
		EMIT("movw $0, %d(%%rdi)", pos);
	}
	for (;pos < len; pos += 1) {
		EMIT("movb $0, %d(%%rdi)", pos);
	}
}

// From rdi to rsi address
void codegen_memcpy(int len) {
	int pos = 0;
	for (;pos < (len & ~7); pos += 8) {
		EMIT("movq %d(%%rdi), %%rax", pos);
		EMIT("movq %%rax, %d(%%rsi)", pos);
	}
	for (;pos < (len & ~3); pos += 4) {
		EMIT("movl %d(%%rdi), %%eax", pos);
		EMIT("movl %%eax, %d(%%rsi)", pos);
	}
	for (;pos < (len & ~1); pos += 2) {
		EMIT("movw %d(%%rdi), %%ax", pos);
		EMIT("movw %%ax, %d(%%rsi)", pos);
	}
	for (;pos < len; pos += 1) {
		EMIT("movb %d(%%rdi), %%al", pos);
		EMIT("movb %%al, %d(%%rsi)", pos);
	}
}

void codegen_stackcpy(int dest, int source, int len) {
	int pos = 0;
	for (;pos < (len & ~7); pos += 8) {
		EMIT("movq %d(%%rbp), %%rax", source + pos);
		EMIT("movq %%rax, %d(%%rbp)", dest + pos);
	}
	for (;pos < (len & ~3); pos += 4) {
		EMIT("movl %d(%%rbp), %%eax", source + pos);
		EMIT("movl %%eax, %d(%%rbp)", dest + pos);
	}
	for (;pos < (len & ~1); pos += 2) {
		EMIT("movw %d(%%rbp), %%ax", source + pos);
		EMIT("movw %%ax, %d(%%rbp)", dest + pos);
	}
	for (;pos < len; pos += 1) {
		EMIT("movb %d(%%rbp), %%al", source + pos);
		EMIT("movb %%al, %d(%%rbp)", dest + pos);
	}
}

void constant_to_buffer(uint8_t *buffer, struct constant constant) {
	assert(constant.type == CONSTANT_TYPE);

	if (type_is_pointer(constant.data_type)) {
		*buffer = constant.long_d;
		return;
	}
	assert(constant.data_type->type == TY_SIMPLE);

	switch (constant.data_type->simple) {
	case ST_CHAR:
		*buffer = constant.char_d;
		break;
	case ST_SCHAR:
		*buffer = constant.char_d;
		break;
	case ST_UCHAR:
		*buffer = constant.char_d;
		break;
	case ST_SHORT:
		*(uint16_t *)buffer = constant.short_d;
		break;
	case ST_USHORT:
		*(uint16_t *)buffer = constant.ushort_d;
		break;
	case ST_INT:
		*(uint32_t *)buffer = constant.int_d;
		break;
	case ST_UINT:
		*(uint32_t *)buffer = constant.uint_d;
		break;
	case ST_LONG:
		*(uint64_t *)buffer = constant.long_d;
		break;
	case ST_ULONG:
		*(uint64_t *)buffer = constant.ulong_d;
		break;
	case ST_LLONG:
		*(uint64_t *)buffer = constant.llong_d;
		break;
	case ST_ULLONG:
		*(uint64_t *)buffer = constant.ullong_d;
		break;

	case ST_FLOAT:
	case ST_DOUBLE:
	case ST_LDOUBLE:
	case ST_BOOL:
	case ST_FLOAT_COMPLEX:
	case ST_DOUBLE_COMPLEX:
	case ST_LDOUBLE_COMPLEX:
		NOTIMP();
	default:
		break;
	}
}

void codegen_initializer(struct type *type,
						 struct initializer *init) {
	// TODO: Make this more portable.
	int size = calculate_size(type);
	if (size > 4096) {
		printf("Size: %d\n", size);
		NOTIMP();
	}
	uint8_t buffer[size];
	label_id labels[size];
	int is_label[size];
	for (int i = 0; i < size; i++) {
		buffer[i] = 0;
		is_label[i] = 0;
		labels[i] = 1337;
	}

	for (int i = 0; i < init->n; i++) {
		struct init_pair *pair = init->pairs + i;
		int offset = pair->offset;

		struct constant *c = expression_to_constant(pair->expr);
		if (!c)
			ERROR("Static initialization can't contain non constant expressions!");

		switch (c->type) {
		case CONSTANT_TYPE:
			constant_to_buffer(buffer + offset, *c);
			break;
		default:
			is_label[offset] = 1;
			labels[offset] = c->label;
			//NOTIMP();
		}
	}

	for (int i = 0; i < size; i++) {
		if (is_label[i]) {
			EMIT(".quad %s", get_label_name(labels[i]));
			i += 7;
		} else {
			//TODO: This shouldn't need an integer cast.
			// But right now I can't be bothered to implement
			// implicit integer casts for variadic functions.
			EMIT(".byte %d", (int)buffer[i]);
		}
	}
}

void codegen_static_var(struct instruction ins) {
	set_section(".data");
	if (ins.static_var.global)
		EMIT(".global %s", ins.static_var.label);
	if (ins.static_var.init) {
		EMIT("%s:", ins.static_var.label);

		codegen_initializer(ins.static_var.type,
							ins.static_var.init);
	} else {
		EMIT("%s:", ins.static_var.label);

		EMIT(".zero %d", calculate_size(ins.static_var.type));
	}
	set_section(".text");
}

const char *constant_to_string(struct constant constant) {
	static char buffer[256];
	assert(constant.type == CONSTANT_TYPE);

	if (type_is_pointer(constant.data_type)) {
		sprintf(buffer, "%ld", constant.long_d);
		return buffer;
	}

	enum simple_type st;
	if (constant.data_type->type == TY_SIMPLE)
		st = constant.data_type->simple;
	else
		ERROR("Tried to print type %s to number\n", type_to_string(constant.data_type));

	switch (st) {
	case ST_CHAR:
		sprintf(buffer, "%d", (int)constant.char_d);
		break;
	case ST_SCHAR:
		sprintf(buffer, "%d", (int)constant.schar_d);
		break;
	case ST_UCHAR:
		sprintf(buffer, "%d", (int)constant.uchar_d);
		break;
	case ST_SHORT:
		sprintf(buffer, "%d", (int)constant.short_d);
		break;
	case ST_USHORT:
		sprintf(buffer, "%d", (int)constant.ushort_d);
		break;
	case ST_INT:
		sprintf(buffer, "%d", constant.int_d);
		break;
	case ST_UINT:
		sprintf(buffer, "%u", constant.uint_d);
		break;
	case ST_LONG:
		sprintf(buffer, "%ld", constant.long_d);
		break;
	case ST_ULONG:
		sprintf(buffer, "%lu", constant.ulong_d);
		break;
	case ST_LLONG:
		sprintf(buffer, "%lld", constant.llong_d);
		break;
	case ST_ULLONG:
		sprintf(buffer, "%llu", constant.ullong_d);
		break;

	case ST_FLOAT:
	case ST_DOUBLE:
	case ST_LDOUBLE:
	case ST_BOOL:
	case ST_FLOAT_COMPLEX:
	case ST_DOUBLE_COMPLEX:
	case ST_LDOUBLE_COMPLEX:
		NOTIMP();
	default:
		break;
	}

	return buffer;
}

void codegen_instruction(struct instruction ins, struct instruction next_ins, struct reg_save_info reg_save_info) {
	const char *ins_str = instruction_to_str(ins);
	EMIT("#instruction start \"%s\":", ins_str);
	switch (ins.type) {
	case IR_CONSTANT: {
		struct constant c = ins.constant.constant;
		switch (c.type) {
		case CONSTANT_TYPE: {
			int size = calculate_size(c.data_type);
			switch (size) {
			case 1:
				EMIT("movb $%s, -%d(%%rbp)", constant_to_string(c), variable_info[ins.constant.result].stack_location);
				break;
			case 2:
				EMIT("movw $%s, -%d(%%rbp)", constant_to_string(c), variable_info[ins.constant.result].stack_location);
				break;
			case 4:
				EMIT("movl $%s, -%d(%%rbp)", constant_to_string(c), variable_info[ins.constant.result].stack_location);
				break;
			case 8:
				EMIT("movq $%s, -%d(%%rbp)", constant_to_string(c), variable_info[ins.constant.result].stack_location);
				break;

			case -1:
				// TODO: Is this really right?
				break;

			default:
				ERROR("Not implemented %d\n", size);
			}
		} break;

		case CONSTANT_LABEL:
			EMIT("movq $%s, -%d(%%rbp)", get_label_name(c.label), variable_info[ins.constant.result].stack_location);
			break;

		default:
			NOTIMP();
		}
	} break;

	case IR_BINARY_OPERATOR:
		codegen_binary_operator(ins.binary_operator.type,
								ins.binary_operator.result,
								ins.binary_operator.lhs,
								ins.binary_operator.rhs);
		break;

	case IR_UNARY_OPERATOR:
		codegen_unary_operator(ins.unary_operator.type,
							   ins.unary_operator.result,
							   ins.unary_operator.operand);
		break;

	case IR_RETURN:
		if (!ins.return_.is_void) {
			var_id ret = ins.return_.value;
			struct type *ret_type = get_variable_type(ret);
			if (ret_type != type_simple(ST_VOID)) {
				enum parameter_class classes[4];
				int n_parts = 0;

				classify(ret_type, &n_parts, classes);

				if (n_parts == 1 && classes[0] == CLASS_MEMORY) {
					EMIT("movq -%d(%%rbp), %%rsi", 8);
					EMIT("leaq -%d(%%rbp), %%rdi", variable_info[ret].stack_location);

					codegen_memcpy(calculate_size(ret_type));
				} else if (n_parts == 1 && classes[0] == CLASS_INTEGER) {
					EMIT("movq -%d(%%rbp), %%rax", variable_info[ret].stack_location);
				} else if (n_parts == 2) {
					EMIT("# COMMENT");
					EMIT("movq -%d(%%rbp), %%rax", variable_info[ret].stack_location);
					EMIT("movq -%d(%%rbp), %%rdx", variable_info[ret].stack_location - 8);
				} else {
					NOTIMP();
				}
			}
		}
		EMIT("leave");
		EMIT("ret");
		break;

	case IR_CALL_VARIABLE:
		codegen_call(ins.call_variable.function,
			ins.call_variable.n_args,
			ins.call_variable.args,
			ins.call_variable.result);
		break;

	case IR_POINTER_INCREMENT: {
		var_id index = ins.pointer_increment.index,
			pointer = ins.pointer_increment.pointer,
			result = ins.pointer_increment.result;
		int size = calculate_size(type_deref(get_variable_type(pointer)));
		struct type *index_type = get_variable_type(index);
		if(index_type == type_simple(ST_INT) ||
		   index_type == type_simple(ST_UINT)) {
			scalar_to_reg(variable_info[index].stack_location, index, REG_RDX);
			scalar_to_reg(variable_info[pointer].stack_location, pointer, REG_RSI);
			EMIT("movslq %%edx, %%rdx");
			EMIT("imulq $%d, %%rdx, %%rdx", size);
			EMIT("leaq (%%rsi, %%rdx), %%rax");
			reg_to_scalar(REG_RAX, variable_info[result].stack_location, result);
		} else if(index_type == type_simple(ST_LONG) ||
				  index_type == type_simple(ST_ULONG)) {
			scalar_to_reg(variable_info[index].stack_location, index, REG_RDX);
			scalar_to_reg(variable_info[pointer].stack_location, pointer, REG_RSI);
			EMIT("imulq $%d, %%rdx, %%rdx", size);
			EMIT("leaq (%%rsi, %%rdx), %%rax");
			reg_to_scalar(REG_RAX, variable_info[result].stack_location, result);
		} else {
			ERROR("Pointer increment by %s not implemented",
				  type_to_string(get_variable_type(index)));
		}
	} break;

	case IR_POINTER_DIFF: {
		var_id result = ins.pointer_diff.result,
			lhs = ins.pointer_diff.lhs,
			rhs = ins.pointer_diff.rhs;

		int size = calculate_size(type_deref(get_variable_type(lhs)));

		scalar_to_reg(variable_info[lhs].stack_location,
					  lhs, REG_RAX);
		scalar_to_reg(variable_info[rhs].stack_location,
					  rhs, REG_RDX);
		EMIT("subq %%rdx, %%rax");
		EMIT("movq $%d, %%rdi", size);
		EMIT("cqto");
		EMIT("idivq %%rdi");

		reg_to_scalar(REG_RAX, variable_info[result].stack_location,
					  result);
	} break;

	case IR_LOAD: {
		struct type *type = get_variable_type(ins.load.result);
		scalar_to_reg(variable_info[ins.load.pointer].stack_location, ins.load.pointer, REG_RDI);
		EMIT("leaq -%d(%%rbp), %%rsi", variable_info[ins.load.result].stack_location);

		codegen_memcpy(calculate_size(type));

	} break;

	case IR_STORE: {
		struct type *type = get_variable_type(ins.store.value);
		scalar_to_reg(variable_info[ins.store.pointer].stack_location, ins.store.pointer, REG_RSI);
		EMIT("leaq -%d(%%rbp), %%rdi", variable_info[ins.store.value].stack_location);

		codegen_memcpy(calculate_size(type));
	} break;

	case IR_START_BLOCK: {
		EMIT(".LB%d:\n", ins.start_block.block);
	} break;

	case IR_GOTO: {
		if (!(next_ins.type == IR_START_BLOCK &&
			  next_ins.start_block.block == ins.goto_.block))
			EMIT("jmp .LB%d\n", ins.goto_.block);
	} break;

	case IR_IF_SELECTION: {
		var_id cond = ins.if_selection.condition;
		struct type *type = get_variable_type(cond);
		int size = calculate_size(type);
		if (size == 1 || size == 2 || size == 4 || size == 8) {
			scalar_to_reg(variable_info[cond].stack_location, cond, REG_RDI);
			const char *reg_name = get_reg_name(REG_RDI, size);
			EMIT("test%c %s, %s", size_to_suffix(size), reg_name, reg_name);
			EMIT("je .LB%d", ins.if_selection.block_false);
			if (!(next_ins.type == IR_START_BLOCK &&
				  next_ins.start_block.block == ins.if_selection.block_true))
				EMIT("jmp .LB%d", ins.if_selection.block_true);
		} else {
			ERROR("Invalid argument to if selection");
		}
	} break;

	case IR_COPY: {
		var_id source = ins.copy.source,
			dest = ins.copy.result;
		struct type *type = get_variable_type(source);

		struct type *dest_type = get_variable_type(dest);
		if (calculate_size(type) != calculate_size(dest_type)) {
			printf("\"%s\" != \"%s\"\n", strdup(type_to_string(type)), strdup(type_to_string(dest_type)));
			ERROR("Incorrect types");
		}

		if (is_scalar(type)) {
			scalar_to_reg(variable_info[source].stack_location, source, REG_RAX);
			reg_to_scalar(REG_RAX, variable_info[dest].stack_location, dest);
		} else {
			EMIT("leaq -%d(%%rbp), %%rdi", variable_info[source].stack_location);
			EMIT("leaq -%d(%%rbp), %%rsi", variable_info[dest].stack_location);
			codegen_memcpy(calculate_size(type));
		}
	} break;

	case IR_CAST: {
		var_id source = ins.cast.rhs,
			dest = ins.cast.result;
		struct type *dest_type = get_variable_type(dest);
		struct type *source_type = get_variable_type(source);

		if (dest_type == type_simple(ST_VOID)) {
			// No o .
		} else if (type_is_pointer(dest_type) && type_is_pointer(source_type)) {
			scalar_to_reg(variable_info[source].stack_location, source, REG_RAX);
			reg_to_scalar(REG_RAX, variable_info[dest].stack_location, dest);
		} else if (dest_type->type == TY_SIMPLE &&
				   source_type->type == TY_SIMPLE) {
			codegen_simple_cast(source, dest);
		} else if ((type_is_pointer(dest_type) && source_type->type == TY_SIMPLE) ||
				   (type_is_pointer(source_type) && dest_type->type == TY_SIMPLE)) {
			scalar_to_reg(variable_info[source].stack_location, source, REG_RAX);
			reg_to_scalar(REG_RAX, variable_info[dest].stack_location, dest);
		} else {
			ERROR("Trying to cast from : \"%s\" to \"%s\"",
				  strdup(type_to_string(source_type)),
				  strdup(type_to_string(dest_type)));
		}
	} break;

	case IR_ADDRESS_OF:
		EMIT("leaq -%d(%%rbp), %%rax", variable_info[ins.address_of.variable].stack_location);
		reg_to_scalar(REG_RAX, variable_info[ins.address_of.result].stack_location, ins.address_of.result);
		break;

	case IR_GET_MEMBER: {
		struct type *type = type_deref(get_variable_type(ins.get_member.pointer));
		int member_offset;
		struct type *member_type;
		type_select(type, ins.get_member.index, &member_offset, &member_type);

		scalar_to_reg(variable_info[ins.get_member.pointer].stack_location, ins.get_member.pointer, REG_RAX);
		EMIT("addq $%d, %%rax", member_offset);
		reg_to_scalar(REG_RAX, variable_info[ins.get_member.result].stack_location, ins.get_member.result);
	} break;

	case IR_VA_START: {
		int gp_offset_offset = builtin_va_list->offsets[0];
		int fp_offset_offset = builtin_va_list->offsets[1];
		int overflow_arg_area_offset = builtin_va_list->offsets[2];
		int reg_save_area_offset = builtin_va_list->offsets[3];
		scalar_to_reg(variable_info[ins.va_start_.result].stack_location, ins.va_start_.result, REG_RAX);
		EMIT("movl $%d, %d(%%rax)", reg_save_info.gp_offset, gp_offset_offset);
		EMIT("movl $%d, %d(%%rax)", 0, fp_offset_offset);
		EMIT("movq $%d, %d(%%rax)", 0, overflow_arg_area_offset);

		EMIT("leaq -%d(%%rbp), %%rdi", reg_save_info.reg_save_position);
		EMIT("movq %%rdi, %d(%%rax)", reg_save_area_offset);
	} break;

	case IR_VA_ARG: {
		static int va_arg_labels = 0;
		if (ins.va_arg_.type == type_simple(ST_INT)) {
			int gp_offset_offset = builtin_va_list->offsets[0];
			//int fp_offset_offset = builtin_va_list->offsets[1];
			int overflow_arg_area_offset = builtin_va_list->offsets[2];
			int reg_save_area_offset = builtin_va_list->offsets[3];
			scalar_to_reg(variable_info[ins.va_arg_.array].stack_location, ins.va_arg_.array, REG_RDI);
			EMIT("movl %d(%%rdi), %%eax", gp_offset_offset);
			EMIT("cmpl $48, %%eax");
			va_arg_labels++;
			EMIT("jae .va_arg_stack%d", va_arg_labels);
			EMIT("leal 8(%%rax), %%edx");
			EMIT("addq %d(%%rdi), %%rax", reg_save_area_offset);
			EMIT("movl %%edx, %d(%%rdi)", gp_offset_offset);
			EMIT("jmp .va_arg_fetch%d", va_arg_labels);
			EMIT(".va_arg_stack%d:", va_arg_labels);
			EMIT("movq %d(%%rdi), %%rax", overflow_arg_area_offset);
			EMIT("leaq 8(%%rax), %%rdx");
			EMIT("movq %%rdx, %d(%%rdi)", overflow_arg_area_offset);
			EMIT(".va_arg_fetch%d:", va_arg_labels);
			EMIT("movl (%%rax), %%eax");
			
			reg_to_scalar(REG_RAX, variable_info[ins.va_arg_.result].stack_location, ins.va_arg_.result);

			// movl l->gp_offset, %%eax
			// cmpl $48, %%eax
			// jae stack
			// leal $8(%%rax), %%edx
			// addq l->reg_save_area, %%rax
			// movl %%edx, l->gp_offset
			// jmp fetch
			// stack: movq l->overflow_arg_area, %%rax
			// leaq 8(%%rax), %%rdx
			// movq %%rdx, l->overflow_arg_area
			// fetch: movl (%%rax), %%eax
		} else if (ins.va_arg_.type->type == TY_POINTER) {
			int gp_offset_offset = builtin_va_list->offsets[0];
			//int fp_offset_offset = builtin_va_list->offsets[1];
			int overflow_arg_area_offset = builtin_va_list->offsets[2];
			int reg_save_area_offset = builtin_va_list->offsets[3];
			scalar_to_reg(variable_info[ins.va_arg_.array].stack_location, ins.va_arg_.array, REG_RDI);
			EMIT("movl %d(%%rdi), %%eax", gp_offset_offset);
			EMIT("cmpl $48, %%eax");
			va_arg_labels++;
			EMIT("jae .va_arg_stack%d", va_arg_labels);
			EMIT("leal 8(%%rax), %%edx");
			EMIT("addq %d(%%rdi), %%rax", reg_save_area_offset);
			EMIT("movl %%edx, %d(%%rdi)", gp_offset_offset);
			EMIT("jmp .va_arg_fetch%d", va_arg_labels);
			EMIT(".va_arg_stack%d:", va_arg_labels);
			EMIT("movq %d(%%rdi), %%rax", overflow_arg_area_offset);
			EMIT("leaq 8(%%rax), %%rdx");
			EMIT("movq %%rdx, %d(%%rdi)", overflow_arg_area_offset);
			EMIT(".va_arg_fetch%d:", va_arg_labels);
			EMIT("movq (%%rax), %%rax");
			
			reg_to_scalar(REG_RAX, variable_info[ins.va_arg_.result].stack_location, ins.va_arg_.result);
		} else {
			NOTIMP();
		}
	} break;

	case IR_SET_ZERO:
		EMIT("leaq -%d(%%rbp), %%rdi", variable_info[ins.set_zero.variable].stack_location);
		codegen_memzero(calculate_size(get_variable_type(ins.set_zero.variable)));
		break;

	case IR_ASSIGN_CONSTANT_OFFSET:
		EMIT("leaq -%d(%%rbp), %%rsi", variable_info[ins.assign_constant_offset.variable].stack_location);
		EMIT("leaq %d(%%rsi), %%rsi", ins.assign_constant_offset.offset);
		EMIT("leaq -%d(%%rbp), %%rdi", variable_info[ins.assign_constant_offset.value].stack_location);
		codegen_memcpy(calculate_size(get_variable_type(ins.assign_constant_offset.value)));
		break;

	case IR_SWITCH_SELECTION: {
		var_id control = ins.switch_selection.condition;
		scalar_to_reg(variable_info[control].stack_location, control, REG_RDI);
		for (int i = 0; i < ins.switch_selection.n; i++) {
			EMIT("cmpl $%d, %%edi", ins.switch_selection.values[i].int_d);
			EMIT("je .LB%d", ins.switch_selection.blocks[i]);
		}
		if (ins.switch_selection.default_) {
			EMIT("jmp .LB%d", ins.switch_selection.default_);
		}
	} break;

	case IR_STACK_ALLOC: {
		// TODO: Also free the stack allocation at the end of blocks.
		//EMIT("pushq %%rsp");
		//EMIT("subq $8, %%rsp");
		scalar_to_reg(variable_info[ins.stack_alloc.length].stack_location, ins.stack_alloc.length, REG_RAX);
		EMIT("subq %%rax, %%rsp");
		reg_to_scalar(REG_RSP, variable_info[ins.stack_alloc.pointer].stack_location, ins.stack_alloc.pointer);
		// Align %rsp to 16 boundary. (Remember stack grows downwards. So rounding down is actually correct.)
		EMIT("andq $-16, %%rsp"); // Round down to nearest 16 by clearing last 4 bits.
	} break;

	case IR_GET_SYMBOL_PTR: {
		EMIT("movq $%s, %%rax", ins.get_symbol_ptr.label);
		reg_to_scalar(REG_RAX, variable_info[ins.get_symbol_ptr.result].stack_location, ins.get_symbol_ptr.result);
	} break;

	case IR_STATIC_VAR:
		codegen_static_var(ins);
		break;

	default:
		printf("%d\n", ins.type);
		NOTIMP();
	}
}

void codegen_function(struct instruction *is,
					  int count) {
	int stack_count = 0;

	struct type *return_type = is->function.signature->children[0];
	int n_args = is->function.signature->n - 1;
	int total_memory_argument_size, current_gp_reg;
	struct classification classifications[n_args];
	struct classification return_classification;
	classify_parameters(n_args, is->function.named_arguments, return_type,
						classifications,
						&return_classification,
						&total_memory_argument_size,
						&current_gp_reg);

	if (return_type != type_simple(ST_VOID) &&
		return_classification.pass_in_memory) {
		stack_count += 8;
	}

	struct reg_save_info reg_save_info;
	reg_save_info.fp_offset = 0;
	reg_save_info.gp_offset = (current_gp_reg) * 8;

	int reg_save = 0;
	for (int i = 1; i < count; i++) {
		switch (is[i].type) {
		case IR_ALLOCA: {
			var_id var = is[i].alloca.variable;

			int size = calculate_size(get_variable_type(var));

			stack_count += size;

			variable_info[var].storage = VAR_STOR_STACK;
			variable_info[var].stack_location = stack_count;
		} break;

		case IR_VA_START:
			reg_save = 1;
			stack_count += 304; // Magic number, size of register save area.
			// According to Figure 3.33 in sysV AMD64 ABI.
			reg_save_info.reg_save_position = stack_count;
			break;

		default:
			break;
		}
	}

	if (is->function.global)
		EMIT(".global %s", is->function.name);
	EMIT("%s:", is->function.name);
	EMIT("pushq %%rbp");
	EMIT("movq %%rsp, %%rbp");
	EMIT("subq $%d, %%rsp", round_up16(stack_count));

	if (return_type != type_simple(ST_VOID) &&
		return_classification.pass_in_memory) {
		EMIT("movq %%rdi, -%d(%%rbp)", 8);
	}

	if (reg_save) {
		EMIT("leaq -%d(%%rbp), %%rax", reg_save_info.reg_save_position);
		EMIT("movq %%rdi, %d(%%rax)", 0);
		EMIT("movq %%rsi, %d(%%rax)", 8);
		EMIT("movq %%rdx, %d(%%rax)", 16);
		EMIT("movq %%rcx, %d(%%rax)", 24);
		EMIT("movq %%r8, %d(%%rax)", 32);
		EMIT("movq %%r9, %d(%%rax)", 40);
		// TODO: xmm0-15
	}

	for (int i = 0; i < n_args; i++) {
		struct classification *classification = classifications + i;
		var_id var = is->function.named_arguments[i];
		int var_size = calculate_size(get_variable_type(var));

		if (classification->pass_in_memory) {
			// TODO: This doesn't have to copy memory at all.
			EMIT("#Passed in stack:");
			codegen_stackcpy(-variable_info[var].stack_location, classification->mem_pos + 16, var_size);
		} else {
			EMIT("#Passed in register:");
			for (int i = 0; i < classification->n_parts; i++) {
				int size = var_size - 8 * i;
				if (size > 8) size = 8;
				if (classification->classes[i] == CLASS_INTEGER) {
					EMIT("mov%c %s, -%d(%%rbp)",
						 size_to_suffix(size),
						 get_reg_name(classification->regs[i], size),
						 variable_info[var].stack_location - 8 * i);
				} else {
					NOTIMP();
				}
			}
		}
	}

	for (int i = 1; i < count; i++) {
		if (is[i].type == IR_ALLOCA)
			continue;
		
		codegen_instruction(is[i], i + 1 < count ? is[i + 1] : (struct instruction) {0}, reg_save_info);
	}

	// Allocate all variables.
	EMIT("xor %%rax, %%rax");
	EMIT("leave");
	EMIT("ret");
}

int stops_function[IR_TYPE_COUNT] = {
	[IR_FUNCTION] = 1
};

int codegen_chunk(void) {
	switch (is[ir_pos].type) {
	case IR_FUNCTION: {
		int end_pos = ir_pos + 1;
		for (; end_pos < ir_count && !stops_function[is[end_pos].type]; end_pos++);
		codegen_function(is + ir_pos, end_pos - ir_pos);
		ir_pos = end_pos;
	} break;
	case IR_STATIC_VAR:
		codegen_static_var(is[ir_pos++]);
		break;
	default:
		ERROR("Not imp %d", is[ir_pos].type);
		NOTIMP();
	}

	return ir_pos < ir_count;
}

void codegen(const char *path) {
	variable_info = malloc(sizeof(*variable_info) * get_n_vars());
	data.out = fopen(path, "w");
	data.current_section = ".text";
	data.local_counter = 0;

	if (!data.out)
		ERROR("Could not open file %s", path);

	struct program *program = get_program();
	ir_pos = 0;
	ir_count = program->size;
	is = program->instructions;

	while (codegen_chunk());

	codegen_rodata();

	fclose(data.out);
}
