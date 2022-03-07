#ifndef ALLOC_H
#define ALLOC_H

#include <stdlib.h>

char *str_alloc(size_t len);
void str_alloc_init(void);
void str_alloc_free(void);

#endif
