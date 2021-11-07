#include "codegen.h"
#include "registers.h"
#include "binary_operators.h"
#include "unary_operators.h"
#include "cast_operators.h"

#include <common.h>
#include <arch/builtins.h>
#include <arch/calling.h>
#include <parser/declaration.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

static const int calling_convention[] = {REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9};

struct variable_info *variable_info;

struct {
	FILE *out;
	const char *current_section;
	int local_counter;
} data;

struct codegen_flags codegen_flags = {
	.cmodel = CMODEL_SMALL,
	.debug_stack_size = 0
};

struct vla_info {
	size_t size, cap;
	struct vla_slot {
		var_id slot;
		int dominance;
	} *slots;
} vla_info;

void set_section(const char *section) {
	if (strcmp(section, data.current_section) != 0)
		emit(".section %s", section);
	data.current_section = section;
}

void emit(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	if (!str_contains(fmt, ':'))
		fprintf(data.out, "\t");
	vfprintf(data.out, fmt, args);
	fprintf(data.out, "\n");
	va_end(args);
}

void emit_no_newline(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(data.out, fmt, args);
	va_end(args);
}

void emit_char(char c) {
	fputc(c, data.out);
}

void codegen_binary_operator(enum ir_binary_operator ibo,
							 var_id lhs, var_id rhs, var_id res) {
	scalar_to_reg(lhs, REG_RDI);
	scalar_to_reg(rhs, REG_RSI);

	int size = get_variable_size(lhs);

	if (size != 4 && size != 8) {
		printf("Invalid size %d = %d op %d with %d\n", get_variable_size(res), size, get_variable_size(rhs), ibo);
	}

	assert(size == 4 || size == 8);

	emit("%s", binary_operator_outputs[size == 4 ? 0 : 1][ibo]);

	reg_to_scalar(REG_RAX, res);
}

void codegen_unary_operator(int operator_type, enum operand_type ot, var_id out,
							var_id rhs) {
	scalar_to_reg(rhs, REG_RDI);

	emit("%s", unary_operator_outputs[ot][operator_type]);

	reg_to_scalar(REG_RAX, out);
}

void codegen_simple_cast(var_id in, var_id out,
						 enum simple_type st_in, enum simple_type st_out) {
	scalar_to_reg(in, REG_RDI);

	emit("%s", cast_operator_outputs[st_in][st_out]);

	reg_to_scalar(REG_RAX, out);
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

void classify_parameters(struct type *return_type, int n_args, struct type **types,
						 struct classification *classifications,
						 struct classification *return_classification,
						 int *total_memory_argument_size,
						 int *current_gp_reg,
						 int *current_sse_reg) {
	if (return_type != type_simple(ST_VOID)) {
		classify(return_type, &return_classification->n_parts,
				 return_classification->classes);

		if (return_classification->n_parts == 1 &&
			return_classification->classes[0] == CLASS_MEMORY) {
			return_classification->pass_in_memory = 1;
		} else {
			return_classification->pass_in_memory = 0;
			static const int return_convention[] = {REG_RAX, REG_RDX};
			if (return_classification->n_parts > 2)
				ERROR("Internal compiler error");
			for (int j = 0; j < return_classification->n_parts; j++) {
				if (return_classification->classes[j] == CLASS_INTEGER) {
					return_classification->regs[j] = return_convention[j];
				} else if (return_classification->classes[j] == CLASS_SSE) {
					return_classification->regs[j] = j;
				} else {
					NOTIMP();
				}
			}
		}
	} else {
		return_classification->pass_in_memory = 0;
	}

	*total_memory_argument_size = 0;
	*current_gp_reg = 0;
	*current_sse_reg = 0;
	if (return_classification->pass_in_memory)
		(*current_gp_reg)++;

	for (int i = 0; i < n_args; i++) {
		struct type *type = types[i];
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
					switch (classification->classes[j]) {
					case CLASS_INTEGER:
						classification->regs[j] = calling_convention[(*current_gp_reg)++];
						break;
					case CLASS_SSE:
						classification->regs[j] = (*current_sse_reg)++;
						break;
					default:
						NOTIMP();
					}
				}
				classification->pass_in_memory = 0;
			}
		}
	}
}

