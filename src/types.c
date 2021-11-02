#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "types.h"
#include "common.h"
#include "parser/expression.h"

static int compare_types(struct type *a, struct type **a_children,
						 struct type *b) {
	if (a->type != b->type)
		return 0;

	if (a->n != b->n)
		return 0;

	if (a->is_const != b->is_const)
		return 0;

	switch(a->type) {
	case TY_FUNCTION:
		if (a->function.is_variadic != b->function.is_variadic)
			return 0;
		break;

	case TY_POINTER:
		break;

	case TY_SIMPLE:
		if (a->simple != b->simple)
			return 0;
		break;

	case TY_STRUCT:
		if (a->struct_data != b->struct_data)
			return 0;
		break;

	case TY_ARRAY:
		if (a->array.length != b->array.length)
			return 0;
		break;

	case TY_INCOMPLETE_ARRAY:
		// TODO: Is this correct?
		break;

	case TY_VARIABLE_LENGTH_ARRAY:
		if (a->variable_length_array.length_expr !=
			b->variable_length_array.length_expr)
			return 0;
		break;

	default:
		ERROR("Not implemented %d", a->type);
		break;
	}

	for (int i = 0; i < a->n; i++) {
		if (!compare_types(a_children[i], a_children[i]->children, b->children[i]))
			return 0;
	}

	return 1;
}

static uint32_t type_hash(struct type *type, struct type **children) {
	uint32_t hash = 0;

	hash ^= hash32(type->type) ^ hash32(type->is_const);

	switch (type->type) {
	case TY_SIMPLE:
		hash ^= hash32(type->simple);
		break;
	case TY_FUNCTION:
		hash ^= hash32(type->function.is_variadic);
		break;
	case TY_POINTER:
		break;
	case TY_STRUCT:
		hash ^= hash32((size_t)type->struct_data);
		break;
	case TY_ARRAY:
		hash ^= hash32(type->array.length);
		break;
	case TY_INCOMPLETE_ARRAY:
		break;
	case TY_VARIABLE_LENGTH_ARRAY:
		hash ^= hash32((size_t)type->variable_length_array.length_expr);
		break;
	default:
		ERROR("Not implemented %d", (int)type->type);
	}

	for (int i = 0; i < type->n; i++) {
		hash ^= type_hash(children[i], children[i]->children);
	}

	return hash;
}

struct type *type_create(struct type *params, struct type **children) {
	static struct type **hashtable = NULL;
	static int hashtable_size = 0;

	if (hashtable_size == 0) {
		hashtable_size = 1024;
		hashtable = malloc(hashtable_size * sizeof(*hashtable));
		for (int i = 0; i < hashtable_size; i++) {
			hashtable[i] = NULL;
		}
	}

	uint32_t hash_idx = type_hash(params, children) % hashtable_size;
	struct type *first = hashtable[hash_idx];
	struct type *ffirst = first;

	while(first && !compare_types(params, children, first))
		first = first->next;

	if (first)
		return first;

	struct type *new = malloc(sizeof(*params) + sizeof(*children) * params->n);
	*new = *params;
	memcpy(new->children, children, sizeof(*children) * params->n);

	new->next = ffirst;
	hashtable[hash_idx] = new;

	return new;
}

struct type *type_simple(enum simple_type type) {
	return type_create(&(struct type) { .type = TY_SIMPLE, .simple = type }, NULL);
}

struct type *type_pointer(struct type *type) {
	return type_create(&(struct type) { .type = TY_POINTER, .n = 1 }, &type);
}

struct type *type_struct(struct struct_data *struct_data) {
	return type_create(&(struct type) { .type = TY_STRUCT,
			.struct_data = struct_data }, NULL);
}

struct type *type_array(struct type *type, int length) {
	return type_create(&(struct type) { .type = TY_ARRAY,
			.n = 1, .array.length = length}, &type);
}

struct type *type_deref(struct type *type) {
	if (type->type != TY_POINTER)
		ERROR("Expected type to be pointer when dereffing %s", dbg_type(type));
	return type->children[0];
}

struct type *type_make_const(struct type *type, int is_const) {
	struct type params = *type;
	params.is_const = is_const;
	return type_create(&params, type->children);
}

