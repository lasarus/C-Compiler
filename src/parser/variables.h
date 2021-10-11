#ifndef VARIABLES_H
#define VARIABLES_H

#define VOID_VAR 0
typedef int var_id;

struct type;

var_id new_variable_sz(int size, int allocate);
var_id new_variable(struct type *type, int allocate);
var_id allocate_vla(struct type **type);
void allocate_var(var_id var);
int get_n_vars(void);

void init_variables(void);

// void change_variable_type(var_id var, struct type *type);

//struct type *get_variable_type(var_id variable);
int get_variable_size(var_id variable);
void change_variable_size(var_id variable, int size);

#endif
