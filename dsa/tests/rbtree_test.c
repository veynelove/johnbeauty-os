/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/test.h>
#include <dsa/rbtree.h>
#include <kernel/memory_manager.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_rbtree"
#include <kernel/printk.h>

#define RBTREE_TEST_COUNT 128

typedef struct {
    uint32_t key;
    jlos_rbtree_node_t node;
} test_entry_t;

static int test_compare(const jlos_rbtree_node_t *a, const jlos_rbtree_node_t *b)
{
    test_entry_t *ea = jlos_rbtree_entry(a, test_entry_t, node);
    test_entry_t *eb = jlos_rbtree_entry(b, test_entry_t, node);
    if (ea->key < eb->key) {
        return -1;
    }
    if (ea->key > eb->key) {
        return 1;
    }
    return 0;
}

static bool rbtree_verify_properties(const jlos_rbtree_t *tree)
{
    if (tree->root == &tree->nil) {
        return true;
    }
    if (tree->root->red) {
        return false;
    }
    return true;
}

JLOS_TEST(rbtree, insert_and_find)
{
    jlos_rbtree_t tree;
    jlos_rbtree_init(&tree);
    test_entry_t entries[16];
    uint32_t keys[16] = {8,3,10,1,6,14,4,7,13,2,0,11,5,12,9,15};
    for (int i = 0; i < 16; i++) {
        entries[i].key = keys[i];
        jlos_rbtree_insert(&tree, &entries[i].node, test_compare);
    }
    JLOS_TEST_TRUE(rbtree_verify_properties(&tree));
    for (int i = 0; i < 16; i++) {
        test_entry_t key_entry;
        key_entry.key = keys[i];
        jlos_rbtree_node_t *found = jlos_rbtree_find(&tree, &key_entry.node, test_compare);
        JLOS_TEST_NOT_NULL(found);
        if (found) {
            test_entry_t *e = jlos_rbtree_entry(found, test_entry_t, node);
            JLOS_TEST_EQ(e->key, keys[i]);
        }
    }
    test_entry_t missing;
    missing.key = 100;
    JLOS_TEST_NULL(jlos_rbtree_find(&tree, &missing.node, test_compare));
}

JLOS_TEST(rbtree, ordered_traversal)
{
    jlos_rbtree_t tree;
    jlos_rbtree_init(&tree);
    test_entry_t entries[32];
    for (int i = 0; i < 32; i++) {
        entries[i].key = (uint32_t)i;
        jlos_rbtree_insert(&tree, &entries[i].node, test_compare);
    }
    uint32_t expected = 0;
    jlos_rbtree_node_t *node;
    for (node = jlos_rbtree_first(&tree); node != NULL; node = jlos_rbtree_next(&tree, node)) {
        test_entry_t *e = jlos_rbtree_entry(node, test_entry_t, node);
        JLOS_TEST_EQ(e->key, expected);
        expected++;
    }
    JLOS_TEST_EQ(expected, 32);
    expected = 31;
    for (node = jlos_rbtree_last(&tree); node != NULL; node = jlos_rbtree_prev(&tree, node)) {
        test_entry_t *e = jlos_rbtree_entry(node, test_entry_t, node);
        JLOS_TEST_EQ(e->key, expected);
        expected--;
    }
}

JLOS_TEST(rbtree, remove)
{
    jlos_rbtree_t tree;
    jlos_rbtree_init(&tree);
    test_entry_t entries[64];
    for (int i = 0; i < 64; i++) {
        entries[i].key = (uint32_t)i;
        jlos_rbtree_insert(&tree, &entries[i].node, test_compare);
    }
    for (int i = 0; i < 64; i += 2) {
        jlos_rbtree_remove(&tree, &entries[i].node);
    }
    JLOS_TEST_TRUE(rbtree_verify_properties(&tree));
    for (int i = 1; i < 64; i += 2) {
        test_entry_t key_entry;
        key_entry.key = (uint32_t)i;
        JLOS_TEST_NOT_NULL(jlos_rbtree_find(&tree, &key_entry.node, test_compare));
    }
    for (int i = 0; i < 64; i += 2) {
        test_entry_t key_entry;
        key_entry.key = (uint32_t)i;
        JLOS_TEST_NULL(jlos_rbtree_find(&tree, &key_entry.node, test_compare));
    }
}

JLOS_TEST(rbtree, find_le)
{
    jlos_rbtree_t tree;
    jlos_rbtree_init(&tree);
    test_entry_t entries[8];
    uint32_t keys[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    for (int i = 0; i < 8; i++) {
        entries[i].key = keys[i];
        jlos_rbtree_insert(&tree, &entries[i].node, test_compare);
    }
    test_entry_t query;
    query.key = 35;
    jlos_rbtree_node_t *found = jlos_rbtree_find_le(&tree, &query.node, test_compare);
    JLOS_TEST_NOT_NULL(found);
    if (found) {
        test_entry_t *e = jlos_rbtree_entry(found, test_entry_t, node);
        JLOS_TEST_EQ(e->key, 30);
    }
    query.key = 10;
    found = jlos_rbtree_find_le(&tree, &query.node, test_compare);
    JLOS_TEST_NOT_NULL(found);
    if (found) {
        test_entry_t *e = jlos_rbtree_entry(found, test_entry_t, node);
        JLOS_TEST_EQ(e->key, 10);
    }
    query.key = 5;
    JLOS_TEST_NULL(jlos_rbtree_find_le(&tree, &query.node, test_compare));
    query.key = 100;
    found = jlos_rbtree_find_le(&tree, &query.node, test_compare);
    JLOS_TEST_NOT_NULL(found);
    if (found) {
        test_entry_t *e = jlos_rbtree_entry(found, test_entry_t, node);
        JLOS_TEST_EQ(e->key, 80);
    }
}

JLOS_TEST(rbtree, random_insert_remove)
{
    jlos_rbtree_t tree;
    jlos_rbtree_init(&tree);
    test_entry_t *entries = (test_entry_t *)jlos_kalloc(sizeof(test_entry_t) * RBTREE_TEST_COUNT);
    JLOS_ASSERT_NOT_NULL(entries);
    uint32_t seed = 12345;
    for (int i = 0; i < RBTREE_TEST_COUNT; i++) {
        seed = seed * 1103515245u + 12345u;
        entries[i].key = seed % 10000;
    }
    for (int i = 0; i < RBTREE_TEST_COUNT; i++) {
        jlos_rbtree_insert(&tree, &entries[i].node, test_compare);
    }
    JLOS_TEST_TRUE(rbtree_verify_properties(&tree));
    uint32_t count = 0;
    jlos_rbtree_node_t *node;
    for (node = jlos_rbtree_first(&tree); node != NULL; node = jlos_rbtree_next(&tree, node)) {
        count++;
    }
    JLOS_TEST_EQ(count, RBTREE_TEST_COUNT);
    uint32_t removed = 0;
    for (int i = 0; i < RBTREE_TEST_COUNT; i += 3) {
        jlos_rbtree_remove(&tree, &entries[i].node);
        removed++;
    }
    JLOS_TEST_TRUE(rbtree_verify_properties(&tree));
    count = 0;
    for (node = jlos_rbtree_first(&tree); node != NULL; node = jlos_rbtree_next(&tree, node)) {
        count++;
    }
    JLOS_TEST_EQ(count, RBTREE_TEST_COUNT - removed);
    jlos_kfree(entries);
}
