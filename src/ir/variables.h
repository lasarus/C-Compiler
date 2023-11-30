#ifndef VARIABLES_H
#define VARIABLES_H

#define VOID_VAR 0
typedef int var_id;

struct type;
struct instruction;

var_id new_variable(struct instruction *instruction, int size);

var_id allocate_vla(struct type *type);
void allocate_var(var_id var);
int get_n_vars(void);

void init_variables(void);

int get_variable_size(var_id variable);

void variables_reset(void);

struct instruction *var_get_instruction(var_id variable);

#endif
