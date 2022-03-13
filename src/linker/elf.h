#ifndef ELF_H
#define ELF_H

#include "object.h"
#include "linker.h"

void elf_write_object(const char *path, struct object *object);
void elf_write_executable(const char *path, struct executable *executable);

struct object *elf_read_object(const char *path);

#endif
