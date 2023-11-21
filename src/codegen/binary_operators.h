#ifndef BINARY_OPERATORS_H
#define BINARY_OPERATORS_H

#include <parser/parser.h>
#include <assembler/assembler.h>

#define BINARY_INS_32(MNEMONIC) {						\
		{MNEMONIC, {R4_(REG_RCX), R4_(REG_RAX)}}	\
	}

#define BINARY_COMP_32(MNEMONIC) {					\
		{"cmpl", {R4_(REG_RCX), R4_(REG_RAX)}},	\
		{MNEMONIC, {R1_(REG_RAX)}},					\
		{"movzbl", {R1_(REG_RAX), R4_(REG_RAX)}},	\
	}

#define BINARY_INS_FLT_32(MNEMONIC) {				\
		{"movd", {R4_(REG_RAX), XMM_(0)}},		\
		{"movd", {R4_(REG_RCX), XMM_(1)}},		\
		{MNEMONIC, {XMM_(1), XMM_(0)}},			\
		{"movd", {XMM_(0), R4_(REG_RAX)}},		\
	}

#define BINARY_COMP_FLT_32(MNEMONIC) {				\
		{"movd", {R4_(REG_RAX), XMM_(0)}},		\
		{"movd", {R4_(REG_RCX), XMM_(1)}},		\
		{"xorq", {R8_(REG_RAX), R8_(REG_RAX)}},	\
		{"ucomiss", {XMM_(1), XMM_(0)}},		\
		{MNEMONIC, {R1_(REG_RAX)}},				\
	}

#define BINARY_INS_64(MNEMONIC) {						\
		{MNEMONIC, {R8_(REG_RCX), R8_(REG_RAX)}}	\
	}

#define BINARY_COMP_64(MNEMONIC) {					\
		{"cmpq", {R8_(REG_RCX), R8_(REG_RAX)}},	\
		{MNEMONIC, {R1_(REG_RAX)}},					\
		{"movzbl", {R1_(REG_RAX), R4_(REG_RAX)}},	\
	}

#define BINARY_INS_FLT_64(MNEMONIC) {				\
		{"movq", {R8_(REG_RAX), XMM_(0)}},		\
		{"movq", {R8_(REG_RCX), XMM_(1)}},		\
		{MNEMONIC, {XMM_(1), XMM_(0)}},			\
		{"movq", {XMM_(0), R8_(REG_RAX)}},		\
	}

#define BINARY_COMP_FLT_64(MNEMONIC) {				\
		{"movq", {R8_(REG_RAX), XMM_(0)}},		\
		{"movq", {R8_(REG_RCX), XMM_(1)}},		\
		{"ucomisd", {XMM_(1), XMM_(0)}},		\
		{MNEMONIC, {R1_(REG_RAX)}},				\
		{"movzbl", {R1_(REG_RAX), R4_(REG_RAX)}},	\
	}

#define BINARY_INS(M32, M64) BINARY_INS_32(M32), BINARY_INS_64(M64)
#define BINARY_COMP(M) BINARY_COMP_32(M), BINARY_COMP_64(M)
#define BINARY_INS_FLT(M32, M64) BINARY_INS_FLT_32(M32), BINARY_INS_FLT_64(M64)
#define BINARY_COMP_FLT(M) BINARY_COMP_FLT_32(M), BINARY_COMP_FLT_64(M)

