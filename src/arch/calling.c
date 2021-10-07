#include "calling.h"

#include <common.h>

int classify_non_recursive(struct type *type, enum parameter_class *class) {
	if (type_is_pointer(type)) {
		*class = CLASS_INTEGER;
	} else if (type->type == TY_SIMPLE) {
		switch (type->simple) {
		case ST_CHAR: case ST_SCHAR: case ST_UCHAR:
		case ST_INT: case ST_UINT:
		case ST_LONG: case ST_LLONG:
		case ST_ULONG: case ST_ULLONG:
		case ST_USHORT: case ST_SHORT:
			*class = CLASS_INTEGER;
			break;
		default:
			NOTIMP();
		}
	} else {
		return 0;
	}
	return 1;
}

enum parameter_class combine_class(enum parameter_class a, enum parameter_class b) {
	if (a == b)
		return a;
	if (a == CLASS_NO_CLASS)
		return b;
	if (b == CLASS_NO_CLASS)
		return a;
	if (a == CLASS_MEMORY || b == CLASS_MEMORY)
		return CLASS_MEMORY;
	if (a == CLASS_INTEGER || b == CLASS_INTEGER)
		return CLASS_INTEGER;
	if (a == CLASS_X87 || a == CLASS_X87UP || a == CLASS_COMPLEX_X87)
		return CLASS_MEMORY;
	if (b == CLASS_X87 || b == CLASS_X87UP || b == CLASS_COMPLEX_X87)
		return CLASS_MEMORY;
	return CLASS_SSE;
}

void classify_recursively(enum parameter_class *current,
						  struct type *type, int lower, int upper) {
	switch (type->type) {
	case TY_STRUCT: {
		struct struct_data *data = type->struct_data;
		for (int i = 0; i < data->n; i++) {
			int offset = data->offsets[i];
			struct type *memb_type = data->types[i];
			if (!(offset >= lower && offset < upper))
				continue;

			enum parameter_class memb_class;
			if (classify_non_recursive(memb_type, &memb_class)) {
				*current = combine_class(*current, memb_class);
			} else {
				classify_recursively(current, memb_type, lower - offset, upper - offset);
			}
		}
	} break;

	case TY_ARRAY: {
		for (int i = 0; i < type->array.length; i++) {
			struct type *memb_type = type->children[0];
			int offset = calculate_size(memb_type) * i;
			if (!(offset >= lower && offset < upper))
				continue;

			enum parameter_class memb_class;
			if (classify_non_recursive(memb_type, &memb_class)) {
				*current = combine_class(*current, memb_class);
			} else {
				classify_recursively(current, memb_type, lower - offset, upper - offset);
			}
		}
	} break;

	default:
		ERROR("Not imp %d\n", type->type);
		NOTIMP();
	}
}

void classify(struct type *type, int *n_parts, enum parameter_class *classes) {
	if (classify_non_recursive(type, classes)) {
		*n_parts = 1;
	} else if (type->type == TY_STRUCT) {
		int size = calculate_size(type);

		// post cleanup:

		if (size > 4 * 8) {
			*n_parts = 1;
			classes[0] = CLASS_MEMORY;
		} else {
			*n_parts = round_up_to_nearest(size, 8) / 8;
			for (int i = 0; i < *n_parts; i++) {
				classes[i] = CLASS_NO_CLASS;
				classify_recursively(&classes[i], type, i * 8, (i + 1) * 8);
			}

			int should_be_mem = 0;
			for (int i = 0; i < *n_parts; i++) {
				if (classes[i] == CLASS_MEMORY)
					should_be_mem = 1;
				if (classes[i] == CLASS_X87UP &&
					!(i > 0 && classes[i - 1] == CLASS_X87))
					should_be_mem = 1;
			}

			if (*n_parts > 2) {
				if (classes[0] != CLASS_SSE)
					should_be_mem = 1;
				// TODO: Is more to this, but I couldn't really follow the logic
				// in the ABI. 3.2.3 Post merger cleanum part (c).
			}

			if (should_be_mem) {
				*n_parts = 1;
				classes[0] = CLASS_MEMORY;
			}
		}
	} else {
		printf("Can't classify type: %s", type_to_string(type));
	}
}
