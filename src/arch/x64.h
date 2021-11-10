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

int calculate_alignment(struct type *type);
int calculate_size(struct type *type);
void calculate_offsets(struct struct_data *data);

int calculate_offset(struct type *type, int index);

#define IS_CHAR_SIGNED 1
#define ENUM_TYPE ST_INT
#define SIZE_TYPE ST_ULONG

struct constant {
	enum {
		CONSTANT_LABEL, // $LABEL
		CONSTANT_LABEL_POINTER, // (LABEL)
		CONSTANT_TYPE
	} type;

	struct type *data_type;

	union {
		_Bool bool_d;
		int8_t char_d;
		int8_t schar_d;
		uint8_t uchar_d;
		int16_t short_d;
		uint16_t ushort_d;
		int32_t int_d;
		uint32_t uint_d;
		int64_t long_d;
		uint64_t ulong_d;
		int64_t llong_d;
		uint64_t ullong_d;

		float float_d;
		double double_d;

		struct {
			label_id label;
			int64_t offset;
		} label;
	};
};

struct constant constant_increment(struct constant a);
struct constant constant_zero(struct type *type);
struct constant constant_from_string(struct string_view str);
struct constant constant_cast(struct constant a, struct type *target);

void convert_rax(enum simple_type from, enum simple_type to);

void constant_to_buffer(uint8_t *buffer, struct constant constant, int bit_offset, int bit_size);
const char *constant_to_string(struct constant constant);

#endif