void codegen_call(var_id variable, struct type *function_type, struct type **argument_types, int n_args, var_id *args, var_id result) {
	int total_memory_argument_size, current_gp_reg, current_sse_reg;
	struct type *return_type = function_type->children[0];

	scalar_to_reg(variable, REG_RBX);

	struct classification classifications[n_args];
	struct classification return_classification;

	classify_parameters(return_type, n_args, argument_types,
						classifications,
						&return_classification,
						&total_memory_argument_size,
						&current_gp_reg,
						&current_sse_reg);

	if (return_classification.pass_in_memory) {
		emit("leaq -%d(%%rbp), %%rdi", variable_info[result].stack_location);
	}

	int stack_sub = round_up_to_nearest(total_memory_argument_size, 16);
	if (stack_sub)
		emit("subq $%d, %%rsp", stack_sub);

	int current_stack_pos = 0;
	for (int i = 0; i < n_args; i++) {
		struct classification *classification = classifications + i;
		var_id var = args[i];

		if (classification->pass_in_memory) {
			emit("#Passed in stack:");
			int stack_loc = variable_info[var].stack_location;
			for (int i = 0; i < classification->arg_size; i += 8) {
				emit("movq %d(%%rbp), %%rax", -stack_loc + i);
				emit("movq %%rax, %d(%%rsp)", current_stack_pos);
				current_stack_pos += 8;
			}
		} else {
			emit("#Passed in register:");
			for (int i = 0; i < classification->n_parts; i++) {
				if (classification->classes[i] == CLASS_INTEGER) {
					emit("movq -%d(%%rbp), %s",
						 variable_info[var].stack_location - 8 * i,
						 get_reg_name(classification->regs[i], 8));
				} else if (classification->classes[i] == CLASS_SSE) {
					emit("movsd -%d(%%rbp), %%xmm%d",
						 variable_info[var].stack_location - 8 * i,
						 classification->regs[i]);
				} else {
					NOTIMP();
				}
			}
		}
	}

	emit("movl $%d, %%eax", current_sse_reg);
	emit("callq *%%rbx");

	if (stack_sub)
		emit("addq $%d, %%rsp", stack_sub);

	if (!return_classification.pass_in_memory && return_type != type_simple(ST_VOID)) {
		int var_size = get_variable_size(result);
		for (int i = 0; i < return_classification.n_parts; i++) {
			int size = var_size - 8 * i;
			if (size > 8) size = 8;
			if (return_classification.classes[i] == CLASS_INTEGER) {
				emit("mov%c %s, -%d(%%rbp)",
					 size_to_suffix(size),
					 get_reg_name(return_classification.regs[i], size),
					 variable_info[result].stack_location - 8 * i);
			} else if (return_classification.classes[i] == CLASS_SSE) {
				if (get_variable_size(result) == 4) {
					emit("movss %%xmm%d, -%d(%%rbp)",
						 return_classification.regs[i],
						 variable_info[result].stack_location - 8 * i);
				} else {
					emit("movsd %%xmm%d, -%d(%%rbp)",
						 return_classification.regs[i],
						 variable_info[result].stack_location - 8 * i);
				}
			} else {
				NOTIMP();
			}
		}
	}
}

struct reg_save_info {
	int reg_save_position;
	int overflow_position;
	int gp_offset;
	int fp_offset;
};

// Address in rdi.
void codegen_memzero(int len) {
	for (int i = 0; i < len;) {
		if (i + 8 <= len) {
			emit("movq $0, %d(%%rdi)", i);
			i += 8;
		} else if (i + 4 <= len) {
			emit("movl $0, %d(%%rdi)", i);
			i += 4;
		} else if (i + 2 <= len) {
			emit("movw $0, %d(%%rdi)", i);
			i += 2;
		} else if (i + 1 <= len) {
			emit("movb $0, %d(%%rdi)", i);
			i += 1;
		} else
			break;
	}
}

