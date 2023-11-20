#ifndef CODEGEN_H
#define CODEGEN_H

#include <assembler/assembler.h>

enum code_model {
	CODE_MODEL_SMALL,
	CODE_MODEL_LARGE,
};

extern struct codegen_flags {
	enum code_model code_model;
	int debug_stack_size;
	int debug_stack_min;
} codegen_flags;

struct variable_info {
	enum {
		VAR_STOR_NONE,
		VAR_STOR_STACK
	} storage;

	int stack_location;
};

extern struct variable_info *variable_info;

void codegen(void);

// TODO: Why is rdi not destination?
// From rdi to rsi address
void codegen_memcpy(int len);

#endif
