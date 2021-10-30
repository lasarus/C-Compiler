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

struct jump_blocks {
	block_id block_break,
		block_continue;

	struct case_labels *case_labels;
};

struct function_scope {
	int size, cap;

	struct function_scope_label {
		const char *label;
		block_id id;
		int used;
	} *labels;
} function_scope;

void add_function_scope_label(const char *label, block_id id, int used) {
	ADD_ELEMENT(function_scope.size, function_scope.cap, function_scope.labels) =
		(struct function_scope_label) { label, id, used };
}

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
		const char *label = T0->str;
		TNEXT();
		TNEXT();

		for (int i = 0; i < function_scope.size; i++) {
			if (strcmp(label, function_scope.labels[i].label) == 0) {
				if (function_scope.labels[i].used)
					ERROR("Label declared more than once %s", label);
				ir_goto(function_scope.labels[i].id);
				ir_block_start(function_scope.labels[i].id);
				function_scope.labels[i].used = 1;
				return 1;
			}
		}

		block_id id = new_block();
		add_function_scope_label(label, id, 1);
		ir_goto(id);
		ir_block_start(id);
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
		ir_goto(block_case);
		ir_block_start(block_case);

		ADD_ELEMENT(labels->size, labels->cap, labels->labels) =
			(struct case_label) { block_case, *constant };

		return 1;
	} else if (TACCEPT(T_KDEFAULT)) {
		TEXPECT(T_COLON);
		block_id block_default = new_block();
		ir_goto(block_default);
		ir_block_start(block_default);
		if (!jump_blocks.case_labels)
			ERROR("Not currently in a switch statement");
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
	expression_to_ir_clear_temp(expr);
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

	ir_goto(block_control);
	ir_block_start(block_body);

	struct case_labels labels = { 0 };
	jump_blocks.case_labels = &labels;
	jump_blocks.block_break = block_end;

	// Parse body
	parse_statement(jump_blocks);

	ir_goto(block_end);
	ir_block_start(block_control);

	ir_switch_selection(expression_to_ir_clear_temp(expression_cast(condition, type_simple(ST_INT))),
						labels);

	ir_block_start(block_end);

	return 1;
}