// From rdi to rsi address
void codegen_memcpy(int len) {
	for (int i = 0; i < len;) {
		if (i + 8 <= len) {
			emit("movq %d(%%rdi), %%rax", i);
			emit("movq %%rax, %d(%%rsi)", i);
			i += 8;
		} else if (i + 4 <= len) {
			emit("movl %d(%%rdi), %%eax", i);
			emit("movl %%eax, %d(%%rsi)", i);
			i += 4;
		} else if (i + 2 <= len) {
			emit("movw %d(%%rdi), %%ax", i);
			emit("movw %%ax, %d(%%rsi)", i);
			i += 2;
		} else if (i + 1 <= len) {
			emit("movb %d(%%rdi), %%al", i);
			emit("movb %%al, %d(%%rsi)", i);
			i += 1;
		} else
			break;
	}
}

void codegen_stackcpy(int dest, int source, int len) {
	for (int i = 0; i < len;) {
		if (i + 8 <= len) {
			emit("movq %d(%%rbp), %%rax", source + i);
			emit("movq %%rax, %d(%%rbp)", dest + i);
			i += 8;
		} else if (i + 4 <= len) {
			emit("movl %d(%%rbp), %%eax", source + i);
			emit("movl %%eax, %d(%%rbp)", dest + i);
			i += 4;
		} else if (i + 2 <= len) {
			emit("movw %d(%%rbp), %%ax", source + i);
			emit("movw %%ax, %d(%%rbp)", dest + i);
			i += 2;
		} else if (i + 1 <= len) {
			emit("movb %d(%%rbp), %%al", source + i);
			emit("movb %%al, %d(%%rbp)", dest + i);
			i += 1;
		} else
			break;
	}
}

