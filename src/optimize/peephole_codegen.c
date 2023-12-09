#include "peephole.h"
#include "common.h"

#include <ir/ir.h>
#include <assembler/assembler.h>

#include <assert.h>

static struct node **worklist = NULL;
static size_t worklist_size = 0, worklist_cap = 0;

enum accept_type {
	ACCEPT_NONE,
	ACCEPT_INTEGER,
	ACCEPT_REGISTER,
};

struct pattern {
	int ir_type;
	enum accept_type matches[3];
	int matches_sizes[3];
	int size;
	int input[3], output[3];

	struct asm_instruction *instructions;
};

/* const char *tmp = "(IR_ADD (int:4) (reg:4)) -> \"addl {0} {1}\""; */
/* const char *tmp = "(IR_ADD (int:4) (reg:4)) -> \"addl {0} {1}\""; */

// When searching this we create a string then try to find a mattern that matches.
/* {IR_ADD, {"i4*4"}, {"addl", PLACE_(0), PLACE_(1)}} */
/* {IR_ADD, {"*4i4"}, {"addl", "1 2"}} */
/* {IR_ADD, {"*4*4"}, {"addl", "1 2"}} */
/* {IR_ADD, {"i8*8"}, {"addq", "1 2"}} */
/* {IR_ADD, {"*8i8"}, {"addq", "1 2"}} */
/* {IR_ADD, {"*8*8"}, {"addq", "1 2"}} */
/* {IR_LSHIFT, {"%rax,i4"}, {"addl", "!i4", "%eax"}} */

/* struct pattern patterns[] = { */
/* 	{ */
/* 		.ir_type = IR_ADD, */
/* 		.matches = {ACCEPT_INTEGER, ACCEPT_REGISTER}, */
/* 		.matches_sizes = {4, 4}, */
/* 		.size = 4, */
/* 		.input = {1}, .output = {1}, */
/* 		.instructions = (struct asm_instruction []) { */
/* 			{"addl", {PLACE_(0), PLACE_(1)}}, */
/* 			{ 0 }, */
/* 		} */
/* 	}, */
/* 	{ */
/* 		.ir_type = IR_ADD, */
/* 		.matches = {ACCEPT_REGISTER, ACCEPT_INTEGER}, */
/* 		.matches_sizes = {4, 4}, */
/* 		.size = 4, */
/* 		.input = {0}, .output = {0}, */
/* 		.instructions = (struct asm_instruction []) { */
/* 			{"addl", {PLACE_(1), PLACE_(0)}}, */
/* 			{ 0 }, */
/* 		} */
/* 	} */
/* }; */

/* static int matches_pattern(struct node *node, struct pattern *pattern) { */
/* 	if (pattern->ir_type != (int)node->type) */
/* 		return 0; */

/* 	for (unsigned i = 0; i < 3; i++) { */
/* 		if (pattern->matches[i] == ACCEPT_NONE) { */
/* 			if (node->arguments[i]) */
/* 				return 0; */
/* 		} else if (pattern->matches[i] == ACCEPT_INTEGER) { */
/* 			if (!(node->arguments[i] && node->arguments[i]->type == IR_INTEGER)) */
/* 				return 0; */
/* 		} else if (pattern->matches[i] == ACCEPT_REGISTER) { */
/* 			if (!node->arguments[i]) */
/* 				return 0; */
/* 		} */
/* 	} */

/* 	for (unsigned i = 0; i < 3; i++) { */
/* 		if (pattern->matches[i] == ACCEPT_NONE) */
/* 			continue; */

/* 		if (!node->arguments[i]->size || */
/* 			pattern->matches_sizes[i] != node->arguments[i]->size) */
/* 			return 0; */
/* 	} */

/* 	return 1; */
/* } */

