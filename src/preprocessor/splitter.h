#ifndef SPLITTER_H
#define SPLITTER_H

#include "preprocessor.h"

struct token splitter_next();
struct token splitter_next_unexpanded();
struct token splitter_next_translate();

void splitter_keywords(int b);

#endif
