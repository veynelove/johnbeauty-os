#include <kernel/memory_manager.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/printk.h>
#include <hal/spinlock.h>

jlos_memory_manager_t *jlos_active_memory_manager = NULL;

static jlos_spinlock_t s_mm_lock = JLOS_SPINLOCK_INIT;

static inline int mm_size_to_class(size_t size, int max_class)
{
    int cls = 0;
    size_t s = JLOS_MM_MIN_ALLOC;
    while (s < size && cls < max_class) {
        s <<= 1;
        cls++;
    }
    return cls;
}

static void mm_class_add(jlos_memory_manager_t *self, int cls, jlos_memory_chunk_t *chunk)
{
    chunk->free_next = self->m_class_head[cls];
    chunk->free_prev = NULL;
    if (self->m_class_head[cls]) {
        self->m_class_head[cls]->free_prev = chunk;
    }
    self->m_class_head[cls] = chunk;
    self->m_size_bitmap |= (1U << cls);
}

static jlos_memory_chunk_t *mm_class_remove_head(jlos_memory_manager_t *self, int cls)
{
    jlos_memory_chunk_t *chunk = self->m_class_head[cls];
    if (chunk) {
        self->m_class_head[cls] = chunk->free_next;
        if (chunk->free_next) {
            chunk->free_next->free_prev = NULL;
        }
        if (!self->m_class_head[cls]) {
            self->m_size_bitmap &= ~(1U << cls);
        }
        chunk->free_next = NULL;
        chunk->free_prev = NULL;
    }
    return chunk;
}

static void mm_class_remove_chunk(jlos_memory_manager_t *self, int cls, jlos_memory_chunk_t *chunk)
{
    if (chunk->free_prev) {
        chunk->free_prev->free_next = chunk->free_next;
    } else {
        self->m_class_head[cls] = chunk->free_next;
    }
    if (chunk->free_next) {
        chunk->free_next->free_prev = chunk->free_prev;
    }
    if (!self->m_class_head[cls]) {
        self->m_size_bitmap &= ~(1U << cls);
    }
    chunk->free_next = NULL;
    chunk->free_prev = NULL;
}

static inline int mm_bitmap_find(jlos_memory_manager_t *self, uint32_t min_class)
{
    uint32_t mask = (min_class >= 32) ? 0xFFFFFFFF : (~0U << min_class);
    uint32_t masked = self->m_size_bitmap & mask;
    if (!masked) return -1;
    return __builtin_ctz(masked);
}

void jlos_memory_manager_init(jlos_memory_manager_t* self, uint8_t *start, size_t m_size)
{
    jlos_active_memory_manager = self;
    self->m_heap_start = start;
    self->m_heap_end = start + m_size;
    self->m_heap_current = self->m_heap_end;
    self->m_size_bitmap = 0;
    self->m_tail = NULL;

    size_t max_alloc = m_size - sizeof(jlos_memory_chunk_t);
    int max_cls = 0;
    size_t s = JLOS_MM_MIN_ALLOC;
    while (s < max_alloc && max_cls < JLOS_MM_CLASS_COUNT - 1) {
        s <<= 1;
        max_cls++;
    }
    self->m_max_class = max_cls;

    for (int i = 0; i < JLOS_MM_CLASS_COUNT; i++) {
        self->m_class_head[i] = NULL;
    }

    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);
    if (m_size < sizeof(jlos_memory_chunk_t)) {
        self->first = NULL;
    } else {
        self->first = (jlos_memory_chunk_t *)start;
        self->first->m_allocated = false;
        self->first->prev = NULL;
        self->first->next = NULL;
        self->first->free_next = NULL;
        self->first->free_prev = NULL;
        self->first->m_size = m_size - sizeof(jlos_memory_chunk_t);
        self->m_tail = self->first;
        int cls = mm_size_to_class(self->first->m_size, self->m_max_class);
        mm_class_add(self, cls, self->first);
    }
    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
}

