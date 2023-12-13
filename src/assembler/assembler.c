#include "assembler.h"
#include "encode.h"
#include "linker/object.h"
#include "linker/linker.h"
#include "linker/elf.h"

#include <common.h>
#include <escape_sequence.h>
#include <inttypes.h>
#include <codegen/registers.h>

#include <stdarg.h>

// TODO: Handle short jumps differently.
// Right now all jumps are treated as 32 bit offsets.

static int assemble_to_text = 0;

static FILE *out;
static const char *current_section;

static struct object *out_object;

void asm_reset(void) {
	assemble_to_text = 0;
	out = NULL;
	current_section = NULL;
	out_object = NULL;
}

void asm_init_text_out(const char *path) {
	assemble_to_text = 1;
	current_section = ".text";

	out = fopen(path, "w");

	if (!out)
		ICE("Could not open file %s", path);
}

void asm_init_object(struct object *object) {
	object_start();
	out_object = object;
}

void asm_finish(void) {
	if (assemble_to_text) {
		fclose(out);
	} else {
		*out_object = *object_finish();
	}
}

// Emit.
static void asm_emit_no_newline(const char *fmt, ...) {
	if (!assemble_to_text)
		ICE("Can't emit assembly when writing elf files.");

	va_list args;
	va_start(args, fmt);
	vfprintf(out, fmt, args);
	va_end(args);
}

static void emit_label(label_id id) {
	char buffer[64];
	rodata_get_label(id, sizeof buffer, buffer);
	asm_emit_no_newline("%s", buffer);
}

void asm_section(const char *section) {
	if (assemble_to_text) {
		if (strcmp(section, current_section) != 0)
			asm_emit_no_newline(".section %s\n", section);
		current_section = section;
	} else {
		object_set_section(section);
	}
}

void asm_comment(const char *fmt, ...) {
	if (!assemble_to_text)
		return;

	va_list args;
	va_start(args, fmt);
	fprintf(out, "\t#");
	vfprintf(out, fmt, args);
	fprintf(out, "\n");
	va_end(args);
}

void asm_label(int global, label_id label) {
	if (!assemble_to_text) {
		object_symbol_set(label, global);
	} else {
		if (global) {
			fprintf(out, ".global ");
			emit_label(label);
			fprintf(out, "\n");
		}

		emit_label(label);
		fprintf(out, ":\n");
	}
}

void asm_string(struct string_view str) {
	if (!assemble_to_text) {
		object_write((uint8_t *)str.str, str.len);
		object_write_byte(0);
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
	case OPERAND_IMM_LABEL:
		if (op.imm_label.label_ == -1) {
			asm_emit_no_newline("$%" PRIi64, op.imm_label.offset);
		} else if (op.imm_label.offset) {
			asm_emit_no_newline("$");
			emit_label(op.imm_label.label_);
			asm_emit_no_newline("+%" PRIi64, op.imm_label.offset);
		} else {
			asm_emit_no_newline("$");
			emit_label(op.imm_label.label_);
		}
		break;
	case OPERAND_IMM_LABEL_ABSOLUTE:
		if (op.imm_label.label_ == -1) {
			asm_emit_no_newline("%" PRIi64, op.imm_label.offset);
		} else if (op.imm_label.offset) {
			emit_label(op.imm_label.label_);
			asm_emit_no_newline("+%" PRIi64, op.imm_label.offset);
		} else {
			emit_label(op.imm_label.label_);
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

static void asm_ins_impl(const char *mnemonic, struct operand ops[4]) {
	if (!assemble_to_text) {
		// Swap order of instructions.
		struct operand swapped[4] = { 0 };
		for (int i = 3, j = 0; i >= 0; i--) {
			if (ops[i].type)
				swapped[j++] = ops[i];
		}

		struct relocation relocations[4];
		int n_relocations = 0;
		uint8_t output[15];
		int len;
		assemble_instruction(output, &len, mnemonic, swapped,
							 relocations, &n_relocations);

		if (len == -1) {
			printf("Could not assemble instruction: \n");
			out = stdout;
			assemble_to_text = 1;
			asm_ins_impl(mnemonic, ops);
			assemble_to_text = 0;
			out = NULL;
			ICE("Assembler error.");
		}

		for (int i = 0; i < n_relocations; i++) {
			struct relocation *rel = relocations + i;

			switch (rel->size) {
			case 8:
				if (rel->relative)
					NOTIMP();
				object_symbol_relocate(rel->label, rel->offset, rel->imm, RELOCATE_64);
				break;

			case 4:
				if (rel->relative)
					object_symbol_relocate(rel->label, rel->offset, -(len - rel->offset), RELOCATE_32_RELATIVE);
				else
					object_symbol_relocate(rel->label, rel->offset, rel->imm, RELOCATE_32);
				break;

			default:
				printf("%d %s\n", rel->size, mnemonic);
					NOTIMP();
				}
			}
			object_write(output, len);
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
	if (!assemble_to_text) {
		switch (op.type) {
		case OPERAND_IMM_LABEL:
			NOTIMP();
			break;

		case OPERAND_IMM_LABEL_ABSOLUTE:
			if (op.imm_label.label_ == -1) {
				object_write_quad(op.imm_label.offset);
			} else {
				object_symbol_relocate(op.imm_label.label_, 0, op.imm_label.offset, RELOCATE_64);
				object_write_quad(0);
			}
			break;

		default: NOTIMP();
		}
	} else {
		asm_emit_no_newline(".quad ");
		asm_emit_operand(op);
		asm_emit_no_newline("\n");
	}
}

void asm_byte(struct operand op) {
	if (!assemble_to_text) {
		switch (op.type) {
		case OPERAND_IMM_LABEL_ABSOLUTE:
			if (op.imm_label.label_ == -1) {
				object_write_byte(op.imm_label.offset);
			} else {
				NOTIMP();
			}
			break;

		default: NOTIMP();
		}
	} else {
		asm_emit_no_newline(".byte ");
		asm_emit_operand(op);
		asm_emit_no_newline("\n");
	}
}

void asm_zero(int len) {
	if (!assemble_to_text) {
		object_write_zero(len);
	} else {
		asm_emit_no_newline(".zero %d\n", len);
	}
}

void asm_align(int alignment) {
	if (!assemble_to_text) {
		object_align(alignment);
	} else {
		asm_emit_no_newline(".align %d\n", alignment);
	}
}
