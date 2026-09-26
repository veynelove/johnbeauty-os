#ifndef _JLOS_DSA_LIST_LRU_H
#define _JLOS_DSA_LIST_LRU_H

#include <common/types.h>
#include <dsa/list.h>

typedef struct {
    jlos_list_head_t    list;
    uint32_t            count;
} jlos_list_lru_t;

static inline void jlos_list_lru_init(jlos_list_lru_t *lru)
{
    jlos_list_init(&lru->list);
    lru->count = 0;
}

static inline void jlos_list_lru_add_tail(jlos_list_lru_t *lru, jlos_list_head_t *node)
{
    jlos_list_add_tail(node, &lru->list);
    lru->count++;
}

static inline void jlos_list_lru_del(jlos_list_lru_t *lru, jlos_list_head_t *node)
{
    jlos_list_del_init(node);
    lru->count--;
}

static inline bool jlos_list_lru_is_empty(jlos_list_lru_t *lru)
{
    return lru->count == 0;
}

static inline jlos_list_head_t *jlos_list_lru_evict(jlos_list_lru_t *lru)
{
    if (jlos_list_empty(&lru->list)) {
        return NULL;
    }
    jlos_list_head_t *victim = lru->list.next;
    jlos_list_del_init(victim);
    lru->count--;
    return victim;
}

#endif
