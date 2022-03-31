#include "x64.h"
#include "../codegen/codegen.h"

#include <common.h>
#include <abi/abi.h>

#include <assert.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>

static int alignof_simple(enum simple_type type) {
	return abi_sizeof_simple(type);
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
		ICE("Type has no signedness");
	}
}

struct range {
	// These represent exponents
	// such that the range
	// becomes
	// [-2^min, 2^max-1]
	// If min == -1
	// then the range is
	// [0, 2^max - 1]
	int min, max;
};

struct range get_range(enum simple_type type, int bitfield) {
	int n_bits = bitfield == -1 ? abi_sizeof_simple(type) * 8 : bitfield;

	if (is_signed(type))
		return (struct range) { n_bits - 1, n_bits - 1};
	else
		return (struct range) { -1, n_bits};
}

static constant_uint get_type_max(enum simple_type st) {
	struct range r = get_range(st, -1);
	return r.max == -1 ? 0 : (((constant_uint)1 << (r.max - 1)) - 1
							  + ((constant_uint)1 << (r.max - 1)));
}

int is_contained_in(enum simple_type large, int large_bitfield,
					enum simple_type small, int small_bitfield) {
	struct range large_range = get_range(large, large_bitfield),
		small_range = get_range(small, small_bitfield);

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
		NOTIMP();
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
		return 5;
	default:
		NOTIMP();
	}
}

int is_scalar(struct type *type) {
	return type->type == TY_SIMPLE ||
		type->type == TY_POINTER;
}

static int calculate_alignment(struct type *type);

int alignof_struct(struct struct_data *struct_data) {
	int max_align = 0;

	if (!struct_data->is_complete)
		ICE("Struct %.*s not complete", struct_data->name.len, struct_data->name.str);

	for (int i = 0; i < struct_data->n; i++) {
		struct type *type = struct_data->fields[i].type;
		if (max_align < calculate_alignment(type))
			max_align = calculate_alignment(type);
	}

	return max_align;
}

static int calculate_alignment(struct type *type) {
	switch (type->type) {
	case TY_SIMPLE:
		return alignof_simple(type->simple);

	case TY_STRUCT:
		return alignof_struct(type->struct_data);

	case TY_POINTER:
		return alignof_simple(abi_info.pointer_type);

	case TY_FUNCTION:
		return -1;

	case TY_ARRAY:
	case TY_VARIABLE_LENGTH_ARRAY:
	case TY_INCOMPLETE_ARRAY:
		return calculate_alignment(type->children[0]);

	default:
		NOTIMP();
	}
}

int calculate_size(struct type *type) {
	switch (type->type) {
	case TY_SIMPLE:
		return abi_sizeof_simple(type->simple);

	case TY_STRUCT:
		return type->struct_data->size;

	case TY_POINTER:
		return abi_sizeof_simple(abi_info.pointer_type);

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
		return type->struct_data->fields[index].offset;
	case TY_ARRAY:
	case TY_INCOMPLETE_ARRAY: // flexible array.
		return calculate_size(type->children[0]) * index;
		break;
	default:
		NOTIMP();
	}
}

void calculate_offsets(struct struct_data *data) {
	int current_offset = 0;
	int max_offset = 0;
	int alignment = 0;

	int last_bit_offset = 0;
	for (int i = 0; i < data->n; i++) {
		struct type *field = data->fields[i].type;
		if (!data->is_packed)
			current_offset = round_up_to_nearest(current_offset, calculate_alignment(field));
		data->fields[i].offset = current_offset;
		data->fields[i].bit_offset = 0;

		if (data->fields[i].bitfield != -1) {
			if (last_bit_offset != 0 && data->fields[i - 1].bitfield != 0)
				data->fields[i].offset = data->fields[i - 1].offset;

			int fits = 0;
			struct type *fit_type = NULL;
			for (int j = i; j >= 0; j--) {
				if (last_bit_offset != 0 && data->fields[j].offset == data->fields[i - 1].offset &&
					last_bit_offset + data->fields[i].bitfield <= 8 * calculate_size(data->fields[j].type)) {
					fits = 1;
					fit_type = data->fields[j].type;
					break;
				}
			}

			if (fits) {
				current_offset = data->fields[i].offset;
				data->fields[i].bit_offset = last_bit_offset;
				data->fields[i].type = fit_type;
			} else {
				if (!data->is_union) {
					current_offset = max_offset;

					if (!data->is_packed)
						current_offset = round_up_to_nearest(current_offset, calculate_alignment(field));
				}
				data->fields[i].offset = current_offset;
				last_bit_offset = 0;
			}

			if (!data->is_union)
				last_bit_offset += data->fields[i].bitfield;
		} else {
			last_bit_offset = 0;
		}

		int new_size = 0;
		if (i == data->n - 1 &&
			field->type == TY_INCOMPLETE_ARRAY) {
			new_size = 0;
		} else {
			new_size = calculate_size(field);
			if (new_size == -1)
				ICE("%s has invalid size", dbg_type(field));
		}

		if (calculate_alignment(field) > alignment)
			alignment = calculate_alignment(field);
		if (!data->is_union) {
			current_offset += new_size;
			if (current_offset > max_offset)
				max_offset = current_offset;
		} else {
			if (new_size > max_offset)
				max_offset = new_size;
		}
	}

	if (data->is_packed)
		alignment = 1;

	data->size = round_up_to_nearest(max_offset, alignment);
	data->alignment = alignment;
}

