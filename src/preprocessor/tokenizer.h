#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "token_list.h"

struct token_list tokenize_input(const char *contents, const char *path);

#endif
