#include "registers.h"
#include <parser/parser.h>

#include "codegen.h"

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
	default: ERROR("Invalid register size, %d", size);
	}
}

char size_to_suffix(int size) {
	switch (size) {
	case 8: return 'q';
	case 4: return 'l';
	case 2: return 'w';
	case 1: return 'b';
	default: ERROR("Invalid register size %d", size);
	}
}

const char *get_reg_name(int id, int size) {
	return registers[id][size_to_idx(size)];
}

void scalar_to_reg(int stack_pos, var_id scalar, int reg) {
	struct type *type = get_variable_type(scalar);
	assert(is_scalar(type));

	EMIT("xor %s, %s", registers[reg][0], registers[reg][0]);

	int size = calculate_size(type);

	EMIT("mov%c -%d(%%rbp), %s", size_to_suffix(size),
		 stack_pos, registers[reg][size_to_idx(size)]);
}

void reg_to_scalar(int reg, int stack_pos, var_id scalar) {
	struct type *type = get_variable_type(scalar);

	if (!is_scalar(type))
		ERROR("%s should be scalar", type_to_string(type));

	int size = calculate_size(type);
	EMIT("mov%c %s, -%d(%%rbp)", size_to_suffix(size),
		 registers[reg][size_to_idx(size)], stack_pos);
}

void load_address(struct type *type, int stack_pos, var_id result) {
	if (type_is_pointer(type)) {
		EMIT("movq (%%rdi), %%rax");
		reg_to_scalar(REG_RAX, stack_pos, result);
	} else if (type->type == TY_SIMPLE) {
		switch (type->simple) {
		case ST_INT:
			EMIT("movl (%%rdi), %%eax");
			reg_to_scalar(REG_RAX, stack_pos, result);
			break;

		default:
			NOTIMP();
		}
	} else {
		ERROR("Can't load type %s", type_to_string(type));
	}
}

void store_address(struct type *type, int stack_pos, var_id result) {
	if (type_is_pointer(type)) {
		scalar_to_reg(stack_pos, result, REG_RAX);
		EMIT("movq %%rax, (%%rdi)");
	} else if (type->type == TY_SIMPLE) {
		switch (type->simple) {
		case ST_INT:
			scalar_to_reg(stack_pos, result, REG_RAX);
			EMIT("movl %%eax, (%%rdi)");
			break;

		default:
			NOTIMP();
		}
	} else {
		NOTIMP();
	}
}

#if 0

void reg_to_stack(int reg, int size, int stack_offset) {
	EMIT("mov%c %s, -%d(%%rbp)", size_to_suffix(size),
		 registers[reg][size_to_idx(size)], stack_offset);
}

void stack_to_reg(int reg, int size, int stack_offset) {
	EMIT("mov%c -%d(%%rbp), %s", size_to_suffix(size),
		 stack_offset, registers[reg][size_to_idx(size)]);
}

#endif