struct constant constant_increment(struct constant a) {
	if (a.type != CONSTANT_TYPE)
		NOTIMP();
	if (!(type_is_integer(a.data_type) && is_signed(a.data_type->simple)))
		NOTIMP();
	a.int_d++;
	return a;
}

// Basically if string is not of the format (0b|0B)?[0-9]+[ulUL]?
// or (0x|0X)[0-9a-fA-F]+[ulUL]?
int is_float(struct string_view str) {
	int i = 0;
	int is_hex = 0;
	if (str.len > 1 && str.str[0] == '0' && (str.str[1] == 'x' || str.str[1] == 'X')) {
		is_hex = 1;
		i += 2;
	} else if (str.len > 1 && str.str[0] == '0' && (str.str[1] == 'b' || str.str[1] == 'B')) {
		i += 2;
	}
	// [0-9]*
	for (; i < str.len; i++)
		if (!((str.str[i] >= '0' && str.str[i] <= '9') ||
			  (is_hex && str.str[i] >= 'a' && str.str[i] <= 'f') ||
			  (is_hex && str.str[i] >= 'A' && str.str[i] <= 'F')))
			break;

	// [ulUL]*
	for (; i < str.len; i++) {
		if (!(str.str[i] == 'u' || str.str[i] == 'U' ||
			  str.str[i] == 'l' || str.str[i] == 'L')) {
			return 1;
		}
	}

	return 0;
}

static struct constant floating_point_constant_from_string(struct string_view sv) {
	char *str = sv_to_str(sv);
	double d = strtod(str, NULL);
	char suffix = str[strlen(str) - 1];

	struct constant res = { .type = CONSTANT_TYPE };
	if (suffix == 'f' || suffix == 'F') {
		res.data_type = type_simple(ST_FLOAT);
		res.float_d = d;
	} else if (suffix == 'l' || suffix == 'L') {
		ICE("Constants of type long double is not supported by this compiler. %s", str);
	} else {
		res.data_type = type_simple(ST_DOUBLE);
		res.double_d = d;
	}

	return res;
}

static struct constant integer_constant_from_string(struct string_view str) {
	unsigned long long parsed = 0; // TODO: Avoid implementation defined behaviour.

	enum {
		BASE_DECIMAL,
		BASE_HEXADECIMAL,
		BASE_OCTAL,
		BASE_BINARY
	} base = BASE_DECIMAL;

	if (str.len > 0 && str.str[0] == '0') {
		if (str.len > 1 && (str.str[1] == 'x' || str.str[1] == 'X'))
			base = BASE_HEXADECIMAL;
		else if (str.len > 1 && (str.str[1] == 'b' || str.str[1] == 'B'))
			base = BASE_BINARY;
		else
			base = BASE_OCTAL;
	} else if (str.len > 0 && str.str[0] >= '1' && str.str[0] <= '9') {
		base = BASE_DECIMAL;
	} else {
		ICE("%.*s has unknown base encoding.", str.len, str.str);
	}

	int start = 0;
	if (base == BASE_HEXADECIMAL || base == BASE_BINARY)
		start += 2;

	for (; start < str.len; start++) {
		int binary_digit = (str.str[start] >= '0' && str.str[start] <= '1');
		int octal_digit = (str.str[start] >= '0' && str.str[start] <= '7');
		int decimal_digit = (str.str[start] >= '0' && str.str[start] <= '9');
		int low_hex_digit = (str.str[start] >= 'a' && str.str[start] <= 'f');
		int high_hex_digit = (str.str[start] >= 'A' && str.str[start] <= 'F');

		if (base == BASE_DECIMAL) {
			if (!decimal_digit)
				break;
			parsed *= 10;
		} else if (base == BASE_HEXADECIMAL) {
			if (!decimal_digit && !low_hex_digit && !high_hex_digit)
				break;
			parsed *= 16;
		} else if (base == BASE_OCTAL) {
			if (!octal_digit)
				break;
			parsed *= 8;
		} else if (base == BASE_BINARY) {
			if (!binary_digit)
				break;
			parsed *= 2;
		}

		if (decimal_digit)
			parsed += str.str[start] - '0';
		else if (low_hex_digit)
			parsed += str.str[start] - 'a' + 10;
		else if (high_hex_digit)
			parsed += str.str[start] - 'A' + 10;
	}