static int peep_binary_int_a(struct node *node, int type, const char *mnemonic4, const char *mnemonic8) {
	if ((int)node->type != type)
		return 0;

	if (node->size != 4 && node->size != 8)
		return 0;

	if (!node->arguments[0] || !node->arguments[1])
		return 0;

	if (node->arguments[0]->type != IR_INTEGER)
		return 0;

	if (node->arguments[0]->integer.integer >= INT32_MAX)
		return 0;

	struct node *new_node =
		ir_new2(IR_ASSEMBLY, NULL, node->arguments[1], node->size);
	uint64_t imm = node->arguments[0]->integer.integer;

	struct asm_instruction *instruction = cc_malloc(sizeof *instruction);
	if (node->size == 4)
		*instruction = (struct asm_instruction) {mnemonic4, {IMM_(imm), R4_(REG_RAX)}};
	else
		*instruction = (struct asm_instruction) {mnemonic8, {IMM_(imm), R8_(REG_RAX)}};
	new_node->assembly.instructions = instruction;

	new_node->assembly.len = 1;

	ir_replace_node(node, new_node);

	return 1;
}

static int peep_binary_int_b(struct node *node, int type, const char *mnemonic4, const char *mnemonic8) {
	if ((int)node->type != type)
		return 0;

	if (node->size != 4 && node->size != 8)
		return 0;

	if (!node->arguments[0] || !node->arguments[1])
		return 0;

	if (node->arguments[1]->type != IR_INTEGER)
		return 0;

	if (node->arguments[1]->integer.integer >= INT32_MAX)
		return 0;

	struct node *new_node =
		ir_new2(IR_ASSEMBLY, NULL, node->arguments[0], node->size);
	uint64_t imm = node->arguments[1]->integer.integer;

	struct asm_instruction *instruction = cc_malloc(sizeof *instruction);
	if (node->size == 4)
		*instruction = (struct asm_instruction) {mnemonic4, {IMM_(imm), R4_(REG_RAX)}};
	else
		*instruction = (struct asm_instruction) {mnemonic8, {IMM_(imm), R8_(REG_RAX)}};
	new_node->assembly.instructions = instruction;

	new_node->assembly.len = 1;

	ir_replace_node(node, new_node);

	return 1;
}

int peep_binary_comp_b(struct node *node, int type, const char *mnemonic) {
	if ((int)node->type != type)
		return 0;

	if (node->size != 4 && node->size != 8)
		return 0;

	if (!node->arguments[0] || !node->arguments[1])
		return 0;

	if (node->arguments[1]->type != IR_INTEGER)
		return 0;

	if (node->arguments[1]->integer.integer >= INT32_MAX)
		return 0;

	struct node *new_node =
		ir_new2(IR_ASSEMBLY, NULL, node->arguments[0], node->size);
	uint64_t imm = node->arguments[1]->integer.integer;

	struct asm_instruction *instructions = cc_malloc(sizeof *instructions * 3);

	if (node->size == 4) {
		instructions[0] = (struct asm_instruction) { "cmpl", {IMM_(imm), R4_(REG_RAX)}};
		instructions[1] = (struct asm_instruction) { mnemonic, {R1_(REG_RAX)}};
		instructions[2] = (struct asm_instruction) { "movzbl", {R1_(REG_RAX), R4_(REG_RAX)}};
	} else {
		instructions[0] = (struct asm_instruction) { "cmpq", {IMM_(imm), R8_(REG_RAX)}};
		instructions[1] = (struct asm_instruction) { mnemonic, {R1_(REG_RAX)}};
		instructions[2] = (struct asm_instruction) { "movzbl", {R1_(REG_RAX), R4_(REG_RAX)}};
	}
	new_node->assembly.instructions = instructions;

	new_node->assembly.len = 3;

	ir_replace_node(node, new_node);

	return 1;
}

static int peep_binary_comp_a(struct node *node, int type, const char *mnemonic) {
	if ((int)node->type != type)
		return 0;

	if (node->size != 4 && node->size != 8)
		return 0;

	if (!node->arguments[0] || !node->arguments[1])
		return 0;

	if (node->arguments[0]->type != IR_INTEGER)
		return 0;

	if (node->arguments[0]->integer.integer >= INT32_MAX)
		return 0;

	struct node *new_node =
		ir_new2(IR_ASSEMBLY, NULL, node->arguments[1], node->size);
	uint64_t imm = node->arguments[0]->integer.integer;

	struct asm_instruction *instructions = cc_malloc(sizeof *instructions * 3);

	if (node->size == 4) {
		instructions[0] = (struct asm_instruction) { "cmpl", {IMM_(imm), R4_(REG_RAX)}};
		instructions[1] = (struct asm_instruction) { mnemonic, {R1_(REG_RAX)}};
		instructions[2] = (struct asm_instruction) { "movzbl", {R1_(REG_RAX), R4_(REG_RAX)}};
	} else {
		instructions[0] = (struct asm_instruction) { "cmpq", {IMM_(imm), R8_(REG_RAX)}};
		instructions[1] = (struct asm_instruction) { mnemonic, {R1_(REG_RAX)}};
		instructions[2] = (struct asm_instruction) { "movzbl", {R1_(REG_RAX), R4_(REG_RAX)}};
	}
	new_node->assembly.instructions = instructions;

	new_node->assembly.len = 3;

	ir_replace_node(node, new_node);

	return 1;
}

