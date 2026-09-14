#include <dsa/rbtree.h>

static void rbtree_left_rotate(jlos_rbtree_t *tree, jlos_rbtree_node_t *x)
{
    jlos_rbtree_node_t *y = x->right;
    x->right = y->left;
    if (y->left != &tree->nil) {
        y->left->parent = x;
    }
    y->parent = x->parent;
    if (x->parent == &tree->nil) {
        tree->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;
}

static void rbtree_right_rotate(jlos_rbtree_t *tree, jlos_rbtree_node_t *x)
{
    jlos_rbtree_node_t *y = x->left;
    x->left = y->right;
    if (y->right != &tree->nil) {
        y->right->parent = x;
    }
    y->parent = x->parent;
    if (x->parent == &tree->nil) {
        tree->root = y;
    } else if (x == x->parent->right) {
        x->parent->right = y;
    } else {
        x->parent->left = y;
    }
    y->right = x;
    x->parent = y;
}

static void rbtree_insert_fixup(jlos_rbtree_t *tree, jlos_rbtree_node_t *z)
{
    while (z->parent->red) {
        if (z->parent == z->parent->parent->left) {
            jlos_rbtree_node_t *y = z->parent->parent->right;
            if (y->red) {
                z->parent->red = false;
                y->red = false;
                z->parent->parent->red = true;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    rbtree_left_rotate(tree, z);
                }
                z->parent->red = false;
                z->parent->parent->red = true;
                rbtree_right_rotate(tree, z->parent->parent);
            }
        } else {
            jlos_rbtree_node_t *y = z->parent->parent->left;
            if (y->red) {
                z->parent->red = false;
                y->red = false;
                z->parent->parent->red = true;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rbtree_right_rotate(tree, z);
                }
                z->parent->red = false;
                z->parent->parent->red = true;
                rbtree_left_rotate(tree, z->parent->parent);
            }
        }
    }
    tree->root->red = false;
}

