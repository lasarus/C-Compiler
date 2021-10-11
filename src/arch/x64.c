#include "x64.h"
#include "../codegen/codegen.h"

#include <common.h>

#include <assert.h>
#include <string.h>
#include <limits.h>

int sizeof_simple(enum simple_type type) {
	switch (type) {
	case ST_BOOL:
	case ST_CHAR:
	case ST_SCHAR:
	case ST_UCHAR:
		return 1;

	case ST_SHORT:
	case ST_USHORT:
		return 2;
	case ST_INT:
	case ST_UINT:
		return 4;

	case ST_LONG:
	case ST_ULONG:
	case ST_LLONG:
	case ST_ULLONG:
		return 8;

	case ST_FLOAT:
		return 4;
	case ST_DOUBLE:
		return 8;
	case ST_LDOUBLE:
		return 16;

	case ST_VOID:
		return 0;

	case ST_FLOAT_COMPLEX:
	case ST_DOUBLE_COMPLEX:
	case ST_LDOUBLE_COMPLEX:
		NOTIMP();
		return -1;

	default:
		return -1;
		//ERROR("Invalid type");
	}
}

int alignof_simple(enum simple_type type) {
	return sizeof_simple(type);
}

int is_signed(enum simple_type type) {
	switch(type) {
	case ST_CHAR:
		return IS_CHAR_SIGNED;

	case ST_INT:
	case ST_SCHAR:
	case ST_SHORT:
	case ST_LONG:
	case ST_LLONG:
		return 1;

	case ST_UINT:
	case ST_UCHAR:
	case ST_USHORT:
	case ST_ULONG:
	case ST_ULLONG:
	case ST_BOOL:
		return 0;

	default:
		ERROR("Type has no signedness");
	}
}

struct range {
	// These represent exponents
	// such that the range
	// becomes
	// [-2^min, 2^max-1]
	// If min == 0
	// then the range is
	// [0, 2^max - 1]
	int min, max;
};
// 
struct range get_range(enum simple_type type) {
	switch (type) {
	case ST_CHAR:
		if (IS_CHAR_SIGNED)
			return get_range(ST_SCHAR);
		else
			return get_range(ST_UCHAR);

	case ST_SCHAR:
		return (struct range){7, 7};
	case ST_SHORT:
		return (struct range){15, 15};
	case ST_INT:
		return (struct range){31, 31};
	case ST_LONG:
	case ST_LLONG:
		return (struct range){63, 63};
	case ST_UCHAR:
		return (struct range){0, 8};
	case ST_UINT:
		return (struct range){0, 16};
	case ST_USHORT:
		return (struct range){0, 32};
	case ST_ULONG:
	case ST_ULLONG:
		return (struct range){0, 64};

	default:
		ERROR("Type has no integer range");
	}
}

int is_contained_in(enum simple_type large,
					enum simple_type small) {
	struct range large_range = get_range(large),
		small_range = get_range(small);

	return large_range.min >= small_range.min &&
		large_range.max >= small_range.max;
		
}


enum simple_type to_unsigned(enum simple_type type) {
	switch (type) {
	case ST_CHAR:
	case ST_SCHAR:
		return ST_UCHAR;
	case ST_INT:
		return ST_UINT;
	case ST_LONG:
		return ST_ULONG;
	case ST_LLONG:
		return ST_ULLONG;
	default:
		return type;
	}
}

int type_register(struct type *type) {
	switch (type->type) {
	case TY_SIMPLE:
	case TY_POINTER:
		return 1;

	case TY_STRUCT:
		return 0;

	default:
		ERROR("Not implemented");
	}
}

int type_rank(enum simple_type t1) {
	switch (t1) {
	case ST_UCHAR:
	case ST_CHAR:
	case ST_SCHAR:
		return 1;
	case ST_SHORT:
	case ST_USHORT:
		return 2;
	case ST_INT:
	case ST_UINT:
		return 3;
	case ST_LONG:
	case ST_ULONG:
		return 4;
	case ST_LLONG:
	case ST_ULLONG:
		return 4;
	default:
		ERROR("NOt imp %d\n", t1);
		NOTIMP();
	}
}

struct type *usual_arithmetic_conversion(struct type *a,
										 struct type *b) {
	assert(a->type == TY_SIMPLE);
	assert(b->type == TY_SIMPLE);

	enum simple_type a_s = a->simple,
		b_s = b->simple;

	if (type_is_floating(a))
		NOTIMP();

	// TODO: Make this more standard.
	// 6.3.1.1p2
	if (type_rank(a_s) < type_rank(ST_INT) &&
		type_rank(b_s) < type_rank(ST_INT))
		return type_simple(ST_INT);

	// 6.3.1.8
	if (a == b)
		return a;

	if (is_signed(a_s) == is_signed(b_s)) {
		if (type_rank(a_s) > type_rank(b_s))
			return a;
		return b;
	}

	if (!is_signed(a_s) &&
		type_rank(a_s) >= type_rank(b_s)) {
		return a;
	}

	if (!is_signed(b_s) &&
		type_rank(b_s) >= type_rank(a_s)) {
		return b;
	}

	if (is_signed(a_s) &&
		is_contained_in(a_s, b_s)) {
		return a;
	}

	if (is_signed(b_s) &&
		is_contained_in(b_s, a_s)) {
		return b;
	}

	NOTIMP();
	// Promote to integer if not already integer.

	return NULL;
}

