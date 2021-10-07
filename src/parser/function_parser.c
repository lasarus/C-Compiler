#include "function_parser.h"
#include "symbols.h"
#include "expression.h"
#include "declaration.h"
#include "parser.h"

#include <common.h>
#include <preprocessor/preprocessor.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

struct case_labels {
	int n;
	block_id *blocks;
	block_id default_;
	struct constant *values;
};

struct jump_blocks {
	block_id block_break,
		block_continue;

	struct case_labels *case_labels;
};

// See section 6.8 of standard.
int parse_statement(struct jump_blocks jump_blocks);
int parse_labeled_statement(struct jump_blocks jump_blocks);
int parse_compound_statement(struct jump_blocks jump_blocks);
int parse_expression_statement(void);
int parse_selection_statement(struct jump_blocks jump_blocks);
int parse_iteration_statement(struct jump_blocks jump_blocks);
int parse_jump_statement(struct jump_blocks jump_blocks);

int parse_labeled_statement(struct jump_blocks jump_blocks) {
	if (TPEEK(0)->type == T_IDENT &&
		TPEEK(1)->type == T_COLON) {
		ERROR("Not implemented");
		return 1;
	} else if (TACCEPT(T_KCASE)) {
		struct expr *value = parse_expression();
		if (!value)
			ERROR("Expected expression");
		TEXPECT(T_COLON);
		struct constant *constant = expression_to_constant(expression_cast(value, type_simple(ST_INT)));
		if (!constant)
			ERROR("Expression not constant, is of type %d", value->type);

		struct case_labels *labels = jump_blocks.case_labels;
		if (!labels)
			ERROR("Not currently in a switch statement");

		block_id block_case = new_block();
		IR_PUSH_GOTO(block_case);
		IR_PUSH_START_BLOCK(block_case);

		labels->n++;
		labels->blocks = realloc(labels->blocks, sizeof (*labels->blocks) * labels->n);
		labels->values = realloc(labels->values, sizeof (*labels->values) * labels->n);
		labels->blocks[labels->n - 1] = block_case;
		labels->values[labels->n - 1] = *constant;

		return 1;
	} else if (TACCEPT(T_KDEFAULT)) {
		TEXPECT(T_COLON);
		block_id block_default = new_block();
		IR_PUSH_GOTO(block_default);
		IR_PUSH_START_BLOCK(block_default);
		jump_blocks.case_labels->default_ = block_default;
	}
	return 0;
}

int parse_compound_statement(struct jump_blocks jump_blocks) {
	if (!TACCEPT(T_LBRACE))
		return 0;

	symbols_push_scope();
	while (parse_declaration(0) ||
		   parse_statement(jump_blocks));
	symbols_pop_scope();

	TEXPECT(T_RBRACE);
	return 1;
}

int parse_expression_statement(void) {
	struct expr *expr = parse_expression();
	if (!expr)
		return 0;
	TEXPECT(T_SEMI_COLON);
	expression_to_ir(expr);
	return 1;
}

int parse_switch(struct jump_blocks jump_blocks) {
	if (!TACCEPT(T_KSWITCH))
		return 0;

	TEXPECT(T_LPAR);
	struct expr *condition = parse_expression();
	if (!condition)
		ERROR("Expected expression");
	TEXPECT(T_RPAR);

	int block_body = new_block(),
		block_control = new_block(),
		block_end = new_block();

	IR_PUSH_GOTO(block_control);
	IR_PUSH_START_BLOCK(block_body);

	struct case_labels labels = { 0 };
	jump_blocks.case_labels = &labels;
	jump_blocks.block_break = block_end;

	// Parse body
	parse_statement(jump_blocks);

	IR_PUSH_GOTO(block_end);
	IR_PUSH_START_BLOCK(block_control);

	IR_PUSH(.type = IR_SWITCH_SELECTION,
			.switch_selection = {
				.condition = expression_to_ir(expression_cast(condition, type_simple(ST_INT))),
				.n = labels.n,
				.values = labels.values,
				.blocks = labels.blocks,
				.default_ = labels.default_
			});

	IR_PUSH_GOTO(block_end);
	IR_PUSH_START_BLOCK(block_end);

	return 1;
}

