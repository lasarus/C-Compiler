#ifndef CALLING_H
#define CALLING_H

#include <types.h>

enum parameter_class {
	CLASS_INTEGER,
	CLASS_SSE,
	CLASS_SSEUP,
	CLASS_X87,
	CLASS_X87UP,
	CLASS_COMPLEX_X87,
	CLASS_NO_CLASS,
	CLASS_MEMORY
};

void classify(struct type *type, int *n_parts, enum parameter_class *classes);

#endif
