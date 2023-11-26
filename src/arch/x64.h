#ifndef ARCH_X64_H
#define ARCH_X64_H

#include <types.h>
#include <codegen/rodata.h>

/*
— signed char
— unsigned char
— short, signed short, short int, or signed short int
— unsigned short, or unsigned short int
— int, signed, or signed int
— unsigned, or unsigned int
— long, signed long, long int, or signed long int
— unsigned long, or unsigned long int
— long long, signed long long, long long int, or signed long long int
— unsigned long long, or unsigned long long int
— float
— double
— long double
— _Bool
— float _Complex
— double _Complex
— long double _Complex
*/

int type_register(struct type *type);
int is_scalar(struct type *type);
int is_signed(enum simple_type type);
int type_rank(enum simple_type t1);
int is_contained_in(enum simple_type large, int large_bitfield,
					enum simple_type small, int small_bitfield);
enum simple_type to_unsigned(enum simple_type type);

int calculate_size(struct type *type);
int calculate_alignment(struct type *type);
void calculate_offsets(struct struct_data *data);

int calculate_offset(struct type *type, int index);

#define IS_CHAR_SIGNED 1
#define ENUM_TYPE ST_INT
#define CHAR32_TYPE ST_UINT
#define CHAR16_TYPE ST_USHORT

typedef uint64_t constant_uint;
typedef int64_t constant_int;

struct constant {
	// TODO: Add is_reference field and remove LABEL_POINTER and TYPE_POINTER.
	enum {
		CONSTANT_LABEL, // $LABEL
		CONSTANT_LABEL_POINTER, // (LABEL)
		CONSTANT_TYPE,
		CONSTANT_TYPE_POINTER,
	} type;

	struct type *data_type;

	union {
		constant_int int_d;
		constant_uint uint_d;

		float float_d;
		double double_d;

		struct {
			label_id label;
			int64_t offset;
		} label;
	};
};

struct constant constant_simple_signed(enum simple_type type, constant_int value);
struct constant constant_simple_unsigned(enum simple_type type, constant_uint value);

struct constant constant_increment(struct constant a);
struct constant constant_from_string(struct string_view str);
struct constant constant_cast(struct constant a, struct type *target);

void convert_rax(enum simple_type from, enum simple_type to);

void constant_to_buffer(uint8_t *buffer, struct constant constant, int bit_offset, int bit_size);
uint64_t constant_to_u64(struct constant constant);

void constant_normalize(struct constant *c);

#endif

