#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>
#include <string_view.h>
#include <codegen/rodata.h>

extern struct assembler_flags {
	int half_assemble;
} assembler_flags;

enum reg {
	REG_RAX,
	REG_RBX,
	REG_RCX,
	REG_RDX,
	REG_RSI,
	REG_RDI,
	REG_RBP,
	REG_RSP,
	REG_R8,
	REG_R9,
	REG_R10,
	REG_R11,
	REG_R12,
	REG_R13,
	REG_R14,
	REG_R15,

	REG_NONE = -1
};

struct operand {
	enum {
		OPERAND_EMPTY,
		OPERAND_REG, // Register.
		OPERAND_SSE_REG, // SSE Register.
		OPERAND_STAR_REG, // *Register. callq *%rax.
		OPERAND_IMM, // Immediate.
		OPERAND_IMM_ABSOLUTE,
		OPERAND_IMM_LABEL, // Immediate of the form $label+offset.
		OPERAND_IMM_LABEL_ABSOLUTE,
		OPERAND_MEM // Memory.
	} type;

	union {
		struct {
			enum reg reg;
			int upper_byte; // 1 if ah, bh, ch, or dh. 0 otherwise.
			int size;
		} reg;

		int sse_reg;

		struct {
			enum reg index, base;
			int scale;
			uint64_t offset;
		} mem;

		uint64_t imm;

		struct {
			label_id label_;
			//const char *label;
			uint64_t offset;
		} imm_label;
	};
};

#define IMM_(X) { .type = OPERAND_IMM, .imm = (X) }
#define IMML_(LABEL_ID, X) { .type = OPERAND_IMM_LABEL, .imm_label = { (LABEL_ID), (X) } }
#define IMML_ABS_(LABEL_ID, X) { .type = OPERAND_IMM_LABEL_ABSOLUTE, .imm_label = { (LABEL_ID), (X) } }
#define IMM_ABS_(X) { .type = OPERAND_IMM_ABSOLUTE, .imm = (X) }
#define MEM_(OFFSET, BASE) { .type = OPERAND_MEM, .mem = { (REG_NONE), BASE, 1, (OFFSET) } }
#define R8S_(REG) { .type = OPERAND_STAR_REG, .reg = { (REG), 0, 8 } }
#define R8_(REG) { .type = OPERAND_REG, .reg = { (REG), 0, 8 } }
#define R4_(REG) { .type = OPERAND_REG, .reg = { (REG), 0, 4 } }
#define R2_(REG) { .type = OPERAND_REG, .reg = { (REG), 0, 2 } }
#define R1_(REG) { .type = OPERAND_REG, .reg = { (REG), 0, 1 } }
#define R1U_(REG) { .type = OPERAND_REG, .reg = { (REG), 1, 1 } }
#define XMM_(IDX) { .type = OPERAND_SSE_REG, .sse_reg = (IDX) }
#define IMM(X) (struct operand) IMM_(X)
#define IMM_ABS(X) (struct operand) IMM_ABS_(X)
#define IMML(LABEL_ID, X) (struct operand) IMML_(LABEL_ID, X)
#define IMML_ABS(LABEL_ID, X) (struct operand) IMML_ABS_(LABEL_ID, X)
#define MEM(OFFSET, BASE) (struct operand) MEM_(OFFSET, BASE)
#define R8S(REG) (struct operand) R8S_(REG)
#define R8(REG) (struct operand) R8_(REG)
#define R4(REG) (struct operand) R4_(REG)
#define R2(REG) (struct operand) R2_(REG)
#define R1(REG) (struct operand) R1_(REG)
#define R1U(REG) (struct operand) R1U_(REG)
#define XMM(IDX) (struct operand) XMM_(IDX)

void asm_init_text_out(const char *path);
void asm_finish(void);

// Emit.
void asm_section(const char *section);
void asm_comment(const char *fmt, ...);

struct asm_instruction {
	const char *mnemonic;
	struct operand ops[4];
};

void asm_ins(struct asm_instruction *ins);
void asm_ins0(const char *mnemonic);
void asm_ins1(const char *mnemonic, struct operand op1);
void asm_ins2(const char *mnemonic, struct operand op1, struct operand op2);
void asm_ins3(const char *mnemonic, struct operand op1, struct operand op2, struct operand op3);

void asm_quad(struct operand op);
void asm_byte(struct operand op);
void asm_zero(int len);

void asm_label(int global, label_id label);
void asm_string(struct string_view str);

#endif
