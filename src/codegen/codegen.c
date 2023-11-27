#include "codegen.h"
#include "registers.h"
#include "binary_operators.h"

#include <common.h>
#include <parser/declaration.h>
#include <abi/abi.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

struct variable_info *variable_info;

struct codegen_flags codegen_flags = {
	.code_model = CODE_MODEL_SMALL,
	.debug_stack_size = 0
};

struct vla_info {
	size_t size, cap;
	struct vla_slot {
		var_id slot;
		int dominance;
	} *slots;
	int alloc_preamble;
} vla_info;

static void codegen_call(var_id variable, int non_clobbered_register) {
	scalar_to_reg(variable, non_clobbered_register);
	asm_ins1("callq", R8S(non_clobbered_register));
}

// Address in rdi.
static void codegen_memzero(int len) {
	for (int i = 0; i < len;) {
		if (i + 8 <= len) {
			asm_ins2("movq", IMM(0), MEM(i, REG_RDI));
			i += 8;
		} else if (i + 4 <= len) {
			asm_ins2("movl", IMM(0), MEM(i, REG_RDI));
			i += 4;
		} else if (i + 2 <= len) {
			asm_ins2("movw", IMM(0), MEM(i, REG_RDI));
			i += 2;
		} else if (i + 1 <= len) {
			asm_ins2("movb", IMM(0), MEM(i, REG_RDI));
			i += 1;
		} else
			break;
	}
}

void codegen_memcpy(int len) {
	for (int i = 0; i < len;) {
		if (i + 8 <= len) {
			asm_ins2("movq", MEM(i, REG_RDI), R8(REG_RAX));
			asm_ins2("movq", R8(REG_RAX), MEM(i, REG_RSI));
			i += 8;
		} else if (i + 4 <= len) {
			asm_ins2("movl", MEM(i, REG_RDI), R4(REG_RAX));
			asm_ins2("movl", R4(REG_RAX), MEM(i, REG_RSI));
			i += 4;
		} else if (i + 2 <= len) {
			asm_ins2("movw", MEM(i, REG_RDI), R2(REG_RAX));
			asm_ins2("movw", R2(REG_RAX), MEM(i, REG_RSI));
			i += 2;
		} else if (i + 1 <= len) {
			asm_ins2("movb", MEM(i, REG_RDI), R1(REG_RAX));
			asm_ins2("movb", R1(REG_RAX), MEM(i, REG_RSI));
			i += 1;
		} else
			break;
	}
}

static void codegen_stackcpy(int dest, int source, int len) {
	for (int i = 0; i < len;) {
		if (i + 8 <= len) {
			asm_ins2("movq", MEM(source + i, REG_RBP), R8(REG_RAX));
			asm_ins2("movq", R8(REG_RAX), MEM(dest + i, REG_RBP));
			i += 8;
		} else if (i + 4 <= len) {
			asm_ins2("movl", MEM(source + i, REG_RBP), R4(REG_RAX));
			asm_ins2("movl", R4(REG_RAX), MEM(dest + i, REG_RBP));
			i += 4;
		} else if (i + 2 <= len) {
			asm_ins2("movw", MEM(source + i, REG_RBP), R2(REG_RAX));
			asm_ins2("movw", R2(REG_RAX), MEM(dest + i, REG_RBP));
			i += 2;
		} else if (i + 1 <= len) {
			asm_ins2("movb", MEM(source + i, REG_RBP), R1(REG_RAX));
			asm_ins2("movb", R1(REG_RAX), MEM(dest + i, REG_RBP));
			i += 1;
		} else
			break;
	}
}

