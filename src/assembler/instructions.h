#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include <stdint.h>

struct operand_encoding {
	enum {
		OE_EMPTY,
		OE_NONE,
		OE_MODRM_REG,
		OE_MODRM_RM,
		OE_IMM8,
		OE_IMM16,
		OE_IMM32,
		OE_IMM64,
		OE_REL8,
		OE_REL16,
		OE_REL32,
		OE_OPEXT
	} type;

	int duplicate;
};

struct operand_accepts {
	enum {
		ACC_EMPTY,
		ACC_RAX,
		ACC_RCX,
		ACC_REG,
		ACC_REG_STAR,
		ACC_IMM8_S,
		ACC_IMM16_S,
		ACC_IMM32_S,
		ACC_IMM8_U,
		ACC_IMM16_U,
		ACC_IMM32_U,
		ACC_IMM64,
		ACC_REL8,
		ACC_REL16,
		ACC_REL32,
		ACC_MODRM,
		ACC_XMM,
		ACC_XMM_M32,
		ACC_XMM_M64,
		ACC_XMM_M128,
	} type;

	union {
		struct {
			int size;
		} reg;
	};
};

#define A_IMM32_S {.type = ACC_IMM32_S }
#define A_IMM8_S {.type = ACC_IMM8_S }
#define A_IMM8 {.type = ACC_IMM8_U }
#define A_IMM16 {.type = ACC_IMM16_U }
#define A_IMM16_S {.type = ACC_IMM16_S }
#define A_IMM32 {.type = ACC_IMM32_U }
#define A_IMM64 {.type = ACC_IMM64 }
#define A_REL8 {.type = ACC_REL8 }
#define A_REL16 {.type = ACC_REL16 }
#define A_REL32 {.type = ACC_REL32 }
#define A_REG(SIZE) {.type = ACC_REG, .reg.size = SIZE }
#define A_XMM { .type = ACC_XMM }
#define A_XMM_M32 { .type = ACC_XMM_M32 }
#define A_XMM_M64 { .type = ACC_XMM_M64 }
#define A_XMM_M128 { .type = ACC_XMM_M128 }
#define A_MODRM(SIZE) {.type = ACC_MODRM, .reg.size = SIZE }
#define A_REG_STAR(SIZE) {.type = ACC_REG_STAR, .reg.size = SIZE }
#define A_RAX(SIZE) {.type = ACC_RAX, .reg.size = SIZE }
#define A_RCX(SIZE) {.type = ACC_RCX, .reg.size = SIZE }

struct encoding {
	const char *mnemonic;
	uint8_t opcode;
	uint8_t op2, op3;
	int rex, rexw;
	int modrm_extension;
	int slash_r;
	int op_size_prefix, repne_prefix, repe_prefix;
	struct operand_encoding operand_encoding[4];
	struct operand_accepts operand_accepts[4];
};

#define MR {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}

