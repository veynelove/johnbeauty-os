#ifndef _JLOS_DSA_LIST_H
#define _JLOS_DSA_LIST_H

#include <common/types.h>

typedef struct jlos_list_head {
    struct jlos_list_head *next;
    struct jlos_list_head *prev;
} jlos_list_head_t;

#define JLOS_LIST_HEAD_INIT(name) {&(name), &(name)}
#define JLOS_LIST_HEAD(name) \
    jlos_list_head_t (name) = JLOS_LIST_HEAD_INIT((name))


#define jlos_list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define jlos_list_for_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define jlos_list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; \
         pos != (head); \
         pos = n, n = pos->next)

#define jlos_list_for_each_entry(pos, head, member) \
    for (pos = container_of((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = container_of(pos->member.next, typeof(*pos), member))

#define jlos_list_for_each_entry_reverse(pos, head, member) \
    for (pos = container_of((head)->prev, typeof(*pos), member); \
         &pos->member != (head); \
         pos = container_of(pos->member.prev, typeof(*pos), member))

#define jlos_list_for_each_entry_safe(pos, n, head, member) \
    for (pos = container_of((head)->next, typeof(*pos), member), \
         n = container_of(pos->member.next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = n, n = container_of(n->member.next, typeof(*pos), member))

static inline void jlos_list_init(jlos_list_head_t *h)
{
    h->next = h;
    h->prev = h;
}

static inline bool jlos_list_empty(const jlos_list_head_t *h)
{
    return h->next == h;
}

static inline void jlos_list_add_1(jlos_list_head_t *add, jlos_list_head_t *prev, jlos_list_head_t *next)
{
    next->prev = add;
    add->next = next;
    add->prev = prev;
    prev->next = add;
}

static inline void jlos_list_add(jlos_list_head_t *add, jlos_list_head_t *head)
{
    jlos_list_add_1(add, head, head->next);
}

static inline void jlos_list_add_tail(jlos_list_head_t *add, jlos_list_head_t *head)
{
    jlos_list_add_1(add, head->prev, head);
}

static inline void jlos_list_del_1(jlos_list_head_t *prev, jlos_list_head_t *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void jlos_list_del(jlos_list_head_t *entry)
{
    jlos_list_del_1(entry->prev, entry->next);
    entry->next = entry;
    entry->prev = entry;
}

static inline void jlos_list_del_init(jlos_list_head_t *entry)
{
    jlos_list_del_1(entry->prev, entry->next);
    jlos_list_init(entry);
}

static inline void jlos_list_move(jlos_list_head_t *entry, jlos_list_head_t *head)
{
    jlos_list_del_1(entry->prev, entry->next);
    jlos_list_add(entry, head);
}

static inline void jlos_list_move_tail(jlos_list_head_t *entry, jlos_list_head_t *head)
{
    jlos_list_del_1(entry->prev, entry->next);
    jlos_list_add_tail(entry, head);
}

static inline bool jlos_list_is_first(jlos_list_head_t *entry, jlos_list_head_t *head)
{
    return entry->prev == head;
}

static inline bool jlos_list_is_last(jlos_list_head_t *entry, jlos_list_head_t *head)
{
    return entry->next == head;
}

#endif
