#include "encode.h"
#include "instructions.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static int get_disp_size(uint64_t disp_u) {
	long disp = disp_u;
	if (disp == 0)
		return 0;
	if (disp >= INT8_MIN && disp <= INT8_MAX)
		return 1;
	if (disp >= INT32_MIN && disp <= INT32_MAX)
		return 4;
	ICE("Invalid displacement size %ld", disp);
}

static int needs_rex(enum reg reg, int upper_byte, int size) {
	return (size == 1 && !upper_byte) &&
		(reg == REG_RSP ||
		 reg == REG_RDI ||
		 reg == REG_RBP ||
		 reg == REG_RSI);
}

static int register_index(enum reg reg) {
	static const int reg_idxs[] = {
		[REG_RAX] = 0,
		[REG_RBX] = 3,
		[REG_RCX] = 1,
		[REG_RDX] = 2,
		[REG_RSI] = 6,
		[REG_RDI] = 7,
		[REG_RBP] = 5,
		[REG_RSP] = 4,
		[REG_R8] = 8,
		[REG_R9] = 9,
		[REG_R10] = 10,
		[REG_R11] = 11,
		[REG_R12] = 12,
		[REG_R13] = 13,
		[REG_R14] = 14,
		[REG_R15] = 15
	};
	return reg_idxs[(int)reg];
}

void encode_sib(struct operand *o, int *rex_b, int *rex_x, int *modrm_mod, int *modrm_rm,
				uint64_t *disp, int *has_disp8, int *has_disp32,
				int *has_sib, int *sib_scale, int *sib_index, int *sib_base) {
	(void)sib_scale;
	int disp_size = get_disp_size(o->mem.offset);

	// (%reg)
	if (o->mem.index == REG_NONE && o->mem.base != REG_NONE &&
		disp_size == 0) {
		if (o->mem.base == REG_RSP ||
			o->mem.base == REG_R12) {
			*modrm_rm = 4;
			*modrm_mod = 0;

			*has_sib = 1;
			*sib_index = 4;
			*sib_base = 4;
			if (o->mem.base == REG_R12)
				*rex_b = 1;
		} else if (o->mem.base == REG_RBP ||
				   o->mem.base == REG_R13) {
			*sib_index = 4;
			*modrm_mod = 1;

			*has_disp8 = 1;
			*disp = 0;
			*sib_base = 5;
			if (o->mem.base == REG_R13)
				*rex_b = 1;
		} else {
			*modrm_mod = 0;

			*modrm_rm = register_index(o->mem.base) & 7;
			*rex_b = (register_index(o->mem.base) & 0x8) >> 3;
		}
	// disp8(%reg)
	} else if (o->mem.index == REG_NONE && o->mem.base != REG_NONE &&
			   disp_size <= 1) {
		if (o->mem.base == REG_RSP ||
			o->mem.base == REG_R12) {
			*modrm_rm = 4;
			*modrm_mod = 1;

			*has_sib = 1;
			*sib_index = 4;
			*sib_base = 4;
			if (o->mem.base == REG_R12)
				*rex_b = 1;
		} else {
			*modrm_mod = 1;
			*has_disp8 = 1;

			*modrm_rm = register_index(o->mem.base) & 7;
			*rex_b = (register_index(o->mem.base) & 0x8) >> 3;
		}

		*has_disp8 = 1;
		*disp = o->mem.offset;
	// disp32(%reg)
	} else if (o->mem.index == REG_NONE && o->mem.base != REG_NONE &&
			   disp_size <= 4) {
		if (o->mem.base == REG_RSP ||
			o->mem.base == REG_R12) {
			*modrm_mod = 2;

			*has_sib = 1;
			*sib_index = 4;

			*modrm_rm = register_index(o->mem.base) & 7;
			*rex_b = (register_index(o->mem.base) & 0x8) >> 3;
		} else {
			*modrm_mod = 2;

			*disp = o->mem.offset;

			*modrm_rm = register_index(o->mem.base) & 7;
			*rex_b = (register_index(o->mem.base) & 0x8) >> 3;
		}

		*has_disp32 = 1;
		*disp = o->mem.offset;
	// (%base, %scale, index)
	} else if (o->mem.index != REG_NONE && o->mem.base != REG_NONE &&
			   disp_size == 0) {
		*has_sib = 1;
		if (o->mem.index == REG_RSP) {
			NOTIMP();
		} else if (o->mem.base == REG_RBP ||
				   o->mem.base == REG_R13) {
			NOTIMP();
		} else {
			*modrm_rm = 4;
				
			*sib_base = register_index(o->mem.base) & 7;
			*rex_b = (register_index(o->mem.base) & 0x8) >> 3;

			*sib_index = register_index(o->mem.index) & 7;
			*rex_x = (register_index(o->mem.index) & 0x8) >> 3;
		}
	} else {
		ICE("Params: %d (%d, %d, %d)", disp_size, o->mem.base, o->mem.index, o->mem.scale);
	}

	// disp8(%base, %scale, index)
	// disp32(%base, %scale, index)
}

