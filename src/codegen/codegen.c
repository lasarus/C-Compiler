#include "codegen.h"
#include "assembler/assembler.h"
#include "ir/ir.h"
#include "registers.h"
#include "binary_operators.h"

#include <common.h>
#include <parser/declaration.h>
#include <abi/abi.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

struct codegen_flags codegen_flags = {
	.code_model = CODE_MODEL_SMALL,
	.debug_stack_size = 0
};

struct vla_info {
	int vla_slot_buffer_offset;
	int count;
	int alloc_preamble;
} vla_info;

struct rbp_save_info {
	int has_saved_rsp;
	int offset;
} rbp_save_info;

static void codegen_call(struct node *variable, int non_clobbered_register) {
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

static void codegen_set_reg_chain(struct node *start) {
	while (start) {
		if (start->set_reg.is_sse) {
			asm_ins2("movsd", MEM(-start->arguments[0]->cg_info.stack_location, REG_RBP),
					 XMM(start->set_reg.register_index));
		} else {
			scalar_to_reg(start->arguments[0], start->set_reg.register_index);
		}

		start = start->arguments[1];
	}
}

static int codegen_call_stack_chain(struct node *start) {
	int change = 0;
	struct node *allocation = start;
	while (allocation->type != IR_ALLOCATE_CALL_STACK) {
		allocation = allocation->arguments[1];
	}

	change = allocation->allocate_call_stack.change;
	asm_ins2("addq", IMM(change), R8(REG_RSP));

	struct node *ins = start;
	while (ins->type != IR_ALLOCATE_CALL_STACK) {
		if (ins->type == IR_STORE_STACK_RELATIVE) {
			asm_ins2("leaq", MEM(ins->store_stack_relative.offset, REG_RSP), R8(REG_RSI));
			asm_ins2("leaq", MEM(-ins->arguments[0]->cg_info.stack_location, REG_RBP), R8(REG_RDI));

			codegen_memcpy(ins->arguments[0]->size);
		} else if (ins->type == IR_STORE_STACK_RELATIVE_ADDRESS) {
			asm_ins2("leaq", MEM(ins->store_stack_relative_address.offset, REG_RSP), R8(REG_RSI));
			scalar_to_reg(ins->arguments[0], REG_RDI);

			codegen_memcpy(ins->store_stack_relative_address.size);
		}

		ins = ins->arguments[1];
	}

	return change;
}

static void codegen_get_reg_uses(struct node *reg_source) {
	for (unsigned i = 0; i < reg_source->use_size; i++) {
		struct node *ins = reg_source->uses[i];

		if (ins->type != IR_GET_REG || ins->block == NULL)
			continue;

		if (ins->get_reg.is_sse) {
			if (ins->size == 4) {
				asm_ins2("movss", XMM(ins->get_reg.register_index),
						 MEM(-ins->cg_info.stack_location, REG_RBP));
			} else {
				asm_ins2("movsd", XMM(ins->get_reg.register_index),
						 MEM(-ins->cg_info.stack_location, REG_RBP));
			}
		} else {
			reg_to_scalar(ins->get_reg.register_index, ins);
		}
	}
}

static void codegen_instruction(struct node *ins, struct node *func) {
	const char *ins_str = dbg_instruction(ins);
	asm_comment("instruction start \"%s\":", ins_str);

	struct asm_instruction (*asm_entry)[2][5] = codegen_asm_table[ins->type];
	if (asm_entry) {
		struct node *output = ins;
		if (ins->type == IR_DIV ||
			ins->type == IR_IDIV ||
			ins->type == IR_MOD ||
			ins->type == IR_IMOD)
			output = ins->projects[0];

		scalar_to_reg(ins->arguments[0], REG_RAX);
		scalar_to_reg(ins->arguments[1], REG_RCX);

		const int size = ins->arguments[0]->size;
		assert(size == 4 || size == 8);

		struct asm_instruction *asms = (*asm_entry)[size == 8];

		for (int i = 0; i < 5 && asms[i].mnemonic; i++)
			asm_ins(&asms[i]);

		reg_to_scalar(REG_RAX, output);
		return;
	}

	switch (ins->type) {
	case IR_CONSTANT:
		asm_ins2("leaq", MEM(-ins->cg_info.stack_location, REG_RBP), R8(REG_RDI));
		codegen_constant_to_rdi(&ins->constant.constant);
		break;

	case IR_BINARY_NOT:
		scalar_to_reg(ins->arguments[0], REG_RAX);
		asm_ins1("notq", R8(REG_RAX));
		reg_to_scalar(REG_RAX, ins);
		break;

	case IR_NEGATE_INT:
		scalar_to_reg(ins->arguments[0], REG_RAX);
		asm_ins1("negq", R8(REG_RAX));
		reg_to_scalar(REG_RAX, ins);
		break;

	case IR_NEGATE_FLOAT:
		scalar_to_reg(ins->arguments[0], REG_RAX);
		if (ins->size == 4) {
			asm_ins2("leal", MEM(-2147483648, REG_RAX), R4(REG_RAX));
		} else if (ins->size == 8) {
			asm_ins2("btcq", IMM(63), R8(REG_RAX));
		} else {
			NOTIMP();
		}
		reg_to_scalar(REG_RAX, ins);
		break;

	case IR_CALL: {
		int change = codegen_call_stack_chain(ins->arguments[3]);
		codegen_set_reg_chain(ins->arguments[2]);
		codegen_call(ins->arguments[0], ins->call.non_clobbered_register);
		asm_ins2("addq", IMM(change), R8(REG_RSP));
		struct node *reg_source = ins->projects[1];
		if (reg_source)
			codegen_get_reg_uses(reg_source);
	} break;

	case IR_LOAD: {
		struct node *value = ins;
		scalar_to_reg(ins->arguments[0], REG_RDI);
		asm_ins2("leaq", MEM(-value->cg_info.stack_location, REG_RBP), R8(REG_RSI));

		codegen_memcpy(value->size);
	} break;

	case IR_LOAD_VOLATILE: {
		struct node *value = ins->projects[1];
		scalar_to_reg(ins->arguments[0], REG_RDI);
		asm_ins2("leaq", MEM(-value->cg_info.stack_location, REG_RBP), R8(REG_RSI));

		codegen_memcpy(value->size);
	} break;

	case IR_LOAD_PART_ADDRESS: {
		struct node *value = ins->projects[1];
		scalar_to_reg(ins->arguments[0], REG_RDI);
		asm_ins2("leaq", MEM(ins->load_part.offset, REG_RDI), R8(REG_RDI));
		asm_ins2("leaq", MEM(-value->cg_info.stack_location, REG_RBP), R8(REG_RSI));

		codegen_memcpy(value->size);
	} break;

	case IR_STORE:
		scalar_to_reg(ins->arguments[0], REG_RSI);
		asm_ins2("leaq", MEM(-ins->arguments[1]->cg_info.stack_location, REG_RBP), R8(REG_RDI));

		codegen_memcpy(ins->arguments[1]->size);
		break;

	case IR_STORE_PART_ADDRESS:
		scalar_to_reg(ins->arguments[0], REG_RSI);
		asm_ins2("leaq", MEM(+ins->store_part.offset, REG_RSI), R8(REG_RSI));
		asm_ins2("leaq", MEM(-ins->arguments[1]->cg_info.stack_location, REG_RBP), R8(REG_RDI));

		codegen_memcpy(ins->arguments[1]->size);
		break;

	case IR_INT_CAST_ZERO:
		scalar_to_reg(ins->arguments[0], REG_RAX);
		reg_to_scalar(REG_RAX, ins);
		break;

	case IR_INT_CAST_SIGN: {
		scalar_to_reg(ins->arguments[0], REG_RAX);
		int size_rhs = ins->arguments[0]->size,
			size_result = ins->size;
		if (size_result > size_rhs) {
			if (size_rhs == 1) {
				asm_ins2("movsbq", R1(REG_RAX), R8(REG_RAX));
			} else if (size_rhs == 2) {
				asm_ins2("movswq", R2(REG_RAX), R8(REG_RAX));
			} else if (size_rhs == 4) {
				asm_ins2("movslq", R4(REG_RAX), R8(REG_RAX));
			}
		}
		reg_to_scalar(REG_RAX, ins);
	} break;

	case IR_BOOL_CAST: {
		scalar_to_reg(ins->arguments[0], REG_RAX);

		asm_ins2("testq", R8(REG_RAX), R8(REG_RAX));
		asm_ins1("setne", R1(REG_RAX));

		reg_to_scalar(REG_RAX, ins);
	} break;

	case IR_FLOAT_CAST: {
		scalar_to_reg(ins->arguments[0], REG_RAX);
		int size_rhs = ins->arguments[0]->size,
			size_result = ins->size;

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

		reg_to_scalar(REG_RAX, ins);
	} break;

	case IR_INT_FLOAT_CAST: {
		scalar_to_reg(ins->arguments[0], REG_RAX);
		int size_rhs = ins->arguments[0]->size,
			size_result = ins->size;
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
		reg_to_scalar(REG_RAX, ins);
	} break;

	case IR_UINT_FLOAT_CAST: {
		scalar_to_reg(ins->arguments[0], REG_RAX);
		int size_rhs = ins->arguments[0]->size,
			size_result = ins->size;
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
		reg_to_scalar(REG_RAX, ins);
	} break;

	case IR_FLOAT_INT_CAST: {
		scalar_to_reg(ins->arguments[0], REG_RAX);
		int size_rhs = ins->arguments[0]->size;
		// This is not the exact same as gcc and clang in the
		// case of unsigned long. But within the C standard?
		if (size_rhs == 4) {
			asm_ins2("movd", R4(REG_RAX), XMM(0));
			asm_ins2("cvttss2si", XMM(0), R8(REG_RAX));
		} else if (size_rhs == 8) {
			asm_ins2("movd", R8(REG_RAX), XMM(0));
			asm_ins2("cvttsd2si", XMM(0), R8(REG_RAX));
		}
		reg_to_scalar(REG_RAX, ins);
	} break;

	case IR_VA_START:
		abi_emit_va_start(ins->arguments[0], func);
		break;

	case IR_VA_ARG:
		abi_emit_va_arg(ins->arguments[0], ins->arguments[1], ins->va_arg_.type);
		break;

	case IR_SET_ZERO_PTR:
		scalar_to_reg(ins->arguments[0], REG_RDI);
		codegen_memzero(ins->set_zero_ptr.size);
		break;

	case IR_UNDEFINED:
		asm_ins2("xorl", R4(REG_RAX), R4(REG_RAX));
		reg_to_scalar(REG_RAX, ins);
		break;

	case IR_VLA_ALLOC: {
		int slot_offset = ins->vla_alloc.dominance * 8;
		for (int i = ins->vla_alloc.dominance + 1; i < vla_info.count; i++)
			asm_ins2("movq", IMM(0), MEM(-vla_info.vla_slot_buffer_offset + i * 8, REG_RBP));

		label_id tmp_label = register_label();
		asm_ins2("movq", MEM(-vla_info.vla_slot_buffer_offset + slot_offset, REG_RBP), R8(REG_RAX));
		asm_ins2("cmpq", IMM(0), R8(REG_RAX));
		asm_ins1("jne", IMML_ABS(tmp_label, 0));
		asm_ins2("movq", R8(REG_RSP), R8(REG_RAX));
		asm_ins2("movq", R8(REG_RAX), MEM(-vla_info.vla_slot_buffer_offset + slot_offset, REG_RBP));
		asm_label(0, tmp_label);
		tmp_label++;

		asm_ins2("movq", R8(REG_RAX), R8(REG_RSP));

		asm_ins2("movq", R8(REG_RSP), MEM(-vla_info.vla_slot_buffer_offset + slot_offset, REG_RBP));
		scalar_to_reg(ins->arguments[0], REG_RAX);
		asm_ins2("subq", R8(REG_RAX), R8(REG_RSP));
		reg_to_scalar(REG_RSP, ins);
		// Align %rsp to 16 boundary. (Remember stack grows downwards. So rounding down is actually correct.)
		asm_ins2("andq", IMM(-16), R8(REG_RSP));
	} break;

	case IR_ALLOC:
		assert(ins->alloc.stack_location != -1);
		asm_ins2("leaq", MEM(-ins->alloc.stack_location, REG_RBP), R8(REG_RSI));
		reg_to_scalar(REG_RSI, ins);
		break;

	case IR_COPY_MEMORY:
		scalar_to_reg(ins->arguments[0], REG_RSI);
		scalar_to_reg(ins->arguments[1], REG_RDI);

		codegen_memcpy(ins->copy_memory.size);
		break;

	case IR_LOAD_BASE_RELATIVE:
		asm_ins2("leaq", MEM(ins->load_base_relative.offset, REG_RBP), R8(REG_RDI));
		asm_ins2("leaq", MEM(-ins->cg_info.stack_location, REG_RBP), R8(REG_RSI));

		codegen_memcpy(ins->size);
		break;

	case IR_LOAD_BASE_RELATIVE_ADDRESS:
		asm_ins2("leaq", MEM(ins->load_base_relative_address.offset, REG_RBP), R8(REG_RDI));
		scalar_to_reg(ins->arguments[0], REG_RSI);

		codegen_memcpy(ins->load_base_relative_address.size);
		break;
		
	case IR_ALLOCATE_CALL_STACK:
	case IR_STORE_STACK_RELATIVE:
	case IR_STORE_STACK_RELATIVE_ADDRESS:
	case IR_GET_REG:
	case IR_SET_REG:
	case IR_IF:
	case IR_RETURN:
	case IR_PHI:
	case IR_PROJECT:
		break;

	default:
		printf("%d %d\n", ins->type, ins->index);
		NOTIMP();
	}
}

static void codegen_phi_node(struct node *current_block, struct node *next_block) {
	if (next_block->type != IR_REGION)
		return;

	for (unsigned i = 0; i < next_block->use_size; i++) {
		struct node *ins = next_block->uses[i];

		if (ins->type != IR_PHI || ins->size == 0)
			continue;

		struct node *source_var;

		if (current_block == next_block->arguments[0]) {
			source_var = ins->arguments[1];
		} else if (current_block == next_block->arguments[1]) {
			source_var = ins->arguments[2];
		} else {
			ICE("Phi node in %d reachable from invalid block %d (not %d or %d)",
				next_block,
				current_block,
				next_block->arguments[0], next_block->arguments[1]);
		}

		if (!source_var)
			continue;

		asm_comment("Phi node to %d (%d %d %d)", ins->index,
					ins->arguments[0] ? ins->arguments[0]->index : -1,
					ins->arguments[1] ? ins->arguments[1]->index : -1,
					ins->arguments[2] ? ins->arguments[2]->index : -1
			);
		scalar_to_reg(source_var, REG_RAX);
		reg_to_scalar(REG_RAX, ins);
	}
}

static void codegen_block(struct node *block, struct node *func) {
	asm_label(0, block->block_info.label);

	for (struct node *ins = block->child; ins; ins = ins->next)
		codegen_instruction(ins, func);

	struct node *end = block->block_info.end;
	if (!end || end->type == IR_DEAD) {
		// TODO: This should actually be a normal ret.
		asm_ins0("ud2");
	} else if (end->type == IR_RETURN) {
		codegen_set_reg_chain(end->arguments[1]);
		asm_comment("Block return.");
		if (rbp_save_info.has_saved_rsp) {
			asm_ins2("movq", MEM(-rbp_save_info.offset, REG_RBP), R8(REG_RBP));
		}
		asm_ins0("leave");
		asm_ins0("ret");
	} else if (node_is_control(end)) {
		codegen_phi_node(block, end);
		if (end == block->next) {
			asm_comment("Block jump elided");
		} else {
			asm_comment("Block jump");
			asm_ins1("jmp", IMML_ABS(end->block_info.label, 0));
		}
	} else if (end->type == IR_IF) {
		struct node *cond = end->arguments[1];
		scalar_to_reg(cond, REG_RDI);
		asm_ins2("testq", R8(REG_RDI), R8(REG_RDI));
		asm_ins1("je", IMML_ABS(end->if_info.block_false->block_info.label, 0));
		asm_ins1("jmp", IMML_ABS(end->if_info.block_true->block_info.label, 0));
	} else {
		printf("Ending node on %d %d\n", end->type, IR_IF);
		NOTIMP();
	}
}

static void codegen_function(struct node *func) {
	int perm_stack_count = 0;
	int max_temp_stack = 0;

	// Give labels to blocks.
	for (struct node *block = func->child; block; block = block->next) {
		block->block_info.label = register_label();
	}

	// Allocate variables that spans multiple blocks.
	for (struct node *block = func->child; block; block = block->next) {
		for (struct node *ins = block->child; ins; ins = ins->next) {
			/* if (!ins->spans_block || ins->size == 0) */
			/* 	continue; */
			if (ins->size == 0)
				continue;
			
			perm_stack_count += ins->size;

			ins->cg_info.storage = VAR_STOR_STACK;
			ins->cg_info.stack_location = perm_stack_count;
		}
	}

	// Allocate VLAs, a bit tricky, but works.
	vla_info.count = 0;
	for (struct node *block = func->child; block; block = block->next) {
		for (struct node *ins = block->child; ins; ins = ins->next) {

			if (ins->type == IR_VLA_ALLOC)
				ins->vla_alloc.dominance = vla_info.count++;
		}
	}

	perm_stack_count += vla_info.count * 8;
	vla_info.vla_slot_buffer_offset = perm_stack_count;

	size_t stack_alignment = 0;
	// Allocate IR_ALLOC instructions.
	for (struct node *block = func->child; block; block = block->next) {
		for (struct node *ins = block->child; ins; ins = ins->next) {
			if (ins->type == IR_ALLOC) {
				perm_stack_count += ins->alloc.size;

				if (ins->alloc.alignment) {
					size_t remainder = perm_stack_count % ins->alloc.alignment;
					if (remainder != 0)
						perm_stack_count += ins->alloc.alignment - remainder;

					stack_alignment = MAX(stack_alignment, (size_t)ins->alloc.alignment);
				}

				ins->alloc.stack_location = perm_stack_count;
			}
		}
	}

	if (func->function.preamble_alloc) {
		perm_stack_count += func->function.preamble_alloc;
		vla_info.alloc_preamble = perm_stack_count;
	}

	// Allocate variables that are local to one block.
	for (struct node *block = func->child; block; block = block->next) {
		for (struct node *ins = block->child; ins; ins = ins->next) {
			continue;
			if (ins->spans_block || !ins->used || ins->size == 0)
				continue;

			struct node *block = ins->first_block;

			block->block_info.stack_counter += ins->size;

			ins->cg_info.storage = VAR_STOR_STACK;
			ins->cg_info.stack_location = perm_stack_count + block->block_info.stack_counter;

			max_temp_stack = MAX(block->block_info.stack_counter, max_temp_stack);
		}
	}

	rbp_save_info.has_saved_rsp = 0;
	rbp_save_info.offset = 0;

	if (stack_alignment > 16) {
		perm_stack_count += 8;
		rbp_save_info.offset = perm_stack_count;
		rbp_save_info.has_saved_rsp = 1;
	}

	label_id func_label = register_label_name(sv_from_str((char *)func->function.name));
	asm_label(func->function.is_global, func_label);
	asm_ins1("pushq", R8(REG_RBP));

	if (rbp_save_info.has_saved_rsp) {
		asm_ins2("movq", R8(REG_RSP), R8(REG_RBP));
		asm_ins2("andq", IMM(-stack_alignment), R8(REG_RSP));
		asm_ins2("movq", R8(REG_RBP), MEM(-rbp_save_info.offset, REG_RSP));
	}

	asm_ins2("movq", R8(REG_RSP), R8(REG_RBP));

	int stack_sub = round_up_to_nearest(perm_stack_count + max_temp_stack, 16);
	if (stack_sub)
		asm_ins2("subq", IMM(stack_sub), R8(REG_RSP));

	abi_emit_function_preamble(func);

	for (int i = 0; i < vla_info.count; i++)
		asm_ins2("movq", IMM(0), MEM(-vla_info.vla_slot_buffer_offset + i * 8, REG_RBP));

	struct node *reg_source = func->projects[1];
	if (reg_source)
		codegen_get_reg_uses(reg_source);

	for (struct node *block = func->child; block; block = block->next)
		codegen_block(block, func);

	int total_stack_usage = max_temp_stack + perm_stack_count;
	if (codegen_flags.debug_stack_size && total_stack_usage >= codegen_flags.debug_stack_min)
		printf("Function %s has stack consumption: %d\n", func->function.name, total_stack_usage);
}

int codegen_get_alloc_preamble(void) {
	return vla_info.alloc_preamble;
}

void codegen(void) {
	for (struct node *func = first_function; func; func = func->next)
		codegen_function(func);

	rodata_codegen();
	data_codegen();

	asm_finish();
}
