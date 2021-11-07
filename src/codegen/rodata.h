#ifndef RODATA_H
#define RODATA_H

#include <string_view.h>

typedef int label_id;

label_id rodata_register(struct string_view str);
const char *rodata_get_label_string(label_id id);

label_id register_label_name(struct string_view str);

void rodata_codegen(void);

struct type;
struct initializer;
void data_register_static_var(struct string_view label, struct type *type, struct initializer *init, int global);
void data_codegen(void);

#endif
