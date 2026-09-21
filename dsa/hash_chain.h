#ifndef _JLOS_DSA_HASH_CHAIN_H
#define _JLOS_DSA_HASH_CHAIN_H

#include <common/types.h>
#include <hal/spinlock.h>

#define JLOS_HASH_STR_PREME 31

typedef struct jlos_hash_node {
    struct jlos_hash_node *next;
    struct jlos_hash_node **pprev;
} jlos_hash_node_t;

typedef struct jlos_hash_head {
    jlos_hash_node_t *first;
} jlos_hash_head_t;

typedef struct {
    jlos_hash_head_t    *buckets;
    uint32_t            bucket_count;
    uint32_t            bucket_mask;
    uint32_t            (*hash)(const void *key);
    int                 (*cmp)(const void *key, const void *node);
    jlos_spinlock_t     lock;
} jlos_hash_chain_t;

typedef int (*jlos_hash_chain_match_t)(jlos_hash_node_t *node, void *args);

void jlos_hash_chain_init(jlos_hash_chain_t *self, uint32_t bucket_count,
    uint32_t (*hash)(const void *key), int (*cmp)(const void *key, const void *node));

    void jlos_hash_chain_destroy(jlos_hash_chain_t *self);
void jlos_hash_chain_insert(jlos_hash_chain_t *self, const void *key, jlos_hash_node_t *node);

jlos_hash_node_t *jlos_hash_chain_see(jlos_hash_chain_t *self, const void *key);
jlos_hash_node_t *jlos_hash_chain_find(jlos_hash_chain_t *self, const void *key, jlos_hash_chain_match_t match, void *arg);

void jlos_hash_chain_remove(jlos_hash_chain_t *self, jlos_hash_node_t *node);

uint32_t jlos_hash_uint16(const void *key);
uint32_t jlos_hash_uint32(const void *key);
uint32_t jlos_hash_ptr(const void *key);
uint32_t jlos_hash_str(const void *key);

#endif