void jlos_memory_manager_destroy(jlos_memory_manager_t* self)
{
    if (jlos_active_memory_manager == self) {
        jlos_active_memory_manager = NULL;
    }
}

static jlos_memory_chunk_t *jlos_memory_manager_expand_heap(jlos_memory_manager_t *self, size_t m_size)
{
    if (!jlos_active_paging_context) {
        return NULL;
    }
    size_t pages_needed = JLOS_EXCEPT_CEIL(m_size, JLOS_PAGE_SIZE);
    uint8_t *new_heap_start = self->m_heap_current;
    uint32_t pages_done = 0;

    for (size_t i = 0; i < pages_needed; i++) {
        void *physical_frame = jlos_page_frame_malloc();
        if (!physical_frame) {
            for (int j = 0; j < pages_done; j++) {
                uint32_t virtual_addr = (uint32_t)(new_heap_start + j * JLOS_PAGE_SIZE);
                jlos_paging_unmap(jlos_active_paging_context, virtual_addr);
            }
            return NULL;
        }
        uint32_t virtual_addr = (uint32_t)(new_heap_start + i * JLOS_PAGE_SIZE);
        if (!jlos_paging_map(jlos_active_paging_context, virtual_addr, (uint32_t)physical_frame, JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE)) {
            jlos_page_frame_free(physical_frame);
            for (int j = 0; j < pages_done; j++) {
                uint32_t vi_addr = (uint32_t)(new_heap_start + j * JLOS_PAGE_SIZE);
                jlos_paging_unmap(jlos_active_paging_context, vi_addr);
            }
            return NULL;
        }
        pages_done++;
    }
    size_t new_chunk_size = pages_needed * JLOS_PAGE_SIZE;
    jlos_memory_chunk_t *new_chunk = (jlos_memory_chunk_t *)new_heap_start;
    new_chunk->m_allocated = false;
    new_chunk->m_size = new_chunk_size - sizeof(jlos_memory_chunk_t);
    new_chunk->prev = NULL;
    new_chunk->next = NULL;
    new_chunk->free_next = NULL;
    new_chunk->free_prev = NULL;

    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);
    if (self->m_tail) {
        self->m_tail->next = new_chunk;
        new_chunk->prev = self->m_tail;
    } else {
        self->first = new_chunk;
    }
    self->m_tail = new_chunk;
    int cls = mm_size_to_class(new_chunk->m_size, self->m_max_class);
    mm_class_add(self, cls, new_chunk);
    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
    self->m_heap_current += new_chunk_size;
    return new_chunk;
}

void *jlos_memory_manager_malloc(jlos_memory_manager_t* self, size_t m_size)
{
    if (m_size < JLOS_MM_MIN_ALLOC) {
        m_size = JLOS_MM_MIN_ALLOC;
    }

    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);
    int target_cls = mm_size_to_class(m_size, self->m_max_class);
    int found_cls = mm_bitmap_find(self, (uint32_t)target_cls);

    jlos_memory_chunk_t *result = NULL;

    if (found_cls >= 0) {
        result = mm_class_remove_head(self, found_cls);
    }

    if (!result) {
        jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
        result = jlos_memory_manager_expand_heap(self, m_size);
        if (!result) {
            return NULL;
        }
        fl = jlos_spin_lock_irqsave(&s_mm_lock);
        int cls = mm_size_to_class(result->m_size, self->m_max_class);
        mm_class_remove_chunk(self, cls, result);
    }

    if (result->m_size >= m_size + sizeof(jlos_memory_chunk_t) + JLOS_MM_MIN_ALLOC) {
        jlos_memory_chunk_t *temp = (jlos_memory_chunk_t *)((size_t)result + sizeof(jlos_memory_chunk_t) + m_size);
        temp->m_allocated = false;
        temp->m_size = result->m_size - m_size - sizeof(jlos_memory_chunk_t);
        temp->prev = result;
        temp->next = result->next;
        temp->free_next = NULL;
        temp->free_prev = NULL;
        if (temp->next) {
            temp->next->prev = temp;
        }
        result->m_size = m_size;
        result->next = temp;
        if (self->m_tail == result) {
            self->m_tail = temp;
        }
        int tcls = mm_size_to_class(temp->m_size, self->m_max_class);
        mm_class_add(self, tcls, temp);
    }
    result->m_allocated = true;
    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
    return (void *)(((size_t)result) + sizeof(jlos_memory_chunk_t));
}

