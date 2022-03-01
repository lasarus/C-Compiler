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

void codegen_binary_operator(enum ir_binary_operator ibo,
							 var_id lhs, var_id rhs, var_id res) {
	scalar_to_reg(lhs, REG_RDI);
	scalar_to_reg(rhs, REG_RSI);

	int size = get_variable_size(lhs);

	if (size != 4 && size != 8) {
		printf("Invalid size %d = %d op %d with %d\n", get_variable_size(res), size, get_variable_size(rhs), ibo);
	}

	assert(size == 4 || size == 8);

	asm_emit("%s", binary_operator_outputs[size == 4 ? 0 : 1][ibo]);

	reg_to_scalar(REG_RAX, res);
}

void codegen_call(var_id variable, int non_clobbered_register) {
	scalar_to_reg(variable, non_clobbered_register);
	asm_ins1("callq", R8S(non_clobbered_register));
}

// Address in rdi.
void codegen_memzero(int len) {
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

void codegen_stackcpy(int dest, int source, int len) {
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

void codegen_instruction(struct instruction ins, struct function *func) {
	const char *ins_str = dbg_instruction(ins);
	asm_comment("instruction start \"%s\":", ins_str);
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
					asm_ins2("movb", IMM(constant_to_u64(c)), MEM(-variable_info[ins.result].stack_location, REG_RBP));
					break;
				case 2:
					asm_ins2("movw", IMM(constant_to_u64(c)), MEM(-variable_info[ins.result].stack_location, REG_RBP));
					break;
				case 4:
					asm_ins2("movl", IMM(constant_to_u64(c)), MEM(-variable_info[ins.result].stack_location, REG_RBP));
					break;
				case 8:
					asm_ins2("movabsq", IMM(constant_to_u64(c)), R8(REG_RAX));
					asm_ins2("movq", R8(REG_RAX), MEM(-variable_info[ins.result].stack_location, REG_RBP));
					break;

				case 0: break;

				default: NOTIMP();
				}
			} else {
				uint8_t buffer[size];
				constant_to_buffer(buffer, c, 0, -1);
				for (int i = 0; i < size; i++)
					asm_ins2("movb", IMM(buffer[i]), MEM(-variable_info[ins.result].stack_location - i, REG_RBP));
			}
		} break;

		case CONSTANT_LABEL:
			if (codegen_flags.cmodel == CMODEL_LARGE) {
				asm_ins2("movabsq", IMML(rodata_get_label_string(c.label.label), c.label.offset), R8(REG_RDI));
			} else if (codegen_flags.cmodel == CMODEL_SMALL) {
				asm_ins2("movq", IMML(rodata_get_label_string(c.label.label), c.label.offset), R8(REG_RDI));
			}
			asm_ins2("leaq", MEM(-variable_info[ins.result].stack_location, REG_RBP), R8(REG_RSI));
			codegen_memcpy(get_variable_size(ins.result));
			break;

		case CONSTANT_LABEL_POINTER:
			if (codegen_flags.cmodel == CMODEL_LARGE) {
				asm_ins2("movabsq", IMML(rodata_get_label_string(c.label.label), c.label.offset), R8(REG_RAX));
				asm_ins2("movq", R8(REG_RAX), MEM(-variable_info[ins.result].stack_location, REG_RBP));
			} else if (codegen_flags.cmodel == CMODEL_SMALL) {
				asm_ins2("movq", IMML(rodata_get_label_string(c.label.label), c.label.offset),
						 MEM(-variable_info[ins.result].stack_location, REG_RBP));
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
		asm_ins1("notq", R8(REG_RAX));
		reg_to_scalar(REG_RAX, ins.result);
		break;

	case IR_NEGATE_INT:
		scalar_to_reg(ins.int_cast.rhs, REG_RAX);
		asm_ins1("negq", R8(REG_RAX));
		reg_to_scalar(REG_RAX, ins.result);
		break;

	case IR_NEGATE_FLOAT:
		scalar_to_reg(ins.int_cast.rhs, REG_RAX);
		if (get_variable_size(ins.result) == 4) {
			asm_ins2("movd", R4(REG_RAX), XMM(1));
			asm_ins1("negq", R8(REG_RAX));
			asm_ins2("xorps", XMM(0), XMM(0));
			asm_ins2("subss", XMM(1), XMM(0));
			asm_ins2("movd", XMM(0), R4(REG_RAX));
		} else if (get_variable_size(ins.result) == 8) {
			asm_ins2("movq", R8(REG_RAX), XMM(1));
			asm_ins2("xorps", XMM(0), XMM(0));
			asm_ins2("subsd", XMM(1), XMM(0));
			asm_ins2("movq", XMM(0), R8(REG_RAX));
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
		asm_ins2("leaq", MEM(-variable_info[ins.result].stack_location, REG_RBP), R8(REG_RSI));

		codegen_memcpy(get_variable_size(ins.result));
	} break;

	case IR_LOAD_BASE_RELATIVE:
		asm_ins2("leaq", MEM(ins.load_base_relative.offset, REG_RBP), R8(REG_RDI));
		asm_ins2("leaq", MEM(-variable_info[ins.result].stack_location, REG_RBP), R8(REG_RSI));

		codegen_memcpy(get_variable_size(ins.result));
	break;

	case IR_STORE: {
		scalar_to_reg(ins.store.pointer, REG_RSI);
		asm_ins2("leaq", MEM(-variable_info[ins.store.value].stack_location, REG_RBP), R8(REG_RDI));

		codegen_memcpy(get_variable_size(ins.store.value));
	} break;

	case IR_STORE_STACK_RELATIVE: {
		asm_ins2("leaq", MEM(ins.store_stack_relative.offset, REG_RSP), R8(REG_RSI));
		asm_ins2("leaq", MEM(-variable_info[ins.store_stack_relative.variable].stack_location, REG_RBP), R8(REG_RDI));

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
				asm_ins2("movsbq", R1(REG_RAX), R8(REG_RAX));
			} else if (size_rhs == 2) {
				asm_ins2("movswq", R2(REG_RAX), R8(REG_RAX));
			} else if (size_rhs == 4) {
				asm_ins2("movslq", R4(REG_RAX), R8(REG_RAX));
			}
		}
		reg_to_scalar(REG_RAX, ins.result);
	} break;

	case IR_BOOL_CAST: {
		scalar_to_reg(ins.bool_cast.rhs, REG_RAX);

		asm_ins2("testq", R8(REG_RAX), R8(REG_RAX));
		asm_ins1("setne", R1(REG_RAX));

		reg_to_scalar(REG_RAX, ins.result);
	} break;

	case IR_FLOAT_CAST: {
		scalar_to_reg(ins.float_cast.rhs, REG_RAX);
		int size_rhs = get_variable_size(ins.float_cast.rhs),
			size_result = get_variable_size(ins.result);

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
				asm_ins2("movd", R4(REG_RAX), XMM(0));
				asm_ins2("cvttss2si", XMM(0), R8(REG_RAX));
			} else if (size_rhs == 8) {
				asm_ins2("movd", R8(REG_RAX), XMM(0));
				asm_ins2("cvttsd2si", XMM(0), R8(REG_RAX));
			}
		} else {
			if (sign && size_rhs == 1) {
				asm_ins2("movsbl", R1(REG_RAX), R4(REG_RAX));
			} else if (!sign && size_rhs == 1) {
				asm_ins2("movzbl", R1(REG_RAX), R4(REG_RAX));
			} else if (sign && size_rhs == 2) {
				asm_ins2("movswl", R2(REG_RAX), R4(REG_RAX));
			} else if (!sign && size_rhs == 2) {
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
		}
		reg_to_scalar(REG_RAX, ins.result);
	} break;

	case IR_ADDRESS_OF:
		asm_ins2("leaq", MEM(-variable_info[ins.address_of.variable].stack_location, REG_RBP), R8(REG_RAX));
		reg_to_scalar(REG_RAX, ins.result);
		break;

	case IR_VA_START:
		abi_emit_va_start(ins.result, func);
		break;

	case IR_VA_ARG:
		abi_emit_va_arg(ins.result, ins.va_arg_.array, ins.va_arg_.type);
		break;

	case IR_SET_ZERO:
		asm_ins2("leaq", MEM(-variable_info[ins.result].stack_location, REG_RBP), R8(REG_RDI));
		codegen_memzero(get_variable_size(ins.result));
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

		static int tmp_label = 0;
		asm_ins2("movq", MEM(-variable_info[slot->slot].stack_location, REG_RBP), R8(REG_RAX));
		asm_ins2("cmpq", IMM(0), R8(REG_RAX));
		asm_emit("jne .Lvla%d", tmp_label);
		asm_ins2("movq", R8(REG_RSP), R8(REG_RAX));
		asm_ins2("movq", R8(REG_RAX), MEM(-variable_info[slot->slot].stack_location, REG_RBP));
		asm_label(0, ".Lvla%d", tmp_label);
		tmp_label++;

		asm_ins2("movq", R8(REG_RAX), R8(REG_RSP));

		asm_ins2("movq", R8(REG_RSP), MEM(-variable_info[slot->slot].stack_location, REG_RBP));
		scalar_to_reg(ins.stack_alloc.length, REG_RAX);
		asm_ins2("subq", R8(REG_RAX), R8(REG_RSP));
		reg_to_scalar(REG_RSP, ins.result);
		// Align %rsp to 16 boundary. (Remember stack grows downwards. So rounding down is actually correct.)
		asm_ins2("andq", IMM(-16), R8(REG_RSP));
	} break;

	case IR_CLEAR_STACK_BUCKET: // no-op
	case IR_ADD_TEMPORARY:
		break;

	case IR_SET_REG:
		if (ins.set_reg.is_ssa) {
			asm_ins2("movsd", MEM(-variable_info[ins.set_reg.variable].stack_location, REG_RBP),
					 XMM(ins.set_reg.register_index));
		} else {
			scalar_to_reg(ins.set_reg.variable, ins.set_reg.register_index);
		}
		break;

	case IR_GET_REG:
		if (ins.get_reg.is_ssa) {
			if (get_variable_size(ins.result) == 4) {
				asm_ins2("movss", XMM(ins.get_reg.register_index),
						 MEM(-variable_info[ins.result].stack_location, REG_RBP));
			} else {
				asm_ins2("movsd", XMM(ins.get_reg.register_index),
						 MEM(-variable_info[ins.result].stack_location, REG_RBP));
			}
		} else {
			reg_to_scalar(ins.get_reg.register_index, ins.result);
		}
		break;

	case IR_MODIFY_STACK_POINTER:
		asm_ins2("addq", IMM(ins.modify_stack_pointer.change), R8(REG_RSP));
		break;

	default:
		printf("%d\n", ins.type);
		NOTIMP();
	}
}

