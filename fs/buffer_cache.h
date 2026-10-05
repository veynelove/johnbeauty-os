#ifndef _JLOS_FS_BUFFER_CACHE_H
#define _JLOS_FS_BUFFER_CACHE_H

#include <common/types.h>
#include <hal/block.h>
#include <hal/atomic.h>
#include <hal/paging.h>
#include <dsa/hash_chain.h>
#include <dsa/list.h>

#define JLOS_BUFFER_SIZE            JLOS_PAGE_SIZE
#define JLOS_BUFFER_SECTORS         (JLOS_BUFFER_SIZE / JLOS_BLOCK_SECTOR_SIZE)
#define JLOS_BUFFER_CACHE_MAX       256
#define JLOS_BUFFER_HASH_BUCKETS    256

#define JLOS_BUF_UPTODATE           0x01
#define JLOS_BUF_DIRTY              0x02

typedef struct jlos_buffer_head {
    jlos_hal_block_dev_t    *dev;
    uint64_t                blocknr;
    uint8_t                 *data;
    uint8_t                 state;
    jlos_atomic_t           refcount;
    jlos_hash_node_t        hash;
    jlos_list_head_t        lru;
} jlos_buffer_head_t;

typedef struct {
    jlos_hal_block_dev_t    *dev;
    uint64_t                blocknr;
} jlos_buffer_key_t;

void jlos_buffer_cache_init(void);
void jlos_buffer_cache_destroy(void);

jlos_buffer_head_t *jlos_buffer_read(jlos_hal_block_dev_t *dev, uint64_t blocknr);
void jlos_buffer_dirty(jlos_buffer_head_t *bh);
void jlos_buffer_put(jlos_buffer_head_t *bh);
void jlos_buffer_sync_all(void);

static inline uint64_t jlos_buffer_blocknr(uint64_t sector)
{
    return sector / JLOS_BUFFER_SECTORS;
}

static inline uint32_t jlos_buffer_offset(uint64_t sector)
{
    return (uint32_t)((sector % JLOS_BUFFER_SECTORS) * JLOS_BLOCK_SECTOR_SIZE);
}

#endif
