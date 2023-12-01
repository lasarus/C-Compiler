#include "dominator_tree.h"
#include "ir/ir.h"

#include <stdio.h>

static void post_order_recurse(struct node *start,
							   struct node **list_head,
							   int *idx) {
	if (!start || start->visited == 7)
		return;
	start->visited = 7;

	for (unsigned i = 0; i < start->use_size; i++) {
		struct node *use = start->uses[i];
		if (node_is_control(use)) {
			post_order_recurse(use, list_head, idx);
		} else if (use->type == IR_IF) {
			post_order_recurse(use->projects[0], list_head, idx);
			post_order_recurse(use->projects[1], list_head, idx);
		}
	}

	start->next = *list_head;
	*list_head = start;
	start->block_info.post_idx = (*idx)++;
}

void ir_post_order_blocks(void) {
	for (struct node *f = first_function; f; f = f->next) {
		struct node *list_head = NULL;
		int idx = 0;

		post_order_recurse(f->projects[0], &list_head, &idx);

		f->child = list_head;
	}
}

// TODO: make an explanation of what this function
// actually does. It is a bit abstract right now,
// and the name isn't really helping.
struct node *intersect(struct node *b1, struct node *b2) {
	if (!b1) return b2;
	if (!b2) return b1;
	while (b1 != b2) {
		while (b1->block_info.post_idx < b2->block_info.post_idx)
			b1 = b1->block_info.idom;
		while (b2->block_info.post_idx < b1->block_info.post_idx)
			b2 = b2->block_info.idom;
	}

	return b1;
}

static void ir_calculate_dominator_tree_function(struct node *entry) {
	if (!entry)
		return;
		
	entry->block_info.idom = entry;

	int changed = 1;
	while (changed) {
		changed = 0;

		for (struct node *b = entry->next; b; b = b->next) {
			struct node *ni = NULL;

			if (b->type == IR_PROJECT &&
				b->arguments[0]->type == IR_IF) {
				if (b->arguments[0]->arguments[0]->block_info.idom)
					ni = b->arguments[0]->arguments[0];
				// The idom for proj is trivially the parent region.
			} else if (b->type == IR_REGION) {
				for (int k = 0; k < IR_MAX; k++) {
					if (b->arguments[k] && b->arguments[k]->block_info.idom)
						ni = intersect(ni, b->arguments[k]);
				}
			}

			if (b->block_info.idom != ni) {
				b->block_info.idom = ni;
				changed = 1;
			}
		}
	}

	entry->block_info.dom_depth = 1;

	for (struct node *b = entry->next; b; b = b->next) {
		b->block_info.dom_depth = b->block_info.idom->block_info.dom_depth + 1;
	}
}

void ir_calculate_dominator_tree(void) {
	for (struct node *f = first_function; f; f = f->next) {
		ir_calculate_dominator_tree_function(f->child);
	}
}
