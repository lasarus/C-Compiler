#ifndef PRECEDENCE_H
#define PRECEDENCE_H

#include "preprocessor/preprocessor.h"

// TODO: This is really a source of bugs....
enum prec_part {
	PREC_PREFIX,
	PREC_INFIX,
	PREC_POSTFIX
};

enum {
	POSTFIX_PREC = 18,
	PREFIX_PREC = 17
};

int precedence_get(enum ttype token_type,
				   enum prec_part part,
				   int loop,
				   int in_function);

#endif