int parse_selection_statement(struct jump_blocks jump_blocks) {
	if (TACCEPT(T_KIF)) {
		TEXPECT(T_LPAR);
		struct expr *expr = parse_expression();
		if(!expr)
			ERROR("Expected expression in if condition");

		var_id condition = expression_to_ir_clear_temp(expr);

		TEXPECT(T_RPAR);
		block_id block_true = new_block(),
			block_false = new_block();

		ir_if_selection(condition, block_true, block_false);

		ir_block_start(block_true);

		parse_statement(jump_blocks);

		if (TACCEPT(T_KELSE)) {
			int block_end = new_block();
			ir_goto(block_end);
			ir_block_start(block_false);

			parse_statement(jump_blocks);

			ir_goto(block_end);
			ir_block_start(block_end);
		} else {
			ir_goto(block_false);
			ir_block_start(block_false);
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

	ir_goto(block_body);
	ir_block_start(block_body);

	jump_blocks.block_break = block_end;
	jump_blocks.block_continue = block_control;
	parse_statement(jump_blocks);

	ir_goto(block_control);
	ir_block_start(block_control);

	TEXPECT(T_KWHILE);
	TEXPECT(T_LPAR);

	struct expr *control_expression = parse_expression();
	if (!control_expression)
		ERROR("Expected expression");

	var_id control_variable = expression_to_ir_clear_temp(control_expression);

	TEXPECT(T_RPAR);

	ir_if_selection(control_variable, block_body, block_end);

	ir_block_start(block_end);

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

	ir_goto(block_control);
	ir_block_start(block_control);

	struct expr *control_expression = parse_expression();
	if (!control_expression)
		ERROR("Expected expression");

	TEXPECT(T_RPAR);

	var_id control_variable = expression_to_ir_clear_temp(control_expression);

	ir_if_selection(control_variable, block_body, block_end);

	ir_block_start(block_body);

	jump_blocks.block_break = block_end;
	jump_blocks.block_continue = block_control;
	parse_statement(jump_blocks);

	ir_goto(block_control);
	ir_block_start(block_end);

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

	ir_goto(block_init);
	ir_block_start(block_init); // not really necessary?

	if (!(TACCEPT(T_SEMI_COLON) ||
		  parse_declaration(0) ||
		  parse_expression_statement()))
		ERROR("Invalid first part of for loop");

	ir_goto(block_control);
	ir_block_start(block_control);

	struct expr *condition = parse_expression();
	if (condition) {
		var_id condition_variable = expression_to_ir_clear_temp(condition);

		ir_if_selection(condition_variable, block_body, block_end);
	} else {
		ir_goto(block_body);
	}

	TEXPECT(T_SEMI_COLON);

	ir_block_start(block_advance);

	struct expr *advance_expression = parse_expression();
	if (advance_expression) // Can be empty
		expression_to_ir_clear_temp(advance_expression);

	ir_goto(block_control);
	ir_block_start(block_body);

	TEXPECT(T_RPAR);

	jump_blocks.block_break = block_end;
	jump_blocks.block_continue = block_advance;
	parse_statement(jump_blocks);

	ir_goto(block_advance);
	ir_block_start(block_end);

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
		const char *label = T0->str;
		TNEXT();
		TEXPECT(T_SEMI_COLON);
		for (int i = 0; i < function_scope.size; i++) {
			if (strcmp(label, function_scope.labels[i].label) == 0) {
				ir_goto(function_scope.labels[i].id);
				ir_block_start(new_block());
				return 1;
			}
		}

		block_id id = new_block();
		add_function_scope_label(label, id, 0);
		ir_goto(id);
		ir_block_start(new_block());
		return 1;
	} else if (TACCEPT(T_KCONTINUE)) {
		ir_goto(jump_blocks.block_continue);
		ir_block_start(new_block());
		TEXPECT(T_SEMI_COLON);
		return 1;
	} else if (TACCEPT(T_KBREAK)) {
		ir_goto(jump_blocks.block_break);
		ir_block_start(new_block());
		TEXPECT(T_SEMI_COLON);
		return 1;
	} else if (TACCEPT(T_KRETURN)) {
		struct expr *expr = parse_expression();
		TEXPECT(T_SEMI_COLON);

		if (!expr) {
			ir_return_void();
		} else {
			var_id return_variable = expression_to_ir_clear_temp(
				expression_cast(expr, current_ret_val));
			ir_return(return_variable, current_ret_val);
		}
		ir_block_start(new_block());
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

const char *get_current_function_name() {
	return current_function;
}

void parse_function(const char *name, struct type *type, int arg_n, var_id *args, int global) {
	current_function = name;
	struct symbol_identifier *symbol = symbols_get_identifier_global(name);

	function_scope.size = 0;

	current_ret_val = type->children[0];

	if (!symbol) {
		symbol = symbols_add_identifier_global(name);
	}

	symbol->type = IDENT_LABEL;
	symbol->label.type = type;
	symbol->label.name = name;

	assert(type->type == TY_FUNCTION);

	ir_new_function(type, args, name, global);
	ir_block_start(new_block());

	type_evaluate_vla(type);

	for (int i = 0; i < arg_n; i++) {
		allocate_var(args[i]);
	}

	struct jump_blocks jump_blocks = { 0 };
	parse_compound_statement(jump_blocks);

	if (strcmp(name, "main") == 0) {
		struct block *b = get_current_block();
		if (b->exit.type == BLOCK_EXIT_NONE)
			b->exit.type = BLOCK_EXIT_RETURN_ZERO;
	} else if (current_ret_val == type_simple(ST_VOID)) {
		ir_return_void();
	}
	
	symbols_pop_scope();
}
