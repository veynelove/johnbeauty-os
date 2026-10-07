/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/test.h>
#include <dsa/timer_wheel.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_tw"
#include <kernel/printk.h>

typedef struct {
    uint32_t expired_count;
} tw_test_ctx_t;

static void tw_test_expire(jlos_timer_wheel_node_t *node, void *ctx)
{
    (void)node;
    tw_test_ctx_t *c = (tw_test_ctx_t *)ctx;
    c->expired_count++;
}

JLOS_TEST(timer_wheel, init_all_slots)
{
    jlos_timer_wheel_t w;
    jlos_timer_wheel_init(&w, 0);
    for (uint32_t i = 0; i < JLOS_TIMER_WHEEL_LN_SIZE; i++) {
        JLOS_TEST_TRUE(jlos_list_empty(&w.vec2[i]));
        JLOS_TEST_TRUE(jlos_list_empty(&w.vec3[i]));
        JLOS_TEST_TRUE(jlos_list_empty(&w.vec4[i]));
        JLOS_TEST_TRUE(jlos_list_empty(&w.vec5[i]));
    }
    jlos_timer_wheel_node_t node;
    node.expires = 50;
    jlos_timer_wheel_add(&w, &node);
    tw_test_ctx_t ctx = {0};
    jlos_timer_wheel_advance(&w, 50, tw_test_expire, &ctx);
    JLOS_TEST_EQ(ctx.expired_count, 1);
}

JLOS_TEST(timer_wheel, cascade_vec2_to_vec1)
{
    jlos_timer_wheel_t w;
    jlos_timer_wheel_init(&w, 0);
    jlos_timer_wheel_node_t node;
    node.expires = 300;
    jlos_timer_wheel_add(&w, &node);
    tw_test_ctx_t ctx = {0};
    jlos_timer_wheel_advance(&w, 255, tw_test_expire, &ctx);
    JLOS_TEST_EQ(ctx.expired_count, 0);
    jlos_timer_wheel_advance(&w, 300, tw_test_expire, &ctx);
    JLOS_TEST_EQ(ctx.expired_count, 1);
}

JLOS_TEST(timer_wheel, cascade_multi_level)
{
    jlos_timer_wheel_t w;
    jlos_timer_wheel_init(&w, 0);
    jlos_timer_wheel_node_t node;
    node.expires = JLOS_TIMER_WHEEL_L2_THRESHOLD + 300;
    jlos_timer_wheel_add(&w, &node);
    tw_test_ctx_t ctx = {0};
    jlos_timer_wheel_advance(&w, node.expires, tw_test_expire, &ctx);
    JLOS_TEST_EQ(ctx.expired_count, 1);
}

JLOS_TEST(timer_wheel, boundary_deltas)
{
    jlos_timer_wheel_t w;
    jlos_timer_wheel_init(&w, 0);
    jlos_timer_wheel_node_t n0;
    n0.expires = 0;
    jlos_timer_wheel_add(&w, &n0);
    jlos_timer_wheel_node_t n256;
    n256.expires = JLOS_TIMER_WHEEL_L1_SIZE;
    jlos_timer_wheel_add(&w, &n256);
    tw_test_ctx_t ctx = {0};
    jlos_timer_wheel_advance(&w, JLOS_TIMER_WHEEL_L1_SIZE, tw_test_expire, &ctx);
    JLOS_TEST_EQ(ctx.expired_count, 2);
}

JLOS_TEST(timer_wheel, del_prevents_expire)
{
    jlos_timer_wheel_t w;
    jlos_timer_wheel_init(&w, 0);
    jlos_timer_wheel_node_t node;
    node.expires = 50;
    jlos_timer_wheel_add(&w, &node);
    jlos_timer_wheel_del(&node);
    tw_test_ctx_t ctx = {0};
    jlos_timer_wheel_advance(&w, 100, tw_test_expire, &ctx);
    JLOS_TEST_EQ(ctx.expired_count, 0);
}

JLOS_TEST(timer_wheel, empty_advance)
{
    jlos_timer_wheel_t w;
    jlos_timer_wheel_init(&w, 0);
    tw_test_ctx_t ctx = {0};
    jlos_timer_wheel_advance(&w, 100000, tw_test_expire, &ctx);
    JLOS_TEST_EQ(ctx.expired_count, 0);
    JLOS_TEST_EQ(w.jiffies, 100001);
}

JLOS_TEST(timer_wheel, same_slot_multiple)
{
    jlos_timer_wheel_t w;
    jlos_timer_wheel_init(&w, 0);
    jlos_timer_wheel_node_t nodes[3];
    for (int i = 0; i < 3; i++) {
        nodes[i].expires = 50;
        jlos_timer_wheel_add(&w, &nodes[i]);
    }
    tw_test_ctx_t ctx = {0};
    jlos_timer_wheel_advance(&w, 50, tw_test_expire, &ctx);
    JLOS_TEST_EQ(ctx.expired_count, 3);
}