void assemble_encoding(uint8_t *output, int *len, struct encoding *encoding, struct operand ops[4]) {
	int has_imm8 = 0, has_imm16 = 0, has_imm32 = 0, has_imm64 = 0;
	uint64_t imm = 0;

	int has_rex = encoding->rexw || encoding->rex;
	int rex_b = 0;
	int rex_r = 0;
	int rex_x = 0;

	int has_modrm = 0;
	int modrm_reg = 0;
	int modrm_mod = 0;
	int modrm_rm = 0;

	int has_disp8 = 0, has_disp32 = 0;
	uint64_t disp = 0;

	int has_sib = 0;
	int sib_index = 0;
	int sib_scale = 0;
	int sib_base = 0;

	uint8_t op_ext = 0;

	int has_rel32 = 0;
	uint32_t rel = 0;

	for (int i = 0, j = 0; i < 4 && j < 4; i++, j++) {
		struct operand *o = ops + i;
		struct operand_encoding *oe = encoding->operand_encoding + j;

		if ((o->type == OPERAND_EMPTY) != (oe->type == OE_EMPTY))
			ICE("Invalid number of arguments to instruction");

		if (o->type == OPERAND_REG &&
			needs_rex(o->reg.reg, o->reg.upper_byte, o->reg.size))
			has_rex = 1;

		switch (oe->type) {
		case OE_IMM8:
			has_imm8 = 1;
			imm = o->imm;
			break;

		case OE_IMM16:
			has_imm16 = 1;
			imm = o->imm;
			break;

		case OE_IMM32:
			has_imm32 = 1;
			imm = o->imm;
			break;

		case OE_IMM64:
			has_imm64 = 1;
			imm = o->imm;
			break;

		case OE_MODRM_RM:
			has_modrm = 1;
			switch (o->type) {
			case OPERAND_REG:
			case OPERAND_STAR_REG:
				modrm_mod = 3;

				modrm_rm = register_index(o->reg.reg);
				if (has_rex)
					rex_b = (modrm_rm & 0x8) >> 3;
				break;

			case OPERAND_SSE_REG:
				modrm_mod = 3;

				modrm_rm = o->sse_reg;
				if (has_rex)
					rex_b = (modrm_rm & 0x8) >> 3;
				break;

			case OPERAND_MEM: {
				encode_sib(o, &rex_b, &rex_x, &modrm_mod, &modrm_rm,
						   &disp, &has_disp8, &has_disp32,
						   &has_sib, &sib_scale, &sib_index, &sib_base);
			} break;

			default:
				ICE("Not imp: %d\n", o->type);
				NOTIMP();
			}
			break;

		case OE_MODRM_REG:
			switch (o->type) {
			case OPERAND_REG:
				assert(encoding->slash_r);
				modrm_reg = register_index(o->reg.reg);
				rex_r = (modrm_reg & 0x8) >> 3;
				break;

			case OPERAND_SSE_REG:
				assert(encoding->slash_r);
				modrm_reg = o->sse_reg;
				rex_r = (modrm_reg & 0x8) >> 3;
				break;

			default:
				ICE("Not imp: %d\n", o->type);
			}
			break;

		case OE_OPEXT:
			op_ext = register_index(o->reg.reg);
			break;

		case OE_NONE:
			break;

		case OE_EMPTY:
			break;

		case OE_REL32:
			has_rel32 = 1;
			rel = o->imm;
			break;

		default:
			ICE("Not imp: %d, %d", oe->type, i);
		}

		if (oe->duplicate)
			i--;
	}

	if (rex_b || rex_r) {
		has_rex = 1;
	}

	int idx = 0;

#define W(X) output[idx++] = (X)
#define WRITE_8(X) do { W(X); } while (0)
#define WRITE_16(X) do { W(X); W(X >> 8); } while (0)
#define WRITE_32(X) do { W(X); W(X >> 8); W(X >> 16); W(X >> 24); } while (0)
#define WRITE_64(X) do { W(X); W(X >> 8); W(X >> 16); W(X >> 24);\
		W(X >> 32); W(X >> 40); W(X >> 48); W(X >> 56);} while (0)

	if (encoding->op_size_prefix)
		WRITE_8(0x66);

	if (encoding->repne_prefix)
		WRITE_8(0xf2);

	if (encoding->repe_prefix)
		WRITE_8(0xf3);

	if (has_rex) {
		uint8_t rex_byte = 0x40;

		if (encoding->rexw)
			rex_byte |= 0x8;

		rex_byte |= rex_b;
		rex_byte |= rex_r << 2;

		WRITE_8(rex_byte);
	}

	WRITE_8(encoding->opcode | op_ext);
	if (encoding->opcode == 0x0f) {
		WRITE_8(encoding->op2);
		if (encoding->op2 == 0x38 || encoding->op2 == 0x3a)
			WRITE_8(encoding->op3);
	}

	if (has_modrm) {
		uint8_t modrm_byte = 0;
		modrm_byte |= (encoding->modrm_extension & 0x7) << 3;
		modrm_byte |= (modrm_reg & 0x7) << 3;
		modrm_byte |= (modrm_mod & 0x3) << 6;
		modrm_byte |= (modrm_rm & 0x7);

		WRITE_8(modrm_byte);
	}

	if (has_sib) {
		uint8_t sib_byte = 0;

		sib_byte |= (sib_scale & 3) << 6;
		sib_byte |= (sib_index & 7) << 3;
		sib_byte |= (sib_base & 7);

		WRITE_8(sib_byte);
	}

	if (has_disp8)
		WRITE_8(disp);
	else if (has_disp32)
		WRITE_32(disp);

	if (has_imm8)
		WRITE_8(imm);
	else if (has_imm16)
		WRITE_16(imm);
	else if (has_imm32)
		WRITE_32(imm);
	else if (has_imm64)
		WRITE_64(imm);

	if (has_rel32)
		WRITE_32(rel);

	*len = idx;
}

