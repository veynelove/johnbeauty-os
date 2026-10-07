/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_FS_PAGE_CACHE_H
#define _JLOS_FS_PAGE_CACHE_H

#include <common/types.h>
#include <hal/block.h>
#include <hal/atomic.h>
#include <hal/spinlock.h>
#include <hal/paging.h>
#include <kernel/page_frame_allocator.h>
#include <dsa/rbtree.h>
#include <dsa/list.h>

#define JLOS_FS_PAGE_SIZE           JLOS_PAGE_SIZE
#define JLOS_FS_PAGE_SECTORS        (JLOS_FS_PAGE_SIZE / JLOS_BLOCK_SECTOR_SIZE)
#define JLOS_FS_PAGE_CACHE_MAX      512

#define JLOS_FS_BH_UPTODATE         0x01
#define JLOS_FS_BH_DIRTY            0x02
#define JLOS_FS_PAGE_UPTODATE       0x01
#define JLOS_FS_PAGE_DIRTY          0x02
#define JLOS_FS_PAGE_LOCKED         0x04

typedef struct jlos_fs_address_space jlos_fs_address_space_t;

typedef int (*jlos_fs_get_block_fn)(jlos_fs_address_space_t *mapping, uint64_t iblock, uint64_t *phys, int create);

typedef struct {
    uint64_t    blocknr;
    uint8_t     state;
} jlos_fs_buffer_head_t;

struct jlos_fs_address_space {
    void                    *host;
    jlos_hal_block_dev_t    *bdev;
    jlos_rbtree_t           page_tree;
    jlos_spinlock_t         lock;
    int (*readpage)(jlos_fs_address_space_t *mapping, jlos_page_t *page);
    int (*writepage)(jlos_fs_address_space_t *mapping, jlos_page_t *page);
};

void jlos_fs_page_cache_init(void);

jlos_fs_address_space_t *jlos_fs_address_space_init(jlos_fs_address_space_t *self, void *host, jlos_hal_block_dev_t *bdev);

jlos_page_t *jlos_fs_page_get(jlos_fs_address_space_t *mapping, uint64_t index);
void jlos_fs_page_put(jlos_page_t *page);
void jlos_fs_page_dirty(jlos_page_t *page);
void jlos_fs_page_bh_dirty(jlos_page_t *page, uint32_t sector_idx);

void jlos_fs_page_cache_shrink(void);
void jlos_fs_page_cache_sync_all(void);

int jlos_fs_block_read_full_page(jlos_fs_address_space_t *mapping, jlos_page_t *page, jlos_fs_get_block_fn get_block);
int jlos_fs_block_write_full_page(jlos_fs_address_space_t *mapping, jlos_page_t *page, jlos_fs_get_block_fn get_block);

static inline uint64_t jlos_fs_blocknr(uint64_t sector)
{
    return sector / JLOS_FS_PAGE_SECTORS;
}

static inline uint32_t jlos_fs_sector_idx(uint64_t sector)
{
    return (uint32_t)(sector % JLOS_FS_PAGE_SECTORS);
}

#endif