// This is a table of all instruction encodings.
// It is not exhaustive, and will probably have to be
// generated automatically from the manual at some
// point.
struct encoding encodings[] = {
	{"addq", 0x05, .rex = 1, .rexw = 1, .operand_encoding = {{OE_NONE, 0}, {OE_IMM32, 0}}, .operand_accepts = {A_RAX(8), A_IMM32_S}},
	{"addq", 0x04, .operand_encoding = {{OE_NONE, 0}, {OE_IMM8, 0}}, .operand_accepts = {A_REG(4), A_IMM8_S}},
	{"addq", 0x83, .rex = 1, .rexw = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM8, 0}}, .operand_accepts = {A_REG(8), A_IMM8_S}},
	{"addq", 0x81, .rexw = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM32, 0}}, .operand_accepts = {A_REG(8), A_IMM32_S}},
	{"addq", 0x01, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_REG(8), A_REG(8)}},
	{"addq", 0x03, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(8), A_MODRM(8)}},

	{"subq", 0x83, .rex = 1, .rexw = 1, .modrm_extension = 5, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM8, 0}}, .operand_accepts = {A_MODRM(8), A_IMM8_S}},
	{"subq", 0x81, .rex = 1, .rexw = 1, .modrm_extension = 5, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM32, 0}}, .operand_accepts = {A_MODRM(8), A_IMM32_S}},
	{"subq", 0x29, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(8), A_REG(8)}},
	{"subl", 0x29, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(4), A_REG(4)}},

	{"andl", 0x21, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_REG(4), A_REG(4)}},
	{"andq", 0x21, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_REG(8), A_REG(8)}},
	{"andq", 0x83, .rex = 1, .rexw = 1, .modrm_extension = 4, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM8, 0}}, .operand_accepts = {A_REG(8), A_IMM8_S}},

	{"orl", 0x09, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_REG(4), A_REG(4)}},
	{"orq", 0x09, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_REG(8), A_REG(8)}},

	{"xor", 0x31, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_REG(8), A_REG(8)}},
	{"xorq", 0x31, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_REG(8), A_REG(8)}},
	{"xorl", 0x31, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_REG(4), A_REG(4)}},

	{"divl", 0xf7, .modrm_extension = 6, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(4)}},
	{"divq", 0xf7, .rexw = 1, .modrm_extension = 6, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(8)}},

	{"idivl", 0xf7, .modrm_extension = 7, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(4)}},
	{"idivq", 0xf7, .rexw = 1, .modrm_extension = 7, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(8)}},

	{"imulq", 0x69, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 1}, {OE_MODRM_REG, 0}, {OE_IMM32, 0}}, .operand_accepts = {A_REG(8), A_IMM32_S}},
	{"imulq", 0x6b, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 1}, {OE_MODRM_REG, 0}, {OE_IMM8, 0}}, .operand_accepts = {A_REG(8), A_IMM8_S}},
	{"imulq", 0x0f, .op2 = 0xaf, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(8), A_MODRM(8)}},

	{"imull", 0x0f, .op2 = 0xaf, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(4), A_MODRM(4)}},

	{"callq", 0xff, .modrm_extension = 2, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_REG_STAR(8)}},
	{ "cltd", .opcode = 0x99 },
	{ "cqto", .rexw = 1, .opcode = 0x99 },
	{ "leave", .opcode = 0xc9 },
	{ "ret", .opcode = 0xc3 },
	{ "ud2", .opcode = 0x0f, .op2 = 0x0b },
	
	{"jmp", 0xe9, .operand_encoding = {{OE_REL32, 0}}, .operand_accepts = {A_REL32}},
	{"je", 0x0f, .op2 = 0x84, .operand_encoding = {{OE_REL32, 0}}, .operand_accepts = {A_REL32}},

	{"cmpl", 0x39, .slash_r = 1, .operand_encoding = MR, .operand_accepts = {A_REG(4), A_REG(4)}},
	{"cmpq", 0x39, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = MR, .operand_accepts = {A_MODRM(8), A_REG(8)}},

	{"cmpl", 0x83, .modrm_extension = 7, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM8, 0}}, .operand_accepts = {A_REG(4), A_IMM8_S}},
	{"cmpq", 0x83, .rex = 1, .rexw = 1, .modrm_extension = 7, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM8, 0}}, .operand_accepts = {A_REG(8), A_IMM8}},

	{"movl", 0xb8, .modrm_extension = 0, .operand_encoding = {{OE_OPEXT, 0}, {OE_IMM32, 0}}, .operand_accepts = {A_REG(4), A_IMM32}},
	{"movl", 0xc7, .modrm_extension = 0, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM32, 0}}, .operand_accepts = {A_MODRM(4), A_IMM32}},
	{"movl", 0xc7, .modrm_extension = 0, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM32, 0}}, .operand_accepts = {A_MODRM(4), A_IMM32_S}},
	{"movl", 0x89, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(4), A_REG(4)}},
	{"movl", 0x89, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(4), A_REG(4)}},
	{"movw", 0xc7, .op_size_prefix = 1, .modrm_extension = 0, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM16, 0}}, .operand_accepts = {A_MODRM(2), A_IMM16}},
	{"movw", 0xc7, .op_size_prefix = 1, .modrm_extension = 0, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM16, 0}}, .operand_accepts = {A_MODRM(2), A_IMM16_S}},
	{"movw", 0x89, .op_size_prefix = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(2), A_REG(2)}},
	{"movw", 0x8b, .op_size_prefix = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(2), A_MODRM(2)}},
	{"movq", 0x89, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(8), A_REG(8)}},
	{"movq", 0x8b, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(8), A_MODRM(8)}},
	{"movb", 0x8a, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(1), A_MODRM(1)}},
	{"movb", 0x88, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(1), A_REG(1)}},
	{"movb", 0xc6, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM8, 0}}, .operand_accepts = {A_MODRM(1), A_IMM8_S}},
	{"movb", 0xc6, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM8, 0}}, .operand_accepts = {A_MODRM(1), A_IMM8}},
	{"movl", 0x8b, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(4), A_MODRM(4)}},
	{"movq", 0xc7, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM32, 0}}, .operand_accepts = {A_MODRM(8), A_IMM32_S}},

	{"movabsq", 0xb8, .rex = 1, .rexw = 1, .operand_encoding = {{OE_OPEXT, 0}, {OE_IMM64, 0}}, .operand_accepts = {A_REG(8), A_IMM64}},

	{"leaq", 0x8d, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(8), A_MODRM(8)}},
	{"leal", 0x8d, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(4), A_MODRM(4)}},

	{"movswl", 0x0f, .op2 = 0xbf, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(4), A_MODRM(2)}},
	{"movswq", 0x0f, .rexw = 1, .op2 = 0xbf, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(8), A_MODRM(2)}},
	{"movslq", 0x63, .rex = 1, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(8), A_MODRM(4)}},
	{"movsbl", 0x0f, .op2 = 0xbe, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(4), A_MODRM(1)}},
	{"movsbw", 0x0f, .op_size_prefix = 1, .op2 = 0xbe, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(2), A_MODRM(1)}},
	{"movsbq", 0x0f, .rexw = 1, .op2 = 0xbe, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(8), A_MODRM(1)}},

	{"movzwl", 0x0f, .op2 = 0xb7, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(4), A_MODRM(2)}},
	{"movzbl", 0x0f, .op2 = 0xb6, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(4), A_MODRM(1)}},

	{"pushq", 0x50, .operand_encoding = {{OE_OPEXT, 0}}, .operand_accepts = {A_REG(8)}},

	{"notl", 0xf7, .modrm_extension = 2, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(4)}},
	{"notq", 0xf7, .rex = 1, .rexw = 1, .modrm_extension = 2, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(8)}},

	{"negl", 0xf7, .modrm_extension = 3, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(4)}},
	{"negq", 0xf7, .rexw = 1, .modrm_extension = 3, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(8)}},

	{"seta", 0x0f, .op2 = 0x97, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(1)}},
	{"setb", 0x0f, .op2 = 0x92, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(1)}},
	{"setbe", 0x0f, .op2 = 0x96, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(1)}},
	{"sete", 0x0f, .op2 = 0x94, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(1)}},
	{"setg", 0x0f, .op2 = 0x9f, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(1)}},
	{"setge", 0x0f, .op2 = 0x9d, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(1)}},
	{"setgl", 0x0f, .op2 = 0x9c, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(1)}},
	{"setl", 0x0f, .op2 = 0x9c, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(1)}},
	{"setle", 0x0f, .op2 = 0x9e, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(1)}},
	{"setnb", 0x0f, .op2 = 0x93, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(1)}},
	{"setne", 0x0f, .op2 = 0x95, .operand_encoding = {{OE_MODRM_RM, 0}}, .operand_accepts = {A_MODRM(1)}},
	
	{"salq", 0xd3, .rex = 1, .rexw = 1, .modrm_extension = 4, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_NONE, 0}}, .operand_accepts = {A_MODRM(8), A_RCX(1)}},
	{"sall", 0xd3, .modrm_extension = 4, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_NONE, 0}}, .operand_accepts = {A_MODRM(4), A_RCX(1)}},

	{"sarl", 0xd3, .modrm_extension = 7, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_NONE, 0}}, .operand_accepts = {A_MODRM(4), A_RCX(1)}},
	{"sarq", 0xd3, .rexw = 1, .modrm_extension = 7, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_NONE, 0}}, .operand_accepts = {A_MODRM(8), A_RCX(1)}},

	{"shrl", 0xd3, .modrm_extension = 5, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_NONE, 0}}, .operand_accepts = {A_MODRM(4), A_RCX(1)}},
	{"shrq", 0xd3, .rexw = 1, .modrm_extension = 5, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_NONE, 0}}, .operand_accepts = {A_MODRM(8), A_RCX(1)}},
	{"shrq", 0xc1, .rexw = 1, .modrm_extension = 5, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_IMM8, 0}}, .operand_accepts = {A_REG(8), A_IMM8}},

	{"testb", 0x84, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(1), A_REG(1)}},
	{"testl", 0x85, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(4), A_REG(4)}},
	{"testq", 0x85, .rexw = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(8), A_REG(8)}},

	{"movsd", 0x0f, .op2 = 0x11, .repne_prefix = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_XMM_M64, A_XMM}},
	{"movsd", 0x0f, .op2 = 0x10, .repne_prefix = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_XMM, A_XMM_M64}},

	{"movss", 0x0f, .op2 = 0x11, .repe_prefix = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_XMM_M32, A_XMM}},

	{"movd", 0x0f, .op2 = 0x7e, .op_size_prefix = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(4), A_XMM}},
	{"movd", 0x0f, .op2 = 0x6e, .op_size_prefix = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_XMM, A_MODRM(4)}},

	{"movd", 0x0f, .op2 = 0x7e, .op_size_prefix = 1, .slash_r = 1, .rexw = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(8), A_XMM}},
	{"movd", 0x0f, .op2 = 0x6e, .op_size_prefix = 1, .slash_r = 1, .rexw = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_XMM, A_MODRM(8)}},

	// TODO: Is `movd %rax, %xmm0` the same as `movq %rax, %xmm0`?
	{"movq", 0x0f, .op2 = 0x7e, .op_size_prefix = 1, .slash_r = 1, .rexw = 1, .operand_encoding = {{OE_MODRM_RM, 0}, {OE_MODRM_REG, 0}}, .operand_accepts = {A_MODRM(8), A_XMM}},
	{"movq", 0x0f, .op2 = 0x6e, .op_size_prefix = 1, .slash_r = 1, .rexw = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_XMM, A_MODRM(8)}},

	{"cvtsi2ss", 0x0f, .op2 = 0x2a, .repe_prefix = 1, .slash_r = 1, .rexw = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_XMM, A_MODRM(8)}},
	{"cvtsi2sd", 0x0f, .op2 = 0x2a, .repne_prefix = 1, .slash_r = 1, .rexw = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_XMM, A_MODRM(8)}},

	{"cvtsd2ss", 0x0f, .op2 = 0x5a, .repne_prefix = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_XMM, A_XMM_M64}},
	{"cvtss2sd", 0x0f, .op2 = 0x5a, .repe_prefix = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_XMM, A_XMM_M32}},

	{"cvttss2si", 0x0f, .op2 = 0x2c, .repe_prefix = 1, .slash_r = 1, .rexw = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(8), A_XMM_M32}},

	{"cvttsd2si", 0x0f, .op2 = 0x2c, .repne_prefix = 1, .slash_r = 1, .rexw = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_REG(8), A_XMM_M64}},

	{"xorps", 0x0f, .op2 = 0x57, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_XMM, A_XMM_M128}},

	{"subss", 0x0f, .op2 = 0x5c, .repe_prefix = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_XMM, A_XMM_M32}},

	{"subsd", 0x0f, .op2 = 0x5c, .repne_prefix = 1, .slash_r = 1, .operand_encoding = {{OE_MODRM_REG, 0}, {OE_MODRM_RM, 0}}, .operand_accepts = {A_XMM, A_XMM_M64}},
};

#endif
