#ifndef BINARY_OPERATORS_H
#define BINARY_OPERATORS_H

#include <parser/parser.h>
#include <assembler/assembler.h>

#define BINARY_INS(MNEMONIC) {						\
		{"movq", {R8_(REG_RDI), R8_(REG_RAX)}},		\
		{MNEMONIC, {R4_(REG_RSI), R4_(REG_RAX)}}	\
	}

#define BINARY_COMP(MNEMONIC) {					\
		{"xorq", {R8_(REG_RAX), R8_(REG_RAX)}},	\
		{"cmpl", {R4_(REG_RSI), R4_(REG_RDI)}},	\
		{MNEMONIC, {R1_(REG_RAX)}}				\
	}

#define BINARY_INS_FLT(MNEMONIC) {				\
		{"movd", {R4_(REG_RDI), XMM_(0)}},		\
		{"movd", {R4_(REG_RSI), XMM_(1)}},		\
		{MNEMONIC, {XMM_(1), XMM_(0)}},			\
		{"movd", {XMM_(0), R4_(REG_RAX)}},		\
	}

#define BINARY_COMP_FLT(MNEMONIC) {				\
		{"movd", {R4_(REG_RDI), XMM_(0)}},		\
		{"movd", {R4_(REG_RSI), XMM_(1)}},		\
		{"ucomiss", {XMM_(1), XMM_(0)}},		\
		{MNEMONIC, {R1_(REG_RAX)}},				\
	}

#define BINARY_INS_64(MNEMONIC) {						\
		{"movq", {R8_(REG_RDI), R8_(REG_RAX)}},		\
		{MNEMONIC, {R8_(REG_RSI), R8_(REG_RAX)}}	\
	}

#define BINARY_COMP_64(MNEMONIC) {					\
		{"xorq", {R8_(REG_RAX), R8_(REG_RAX)}},	\
		{"cmpq", {R8_(REG_RSI), R8_(REG_RDI)}},	\
		{MNEMONIC, {R1_(REG_RAX)}}				\
	}

#define BINARY_INS_FLT_64(MNEMONIC) {				\
		{"movq", {R8_(REG_RDI), XMM_(0)}},		\
		{"movq", {R8_(REG_RSI), XMM_(1)}},		\
		{MNEMONIC, {XMM_(1), XMM_(0)}},			\
		{"movq", {XMM_(0), R8_(REG_RAX)}},		\
	}

#define BINARY_COMP_FLT_64(MNEMONIC) {				\
		{"movq", {R8_(REG_RDI), XMM_(0)}},		\
		{"movq", {R8_(REG_RSI), XMM_(1)}},		\
		{"ucomisd", {XMM_(1), XMM_(0)}},		\
		{MNEMONIC, {R1_(REG_RAX)}},				\
	}

struct asm_instruction binary_operator_output[2][IBO_COUNT][5] = {
	[0][IBO_ADD] = BINARY_INS("addl"),
	[0][IBO_SUB] = BINARY_INS("subl"),
	[0][IBO_IMUL] = BINARY_INS("imull"),
	[0][IBO_MUL] = BINARY_INS("imull"),
	[0][IBO_BXOR] = BINARY_INS("xorl"),
	[0][IBO_BOR] = BINARY_INS("orl"),
	[0][IBO_BAND] = BINARY_INS("andl"),
	[0][IBO_IDIV] = {{"movq", {R8_(REG_RDI), R8_(REG_RAX)}}, {"cltd", { { 0 } }}, {"idivl", {R4_(REG_RSI)}}},
	[0][IBO_IMOD] = {{"movq", {R8_(REG_RDI), R8_(REG_RAX)}}, {"cltd", { { 0 } }},
					 {"idivl", {R4_(REG_RSI)}}, {"movl", {R4_(REG_RDX), R4_(REG_RAX)}}},
	[0][IBO_LSHIFT] = {{"movq", {R8_(REG_RSI), R8_(REG_RCX)}}, {"movq", {R8_(REG_RDI), R8_(REG_RAX)}},
					   {"sall", {R1_(REG_RCX), R4_(REG_RAX)}}},
	[0][IBO_IRSHIFT] = {{"movq", {R8_(REG_RSI), R8_(REG_RCX)}}, {"movq", {R8_(REG_RDI), R8_(REG_RAX)}},
						{"sarl", {R1_(REG_RCX), R4_(REG_RAX)}}},
	[0][IBO_RSHIFT] = {{"movq", {R8_(REG_RSI), R8_(REG_RCX)}}, {"movq", {R8_(REG_RDI), R8_(REG_RAX)}},
						{"shrl", {R1_(REG_RCX), R4_(REG_RAX)}}},
	[0][IBO_IGREATER] = BINARY_COMP("setg"),
	[0][IBO_ILESS_EQ] = BINARY_COMP("setle"),
	[0][IBO_ILESS] = BINARY_COMP("setl"),
	[0][IBO_IGREATER_EQ] = BINARY_COMP("setge"),
	[0][IBO_EQUAL] = BINARY_COMP("sete"),
	[0][IBO_NOT_EQUAL] = BINARY_COMP("setne"),
	[0][IBO_LESS] = BINARY_COMP("setb"),
	[0][IBO_GREATER] = BINARY_COMP("seta"),
	[0][IBO_LESS_EQ] = BINARY_COMP("setbe"),
	[0][IBO_GREATER_EQ] = BINARY_COMP("setnb"),
	[0][IBO_DIV] = {{"movq", {R8_(REG_RDI), R8_(REG_RAX)}}, {"xorq", {R8_(REG_RDX), R8_(REG_RDX)}}, {"divl", {R4_(REG_RSI)}}},
	[0][IBO_MOD] = {{"movq", {R8_(REG_RDI), R8_(REG_RAX)}}, {"xorq", {R8_(REG_RDX), R8_(REG_RDX)}}, {"divl", {R4_(REG_RSI)}}, {"movl", {R4_(REG_RDX), R4_(REG_RAX)}}},

