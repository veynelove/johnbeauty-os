#include <fs/buffer_cache.h>
#include <kernel/memory_manager.h>
#include <kernel/page_frame_allocator.h>
#include <dsa/list_lru.h>
#include <hal/spinlock.h>

static jlos_hash_chain_t        s_buffer_hash;
static jlos_list_lru_t          s_buffer_lru;
static jlos_spinlock_t          s_buffer_lock;
static jlos_memory_slab_cache_t *s_bh_cache;
static uint32_t                 s_buffer_count;

static uint32_t buffer_hash_fn(const void *key)
{
    const jlos_buffer_key_t *k = (const jlos_buffer_key_t *)key;
    return jlos_hash_ptr(k->dev) ^ (uint32_t)(k->blocknr >> 32) ^ (uint32_t)k->blocknr;
}

static int buffer_cmp_fn(const void *key, const void *node)
{
    const jlos_buffer_key_t *k = (const jlos_buffer_key_t *)key;
    const jlos_buffer_head_t *bh = container_of((const jlos_hash_node_t *)node, jlos_buffer_head_t, hash);
    if (bh->dev != k->dev) {
        return 1;
    }
    return (bh->blocknr != k->blocknr);
}

void jlos_buffer_cache_init(void)
{
    jlos_hash_chain_init(&s_buffer_hash, JLOS_BUFFER_HASH_BUCKETS, buffer_hash_fn, buffer_cmp_fn);
    jlos_list_lru_init(&s_buffer_lru);
    jlos_spinlock_init(&s_buffer_lock);
    s_bh_cache = jlos_memory_slab_cache_create("buffer_head", sizeof(jlos_buffer_head_t), 0, 0, NULL, NULL);
    s_buffer_count = 0;
}

static jlos_buffer_head_t *buffer_alloc(jlos_hal_block_dev_t *dev, uint64_t blocknr)
{
    jlos_buffer_head_t *bh = jlos_memory_slab_cache_alloc(s_bh_cache);
    if (!bh) {
        return NULL;
    }
    bh->data = (uint8_t *)jlos_page_frame_malloc();
    if (!bh->data) {
        jlos_memory_slab_cache_free(s_bh_cache, bh);
        return NULL;
    }
    bh->dev = dev;
    bh->blocknr = blocknr;
    bh->state = 0;
    jlos_atomic_set(&bh->refcount, 1);
    jlos_list_init(&bh->lru);
    return bh;
}

static void buffer_free(jlos_buffer_head_t *bh)
{
    jlos_page_frame_free(bh->data);
    jlos_memory_slab_cache_free(s_bh_cache, bh);
}

void jlos_buffer_cache_shrink(void)
{
    for (;;) {
        jlos_buffer_head_t *victim = NULL;
        uint32_t fl = jlos_spin_lock_irqsave(&s_buffer_lock);
        jlos_list_head_t *n = jlos_list_lru_evict(&s_buffer_lru);
        if (n) {
            victim = container_of(n, jlos_buffer_head_t, lru);
            jlos_hash_chain_remove(&s_buffer_hash, &victim->hash);
            s_buffer_count--;
        }
        jlos_spin_unlock_irqrestore(&s_buffer_lock, fl);
        if (!victim) {
            return;
        }
        if (victim->state & JLOS_BUF_DIRTY) {
            jlos_hal_block_write(victim->dev, victim->blocknr * JLOS_BUFFER_SECTORS, victim->data, JLOS_BUFFER_SECTORS);
        }
        buffer_free(victim);
    }
}

static void buffer_shrink_locked(void)
{
    while (s_buffer_count >= JLOS_BUFFER_CACHE_MAX) {
        jlos_list_head_t *n = jlos_list_lru_evict(&s_buffer_lru);
        if (!n) {
            break;
        }
        jlos_buffer_head_t *victim = container_of(n, jlos_buffer_head_t, lru);
        jlos_hash_chain_remove(&s_buffer_hash, &victim->hash);
        if (victim->state & JLOS_BUF_DIRTY) {
            jlos_hal_block_write(victim->dev, victim->blocknr * JLOS_BUFFER_SECTORS, victim->data, JLOS_BUFFER_SECTORS);
        }
        buffer_free(victim);
        s_buffer_count--;
    }
}