int is_scalar(struct type *type) {
	return type->type == TY_SIMPLE ||
		type->type == TY_POINTER;
}

int alignof_struct(struct struct_data *struct_data) {
	int max_align = 0;

	if (!struct_data->is_complete)
		ERROR("Struct %s not complete", struct_data->name);

	for (int i = 0; i < struct_data->n; i++) {
		struct type *type = struct_data->types[i];
		if (max_align < calculate_alignment(type))
			max_align = calculate_alignment(type);
	}

	return max_align;
}

int calculate_alignment(struct type *type) {
	switch (type->type) {
	case TY_SIMPLE:
		return alignof_simple(type->simple);
		break;

	case TY_STRUCT:
		return alignof_struct(type->struct_data);
		break;

	case TY_POINTER:
		return alignof_simple(ST_ULONG);
		break;

	case TY_FUNCTION:
		return -1;

	case TY_ARRAY:
		return calculate_alignment(type->children[0]);

	case TY_INCOMPLETE_ARRAY:
		return -1;

	default:
		NOTIMP();
	}
}

int calculate_size(struct type *type) {
	switch (type->type) {
	case TY_SIMPLE:
		return sizeof_simple(type->simple);

	case TY_STRUCT:
		return type->struct_data->size;

	case TY_POINTER:
		return sizeof_simple(ST_ULONG);

	case TY_FUNCTION:
		return -1;

	case TY_ARRAY:
		return type->array.length * calculate_size(type->children[0]);

	case TY_INCOMPLETE_ARRAY:
		return -1;

	case TY_VARIABLE_LENGTH_ARRAY:
		NOTIMP();
		break;

	default:
		NOTIMP();
	}
}

int calculate_offset(struct type *type, int index) {
	switch (type->type) {
	case TY_STRUCT:
		return type->struct_data->offsets[index];
	case TY_ARRAY:
	case TY_INCOMPLETE_ARRAY: // flexible array.
		return calculate_size(type->children[0]) * index;
		break;
	default:
		ERROR("Not imp: %d", type->type);
	}
}

void calculate_offsets(struct struct_data *data) {
	int current_offset = 0;
	int max_offset = 0;
	int alignment = 0;

	data->offsets = malloc(sizeof *data->offsets * data->n);
	for (int i = 0; i < data->n; i++) {
		struct type *field = data->types[i];
		current_offset = round_up_to_nearest(current_offset, calculate_alignment(field));
		data->offsets[i] = current_offset;

		int new_size = 0;
		if (i == data->n - 1 &&
			field->type == TY_INCOMPLETE_ARRAY) {
			new_size = 0;
		} else {
			new_size = calculate_size(field);
			if (new_size == -1)
				ERROR("%s has invalid size", type_to_string(field));
		}

		if (calculate_alignment(field) > alignment)
			alignment = calculate_alignment(field);
		if (!data->is_union) {
			current_offset += new_size;
			max_offset = current_offset;
		} else {
			if (new_size > max_offset)
				max_offset = new_size;
		}
	}
	data->size = round_up_to_nearest(max_offset, alignment);
	data->alignment = alignment;
}

/* // Constant */
struct constant constant_increment(struct constant a) {
	switch (a.type) {
	case CONSTANT_TYPE:
		switch(a.data_type->type) {
		case TY_SIMPLE:
			switch (a.data_type->simple) {
			case ST_INT:
				a.int_d++;
				return a;
			default:
				NOTIMP();
			}
			break;
		default:
			NOTIMP();
		} break;

	default:
		NOTIMP();
	}
}

struct constant constant_zero(struct type *type) {
	return (struct constant) {
		.type = CONSTANT_TYPE,
		.data_type = type
		// Rest will default to zero.
	};
}

struct constant constant_one(struct type *type) {
	return constant_increment(constant_zero(type));
}

struct constant constant_from_string(const char *str) {
	unsigned long long parsed = 0;
	const char *start = str;

	int is_hex = str[0] == '0' && str[1] == 'x';
	if (is_hex)
		start += 2;

	if (!is_hex && str[0] == '0' && str[1] >= '0' && str[1] <= '9')
		ERROR("Octals are not implemented yet");

	for (; *start; start++) {
		int decimal_digit = (*start >= '0' && *start <= '9');
		int low_hex_digit = (*start >= 'a' && *start <= 'f');
		int high_hex_digit = (*start >= 'A' && *start <= 'F');
		if (!decimal_digit &&
			(!is_hex || !(low_hex_digit || high_hex_digit)))
			break;

		parsed *= is_hex ? 16 : 10;

		if (decimal_digit)
			parsed += *start - '0';
		else if (low_hex_digit)
			parsed += *start - 'a' + 10;
		else if (high_hex_digit)
			parsed += *start - 'A' + 10;
	}