// TODO: make this better.
struct struct_data *register_struct(void) {
	return malloc(sizeof (struct struct_data));
}

struct enum_data *register_enum(void) {
	return malloc(sizeof (struct enum_data));
}

int type_member_idx(struct type *type,
					const char *name) {
	if (type->type != TY_STRUCT) {
		ERROR("%s is not a struct", dbg_type(type));
	}

	struct struct_data *data = type->struct_data;

	if (!data->is_complete)
		ERROR("Member access on incomplete type not allowed");

	for (int i = 0; i < data->n; i++) {
		if (data->fields[i].name && strcmp(name, data->fields[i].name) == 0)
			return i;
	}

	ERROR("%s has no member with name %s", dbg_type(type), name);
}

// Make arrays into pointers, and functions into function pointers.
struct type *type_adjust_parameter(struct type *type) {
	if (type->type == TY_ARRAY ||
		type->type == TY_INCOMPLETE_ARRAY ||
		type->type == TY_VARIABLE_LENGTH_ARRAY) {
		struct type *ptr = type_pointer(type->children[0]);
		if (type->is_const)
			ptr = type_make_const(ptr, 1);
		return ptr;
	} else if (type->type == TY_FUNCTION) {
		return type_pointer(type);
	} else
		return type;
}

int type_is_integer(struct type *type) {
	if (type->type != TY_SIMPLE)
		return 0;

	switch (type->simple) {
	case ST_VOID:
	case ST_FLOAT:
	case ST_DOUBLE:
	case ST_LDOUBLE:
	case ST_FLOAT_COMPLEX:
	case ST_DOUBLE_COMPLEX:
	case ST_LDOUBLE_COMPLEX:
		return 0;
	case ST_CHAR:
	case ST_SCHAR:
	case ST_UCHAR:
	case ST_SHORT:
	case ST_USHORT:
	case ST_INT:
	case ST_UINT:
	case ST_LONG:
	case ST_ULONG:
	case ST_LLONG:
	case ST_ULLONG:
	case ST_BOOL:
		return 1;
	default:
		NOTIMP();
	}
}

int type_is_floating(struct type *type) {
	if (type->type != TY_SIMPLE)
		return 0;

	switch (type->simple) {
	case ST_VOID:
	case ST_CHAR:
	case ST_SCHAR:
	case ST_UCHAR:
	case ST_SHORT:
	case ST_USHORT:
	case ST_INT:
	case ST_UINT:
	case ST_LONG:
	case ST_ULONG:
	case ST_LLONG:
	case ST_ULLONG:
	case ST_BOOL:
		return 0;
	case ST_FLOAT:
	case ST_DOUBLE:
	case ST_LDOUBLE:
	case ST_FLOAT_COMPLEX:
	case ST_DOUBLE_COMPLEX:
	case ST_LDOUBLE_COMPLEX:
		return 1;
	default:
		NOTIMP();
	}
}

int type_is_arithmetic(struct type *type) {
	return type_is_integer(type) ||
		type_is_floating(type);
}

int type_is_real(struct type *type) {
	if (type->type != TY_SIMPLE)
		return 0;

	if (type->simple == ST_FLOAT_COMPLEX ||
		type->simple == ST_DOUBLE_COMPLEX ||
		type->simple == ST_LDOUBLE_COMPLEX)
		return 0;
	return type_is_integer(type) ||
		type_is_floating(type);
}

int type_is_pointer(struct type *type) {
	return type->type == TY_POINTER;
}

void type_select(struct type *type, int index,
				 int *field_offset, struct type **field_type) {
	if (field_offset)
		*field_offset = calculate_offset(type, index);

	if (!field_type)
		return;

	switch (type->type) {
	case TY_STRUCT:
		*field_type = type->struct_data->fields[index].type;
		break;

	case TY_ARRAY:
	case TY_INCOMPLETE_ARRAY:
		*field_type = type->children[0];
		break;

	default:
		NOTIMP();
	}
}

