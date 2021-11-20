#include "rodata.h"
#include "codegen.h"

#include <common.h>
#include <arch/x64.h>
#include <parser/expression.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

struct entry {
	enum entry_type {
		ENTRY_STR,
		ENTRY_LABEL_NAME
	} type;
	struct string_view name;
	label_id id;
};

static struct entry *entries = NULL;
static int entries_size = 0, entries_cap = 0;

label_id label_register(enum entry_type type, struct string_view str) {
	if (type == ENTRY_STR) {
			for (int i = 0; i < entries_size; i++) {
				if (sv_cmp(entries[i].name, str)) {
					return entries[i].id;
				}
			}
	}

	int id = entries_size;
	ADD_ELEMENT(entries_size, entries_cap, entries) = (struct entry) {
		.type = type,
		.name = str,
		.id = id
	};

	return id;
}

label_id rodata_register(struct string_view str) {
	return label_register(ENTRY_STR, str);
}

const char *rodata_get_label_string(label_id id) {
	if (entries[id].type == ENTRY_STR) {
		static char *buffer = NULL;

		if (!buffer)
			buffer = malloc(128);

		sprintf(buffer, ".L_rodata%d", id);
		return buffer;
	} else if (entries[id].type == ENTRY_LABEL_NAME) {
		return sv_to_str(entries[id].name);
	} else {
		NOTIMP();
	}
}

void rodata_codegen(void) {
	for (int i = 0; i < entries_size; i++) {
		if (entries[i].type != ENTRY_STR)
			continue;
		emit("%s:", rodata_get_label_string(entries[i].id));

		emit_no_newline("\t.string \"", entries[i].name);
		struct string_view str = entries[i].name;
		for (int i = 0; i < str.len; i++) {
			char buffer[5];
			character_to_escape_sequence(str.str[i], buffer, 0);
			emit_no_newline("%s", buffer);
		}
		emit_no_newline("\"\n");
	}
}

label_id register_label_name(struct string_view str) {
	return label_register(ENTRY_LABEL_NAME, str);
}

struct static_var {
	struct string_view label;
	struct type *type;
	struct initializer init;
	int global;
};

static struct static_var *static_vars = NULL;
static int static_vars_size, static_vars_cap;

void data_register_static_var(struct string_view label, struct type *type, struct initializer init, int global) {
	ADD_ELEMENT(static_vars_size, static_vars_cap, static_vars) = (struct static_var) {
		.label = label,
		.type = type,
		.init = init,
		.global = global
	};
}

void codegen_initializer_recursive(struct initializer *init, struct type *type,
								   int bit_size, int bit_offset,
								   uint8_t *buffer, label_id *labels,
								   int64_t *label_offsets, int *is_label) {
	switch (init->type) {
	case INIT_EMPTY: break;
	case INIT_STRING:
		for (int i = 0; i < init->string.len; i++)
			buffer[i] = init->string.str[i];
		break;

	case INIT_EXPRESSION: {
		struct constant *c = expression_to_constant(init->expr);
		if (c) {
			switch (c->type) {
			case CONSTANT_TYPE:
				constant_to_buffer(buffer, *c, bit_offset, bit_size);
				break;

			case CONSTANT_LABEL_POINTER:
				is_label[0] = 1;
				labels[0] = c->label.label;
				label_offsets[0] = c->label.offset;
				break;

			case CONSTANT_LABEL:
				NOTIMP();
				break;

			default:
				NOTIMP();
			}
		} else if (init->expr->type == E_COMPOUND_LITERAL) {
			codegen_initializer_recursive(&init->expr->compound_literal.init, type,
										  -1, -1,
										  buffer, labels, label_offsets,
										  is_label);
		}
	} break;
	case INIT_BRACE:
		for (int i = 0; i < init->brace.size; i++) {
			int coffset, cbit_size, cbit_offset;
			struct type *child_type = type_select(type, i);
			type_get_offsets(type, i, &coffset, &cbit_offset, &cbit_size);
			codegen_initializer_recursive(init->brace.entries + i, child_type,
										  cbit_size, cbit_offset,
										  buffer + coffset, labels + coffset,
										  label_offsets + coffset, is_label + coffset);
		}
		break;
	}
}

void codegen_initializer(struct type *type, struct initializer *init);