static struct asm_instruction (*codegen_asm_table[IR_COUNT])[2][5] = {
	[IR_ADD] = &(struct asm_instruction [2][5]) {BINARY_INS("addl", "addq")},
	[IR_SUB] = &(struct asm_instruction [2][5]) { BINARY_INS("subl", "subq") },
	[IR_IMUL] = &(struct asm_instruction [2][5]) { BINARY_INS("imull", "imulq") },
	[IR_MUL] = &(struct asm_instruction [2][5]) { BINARY_INS("imull", "imulq") },
	[IR_BXOR] = &(struct asm_instruction [2][5]) { BINARY_INS("xorl", "xorq") },
	[IR_BOR] = &(struct asm_instruction [2][5]) { BINARY_INS("orl", "orq") },
	[IR_BAND] = &(struct asm_instruction [2][5]) { BINARY_INS("andl", "andq") },
	[IR_IDIV] = &(struct asm_instruction [2][5]) {
		{{"cltd", { { 0 } }}, {"idivl", {R4_(REG_RCX)}}},
		{{"cqto", { { 0 } }}, {"idivq", {R8_(REG_RCX)}}},
	},
	[IR_IMOD] = &(struct asm_instruction [2][5]) {
		{{"cltd", { { 0 } }},
		 {"idivl", {R4_(REG_RCX)}}, {"movl", {R4_(REG_RDX), R4_(REG_RAX)}}},
		{{"cqto", { { 0 } }},
		 {"idivq", {R8_(REG_RCX)}}, {"movq", {R8_(REG_RDX), R8_(REG_RAX)}}},
	},
	[IR_LSHIFT] = &(struct asm_instruction [2][5]) {
		{{"sall", {R1_(REG_RCX), R4_(REG_RAX)}}},
		{{"salq", {R1_(REG_RCX), R8_(REG_RAX)}}},
	},
	[IR_IRSHIFT] = &(struct asm_instruction [2][5]) {
		{{"sarl", {R1_(REG_RCX), R4_(REG_RAX)}}},
		{{"sarq", {R1_(REG_RCX), R8_(REG_RAX)}}},
	},
	[IR_RSHIFT] = &(struct asm_instruction [2][5]) {
		{{"shrl", {R1_(REG_RCX), R4_(REG_RAX)}}},
		{{"shrq", {R1_(REG_RCX), R8_(REG_RAX)}}},
	},
	[IR_IGREATER] = &(struct asm_instruction [2][5]) { BINARY_COMP("setg") },
	[IR_ILESS_EQ] = &(struct asm_instruction [2][5]) { BINARY_COMP("setle") },
	[IR_ILESS] = &(struct asm_instruction [2][5]) { BINARY_COMP("setl") },
	[IR_IGREATER_EQ] = &(struct asm_instruction [2][5]) { BINARY_COMP("setge") },
	[IR_EQUAL] = &(struct asm_instruction [2][5]) { BINARY_COMP("sete") },
	[IR_NOT_EQUAL] = &(struct asm_instruction [2][5]) { BINARY_COMP("setne") },
	[IR_LESS] = &(struct asm_instruction [2][5]) { BINARY_COMP("setb") },
	[IR_GREATER] = &(struct asm_instruction [2][5]) { BINARY_COMP("seta") },
	[IR_LESS_EQ] = &(struct asm_instruction [2][5]) { BINARY_COMP("setbe") },
	[IR_GREATER_EQ] = &(struct asm_instruction [2][5]) { BINARY_COMP("setnb") },
	[IR_DIV] = &(struct asm_instruction [2][5]) {
		{{"xorq", {R8_(REG_RDX), R8_(REG_RDX)}}, {"divl", {R4_(REG_RCX)}}},
		{{"xorq", {R8_(REG_RDX), R8_(REG_RDX)}}, {"divq", {R8_(REG_RCX)}}},
	},
	[IR_MOD] = &(struct asm_instruction [2][5]) {
		{{"xorq", {R8_(REG_RDX), R8_(REG_RDX)}}, {"divl", {R4_(REG_RCX)}}, {"movl", {R4_(REG_RDX), R4_(REG_RAX)}}},
		{{"xorq", {R8_(REG_RDX), R8_(REG_RDX)}}, {"divq", {R8_(REG_RCX)}}, {"movq", {R8_(REG_RDX), R8_(REG_RAX)}}},
	},

	[IR_FLT_ADD] = &(struct asm_instruction [2][5]) { BINARY_INS_FLT("addss", "addsd") },
	[IR_FLT_SUB] = &(struct asm_instruction [2][5]) { BINARY_INS_FLT("subss", "subsd") },
	[IR_FLT_MUL] = &(struct asm_instruction [2][5]) { BINARY_INS_FLT("mulss", "mulsd") },
	[IR_FLT_DIV] = &(struct asm_instruction [2][5]) { BINARY_INS_FLT("divss", "divsd") },
	[IR_FLT_LESS] = &(struct asm_instruction [2][5]) { BINARY_COMP_FLT("setb") },
	[IR_FLT_GREATER] = &(struct asm_instruction [2][5]) { BINARY_COMP_FLT("seta") },
	[IR_FLT_LESS_EQ] = &(struct asm_instruction [2][5]) { BINARY_COMP_FLT("setbe") },
	[IR_FLT_GREATER_EQ] = &(struct asm_instruction [2][5]) { BINARY_COMP_FLT("setnb") },
	[IR_FLT_EQUAL] = &(struct asm_instruction [2][5]) { BINARY_COMP_FLT("sete") },
	[IR_FLT_NOT_EQUAL] = &(struct asm_instruction [2][5]) { BINARY_COMP_FLT("setne") },
};

#endif
