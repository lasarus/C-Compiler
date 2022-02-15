#ifndef STRING_CONCAT_H
#define STRING_CONCAT_H

#include "preprocessor.h"

struct token string_concat_next(void);

// Used in directives.c to get integer values of
// non-escaped character constants.
intmax_t escaped_character_constant_to_int(struct token t);

#endif
