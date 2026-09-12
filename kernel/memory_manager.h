#ifndef _JLOS_KERNEL_MEMORY_MANAGER_H
#define _JLOS_KERNEL_MEMORY_MANAGER_H

#include <common/types.h>
#include <hal/smp.h>
#include <dsa/list.h>

#define KERNEL_MEMORY_PHYSICAL_START        0x100000

#define KERNEL_LOW_MEMORY_ADDR_START        0x50000
#define KERNEL_LOW_MEMORY_SIZE              0x50000

#define KERNEL_MAIN_MEMORY_MIN_SIZE         (4 * 1024 * 1024)
#define KERNEL_MAIN_MEMORY_MAX_SIZE         (64 * 1024 * 1024)

#define JLOS_MM_MIN_ALLOC      16
#define JLOS_MM_CLASS_COUNT    32

#define JLOS_KV_CONTIG_MAGIC        0x4B564354U
#define JLOS_KV_CONTIG_MAX_PAGES    0xFFFFFFFEU

typedef struct jlos_memory_chunk {
    jlos_list_head_t            link;
    jlos_list_head_t            free_link;
    bool                        allocated;
    size_t                      size;
} __attribute__((aligned(16))) jlos_memory_chunk_t;

typedef struct {
    jlos_list_head_t    chunk_head;
    jlos_memory_chunk_t *tail;
    uint8_t             *heap_start;
    uint8_t             *heap_end;
    uint8_t             *heap_current;
    uint32_t            size_bitmap;
    int                 max_class;
    jlos_list_head_t    class_head[JLOS_MM_CLASS_COUNT];
} jlos_memory_manager_t;

typedef struct {
    uint32_t magic;
    uint32_t npages;
} jlos_kv_contig_hdr_t;

extern jlos_memory_manager_t *jlos_active_memory_manager;
extern jlos_memory_manager_t *jlos_low_memory_manager;
extern jlos_memory_manager_t *jlos_main_memory_manager;

void jlos_memory_manager_init_low(void);
void jlos_memory_manager_init_main(void);
void jlos_memory_manager_init(void);

void jlos_memory_manager_switch_low(void);
void Jlos_memory_manager_switch_main(void);

void jlos_memory_manager_destroy(jlos_memory_manager_t* self);

void *jlos_memory_manager_malloc(jlos_memory_manager_t* self, size_t size);
void jlos_memory_manager_free(jlos_memory_manager_t* self, void *ptr);

void *jlos_kvalloc(size_t size);
void jlos_kvfree(void *ptr);
void jlos_memset(void *ptr, uint8_t value, size_t size);
void *jlos_memcpy(void *dst, const void *src, size_t size);
size_t jlos_strlcpy(char *dst, const char *src, size_t dsize);

void jlos_kvalloc_stats(jlos_memory_manager_t *self);

//unqueueed slab allocator
typedef void (*jlos_memory_slab_ctor_t)(void *obj);
typedef void (*jlos_memory_slab_dtor_t)(void *obj);

typedef struct jlos_memory_slab_page jlos_memory_slab_page_t;

typedef struct {
    jlos_memory_slab_page_t *partial;
    uint32_t                 partial_count;
} jlos_memory_slab_cpu_t;

typedef struct jlos_memory_slab_cache {
    char                     name[20];
    size_t                   size;
    size_t                   obj_size;
    size_t                   align;
    uint32_t                 pg_1_num;
    uint32_t                 colour;
    uint32_t                 colour_next;
    uint32_t                 cacheline_size;
    unsigned long            flags;
    jlos_memory_slab_ctor_t  ctor;
    jlos_memory_slab_dtor_t  dtor;
    jlos_memory_slab_page_t  *partial;
    jlos_memory_slab_page_t  *full;
    jlos_memory_slab_page_t  *empty;
    jlos_memory_slab_cpu_t   cpu[JLOS_MAX_CPUS];
    uint32_t                 total;
    uint32_t                 empty_ratio;
    uint32_t                 min_partial;
} jlos_memory_slab_cache_t;

typedef struct jlos_memory_slab_page {
    struct jlos_memory_slab_page *next;
    struct jlos_memory_slab_page *prev;
    jlos_memory_slab_cache_t     *cache;
    uint32_t                     inuse;
    void                         *freelist;
    uint32_t                     obj_count;
    uint8_t                      *obj_start;
} jlos_memory_slab_page_t;

jlos_memory_slab_cache_t *jlos_memory_slab_cache_create(const char *name, size_t size, size_t align,
    unsigned long flags, jlos_memory_slab_ctor_t ctor, jlos_memory_slab_dtor_t dtor);
void jlos_memory_slab_cache_destroy(jlos_memory_slab_cache_t *cache);
void *jlos_memory_slab_cache_alloc(jlos_memory_slab_cache_t *cache);
void jlos_memory_slab_cache_free(jlos_memory_slab_cache_t *cache, const void *obj);

void *jlos_kalloc(size_t size);
void jlos_kfree(const void *obj);

#endif