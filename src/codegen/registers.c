#include "registers.h"
#include "codegen.h"

#include <common.h>
#include <parser/parser.h>

#include <assert.h>

const char *registers[][4] = {
	{"%rax", "%eax", "%ax", "%al"}, // 0 0
	{"%rbx", "%ebx", "%bx", "%bl"}, // 1 1
	{"%rcx", "%ecx", "%cx", "%cl"}, // 2 0
	{"%rdx", "%edx", "%dx", "%dl"}, // 3 0
	{"%rsi", "%esi", "%si", "%sil"}, // 4 0
	{"%rdi", "%edi", "%di", "%dil"}, // 5 0
	{"%rbp", "%ebp", "%bp", "%bpl"}, // 6 1
	{"%rsp", "%esp", "%sp", "%spl"}, // 7 1
	{"%r8", "%r8d", "%r8w", "%r8b"}, // 8 0
	{"%r9", "%r9d", "%r9w", "%r9b"}, // 9 0
	{"%r10", "%r10d", "%r10w", "%r10b"}, // 10 0
	{"%r11", "%r11d", "%r11w", "%r11b"}, // 11 0
	{"%r12", "%r12d", "%r12w", "%r12b"}, // 12 1
	{"%r13", "%r13d", "%r13w", "%r13b"}, // 13 1
	{"%r14", "%r14d", "%r14w", "%r14b"}, // 14 1
	{"%r15", "%r15d", "%r15w", "%r15b"}}; // 15 1

int size_to_idx(int size) {
	switch (size) {
	case 8: return 0;
	case 4: return 1;
	case 2: return 2;
	case 1: return 3;
	default: ICE("Invalid register size, %d", size);
	}
}

char size_to_suffix(int size) {
	switch (size) {
	case 8: return 'q';
	case 4: return 'l';
	case 2: return 'w';
	case 1: return 'b';
	default: ICE("Invalid register size %d", size);
	}
}

const char *get_reg_name(int id, int size) {
	return registers[id][size_to_idx(size)];
}

void scalar_to_reg(var_id scalar, int reg) {
	emit("xor %s, %s", registers[reg][0], registers[reg][0]);

	int size = get_variable_size(scalar);

	emit("mov%c -%d(%%rbp), %s", size_to_suffix(size),
		 variable_info[scalar].stack_location, registers[reg][size_to_idx(size)]);
}

void reg_to_scalar(int reg, var_id scalar) {
	int size = get_variable_size(scalar);
	int msize = 0;
	for (int i = 0; i < size;) {
		if (msize)
			emit("shrq $%d, %s", msize * 8, get_reg_name(reg, 8));

		if (i + 8 <= size) {
			msize = 8;
		} else if (i + 4 <= size) {
			msize = 4;
		} else if (i + 2 <= size) {
			msize = 2;
		} else if (i + 1 <= size) {
			msize = 1;
		}
		emit("mov%c %s, -%d(%%rbp)",
			 size_to_suffix(msize),
			 get_reg_name(reg, msize),
			 variable_info[scalar].stack_location - i);

		i += msize;
	}
}

void load_address(struct type *type, var_id result) {
	if (type_is_pointer(type)) {
		emit("movq (%%rdi), %%rax");
		reg_to_scalar(REG_RAX, result);
	} else if (type->type == TY_SIMPLE) {
		switch (type->simple) {
		case ST_INT:
			emit("movl (%%rdi), %%eax");
			reg_to_scalar(REG_RAX, result);
			break;

		default:
			NOTIMP();
		}
	} else {
		ICE("Can't load type %s", dbg_type(type));
	}
}

void store_address(struct type *type, var_id result) {
	if (type_is_pointer(type)) {
		scalar_to_reg(result, REG_RAX);
		emit("movq %%rax, (%%rdi)");
	} else if (type->type == TY_SIMPLE) {
		switch (type->simple) {
		case ST_INT:
			scalar_to_reg(result, REG_RAX);
			emit("movl %%eax, (%%rdi)");
			break;

		default:
			NOTIMP();
		}
	} else {
		NOTIMP();
	}
}
