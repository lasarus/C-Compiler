#ifndef CODEGEN_H
#define CODEGEN_H

void set_section(const char *section);

#include <parser/parser.h>
#include <common.h>
#include <stdio.h>

FILE *get_fp(void);
#define EMIT(STR, ...) do {						\
	fprintf(get_fp(), "%s" STR "\n",			\
			(str_contains(STR, ':') || STR[0] == '.') ? "" : "\t",	\
			##__VA_ARGS__); } while(0)

struct instruction *ir_next();

void codegen(const char *path);


#endif