void type_merge_anonymous_substructures(struct struct_data *data) {
	// types names offsets need to be modified.
	for (int i = 0; i < data->n; i++) {
		if (data->fields[i].name)
			continue;

		int offset = data->fields[i].offset;
		struct type *type = data->fields[i].type;

		if (type->type != TY_STRUCT)
			continue; // It is probably an anonymous bit-field.

		assert(type->type == TY_STRUCT);
		struct struct_data *sub_data = type->struct_data;

		int n_new_elements = sub_data->n;
		int new_n = data->n + n_new_elements - 1;

		if (n_new_elements == 0) {
			ERROR("This should not be allowed");
		}

		// Make place for new elements.
		data->fields = realloc(data->fields, sizeof *data->fields * new_n);

		for (int j = new_n - 1; j >= i + sub_data->n; j--) {
			data->fields[j] = data->fields[j - sub_data->n + 1];
		}

		data->n = new_n;

		for (int j = 0; j < sub_data->n; j++) {
			data->fields[i + j] = sub_data->fields[j];
			data->fields[i + j].offset += offset;
		}

		i += n_new_elements - 1;
	}
}

int type_has_variable_size(struct type *type) {
	switch (type->type) {
	case TY_SIMPLE:
	case TY_STRUCT:
	case TY_POINTER:
		return 0;

	case TY_FUNCTION:
		return 0; // At least I hope this is correct.

	case TY_ARRAY:
		return type_has_variable_size(type->children[0]);

	case TY_INCOMPLETE_ARRAY:
		return 0;

	case TY_VARIABLE_LENGTH_ARRAY:
		return 1;

	default:
		NOTIMP();
	}
}

int type_is_simple(struct type *type, enum simple_type st) {
	return type->type == TY_SIMPLE &&
		type->simple == st;
}

int type_is_aggregate(struct type *type) {
	return type->type == TY_STRUCT ||
		type->type == TY_ARRAY ||
		type->type == TY_INCOMPLETE_ARRAY;
}

struct expr *type_sizeof(struct type *type) {
	if (type_has_variable_size(type)) {
		switch (type->type) {
		case TY_VARIABLE_LENGTH_ARRAY:
			assert(type->variable_length_array.is_evaluated);
			return EXPR_BINARY_OP(OP_MUL, EXPR_VAR(type->variable_length_array.length_var, type_simple(ST_INT)),
								  type_sizeof(type->children[0]));
		case TY_ARRAY:
			return EXPR_BINARY_OP(OP_MUL, EXPR_INT(type->array.length),
								  type_sizeof(type->children[0]));
		default:
			ERROR("Type can't have variable size");
		}
	} else {
		struct constant c = {.type = CONSTANT_TYPE, .data_type = type_simple(ST_ULONG), .ulong_d = calculate_size(type) };

		return expr_new((struct expr) {
				.type = E_CONSTANT,
				.constant = c
			});
	}
}

void type_evaluate_vla(struct type *type) {
	for (int i = 0; i < type->n; i++)
		type_evaluate_vla(type->children[i]);

	if (type->type == TY_VARIABLE_LENGTH_ARRAY) {
		if (!type->variable_length_array.is_evaluated) {
			type->variable_length_array.length_var = expression_to_ir_clear_temp(type->variable_length_array.length_expr);
		}
		type->variable_length_array.is_evaluated = 1;
	}
}

int type_contains_unevaluated_vla(struct type *type) {
	for (int i = 0; i < type->n; i++)
		if (type_contains_unevaluated_vla(type->children[i]))
			return 1;

	return type->type == TY_VARIABLE_LENGTH_ARRAY && type->variable_length_array.is_evaluated;
}

// See 6.2.7.
// Returns NULL if not compatible.
struct type *type_make_composite(struct type *a, struct type *b) {
	if (a == b)
		return a;

	if (a->type == TY_ARRAY && b->type == TY_INCOMPLETE_ARRAY) {
		return a;
	} else if (b->type == TY_ARRAY && a->type == TY_INCOMPLETE_ARRAY) {
		return b;
	}

	if (a->type != b->type)
		return NULL;

	switch (a->type) {
	case TY_FUNCTION:
		if (a->function.is_variadic && a->n == 1) {
			return b;
		} else if (b->function.is_variadic && b->n == 1) {
			return a;
		}
		break;

	case TY_ARRAY:
		break;

	default:;
	}

	return NULL;
}
