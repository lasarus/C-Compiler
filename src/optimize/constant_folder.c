#include "constant_folder.h"

#include <ir/ir.h>
#include <common.h>

static struct node **to_be_replaced = NULL;
static size_t to_be_replaced_size = 0, to_be_replaced_cap = 0;

static int can_be_constant(struct node *node) {
	if (node->visited == 0)
		return 0;
	if (node->visited == 1)
		return 1;

	node->visited = 0;

	int can_this_node_be_constant = 0;

	switch (node->type) {
	case IR_INTEGER:
		can_this_node_be_constant = 1;
		break;

	case IR_INT_CAST_SIGN:
	case IR_INT_CAST_ZERO:
		can_this_node_be_constant = can_be_constant(node->arguments[0]);
		break;

	case IR_MUL:
	case IR_ADD:
	case IR_SUB:
		can_this_node_be_constant = can_be_constant(node->arguments[0]) &&
			can_be_constant(node->arguments[1]);
		break;

	default:;
	}

	if (can_this_node_be_constant) {
		node->visited = 1;
		return 1;
	} else {
		node->visited = 0;
		return 0;
	}
}

// TODO: Move these into arch/.
static uint64_t do_operation(struct node *node) {
	switch (node->type) {
	case IR_INTEGER:
		return node->integer.integer;
	case IR_INT_CAST_ZERO:
		return do_operation(node->arguments[0]);
	case IR_INT_CAST_SIGN:
		return do_operation(node->arguments[0]);
	case IR_ADD:
		return do_operation(node->arguments[0]) + do_operation(node->arguments[1]);
	case IR_SUB:
		return do_operation(node->arguments[0]) - do_operation(node->arguments[1]);
	case IR_MUL:
		return do_operation(node->arguments[0]) * do_operation(node->arguments[1]);
	default: NOTIMP();
	}
}

void optimize_constant_folder(void) {
	struct node **nodes;
	size_t size;
	ir_get_node_list(&nodes, &size);
	for (unsigned i = 0; i < size; i++) {
		struct node *node = nodes[i];

		if (can_be_constant(node) && node->type != IR_INTEGER) {
			ADD_ELEMENT(to_be_replaced_size, to_be_replaced_cap, to_be_replaced) = node;
		}
	}

	for (unsigned i = 0; i < to_be_replaced_size; i++) {
		struct node *node = to_be_replaced[i];
		uint64_t value = do_operation(node);
		struct node *new_node = ir_integer(node->size, value);
		ir_replace_node(node, new_node);
	}

	free(to_be_replaced);
	to_be_replaced = NULL;
	to_be_replaced_cap = to_be_replaced_size = 0;
}
