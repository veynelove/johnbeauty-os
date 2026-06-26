#include <kernel/memory_manager.h>

jlos_memory_manager_t *jlos_active_memory_manager = NULL;

void jlos_memory_manager_init(jlos_memory_manager_t* self, uint8_t *start, size_t m_size)
{
    jlos_active_memory_manager = self;
    if (m_size < sizeof(jlos_memory_chunk_t)) {
        self->first = NULL;
    } else {
        self->first = (jlos_memory_chunk_t *)start;
        self->first->m_allocated = false;
        self->first->prev = NULL;
        self->first->next = NULL;
        self->first->m_size = m_size - sizeof(jlos_memory_chunk_t);
    }
}

void jlos_memory_manager_destroy(jlos_memory_manager_t* self)
{
    if (jlos_active_memory_manager == self) {
        jlos_active_memory_manager = NULL;
    }
}

void *jlos_memory_manager_malloc(jlos_memory_manager_t* self, size_t m_size)
{
    jlos_memory_chunk_t *result = NULL;
    for (jlos_memory_chunk_t *chunk = self->first; chunk != NULL && result == NULL; chunk = chunk->next) {
        if (chunk->m_size > m_size && !chunk->m_allocated) {
            result = chunk;
        }
    }
    if (!result) {
        return NULL;
    }
    if (result->m_size >= m_size + sizeof(jlos_memory_chunk_t) + 1) {
        jlos_memory_chunk_t *temp = (jlos_memory_chunk_t *)((size_t)result + sizeof(jlos_memory_chunk_t) + m_size);
        temp->m_allocated = false;
        temp->m_size = result->m_size - m_size - sizeof(jlos_memory_chunk_t);
        temp->prev = result;
        temp->next = result->next;
        if (temp->next) {
            temp->next->prev = temp;
        }
        result->m_size = m_size;
        result->next = temp;
    }
    result->m_allocated = true;
    return (void *)(((size_t)result) + sizeof(jlos_memory_chunk_t));
}

void jlos_memory_manager_free(jlos_memory_manager_t* self, void *ptr)
{
    jlos_memory_chunk_t *chunk = (jlos_memory_chunk_t *)((size_t)ptr - sizeof(jlos_memory_chunk_t));
    chunk->m_allocated = false;
    if (chunk->prev && !chunk->prev->m_allocated) {
        chunk->prev->next = chunk->next;
        chunk->prev->m_size += chunk->m_size + sizeof(jlos_memory_chunk_t);
        if (chunk->next) {
            chunk->next->prev = chunk->prev;
        }
        chunk = chunk->prev;
    }
    if (chunk->next && !chunk->next->m_allocated) {
        chunk->m_size += chunk->next->m_size + sizeof(jlos_memory_chunk_t);
        chunk->next = chunk->next->next;
        if (chunk->next) { 
            chunk->next->prev = chunk;
        }
    }
}

void *jlos_malloc(size_t m_size)
{
    if (!jlos_active_memory_manager) {
        return NULL;
    }
    return jlos_memory_manager_malloc(jlos_active_memory_manager, m_size);
}

void jlos_free(void *ptr)
{
    if (jlos_active_memory_manager) {
        jlos_memory_manager_free(jlos_active_memory_manager, ptr);
    }
}

void jlos_memset(void *ptr, uint8_t value, size_t size) {
    uint8_t *p = (uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = value;
    }
}