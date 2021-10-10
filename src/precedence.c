#include "precedence.h"

// Taken from https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B
static const int precedence_table[T_COUNT] = {
	[T_INC] = 14, [T_DEC] = 14, [T_LPAR] = 14, [T_LBRACK] = 14, [T_ARROW] = 14, [T_DOT] = 14,
	[T_STAR] = 12, [T_DIV] = 12, [T_MOD] = 12,
	[T_ADD] = 11, [T_SUB] = 11,
	[T_LSHIFT] = 10, [T_RSHIFT] = 10,
	[T_LEQ] = 9, [T_L] = 9, [T_G] = 9, [T_GEQ] = 9,
	[T_EQ] = 8, [T_NEQ] = 8,
	[T_AMP] = 7,
	[T_XOR] = 6,
	[T_BOR] = 5,
	[T_AND] = 4,
	[T_OR] = 3,
	[T_A] = 2, [T_ADDA] = 2, [T_SUBA] = 2, [T_MULA] = 2,
	[T_DIVA] = 2, [T_MODA] = 2, [T_LSHIFTA] = 2, [T_RSHIFTA] = 2,
	[T_BORA] = 2, [T_XORA] = 2, [T_BANDA] = 2, [T_QUEST] = 2,
	[T_COMMA] = 1,
};

int precedence_get(enum ttype token_type, int loop) {
	int prec = precedence_table[token_type];
	if (!loop && prec == ASSIGNMENT_PREC + 1)
		return ASSIGNMENT_PREC;
	return prec;
}
