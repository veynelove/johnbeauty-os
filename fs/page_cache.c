/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fs/page_cache.h>
#include <kernel/memory_manager.h>
#include <dsa/list_lru.h>

static jlos_memory_slab_cache_t *s_bh_cache;
static jlos_list_lru_t           s_page_lru;
static jlos_spinlock_t           s_page_lock;

static int page_cmp(const jlos_rbtree_node_t *a, const jlos_rbtree_node_t *b)
{
    const jlos_page_t *pa = jlos_rbtree_entry(a, jlos_page_t, node);
    const jlos_page_t *pb = jlos_rbtree_entry(b, jlos_page_t, node);
    if (pa->u.file.index < pb->u.file.index) {
        return -1;
    }
    if (pa->u.file.index > pb->u.file.index) {
        return 1;
    }
    return 0;
}

static int page_cmp_key(const jlos_rbtree_node_t *node, const void *key)
{
    const jlos_page_t *page = jlos_rbtree_entry(node, jlos_page_t, node);
    uint64_t index = *(const uint64_t *)key;
    if (page->u.file.index < index) {
        return -1;
    }
    if (page->u.file.index > index) {
        return 1;
    }
    return 0;
}

void jlos_fs_page_cache_init(void)
{
    jlos_list_lru_init(&s_page_lru);
    jlos_spinlock_init(&s_page_lock);
    s_bh_cache = jlos_memory_slab_cache_create("fs_buffer_head_array",
        sizeof(jlos_fs_buffer_head_t) * JLOS_FS_PAGE_SECTORS, 0, 0, NULL, NULL);
}

jlos_fs_address_space_t *jlos_fs_address_space_init(jlos_fs_address_space_t *self, void *host, jlos_hal_block_dev_t *bdev)
{
    if (!self) {
        return NULL;
    }
    jlos_memset(self, 0, sizeof(jlos_fs_address_space_t));
    self->host = host;
    self->bdev = bdev;
    jlos_rbtree_init(&self->page_tree);
    jlos_spinlock_init(&self->lock);
    return self;
}

static jlos_page_t *page_alloc(jlos_fs_address_space_t *mapping, uint64_t index)
{
    void *addr = jlos_page_frame_malloc();
    if (!addr) {
        return NULL;
    }
    jlos_page_t *page = jlos_page_frame_to_page(addr);
    jlos_list_init(&page->lru);
    page->bufs = NULL;
    page->page_state = 0;
    page->type = JLOS_PAGE_FRAME_TYPE_FILE_PAGE;
    page->u.file.mapping = mapping;
    page->u.file.index = index;
    return page;
}

static void page_free(jlos_page_t *page)
{
    if (page->bufs) {
        jlos_memory_slab_cache_free(s_bh_cache, page->bufs);
    }
    jlos_page_frame_free(jlos_page_frame_page_addr(page));
}

static void page_ref_acquire(jlos_page_t *page)
{
    uint32_t fl = jlos_spin_lock_irqsave(&s_page_lock);
    if (jlos_atomic_read(&page->refcount) == 1) {
        jlos_list_lru_del(&s_page_lru, &page->lru);
    }
    jlos_atomic_inc(&page->refcount);
    jlos_spin_unlock_irqrestore(&s_page_lock, fl);
}

static int page_read_block(jlos_page_t *page)
{
    jlos_fs_address_space_t *mapping = (jlos_fs_address_space_t *)page->u.file.mapping;
    jlos_hal_block_dev_t *dev = mapping->bdev;
    uint8_t *data = (uint8_t *)jlos_page_frame_page_addr(page);
    uint64_t start = page->u.file.index * JLOS_FS_PAGE_SECTORS;
    if (jlos_hal_block_read(dev, start, data, JLOS_FS_PAGE_SECTORS) != 0) {
        return -1;
    }
    page->bufs = jlos_memory_slab_cache_alloc(s_bh_cache);
    if (!page->bufs) {
        return -1;
    }
    jlos_fs_buffer_head_t *bh = (jlos_fs_buffer_head_t *)page->bufs;
    for (uint32_t i = 0; i < JLOS_FS_PAGE_SECTORS; i++) {
        bh[i].blocknr = start + i;
        bh[i].state = JLOS_FS_BH_UPTODATE;
    }
    page->page_state |= JLOS_FS_PAGE_UPTODATE;
    return 0;
}

