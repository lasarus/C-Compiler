#ifndef FUNCTION_PARSER_H
#define FUNCTION_PARSER_H

#include "parser.h"

void parse_function(struct string_view name, struct type *type, int arg_n, var_id *args, int global);
struct string_view get_current_function_name(void);

#endif
