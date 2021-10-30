#ifndef VARIABLES_H
#define VARIABLES_H

#define VOID_VAR 0
typedef int var_id;

struct type;

var_id new_variable_sz(int size, int allocate, int stack_bucket);
var_id new_variable(struct type *type, int allocate, int stack_bucket);

var_id allocate_vla(struct type *type);
void allocate_var(var_id var);
int get_n_vars(void);

void init_variables(void);

int get_variable_size(var_id variable);
int get_variable_stack_bucket(var_id variable);
void change_variable_size(var_id variable, int size);
void variable_set_stack_bucket(var_id variable, int stack_bucket);

#endif