void codegen_instruction(struct instruction ins, struct reg_save_info reg_save_info) {
	const char *ins_str = dbg_instruction(ins);
	emit("#instruction start \"%s\":", ins_str);
	switch (ins.type) {
	case IR_CONSTANT: {
		struct constant c = ins.constant.constant;
		switch (c.type) {
		case CONSTANT_TYPE: {
			int size = calculate_size(c.data_type);
			if (c.data_type->type == TY_SIMPLE ||
				type_is_pointer(c.data_type)) {
				switch (size) {
				case 1:
					emit("movb $%s, -%d(%%rbp)", constant_to_string(c), variable_info[ins.result].stack_location);
					break;
				case 2:
					emit("movw $%s, -%d(%%rbp)", constant_to_string(c), variable_info[ins.result].stack_location);
					break;
				case 4:
					emit("movl $%s, -%d(%%rbp)", constant_to_string(c), variable_info[ins.result].stack_location);
					break;
				case 8: {
					const char *str = constant_to_string(c);
					emit("movabsq $%s, %%rax", str);
					emit("movq %%rax, -%d(%%rbp)", variable_info[ins.result].stack_location);
				} break;

				case -1:
				case 0:
					// TODO: Is this really right?
					break;

				default: NOTIMP();
				}
			} else {
				uint8_t buffer[size];
				constant_to_buffer(buffer, c, 0, -1);
				for (int i = 0; i < size; i++)
					emit("movb $%d, -%d(%%rbp)", (int)buffer[i], variable_info[ins.result].stack_location - i);
			}
		} break;

		case CONSTANT_LABEL:
			if (codegen_flags.cmodel == CMODEL_LARGE) {
				if (c.label.offset == 0) {
					emit("movabsq $%s, %%rdi", rodata_get_label_string(c.label.label));
				} else
					emit("movabsq $%s+%lld, %%rdi", rodata_get_label_string(c.label.label),
						c.label.offset);
			} else if (codegen_flags.cmodel == CMODEL_SMALL) {
				if (c.label.offset == 0) {
					emit("movq $%s, %%rdi", rodata_get_label_string(c.label.label));
				} else
					emit("movq $%s+%lld, %%rdi", rodata_get_label_string(c.label.label),
						c.label.offset);
			}
			emit("leaq -%d(%%rbp), %%rsi", variable_info[ins.result].stack_location);
			codegen_memcpy(get_variable_size(ins.result));
			break;

		case CONSTANT_LABEL_POINTER:
			if (codegen_flags.cmodel == CMODEL_LARGE) {
				if (c.label.offset == 0)
					emit("movabsq $%s, %%rax", rodata_get_label_string(c.label.label));
				else
					emit("movabsq $%s+%lld, %%rax", rodata_get_label_string(c.label.label),
						c.label.offset);
				emit("movq %%rax, -%d(%%rbp)", variable_info[ins.result].stack_location);
			} else if (codegen_flags.cmodel == CMODEL_SMALL) {
				if (c.label.offset == 0) {
					emit("movq $%s, -%d(%%rbp)", rodata_get_label_string(c.label.label), variable_info[ins.result].stack_location);
				} else {
					emit("movq $%s+%lld, -%d(%%rbp)", rodata_get_label_string(c.label.label), c.label.offset, variable_info[ins.result].stack_location);
				}
			}
			break;

		default:
			NOTIMP();
		}
	} break;

	case IR_BINARY_OPERATOR:
		codegen_binary_operator(ins.binary_operator.type,
								ins.binary_operator.lhs,
								ins.binary_operator.rhs,
								ins.result);
		break;

	case IR_UNARY_OPERATOR:
		codegen_unary_operator(ins.unary_operator.type,
							   ins.unary_operator.operand_type,
							   ins.result,
							   ins.unary_operator.operand);
		break;

	case IR_CALL_VARIABLE:
		codegen_call(ins.call_variable.function,
					 ins.call_variable.function_type,
					 ins.call_variable.argument_types,
					 ins.call_variable.n_args,
					 ins.call_variable.args,
					 ins.result);
		break;

	case IR_LOAD: {
		scalar_to_reg(ins.load.pointer, REG_RDI);
		emit("leaq -%d(%%rbp), %%rsi", variable_info[ins.result].stack_location);

		codegen_memcpy(get_variable_size(ins.result));
	} break;

	case IR_STORE: {
		scalar_to_reg(ins.store.pointer, REG_RSI);
		emit("leaq -%d(%%rbp), %%rdi", variable_info[ins.store.value].stack_location);

		codegen_memcpy(get_variable_size(ins.store.value));
	} break;

	case IR_COPY:
		codegen_stackcpy(-variable_info[ins.result].stack_location,
						 -variable_info[ins.copy.source].stack_location,
						 get_variable_size(ins.copy.source));
		break;

	case IR_CAST: {
		var_id source = ins.cast.rhs,
			dest = ins.result;
		struct type *dest_type = ins.cast.result_type;
		struct type *source_type = ins.cast.rhs_type;

		if (dest_type == type_simple(ST_VOID)) {
			// No op.
		} else if (dest_type->type == TY_SIMPLE &&
				   source_type->type == TY_SIMPLE) {
			codegen_simple_cast(source, dest,
								source_type->simple, dest_type->simple);
		} else {
			// All other casts are just copies.
			scalar_to_reg(source, REG_RAX);
			reg_to_scalar(REG_RAX, dest);
		}
	} break;

	case IR_ADDRESS_OF:
		emit("leaq -%d(%%rbp), %%rax", variable_info[ins.address_of.variable].stack_location);
		reg_to_scalar(REG_RAX, ins.result);
		break;

	case IR_VA_START: {
		int gp_offset_offset = builtin_va_list->fields[0].offset;
		int fp_offset_offset = builtin_va_list->fields[1].offset;
		int overflow_arg_area_offset = builtin_va_list->fields[2].offset;
		int reg_save_area_offset = builtin_va_list->fields[3].offset;
		scalar_to_reg(ins.result, REG_RAX);
		emit("movl $%d, %d(%%rax)", reg_save_info.gp_offset, gp_offset_offset);
		emit("movl $%d, %d(%%rax)", 0, fp_offset_offset);
		emit("leaq %d(%%rbp), %%rdi", reg_save_info.overflow_position);
		emit("movq %%rdi, %d(%%rax)", overflow_arg_area_offset);

		emit("leaq -%d(%%rbp), %%rdi", reg_save_info.reg_save_position);
		emit("movq %%rdi, %d(%%rax)", reg_save_area_offset);
	} break;

	case IR_VA_ARG: {
		struct type *type = ins.va_arg_.type;
		int n_parts;
		enum parameter_class classes[4];
		classify(type, &n_parts, classes);

		static int va_arg_labels = 0;
		int gp_offset_offset = builtin_va_list->fields[0].offset;
		int reg_save_area_offset = builtin_va_list->fields[3].offset;
		int overflow_arg_area_offset = builtin_va_list->fields[2].offset;
		va_arg_labels++;
		// 1. Determine whether type may be passed in the registers. If not go to step 7
		scalar_to_reg(ins.va_arg_.array, REG_RDI);
		if (classes[0] != CLASS_MEMORY) {
			// 2. Compute num_gp to hold the number of general purpose registers needed
			// to pass type and num_fp to hold the number of floating point registers needed.

			int num_gp = 0;
			//int num_fp = 0;
			for (int i = 0; i < n_parts; i++) {
				if (classes[i] == CLASS_INTEGER)
					num_gp++;
				else
					NOTIMP();
			}

			// 3. Verify whether arguments fit into registers. In the case:
			//     l->gp_offset > 48 − num_gp ∗ 8
			// or
			//     l->fp_offset > 304 − num_fp ∗ 16
			// go to step 7.

			emit("movl %d(%%rdi), %%eax", gp_offset_offset);
			emit("cmpl $%d, %%eax", 48 - 8 * num_gp);
			emit("ja .va_arg_stack%d", va_arg_labels);

			// 4. Fetch type from l->reg_save_area with an offset of l->gp_offset
			// and/or l->fp_offset. This may require copying to a temporary loca-
			// tion in case the parameter is passed in different register classes or requires
			// an alignment greater than 8 for general purpose registers and 16 for XMM
			// registers. [Note: Alignment is largely ignored in this implementation]

			// 5. Set:
			// l->gp_offset = l->gp_offset + num_gp ∗ 8
			// l->fp_offset = l->fp_offset + num_fp ∗ 16.
			// 6. Return the fetched type.

			emit("leal %d(%%rax), %%edx", num_gp * 8);
			emit("addq %d(%%rdi), %%rax", reg_save_area_offset);
			emit("movl %%edx, %d(%%rdi)", gp_offset_offset);
			emit("jmp .va_arg_fetch%d", va_arg_labels);
		}
		// 7. Align l->overflow_arg_area upwards to a 16 byte boundary if align-
		//ment needed by type exceeds 8 byte boundary. [This is ignored.]
		emit(".va_arg_stack%d:", va_arg_labels);

		// 8. Fetch type from l->overflow_arg_area.

		emit("movq %d(%%rdi), %%rax", overflow_arg_area_offset);

		// 9. Set l->overflow_arg_area to:
		// l->overflow_arg_area + sizeof(type)
		// 10. Align l->overflow_arg_area upwards to an 8 byte boundary.
		emit("leaq %d(%%rax), %%rdx", round_up_to_nearest(calculate_size(type), 8));
		emit("movq %%rdx, %d(%%rdi)", overflow_arg_area_offset);

		// 11. Return the fetched type.
		emit(".va_arg_fetch%d:", va_arg_labels);

		// Address is now in %%rax.
		emit("movq %%rax, %%rdi");
		emit("leaq -%d(%%rbp), %%rsi", variable_info[ins.result].stack_location);

		codegen_memcpy(calculate_size(type));
	} break;

	case IR_SET_ZERO:
		emit("leaq -%d(%%rbp), %%rdi", variable_info[ins.result].stack_location);
		codegen_memzero(get_variable_size(ins.result));
		break;

	case IR_STACK_ALLOC: {
		struct vla_slot *slot = NULL;
		for (size_t i = 0; i < vla_info.size; i++) {
			if (vla_info.slots[i].dominance == ins.stack_alloc.dominance) {
				slot = vla_info.slots + i;
			} else if (vla_info.slots[i].dominance > ins.stack_alloc.dominance) {
				emit("movq $0, -%d(%%rbp)", variable_info[vla_info.slots[i].slot].stack_location);
			}
		}
		assert(slot);

		static int tmp_label = 0;
		emit("movq -%d(%%rbp), %%rax", variable_info[slot->slot].stack_location);
		emit("cmpq $0, %%rax");
		emit("jne .Lvla%d", tmp_label);
		emit("movq %%rsp, %%rax");
		emit("movq %%rax, -%d(%%rbp)", variable_info[slot->slot].stack_location);
		emit(".Lvla%d:", tmp_label);
		tmp_label++;

		emit("movq %%rax, %%rsp");

		emit("movq %%rsp, -%d(%%rbp)", variable_info[slot->slot].stack_location);
		scalar_to_reg(ins.stack_alloc.length, REG_RAX);
		emit("subq %%rax, %%rsp");
		reg_to_scalar(REG_RSP, ins.result);
		// Align %rsp to 16 boundary. (Remember stack grows downwards. So rounding down is actually correct.)
		emit("andq $-16, %%rsp"); // Round down to nearest 16 by clearing last 4 bits.
	} break;

	case IR_GET_BITS:
		scalar_to_reg(ins.get_bits.field, REG_RAX);
		emit("shl $%d, %%rax", 64 - ins.get_bits.offset - ins.get_bits.length);
		if (ins.get_bits.sign_extend)
			emit("sar $%d, %%rax", 64 - ins.get_bits.length);
		else
			emit("shr $%d, %%rax", 64 - ins.get_bits.length);
		reg_to_scalar(REG_RAX, ins.result);
		break;

	case IR_CLEAR_STACK_BUCKET: // no-op
	case IR_ADD_TEMPORARY:
		break;

	default:
		printf("%d\n", ins.type);
		NOTIMP();
	}
}