void codegen_block(struct block *block, struct function *func) {
	asm_label(0, ".LB%d", block->id);

	for (int i = 0; i < block->size; i++)
		codegen_instruction(block->instructions[i], func);

	struct block_exit *block_exit = &block->exit;
	asm_comment("EXIT IS OF TYPE : %d", block_exit->type);
	switch (block_exit->type) {
	case BLOCK_EXIT_JUMP:
		asm_emit("jmp .LB%d", block_exit->jump);
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
		asm_emit("je .LB%d", block_exit->if_.block_false);
		asm_emit("jmp .LB%d", block_exit->if_.block_true);
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
			asm_emit("je .LB%d", block_exit->switch_.labels.labels[i].block);
		}
		if (block_exit->switch_.labels.default_) {
			asm_emit("jmp .LB%d", block_exit->switch_.labels.default_);
		}
	} break;

	case BLOCK_EXIT_NONE:
		asm_ins0("ud2");
		break;
	}
}

void codegen_function(struct function *func) {
	int temp_stack_count = 0, perm_stack_count = 0;
	int max_temp_stack = 0;

	for (int i = 0; i < func->var_size; i++) {
		var_id var = func->vars[i];
		if (get_variable_stack_bucket(var))
			continue;

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

	asm_label(func->is_global, "%s", func->name);
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

void codegen(const char *path) {
	variable_info = malloc(sizeof(*variable_info) * get_n_vars());

	asm_init_text_out(path);

	for (int i = 0; i < ir.size; i++)
		codegen_function(ir.functions + i);

	rodata_codegen();
	data_codegen();

	asm_finish();
}
