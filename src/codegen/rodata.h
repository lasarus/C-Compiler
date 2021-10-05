#ifndef RODATA_H
#define RODATA_H

// Returns label.

typedef int label_id;

label_id register_string(const char *str);
const char *get_label_name(label_id id);

void codegen_rodata(void);

#endif