static int page_write_block(jlos_page_t *page)
{
    jlos_fs_address_space_t *mapping = (jlos_fs_address_space_t *)page->u.file.mapping;
    jlos_hal_block_dev_t *dev = mapping->bdev;
    uint8_t *data = (uint8_t *)jlos_page_frame_page_addr(page);
    jlos_fs_buffer_head_t *bh = (jlos_fs_buffer_head_t *)page->bufs;
    if (bh) {
        for (uint32_t i = 0; i < JLOS_FS_PAGE_SECTORS; i++) {
            if (bh[i].state & JLOS_FS_BH_DIRTY) {
                if (jlos_hal_block_write(dev, bh[i].blocknr,
                        data + i * JLOS_BLOCK_SECTOR_SIZE, 1) != 0) {
                    return -1;
                }
                bh[i].state &= ~JLOS_FS_BH_DIRTY;
            }
        }
    } else {
        uint64_t start = page->u.file.index * JLOS_FS_PAGE_SECTORS;
        if (jlos_hal_block_write(dev, start, data, JLOS_FS_PAGE_SECTORS) != 0) {
            return -1;
        }
    }
    page->page_state &= ~JLOS_FS_PAGE_DIRTY;
    return 0;
}

static int page_writeback(jlos_page_t *page)
{
    if (!(page->page_state & JLOS_FS_PAGE_DIRTY)) {
        return 0;
    }
    jlos_fs_address_space_t *mapping = (jlos_fs_address_space_t *)page->u.file.mapping;
    if (mapping->writepage) {
        return mapping->writepage(mapping, page);
    }
    return page_write_block(page);
}

int jlos_fs_block_read_full_page(jlos_fs_address_space_t *mapping, jlos_page_t *page, jlos_fs_get_block_fn get_block)
{
    jlos_hal_block_dev_t *dev = mapping->bdev;
    uint8_t *data = (uint8_t *)jlos_page_frame_page_addr(page);
    uint64_t file_sector = page->u.file.index * JLOS_FS_PAGE_SECTORS;

    jlos_fs_buffer_head_t *bh = jlos_memory_slab_cache_alloc(s_bh_cache);
    if (!bh) {
        return -1;
    }
    uint32_t mapped = 0;
    for (uint32_t i = 0; i < JLOS_FS_PAGE_SECTORS; i++) {
        uint64_t phys;
        if (get_block(mapping, file_sector + i, &phys, 0) == 0) {
            bh[i].blocknr = phys;
            bh[i].state = JLOS_FS_BH_UPTODATE;
            if (jlos_hal_block_read(dev, phys, data + i * JLOS_BLOCK_SECTOR_SIZE, 1) != 0) {
                jlos_memory_slab_cache_free(s_bh_cache, bh);
                return -1;
            }
            mapped++;
        } else {
            bh[i].blocknr = 0;
            bh[i].state = 0;
            jlos_memset(data + i * JLOS_BLOCK_SECTOR_SIZE, 0, JLOS_BLOCK_SECTOR_SIZE);
        }
    }
    if (mapped == 0) {
        jlos_memory_slab_cache_free(s_bh_cache, bh);
        return -1;
    }
    page->bufs = bh;
    page->page_state |= JLOS_FS_PAGE_UPTODATE;
    return 0;
}

int jlos_fs_block_write_full_page(jlos_fs_address_space_t *mapping, jlos_page_t *page, jlos_fs_get_block_fn get_block)
{
    jlos_hal_block_dev_t *dev = mapping->bdev;
    uint8_t *data = (uint8_t *)jlos_page_frame_page_addr(page);
    jlos_fs_buffer_head_t *bh = (jlos_fs_buffer_head_t *)page->bufs;
    if (!bh) {
        return -1;
    }
    uint64_t file_sector = page->u.file.index * JLOS_FS_PAGE_SECTORS;
    for (uint32_t i = 0; i < JLOS_FS_PAGE_SECTORS; i++) {
        if (!(bh[i].state & JLOS_FS_BH_DIRTY)) {
            continue;
        }
        uint64_t phys;
        if (get_block(mapping, file_sector + i, &phys, 0) != 0) {
            continue;
        }
        if (jlos_hal_block_write(dev, phys, data + i * JLOS_BLOCK_SECTOR_SIZE, 1) != 0) {
            return -1;
        }
        bh[i].state &= ~JLOS_FS_BH_DIRTY;
    }
    page->page_state &= ~JLOS_FS_PAGE_DIRTY;
    return 0;
}

