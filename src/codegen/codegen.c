#include "codegen.h"
#include "registers.h"
#include "binary_operators.h"

#include <common.h>
#include <arch/builtins.h>
#include <arch/calling.h>
#include <parser/declaration.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

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

void codegen_call(var_id variable, int non_clobbered_register) {
	scalar_to_reg(variable, non_clobbered_register);
	emit("callq *%s", get_reg_name(non_clobbered_register, 8));
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

// TODO: Why is rdi not destination?
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

	case IR_BINARY_NOT:
		scalar_to_reg(ins.int_cast.rhs, REG_RAX);
		emit("notq %%rax");
		reg_to_scalar(REG_RAX, ins.result);
		break;

	case IR_NEGATE_INT:
		scalar_to_reg(ins.int_cast.rhs, REG_RAX);
		emit("negq %%rax");
		reg_to_scalar(REG_RAX, ins.result);
		break;

	case IR_NEGATE_FLOAT:
		scalar_to_reg(ins.int_cast.rhs, REG_RAX);
		if (get_variable_size(ins.result) == 4) {
			emit("movd %%eax, %%xmm1");
			emit("negq %%rax");
			emit("xorps %%xmm0, %%xmm0");
			emit("subss %%xmm1, %%xmm0");
			emit("movd %%xmm0, %%eax");
		} else if (get_variable_size(ins.result) == 8) {
			emit("movq %%rax, %%xmm1");
			emit("xorps %%xmm0, %%xmm0");
			emit("subsd %%xmm1, %%xmm0");
			emit("movq %%xmm0, %%rax");
		} else {
			NOTIMP();
		}
		reg_to_scalar(REG_RAX, ins.result);
		break;

	case IR_CALL:
		codegen_call(ins.call.function, ins.call.non_clobbered_register);
		break;

	case IR_LOAD: {
		scalar_to_reg(ins.load.pointer, REG_RDI);
		emit("leaq -%d(%%rbp), %%rsi", variable_info[ins.result].stack_location);

		codegen_memcpy(get_variable_size(ins.result));
	} break;

	case IR_LOAD_BASE_RELATIVE:
		emit("leaq %d(%%rbp), %%rdi", ins.load_base_relative.offset);
		emit("leaq -%d(%%rbp), %%rsi", variable_info[ins.result].stack_location);

		codegen_memcpy(get_variable_size(ins.result));
	break;

	case IR_STORE: {
		scalar_to_reg(ins.store.pointer, REG_RSI);
		emit("leaq -%d(%%rbp), %%rdi", variable_info[ins.store.value].stack_location);

		codegen_memcpy(get_variable_size(ins.store.value));
	} break;

	case IR_STORE_STACK_RELATIVE: {
		emit("leaq %d(%%rsp), %%rsi", ins.store_stack_relative.offset);
		emit("leaq -%d(%%rbp), %%rdi", variable_info[ins.store_stack_relative.variable].stack_location);

		codegen_memcpy(get_variable_size(ins.store_stack_relative.variable));
	} break;

	case IR_COPY:
		codegen_stackcpy(-variable_info[ins.result].stack_location,
						 -variable_info[ins.copy.source].stack_location,
						 get_variable_size(ins.copy.source));
		break;

	case IR_INT_CAST: {
		scalar_to_reg(ins.int_cast.rhs, REG_RAX);
		int size_rhs = get_variable_size(ins.int_cast.rhs),
			size_result = get_variable_size(ins.result);
		if (size_result > size_rhs && ins.int_cast.sign_extend) {
			if (size_rhs == 1) {
				emit("movsbq %%al, %%rax");
			} else if (size_rhs == 2) {
				emit("movswq %%ax, %%rax");
			} else if (size_rhs == 4) {
				emit("movslq %%eax, %%rax");
			}
		}
		reg_to_scalar(REG_RAX, ins.result);
	} break;

	case IR_BOOL_CAST: {
		scalar_to_reg(ins.bool_cast.rhs, REG_RAX);

		emit("testq %%rax, %%rax");
		emit("setne %%al");

		reg_to_scalar(REG_RAX, ins.result);
	} break;

	case IR_FLOAT_CAST: {
		scalar_to_reg(ins.float_cast.rhs, REG_RAX);
		int size_rhs = get_variable_size(ins.float_cast.rhs),
			size_result = get_variable_size(ins.result);

		if (size_rhs == 4 && size_result == 8) {
			emit("movd %%eax, %%xmm0");
			emit("cvtss2sd %%xmm0, %%xmm0");
			emit("movq %%xmm0, %rax");
		} else if (size_rhs == 8 && size_result == 4) {
			emit("movq %%rax, %%xmm0");
			emit("cvtsd2ss %%xmm0, %%xmm0");
			emit("movd %%xmm0, %%eax");
		} else {
			assert(size_rhs == size_result);
		}

		reg_to_scalar(REG_RAX, ins.result);
	} break;

	case IR_INT_FLOAT_CAST: {
		scalar_to_reg(ins.int_float_cast.rhs, REG_RAX);
		int size_rhs = get_variable_size(ins.int_float_cast.rhs),
			size_result = get_variable_size(ins.result);
		int sign = ins.int_float_cast.sign;
		if (ins.int_float_cast.from_float) {
			// This is not the exact same as gcc and clang in the
			// case of unsigned long. But within the C standard?
			if (size_rhs == 4) {
				emit("movd %%eax, %%xmm0");
				emit("cvttss2si %%xmm0, %%rax");
			} else if (size_rhs == 8) {
				emit("movd %%rax, %%xmm0");
				emit("cvttsd2si %%xmm0, %%rax");
			}
		} else {
			if (sign && size_rhs == 1) {
				emit("movsbl %%al, %%eax");
			} else if (!sign && size_rhs == 1) {
				emit("movzbl %%al, %%eax");
			} else if (sign && size_rhs == 2) {
				emit("movswl %%ax, %%eax");
			} else if (!sign && size_rhs == 2) {
				emit("movzwl %%ax, %%eax");
			}

			if (size_result == 4) {
				emit("cvtsi2ss %%rax, %%xmm0");
				emit("movd %%xmm0, %%eax");
			} else if (size_result == 8) {
				emit("cvtsi2sd %%rax, %%xmm0");
				emit("movq %%xmm0, %%rax");
			} else {
				NOTIMP();
			}
		}
		reg_to_scalar(REG_RAX, ins.result);
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

	case IR_CLEAR_STACK_BUCKET: // no-op
	case IR_ADD_TEMPORARY:
		break;

	case IR_SET_REG:
		if (ins.set_reg.is_ssa) {
			emit("movsd -%d(%%rbp), %%xmm%d", variable_info[ins.set_reg.variable].stack_location,
				 ins.set_reg.register_index);
		} else {
			scalar_to_reg(ins.set_reg.variable, ins.set_reg.register_index);
		}
		break;

	case IR_GET_REG:
		if (ins.get_reg.is_ssa) {
			if (get_variable_size(ins.result) == 4) {
				emit("movss %%xmm%d, -%d(%%rbp)",
					 ins.get_reg.register_index,
					 variable_info[ins.result].stack_location);
			} else {
				emit("movsd %%xmm%d, -%d(%%rbp)",
					 ins.get_reg.register_index,
					 variable_info[ins.result].stack_location);
			}
		} else {
			reg_to_scalar(ins.get_reg.register_index, ins.result);
		}
		break;

	case IR_MODIFY_STACK_POINTER:
		emit("addq $%d, %%rsp", ins.modify_stack_pointer.change);
		break;

	default:
		printf("%d\n", ins.type);
		NOTIMP();
	}
}

