#ifndef _JLOS_DSA_RBTREE_H
#define _JLOS_DSA_RBTREE_H

#include <common/types.h>

typedef struct jlos_rbtree_node {
    struct jlos_rbtree_node *parent;
    struct jlos_rbtree_node *left;
    struct jlos_rbtree_node *right;
    bool                    red;
} jlos_rbtree_node_t;

typedef int (*jlos_rbtree_compare_fn)(const jlos_rbtree_node_t *a, const jlos_rbtree_node_t *b);

typedef struct jlos_rbtree {
    jlos_rbtree_node_t *root;
    jlos_rbtree_node_t nil;
    jlos_rbtree_compare_fn compare;
} jlos_rbtree_t;

#define jlos_rbtree_entry(node, type, member) \
    container_of(node, type, member)

void jlos_rbtree_init(jlos_rbtree_t *tree, jlos_rbtree_compare_fn compare);
void jlos_rbtree_insert(jlos_rbtree_t *tree, jlos_rbtree_node_t *z);
void jlos_rbtree_remove(jlos_rbtree_t *tree, jlos_rbtree_node_t *z);

jlos_rbtree_node_t *jlos_rbtree_find(const jlos_rbtree_t *tree, const jlos_rbtree_node_t *key);
jlos_rbtree_node_t *jlos_rbtree_find_le(const jlos_rbtree_t *tree, const jlos_rbtree_node_t *key);
jlos_rbtree_node_t *jlos_rbtree_first(const jlos_rbtree_t *tree);
jlos_rbtree_node_t *jlos_rbtree_last(const jlos_rbtree_t *tree);
jlos_rbtree_node_t *jlos_rbtree_next(const jlos_rbtree_t *tree, jlos_rbtree_node_t *x);
jlos_rbtree_node_t *jlos_rbtree_prev(const jlos_rbtree_t *tree, jlos_rbtree_node_t *x);
bool jlos_rbtree_empty(const jlos_rbtree_t *tree);

#endif
