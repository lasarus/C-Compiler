#include "precedence.h"

// Taken from https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B
static const int precedence_table[T_COUNT] = {
	[T_INC] = 18, [T_DEC] = 18, [T_LPAR] = 18, [T_LBRACK] = 18, [T_ARROW] = 18, [T_DOT] = 18,
	[T_STAR] = 16, [T_DIV] = 16, [T_MOD] = 16,
	[T_ADD] = 15, [T_SUB] = 15,
	[T_LSHIFT] = 14, [T_RSHIFT] = 14,
	[T_LEQ] = 13, [T_L] = 13, [T_G] = 13, [T_GEQ] = 13,
	[T_EQ] = 12, [T_NEQ] = 12,
	[T_AMP] = 11,
	[T_XOR] = 10,
	[T_BOR] = 9,
	[T_AND] = 8,
	[T_OR] = 7,
	[T_A] = 6, [T_ADDA] = 6, [T_SUBA] = 6, [T_MULA] = 6,
	[T_DIVA] = 6, [T_MODA] = 6, [T_LSHIFTA] = 6, [T_RSHIFTA] = 6,
	[T_BORA] = 6, [T_XORA] = 6, [T_BANDA] = 6, [T_QUEST] = 6,
	[T_COMMA] = 5,
};

int precedence_get(enum ttype token_type, int loop) {
	int prec = precedence_table[token_type];
	if (!loop && prec == 6)
		return 5;
	return prec;
}