static void codegen_constant_to_rdi(struct constant *constant) {
	switch (constant->type) {
	case CONSTANT_TYPE: {
		int size = calculate_size(constant->data_type);
		if (constant->data_type->type == TY_SIMPLE ||
			type_is_pointer(constant->data_type)) {
			switch (size) {
			case 1:
				asm_ins2("movb", IMM(constant_to_u64(*constant)), MEM(0, REG_RDI));
				break;
			case 2:
				asm_ins2("movw", IMM(constant_to_u64(*constant)), MEM(0, REG_RDI));
				break;
			case 4:
				asm_ins2("movl", IMM(constant_to_u64(*constant)), MEM(0, REG_RDI));
				break;
			case 8:
				asm_ins2("movabsq", IMM(constant_to_u64(*constant)), R8(REG_RAX));
				asm_ins2("movq", R8(REG_RAX), MEM(0, REG_RDI));
				break;

			case 0: break;

			default: NOTIMP();
			}
		} else {
			uint8_t buffer[size];
			constant_to_buffer(buffer, *constant, 0, -1);
			for (int i = 0; i < size; i++)
				asm_ins2("movb", IMM(buffer[i]), MEM(-i, REG_RDI));
		}
	} break;

	case CONSTANT_LABEL:
		if (codegen_flags.code_model == CODE_MODEL_LARGE) {
			asm_ins2("movabsq", IMML(constant->label.label, constant->label.offset), R8(REG_RDI));
		} else if (codegen_flags.code_model == CODE_MODEL_SMALL) {
			asm_ins2("movq", IMML(constant->label.label, constant->label.offset), R8(REG_RDI));
		}
		asm_ins2("leaq", MEM(0, REG_RDI), R8(REG_RSI));
		codegen_memcpy(calculate_size(constant->data_type));
		break;

	case CONSTANT_LABEL_POINTER:
		if (codegen_flags.code_model == CODE_MODEL_LARGE) {
			asm_ins2("movabsq", IMML(constant->label.label, constant->label.offset), R8(REG_RAX));
			asm_ins2("movq", R8(REG_RAX), MEM(0, REG_RDI));
		} else if (codegen_flags.code_model == CODE_MODEL_SMALL) {
			asm_ins2("movq", IMML(constant->label.label, constant->label.offset), MEM(0, REG_RDI));
		}
		break;

	default:
		NOTIMP();
	}
}

