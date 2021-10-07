#ifndef RODATA_H
#define RODATA_H

typedef int label_id;

label_id rodata_register(const char *str);
const char *rodata_get_label_string(label_id id);

void rodata_codegen(void);

#endif
