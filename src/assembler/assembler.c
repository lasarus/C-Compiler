#include "assembler.h"
#include "encode.h"

#include <common.h>
#include <inttypes.h>
#include <codegen/registers.h>

#include <stdarg.h>

struct assembler_flags assembler_flags = {
	.half_assemble = 0
};

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

void asm_label(int global, label_id label) {
	if (global) {
		fprintf(out, ".global ");
		rodata_emit_label(label);
		fprintf(out, "\n");
	}

	rodata_emit_label(label);
	fprintf(out, ":\n");
}

void asm_string(struct string_view str) {
	if (assembler_flags.half_assemble) {
		asm_emit_no_newline("\t.byte ");
		for (int i = 0; i < str.len; i++) {
			asm_emit_no_newline("0x%.2x", (uint8_t)str.str[i]);
			if (i != str.len - 1)
				asm_emit_no_newline(", ");
		}
		asm_emit_no_newline("\n");
	} else {
		asm_emit_no_newline("\t.string \"");
		for (int i = 0; i < str.len; i++) {
			char buffer[5];
			character_to_escape_sequence(str.str[i], buffer, 0);
			asm_emit_no_newline("%s", buffer);
		}
		asm_emit_no_newline("\"\n");
	}
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
	case OPERAND_IMM_ABSOLUTE:
		asm_emit_no_newline("%" PRIi64, op.imm);
		break;
	case OPERAND_IMM_LABEL:
		if (op.imm_label.offset) {
			asm_emit_no_newline("$");
			rodata_emit_label(op.imm_label.label_);
			asm_emit_no_newline("+%" PRIi64, op.imm_label.offset);
		} else {
			asm_emit_no_newline("$");
			rodata_emit_label(op.imm_label.label_);
		}
		break;
	case OPERAND_IMM_LABEL_ABSOLUTE:
		if (op.imm_label.offset) {
			rodata_emit_label(op.imm_label.label_);
			asm_emit_no_newline("+%" PRIi64, op.imm_label.offset);
		} else {
			rodata_emit_label(op.imm_label.label_);
		}
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

void asm_ins_impl(const char *mnemonic, struct operand ops[4]) {
	int do_half_assemble = 0;
	if (assembler_flags.half_assemble) {
		do_half_assemble = 1;
		for (int i = 0; i < 4; i++) {
			if (ops[i].type == OPERAND_IMM_LABEL ||
				ops[i].type == OPERAND_IMM_LABEL_ABSOLUTE) {
				do_half_assemble = 0;
				break;
			}
		}
	}
		
	if (do_half_assemble) {
		// Swap order of instructions.
		struct operand swapped[4] = { 0 };
		for (int i = 3, j = 0; i >= 0; i--) {
			if (ops[i].type)
				swapped[j++] = ops[i];
		}

		uint8_t output[15];
		int len;
		assemble_instruction(output, &len, mnemonic, swapped);

		if (len == -1)
			ICE("Could not assemble %s", mnemonic);

		asm_emit_no_newline("\t.byte ");
		for (int i = 0; i < len; i++) {
			asm_emit_no_newline("0x%.2x", output[i]);
			if (i != len - 1)
				asm_emit_no_newline(", ", output[i]);
		}
		asm_emit_no_newline("\n");
	} else {
		asm_emit_no_newline("\t%s ", mnemonic);
		for (int i = 0; i < 4 && ops[i].type; i++) {
			if (i)
				asm_emit_no_newline(", ");

			asm_emit_operand(ops[i]);
		}
		asm_emit_no_newline("\n");
	}
}

void asm_ins(struct asm_instruction *ins) {
	asm_ins_impl(ins->mnemonic, ins->ops);
}

void asm_ins0(const char *mnemonic) {
	asm_ins_impl(mnemonic, (struct operand[4]) { 0 });
}

void asm_ins1(const char *mnemonic, struct operand op1) {
	asm_ins_impl(mnemonic, (struct operand[4]) { op1 });
}

void asm_ins2(const char *mnemonic, struct operand op1, struct operand op2) {
	asm_ins_impl(mnemonic, (struct operand[4]) { op1, op2 });
}

void asm_quad(struct operand op) {
	asm_emit_no_newline(".quad ");
	asm_emit_operand(op);
	asm_emit_no_newline("\n");
}

void asm_byte(struct operand op) {
	asm_emit_no_newline(".byte ");
	asm_emit_operand(op);
	asm_emit_no_newline("\n");
}

void asm_zero(int len) {
	asm_emit_no_newline(".zero %d\n", len);
}
