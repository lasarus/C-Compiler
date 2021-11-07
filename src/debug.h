#ifndef DEBUG_H
#define DEBUG_H

#include "types.h"
#include "parser/parser.h"
#include "preprocessor/preprocessor.h"

struct token;
struct type;

const char *dbg_type(struct type *type);
const char *dbg_instruction(struct instruction ins);
const char *dbg_token(struct token *t);
const char *dbg_token_type(enum ttype tt);

#endif
