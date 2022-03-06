#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "preprocessor.h"
#include "token_list.h"

struct token_list tokenize_input(struct input *input);

int parse_escape_sequence(struct string_view *string, uint32_t *character, struct position pos);

#endif
