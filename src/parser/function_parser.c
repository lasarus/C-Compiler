#include "function_parser.h"
#include "expression_to_ir.h"
#include "ir/ir.h"
#include "symbols.h"
#include "expression.h"
#include "declaration.h"
#include "parser.h"

#include <common.h>
#include <preprocessor/preprocessor.h>
#include <abi/abi.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

struct jump_blocks {
	struct node *block_break,
		*block_continue;

	struct node *block_entry, *block_default;
	struct node *case_control;
};

static struct function_scope {
	size_t size, cap;

	struct function_scope_label {
		struct string_view label;
		struct node *block, *end_block;
		int used;
	} *labels;
} function_scope;

static void add_function_scope_label(struct string_view label, struct node *block, struct node *end_block, int used) {
	ADD_ELEMENT(function_scope.size, function_scope.cap, function_scope.labels) =
		(struct function_scope_label) { label, block, end_block, used };
}

// See section 6.8 of standard.
int parse_statement(struct jump_blocks *jump_blocks);
int parse_labeled_statement(struct jump_blocks *jump_blocks);
int parse_compound_statement(struct jump_blocks *jump_blocks);
int parse_expression_statement(void);
int parse_selection_statement(struct jump_blocks *jump_blocks);
int parse_iteration_statement(struct jump_blocks *jump_blocks);
int parse_jump_statement(struct jump_blocks *jump_blocks);

int parse_labeled_statement(struct jump_blocks *jump_blocks) {
	if (T0->type == T_IDENT &&
		T1->type == T_COLON) {
		struct string_view label = T0->str;
		TNEXT();
		TNEXT();

		struct node *goto_block = 0;

		for (unsigned i = 0; i < function_scope.size; i++) {
			if (sv_cmp(label, function_scope.labels[i].label)) {
				if (function_scope.labels[i].used)
					ERROR(T0->pos, "Label declared more than once %.*s", label.len, label.str);

				goto_block = function_scope.labels[i].end_block;
				function_scope.labels[i].used = 1;
				break;
			}
		}

		if (!goto_block) {
			goto_block = new_block();
			add_function_scope_label(label, goto_block, goto_block, 1);
		}

		ir_goto(goto_block);
		ir_block_start(goto_block);

		parse_statement(jump_blocks);
		return 1;
	} else if (TACCEPT(T_KCASE)) {
		struct expr *value = parse_expression();
		if (!value)
			ERROR(T0->pos, "Expected expression");
		TEXPECT(T_COLON);
		struct constant *constant = expression_to_constant(expression_cast(value, type_simple(ST_INT)));
		if (!constant)
			ERROR(T0->pos, "Expression not constant, is of type %d", value->type);

		struct node *case_control = jump_blocks->case_control;
		if (!case_control)
			ERROR(T0->pos, "Not currently in a switch statement");

		struct node *block_case = new_block(),
			*new_entry = new_block();

		ir_goto(block_case);
		ir_block_start(jump_blocks->block_entry);
		struct node *comparison = ir_equal(case_control,
												  ir_constant(*constant));

		struct node *block_true, *block_false;
		ir_if_selection(comparison, &block_true, &block_false);

		ir_connect(block_true, block_case);
		ir_connect(block_false, new_entry);

		ir_block_start(block_case);

		jump_blocks->block_entry = new_entry;

		parse_statement(jump_blocks);
		return 1;
	} else if (TACCEPT(T_KDEFAULT)) {
		TEXPECT(T_COLON);
		struct node *block_default = new_block();
		ir_goto(block_default);
		ir_block_start(block_default);
		if (!jump_blocks->block_entry)
			ERROR(T0->pos, "Not currently in a switch statement");
		jump_blocks->block_default = block_default;

		parse_statement(jump_blocks);
	}
	return 0;
}

int parse_compound_statement(struct jump_blocks *jump_blocks) {
	if (!TACCEPT(T_LBRACE))
		return 0;

	symbols_push_scope();
	while (parse_labeled_statement(jump_blocks) ||
		   parse_declaration(0) || parse_statement(jump_blocks));
	symbols_pop_scope();

	TEXPECT(T_RBRACE);
	return 1;
}

int parse_expression_statement(void) {
	struct expr *expr = parse_expression();
	if (!expr)
		return 0;
	TEXPECT(T_SEMI_COLON);
	expression_to_void(expr);
	return 1;
}

static int parse_switch(struct jump_blocks *jump_blocks) {
	if (!TACCEPT(T_KSWITCH))
		return 0;

	TEXPECT(T_LPAR);
	struct expr *condition = parse_expression();

	if (!condition)
		ERROR(T0->pos, "Expected expression");

	struct node *case_control = expression_to_int(condition);

	TEXPECT(T_RPAR);

	struct node *block_body = new_block(),
		*block_entry = new_block(),
		*block_end = new_block();

	ir_goto(block_entry);
	ir_block_start(block_body);

	struct jump_blocks new_jump_blocks = *jump_blocks;

	new_jump_blocks.block_entry = block_entry;
	new_jump_blocks.block_break = block_end;
	new_jump_blocks.case_control = case_control;
	new_jump_blocks.block_default = NULL;

	// Parse body
	parse_statement(&new_jump_blocks);

	if (new_jump_blocks.block_default)
		ir_connect(new_jump_blocks.block_entry, new_jump_blocks.block_default);
	else
		ir_connect(new_jump_blocks.block_entry, new_jump_blocks.block_break);

	ir_goto(block_end);

	ir_block_start(block_end);

	return 1;
}

