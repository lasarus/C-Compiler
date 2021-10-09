#ifndef PRECEDENCE_H
#define PRECEDENCE_H

#include "preprocessor/preprocessor.h"

enum {
	POSTFIX_PREC = 18,
	PREFIX_PREC = 17
};

int precedence_get(enum ttype token_type, int loop);

#endif