void codegen_block(struct block *block, struct function *func, struct reg_save_info reg_save_info) {
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
			emit("jmp .LB%d", block_exit->if_.block_true);
		} else {
			ICE("Invalid argument to if selection");
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
				emit("movq -%d(%%rbp), %%rsi", variable_info[func->ret_ptr].stack_location);
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

	struct reg_save_info reg_save_info;

	for (int i = 0; i < func->var_size; i++) {
		var_id var = func->vars[i];
		if (get_variable_stack_bucket(var))
			continue;

		int size = get_variable_size(var);

		perm_stack_count += size;

		variable_info[var].storage = VAR_STOR_STACK;
		variable_info[var].stack_location = perm_stack_count;
	}

	if (func->uses_va && func->abi == CALL_ABI_SYSV) {
		perm_stack_count += 304; // Magic number, size of register save area.
		// According to Figure 3.33 in sysV AMD64 ABI.
		reg_save_info.fp_offset = 0;
		reg_save_info.reg_save_position = perm_stack_count;
		reg_save_info.gp_offset = func->gp_offset;
		reg_save_info.overflow_position = func->overflow_position;
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

	if (func->uses_va) {
		switch (func->abi) {
		case CALL_ABI_SYSV:
			emit("movq %%rdi, -%d(%%rbp)", reg_save_info.reg_save_position - 0);
			emit("movq %%rsi, -%d(%%rbp)", reg_save_info.reg_save_position - 8);
			emit("movq %%rdx, -%d(%%rbp)", reg_save_info.reg_save_position - 16);
			emit("movq %%rcx, -%d(%%rbp)", reg_save_info.reg_save_position - 24);
			emit("movq %%r8, -%d(%%rbp)", reg_save_info.reg_save_position - 32);
			emit("movq %%r9, -%d(%%rbp)", reg_save_info.reg_save_position - 40);
			// TODO: xmm0-15
			break;

		default:
			NOTIMP();
		}
	}

	for (size_t i = 0; i < vla_info.size; i++) {
		emit("movq $0, -%d(%%rbp)", variable_info[vla_info.slots[i].slot].stack_location);
	}

	for (int i = 0; i < func->size; i++) {
		codegen_block(get_block(func->blocks[i]), func, reg_save_info);
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
		ICE("Could not open file %s", path);

	for (int i = 0; i < ir.size; i++)
		codegen_function(ir.functions + i);

	rodata_codegen();
	data_codegen();

	fclose(data.out);
}
