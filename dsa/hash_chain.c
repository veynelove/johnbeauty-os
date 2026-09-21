#include <dsa/hash_chain.h>
#include <kernel/memory_manager.h>
#include <kernel/printk.h>

void jlos_hash_chain_init(jlos_hash_chain_t *self, uint32_t bucket_count,
    uint32_t (*hash)(const void *key), int (*cmp)(const void *key, const void *node))
{
    if (bucket_count == 0) {
        bucket_count = 64;
    }
    uint32_t power = 1;
    while (power < bucket_count) {
        power <<= 1;
    }

    self->bucket_count = power;
    self->bucket_mask = power - 1;
    self->buckets = (jlos_hash_head_t *)jlos_kalloc(power * sizeof(jlos_hash_head_t));
    for (uint32_t i = 0; i < power; i++) {
        self->buckets[i].first = NULL;
    }
    self->hash = hash;
    self->cmp = cmp;
    jlos_spinlock_init(&self->lock);
}

void jlos_hash_chain_insert(jlos_hash_chain_t *self, const void *key, jlos_hash_node_t *node)
{
    uint32_t index = self->hash(key) & self->bucket_mask;
    uint32_t flags = jlos_spin_lock_irqsave(&self->lock);
    node->next = self->buckets[index].first;
    if (self->buckets[index].first) {
        self->buckets[index].first->pprev = &node->next;
    }
    self->buckets[index].first = node;
    node->pprev = &self->buckets[index].first;

    jlos_spin_unlock_irqrestore(&self->lock, flags);
}

jlos_hash_node_t *jlos_hash_chain_see(jlos_hash_chain_t *self, const void *key)
{
    uint32_t index = self->hash(key) & self->bucket_mask;
    uint32_t flags = jlos_spin_lock_irqsave(&self->lock);
    jlos_hash_node_t *node = self->buckets[index].first;
    while (node) {
        if (self->cmp(key, node) == 0) {
            jlos_spin_unlock_irqrestore(&self->lock, flags);
            return node;
        }
        node = node->next;
    }

    jlos_spin_unlock_irqrestore(&self->lock, flags);
    return NULL;
}

jlos_hash_node_t *jlos_hash_chain_find(jlos_hash_chain_t *self, const void *key, jlos_hash_chain_match_t match, void *arg)
{
    uint32_t index = self->hash(key) & self->bucket_mask;
    uint32_t flags = jlos_spin_lock_irqsave(&self->lock);
    jlos_hash_node_t *node = self->buckets[index].first;
    while (node) {
        if (match(node, arg) == 0) {
            jlos_spin_unlock_irqrestore(&self->lock, flags);
            return node;
        }
        node = node->next;
    }
    jlos_spin_unlock_irqrestore(&self->lock, flags);
    return NULL;
}

void jlos_hash_chain_remove(jlos_hash_chain_t *self, jlos_hash_node_t *node)
{
    uint32_t flags = jlos_spin_lock_irqsave(&self->lock);

    if (node->pprev) {
        *node->pprev = node->next;
    }
    if (node->next) {
        node->next->pprev = node->pprev;
    }

    jlos_spin_unlock_irqrestore(&self->lock, flags);
}

void jlos_hash_chain_destroy(jlos_hash_chain_t *self)
{
    uint32_t flags = jlos_spin_lock_irqsave(&self->lock);
    if (self->buckets) {
        for (uint32_t i = 0; i < self->bucket_count; i++) {
            self->buckets[i].first = NULL;
        }
        jlos_kfree(self->buckets);
        self->buckets = NULL;
    }
    self->bucket_count = 0;
    self->bucket_mask = 0;
    self->hash = NULL;
    self->cmp = NULL;
    jlos_spin_unlock_irqrestore(&self->lock, flags);
}

uint32_t jlos_hash_uint16(const void *key)
{
    return *(uint16_t *)key;
}

uint32_t jlos_hash_uint32(const void *key)
{
    return *(uint32_t *)key;
}

uint32_t jlos_hash_ptr(const void *key)
{
    return (uint32_t)key;
}

uint32_t jlos_hash_str(const void *key)
{
    const char *s = (const char *)key;
    uint32_t h = 0;
    while (*s) {
        h = h * JLOS_HASH_STR_PREME + (uint8_t)*s++;
    }
    return h;
}
