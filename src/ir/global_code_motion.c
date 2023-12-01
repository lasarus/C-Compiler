#include "global_code_motion.h"
#include "ir.h"
#include "dominator_tree.h"

#include <common.h>

#include <stdio.h>
#include <assert.h>

// Algorithm taken from "Global Code Motion Global Value Numbering" by Cliff Click.
static void schedule_early(struct node *ins, struct node *root) {
	if (!ins || ins->visited == 10 || ins->block)
		return;

	ins->visited = 10;
	ins->block = root;

	for (int i = 0; i < IR_MAX; i++) {
		struct node *x = ins->arguments[i];

		if (!x)
			continue;

		schedule_early(x, root);

		if (ins->block->block_info.dom_depth < x->block->block_info.dom_depth ||
			x->block->block_info.dom_depth == 0) {
			ins->block = x->block;
		}
	}
}

static void schedule_late(struct node *ins);

static void schedule_user_late(struct node *ins, struct node *y,
							   struct node **lca) {
	schedule_late(y);

	if (!y->block || !y->block->block_info.idom)
		return; // This node is unreachable, thus doesn't matter.

	struct node *use = y->block;

	if (y->type == IR_PHI) {
		struct node *block = y->arguments[0];
		assert(block->type == IR_REGION);

		if (y->arguments[1] == ins)
			*lca = intersect(*lca, block->arguments[0]);

		if (y->arguments[2] == ins)
			*lca = intersect(*lca, block->arguments[1]);
	} else {
		*lca = intersect(*lca, use);
	}
}

static void schedule_late(struct node *ins) {
	if (ins->visited == 11 || ins->block || !node_is_instruction(ins))
		return;

	ins->visited = 11;

	struct node *lca = NULL;

	for (unsigned i = 0; i < ins->use_size; i++) {
		struct node *use = ins->uses[i];

		if (use->type == IR_PROJECT) {
			for (unsigned j = 0; j < use->use_size; j++) {
				schedule_user_late(use, use->uses[j], &lca);
			}
		} else {
			schedule_user_late(ins, use, &lca);
		}
	}

	for (unsigned i = 0; i < ins->use_size; i++) {
		struct node *use = ins->uses[i];
		if (use->type == IR_PROJECT) {
			use->block = lca;
		}
	}

	ins->block = lca;
}

void ir_schedule_instructions_to_blocks(void) {
	struct node **nodes;
	size_t nodes_size;

	(void)schedule_early;
	(void)schedule_late;

	ir_get_node_list(&nodes, &nodes_size);

	// Pin nodes to correct block.
	for (unsigned i = 0; i < nodes_size; i++) {
		struct node *node = nodes[i];

		switch (node->type) {
		case IR_PHI:
			node->block = node->arguments[0];
			break;

		case IR_IF:
			node->block = node->arguments[0];
			break;

		case IR_RETURN:
			node->block = node->arguments[0];
			break;

		default:;
		}
	}

	for (unsigned i = 0; i < nodes_size; i++) {
		struct node *node = nodes[i];

		if (!node_is_instruction(node))
			continue;

		schedule_late(node);
	}
}