	int allow_unsigned = base == BASE_HEXADECIMAL || base == BASE_OCTAL || base == BASE_BINARY;
	int allow_signed = 1;
	int allow_int = 1;
	int allow_long = 1;
	int allow_llong = 1;

	while (start < str.len) {
		if (start + 1 < str.len &&
			((str.str[start] == 'l' && str.str[start + 1] == 'l') ||
			 (str.str[start] == 'L' && str.str[start + 1] == 'L'))) {
			start += 2;
			allow_int = allow_long = 0;
		} else if (str.str[start] == 'l' || str.str[start] == 'L') {
			start += 1;
			allow_int = 0;
		} else if (str.str[start] == 'u' || str.str[start] == 'U') {
			start += 1;
			allow_unsigned = 1;
			allow_signed = 0;
		} else {
			ICE("Invalid format of integer constant: %.*s", str.len, str.str);
		}
	}

	if (parsed <= get_type_max(ST_INT) && allow_int && allow_signed) {
		return constant_simple_signed(ST_INT, parsed);
	} else if (parsed <= get_type_max(ST_UINT) && allow_int && allow_unsigned) {
		return constant_simple_unsigned(ST_UINT, parsed);
	} else if (parsed <= get_type_max(ST_LONG) && allow_long && allow_signed) {
		return constant_simple_signed(ST_LONG, parsed);
	} else if (parsed <= get_type_max(ST_ULONG) && allow_long && allow_unsigned) {
		return constant_simple_unsigned(ST_ULONG, parsed);
	} else if (parsed <= get_type_max(ST_LLONG) && allow_llong && allow_signed) {
		return constant_simple_signed(ST_LLONG, parsed);
	} else if (parsed <= get_type_max(ST_ULLONG) && allow_llong && allow_unsigned) {
		return constant_simple_unsigned(ST_ULLONG, parsed);
	} else {
		ICE("Cant fit %llu (%.*s) into any type", parsed, str.len, str.str);
	}
}

struct constant constant_from_string(struct string_view str) {
	if (is_float(str)) {
		return floating_point_constant_from_string(str);
	} else {
		return integer_constant_from_string(str);
	}
}

static constant_int conv_ui(constant_uint x, constant_uint range_min, constant_uint range_max) {
    x = x & (range_min + range_max);
	return (x > range_max) ? -((range_min + 1) - (x - range_max)) : x;
}

static constant_uint conv_iu(constant_int x, constant_uint range_max) {
	return (constant_uint)x & range_max;
}

static constant_uint conv_uu(constant_uint x, constant_uint range_max) {
	return x & range_max;
}

static constant_int conv_ii(constant_int x, constant_uint range_min, constant_uint range_max) {
	return conv_ui(conv_iu(x, range_max + range_min), range_min, range_max);
}

void constant_normalize(struct constant *c) {
	assert(type_is_integer(c->data_type));
	struct range r = get_range(c->data_type->simple, -1);
	constant_uint range_min = r.min == -1 ? 0 : (constant_uint)1 << r.min;
	constant_uint range_max = r.max == -1 ? 0 : (((constant_uint)1 << (r.max - 1)) - 1
												 + ((constant_uint)1 << (r.max - 1)));
	if (is_signed(c->data_type->simple))
		c->int_d = conv_ii(c->int_d, range_min, range_max);
	else
		c->uint_d = conv_uu(c->int_d, range_max);
}

struct constant simple_cast(struct constant from, enum simple_type target) {
	assert(from.type == CONSTANT_TYPE);
	assert(from.data_type->type == TY_SIMPLE);
	enum simple_type from_type = from.data_type->simple;

	int from_int = type_is_integer(from.data_type);
	int to_int = type_is_integer(type_simple(target));

	int from_float = type_is_floating(from.data_type);
	int to_float = type_is_floating(type_simple(target));

	from.data_type = type_simple(target);

	if (target == ST_VOID || target == from_type)
		return from;

