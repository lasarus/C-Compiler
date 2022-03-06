#ifndef DIRECTIVES_H
#define DIRECTIVES_H

#include "preprocessor.h"
struct token directiver_next(void);
void directiver_push_input(const char *path, int system);

#endif
