#ifndef _JLOS_DSA_TIMER_WHEEL_H
#define _JLOS_DSA_TIMER_WHEEL_H

#include <common/types.h>
#include <dsa/list.h>

#define JLOS_TIMER_WHEEL_L1_BITS    8
#define JLOS_TIMER_WHEEL_L1_SIZE    (1 << JLOS_TIMER_WHEEL_L1_BITS)
#define JLOS_TIMER_WHEEL_L1_MASK    (JLOS_TIMER_WHEEL_L1_SIZE - 1)

#define JLOS_TIMER_WHEEL_LN_BITS    6
#define JLOS_TIMER_WHEEL_LN_SIZE    (1 << JLOS_TIMER_WHEEL_LN_BITS)
#define JLOS_TIMER_WHEEL_LN_MASK    (JLOS_TIMER_WHEEL_LN_SIZE - 1)

#define JLOS_TIMER_WHEEL_SHIFT_L1   0
#define JLOS_TIMER_WHEEL_SHIFT_L2   JLOS_TIMER_WHEEL_L1_BITS
#define JLOS_TIMER_WHEEL_SHIFT_L3   (JLOS_TIMER_WHEEL_L1_BITS + JLOS_TIMER_WHEEL_LN_BITS)
#define JLOS_TIMER_WHEEL_SHIFT_L4   (JLOS_TIMER_WHEEL_L1_BITS + 2 * JLOS_TIMER_WHEEL_LN_BITS)
#define JLOS_TIMER_WHEEL_SHIFT_L5   (JLOS_TIMER_WHEEL_L1_BITS + 3 * JLOS_TIMER_WHEEL_LN_BITS)

#define JLOS_TIMER_WHEEL_L2_THRESHOLD   (1 << (JLOS_TIMER_WHEEL_L1_BITS + JLOS_TIMER_WHEEL_LN_BITS))
#define JLOS_TIMER_WHEEL_L3_THRESHOLD   (1 << (JLOS_TIMER_WHEEL_L1_BITS + 2 * JLOS_TIMER_WHEEL_LN_BITS))
#define JLOS_TIMER_WHEEL_L4_THRESHOLD   (1 << (JLOS_TIMER_WHEEL_L1_BITS + 3 * JLOS_TIMER_WHEEL_LN_BITS))

typedef struct {
    jlos_list_head_t    node;
    uint32_t            expires;
} jlos_timer_wheel_node_t;

typedef struct {
    jlos_list_head_t    vec1[JLOS_TIMER_WHEEL_L1_SIZE];
    jlos_list_head_t    vec2[JLOS_TIMER_WHEEL_LN_SIZE];
    jlos_list_head_t    vec3[JLOS_TIMER_WHEEL_LN_SIZE];
    jlos_list_head_t    vec4[JLOS_TIMER_WHEEL_LN_SIZE];
    jlos_list_head_t    vec5[JLOS_TIMER_WHEEL_LN_SIZE];
    uint32_t            jiffies;
} jlos_timer_wheel_t;

typedef void (*jlos_timer_wheel_expire_fn)(jlos_timer_wheel_node_t *node, void *ctx);

void jlos_timer_wheel_init(jlos_timer_wheel_t *w, uint32_t jiffies);
void jlos_timer_wheel_add(jlos_timer_wheel_t *w, jlos_timer_wheel_node_t *node);
void jlos_timer_wheel_del(jlos_timer_wheel_node_t *node);
void jlos_timer_wheel_advance(jlos_timer_wheel_t *w, uint32_t now, jlos_timer_wheel_expire_fn expire, void *ctx);

#endif