void jlos_memory_manager_free(jlos_memory_manager_t* self, void *ptr)
{
    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);
    if (!ptr || !self) {
        jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
        return;
    }
    if ((uint8_t *)ptr < self->m_heap_start || (uint8_t *)ptr >= self->m_heap_current) {
        jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
        return;
    }
    jlos_memory_chunk_t *chunk = (jlos_memory_chunk_t *)((size_t)ptr - sizeof(jlos_memory_chunk_t));
    chunk->m_allocated = false;

    if (chunk->prev && !chunk->prev->m_allocated) {
        jlos_memory_chunk_t *prev = chunk->prev;
        int pcls = mm_size_to_class(prev->m_size, self->m_max_class);
        mm_class_remove_chunk(self, pcls, prev);
        chunk->m_size += prev->m_size + sizeof(jlos_memory_chunk_t);
        chunk->prev = prev->prev;
        if (chunk->prev) {
            chunk->prev->next = chunk;
        } else {
            self->first = chunk;
        }
        if (self->m_tail == chunk) {
            self->m_tail = prev;
        }
    }
    if (chunk->next && !chunk->next->m_allocated) {
        jlos_memory_chunk_t *next = chunk->next;
        int ncls = mm_size_to_class(next->m_size, self->m_max_class);
        mm_class_remove_chunk(self, ncls, next);
        chunk->m_size += next->m_size + sizeof(jlos_memory_chunk_t);
        chunk->next = next->next;
        if (chunk->next) {
            chunk->next->prev = chunk;
        }
    }
    int cls = mm_size_to_class(chunk->m_size, self->m_max_class);
    mm_class_add(self, cls, chunk);
    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
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
    while (size > 0 && ((size_t)p & 3)) {
        *p++ = value;
        size--;
    }
    uint32_t val32 = value | (value << 8) | (value << 16) | (value << 24);
    uint32_t *p32 = (uint32_t *)p;
    while (size >= 4) {
        *p32++ = val32;
        size -= 4;
    }
    p = (uint8_t *)p32;
    while (size > 0) {
        *p++ = value;
        size--;
    }
}

void jlos_malloc_stats(jlos_memory_manager_t *self)
{
    if (!self) return;
    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);

    uint32_t total = 0, free = 0, alloc = 0;
    uint32_t free_bytes = 0, alloc_bytes = 0;

    for (jlos_memory_chunk_t *c = self->first; c; c = c->next) {
        total++;
        if (c->m_allocated) {
            alloc++;
            alloc_bytes += c->m_size;
        } else {
            free++;
            free_bytes += c->m_size;
        }
    }

    printk("Memory Manager Stats:\n");
    printk("  Total chunks: %u\n", total);
    printk("  Allocated: %u chunks, %u bytes\n", alloc, alloc_bytes);
    printk("  Free: %u chunks, %u bytes\n", free, free_bytes);
    printk("  Free bitmap: 0x%x\n", self->m_size_bitmap);

    for (int i = 0; i <= self->m_max_class; i++) {
        uint32_t count = 0;
        for (jlos_memory_chunk_t *c = self->m_class_head[i]; c; c = c->free_next) {
            count++;
        }
        if (count > 0) {
            uint32_t sz = JLOS_MM_MIN_ALLOC << i;
            printk("  Class %2u (%7uB): %u free\n", i, sz, count);
        }
    }

    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
}