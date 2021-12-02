#include "assembler.h"

#include <common.h>
#include <inttypes.h>
#include <codegen/registers.h>

#include <stdarg.h>

static FILE *out;
static const char *current_section;

void asm_init_text_out(const char *path) {
	current_section = ".text";

	out = fopen(path, "w");

	if (!out)
		ICE("Could not open file %s", path);
}

void asm_finish(void) {
	fclose(out);
}

// Emit.
void asm_section(const char *section) {
	if (strcmp(section, current_section) != 0)
		asm_emit(".section %s", section);
	current_section = section;
}

void asm_emit(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	if (!str_contains(fmt, ':'))
		fprintf(out, "\t");
	vfprintf(out, fmt, args);
	fprintf(out, "\n");
	va_end(args);
}

void asm_emit_no_newline(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(out, fmt, args);
	va_end(args);
}

void asm_comment(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	fprintf(out, "\t#");
	vfprintf(out, fmt, args);
	fprintf(out, "\n");
	va_end(args);
}

void asm_emit_char(char c) {
	fputc(c, out);
}

static void asm_emit_operand(struct operand op) {
	switch (op.type) {
	case OPERAND_EMPTY: break;
	case OPERAND_REG:
		if (op.reg.upper_byte)
			NOTIMP();
		asm_emit_no_newline("%s", get_reg_name(op.reg.reg, op.reg.size));
		break;
	case OPERAND_SSE_REG:
		asm_emit_no_newline("%%xmm%d", op.sse_reg);
		break;
	case OPERAND_STAR_REG:
		if (op.reg.upper_byte)
			NOTIMP();
		asm_emit_no_newline("*%s", get_reg_name(op.reg.reg, op.reg.size));
		break;
	case OPERAND_IMM:
		asm_emit_no_newline("$%" PRIi64, op.imm);
		break;
	case OPERAND_IMM_LABEL:
		if (op.imm_label.offset)
			asm_emit_no_newline("$%s+%" PRIi64, op.imm_label.label, op.imm_label.offset);
		else
			asm_emit_no_newline("$%s", op.imm_label.label);
		break;
	case OPERAND_MEM:
		if (op.mem.offset)
			asm_emit_no_newline("%" PRIi64, op.mem.offset);
		asm_emit_no_newline("(");
		if (op.mem.base != REG_NONE && op.mem.index == REG_NONE && op.mem.scale == 1) {
			asm_emit_no_newline("%s", get_reg_name(op.mem.base, 8));
		} else {
			NOTIMP();
		}
		asm_emit_no_newline(")");
		break;
	}
}

void asm_ins0(const char *mnemonic) {
	asm_emit_no_newline("\t%s\n", mnemonic);
}

void asm_ins1(const char *mnemonic, struct operand op1) {
	asm_emit_no_newline("\t%s ", mnemonic);

	asm_emit_operand(op1);
	asm_emit_no_newline("\n");
}

void asm_ins2(const char *mnemonic, struct operand op1, struct operand op2) {
	asm_emit_no_newline("\t%s ", mnemonic);

	asm_emit_operand(op1);
	asm_emit_no_newline(", ");
	asm_emit_operand(op2);
	asm_emit_no_newline("\n");
}
