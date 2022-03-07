#ifndef ELF_H
#define ELF_H

#include "object.h"

void elf_write(const char *path, struct object *object);

#endif
