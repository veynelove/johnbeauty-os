#ifndef __JLOS_KERNEL_MEMORY_MANAGER_H
#define __JLOS_KERNEL_MEMORY_MANAGER_H

#include <common/types.h>

typedef struct jlos_memory_chunk jlos_memory_chunk_t;

struct jlos_memory_chunk {
    jlos_memory_chunk_t *next;
    jlos_memory_chunk_t *prev;
    bool m_allocated;
    size_t m_size;
};

typedef struct {
    jlos_memory_chunk_t *first;
} jlos_memory_manager_t;

extern jlos_memory_manager_t *jlos_active_memory_manager;

void jlos_memory_manager_init(jlos_memory_manager_t* self, uint8_t *start, size_t m_size);
void jlos_memory_manager_destroy(jlos_memory_manager_t* self);

void *jlos_memory_manager_malloc(jlos_memory_manager_t* self, size_t m_size);
void jlos_memory_manager_free(jlos_memory_manager_t* self, void *ptr);

void *jlos_malloc(size_t m_size);
void jlos_free(void *ptr);
void jlos_memset(void *ptr, uint8_t value, size_t size);

#endif