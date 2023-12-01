#include "export_dot.h"
#include "ir.h"

#include <debug.h>

#include <stdio.h>

static FILE *fp = NULL;

static void add_node_definition(struct node *node) {
	node->visited = 2;
	fprintf(fp, "%d [label=\"%s\"];\n", node->index, dbg_instruction(node));

	if (node_is_control(node))
		fprintf(fp, "%d [shape=rectangle];\n", node->index);
	else if (node->type == IR_FUNCTION)
		fprintf(fp, "%d [shape=diamond];\n", node->index);
	else
		fprintf(fp, "%d [shape=none];\n", node->index);
}

static void add_node(struct node *node) {
	for (int i = 0; i < IR_MAX; i++) {
		if (node->arguments[i])
			fprintf(fp, "%d -> %d;\n", node->arguments[i]->index, node->index);
	}
	if (node->next)
		fprintf(fp, "%d -> %d [style=dotted];\n", node->index, node->next->index);
	if (node->child)
		fprintf(fp, "%d -> %d [style=dashed];\n", node->index, node->child->index);
	if (node_is_control(node) && node->block_info.idom)
		fprintf(fp, "%d -> %d [style=dashed, color=blue];\n", node->index, node->block_info.idom->index);
}

static void add_recursive(struct node *start) {
	if (start->visited == 3)
		return;

	if (start->visited != 2) {
		add_node_definition(start);
		add_node(start);
	}
	
	start->visited = 3;

	if (start->child)
		add_recursive(start->child);

	for (int i = 0; i < IR_MAX; i++)
		if (start->arguments[i])
			add_recursive(start->arguments[i]);

	for (unsigned i = 0; i < start->use_size; i++)
		add_recursive(start->uses[i]);

	if (start->next)
		add_recursive(start->next);
}

void export_dot(const char *path) {
	fp = fopen(path, "w");
	fprintf(fp, "digraph graphname {\n");

	int count = 0;
	for (struct node *f = first_function; f; f = f->next) {
		add_node_definition(f);
		for (struct node *b = f->child; b; b = b->next) {
			fprintf(fp, "subgraph cluster_%d {\n", count++);
			add_node_definition(b);
			for (struct node *ins = b->child; ins; ins = ins->next) {
				add_node_definition(ins);
			}
			/* for (unsigned i = 0; i < b->block_info.children_size; i++) { */
			/* 	add_node_definition(b->block_info.children[i]); */
			/* } */
			fprintf(fp, "}\n");
		}
	}

	struct node **nodes;
	size_t size;
	ir_get_node_list(&nodes, &size);

	/* for (unsigned i = 0; i < size; i++) { */
	/* 	struct node *node = nodes[i]; */
	/* 	add_node_definition(node); */
	/* } */

	for (struct node *f = first_function; f; f = f->next) {
		add_node(f);
		for (struct node *b = f->child; b; b = b->next) {
			add_node(b);
			for (struct node *ins = b->child; ins; ins = ins->next) {
				add_node(ins);
			}
			/* for (unsigned i = 0; i < b->block_info.children_size; i++) { */
			/* 	add_node(b->block_info.children[i]); */
			/* } */
		}
	}

	for (unsigned i = 0; i < size; i++) {
		/* struct node *node = nodes[i]; */
		/* add_node(node); */
	}

	add_recursive(first_function);

	fprintf(fp, "}\n");
	fclose(fp);
}
