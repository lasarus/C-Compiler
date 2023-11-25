#ifndef EXPRESSION_TO_IR
#define EXPRESSION_TO_IR

#include "expression.h"

#include <ir/ir.h>

var_id expression_to_ir(struct expr *expr);
var_id expression_to_ir_clear_temp(struct expr *expr);

var_id expression_to_size_t(struct expr *expr);
var_id expression_to_int(struct expr *expr);
void expression_to_void(struct expr *expr);

#endif