	int allow_unsigned = is_hex;
	int allow_signed = 1;
	int allow_int = 1;
	int allow_long = 1;
	int allow_llong = 1;

	while (*start) {
		if ((start[0] == 'l' && start[1] == 'l') ||
			(start[0] == 'L' && start[1] == 'L')) {
			start += 2;
			allow_int = allow_long = 0;
		} else if (start[0] == 'l' || start[0] == 'L') {
			start += 1;
			allow_int = 0;
		} else if (start[0] == 'u' || start[0] == 'U') {
			start += 1;
			allow_unsigned = 1;
			allow_signed = 0;
		}
	}

	struct constant res = { .type = CONSTANT_TYPE };

	if (parsed <= INT_MAX && allow_int && allow_signed) {
		res.data_type = type_simple(ST_INT);
		res.int_d = parsed;
	} else if (parsed <= UINT_MAX && allow_int && allow_unsigned) {
		res.data_type = type_simple(ST_UINT);
		res.uint_d = parsed;
	} else if (parsed <= LONG_MAX && allow_long && allow_signed) {
		res.data_type = type_simple(ST_LONG);
		res.long_d = parsed;
	} else if (parsed <= ULONG_MAX && allow_long && allow_unsigned) {
		res.data_type = type_simple(ST_ULONG);
		res.ulong_d = parsed;
	} else if (parsed <= LLONG_MAX && allow_llong && allow_signed) {
		res.data_type = type_simple(ST_LLONG);
		res.llong_d = parsed;
	} else if (parsed <= ULLONG_MAX && allow_llong && allow_unsigned) {
		res.data_type = type_simple(ST_ULLONG);
		res.ullong_d = parsed;
	} else {
		ERROR("Cant fit %llu (%s) into any type", parsed, str);
	}

	return res;
}

struct constant simple_cast(struct constant from, enum simple_type target) {
	assert(from.type == CONSTANT_TYPE);
	assert(from.data_type->type == TY_SIMPLE);

#define CONVERT_TO(NAME)\
		switch (from.data_type->simple) {\
		case ST_VOID: break;							\
		case ST_CHAR: from.NAME = from.char_d; break;\
		case ST_UCHAR: from.NAME = from.uchar_d; break;\
		case ST_SCHAR: from.NAME = from.schar_d; break;\
		case ST_SHORT: from.NAME = from.short_d; break;\
		case ST_USHORT: from.NAME = from.ushort_d; break;\
		case ST_INT: from.NAME = from.int_d; break;\
		case ST_UINT: from.NAME = from.uint_d; break;\
		case ST_LONG: from.NAME = from.long_d; break;\
		case ST_ULONG: from.NAME = from.ulong_d; break;\
		case ST_LLONG: from.NAME = from.llong_d; break;\
		case ST_ULLONG: from.NAME = from.ullong_d; break;\
		default: ERROR("Trying to convert from %s to %s", strdup(type_to_string(from.data_type)), strdup(type_to_string(type_simple(target)))); \
		}\

	switch (target) {
	case ST_VOID: break;
	case ST_CHAR: CONVERT_TO(char_d); break;
	case ST_SCHAR: CONVERT_TO(schar_d); break;
	case ST_UCHAR: CONVERT_TO(uchar_d); break;
	case ST_SHORT: CONVERT_TO(short_d); break;
	case ST_USHORT: CONVERT_TO(ushort_d); break;
	case ST_INT: CONVERT_TO(int_d); break;
	case ST_UINT: CONVERT_TO(uint_d); break;
	case ST_LONG: CONVERT_TO(long_d); break;
	case ST_ULONG: CONVERT_TO(ulong_d); break;
	case ST_LLONG: CONVERT_TO(llong_d); break;
	case ST_ULLONG: CONVERT_TO(ullong_d); break;
	default: ERROR("NOTIMP: %d\n", target);
	}
	from.data_type = type_simple(target);
	return from;
}

struct constant constant_cast(struct constant a, struct type *target) {
	if (a.type == CONSTANT_LABEL) {
		if (type_is_pointer(a.data_type) &&
			type_is_pointer(target)) {
			a.data_type = target;
			return a;
		} else {
			NOTIMP();
		}
		return a;
	}

	if (a.data_type == target) {
		return a;
	}

	if (type_is_pointer(target) &&
		type_is_pointer(a.data_type)) {
		a.data_type = target;
		return a;
	}

	if (type_is_pointer(target) && type_is_simple(a.data_type, ST_INT)) {
		a.data_type = target;
		a.long_d = a.int_d;
		return a;
	}

	if (target->type == TY_SIMPLE &&
		a.data_type->type == TY_SIMPLE) {
		return simple_cast(a, target->simple);
	}

	printf("Trying to cast from %s to %s\n",
		   strdup(type_to_string(a.data_type)),
		   strdup(type_to_string(target)));

	NOTIMP();
}