static void codegen_instruction(struct instruction ins, struct function *func) {
	const char *ins_str = dbg_instruction(ins);
	asm_comment("instruction start \"%s\":", ins_str);

	struct asm_instruction (*asm_entry)[2][5] = codegen_asm_table[ins.type];
	if (asm_entry) {
		scalar_to_reg(ins.operands[1], REG_RAX);
		scalar_to_reg(ins.operands[2], REG_RCX);

		const int size = get_variable_size(ins.operands[1]);
		assert(size == 4 || size == 8);

		struct asm_instruction *asms = (*asm_entry)[size == 8];

		for (int i = 0; i < 5 && asms[i].mnemonic; i++)
			asm_ins(&asms[i]);

		reg_to_scalar(REG_RAX, ins.operands[0]);
		return;
	}

	switch (ins.type) {
	case IR_CONSTANT:
		asm_ins2("leaq", MEM(-variable_info[ins.operands[0]].stack_location, REG_RBP), R8(REG_RDI));
		codegen_constant_to_rdi(&ins.constant.constant);
		break;

	case IR_CONSTANT_ADDRESS:
		scalar_to_reg(ins.operands[0], REG_RDI);
		codegen_constant_to_rdi(&ins.constant.constant);
		break;

	case IR_BINARY_NOT:
		scalar_to_reg(ins.operands[1], REG_RAX);
		asm_ins1("notq", R8(REG_RAX));
		reg_to_scalar(REG_RAX, ins.operands[0]);
		break;

	case IR_NEGATE_INT:
		scalar_to_reg(ins.operands[1], REG_RAX);
		asm_ins1("negq", R8(REG_RAX));
		reg_to_scalar(REG_RAX, ins.operands[0]);
		break;

	case IR_NEGATE_FLOAT:
		scalar_to_reg(ins.operands[1], REG_RAX);
		if (get_variable_size(ins.operands[0]) == 4) {
			asm_ins2("leal", MEM(-2147483648, REG_RAX), R4(REG_RAX));
		} else if (get_variable_size(ins.operands[0]) == 8) {
			asm_ins2("btcq", IMM(63), R8(REG_RAX));
		} else {
			NOTIMP();
		}
		reg_to_scalar(REG_RAX, ins.operands[0]);
		break;

	case IR_CALL:
		codegen_call(ins.operands[0], ins.call.non_clobbered_register);
		break;

	case IR_LOAD:
		scalar_to_reg(ins.operands[1], REG_RDI);
		asm_ins2("leaq", MEM(-variable_info[ins.operands[0]].stack_location, REG_RBP), R8(REG_RSI));

		codegen_memcpy(get_variable_size(ins.operands[0]));
		break;

	case IR_LOAD_PART:
		asm_ins2("leaq", MEM(-variable_info[ins.operands[1]].stack_location + ins.load_part.offset, REG_RBP), R8(REG_RDI));
		asm_ins2("leaq", MEM(-variable_info[ins.operands[0]].stack_location, REG_RBP), R8(REG_RSI));

		codegen_memcpy(get_variable_size(ins.operands[0]));
		break;

	case IR_LOAD_PART_ADDRESS:
		scalar_to_reg(ins.operands[1], REG_RDI);
		asm_ins2("leaq", MEM(ins.load_part.offset, REG_RDI), R8(REG_RDI));
		asm_ins2("leaq", MEM(-variable_info[ins.operands[0]].stack_location, REG_RBP), R8(REG_RSI));

		codegen_memcpy(get_variable_size(ins.operands[0]));
		break;

	case IR_LOAD_BASE_RELATIVE:
		asm_ins2("leaq", MEM(ins.load_base_relative.offset, REG_RBP), R8(REG_RDI));
		asm_ins2("leaq", MEM(-variable_info[ins.operands[0]].stack_location, REG_RBP), R8(REG_RSI));

		codegen_memcpy(get_variable_size(ins.operands[0]));
		break;

	case IR_LOAD_BASE_RELATIVE_ADDRESS:
		asm_ins2("leaq", MEM(ins.load_base_relative_address.offset, REG_RBP), R8(REG_RDI));
		scalar_to_reg(ins.operands[0], REG_RSI);

		codegen_memcpy(ins.load_base_relative_address.size);
		break;

	case IR_STORE:
		scalar_to_reg(ins.operands[1], REG_RSI);
		asm_ins2("leaq", MEM(-variable_info[ins.operands[0]].stack_location, REG_RBP), R8(REG_RDI));

		codegen_memcpy(get_variable_size(ins.operands[0]));
		break;

	case IR_STORE_PART:
		asm_ins2("leaq", MEM(-variable_info[ins.operands[0]].stack_location + ins.store_part.offset, REG_RBP), R8(REG_RSI));
		asm_ins2("leaq", MEM(-variable_info[ins.operands[1]].stack_location, REG_RBP), R8(REG_RDI));

		assert(get_variable_size(ins.operands[1]) + ins.store_part.offset <= get_variable_size(ins.operands[0]));

		codegen_memcpy(get_variable_size(ins.operands[1]));
		break;

	case IR_STORE_PART_ADDRESS:
		scalar_to_reg(ins.operands[0], REG_RSI);
		asm_ins2("leaq", MEM(+ins.store_part.offset, REG_RSI), R8(REG_RSI));
		asm_ins2("leaq", MEM(-variable_info[ins.operands[1]].stack_location, REG_RBP), R8(REG_RDI));

		codegen_memcpy(get_variable_size(ins.operands[1]));
		break;

	case IR_STORE_STACK_RELATIVE: {
		asm_ins2("leaq", MEM(ins.store_stack_relative.offset, REG_RSP), R8(REG_RSI));
		asm_ins2("leaq", MEM(-variable_info[ins.operands[0]].stack_location, REG_RBP), R8(REG_RDI));

		codegen_memcpy(get_variable_size(ins.operands[0]));
	} break;

	case IR_STORE_STACK_RELATIVE_ADDRESS: {
		asm_ins2("leaq", MEM(ins.store_stack_relative_address.offset, REG_RSP), R8(REG_RSI));
		scalar_to_reg(ins.operands[0], REG_RDI);

		codegen_memcpy(ins.store_stack_relative_address.size);
	} break;

	case IR_COPY:
		codegen_stackcpy(-variable_info[ins.operands[0]].stack_location,
						 -variable_info[ins.operands[1]].stack_location,
						 get_variable_size(ins.operands[1]));
		break;

	case IR_INT_CAST_ZERO:
		scalar_to_reg(ins.operands[1], REG_RAX);
		reg_to_scalar(REG_RAX, ins.operands[0]);
		break;

	case IR_INT_CAST_SIGN: {
		scalar_to_reg(ins.operands[1], REG_RAX);
		int size_rhs = get_variable_size(ins.operands[1]),
			size_result = get_variable_size(ins.operands[0]);
		if (size_result > size_rhs) {
			if (size_rhs == 1) {
				asm_ins2("movsbq", R1(REG_RAX), R8(REG_RAX));
			} else if (size_rhs == 2) {
				asm_ins2("movswq", R2(REG_RAX), R8(REG_RAX));
			} else if (size_rhs == 4) {
				asm_ins2("movslq", R4(REG_RAX), R8(REG_RAX));
			}
		}
		reg_to_scalar(REG_RAX, ins.operands[0]);
	} break;

	case IR_BOOL_CAST: {
		scalar_to_reg(ins.operands[1], REG_RAX);

		asm_ins2("testq", R8(REG_RAX), R8(REG_RAX));
		asm_ins1("setne", R1(REG_RAX));

		reg_to_scalar(REG_RAX, ins.operands[0]);
	} break;

	case IR_FLOAT_CAST: {
		scalar_to_reg(ins.operands[1], REG_RAX);
		int size_rhs = get_variable_size(ins.operands[1]),
			size_result = get_variable_size(ins.operands[0]);

		if (size_rhs == 4 && size_result == 8) {
			asm_ins2("movd", R4(REG_RAX), XMM(0));
			asm_ins2("cvtss2sd", XMM(0), XMM(0));
			asm_ins2("movq", XMM(0), R8(REG_RAX));
		} else if (size_rhs == 8 && size_result == 4) {
			asm_ins2("movq", R8(REG_RAX), XMM(0));
			asm_ins2("cvtsd2ss", XMM(0), XMM(0));
			asm_ins2("movd", XMM(0), R4(REG_RAX));
		} else {
			assert(size_rhs == size_result);
		}

		reg_to_scalar(REG_RAX, ins.operands[0]);
	} break;

	case IR_INT_FLOAT_CAST: {
		scalar_to_reg(ins.operands[1], REG_RAX);
		int size_rhs = get_variable_size(ins.operands[1]),
			size_result = get_variable_size(ins.operands[0]);
		if (size_rhs == 1) {
			asm_ins2("movsbl", R1(REG_RAX), R4(REG_RAX));
		} else if (size_rhs == 2) {
			asm_ins2("movswl", R2(REG_RAX), R4(REG_RAX));
		} else if (size_rhs == 4) {
			asm_ins2("movslq", R4(REG_RAX), R8(REG_RAX));
		}

		if (size_result == 4) {
			asm_ins2("cvtsi2ss", R8(REG_RAX), XMM(0));
			asm_ins2("movd", XMM(0), R4(REG_RAX));
		} else if (size_result == 8) {
			asm_ins2("cvtsi2sd", R8(REG_RAX), XMM(0));
			asm_ins2("movq", XMM(0), R8(REG_RAX));
		} else {
			NOTIMP();
		}
		reg_to_scalar(REG_RAX, ins.operands[0]);
	} break;

	case IR_UINT_FLOAT_CAST: {
		scalar_to_reg(ins.operands[1], REG_RAX);
		int size_rhs = get_variable_size(ins.operands[1]),
			size_result = get_variable_size(ins.operands[0]);
		if (size_rhs == 1) {
			asm_ins2("movzbl", R1(REG_RAX), R4(REG_RAX));
		} else if (size_rhs == 2) {
			asm_ins2("movzwl", R2(REG_RAX), R4(REG_RAX));
		}

		if (size_result == 4) {
			asm_ins2("cvtsi2ss", R8(REG_RAX), XMM(0));
			asm_ins2("movd", XMM(0), R4(REG_RAX));
		} else if (size_result == 8) {
			asm_ins2("cvtsi2sd", R8(REG_RAX), XMM(0));
			asm_ins2("movq", XMM(0), R8(REG_RAX));
		} else {
			NOTIMP();
		}
		reg_to_scalar(REG_RAX, ins.operands[0]);
	} break;

	case IR_FLOAT_INT_CAST: {
		scalar_to_reg(ins.operands[1], REG_RAX);
		int size_rhs = get_variable_size(ins.operands[1]);
		// This is not the exact same as gcc and clang in the
		// case of unsigned long. But within the C standard?
		if (size_rhs == 4) {
			asm_ins2("movd", R4(REG_RAX), XMM(0));
			asm_ins2("cvttss2si", XMM(0), R8(REG_RAX));
		} else if (size_rhs == 8) {
			asm_ins2("movd", R8(REG_RAX), XMM(0));
			asm_ins2("cvttsd2si", XMM(0), R8(REG_RAX));
		}
		reg_to_scalar(REG_RAX, ins.operands[0]);
	} break;

	case IR_VA_START:
		abi_emit_va_start(ins.operands[0], func);
		break;

	case IR_VA_ARG:
		abi_emit_va_arg(ins.operands[0], ins.va_arg_.array, ins.va_arg_.type);
		break;

	case IR_SET_ZERO_PTR:
		scalar_to_reg(ins.operands[0], REG_RDI);
		codegen_memzero(ins.set_zero_ptr.size);
		break;

	case IR_STACK_ALLOC: {
		struct vla_slot *slot = NULL;
		for (size_t i = 0; i < vla_info.size; i++) {
			if (vla_info.slots[i].dominance == ins.stack_alloc.dominance) {
				slot = vla_info.slots + i;
			} else if (vla_info.slots[i].dominance > ins.stack_alloc.dominance) {
				asm_ins2("movq", IMM(0), MEM(-variable_info[vla_info.slots[i].slot].stack_location, REG_RBP));
			}
		}
		assert(slot);

		label_id tmp_label = register_label();
		asm_ins2("movq", MEM(-variable_info[slot->slot].stack_location, REG_RBP), R8(REG_RAX));
		asm_ins2("cmpq", IMM(0), R8(REG_RAX));
		asm_ins1("jne", IMML_ABS(tmp_label, 0));
		asm_ins2("movq", R8(REG_RSP), R8(REG_RAX));
		asm_ins2("movq", R8(REG_RAX), MEM(-variable_info[slot->slot].stack_location, REG_RBP));
		asm_label(0, tmp_label);
		tmp_label++;

		asm_ins2("movq", R8(REG_RAX), R8(REG_RSP));

		asm_ins2("movq", R8(REG_RSP), MEM(-variable_info[slot->slot].stack_location, REG_RBP));
		scalar_to_reg(ins.operands[1], REG_RAX);
		asm_ins2("subq", R8(REG_RAX), R8(REG_RSP));
		reg_to_scalar(REG_RSP, ins.operands[0]);
		// Align %rsp to 16 boundary. (Remember stack grows downwards. So rounding down is actually correct.)
		asm_ins2("andq", IMM(-16), R8(REG_RSP));
	} break;

	case IR_SET_REG:
		if (ins.set_reg.is_sse) {
			asm_ins2("movsd", MEM(-variable_info[ins.operands[0]].stack_location, REG_RBP),
					 XMM(ins.set_reg.register_index));
		} else {
			scalar_to_reg(ins.operands[0], ins.set_reg.register_index);
		}
		break;

	case IR_GET_REG:
		if (ins.get_reg.is_sse) {
			if (get_variable_size(ins.operands[0]) == 4) {
				asm_ins2("movss", XMM(ins.get_reg.register_index),
						 MEM(-variable_info[ins.operands[0]].stack_location, REG_RBP));
			} else {
				asm_ins2("movsd", XMM(ins.get_reg.register_index),
						 MEM(-variable_info[ins.operands[0]].stack_location, REG_RBP));
			}
		} else {
			reg_to_scalar(ins.get_reg.register_index, ins.operands[0]);
		}
		break;

	case IR_MODIFY_STACK_POINTER:
		asm_ins2("addq", IMM(ins.modify_stack_pointer.change), R8(REG_RSP));
		break;

	case IR_ALLOC:
		assert(ins.alloc.stack_location != -1);
		asm_ins2("leaq", MEM(-ins.alloc.stack_location, REG_RBP), R8(REG_RSI));
		reg_to_scalar(REG_RSI, ins.operands[0]);
		break;

	case IR_COPY_MEMORY:
		scalar_to_reg(ins.operands[0], REG_RSI);
		scalar_to_reg(ins.operands[1], REG_RDI);

		codegen_memcpy(ins.copy_memory.size);
		break;

	default:
		printf("%d\n", ins.type);
		NOTIMP();
	}
}