void codegen_block(struct block *block, struct reg_save_info reg_save_info) {
	emit(".LB%d:", block->id);

	for (int i = 0; i < block->size; i++) {
		codegen_instruction(block->instructions[i], reg_save_info);
	}

	struct block_exit *block_exit = &block->exit;
	emit("# EXIT IS OF TYPE : %d", block_exit->type);
	switch (block_exit->type) {
	case BLOCK_EXIT_JUMP:
		emit("jmp .LB%d", block_exit->jump);
		break;

	case BLOCK_EXIT_IF: {
		var_id cond = block_exit->if_.condition;
		int size = get_variable_size(cond);
		if (size == 1 || size == 2 || size == 4 || size == 8) {
			scalar_to_reg(cond, REG_RDI);
			const char *reg_name = get_reg_name(REG_RDI, size);
			emit("test%c %s, %s", size_to_suffix(size), reg_name, reg_name);
			emit("je .LB%d", block_exit->if_.block_false);
			/* if (!(next_ins.type == IR_START_BLOCK && */
			/* 	  next_ins.start_block.block == ins.if_selection.block_true)) */
			emit("jmp .LB%d", block_exit->if_.block_true);
		} else {
			ERROR("Invalid argument to if selection");
		}
	} break;

	case BLOCK_EXIT_RETURN: {
		var_id ret = block_exit->return_.value;
		struct type *ret_type = block_exit->return_.type;
		if (ret_type != type_simple(ST_VOID)) {
			enum parameter_class classes[4];
			int n_parts = 0;

			classify(ret_type, &n_parts, classes);

			if (n_parts == 1 && classes[0] == CLASS_MEMORY) {
				emit("movq -%d(%%rbp), %%rsi", 8);
				emit("leaq -%d(%%rbp), %%rdi", variable_info[ret].stack_location);

				codegen_memcpy(calculate_size(ret_type));
			} else if (n_parts == 1 && classes[0] == CLASS_INTEGER) {
				emit("movq -%d(%%rbp), %%rax", variable_info[ret].stack_location);
			} else if (n_parts == 2 && classes[0] == CLASS_INTEGER) {
				emit("movq -%d(%%rbp), %%rax", variable_info[ret].stack_location);
				emit("movq -%d(%%rbp), %%rdx", variable_info[ret].stack_location - 8);
			} else if (n_parts == 1 && classes[0] == CLASS_SSE) {
				if (get_variable_size(ret) == 4) {
					emit("movss -%d(%%rbp), %%xmm0", variable_info[ret].stack_location);
				} else {
					emit("movsd -%d(%%rbp), %%xmm0", variable_info[ret].stack_location);
				}
			} else {
				NOTIMP();
			}
		}
		emit("leave");
		emit("ret");
	} break;

	case BLOCK_EXIT_RETURN_ZERO: {
		emit("xor %%rax, %%rax");
		emit("leave");
		emit("ret");
	} break;

	case BLOCK_EXIT_SWITCH: {
		emit("#SWITCH");
		var_id control = block_exit->switch_.condition;
		scalar_to_reg(control, REG_RDI);
		for (int i = 0; i < block_exit->switch_.labels.size; i++) {
			emit("cmpl $%d, %%edi", block_exit->switch_.labels.labels[i].value.int_d);
			emit("je .LB%d", block_exit->switch_.labels.labels[i].block);
		}
		if (block_exit->switch_.labels.default_) {
			emit("jmp .LB%d", block_exit->switch_.labels.default_);
		}
	} break;

	case BLOCK_EXIT_NONE:
		emit("ud2");
		break;
	}
}

