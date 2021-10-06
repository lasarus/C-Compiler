#ifndef CODEGEN_H
#define CODEGEN_H

void set_section(const char *section);

#include <parser/parser.h>
#include <common.h>
#include <stdio.h>

void emit(const char *fmt, ...);

void codegen(const char *path);

#endif