static void codegen_block(struct block *block, struct function *func) {
	asm_label(0, block->label);

	for (int i = 0; i < block->size; i++)
		codegen_instruction(block->instructions[i], func);

	struct block_exit *block_exit = &block->exit;
	asm_comment("EXIT IS OF TYPE : %d", block_exit->type);
	switch (block_exit->type) {
	case BLOCK_EXIT_JUMP:
		asm_ins1("jmp", IMML_ABS(get_block(block_exit->jump)->label, 0));
		break;

	case BLOCK_EXIT_IF: {
		var_id cond = block_exit->if_.condition;
		int size = get_variable_size(cond);
		scalar_to_reg(cond, REG_RDI);
		switch (size) {
		case 1: asm_ins2("testb", R1(REG_RDI), R1(REG_RDI)); break;
		case 2: asm_ins2("testw", R2(REG_RDI), R2(REG_RDI)); break;
		case 4: asm_ins2("testl", R4(REG_RDI), R4(REG_RDI)); break;
		case 8: asm_ins2("testq", R8(REG_RDI), R8(REG_RDI)); break;
		default: ICE("Invalid argument to if selection.");
		}
		asm_ins1("je", IMML_ABS(get_block(block_exit->if_.block_false)->label, 0));
		asm_ins1("jmp", IMML_ABS(get_block(block_exit->if_.block_true)->label, 0));
	} break;

	case BLOCK_EXIT_RETURN:
		asm_ins0("leave");
		asm_ins0("ret");
		break;

	case BLOCK_EXIT_RETURN_ZERO:
		asm_ins2("xorq", R8(REG_RAX), R8(REG_RAX));
		asm_ins0("leave");
		asm_ins0("ret");
		break;

	case BLOCK_EXIT_SWITCH: {
		asm_comment("SWITCH");
		var_id control = block_exit->switch_.condition;
		scalar_to_reg(control, REG_RDI);
		for (int i = 0; i < block_exit->switch_.labels.size; i++) {
			asm_ins2("cmpl", IMM(block_exit->switch_.labels.labels[i].value.int_d), R4(REG_RDI));
			asm_ins1("je", IMML_ABS(get_block(block_exit->switch_.labels.labels[i].block)->label, 0));
		}
		if (block_exit->switch_.labels.default_) {
			asm_ins1("jmp", IMML_ABS(get_block(block_exit->switch_.labels.default_)->label, 0));
		}
	} break;

	case BLOCK_EXIT_NONE:
		asm_ins0("ud2");
		break;
	}
}