int peep_load(struct node *node) {
	if (node->type != IR_LOAD)
		return 0;

	if (node->arguments[0]->type != IR_LABEL ||
		node->arguments[0]->label.reference)
		return 0;

	if (node->size != 1 && node->size != 2 &&
		  node->size != 4 && node->size != 8)
		return 0;

	struct node *new_node =
		ir_new2(IR_ASSEMBLY, node->arguments[1], NULL, node->size);

	struct asm_instruction *instructions = cc_malloc(sizeof *instructions * 2);
	new_node->assembly.instructions = instructions;

	struct node *label_node = node->arguments[0];
	struct operand imm_operand = IMML(label_node->label.label,
									  label_node->label.offset);

	instructions[0] = (struct asm_instruction) {"movq", {imm_operand, R8_(REG_RAX)}};
	switch (node->size) {
	case 1:
		instructions[1] = (struct asm_instruction) {"movb", {MEM_(0, REG_RAX), R1_(REG_RAX)}};
		break;
	case 2:
		instructions[1] = (struct asm_instruction) {"movw", {MEM_(0, REG_RAX), R2_(REG_RAX)}};
		break;
	case 4:
		instructions[1] = (struct asm_instruction) {"movl", {MEM_(0, REG_RAX), R4_(REG_RAX)}};
		break;
	case 8:
		instructions[1] = (struct asm_instruction) {"movq", {MEM_(0, REG_RAX), R8_(REG_RAX)}};
		break;
	default: NOTIMP();
	}
	new_node->assembly.len = 2;

	ir_replace_node(node, new_node);
		
	return 1;
}

int peep_store(struct node *node) {
	if (node->type != IR_STORE)
		return 0;

	if (node->arguments[0]->type != IR_LABEL ||
		node->arguments[0]->label.reference)
		return 0;

	if (node->arguments[1]->type != IR_INTEGER)
		return 0;

	struct node *value_node = node->arguments[1];

	if (value_node->size != 1 && value_node->size != 2 &&
		value_node->size != 4 && value_node->size != 8)
		return 0;

	struct node *new_node =
		ir_new2(IR_ASSEMBLY_STATE, node->arguments[2], NULL, node->size);

	struct asm_instruction *instructions = cc_malloc(sizeof *instructions * 2);
	new_node->assembly.instructions = instructions;

	struct node *label_node = node->arguments[0];
	struct operand imm_operand = IMML(label_node->label.label,
									  label_node->label.offset);

	uint64_t value = value_node->integer.integer;

	instructions[0] = (struct asm_instruction) {"movq", {imm_operand, R8_(REG_RAX)}};
	switch (value_node->size) {
	case 1:
		instructions[1] = (struct asm_instruction) {"movb", {IMM_(value), MEM_(0, REG_RAX)}};
		break;
	case 2:
		instructions[1] = (struct asm_instruction) {"movw", {IMM_(value), MEM_(0, REG_RAX)}};
		break;
	case 4:
		instructions[1] = (struct asm_instruction) {"movl", {IMM_(value), MEM_(0, REG_RAX)}};
		break;
	case 8:
		instructions[1] = (struct asm_instruction) {"movq", {IMM_(value), MEM_(0, REG_RAX)}};
		break;
	default: NOTIMP();
	}
	new_node->assembly.len = 2;

	ir_replace_node(node, new_node);
		
	return 1;
}

