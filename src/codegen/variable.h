#ifndef VARIABLE_H
#define VARIABLE_H

struct codegen_variable {
	enum {
		CVAR_VOID,
		CVAR_LABEL,
		CVAR_STACK,
		CVAR_REG,
		CVAR_CONSTANT
	} type;

	union {
		const char *label;
		struct {
			int offset;
		} stack;
		int reg;
		int constant;
	};
};

#endif
