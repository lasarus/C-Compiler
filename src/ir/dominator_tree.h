#ifndef DOMINATOR_TREE
#define DOMINATOR_TREE

// TODO: Rename to cfg.h or something like that.

#include "ir.h"
void ir_post_order_blocks(void);
void ir_calculate_dominator_tree(void);
struct node *intersect(struct node *b1, struct node *b2);

#endif
