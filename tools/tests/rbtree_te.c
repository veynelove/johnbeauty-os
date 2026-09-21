#include <tools/tests/rbtree_te.h>
#include <dsa/rbtree.h>
#include <kernel/memory_manager.h>

#define JLOS_KERNEL_LOG_SUBSYS "test"
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
    if (ea->key < eb->key)
        return -1;
    if (ea->key > eb->key)
        return 1;
    return 0;
}

static bool rbtree_verify_properties(const jlos_rbtree_t *tree)
{
    if (tree->root == &tree->nil)
        return true;
    if (tree->root->red) {
        printk_err("FAIL: root is red\n");
        return false;
    }
    return true;
}

static int test_insert_and_find(void)
{
    int fail = 0;
    printk_info("[test 1] insert and find\n");
    jlos_rbtree_t tree;
    jlos_rbtree_init(&tree);
    test_entry_t entries[16];
    uint32_t keys[16] = {8,3,10,1,6,14,4,7,13,2,0,11,5,12,9,15};
    for (int i = 0; i < 16; i++) {
        entries[i].key = keys[i];
        jlos_rbtree_insert(&tree, &entries[i].node, test_compare);
    }
    if (!rbtree_verify_properties(&tree)) {
        fail++;
    }
    for (int i = 0; i < 16; i++) {
        test_entry_t key_entry;
        key_entry.key = keys[i];
        jlos_rbtree_node_t *found = jlos_rbtree_find(&tree, &key_entry.node, test_compare);
        if (!found) {
            printk_err("FAIL: find key=%u returned NULL\n", keys[i]);
            fail++;
        } else {
            test_entry_t *e = jlos_rbtree_entry(found, test_entry_t, node);
            if (e->key != keys[i]) {
                printk_err("FAIL: find key=%u found key=%u\n", keys[i], e->key);
                fail++;
            }
        }
    }
    test_entry_t missing;
    missing.key = 100;
    if (jlos_rbtree_find(&tree, &missing.node, test_compare) != NULL) {
        printk_err("FAIL: find missing key returned non-NULL\n");
        fail++;
    }
    if (!fail)
        printk_info("OK: insert and find (16 entries)\n");
    return fail;
}

static int test_order_traversal(void)
{
    int fail = 0;
    printk_info("[test 2] ordered traversal\n");
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
        if (e->key != expected) {
            printk_err("FAIL: expected %u got %u\n", expected, e->key);
            fail++;
        }
        expected++;
    }
    if (expected != 32) {
        printk_err("FAIL: traversed %u expected 32\n", expected);
        fail++;
    }
    expected = 31;
    for (node = jlos_rbtree_last(&tree); node != NULL; node = jlos_rbtree_prev(&tree, node)) {
        test_entry_t *e = jlos_rbtree_entry(node, test_entry_t, node);
        if (e->key != expected) {
            printk_err("FAIL: reverse expected %u got %u\n", expected, e->key);
            fail++;
        }
        expected--;
    }
    if (!fail)
        printk_info("OK: ordered traversal (32 entries)\n");
    return fail;
}

static int test_remove(void)
{
    int fail = 0;
    printk_info("[test 3] remove\n");
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
    if (!rbtree_verify_properties(&tree)) {
        fail++;
    }
    for (int i = 1; i < 64; i += 2) {
        test_entry_t key_entry;
        key_entry.key = (uint32_t)i;
        jlos_rbtree_node_t *found = jlos_rbtree_find(&tree, &key_entry.node, test_compare);
        if (!found) {
            printk_err("FAIL: odd key=%u not found after removing evens\n", i);
            fail++;
        }
    }
    for (int i = 0; i < 64; i += 2) {
        test_entry_t key_entry;
        key_entry.key = (uint32_t)i;
        jlos_rbtree_node_t *found = jlos_rbtree_find(&tree, &key_entry.node, test_compare);
        if (found) {
            printk_err("FAIL: even key=%u still found after remove\n", i);
            fail++;
        }
    }
    if (!fail)
        printk_info("OK: remove (64 entries, removed evens)\n");
    return fail;
}