	if (from_int && to_int) {
		struct range r = get_range(target, -1);
		constant_uint range_min = r.min == -1 ? 0 : (constant_uint)1 << r.min;
		constant_uint range_max = r.max == -1 ? 0 : (((constant_uint)1 << (r.max - 1)) - 1
													 + ((constant_uint)1 << (r.max - 1)));

		if (is_signed(from_type) && is_signed(target)) {
			from.int_d = conv_ii(from.int_d, range_min, range_max);
		} else if (is_signed(from_type) && !is_signed(target)) {
			from.uint_d = conv_iu(from.int_d, range_max);
		} else if (!is_signed(from_type) && is_signed(target)) {
			from.int_d = conv_ui(from.uint_d, range_min, range_max);
		} else if (!is_signed(from_type) && !is_signed(target)) {
			from.uint_d = conv_uu(from.int_d, range_max);
		}
	} else if (from_int && to_float && target == ST_DOUBLE) {
		if (is_signed(from_type))
			from.double_d = from.int_d;
		else
			from.double_d = from.uint_d;
	} else if (from_int && to_float && target == ST_FLOAT) {
		if (is_signed(from_type))
			from.float_d = from.int_d;
		else
			from.float_d = from.uint_d;
	} else if (to_int && from_float && from_type == ST_DOUBLE) {
		if (is_signed(target))
			from.int_d = from.double_d;
		else
			from.uint_d = from.double_d;
		constant_normalize(&from);
	} else if (to_int && from_float && from_type == ST_FLOAT) {
		if (is_signed(target))
			from.int_d = from.float_d;
		else
			from.uint_d = from.float_d;
		constant_normalize(&from);
	} else if (from_float && to_float) {
		if (target == ST_DOUBLE && from_type == ST_FLOAT) {
			from.double_d = from.float_d;
		} else if (target == ST_FLOAT && from_type == ST_DOUBLE) {
			from.float_d = from.double_d;
		}
	} else {
		ICE("From %s to %s\n", strdup(dbg_type(type_simple(from_type))),
			strdup(dbg_type(type_simple(target))));
		NOTIMP();
	}

	return from;
}

uint64_t constant_to_u64(struct constant constant) {
	int is_unsigned = type_is_pointer(constant.data_type) ||
		(type_is_integer(constant.data_type) && !is_signed(constant.data_type->simple));

	if (type_is_simple(constant.data_type, ST_FLOAT)) {
		return (uint32_t)constant.uint_d; // Type-punning constant.float_d.
	} else if (type_is_simple(constant.data_type, ST_DOUBLE)) {
		return constant.uint_d; // Type-punning constant.double_d.
	} else
		return is_unsigned ? (uint64_t)constant.uint_d : (uint64_t)constant.int_d;
}

struct constant constant_cast(struct constant a, struct type *target) {
	if (a.type == CONSTANT_LABEL) {
		if (type_is_pointer(a.data_type) &&
			type_is_pointer(target)) {
			a.data_type = target;
			return a;
		} else {
			printf("Can't do constant cast of the form %s to %s\n",
				   strdup(dbg_type(a.data_type)),
				   strdup(dbg_type(target)));
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

	if (type_is_pointer(target) && type_is_integer(a.data_type)) {
		a.data_type = target;
		a.uint_d = constant_to_u64(a);
		return a;
	}

	if (target->type == TY_SIMPLE &&
		a.data_type->type == TY_SIMPLE) {
		return simple_cast(a, target->simple);
	}

	printf("Trying to cast from %s to %s\n",
		   strdup(dbg_type(a.data_type)),
		   strdup(dbg_type(target)));

	NOTIMP();
}

void constant_to_buffer(uint8_t *buffer, struct constant constant, int bit_offset, int bit_size) {
	assert(constant.type == CONSTANT_TYPE);

	int size = calculate_size(constant.data_type);
	uint64_t value = constant_to_u64(constant);

	if (bit_size != -1) {
		uint64_t mask = gen_mask(64 - bit_size - bit_offset, bit_offset);

		value <<= bit_offset;
		uint64_t prev_value;
		switch (size) {
		case 1: prev_value = *buffer; break;
		case 2: prev_value = *(uint16_t *)buffer; break;
		case 4: prev_value = *(uint32_t *)buffer; break;
		case 8: prev_value = *(uint64_t *)buffer; break;
		default: NOTIMP();
		}
		value = (prev_value & mask) | (value & ~mask);
	}

	switch (size) {
	case 1: *buffer = value & 0xff; break;
	case 2: *(uint16_t *)buffer = value & 0xffff; break;
	case 4: *(uint32_t *)buffer = value & 0xffffffff; break;
	case 8: *(uint64_t *)buffer = value; break;
	default: NOTIMP();
	}
}

struct constant constant_simple_signed(enum simple_type type, constant_int value) {
	return (struct constant) {
		.type = CONSTANT_TYPE,
		.data_type = type_simple(type),
		.int_d = value
	};
}

struct constant constant_simple_unsigned(enum simple_type type, constant_uint value) {
	return (struct constant) {
		.type = CONSTANT_TYPE,
		.data_type = type_simple(type),
		.uint_d = value
	};
}
