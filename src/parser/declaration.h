#ifndef PARSER_DECLARATION_H
#define PARSER_DECLARATION_H

#include "parser.h"
#include "expression.h"

#include <types.h>

struct initializer {
	int size, cap;
	struct init_pair {
		int offset;
		struct expr *expr;
	} *pairs;
};

struct initializer *parse_initializer(struct type **type);

struct declaration {
	int n;
	struct type **types;
	struct initializer **initializers;
	char **names;

	int arg_n;
	char **arg_names; // Argument names of the function declared last.

	int is_extern,
		is_static,
		is_thread_local,
		is_auto,
		is_register;
};

enum {
	TSF_VOID = 1UL << 0,
	TSF_CHAR = 1UL << 1,
	TSF_SHORT = 1UL << 2,
	TSF_INT = 1UL << 3,
	TSF_LONG1 = 1UL << 4,
	TSF_LONG2 = 1UL << 5,
	TSF_LONGB = TSF_LONG1 | TSF_LONG2,
	TSF_FLOAT = 1UL << 6,
	TSF_DOUBLE = 1UL << 7,
	TSF_SIGNED = 1UL << 8,
	TSF_UNSIGNED = 1UL << 9,
	TSF_BOOL = 1UL << 10,
	TSF_COMPLEX = 1UL << 11,
	TSF_OTHER = 1UL << 12
};

struct type_specifiers {
	unsigned int specifiers;

	struct type *data_type; // Can be read from typedef or struct/union/enum.

	struct position pos;
};

struct storage_class_specifiers {
	int typedef_n,
		extern_n,
		static_n,
		thread_local_n,
		auto_n,
		register_n;
};

struct type_qualifiers {
	int const_n,
		restrict_n,
		volatile_n,
		atomic_n;
};

struct function_specifiers {
	int inline_n;
	int noreturn_n;
};

struct alignment_specifiers {
	int tmp;
};

struct specifiers {
	struct type_specifiers ts;
	struct storage_class_specifiers scs;
	struct type_qualifiers tq;
	struct function_specifiers fs;
	struct alignment_specifiers as;
};

// See section A.2.2 in the standard.
struct type *parse_type_name(void);
int parse_declaration(int global);

#endif
