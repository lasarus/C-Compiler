#ifndef LINKER_H
#define LINKER_H

#include "object.h"

#include <stdint.h>

struct segment {
	size_t size, cap;
	uint8_t *data;

	size_t load_address, load_size;

	int executable, writable, readable;
};

struct executable {
	size_t entry;

	size_t segment_size, segment_cap;
	struct segment *segments;
};

struct executable *linker_link(int n_objects, struct object *objects);

#endif
