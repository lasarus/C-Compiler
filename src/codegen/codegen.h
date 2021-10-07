#ifndef CODEGEN_H
#define CODEGEN_H

void set_section(const char *section);
void emit(const char *fmt, ...);

void codegen(const char *path);

#endif
