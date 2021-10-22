#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "preprocessor.h"

#include <stdio.h>

void tokenizer_push_input(const char *rel_path);
void tokenizer_push_input_absolute(const char *path);

void tokenizer_disable_current_path(void);

struct token tokenizer_next(void);
void set_header(int i);

#endif
