#ifndef SPLITTER_H
#define SPLITTER_H

#include "preprocessor.h"

struct token splitter_next(void);
struct token splitter_next_unexpanded(void);
struct token splitter_next_translate(void);

void splitter_keywords(int b);

#endif
