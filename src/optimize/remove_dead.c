#include "remove_dead.h"

#include <ir/ir.h>

#include <common.h>

void recurse_mark_reachable(struct node *control) {
	if (control->visited == -1)
		return;

	control->visited = -1;

	for (unsigned i = 0; i < control->use_size; i++) {
		struct node *use = control->uses[i];

		if (use->type == IR_RETURN) {
			recurse_mark_reachable(use);
		} else if (node_is_control(use)) {
			recurse_mark_reachable(use);
		} else if (use->type == IR_IF) {
			recurse_mark_reachable(use->projects[0]);
			recurse_mark_reachable(use->projects[1]);
		}
	}
}

void recurse_mark_useful(struct node *node) {
	if (node->visited == -2)
		return;

	node->visited = -2;

	for (int i = 0; i < IR_MAX; i++) {
		if (node->arguments[i])
			recurse_mark_useful(node->arguments[i]);
	}
}

void prune_dead_nodes(void) {
	struct node **nodes;
	size_t size;
	ir_get_node_list(&nodes, &size);
}

void optimize_remove_dead(void) {
	struct node **nodes;
	size_t size;
	ir_get_node_list(&nodes, &size);

	// Remove unused.
	for (unsigned i = 0; i < size; i++) {
		struct node *node = nodes[i];

		if (node->type == IR_FUNCTION)
			recurse_mark_reachable(node->projects[0]);
	}

	for (unsigned i = 0; i < size; i++) {
		struct node *node = nodes[i];

		if (node->visited != -1)
			continue;

		if (node->type == IR_RETURN)
			recurse_mark_useful(node);
		else if (node->type == IR_REGION)
			recurse_mark_useful(node);
	}

	for (unsigned i = 0; i < size; i++) {
		struct node *node = nodes[i];

		if (node->visited != -2 && node->type != IR_PROJECT) {
			for (int i = 0; i < IR_MAX; i++) {
				if (node->arguments[i])
					node_set_argument(node, i, NULL);
			}
			node->type = IR_DEAD;
		}
	}
}