void codegen_compound_literals(struct expr **expr, int lvalue) {
	switch ((*expr)->type) {
	case E_DOT_OPERATOR:
		if (lvalue) {
			codegen_compound_literals(&(*expr)->member.lhs, 1);
		} else {
			NOTIMP();
		}
		break;
	case E_GENERIC_SELECTION:
	case E_COMPOUND_LITERAL:
		if (lvalue) {
			static int counter = 0;
			char name[256];
			sprintf(name, ".compundliteral%d", counter++);
			emit("%s:", name);
			codegen_initializer((*expr)->compound_literal.type,
								&(*expr)->compound_literal.init);

			label_id label = register_label_name(sv_from_str(strdup(name)));

			*expr = expr_new((struct expr) {
					.type = E_CONSTANT,
					.constant = {
						.type = CONSTANT_LABEL,
						.data_type = (*expr)->compound_literal.type,
						.label = { label, 0 }
					}
				});
		}
		break;
	case E_ARRAY_PTR_DECAY:
	case E_ADDRESS_OF:
		codegen_compound_literals(&(*expr)->args[0], 1);
		break;
	case E_INDIRECTION:
		codegen_compound_literals(&(*expr)->args[0], 0);
		break;
	case E_UNARY_OP:
		codegen_compound_literals(&(*expr)->args[0], lvalue);
		break;
	case E_CAST:
		codegen_compound_literals(&(*expr)->cast.arg, lvalue);
		break;
	case E_ALIGNOF:
	case E_POINTER_ADD:
	case E_POINTER_SUB:
		codegen_compound_literals(&(*expr)->args[0], lvalue);
		codegen_compound_literals(&(*expr)->args[1], lvalue);
		break;

	case E_POINTER_DIFF:
	case E_ASSIGNMENT:
	case E_ASSIGNMENT_POINTER:
	case E_ASSIGNMENT_OP:
	case E_CONDITIONAL:
	case E_COMMA:
	case E_BUILTIN_VA_START:
	case E_BUILTIN_VA_END:
	case E_BUILTIN_VA_ARG:
	case E_BUILTIN_VA_COPY:
		break;

	default: return;
	}

	if ((*expr)->type != E_CONSTANT) {
		struct constant c;
		if (evaluate_constant_expression(*expr, &c)) {
			*expr = expr_new((struct expr) {
					.type = E_CONSTANT,
					.constant = c
				});
		}
	}
}

void codegen_pre_initializer(struct initializer *init) {
	// Iterate through all child expressions and find uninitialized compound literals.
	switch (init->type) {
	case INIT_BRACE:
		for (int i = 0; i < init->brace.size; i++)
			codegen_pre_initializer(init->brace.entries + i);
		break;

	case INIT_EXPRESSION:
		codegen_compound_literals(&init->expr, 0);
		break;

	default: break;
	}
}

void codegen_initializer(struct type *type,
						 struct initializer *init) {
	// TODO: Make this more portable.
	int size = calculate_size(type);
	if (size == -1) {
		printf("TYPE FAIL: %s\n", dbg_type(type));
	}
	assert(size != -1);

	uint8_t *buffer = malloc(sizeof *buffer * size);
	label_id *labels = malloc(sizeof *labels * size);
	int64_t *label_offsets = malloc(sizeof *label_offsets * size);
	int *is_label = malloc(sizeof *is_label * size);

	for (int i = 0; i < size; i++) {
		buffer[i] = 0;
		is_label[i] = 0;
		labels[i] = 0;
		label_offsets[i] = 0;
	}

	codegen_initializer_recursive(init, type, -1, -1, buffer, labels, label_offsets, is_label);

	for (int i = 0; i < size; i++) {
		if (is_label[i]) {
			if (label_offsets[i] == 0)
				emit(".quad %s", rodata_get_label_string(labels[i]));
			else
				emit(".quad %s+%lld", rodata_get_label_string(labels[i]),
					label_offsets[i]);
			i += 7;
		} else {
			int how_long = 0;
			for (; how_long < 8 && i + how_long < size; how_long++) {
				if (is_label[i + how_long])
					break;
			}
			if (how_long == 8) {
				emit(".quad %" PRIu64, *(uint64_t *)(buffer + i));
				i += how_long - 1;
			} else {
				emit(".byte %d", (int)buffer[i]);
			}
		}
	}

	free(buffer);
	free(labels);
	free(label_offsets);
	free(is_label);
}

void codegen_static_var(struct static_var *static_var) {
	(void)static_var;
	set_section(".data");
	if (static_var->global)
		emit(".global %.*s", static_var->label.len, static_var->label.str);

	if (static_var->init.type == INIT_EMPTY) {
		emit("%.*s:", static_var->label.len, static_var->label.str);

		emit(".zero %d", calculate_size(static_var->type));
	} else {
		codegen_pre_initializer(&static_var->init);

		emit("%.*s:", static_var->label.len, static_var->label.str);

		codegen_initializer(static_var->type, &static_var->init);
	}

	set_section(".text");
}

void data_codegen(void) {
	for (int i = 0; i < static_vars_size; i++) {
		codegen_static_var(static_vars + i);
	}
}
