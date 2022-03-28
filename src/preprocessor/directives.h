#ifndef DIRECTIVES_H
#define DIRECTIVES_H

#include "preprocessor.h"

struct token directiver_next(void);
void directiver_push_input(const char *path, int system);
void directiver_reset(void);

void directiver_write_dependencies(void);
void directiver_finish_writing_dependencies(const char *mt, const char *mf);

#endif