static void buffer_destroy_visit(jlos_hash_node_t *node, void *arg)
{
    (void)arg;
    jlos_buffer_head_t *bh = container_of(node, jlos_buffer_head_t, hash);
    if (bh->lru.next != &bh->lru) {
        jlos_list_lru_del(&s_buffer_lru, &bh->lru);
    }
    buffer_free(bh);
}

void jlos_buffer_cache_destroy(void)
{
    uint32_t fl = jlos_spin_lock_irqsave(&s_buffer_lock);
    jlos_hash_chain_for_each(&s_buffer_hash, buffer_destroy_visit, NULL);
    s_buffer_count = 0;
    jlos_spin_unlock_irqrestore(&s_buffer_lock, fl);
    jlos_hash_chain_destroy(&s_buffer_hash);
    jlos_memory_slab_cache_destroy(s_bh_cache);
}

jlos_buffer_head_t *jlos_buffer_read(jlos_hal_block_dev_t *dev, uint64_t blocknr)
{
    jlos_buffer_key_t key = {dev, blocknr};
    uint32_t fl = jlos_spin_lock_irqsave(&s_buffer_lock);
    jlos_hash_node_t *node = jlos_hash_chain_see(&s_buffer_hash, &key);
    if (node) {
        jlos_buffer_head_t *bh = container_of(node, jlos_buffer_head_t, hash);
        if (jlos_atomic_inc_return(&bh->refcount) == 1) {
            jlos_list_lru_del(&s_buffer_lru, &bh->lru);
        }
        jlos_spin_unlock_irqrestore(&s_buffer_lock, fl);
        return bh;
    }
    jlos_spin_unlock_irqrestore(&s_buffer_lock, fl);

    jlos_buffer_head_t *bh = buffer_alloc(dev, blocknr);
    if (!bh) {
        return NULL;
    }
    if (jlos_hal_block_read(dev, blocknr * JLOS_BUFFER_SECTORS, bh->data, JLOS_BUFFER_SECTORS) != 0) {
        buffer_free(bh);
        return NULL;
    }
    bh->state = JLOS_BUF_UPTODATE;
    
    fl = jlos_spin_lock_irqsave(&s_buffer_lock);
    node = jlos_hash_chain_see(&s_buffer_hash, &key);
    if (node) {
        jlos_buffer_head_t *existing = container_of(node, jlos_buffer_head_t, hash);
        if (jlos_atomic_inc_return(&existing->refcount) == 1) {
            jlos_list_lru_del(&s_buffer_lru, &existing->lru);
        }
        jlos_spin_unlock_irqrestore(&s_buffer_lock, fl);
        buffer_free(bh);
        return existing;
    }
    buffer_shrink_locked();
    jlos_hash_chain_insert(&s_buffer_hash, &key, &bh->hash);
    s_buffer_count++;
    jlos_spin_unlock_irqrestore(&s_buffer_lock, fl);
    return bh;
}

void jlos_buffer_dirty(jlos_buffer_head_t *bh)
{
    if (!bh) {
        return;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&s_buffer_lock);
    bh->state |= JLOS_BUF_DIRTY;
    jlos_spin_unlock_irqrestore(&s_buffer_lock, fl);
}

void jlos_buffer_put(jlos_buffer_head_t *bh)
{
    if (!bh) {
        return;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&s_buffer_lock);
    if (jlos_atomic_dec_return(&bh->refcount) == 0) {
        jlos_list_lru_add_tail(&s_buffer_lru, &bh->lru);
    }
    jlos_spin_unlock_irqrestore(&s_buffer_lock, fl);
}

static void buffer_sync_visit(jlos_hash_node_t *node, void *arg)
{
    (void)arg;
    jlos_buffer_head_t *bh = container_of(node, jlos_buffer_head_t, hash);
    if (bh->state & JLOS_BUF_DIRTY) {
        bh->state &= ~JLOS_BUF_DIRTY;
        jlos_hal_block_write(bh->dev, bh->blocknr * JLOS_BUFFER_SECTORS, bh->data, JLOS_BUFFER_SECTORS);
    }
}

void jlos_buffer_sync_all(void)
{
    uint32_t fl = jlos_spin_lock_irqsave(&s_buffer_lock);
    jlos_hash_chain_for_each(&s_buffer_hash, buffer_sync_visit, NULL);
    jlos_spin_unlock_irqrestore(&s_buffer_lock, fl);
}