jlos_page_t *jlos_fs_page_get(jlos_fs_address_space_t *mapping, uint64_t index)
{
    uint32_t fl = jlos_spin_lock_irqsave(&mapping->lock);
    jlos_rbtree_node_t *node = jlos_rbtree_find_key(&mapping->page_tree, &index, page_cmp_key);
    if (node) {
        jlos_page_t *page = jlos_rbtree_entry(node, jlos_page_t, node);
        jlos_spin_unlock_irqrestore(&mapping->lock, fl);
        page_ref_acquire(page);
        return page;
    }
    jlos_spin_unlock_irqrestore(&mapping->lock, fl);

    jlos_page_t *page = page_alloc(mapping, index);
    if (!page) {
        return NULL;
    }
    if (mapping->readpage) {
        if (mapping->readpage(mapping, page) != 0) {
            page_free(page);
            return NULL;
        }
    } else if (page_read_block(page) != 0) {
        page_free(page);
        return NULL;
    }

    jlos_atomic_inc(&page->refcount);

    fl = jlos_spin_lock_irqsave(&mapping->lock);
    node = jlos_rbtree_find_key(&mapping->page_tree, &index, page_cmp_key);
    if (node) {
        jlos_page_t *existing = jlos_rbtree_entry(node, jlos_page_t, node);
        jlos_spin_unlock_irqrestore(&mapping->lock, fl);
        jlos_atomic_dec(&page->refcount);
        page_free(page);
        page_ref_acquire(existing);
        return existing;
    }
    jlos_rbtree_insert(&mapping->page_tree, &page->node, page_cmp);
    jlos_spin_unlock_irqrestore(&mapping->lock, fl);
    return page;
}

void jlos_fs_page_put(jlos_page_t *page)
{
    if (!page) {
        return;
    }
    bool shrink_now = false;
    uint32_t fl = jlos_spin_lock_irqsave(&s_page_lock);
    if (jlos_atomic_dec_return(&page->refcount) == 1) {
        jlos_list_lru_add_tail(&s_page_lru, &page->lru);
        shrink_now = (s_page_lru.count > JLOS_FS_PAGE_CACHE_MAX);
    }
    jlos_spin_unlock_irqrestore(&s_page_lock, fl);
    if (shrink_now) {
        jlos_fs_page_cache_shrink();
    }
}

void jlos_fs_page_dirty(jlos_page_t *page)
{
    if (!page) {
        return;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&s_page_lock);
    page->page_state |= JLOS_FS_PAGE_DIRTY;
    jlos_fs_buffer_head_t *bh = (jlos_fs_buffer_head_t *)page->bufs;
    if (bh) {
        for (uint32_t i = 0; i < JLOS_FS_PAGE_SECTORS; i++) {
            bh[i].state |= JLOS_FS_BH_DIRTY;
        }
    }
    jlos_spin_unlock_irqrestore(&s_page_lock, fl);
}

void jlos_fs_page_bh_dirty(jlos_page_t *page, uint32_t sector_idx)
{
    if (!page || !page->bufs || sector_idx >= JLOS_FS_PAGE_SECTORS) {
        return;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&s_page_lock);
    jlos_fs_buffer_head_t *bh = (jlos_fs_buffer_head_t *)page->bufs;
    bh[sector_idx].state |= JLOS_FS_BH_DIRTY;
    page->page_state |= JLOS_FS_PAGE_DIRTY;
    jlos_spin_unlock_irqrestore(&s_page_lock, fl);
}

void jlos_fs_page_cache_shrink(void)
{
    for (;;) {
        jlos_page_t *victim = NULL;
        uint32_t fl = jlos_spin_lock_irqsave(&s_page_lock);
        if (s_page_lru.count > JLOS_FS_PAGE_CACHE_MAX) {
            jlos_list_head_t *n = jlos_list_lru_evict(&s_page_lru);
            if (n) {
                victim = container_of(n, jlos_page_t, lru);
            }
        }
        jlos_spin_unlock_irqrestore(&s_page_lock, fl);
        if (!victim) {
            return;
        }
        page_writeback(victim);
        jlos_fs_address_space_t *mapping = (jlos_fs_address_space_t *)victim->u.file.mapping;
        uint32_t fl2 = jlos_spin_lock_irqsave(&mapping->lock);
        jlos_rbtree_remove(&mapping->page_tree, &victim->node);
        jlos_spin_unlock_irqrestore(&mapping->lock, fl2);
        page_free(victim);
    }
}

void jlos_fs_page_cache_sync_all(void)
{
    uint32_t fl = jlos_spin_lock_irqsave(&s_page_lock);
    jlos_page_t *page;
    jlos_list_for_each_entry(page, &s_page_lru.list, lru) {
        page_writeback(page);
    }
    jlos_spin_unlock_irqrestore(&s_page_lock, fl);
}
