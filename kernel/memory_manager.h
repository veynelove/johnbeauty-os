#ifndef __JLOS_KERNEL_MEMORY_MANAGER_H
#define __JLOS_KERNEL_MEMORY_MANAGER_H

#include <common/types.h>

#define KERNEL_MEMORY_ADDR_START        0x100000
#define KERNEL_MEMORY_ADDR_END          0x10000000
#define KERNEL_MEMORY_ADDR_SIZE (KERNEL_MEMORY_ADDR_END - KERNEL_MEMORY_ADDR_START)
#define KERNEL_LOW_MEMORY_ADDR_START    0x50000
#define KERNEL_LOW_MEMORY_SIZE          0x50000 
#define KERNEL_MAIN_MEMORY_SIZE         32 * 1024 * 1024

#define JLOS_MM_MIN_ALLOC      16
#define JLOS_MM_CLASS_COUNT    32

typedef struct jlos_memory_chunk jlos_memory_chunk_t;

struct jlos_memory_chunk {
    jlos_memory_chunk_t *next;
    jlos_memory_chunk_t *prev;
    jlos_memory_chunk_t *free_next;
    jlos_memory_chunk_t *free_prev;
    bool allocated;
    size_t size;
};

typedef struct {
    jlos_memory_chunk_t *first;
    jlos_memory_chunk_t *tail;
    uint8_t *heap_start;
    uint8_t *heap_end;
    uint8_t *heap_current;
    uint32_t size_bitmap;
    int max_class;
    jlos_memory_chunk_t *class_head[JLOS_MM_CLASS_COUNT];
} jlos_memory_manager_t;

extern jlos_memory_manager_t *jlos_active_memory_manager;

void jlos_memory_manager_init(jlos_memory_manager_t* self, uint8_t *start, size_t size);
void jlos_memory_manager_destroy(jlos_memory_manager_t* self);

void *jlos_memory_manager_malloc(jlos_memory_manager_t* self, size_t size);
void jlos_memory_manager_free(jlos_memory_manager_t* self, void *ptr);

void *jlos_malloc(size_t size);
void jlos_free(void *ptr);
void jlos_memset(void *ptr, uint8_t value, size_t size);
void *jlos_memcpy(void *dst, const void *src, size_t size);

void jlos_malloc_stats(jlos_memory_manager_t *self);

#endif