static void rbtree_transplant(jlos_rbtree_t *tree, jlos_rbtree_node_t *u, jlos_rbtree_node_t *v)
{
    if (u->parent == &tree->nil) {
        tree->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

static void rbtree_delete_fixup(jlos_rbtree_t *tree, jlos_rbtree_node_t *x)
{
    while (x != tree->root && !x->red) {
        if (x == x->parent->left) {
            jlos_rbtree_node_t *w = x->parent->right;
            if (w->red) {
                w->red = false;
                x->parent->red = true;
                rbtree_left_rotate(tree, x->parent);
                w = x->parent->right;
            }
            if (!w->left->red && !w->right->red) {
                w->red = true;
                x = x->parent;
            } else {
                if (!w->right->red) {
                    w->left->red = false;
                    w->red = true;
                    rbtree_right_rotate(tree, w);
                    w = x->parent->right;
                }
                w->red = x->parent->red;
                x->parent->red = false;
                w->right->red = false;
                rbtree_left_rotate(tree, x->parent);
                x = tree->root;
            }
        } else {
            jlos_rbtree_node_t *w = x->parent->left;
            if (w->red) {
                w->red = false;
                x->parent->red = true;
                rbtree_right_rotate(tree, x->parent);
                w = x->parent->left;
            }
            if (!w->right->red && !w->left->red) {
                w->red = true;
                x = x->parent;
            } else {
                if (!w->left->red) {
                    w->right->red = false;
                    w->red = true;
                    rbtree_left_rotate(tree, w);
                    w = x->parent->left;
                }
                w->red = x->parent->red;
                x->parent->red = false;
                w->left->red = false;
                rbtree_right_rotate(tree, x->parent);
                x = tree->root;
            }
        }
    }
    x->red = false;
}

static jlos_rbtree_node_t *rbtree_minimum(const jlos_rbtree_t *tree, jlos_rbtree_node_t *x)
{
    while (x->left != &tree->nil) {
        x = x->left;
    }
    return x;
}

static jlos_rbtree_node_t *rbtree_maximum(const jlos_rbtree_t *tree, jlos_rbtree_node_t *x)
{
    while (x->right != &tree->nil) {
        x = x->right;
    }
    return x;
}

void jlos_rbtree_init(jlos_rbtree_t *tree, jlos_rbtree_compare_fn compare)
{
    tree->nil.parent = &tree->nil;
    tree->nil.left = &tree->nil;
    tree->nil.right = &tree->nil;
    tree->nil.red = false;
    tree->root = &tree->nil;
    tree->compare = compare;
}

void jlos_rbtree_insert(jlos_rbtree_t *tree, jlos_rbtree_node_t *z)
{
    jlos_rbtree_node_t *y = &tree->nil;
    jlos_rbtree_node_t *x = tree->root;
    while (x != &tree->nil) {
        y = x;
        if (tree->compare(z, x) < 0) {
            x = x->left;
        } else {
            x = x->right;
        }
    }
    z->parent = y;
    if (y == &tree->nil) {
        tree->root = z;
    } else if (tree->compare(z, y) < 0) {
        y->left = z;
    } else {
        y->right = z;
    }
    z->left = &tree->nil;
    z->right = &tree->nil;
    z->red = true;
    rbtree_insert_fixup(tree, z);
}

void jlos_rbtree_remove(jlos_rbtree_t *tree, jlos_rbtree_node_t *z)
{
    jlos_rbtree_node_t *y = z;
    jlos_rbtree_node_t *x;
    bool y_original_red = y->red;
    if (z->left == &tree->nil) {
        x = z->right;
        rbtree_transplant(tree, z, z->right);
    } else if (z->right == &tree->nil) {
        x = z->left;
        rbtree_transplant(tree, z, z->left);
    } else {
        y = rbtree_minimum(tree, z->right);
        y_original_red = y->red;
        x = y->right;
        if (y->parent == z) {
            x->parent = y;
        } else {
            rbtree_transplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        rbtree_transplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->red = z->red;
    }
    if (!y_original_red) {
        rbtree_delete_fixup(tree, x);
    }
}

jlos_rbtree_node_t *jlos_rbtree_find(const jlos_rbtree_t *tree, const jlos_rbtree_node_t *key)
{
    jlos_rbtree_node_t *x = tree->root;
    while (x != &tree->nil) {
        int cmp = tree->compare(key, x);
        if (cmp < 0) {
            x = x->left;
        } else if (cmp > 0) {
            x = x->right;
        } else {
            return x;
        }
    }
    return NULL;
}

jlos_rbtree_node_t *jlos_rbtree_find_le(const jlos_rbtree_t *tree, const jlos_rbtree_node_t *key)
{
    jlos_rbtree_node_t *best = NULL;
    jlos_rbtree_node_t *x = tree->root;
    while (x != &tree->nil) {
        if (tree->compare(x, key) <= 0) {
            best = x;
            x = x->right;
        } else {
            x = x->left;
        }
    }
    return best;
}

jlos_rbtree_node_t *jlos_rbtree_first(const jlos_rbtree_t *tree)
{
    if (tree->root == &tree->nil) {
        return NULL;
    }
    return rbtree_minimum(tree, tree->root);
}

jlos_rbtree_node_t *jlos_rbtree_last(const jlos_rbtree_t *tree)
{
    if (tree->root == &tree->nil) {
        return NULL;
    }
    return rbtree_maximum(tree, tree->root);
}

jlos_rbtree_node_t *jlos_rbtree_next(const jlos_rbtree_t *tree, jlos_rbtree_node_t *x)
{
    if (x->right != &tree->nil) {
        return rbtree_minimum(tree, x->right);
    }
    jlos_rbtree_node_t *y = x->parent;
    while (y != &tree->nil && x == y->right) {
        x = y;
        y = y->parent;
    }
    if (y == &tree->nil) {
        return NULL;
    }
    return y;
}

jlos_rbtree_node_t *jlos_rbtree_prev(const jlos_rbtree_t *tree, jlos_rbtree_node_t *x)
{
    if (x->left != &tree->nil) {
        return rbtree_maximum(tree, x->left);
    }
    jlos_rbtree_node_t *y = x->parent;
    while (y != &tree->nil && x == y->left) {
        x = y;
        y = y->parent;
    }
    if (y == &tree->nil) {
        return NULL;
    }
    return y;
}

bool jlos_rbtree_empty(const jlos_rbtree_t *tree)
{
    return tree->root == &tree->nil;
}