int parse_selection_statement(struct jump_blocks *jump_blocks) {
	if (TACCEPT(T_KIF)) {
		TEXPECT(T_LPAR);
		struct expr *expr = parse_expression();
		if(!expr)
			ERROR(T0->pos, "Expected expression in if condition");

		struct node *condition = expression_to_ir(expr);

		TEXPECT(T_RPAR);
		struct node *block_true, *block_false;
		ir_if_selection(condition, &block_true, &block_false);

		ir_block_start(block_true);

		parse_statement(jump_blocks);

		if (TACCEPT(T_KELSE)) {
			struct node *block_end = new_block();
			ir_goto(block_end);
			ir_block_start(block_false);

			parse_statement(jump_blocks);

			ir_goto(block_end);
			ir_block_start(block_end);
		} else {
			struct node *block_end = new_block();
			ir_goto(block_end);
			ir_block_start(block_false);

			ir_goto(block_end);
			ir_block_start(block_end);
		}

		return 1;
	} else if (parse_switch(jump_blocks)) {
		return 1;
	} else {
		return 0;
	}
}

static int parse_do_while_statement(struct jump_blocks *jump_blocks) {
	if (!TACCEPT(T_KDO))
		return 0;

	struct node *block_body = new_block(),
		*block_control = new_block(),
		*block_end = new_block();

	ir_goto(block_body);
	ir_block_start(block_body);

	struct jump_blocks new_jump_blocks = *jump_blocks;
	new_jump_blocks.block_break = block_end;
	new_jump_blocks.block_continue = block_control;
	parse_statement(&new_jump_blocks);

	ir_goto(block_control);
	ir_block_start(block_control);

	TEXPECT(T_KWHILE);
	TEXPECT(T_LPAR);

	struct expr *control_expression = parse_expression();
	if (!control_expression)
		ERROR(T0->pos, "Expected expression");

	struct node *control_variable = expression_to_int(control_expression);

	TEXPECT(T_RPAR);

	struct node *block_true, *block_false;
	ir_if_selection(control_variable, &block_true, &block_false);
	ir_connect(block_true, block_body);
	ir_connect(block_false, block_end);

	ir_block_start(block_end);

	TEXPECT(T_SEMI_COLON);

	return 1;
}

static int parse_while_statement(struct jump_blocks *jump_blocks) {
	if (!TACCEPT(T_KWHILE))
		return 0;

	TEXPECT(T_LPAR);

	// control:
	// if-selection end or loop-body

	// loop-body:
	// ... body ...
	// jmp control

	// end:

	struct node *block_body = new_block(),
		*block_control = new_block(),
		*block_end = new_block();

	ir_goto(block_control);
	ir_block_start(block_control);

	struct expr *control_expression = parse_expression();
	if (!control_expression)
		ERROR(T0->pos, "Expected expression");

	TEXPECT(T_RPAR);

	struct node *control_variable = expression_to_int(control_expression);

	struct node *block_true, *block_false;
	ir_if_selection(control_variable, &block_true, &block_false);

	ir_connect(block_true, block_body);
	ir_connect(block_false, block_end);

	ir_block_start(block_body);

	struct jump_blocks new_jump_blocks = *jump_blocks;
	new_jump_blocks.block_break = block_end;
	new_jump_blocks.block_continue = block_control;
	parse_statement(&new_jump_blocks);

	ir_goto(new_jump_blocks.block_continue);

	ir_block_start(block_end);

	return 1;
}

static int parse_for_statement(struct jump_blocks *jump_blocks) {
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

	struct node *block_init = new_block(),
		*block_control = new_block(),
		*block_advance = new_block(),
		*block_body = new_block(),
		*block_end = new_block();

	ir_goto(block_init);
	ir_block_start(block_init); // not really necessary?

	if (!(TACCEPT(T_SEMI_COLON) ||
		  parse_declaration(0) ||
		  parse_expression_statement()))
		ERROR(T0->pos, "Invalid first part of for loop");

	ir_goto(block_control);
	ir_block_start(block_control);

	struct expr *condition = parse_expression();
	if (condition) {
		struct node *condition_variable = expression_to_int(condition);

		struct node *block_true, *block_false;
		ir_if_selection(condition_variable, &block_true, &block_false);
		ir_connect(block_true, block_body);
		ir_connect(block_false, block_end);
	} else {
		ir_goto(block_body);
	}

	TEXPECT(T_SEMI_COLON);

	ir_block_start(block_advance);

	struct expr *advance_expression = parse_expression();
	if (advance_expression) // Can be empty
		expression_to_void(advance_expression);

	ir_goto(block_control);
	ir_block_start(block_body);

	TEXPECT(T_RPAR);

	struct jump_blocks new_jump_blocks = *jump_blocks;
	new_jump_blocks.block_break = block_end;
	new_jump_blocks.block_continue = block_advance;
	parse_statement(&new_jump_blocks);

	ir_goto(block_advance);
	ir_block_start(block_end);

	symbols_pop_scope();
	return 1;
}

