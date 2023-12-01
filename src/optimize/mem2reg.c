#include "mem2reg.h"
#include "debug.h"
#include <ir/ir.h>

#include <common.h>

#include <stdlib.h>

static struct node *recursive_find(struct node *state, struct node *alloc, int visit_idx) {
	if (state->visited == visit_idx && state->scratch)
		return state->scratch;

	struct node *ret_value = NULL;
	state->visited = visit_idx;
	state->scratch = NULL;

	switch (state->type) {
	case IR_PROJECT:
		ret_value = recursive_find(state->arguments[0], alloc, visit_idx);
		break;

	case IR_FUNCTION:
		ret_value = ir_new(IR_UNDEFINED, alloc->alloc.size);
		break;

	case IR_SET_ZERO_PTR:
		if (state->arguments[0] == alloc)
			ret_value = ir_zero(alloc->alloc.size);
		else
			ret_value = recursive_find(state->arguments[1], alloc, visit_idx);
		break;

	case IR_STORE:
		if (state->arguments[0] == alloc)
			ret_value = state->arguments[1];
		else
			ret_value = recursive_find(state->arguments[2], alloc, visit_idx);
		break;

	case IR_PHI:
		ret_value = state->scratch = ir_new1(IR_PHI, state->arguments[0], alloc->alloc.size);
		if (state->arguments[1])
			node_set_argument(ret_value, 1, recursive_find(state->arguments[1], alloc, visit_idx));
		if (state->arguments[2])
			node_set_argument(ret_value, 2, recursive_find(state->arguments[2], alloc, visit_idx));
		break;

	default: {
		struct node *prev_state = node_get_prev_state(state);
		if (!prev_state) {
			printf("%d %d\n", state->index, state->type); NOTIMP();
			NOTIMP();
		}
		ret_value = recursive_find(prev_state, alloc, visit_idx);
		break;
	}
	}

	state->scratch = ret_value;

	return ret_value;
}

static void try_remove_alloc(struct node *alloc, int visit_idx) {
	// This doesn't actually delete anything.

	(void)alloc;

	int size = alloc->alloc.size;
	set_current_function(alloc->parent_function);

	for (unsigned i = 0; i < alloc->use_size; i++) {
		struct node *use = alloc->uses[i];

		switch (use->type) {
		case IR_SET_ZERO_PTR:
			if (use->set_zero_ptr.size != size)
				return;
			break;

		case IR_STORE:
			if (use->arguments[1]->size != size ||
				use->arguments[1] == alloc ||
				use->arguments[0] != alloc) // Storing the address is not allowed.
				return;
			break;

		case IR_LOAD:
			if (use->size != size)
				return;
			break;

		default: return; // Not possible to turn into register.
		}
	}

	for (unsigned i = 0; i < alloc->use_size; i++) {
		struct node *use = alloc->uses[i];

		switch (use->type) {
		case IR_LOAD: {
			struct node *replacement = recursive_find(use->arguments[1], alloc, visit_idx);
			ir_replace_node(use, replacement);
		} break;
		default:;
		}
	}

	// Reroute the state variable around the no longer necessary nodes.
	for (unsigned i = 0; i < alloc->use_size; i++) {
		struct node *use = alloc->uses[i];
		struct node *prev_state = node_get_prev_state(use);

		switch (use->type) {
		case IR_STORE:
		case IR_SET_ZERO_PTR:
			ir_replace_node(use, prev_state);
			break;
		case IR_LOAD:
			break;
		default: NOTIMP();
		}
	}
}

void optimize_mem2reg(void) {
	struct node **nodes;
	size_t size;
	ir_get_node_list(&nodes, &size);

	struct node **allocs = NULL;
	size_t alloc_size = 0, alloc_cap = 0;

	for (unsigned i = 0; i < size; i++) {
		struct node *node = nodes[i];

		if (node->type == IR_ALLOC)
			ADD_ELEMENT(alloc_size, alloc_cap, allocs) = node;
	}

	for (unsigned i = 0; i < alloc_size; i++) {
		try_remove_alloc(allocs[i], 30 + i);
	}

	free(allocs);
}