int peep_store_label(struct node *node) {
	if (node->type != IR_STORE)
		return 0;

	if (node->arguments[0]->type != IR_LABEL ||
		node->arguments[0]->label.reference)
		return 0;

	struct node *value_node = node->arguments[1];

	if (value_node->size != 1 && value_node->size != 2 &&
		value_node->size != 4 && value_node->size != 8)
		return 0;

	struct node *new_node =
		ir_new2(IR_ASSEMBLY_STATE, node->arguments[2], node->arguments[1], node->size);

	struct asm_instruction *instructions = cc_malloc(sizeof *instructions * 2);
	new_node->assembly.instructions = instructions;

	struct node *label_node = node->arguments[0];
	struct operand imm_operand = IMML(label_node->label.label,
									  label_node->label.offset);

	instructions[0] = (struct asm_instruction) {"movq", {imm_operand, R8_(REG_RCX)}};
	switch (value_node->size) {
	case 1:
		instructions[1] = (struct asm_instruction) {"movb", {R1_(REG_RAX), MEM_(0, REG_RCX)}};
		break;
	case 2:
		instructions[1] = (struct asm_instruction) {"movw", {R2_(REG_RAX), MEM_(0, REG_RCX)}};
		break;
	case 4:
		instructions[1] = (struct asm_instruction) {"movl", {R4_(REG_RAX), MEM_(0, REG_RCX)}};
		break;
	case 8:
		instructions[1] = (struct asm_instruction) {"movq", {R8_(REG_RAX), MEM_(0, REG_RCX)}};
		break;
	default: NOTIMP();
	}
	new_node->assembly.len = 2;

	ir_replace_node(node, new_node);
		
	return 1;
}

