#include "precedence.h"

// Taken from https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B
// Not used any more in the expression parser, but still used in
// preprocessor.

int precedence_get(enum ttype token_type,
				   enum prec_part part,
				   int loop,
				   int in_function) {
	switch (part) {
	case PREC_POSTFIX:
		return 18;
		break;

	case PREC_PREFIX:
		return 17;
		break;

	case PREC_INFIX:
		switch (token_type) {
		case T_INC:
		case T_DEC:
		case T_LPAR:
		case T_LBRACK:
		case T_ARROW:
		case T_DOT:
			return 18; // It is really a postfix.
		case T_STAR:
		case T_DIV:
		case T_MOD:
			return 16;
		case T_ADD:
		case T_SUB:
			return 15;
		case T_LSHIFT:
		case T_RSHIFT:
			return 14;
		case T_LEQ:
		case T_L:
		case T_G:
		case T_GEQ:
			return 13;
		case T_EQ:
		case T_NEQ:
			return 12;
		case T_AMP:
			return 11;
		case T_XOR:
			return 10;
		case T_BOR:
			return 9;
		case T_AND:
			return 8;
		case T_OR:
			return 7;
		case T_A:
		case T_ADDA:
		case T_SUBA:
		case T_MULA:
		case T_DIVA:
		case T_MODA:
		case T_LSHIFTA:
		case T_RSHIFTA:
		case T_BORA:
		case T_XORA:
		case T_BANDA:
		case T_QUEST:
			return 6 - (loop ? 0 : 1);
		case T_COMMA:
			return in_function ? 0 : 5;
		default:
			return -1;
		}
	}

	return 0;
}
