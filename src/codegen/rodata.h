#ifndef RODATA_H
#define RODATA_H

typedef int label_id;

label_id rodata_register(const char *str);
const char *rodata_get_label_string(label_id id);

label_id register_label_name(const char *str);

void rodata_codegen(void);

struct type;
struct initializer;
void data_register_static_var(const char *label, struct type *type, struct initializer *init, int global);
void data_codegen(void);

#endif