	[0][IBO_FLT_ADD] = BINARY_INS_FLT("addss"),
	[0][IBO_FLT_SUB] = BINARY_INS_FLT("subss"),
	[0][IBO_FLT_MUL] = BINARY_INS_FLT("mulss"),
	[0][IBO_FLT_DIV] = BINARY_INS_FLT("divss"),
	[0][IBO_FLT_LESS] = BINARY_COMP_FLT("setb"),
	[0][IBO_FLT_GREATER] = BINARY_COMP_FLT("seta"),
	[0][IBO_FLT_LESS_EQ] = BINARY_COMP_FLT("setbe"),
	[0][IBO_FLT_GREATER_EQ] = BINARY_COMP_FLT("setnb"),
	[0][IBO_FLT_EQUAL] = BINARY_COMP_FLT("sete"),
	[0][IBO_FLT_NOT_EQUAL] = BINARY_COMP_FLT("setne"),

	[1][IBO_ADD] = BINARY_INS_64("addq"),
	[1][IBO_SUB] = BINARY_INS_64("subq"),
	[1][IBO_IMUL] = BINARY_INS_64("imulq"),
	[1][IBO_MUL] = BINARY_INS_64("imulq"),
	[1][IBO_BXOR] = BINARY_INS_64("xorq"),
	[1][IBO_BOR] = BINARY_INS_64("orq"),
	[1][IBO_BAND] = BINARY_INS_64("andq"),
	[1][IBO_IDIV] = {{"movq", {R8_(REG_RDI), R8_(REG_RAX)}}, {"cqto", { { 0 } }}, {"idivq", {R8_(REG_RSI)}}},
	[1][IBO_IMOD] = {{"movq", {R8_(REG_RDI), R8_(REG_RAX)}}, {"cqto", { { 0 } }},
					 {"idivq", {R8_(REG_RSI)}}, {"movq", {R8_(REG_RDX), R8_(REG_RAX)}}},
	[1][IBO_LSHIFT] = {{"movq", {R8_(REG_RSI), R8_(REG_RCX)}}, {"movq", {R8_(REG_RDI), R8_(REG_RAX)}},
					   {"salq", {R1_(REG_RCX), R8_(REG_RAX)}}},
	[1][IBO_IRSHIFT] = {{"movq", {R8_(REG_RSI), R8_(REG_RCX)}}, {"movq", {R8_(REG_RDI), R8_(REG_RAX)}},
						{"sarq", {R1_(REG_RCX), R8_(REG_RAX)}}},
	[1][IBO_RSHIFT] = {{"movq", {R8_(REG_RSI), R8_(REG_RCX)}}, {"movq", {R8_(REG_RDI), R8_(REG_RAX)}},
						{"shrq", {R1_(REG_RCX), R8_(REG_RAX)}}},
	[1][IBO_IGREATER] = BINARY_COMP_64("setg"),
	[1][IBO_ILESS_EQ] = BINARY_COMP_64("setle"),
	[1][IBO_ILESS] = BINARY_COMP_64("setl"),
	[1][IBO_IGREATER_EQ] = BINARY_COMP_64("setge"),
	[1][IBO_EQUAL] = BINARY_COMP_64("sete"),
	[1][IBO_NOT_EQUAL] = BINARY_COMP_64("setne"),
	[1][IBO_LESS] = BINARY_COMP_64("setb"),
	[1][IBO_GREATER] = BINARY_COMP_64("seta"),
	[1][IBO_LESS_EQ] = BINARY_COMP_64("setbe"),
	[1][IBO_GREATER_EQ] = BINARY_COMP_64("setnb"),
	[1][IBO_DIV] = {{"movq", {R8_(REG_RDI), R8_(REG_RAX)}}, {"xorq", {R8_(REG_RDX), R8_(REG_RDX)}}, {"divq", {R8_(REG_RSI)}}},
	[1][IBO_MOD] = {{"movq", {R8_(REG_RDI), R8_(REG_RAX)}}, {"xorq", {R8_(REG_RDX), R8_(REG_RDX)}}, {"divq", {R8_(REG_RSI)}}, {"movq", {R8_(REG_RDX), R8_(REG_RAX)}}},

	[1][IBO_FLT_ADD] = BINARY_INS_FLT_64("addsd"),
	[1][IBO_FLT_SUB] = BINARY_INS_FLT_64("subsd"),
	[1][IBO_FLT_MUL] = BINARY_INS_FLT_64("mulsd"),
	[1][IBO_FLT_DIV] = BINARY_INS_FLT_64("divsd"),
	[1][IBO_FLT_LESS] = BINARY_COMP_FLT_64("setb"),
	[1][IBO_FLT_GREATER] = BINARY_COMP_FLT_64("seta"),
	[1][IBO_FLT_LESS_EQ] = BINARY_COMP_FLT_64("setbe"),
	[1][IBO_FLT_GREATER_EQ] = BINARY_COMP_FLT_64("setnb"),
	[1][IBO_FLT_EQUAL] = BINARY_COMP_FLT_64("sete"),
	[1][IBO_FLT_NOT_EQUAL] = BINARY_COMP_FLT_64("setne"),
};

#endif