static void codegen_function(struct function *func) {
	int perm_stack_count = 0;
	int max_temp_stack = 0;

	for (int i = 0; i < func->var_size; i++) {
		var_id var = func->vars[i];

		int size = get_variable_size(var);

		perm_stack_count += size;

		variable_info[var].storage = VAR_STOR_STACK;
		variable_info[var].stack_location = perm_stack_count;
	}

	vla_info.size = 0;

	for (int i = 0; i < func->size; i++) {
		struct block *block = get_block(func->blocks[i]);

		for (int j = 0; j < block->size; j++) {
			struct instruction *ins = block->instructions + j;
			if (ins->type == IR_ALLOC) {
				perm_stack_count += ins->alloc.size;
				ins->alloc.stack_location = perm_stack_count;

				if (ins->alloc.save_to_preamble)
					vla_info.alloc_preamble = perm_stack_count;
			}
		}
	}

	for (int i = 0; i < func->size; i++) {
		struct block *block = get_block(func->blocks[i]);

		for (int j = 0; j < block->size; j++) {
			struct instruction *ins = block->instructions + j;

			if (ins->type == IR_STACK_ALLOC) {
				ADD_ELEMENT(vla_info.size, vla_info.cap, vla_info.slots) = (struct vla_slot) {
					.slot = ins->operands[2],
					.dominance = ins->stack_alloc.dominance
				};
			}
		}
	}

	label_id func_label = register_label_name(sv_from_str((char *)func->name));
	asm_label(func->is_global, func_label);
	asm_ins1("pushq", R8(REG_RBP));
	asm_ins2("movq", R8(REG_RSP), R8(REG_RBP));

	int stack_sub = round_up_to_nearest(perm_stack_count + max_temp_stack, 16);
	if (stack_sub)
		asm_ins2("subq", IMM(stack_sub), R8(REG_RSP));

	for (size_t i = 0; i < vla_info.size; i++)
		asm_ins2("movq", IMM(0), MEM(-variable_info[vla_info.slots[i].slot].stack_location, REG_RBP));

	abi_emit_function_preamble(func);

	for (int i = 0; i < func->size; i++)
		codegen_block(get_block(func->blocks[i]), func);

	int total_stack_usage = max_temp_stack + perm_stack_count;
	if (codegen_flags.debug_stack_size && total_stack_usage >= codegen_flags.debug_stack_min)
		printf("Function %s has stack consumption: %d\n", func->name, total_stack_usage);
}

int codegen_get_alloc_preamble(void) {
	return vla_info.alloc_preamble;
}

void codegen(void) {
	variable_info = cc_malloc(sizeof(*variable_info) * get_n_vars());

	for (int i = 0; i < ir.size; i++)
		codegen_function(ir.functions + i);

	rodata_codegen();
	data_codegen();

	asm_finish();
}