int parse_selection_statement(struct jump_blocks jump_blocks) {
	if (TACCEPT(T_KIF)) {
		TEXPECT(T_LPAR);
		struct expr *expr = parse_expression();
		if(!expr)
			ERROR("Expected expression in if condition");

		var_id condition = expression_to_ir(expr);

		TEXPECT(T_RPAR);
		block_id block_true = new_block(),
			block_false = new_block();

		IR_PUSH(.type = IR_IF_SELECTION,
				.if_selection = {condition, block_true, block_false});

		IR_PUSH_START_BLOCK(block_true);

		parse_statement(jump_blocks);

		if (TACCEPT(T_KELSE)) {
			int block_end = new_block();
			IR_PUSH_GOTO(block_end);
			IR_PUSH_START_BLOCK(block_false);

			parse_statement(jump_blocks);

			IR_PUSH_GOTO(block_end);
			IR_PUSH_START_BLOCK(block_end);
		} else {
			IR_PUSH_GOTO(block_false);
			IR_PUSH_START_BLOCK(block_false);
		}

		return 1;
	} else if (parse_switch(jump_blocks)) {
		return 1;
	} else {
		return 0;
	}
}

int parse_do_while_statement(struct jump_blocks jump_blocks) {
	if (!TACCEPT(T_KDO))
		return 0;

	block_id block_body = new_block(),
		block_control = new_block(),
		block_end = new_block();

	IR_PUSH_START_BLOCK(block_body);


	jump_blocks.block_break = block_end;
	jump_blocks.block_continue = block_control;
	parse_statement(jump_blocks);

	IR_PUSH_START_BLOCK(block_control);

	TEXPECT(T_KWHILE);
	TEXPECT(T_LPAR);

	struct expr *control_expression = parse_expression();
	if (!control_expression)
		ERROR("Expected expression");

	var_id control_variable = expression_to_ir(control_expression);

	TEXPECT(T_RPAR);

	IR_PUSH(.type = IR_IF_SELECTION,
			.if_selection = {control_variable, block_body, block_end});

	IR_PUSH_START_BLOCK(block_end);

	TEXPECT(T_SEMI_COLON);

	return 1;
}

int parse_while_statement(struct jump_blocks jump_blocks) {
	if (!TACCEPT(T_KWHILE))
		return 0;

	TEXPECT(T_LPAR);

	// control:
	// if-selection end or loop-body

	// loop-body:
	// ... body ...
	// jmp control

	// end:

	block_id block_body = new_block(),
		block_control = new_block(),
		block_end = new_block();

	IR_PUSH_START_BLOCK(block_control);

	struct expr *control_expression = parse_expression();
	if (!control_expression)
		ERROR("Expected expression");

	TEXPECT(T_RPAR);

	var_id control_variable = expression_to_ir(control_expression);

	IR_PUSH(.type = IR_IF_SELECTION,
			.if_selection = {control_variable, block_body, block_end});

	IR_PUSH_START_BLOCK(block_body);

	jump_blocks.block_break = block_end;
	jump_blocks.block_continue = block_control;
	parse_statement(jump_blocks);

	IR_PUSH_GOTO(block_control);
	IR_PUSH_START_BLOCK(block_end);

	return 1;
}

