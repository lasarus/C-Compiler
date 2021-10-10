#ifndef PRECEDENCE_H
#define PRECEDENCE_H

#include "preprocessor/preprocessor.h"

#define PREFIX_PREC 13
#define ASSIGNMENT_PREC 1

int precedence_get(enum ttype token_type, int loop);

#endif
