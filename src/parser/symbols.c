#include "symbols.h"

#include <common.h>

#include <string.h>

// I got the idea for data structure from
// https://www.ida.liu.se/~TDDB44/lectures/PDF-OH2006/Symboltable-2008.pdf
// The performance improvement was quite modest, but it still feels like
// the correct design.

struct entry_id {
	enum entry_type {
		ENTRY_TYPEDEF,
		ENTRY_STRUCT,
		ENTRY_IDENTIFIER
	} type;
	struct string_view name;
};

struct table_entry {
	struct entry_id id;

	union {
		struct symbol_identifier identifier_data;
		struct symbol_struct struct_data;
		struct symbol_typedef typedef_data;
	};

	int block, link;
};

static struct hash_table {
	int size;
	int *entries;
} hash_table;

static struct table {
	int size, cap;
	struct table_entry *entries;
} table;

static int current_block = 0;

uint32_t hash_entry(struct entry_id id) {
	return hash32(id.type) ^ sv_hash(id.name);
}

int compare_entry(struct entry_id a, struct entry_id b) {
	return a.type == b.type && sv_cmp(a.name, b.name);
}

void symbols_push_scope(void) {
	current_block++;
}

void symbols_pop_scope(void) {
	current_block--;
	for (int i = table.size - 1; i >= 0; i--) {
		struct table_entry *entry = table.entries + i;
		if (entry->block <= current_block)
			break;

		uint32_t hash = hash_entry(entry->id) % hash_table.size;

		hash_table.entries[hash] = entry->link;
		table.size = i;
	}
}

struct table_entry *get_entry(struct entry_id id, int global) {
	uint32_t hash = hash_entry(id) % hash_table.size;
	int current_idx = hash_table.entries[hash];

	while (current_idx >= 0) {
		struct table_entry *entry = table.entries + current_idx;
		if (compare_entry(entry->id, id) &&
			!(global && entry->block != 0)) {
			return entry;
		}
		current_idx = entry->link;
	}

	return NULL;
}

struct table_entry *add_entry_with_block(struct entry_id id, int block) {
	// Move entire table one step down for entry->block > block.
	(void)ADD_ELEMENT(table.size, table.cap, table.entries);

	int i;
	for (i = table.size - 2; i >= 0; i--) {
		struct table_entry *entry = table.entries + i;
		if (entry->block > block) {
			uint32_t hash = hash_entry(entry->id) % hash_table.size;

			if (hash_table.entries[hash] == i)
				hash_table.entries[hash]++;

			if (entry->link >= 0 && table.entries[entry->link].block > block) {
				entry->link++;
			}

			table.entries[i + 1] = *entry;
		} else {
			break;
		}
	}
	i++;

	uint32_t hash = hash_entry(id) % hash_table.size;
	int *link = &hash_table.entries[hash];
	while (*link >= 0 && *link > i) {
		struct table_entry *entry = table.entries + *link;
		link = &entry->link;
	}

	table.entries[i] = (struct table_entry) {
		.id.name = id.name,
		.id.type = id.type,
		.block = block,
		.link = *link,
	};

	*link = i;

	return table.entries + i;
}

struct table_entry *add_entry(struct entry_id id) {
	uint32_t hash = hash_entry(id) % hash_table.size;

	struct table_entry *new_entry = &ADD_ELEMENT(table.size, table.cap, table.entries);

	*new_entry = (struct table_entry) {
		.id.name = id.name,
		.id.type = id.type,
		.block = current_block,
		.link = hash_table.entries[hash],
	};

	hash_table.entries[hash] = table.size - 1;

	return new_entry;
}

// table_entry querying.
struct table_entry *symbols_add(enum entry_type type, struct string_view name) {
	struct table_entry *entry = get_entry((struct entry_id) { type, name }, 0);

	if (entry && entry->block == current_block)
		ICE("Name already declared, %.*s", name.len, name.str);

	return add_entry((struct entry_id) { type, name });
}

struct table_entry *symbols_get(enum entry_type type, struct string_view name) {
	return get_entry((struct entry_id) { type, name }, 0);
}

struct table_entry *symbols_get_in_current_scope(enum entry_type type, struct string_view name) {
	struct table_entry *entry = get_entry((struct entry_id) { type, name }, 0);

	return (entry && entry->block == current_block) ? entry : NULL;
}

// Identifier help functions.
struct symbol_identifier *symbols_add_identifier(struct string_view name) {
	return &symbols_add(ENTRY_IDENTIFIER, name)->identifier_data;
}

struct symbol_identifier *symbols_get_identifier(struct string_view name) {
	struct table_entry *entry = symbols_get(ENTRY_IDENTIFIER, name);
	return entry ? &entry->identifier_data : NULL;
}

struct symbol_identifier *symbols_get_identifier_in_current_scope(struct string_view name) {
	struct table_entry *entry = symbols_get_in_current_scope(ENTRY_IDENTIFIER, name);
	return entry ? &entry->identifier_data : NULL;
}

struct symbol_identifier *symbols_add_identifier_global(struct string_view name) {
	struct table_entry *entry = get_entry((struct entry_id) { ENTRY_IDENTIFIER, name }, 1);

	if (entry && entry->block == current_block)
		ICE("Name already declared, %.*s", name.len, name.str);

	return &add_entry_with_block((struct entry_id) { ENTRY_IDENTIFIER, name }, 0)->identifier_data;
}

struct symbol_identifier *symbols_get_identifier_global(struct string_view name) {
	struct table_entry *entry = get_entry((struct entry_id) { ENTRY_IDENTIFIER, name }, 1);
	return entry ? &entry->identifier_data : NULL;
}

struct type *symbols_get_identifier_type(struct symbol_identifier *symbol) {
	switch (symbol->type) {
	case IDENT_LABEL: return symbol->label.type;
	case IDENT_CONSTANT: return symbol->constant.data_type;
	case IDENT_VARIABLE: return symbol->variable.type;
	case IDENT_VARIABLE_LENGTH_ARRAY: return symbol->variable_length_array.type;
	default: NOTIMP();
	}
}

// Struct help functions.
struct symbol_struct *symbols_add_struct(struct string_view name) {
	return &symbols_add(ENTRY_STRUCT, name)->struct_data;
}

struct symbol_struct *symbols_get_struct(struct string_view name) {
	struct table_entry *entry = symbols_get(ENTRY_STRUCT, name);
	return entry ? &entry->struct_data : NULL;
}

struct symbol_struct *symbols_get_struct_in_current_scope(struct string_view name) {
	struct table_entry *entry = symbols_get_in_current_scope(ENTRY_STRUCT, name);
	return entry ? &entry->struct_data : NULL;
}

// Typedef help functions.
struct symbol_typedef *symbols_add_typedef(struct string_view name) {
	struct entry_id id = {ENTRY_TYPEDEF, name};
	struct table_entry *entry = get_entry(id, 0);

	if (entry && entry->block == current_block)
		return &entry->typedef_data;

	return &add_entry(id)->typedef_data;
}

struct symbol_typedef *symbols_get_typedef(struct string_view name) {
	struct table_entry *entry = symbols_get(ENTRY_TYPEDEF, name);
	return entry ? &entry->typedef_data : NULL;
}

// Init everything. Called from main.
void symbols_init(void) {
	hash_table.size = 1024;
	hash_table.entries = malloc(sizeof *hash_table.entries * hash_table.size);
	for (int i = 0; i < hash_table.size; i++)
		hash_table.entries[i] = -1;
}
