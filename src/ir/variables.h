#ifndef VARIABLES_H
#define VARIABLES_H

struct variable_data {
	int size;
	int spans_block;
	int first_block;
	int used;
};

#define VOID_VAR 0
typedef int var_id;

struct type;

var_id new_variable_sz(int size, int allocate);
var_id new_variable(struct type *type, int allocate);
var_id new_ptr(void);

var_id allocate_vla(struct type *type);
void allocate_var(var_id var);
int get_n_vars(void);

void init_variables(void);

int get_variable_size(var_id variable);

void variables_reset(void);

struct variable_data *var_get_data(var_id variable);

#endif
