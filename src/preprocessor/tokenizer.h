#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "preprocessor.h"
#include "token_list.h"

#include <stdio.h>

void tokenizer_push_input(const char *path);

void tokenizer_disable_current_path(void);

struct token tokenizer_next(void);

struct token_list tokenizer_whole(struct input *new_input);

#endif
