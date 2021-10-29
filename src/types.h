#ifndef TYPES_H
#define TYPES_H

#include <parser/variables.h>

#include <stdint.h>

// This describes the types used for all parts of the compiler.
// These should be able to describe all types of the C programming
// language.


// From 6.7.2p2
// - void
// - char
// - signed char
// - unsigned char
// - short, signed short, short int, or signed short int
// - unsigned short, or unsigned short int
// - int, signed, or signed int
// - unsigned, or unsigned int
// - long, signed long, long int, or signed long int
// - unsigned long, or unsigned long int
// - long long, signed long long, long long int, or signed long long int
// - unsigned long long, or unsigned long long int
// - float
// - double
// - long double
// - _Bool
// - float _Complex
// - double _Complex
// - long double _Complex
// - atomic type specifier
// - struct or union specifier
// - enum specifier
// - typedef name

enum simple_type {
	ST_VOID,
	ST_CHAR,
	ST_SCHAR,
	ST_UCHAR,
	ST_SHORT,
	ST_USHORT,
	ST_INT,
	ST_UINT,
	ST_LONG,
	ST_ULONG,
	ST_LLONG,
	ST_ULLONG,
	ST_FLOAT,
	ST_DOUBLE,
	ST_LDOUBLE,
	ST_BOOL,
	ST_FLOAT_COMPLEX,
	ST_DOUBLE_COMPLEX,
	ST_LDOUBLE_COMPLEX,

	ST_COUNT
};

struct type {
	enum type_ty {
		TY_ARRAY,
		TY_INCOMPLETE_ARRAY,
		TY_STRUCT,
		TY_POINTER,
		TY_FUNCTION,
		TY_SIMPLE, // non-composite
		TY_VARIABLE_LENGTH_ARRAY
	} type;

	union {
		enum simple_type simple;
		struct { 
			int length;
		} array;
		struct {
			var_id length;
		} variable_length_array;
		struct {
			int is_variadic;
		} function;
		struct struct_data *struct_data;
	};

	int is_const;

	struct type *next; // Used in hash-map.

	int n;
	struct type *children[];
};

struct struct_data {
	char *name;

	int is_complete,
		is_union,
		is_packed;

	int n;
	struct field {
		char *name;
		struct type *type;
		int bitfield; // -1 means no bit-field.
		int offset;
		int bit_offset;
	} *fields;

	int alignment, size;

	int flexible;
};

struct enum_data {
	char *name;
	int is_complete;
};

struct struct_data *register_struct(void);
struct enum_data *register_enum(void);

// The interface guarantees that if only these functions are used
// to create and modify a type, it should be true that two types
// are equal iff their pointers are equal. Similar to hash consing or
// string interning.
struct type *type_simple(enum simple_type type);
struct type *type_create(struct type *params, struct type **children);
struct type *type_pointer(struct type *type);
struct type *type_array(struct type *type, int length);
struct type *type_deref(struct type *type);
struct type *type_struct(struct struct_data *struct_data);
struct type *type_make_const(struct type *type);
struct type *type_adjust_parameter(struct type *type);

int type_member_idx(struct type *type,
					const char *name);

void type_select(struct type *type, int index,
				 int *field_offset, struct type **field_type);

int type_is_real(struct type *type);
int type_is_arithmetic(struct type *type);
int type_is_floating(struct type *type);
int type_is_integer(struct type *type);
int type_is_pointer(struct type *type);
int type_is_simple(struct type *type, enum simple_type st);
int type_is_aggregate(struct type *type);

const char *type_to_string(struct type *type);

void type_merge_anonymous_substructures(struct struct_data *data);
int type_has_variable_size(struct type *type);

#include <arch/x64.h>

#endif