int does_match(struct operand *o, struct operand_accepts *oa) {
	switch (oa->type) {
	case ACC_RAX:
		if (o->type != OPERAND_REG)
			return 0;
		if (o->reg.size != oa->reg.size)
			return 0;
		if (o->reg.reg != REG_RAX)
			return 0;
		break;

	case ACC_RCX:
		if (o->type != OPERAND_REG)
			return 0;
		if (o->reg.size != oa->reg.size)
			return 0;
		if (o->reg.reg != REG_RCX)
			return 0;
		break;

	case ACC_REG:
		if (o->type != OPERAND_REG)
			return 0;
		if (o->reg.size != oa->reg.size)
			return 0;
		break;

	case ACC_REG_STAR:
		if (o->type != OPERAND_STAR_REG)
			return 0;
		if (o->reg.size != oa->reg.size)
			return 0;
		break;

	case ACC_XMM:
		if (o->type != OPERAND_SSE_REG)
			return 0;
		break;

	case ACC_EMPTY:
		if (o->type != OPERAND_EMPTY)
			return 1;
		break;

	case ACC_IMM8_S: {
		long s = o->imm;
		if (o->type != OPERAND_IMM)
			return 0;
		if (s < INT8_MIN || s > INT8_MAX)
			return 0;
	} break;

	case ACC_IMM8_U: {
		long s = o->imm;
		if (o->type != OPERAND_IMM)
			return 0;
		if (s > UINT8_MAX)
			return 0;
	} break;

	case ACC_IMM16_S: {
		long s = o->imm;
		if (o->type != OPERAND_IMM)
			return 0;
		if (s < INT16_MIN || s > INT16_MAX)
			return 0;
	} break;

	case ACC_IMM16_U: {
		long s = o->imm;
		if (o->type != OPERAND_IMM)
			return 0;
		if (s > UINT16_MAX)
			return 0;
	} break;

	case ACC_IMM32_S: {
		long s = o->imm;
		if (o->type != OPERAND_IMM)
			return 0;
		if (s < INT32_MIN || s > INT32_MAX)
			return 0;
	} break;

	case ACC_IMM32_U: {
		if (o->type != OPERAND_IMM)
			return 0;
		if (o->imm > UINT32_MAX)
			return 0;
	} break;

	case ACC_MODRM:
		// Either accept register of correct size or SIB.
		if (o->type == OPERAND_REG) {
			if (o->reg.size != oa->reg.size)
				return 0;
		} else if (o->type == OPERAND_MEM) {
		} else {
			return 0;
		}
		break;

	case ACC_XMM_M128:
	case ACC_XMM_M64:
	case ACC_XMM_M32: // TODO: Differentiate M64 and M32?
		// Either accept SSE register of correct size or SIB.
		if (!(o->type == OPERAND_SSE_REG ||
			  o->type == OPERAND_MEM))
			return 0;
		break;

	case ACC_IMM64:
		if (o->type != OPERAND_IMM)
			return 0;
		break;

	case ACC_REL32:
		/* if (o->type != OPERAND_IMM_ABSOLUTE) */
		/* 	return 0; */
		break;

	default:
		ICE("Not imp: %d\n", oa->type);
	}

	return 1;
}

void assemble_instruction(uint8_t *output, int *len, const char *mnemonic, struct operand ops[4]) {
	int best_len = 16;
	uint8_t best_output[15] = { 0 };
	// TODO: Just order the instructions in such a way that we can just take the first one that appears.
	for (unsigned i = 0; i < sizeof encodings / sizeof *encodings; i++) {
		struct encoding *encoding = encodings + i;

		if (strcmp(encoding->mnemonic, mnemonic) != 0)
			continue;

		(void)encoding;
		int matches = 1;
		for (int j = 0; j < 4; j++) {
			struct operand *o = ops + j;
			struct operand_accepts *oa = encoding->operand_accepts + j;
			matches = does_match(o, oa);

			if (!matches)
				break;
		}

		if (!matches)
			continue;

		uint8_t current_output[15];
		int current_len;
		assemble_encoding(current_output, &current_len, encoding, ops);

		if (current_len < best_len) {
			memcpy(best_output, current_output, sizeof (best_output));
			best_len = current_len;
		}
	}

	if (best_len == 16) {
		*len = -1;
		return;
	}

	*len = best_len;
	memcpy(output, best_output, sizeof (best_output));
}