static int test_find_le(void)
{
    int fail = 0;
    printk_info("[test 4] find_le\n");
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
    if (!found) {
        printk_err("FAIL: find_le(35) returned NULL\n");
        fail++;
    } else {
        test_entry_t *e = jlos_rbtree_entry(found, test_entry_t, node);
        if (e->key != 30) {
            printk_err("FAIL: find_le(35) expected 30 got %u\n", e->key);
            fail++;
        }
    }
    query.key = 10;
    found = jlos_rbtree_find_le(&tree, &query.node, test_compare);
    if (!found) {
        printk_err("FAIL: find_le(10) returned NULL\n");
        fail++;
    } else {
        test_entry_t *e = jlos_rbtree_entry(found, test_entry_t, node);
        if (e->key != 10) {
            printk_err("FAIL: find_le(10) expected 10 got %u\n", e->key);
            fail++;
        }
    }
    query.key = 5;
    found = jlos_rbtree_find_le(&tree, &query.node, test_compare);
    if (found) {
        printk_err("FAIL: find_le(5) expected NULL\n");
        fail++;
    }
    query.key = 100;
    found = jlos_rbtree_find_le(&tree, &query.node, test_compare);
    if (!found) {
        printk_err("FAIL: find_le(100) returned NULL\n");
        fail++;
    } else {
        test_entry_t *e = jlos_rbtree_entry(found, test_entry_t, node);
        if (e->key != 80) {
            printk_err("FAIL: find_le(100) expected 80 got %u\n", e->key);
            fail++;
        }
    }
    if (!fail)
        printk_info("OK: find_le (4 queries)\n");
    return fail;
}

static int test_random_insert_remove(void)
{
    int fail = 0;
    printk_info("[test 5] random insert/remove %u entries\n", RBTREE_TEST_COUNT);
    jlos_rbtree_t tree;
    jlos_rbtree_init(&tree);
    test_entry_t *entries = (test_entry_t *)jlos_kalloc(sizeof(test_entry_t) * RBTREE_TEST_COUNT);
    if (!entries) {
        printk_err("FAIL: kalloc for test entries\n");
        return 1;
    }
    uint32_t seed = 12345;
    for (int i = 0; i < RBTREE_TEST_COUNT; i++) {
        seed = seed * 1103515245u + 12345u;
        entries[i].key = seed % 10000;
    }
    for (int i = 0; i < RBTREE_TEST_COUNT; i++)
        jlos_rbtree_insert(&tree, &entries[i].node, test_compare);
    if (!rbtree_verify_properties(&tree)) {
        fail++;
    }
    int count_after_insert = 0;
    jlos_rbtree_node_t *node;
    for (node = jlos_rbtree_first(&tree); node != NULL; node = jlos_rbtree_next(&tree, node))
        count_after_insert++;
    if (count_after_insert != RBTREE_TEST_COUNT) {
        printk_err("FAIL: count after insert %d expected %d\n", count_after_insert, RBTREE_TEST_COUNT);
        fail++;
    }
    int removed = 0;
    for (int i = 0; i < RBTREE_TEST_COUNT; i += 3) {
        jlos_rbtree_remove(&tree, &entries[i].node);
        removed++;
    }
    if (!rbtree_verify_properties(&tree)) {
        fail++;
    }
    int count_after_remove = 0;
    for (node = jlos_rbtree_first(&tree); node != NULL; node = jlos_rbtree_next(&tree, node))
        count_after_remove++;
    int expected_remaining = RBTREE_TEST_COUNT - removed;
    if (count_after_remove != expected_remaining) {
        printk_err("FAIL: count after remove %d expected %d\n", count_after_remove, expected_remaining);
        fail++;
    }
    jlos_kfree(entries);
    if (!fail)
        printk_info("OK: random insert/remove (%u entries)\n", RBTREE_TEST_COUNT);
    return fail;
}

void rbtree_test(void)
{
    int fail = 0;
    printk_info("=== rbtree test start ===\n");
    fail += test_insert_and_find();
    fail += test_order_traversal();
    fail += test_remove();
    fail += test_find_le();
    fail += test_random_insert_remove();
    if (fail)
        printk_err("rbtree: %d FAILURES\n", fail);
    else
        printk_info("rbtree: ALL PASSED\n");
}