void codegen_function(struct function *func) {
	int temp_stack_count = 0, perm_stack_count = 0;
	int max_temp_stack = 0;

	struct type *return_type = func->signature->children[0];
	int n_args = func->signature->n - 1;
	int total_memory_argument_size, current_gp_reg, current_sse_reg;
	struct classification classifications[n_args];
	struct classification return_classification;

	classify_parameters(return_type, func->signature->n - 1, func->signature->children + 1,
						classifications,
						&return_classification,
						&total_memory_argument_size,
						&current_gp_reg,
						&current_sse_reg);

	if (return_type != type_simple(ST_VOID) &&
		return_classification.pass_in_memory) {
		perm_stack_count += 8;
	}

	struct reg_save_info reg_save_info;
	reg_save_info.fp_offset = 0;
	reg_save_info.gp_offset = (current_gp_reg) * 8;
	reg_save_info.overflow_position = 16;

	for (int i = 0; i < func->var_size; i++) {
		var_id var = func->vars[i];
		if (get_variable_stack_bucket(var))
			continue;

		int size = get_variable_size(var);

		perm_stack_count += size;

		variable_info[var].storage = VAR_STOR_STACK;
		variable_info[var].stack_location = perm_stack_count;
	}

	if (func->uses_va) {
		perm_stack_count += 304; // Magic number, size of register save area.
		// According to Figure 3.33 in sysV AMD64 ABI.
		reg_save_info.reg_save_position = perm_stack_count;
	}

	vla_info.size = 0;

	for (int i = 0; i < func->size; i++) {
		struct block *block = get_block(func->blocks[i]);

		for (int j = 0; j < block->size; j++) {
			struct instruction *ins = block->instructions + j;

			if (ins->type == IR_ADD_TEMPORARY) {
				var_id var = ins->result;
				if (!get_variable_stack_bucket(var))
					continue;

				int size = get_variable_size(var);

				temp_stack_count += size;
				max_temp_stack = MAX(max_temp_stack, temp_stack_count);

				variable_info[var].storage = VAR_STOR_STACK;
				variable_info[var].stack_location = temp_stack_count + perm_stack_count;
			} else if (ins->type == IR_CLEAR_STACK_BUCKET) {
				temp_stack_count = 0;
			} else if (ins->type == IR_STACK_ALLOC) {
				ADD_ELEMENT(vla_info.size, vla_info.cap, vla_info.slots) = (struct vla_slot) {
					.slot = ins->stack_alloc.slot,
					.dominance = ins->stack_alloc.dominance
				};
			}
		}
	}

	if (func->is_global)
		emit(".global %s", func->name);
	emit("%s:", func->name);
	emit("pushq %%rbp");
	emit("movq %%rsp, %%rbp");

	int stack_sub = round_up_to_nearest(perm_stack_count + max_temp_stack, 16);
	if (stack_sub)
		emit("subq $%d, %%rsp", stack_sub);

	if (return_type != type_simple(ST_VOID) &&
		return_classification.pass_in_memory) {
		emit("movq %%rdi, -%d(%%rbp)", 8);
	}

	if (func->uses_va) {
		emit("leaq -%d(%%rbp), %%rax", reg_save_info.reg_save_position);
		emit("movq %%rdi, %d(%%rax)", 0);
		emit("movq %%rsi, %d(%%rax)", 8);
		emit("movq %%rdx, %d(%%rax)", 16);
		emit("movq %%rcx, %d(%%rax)", 24);
		emit("movq %%r8, %d(%%rax)", 32);
		emit("movq %%r9, %d(%%rax)", 40);
		// TODO: xmm0-15
	}

	for (size_t i = 0; i < vla_info.size; i++) {
		emit("movq $0, -%d(%%rbp)", variable_info[vla_info.slots[i].slot].stack_location);
	}

	for (int i = 0; i < n_args; i++) {
		struct classification *classification = classifications + i;
		var_id var = func->named_arguments[i];
		int var_size = get_variable_size(var);

		if (classification->pass_in_memory) {
			// TODO: This doesn't have to copy memory at all.
			emit("#Passed in stack:");
			codegen_stackcpy(-variable_info[var].stack_location, classification->mem_pos + 16, var_size);
		} else {
			emit("#Passed in register:");
			for (int i = 0; i < classification->n_parts; i++) {
				int size = var_size - 8 * i;
				if (size > 8) size = 8;
				if (classification->classes[i] == CLASS_INTEGER) {
					emit("mov%c %s, -%d(%%rbp)",
						 size_to_suffix(size),
						 get_reg_name(classification->regs[i], size),
						 variable_info[var].stack_location - 8 * i);
				} else if (classification->classes[i] == CLASS_SSE) {
					if (get_variable_size(var) == 4) {
						emit("movss %%xmm%d, -%d(%%rbp)",
							 classification->regs[i],
							 variable_info[var].stack_location - 8 * i);
					} else {
						emit("movsd %%xmm%d, -%d(%%rbp)",
							 classification->regs[i],
							 variable_info[var].stack_location - 8 * i);
					}
				} else {
					NOTIMP();
				}
			}
		}
	}

	reg_save_info.overflow_position = total_memory_argument_size + 16;

	for (int i = 0; i < func->size; i++) {
		codegen_block(get_block(func->blocks[i]), reg_save_info);
	}

	int total_stack_usage = max_temp_stack + perm_stack_count;
	if (codegen_flags.debug_stack_size && total_stack_usage >= codegen_flags.debug_stack_min)
		printf("Function %s has stack consumption: %d\n", func->name, total_stack_usage);
}

void codegen(const char *path) {
	variable_info = malloc(sizeof(*variable_info) * get_n_vars());
	data.out = fopen(path, "w");
	data.current_section = ".text";
	data.local_counter = 0;

	if (!data.out)
		ERROR("Could not open file %s", path);

	for (int i = 0; i < ir.size; i++)
		codegen_function(ir.functions + i);

	rodata_codegen();
	data_codegen();

	fclose(data.out);
}
