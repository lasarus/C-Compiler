#ifndef CODEGEN_H
#define CODEGEN_H

extern struct codegen_flags {
	enum {
		CMODEL_SMALL,
		CMODEL_LARGE
	} cmodel;
} codegen_flags;

void set_section(const char *section);
void emit(const char *fmt, ...);

void codegen(const char *path);

#endif
