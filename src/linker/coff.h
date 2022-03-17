#ifndef COFF_H
#define COFF_H

#include "object.h"

void coff_write_object(const char *path, struct object *object);

#endif
