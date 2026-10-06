#include <dsa/timer_wheel.h>

static jlos_list_head_t *timer_wheel_slot(jlos_timer_wheel_t *w, uint32_t expires, uint32_t jiffies)
{
    int32_t delta = (int32_t)(expires - jiffies);
    if (delta < 0) {
        return &w->vec1[jiffies & JLOS_TIMER_WHEEL_L1_MASK];
    }
    uint32_t idx = (uint32_t)delta;
    if (idx < JLOS_TIMER_WHEEL_L1_SIZE) {
        return &w->vec1[expires & JLOS_TIMER_WHEEL_L1_MASK];
    } else if (idx < JLOS_TIMER_WHEEL_L2_THRESHOLD) {
        return &w->vec2[(expires >> JLOS_TIMER_WHEEL_SHIFT_L2) & JLOS_TIMER_WHEEL_LN_MASK];
    } else if (idx < JLOS_TIMER_WHEEL_L3_THRESHOLD) {
        return &w->vec3[(expires >> JLOS_TIMER_WHEEL_SHIFT_L3) & JLOS_TIMER_WHEEL_LN_MASK];
    } else if (idx < JLOS_TIMER_WHEEL_L4_THRESHOLD) {
        return &w->vec4[(expires >> JLOS_TIMER_WHEEL_SHIFT_L4) & JLOS_TIMER_WHEEL_LN_MASK];
    } else {
        return &w->vec5[(expires >> JLOS_TIMER_WHEEL_SHIFT_L5) & JLOS_TIMER_WHEEL_LN_MASK];
    }
}

void jlos_timer_wheel_init(jlos_timer_wheel_t *w, uint32_t jiffies)
{
    for (uint32_t i = 0; i < JLOS_TIMER_WHEEL_L1_SIZE; i++) {
        jlos_list_init(&w->vec1[i]);
    }
    for (uint32_t i = 0; i < JLOS_TIMER_WHEEL_LN_SIZE; i++) {
        jlos_list_init(&w->vec2[i]);
        jlos_list_init(&w->vec3[i]);
        jlos_list_init(&w->vec4[i]);
        jlos_list_init(&w->vec5[i]);
    }
    w->jiffies = jiffies;
}

void jlos_timer_wheel_add(jlos_timer_wheel_t *w, jlos_timer_wheel_node_t *node)
{
    jlos_list_head_t *slot = timer_wheel_slot(w, node->expires, w->jiffies);
    jlos_list_add_tail(&node->node, slot);
}

void jlos_timer_wheel_del(jlos_timer_wheel_node_t *node)
{
    jlos_list_del_init(&node->node);
}

static void timer_wheel_cascade(jlos_timer_wheel_t *w, jlos_list_head_t *vec)
{
    jlos_list_head_t *pos, *n;
    jlos_list_for_each_safe(pos, n, vec) {
        jlos_timer_wheel_node_t *node = container_of(pos, jlos_timer_wheel_node_t, node);
        jlos_list_del_init(&node->node);
        jlos_timer_wheel_add(w, node);
    }
}

void jlos_timer_wheel_advance(jlos_timer_wheel_t *w, uint32_t now, jlos_timer_wheel_expire_fn expire, void *ctx)
{
    while ((int32_t)(now - w->jiffies) >= 0) {
        uint32_t j = w->jiffies;
        uint32_t i1 = j & JLOS_TIMER_WHEEL_L1_MASK;
        if (i1 == 0) {
            uint32_t i2 = (j >> JLOS_TIMER_WHEEL_SHIFT_L2) & JLOS_TIMER_WHEEL_LN_MASK;
            timer_wheel_cascade(w, &w->vec2[i2]);
            if (i2 == 0) {
                uint32_t i3 = (j >> JLOS_TIMER_WHEEL_SHIFT_L3) & JLOS_TIMER_WHEEL_LN_MASK;
                timer_wheel_cascade(w, &w->vec3[i3]);
                if (i3 == 0) {
                    uint32_t i4 = (j >> JLOS_TIMER_WHEEL_SHIFT_L4) & JLOS_TIMER_WHEEL_LN_MASK;
                    timer_wheel_cascade(w, &w->vec4[i4]);
                    if (i4 == 0) {
                        uint32_t i5 = (j >> JLOS_TIMER_WHEEL_SHIFT_L5) & JLOS_TIMER_WHEEL_LN_MASK;
                        timer_wheel_cascade(w, &w->vec5[i5]);
                    }
                }
            }
        }
        w->jiffies++;
        jlos_list_head_t *pos, *n;
        jlos_list_for_each_safe(pos, n, &w->vec1[i1]) {
            jlos_timer_wheel_node_t *node = container_of(pos, jlos_timer_wheel_node_t, node);
            jlos_list_del_init(&node->node);
            expire(node, ctx);
        }
    }
}
