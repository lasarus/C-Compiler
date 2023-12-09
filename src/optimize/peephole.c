#include "peephole.h"
#include "common.h"

#include <ir/ir.h>

#include <assert.h>

static struct node **worklist = NULL;
static size_t worklist_size = 0, worklist_cap = 0;

void add_neighbors_to_worklist(struct node *node) {
	for (unsigned i = 0; i < node->use_size; i++) {
	}
}

static void peephole_phi(struct node *phi) {
	if (phi->arguments[1] == NULL && phi->arguments[2] == NULL) {
		// Dead phi.
		//ir_replace_node(phi, phi->arguments[1]);
	} else if (phi->arguments[1] == NULL) {
		ir_replace_node(phi, phi->arguments[2]);
	} else if (phi->arguments[2] == NULL) {
		ir_replace_node(phi, phi->arguments[1]);
	} else if (phi->arguments[1] == phi->arguments[2]) {
		// Redundant phi-node.
		ir_replace_node(phi, phi->arguments[1]);
	}
}

static void peephole_mul(struct node *node) {
	if (node->arguments[1]->type == IR_INTEGER &&
		node->arguments[1]->integer.integer == 1) {
		ir_replace_node(node, node->arguments[0]);
	} else if (node->arguments[0]->type == IR_INTEGER &&
		node->arguments[0]->integer.integer == 1) {
		ir_replace_node(node, node->arguments[1]);
	}
}

void optimize_peephole(void) {
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

		switch (node->type) {
		case IR_PHI: peephole_phi(node); break;
		case IR_MUL: peephole_mul(node); break;
		default:;
		}
	}
}