static void peephole_codegen_node(struct node *node) {
	int was_replaced =
		peep_load(node)
		|| peep_store(node)
		|| peep_store_label(node)
		|| peep_binary_int_a(node, IR_ADD, "addl", "addq")
		|| peep_binary_int_b(node, IR_ADD, "addl", "addq")

		|| peep_binary_int_b(node, IR_SUB, "subl", "subq")

		|| peep_binary_int_b(node, IR_MUL, "imull", "imulq")
		|| peep_binary_int_a(node, IR_MUL, "imull", "imulq")

		|| peep_binary_int_b(node, IR_IMUL, "imull", "imulq")
		|| peep_binary_int_a(node, IR_IMUL, "imull", "imulq")

		|| peep_binary_int_b(node, IR_BXOR, "xorl", "xorq")
		|| peep_binary_int_a(node, IR_BXOR, "xorl", "xorq")

		|| peep_binary_int_b(node, IR_BOR, "orl", "orq")
		|| peep_binary_int_a(node, IR_BOR, "orl", "orq")

		|| peep_binary_int_b(node, IR_BAND, "andl", "andq")
		|| peep_binary_int_a(node, IR_BAND, "andl", "andq")

		|| peep_binary_comp_b(node, IR_IGREATER, "setg")
		|| peep_binary_comp_b(node, IR_ILESS_EQ, "setle")
		|| peep_binary_comp_b(node, IR_ILESS, "setl")
		|| peep_binary_comp_b(node, IR_IGREATER_EQ, "setge")
		|| peep_binary_comp_b(node, IR_EQUAL, "sete")
		|| peep_binary_comp_b(node, IR_NOT_EQUAL, "setne")
		|| peep_binary_comp_b(node, IR_LESS, "setb")
		|| peep_binary_comp_b(node, IR_GREATER, "seta")
		|| peep_binary_comp_b(node, IR_LESS_EQ, "setbe")
		|| peep_binary_comp_b(node, IR_GREATER_EQ, "setnb")

		|| peep_binary_comp_a(node, IR_IGREATER, "setg")
		|| peep_binary_comp_a(node, IR_ILESS_EQ, "setle")
		|| peep_binary_comp_a(node, IR_ILESS, "setl")
		|| peep_binary_comp_a(node, IR_IGREATER_EQ, "setge")
		|| peep_binary_comp_a(node, IR_EQUAL, "sete")
		|| peep_binary_comp_a(node, IR_NOT_EQUAL, "setne")
		|| peep_binary_comp_a(node, IR_LESS, "setb")
		|| peep_binary_comp_a(node, IR_GREATER, "seta")
		|| peep_binary_comp_a(node, IR_LESS_EQ, "setbe")
		|| peep_binary_comp_a(node, IR_GREATER_EQ, "setnb")
		;

	(void)was_replaced;
	/* switch (node->type) { */
	/* case IR_ADD: break; */
	/* default; */
	/* } */
	/* return; */
	/* struct pattern *matched_pattern = NULL; */
	/* for (unsigned i = 0; i < sizeof patterns / sizeof *patterns; i++) { */
	/* 	if (matches_pattern(node, &patterns[i])) { */
	/* 		matched_pattern = &patterns[i]; */
	/* 		break; */
	/* 	} */
	/* } */

	/* struct node *new_node = NULL; */
	/* new_node = ir_new2(IR_ASSEMBLY, NULL, node->arguments[0], matched_pattern->size); */

	/* struct asm_instruction *instruction = cc_malloc(sizeof *instruction); */
	/* //\*instruction = (struct asm_instruction) {"addq", {IMM_(imm), R8_(REG_RAX)}}; */
	/* new_node->assembly.instructions = instruction; */

	/* assert(node->size == node->arguments[0]->size); */
	/* assert(node->size == node->arguments[1]->size); */

	/* if (node->arguments[0]->type == IR_LABEL) { */
	/* 	struct node *tmp = node->arguments[0]; */
	/* 	node->arguments[0] = node->arguments[1]; */
	/* 	node->arguments[1] = tmp; */
	/* } */

	/* if (node->arguments[0]->type == IR_INTEGER) { */
	/* 	struct node *tmp = node->arguments[0]; */
	/* 	node->arguments[0] = node->arguments[1]; */
	/* 	node->arguments[1] = tmp; */
	/* } */

	/* struct node *new_node = NULL; */

	/* if (node->arguments[1]->type == IR_INTEGER && */
	/* 	node->arguments[1]->integer.integer < UINT32_MAX) { */
	/* 	new_node = ir_new2(IR_ASSEMBLY, NULL, node->arguments[0], node->size); */
	/* 	uint64_t imm = node->arguments[1]->integer.integer; */
	/* 	if (node->size == 4) { */
	/* 		struct asm_instruction *instruction = cc_malloc(sizeof *instruction); */
	/* 		*instruction = (struct asm_instruction) {"addl", {IMM_(imm), R4_(REG_RAX)}}; */
	/* 		new_node->assembly.instructions = instruction; */
	/* 	} else { */
	/* 		struct asm_instruction *instruction = cc_malloc(sizeof *instruction); */
	/* 		*instruction = (struct asm_instruction) {"addq", {IMM_(imm), R8_(REG_RAX)}}; */
	/* 		new_node->assembly.instructions = instruction; */
	/* 	} */

	/* 	new_node->assembly.len = 1; */
	/* } else { */
	/* 	new_node = ir_new3(IR_ASSEMBLY, NULL, node->arguments[0], node->arguments[1], node->size); */

	/* 	if (node->size == 4) { */
	/* 		struct asm_instruction *instruction = cc_malloc(sizeof *instruction); */
	/* 		*instruction = (struct asm_instruction) {"addl", {R4_(REG_RCX), R4_(REG_RAX)}}; */
	/* 		new_node->assembly.instructions = instruction; */
	/* 	} else { */
	/* 		struct asm_instruction *instruction = cc_malloc(sizeof *instruction); */
	/* 		*instruction = (struct asm_instruction) {"addq", {R8_(REG_RCX), R8_(REG_RAX)}}; */
	/* 		new_node->assembly.instructions = instruction; */
	/* 	} */

	/* 	new_node->assembly.len = 1; */
	/* } */

	/* ir_replace_node(node, new_node); */
}

void optimize_peephole_codegen(void) {
	// Add all non-dead nodes to the worklist.
	{
		struct node **nodes;
		size_t size;
		ir_get_node_list(&nodes, &size);
		for (unsigned i = 0; i < size; i++) {
			struct node *node = nodes[i];
			if (node->type != IR_DEAD)
				ADD_ELEMENT(worklist_size, worklist_cap, worklist) = node;
		}
	}

	while (worklist_size) {
		struct node *node = worklist[worklist_size - 1];
		worklist_size--;
		peephole_codegen_node(node);
	}
}