int parse_for_statement(struct jump_blocks jump_blocks) {
	if (!TACCEPT(T_KFOR))
		return 0;

	TEXPECT(T_LPAR);

	symbols_push_scope();

	// init:
	// ... init stuff ...
	// jmp control

	// control:
	// if-selection ast
	// 

	// advance:
	// .. advance statment ...
	// jmp control

	// loop-body:
	// ... body ...
	// jmp advance

	// end:

	block_id block_init = new_block(),
		block_control = new_block(),
		block_advance = new_block(),
		block_body = new_block(),
		block_end = new_block();

	IR_PUSH_START_BLOCK(block_init); // not really necessary?

	if (!(TACCEPT(T_SEMI_COLON) ||
		  parse_declaration(0) ||
		  parse_expression_statement()))
		ERROR("Invalid first part of for loop");

	IR_PUSH_GOTO(block_control);
	IR_PUSH_START_BLOCK(block_control);

	struct expr *condition = parse_expression();
	if (condition) {
		var_id condition_variable = expression_to_ir(condition);

		IR_PUSH(.type = IR_IF_SELECTION,
				.if_selection = {condition_variable, block_body, block_end});
	} else {
		IR_PUSH_GOTO(block_body);
	}

	TEXPECT(T_SEMI_COLON);

	IR_PUSH_START_BLOCK(block_advance);

	struct expr *advance_expression = parse_expression();
	if (advance_expression) // Can be empty
		expression_to_ir(advance_expression);

	IR_PUSH_GOTO(block_control);
	IR_PUSH_START_BLOCK(block_body);

	TEXPECT(T_RPAR);

	jump_blocks.block_break = block_end;
	jump_blocks.block_continue = block_advance;
	parse_statement(jump_blocks);

	IR_PUSH_GOTO(block_advance);
	IR_PUSH_START_BLOCK(block_end);

	symbols_pop_scope();
	return 1;
}

int parse_iteration_statement(struct jump_blocks jump_blocks) {
	if (parse_for_statement(jump_blocks) ||
			   parse_do_while_statement(jump_blocks) ||
			   parse_while_statement(jump_blocks)) {
		return 1;
	} else {
		return 0;
	}
}

struct type *current_ret_val = NULL;

int parse_jump_statement(struct jump_blocks jump_blocks) {
	if (TACCEPT(T_KGOTO)) {
		ERROR("Not implemented");
		return 1;
	} else if (TACCEPT(T_KCONTINUE)) {
		IR_PUSH_GOTO(jump_blocks.block_continue);
		TEXPECT(T_SEMI_COLON);
		return 1;
	} else if (TACCEPT(T_KBREAK)) {
		IR_PUSH_GOTO(jump_blocks.block_break);
		TEXPECT(T_SEMI_COLON);
		return 1;
	} else if (TACCEPT(T_KRETURN)) {
		struct expr *expr = parse_expression();
		TEXPECT(T_SEMI_COLON);

		if (!expr) {
			IR_PUSH_RETURN_VOID();
		} else {
			var_id return_variable = expression_to_ir(
				expression_cast(expr, current_ret_val));
			IR_PUSH_RETURN_VALUE(return_variable);
		}
		return 1;
	} else {
		return 0;
	}
}

int parse_statement(struct jump_blocks jump_blocks) {
	return parse_labeled_statement(jump_blocks) ||
		parse_compound_statement(jump_blocks) ||
		parse_expression_statement() ||
		parse_selection_statement(jump_blocks) ||
		parse_iteration_statement(jump_blocks) ||
		parse_jump_statement(jump_blocks) ||
		TACCEPT(T_SEMI_COLON);
}

static const char *current_function = "";

const char *get_current_function() {
	return current_function;
}

void parse_function(const char *name, struct type *type, int arg_n, char **arg_names, int global) {
	current_function = name;
	struct symbol_identifier *symbol = symbols_get_identifier(name);

	current_ret_val = type->children[0];

	if (!symbol) {
		symbol = symbols_add_identifier(name);
	}

	symbol->type = IDENT_LABEL;
	symbol->label.type = type;
	symbol->label.name = name;

	if (arg_n && !arg_names)
		ERROR("Should not be null");

	symbols_push_scope();

	var_id *vars = malloc(sizeof(*vars) * arg_n);
	
	assert(type->type == TY_FUNCTION);
	for (int i = 0; i < arg_n; i++) {
		char *arg_name = arg_names[i];
		struct type *arg_type = type->children[i + 1];

		var_id arg_var = new_variable(arg_type, 0); // Don't allocate yet.
		struct symbol_identifier *arg_sym = symbols_add_identifier(arg_name);

		arg_sym->type = IDENT_VARIABLE;
		arg_sym->variable = arg_var;
		vars[i] = arg_var;
	}

	IR_PUSH_FUNCTION(type, vars, name, global);

	for (int i = 0; i < arg_n; i++) {
		allocate_var(vars[i]);
	}

	struct jump_blocks jump_blocks = { 0 };
	parse_compound_statement(jump_blocks);
	symbols_pop_scope();
}
