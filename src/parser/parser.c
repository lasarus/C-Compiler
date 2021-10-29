#include "parser.h"
#include "declaration.h"
#include "symbols.h"

#include <common.h>
#include <preprocessor/preprocessor.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct parser_flags parser_flags = {
	.dmodel = DMODEL_LP64
};

block_id new_block() {
	static int block_counter = 1; // reserve space for null block.
	return block_counter++;
}

void parse_into_ir() {
	init_variables();

	while (parse_declaration(1));
	if (TACCEPT(T_SEMI_COLON)) {
		PRINT_POS(T0->pos);
		ERROR("Extra semicolon outside function.");
	}
	TEXPECT(T_EOI);
}

struct program program;

struct function *get_current_function(void) {
	return &program.functions[program.size - 1];
}

struct block *get_current_block(void) {
	struct function *func = get_current_function();
	return &func->blocks[func->size - 1];
}

void push_ir(struct instruction instruction) {
	struct block *block = get_current_block();

	ADD_ELEMENT(block->size, block->cap, block->instructions) = instruction;
}

void ir_block_start(block_id id) {
	struct function *func = get_current_function();

	ADD_ELEMENT(func->size, func->cap, func->blocks) = (struct block) {
		.id = id,
		.exit.type = BLOCK_EXIT_NONE
	};
}

void ir_new_function(struct type *signature, var_id *arguments, const char *name, int is_global) {
	ADD_ELEMENT(program.size, program.cap, program.functions) = (struct function) {
		.signature = signature,
		.named_arguments = arguments,
		.name = name,
		.is_global = is_global
	};
}

struct program *get_program(void) {
	return &program;
}

void print_instruction(struct instruction instruction) {
	const char *str = dbg_instruction(instruction);
	printf("\t%s\n", str);
}

enum operand_type ot_from_st(enum simple_type st) {
	switch (st) {
	case ST_INT: return OT_INT;
	case ST_UINT: return OT_UINT;
	case ST_LONG: return OT_LONG;
	case ST_ULONG: return OT_ULONG;
	case ST_LLONG: return OT_LLONG;
	case ST_ULLONG: return OT_ULLONG;
	case ST_FLOAT: return OT_FLOAT;
	case ST_DOUBLE: return OT_DOUBLE;
	default: ERROR("Invalid operand type %d", st);
	}
}

enum operand_type ot_from_type(struct type *type) {
	if (type_is_pointer(type)) {
		return OT_PTR;
	} else if (type->type == TY_SIMPLE) {
		return ot_from_st(type->simple);
	} else {
		ERROR("Invalid operand type");
	}
}


void allocate_var(var_id var) {
	struct function *func = &program.functions[program.size - 1];
	ADD_ELEMENT(func->var_size, func->var_cap, func->vars) = var;
}

void ir_if_selection(var_id condition, block_id block_true, block_id block_false) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_IF;
	block->exit.if_.condition = condition;
	block->exit.if_.block_true = block_true;
	block->exit.if_.block_false = block_false;
}

void ir_switch_selection(var_id condition, struct case_labels labels) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_SWITCH;
	block->exit.switch_.condition = condition;
	block->exit.switch_.labels = labels;
}

void ir_goto(block_id jump) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_JUMP;
	block->exit.jump = jump;
}

void ir_return(var_id value, struct type *type) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_RETURN;
	block->exit.return_.type = type;
	block->exit.return_.value = value;
}

void ir_return_void(void) {
	struct block *block = get_current_block();
	assert(block->exit.type == BLOCK_EXIT_NONE);

	block->exit.type = BLOCK_EXIT_RETURN;
	block->exit.return_.type = type_simple(ST_VOID);
	block->exit.return_.value = 0;
}


void ir_init_var(struct initializer *init, var_id result) {
	IR_PUSH_SET_ZERO(result);
	var_id base_address = new_variable(type_pointer(type_simple(ST_VOID)), 1, 1);
	IR_PUSH_ADDRESS_OF(base_address, result);
	var_id member_address = new_variable(type_pointer(type_simple(ST_VOID)), 1, 1);

	for (int i = 0; i < init->size; i++) {
		IR_PUSH_GET_OFFSET(member_address, base_address, init->pairs[i].offset);
		IR_PUSH_STORE(expression_to_ir(init->pairs[i].expr), member_address);
	}
}
