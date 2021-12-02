#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>

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
		OPERAND_IMM_LABEL, // Immediate of the form $label+offset.
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
			const char *label;
			uint64_t offset;
		} imm_label;
	};
};

#define IMM(X) (struct operand) { .type = OPERAND_IMM, .imm = (X) }
#define IMML(STR, X) (struct operand) { .type = OPERAND_IMM_LABEL, .imm_label = { (STR), (X) } }
#define MEM(OFFSET, BASE) (struct operand) { .type = OPERAND_MEM, .mem = { (REG_NONE), BASE, 1, (OFFSET) } }
#define R8S(REG) (struct operand) { .type = OPERAND_STAR_REG, .reg = { (REG), 0, 8 } }
#define R8(REG) (struct operand) { .type = OPERAND_REG, .reg = { (REG), 0, 8 } }
#define R4(REG) (struct operand) { .type = OPERAND_REG, .reg = { (REG), 0, 4 } }
#define R2(REG) (struct operand) { .type = OPERAND_REG, .reg = { (REG), 0, 2 } }
#define R1(REG) (struct operand) { .type = OPERAND_REG, .reg = { (REG), 0, 1 } }
#define R1U(REG) (struct operand) { .type = OPERAND_REG, .reg = { (REG), 1, 1 } }
#define XMM(IDX) (struct operand) { .type = OPERAND_SSE_REG, .sse_reg = (IDX) }

void asm_init_text_out(const char *path);
void asm_finish(void);

// Emit.
void asm_section(const char *section);
void asm_emit(const char *fmt, ...);
void asm_instruction(const char *fmt, ...);
void asm_emit_no_newline(const char *fmt, ...);
void asm_emit_char(char c); // Used for string printing.

void asm_comment(const char *fmt, ...);

void asm_ins0(const char *mnemonic);
void asm_ins1(const char *mnemonic, struct operand op1);
void asm_ins2(const char *mnemonic, struct operand op1, struct operand op2);
void asm_ins3(const char *mnemonic, struct operand op1, struct operand op2, struct operand op3);

#endif