int parse_iteration_statement(struct jump_blocks *jump_blocks) {
	if (parse_for_statement(jump_blocks) ||
			   parse_do_while_statement(jump_blocks) ||
			   parse_while_statement(jump_blocks)) {
		return 1;
	} else {
		return 0;
	}
}

struct type *current_ret_val = NULL;

int parse_jump_statement(struct jump_blocks *jump_blocks) {
	if (TACCEPT(T_KGOTO)) {
		struct string_view label = T0->str;
		TNEXT();
		TEXPECT(T_SEMI_COLON);
		for (unsigned i = 0; i < function_scope.size; i++) {
			if (sv_cmp(label, function_scope.labels[i].label)) {
				// Necessary to avoid more than 2 predecessors
				// to each block.
				struct node *step = new_block();
				ir_goto(step);
				ir_block_start(step);
				ir_goto(function_scope.labels[i].block);
				ir_block_start(new_block());
				function_scope.labels[i].block = step;
				return 1;
			}
		}

		struct node *end_block = new_block();
		struct node *step = new_block();
		add_function_scope_label(label, step, end_block, 0);
		ir_goto(step);
		ir_block_start(step);
		ir_goto(end_block);
		ir_block_start(new_block());
		return 1;
	} else if (TACCEPT(T_KCONTINUE)) {
		struct node *step = new_block();
		ir_goto(step);
		ir_block_start(step);
		ir_goto(jump_blocks->block_continue);
		ir_block_start(new_block());
		jump_blocks->block_continue = step;
		TEXPECT(T_SEMI_COLON);
		return 1;
	} else if (TACCEPT(T_KBREAK)) {
		struct node *step = new_block();
		ir_goto(step);
		ir_block_start(step);
		ir_goto(jump_blocks->block_break);
		ir_block_start(new_block());
		jump_blocks->block_break = step;
		TEXPECT(T_SEMI_COLON);
		return 1;
	} else if (TACCEPT(T_KRETURN)) {
		struct expr *expr = parse_expression();
		TEXPECT(T_SEMI_COLON);

		struct evaluated_expression value = { .type = EE_VOID };

		if (expr)
			value = expression_evaluate(expression_cast(expr, current_ret_val));

		struct node *reg_state = NULL;
		abi_expr_return(get_current_function(), &value, &reg_state);
		ir_return(reg_state);
		ir_block_start(new_block());
		return 1;
	} else {
		return 0;
	}
}

int parse_statement(struct jump_blocks *jump_blocks) {
	return parse_labeled_statement(jump_blocks) ||
		parse_compound_statement(jump_blocks) ||
		parse_expression_statement() ||
		parse_selection_statement(jump_blocks) ||
		parse_iteration_statement(jump_blocks) ||
		parse_jump_statement(jump_blocks) ||
		parse_handle_pragma() ||
		TACCEPT(T_SEMI_COLON);
}

static struct string_view current_function = { 0 };

struct string_view get_current_function_name(void) {
	return current_function;
}

void parse_function(struct string_view name, struct type *type, int arg_n, struct symbol_identifier **args, int global) {
	(void)arg_n;
	current_function = name;
	struct symbol_identifier *symbol = symbols_get_identifier_global(name);

	function_scope.size = 0;

	current_ret_val = type->children[0];

	if (!symbol)
		symbol = symbols_add_identifier_global(name);

	symbol->type = IDENT_LABEL;
	symbol->label.type = type;
	symbol->label.name = name;

	assert(type->type == TY_FUNCTION);

	struct node *func = new_function(sv_to_str(name), global);
	abi_expr_function(func, type, args);

	type_evaluate_vla(type);

	struct jump_blocks jump_blocks = { 0 };
	parse_compound_statement(&jump_blocks);

	if (sv_string_cmp(name, "main")) {
		struct node *b = get_current_block();
		if (!b->block_info.end) {
			struct node *reg_state = NULL;
			struct constant c = constant_simple_signed(ST_INT, 0);
			abi_expr_return(get_current_function(), &(struct evaluated_expression) { .type = EE_CONSTANT, .data_type = c.data_type, .constant = c}, &reg_state);
			ir_return(reg_state);
		}
	} else if (current_ret_val == type_simple(ST_VOID)) {
		struct node *reg_state = NULL;
		abi_expr_return(get_current_function(), &(struct evaluated_expression) { .type = EE_VOID }, &reg_state);
		ir_return(reg_state);
	}
	
	symbols_pop_scope();
}
