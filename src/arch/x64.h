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
int is_contained_in(enum simple_type large,
					enum simple_type small);
enum simple_type to_unsigned(enum simple_type type);

struct type *usual_arithmetic_conversion(struct type *a,
										 struct type *b);

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
		char char_d;
		signed char schar_d;
		unsigned char uchar_d;
		short short_d;
		unsigned short ushort_d;
		int int_d;
		unsigned int uint_d;
		long long_d;
		unsigned long ulong_d;
		long long llong_d;
		unsigned long long ullong_d;

		float float_d;
		double double_d;

		char *str_d;

		struct {
			label_id label;
			int64_t offset;
		} label;
	};
};

struct constant constant_add(struct constant a, struct constant b);
struct constant constant_sub(struct constant a, struct constant b);
struct constant constant_increment(struct constant a);
struct constant constant_zero(struct type *type);
struct constant constant_one(struct type *type);
struct constant constant_from_string(const char *str);
struct constant constant_cast(struct constant a, struct type *target);
struct constant constant_shift_left(struct constant a, struct constant b);
struct constant constant_or(struct constant a, struct constant b);

void convert_rax(enum simple_type from, enum simple_type to);

void constant_to_buffer(uint8_t *buffer, struct constant constant);
const char *constant_to_string(struct constant constant);

#endif

