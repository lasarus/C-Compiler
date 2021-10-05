#include "symbols.h"

#include "list.h"
#include "common.h"
#include <string.h>

struct identifier_storage {
	char *name;
	int nest;
	struct symbol_identifier data;
};

struct struct_storage {
	char *name;
	int nest;
	struct symbol_struct data;
};

struct typedef_storage {
	char *name;
	int nest;
	struct symbol_typedef data;
};

LIST(identifier_storage_list, struct identifier_storage);
LIST(struct_storage_list, struct struct_storage);
LIST(typedef_storage_list, struct typedef_storage);

struct identifier_storage_list *var_stor = NULL;
struct struct_storage_list *struct_stor = NULL;
struct typedef_storage_list *typedef_stor = NULL;
int current_nest = 0;

void symbols_push_scope() {
	current_nest++;
}

void symbols_pop_scope() {
	current_nest--;
	if (var_stor) {
		int n;

		for (n = var_stor->n - 1; n >= 0; n--) {
			if (var_stor->list[n].nest <= current_nest)
				break;
		}

		var_stor->n = n + 1;
	}

	if (struct_stor) {
		int n;

		for (n = struct_stor->n - 1; n >= 0; n--) {
			if (struct_stor->list[n].nest <= current_nest)
				break;
		}

		struct_stor->n = n + 1;
	}

	if (typedef_stor) {
		int n;

		for (n = typedef_stor->n - 1; n >= 0; n--) {
			if (typedef_stor->list[n].nest <= current_nest)
				break;
		}

		typedef_stor->n = n + 1;
	}
}

struct identifier_storage *get_var(const char *name) {
	if (var_stor)
		for (int i = var_stor->n - 1; i >= 0; i--)
			if (strcmp(var_stor->list[i].name, name) == 0)
				return &var_stor->list[i];

	return NULL;
}

struct struct_storage *get_struct(const char *name) {
	if (struct_stor)
		for (int i = struct_stor->n - 1; i >= 0; i--)
			if (strcmp(struct_stor->list[i].name, name) == 0)
				return &struct_stor->list[i];

	return NULL;
}

struct typedef_storage *get_typedef(const char *name) {
	if (typedef_stor)
		for (int i = typedef_stor->n - 1; i >= 0; i--)
			if (strcmp(typedef_stor->list[i].name, name) == 0)
				return &typedef_stor->list[i];

	return NULL;
}

struct symbol_identifier *symbols_add_identifier(const char *name) {
	struct identifier_storage *stor = get_var(name);

	if (stor && stor->nest == current_nest)
		ERROR("Identifier name already declared, %s", name);

	struct identifier_storage nstor;
	nstor.nest = current_nest;
	nstor.name = strdup(name);

	identifier_storage_list_add(&var_stor, nstor);
	return &var_stor->list[var_stor->n - 1].data;
}

struct symbol_identifier *symbols_get_identifier(const char *name) {
	struct identifier_storage *stor = get_var(name);
	return stor ? &stor->data : NULL;
}

struct symbol_identifier *symbols_get_identifier_in_current_scope(const char *name) {
	struct identifier_storage *stor = get_var(name);

	if (stor && stor->nest == current_nest)
		return &stor->data;
	return NULL;
}

struct symbol_struct *symbols_get_struct_in_current_scope(const char *name) {
	struct struct_storage *stor = get_struct(name);

	if (stor && stor->nest == current_nest)
		return &stor->data;
	return NULL;
}

struct symbol_struct *symbols_add_struct(const char *name) {
	struct struct_storage *stor = get_struct(name);

	if (stor && stor->nest == current_nest) {
		ERROR("Struct/union name already declared, %s", name);
	}

	struct struct_storage nstor;
	nstor.nest = current_nest;
	nstor.name = strdup(name);

	struct_storage_list_add(&struct_stor, nstor);
	return &struct_stor->list[struct_stor->n - 1].data;
}

struct symbol_struct *symbols_get_struct(const char *name) {
	struct struct_storage *stor = get_struct(name);
	return stor ? &stor->data : NULL;
}

struct symbol_typedef *symbols_add_typedef(const char *name) {
	struct typedef_storage *stor = get_typedef(name);

	if (stor && stor->nest == current_nest) {
		return &stor->data; // This is technically wrong.
		// Has to be fixed in declaration_parser.c
	}

	struct typedef_storage nstor;
	nstor.nest = current_nest;
	nstor.name = strdup(name);

	typedef_storage_list_add(&typedef_stor, nstor);
	return &typedef_stor->list[typedef_stor->n - 1].data;
}

struct symbol_typedef *symbols_get_typedef(const char *name) {
	struct typedef_storage *stor = get_typedef(name);
	return stor ? &stor->data : NULL;
}

/* struct symbol_typedef *symbols_add_typedef(const char *name) { */
/* } */

/* struct symbol_typedef *symbols_get_typedef(const char *name) { */
/* } */

