#include <kernel/memory_manager.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/printk.h>
#include <kernel/device.h>
#include <hal/spinlock.h>
#include <hal/hal.h>

jlos_memory_manager_t *jlos_active_memory_manager = NULL;
static jlos_memory_slab_cache_t *s_kalloc_caches[JLOS_MM_CLASS_COUNT];

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
    chunk->free_next = self->class_head[cls];
    chunk->free_prev = NULL;
    if (self->class_head[cls]) {
        self->class_head[cls]->free_prev = chunk;
    }
    self->class_head[cls] = chunk;
    self->size_bitmap |= (1U << cls);
}

static jlos_memory_chunk_t *mm_class_remove_head(jlos_memory_manager_t *self, int cls)
{
    jlos_memory_chunk_t *chunk = self->class_head[cls];
    if (chunk) {
        self->class_head[cls] = chunk->free_next;
        if (chunk->free_next) {
            chunk->free_next->free_prev = NULL;
        }
        if (!self->class_head[cls]) {
            self->size_bitmap &= ~(1U << cls);
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
        self->class_head[cls] = chunk->free_next;
    }
    if (chunk->free_next) {
        chunk->free_next->free_prev = chunk->free_prev;
    }
    if (!self->class_head[cls]) {
        self->size_bitmap &= ~(1U << cls);
    }
    chunk->free_next = NULL;
    chunk->free_prev = NULL;
}

static inline int mm_bitmap_find(jlos_memory_manager_t *self, uint32_t min_class)
{
    uint32_t mask = (min_class >= 32) ? 0xFFFFFFFF : (~0U << min_class);
    uint32_t masked = self->size_bitmap & mask;
    if (!masked) return -1;
    return __builtin_ctz(masked);
}

void jlos_memory_manager_init(jlos_memory_manager_t* self, uint8_t *start, size_t size)
{
    jlos_active_memory_manager = self;
    self->heap_start = start;
    self->heap_end = start + size;
    self->heap_current = self->heap_end;
    self->size_bitmap = 0;
    self->tail = NULL;

    size_t max_alloc = size - sizeof(jlos_memory_chunk_t);
    int max_cls = 0;
    size_t s = JLOS_MM_MIN_ALLOC;
    while (s < max_alloc && max_cls < JLOS_MM_CLASS_COUNT - 1) {
        s <<= 1;
        max_cls++;
    }
    self->max_class = max_cls;

    for (int i = 0; i < JLOS_MM_CLASS_COUNT; i++) {
        self->class_head[i] = NULL;
    }

    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);
    if (size < sizeof(jlos_memory_chunk_t)) {
        self->first = NULL;
    } else {
        self->first = (jlos_memory_chunk_t *)start;
        self->first->allocated = false;
        self->first->prev = NULL;
        self->first->next = NULL;
        self->first->free_next = NULL;
        self->first->free_prev = NULL;
        self->first->size = size - sizeof(jlos_memory_chunk_t);
        self->tail = self->first;
        int cls = mm_size_to_class(self->first->size, self->max_class);
        mm_class_add(self, cls, self->first);
    }
    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
}

void jlos_memory_manager_init_main(jlos_memory_manager_t *self)
{
    uint32_t initial_pages = KERNEL_MAIN_MEMORY_MIN_SIZE / JLOS_PAGE_FRAME_SIZE;
    uint8_t *heap_start = (uint8_t *)KERNEL_HEAP_VIRT_BASE;

    for (uint32_t i = 0; i < initial_pages; i++) {
        void *pf = jlos_page_frame_malloc();
        if (!pf) {
            printk("out of memory: initial heap page %u / %u\n", i, initial_pages);
            for (;;) {
                jlos_hal_halt();
            }
        }
        uint32_t va = KERNEL_HEAP_VIRT_BASE + i * JLOS_PAGE_FRAME_SIZE;
        uint32_t phys = (uint32_t)VIRT_TO_PHYS(pf);
        if (!jlos_paging_map(jlos_active_paging_context, va, phys, JLOS_PTE_KERNEL_RW)) {
            printk("out of memory: paging map faild at page %u\n", i);
            jlos_page_frame_free(pf);
            for (;;) {
                jlos_hal_halt();
            }
        }
        jlos_page_frame_set_owner_type(phys, (void *)self, JLOS_PAGE_FRAME_TYPE_KV_HEAP);
        jlos_page_frame_refcount_inc(phys);
    }
    jlos_memory_manager_init(self, heap_start, KERNEL_MAIN_MEMORY_MIN_SIZE);
    printk("init_main: first=%p size=%u heap=[%p,%p) current=%p\n",
        self->first, self->first ? (unsigned)self->first->size : 0,
        self->heap_start, self->heap_end, self->heap_current);
}

void jlos_memory_manager_destroy(jlos_memory_manager_t* self)
{
    if (jlos_active_memory_manager == self) {
        jlos_active_memory_manager = NULL;
    }
}

static jlos_memory_chunk_t *jlos_memory_manager_expand_heap(jlos_memory_manager_t *self, size_t size)
{
    if (!jlos_active_paging_context) {
        return NULL;
    }
    size_t pages_needed = JLOS_EXCEPT_CEIL(size, JLOS_PAGE_SIZE);
    uint8_t *new_heap_start = self->heap_current;

    void *vframe = jlos_page_frame_reserve_bulk(pages_needed);
    if (!vframe) {
        return NULL;
    }
    uint32_t phys = (uint32_t)VIRT_TO_PHYS(vframe);
    for (size_t i = 0; i < pages_needed; i++) {
        uint32_t virtual_addr = (uint32_t)(new_heap_start + i * JLOS_PAGE_SIZE);
        if (!jlos_paging_map(jlos_active_paging_context, virtual_addr, phys + i * JLOS_PAGE_SIZE, JLOS_PTE_KERNEL_RW)) {
            for (uint32_t j = 0; j < i; j++) {
                jlos_paging_unmap(jlos_active_paging_context, (uint32_t)(new_heap_start + j * JLOS_PAGE_SIZE));
                jlos_page_frame_clear_owner_type(phys + j * JLOS_PAGE_SIZE);
                jlos_page_frame_refcount_dec(phys + j * JLOS_PAGE_SIZE);
            }
            jlos_page_frame_free_bulk(phys, pages_needed);
            return NULL;
        }
        jlos_page_frame_set_owner_type(phys + i * JLOS_PAGE_SIZE, (void *)jlos_active_memory_manager, JLOS_PAGE_FRAME_TYPE_KV_HEAP);
        jlos_page_frame_refcount_inc(phys + i * JLOS_PAGE_SIZE);
    }
    size_t new_chunk_size = pages_needed * JLOS_PAGE_SIZE;
    jlos_memory_chunk_t *new_chunk = (jlos_memory_chunk_t *)new_heap_start;
    new_chunk->allocated = false;
    new_chunk->size = new_chunk_size - sizeof(jlos_memory_chunk_t);
    new_chunk->prev = NULL;
    new_chunk->next = NULL;
    new_chunk->free_next = NULL;
    new_chunk->free_prev = NULL;

    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);
    if (self->tail && !self->tail->allocated) {
        jlos_memory_chunk_t *tail = self->tail;
        int tcls = mm_size_to_class(tail->size, self->max_class);
        mm_class_remove_chunk(self, tcls, tail);
        tail->size += new_chunk->size + sizeof(jlos_memory_chunk_t);
        new_chunk = tail;
    } else {
        new_chunk->prev = self->tail;
        if (self->tail) {
            self->tail->next = new_chunk;
        } else {
            self->first = new_chunk;
        }
        self->tail = new_chunk;
    }
    int cls = mm_size_to_class(new_chunk->size, self->max_class);
    mm_class_add(self, cls, new_chunk);
    self->heap_current += new_chunk_size;
    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
    return new_chunk;
}

void *jlos_memory_manager_malloc(jlos_memory_manager_t* self, size_t size)
{
    if (size < JLOS_MM_MIN_ALLOC) {
        size = JLOS_MM_MIN_ALLOC;
    }

    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);
    int target_cls = mm_size_to_class(size, self->max_class);
    int found_cls = mm_bitmap_find(self, (uint32_t)target_cls);

    jlos_memory_chunk_t *result = NULL;

    if (found_cls >= 0) {
        result = mm_class_remove_head(self, found_cls);
    }

    if (!result) {
        jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
        result = jlos_memory_manager_expand_heap(self, size);
        if (!result) {
            return NULL;
        }
        fl = jlos_spin_lock_irqsave(&s_mm_lock);
        int cls = mm_size_to_class(result->size, self->max_class);
        mm_class_remove_chunk(self, cls, result);
    }

    if (result->size >= size + sizeof(jlos_memory_chunk_t) + JLOS_MM_MIN_ALLOC) {
        jlos_memory_chunk_t *temp = (jlos_memory_chunk_t *)((size_t)result + sizeof(jlos_memory_chunk_t) + size);
        temp->allocated = false;
        temp->size = result->size - size - sizeof(jlos_memory_chunk_t);
        temp->prev = result;
        temp->next = result->next;
        temp->free_next = NULL;
        temp->free_prev = NULL;
        if (temp->next) {
            temp->next->prev = temp;
        }
        result->size = size;
        result->next = temp;
        if (self->tail == result) {
            self->tail = temp;
        }
        int tcls = mm_size_to_class(temp->size, self->max_class);
        mm_class_add(self, tcls, temp);
    }
    result->allocated = true;
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
    if ((uint8_t *)ptr < self->heap_start || (uint8_t *)ptr >= self->heap_current) {
        jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
        return;
    }
    jlos_memory_chunk_t *chunk = (jlos_memory_chunk_t *)((size_t)ptr - sizeof(jlos_memory_chunk_t));
    chunk->allocated = false;

    if (chunk->prev && !chunk->prev->allocated) {
        jlos_memory_chunk_t *prev = chunk->prev;
        int pcls = mm_size_to_class(prev->size, self->max_class);
        mm_class_remove_chunk(self, pcls, prev);
        prev->size += chunk->size + sizeof(jlos_memory_chunk_t);
        prev->next = chunk->next;
        if (prev->next) {
            prev->next->prev = prev;
        }
        if (self->tail == chunk) {
            self->tail = prev;
        }
        chunk = prev;
    }
    if (chunk->next && !chunk->next->allocated) {
        jlos_memory_chunk_t *next = chunk->next;
        int ncls = mm_size_to_class(next->size, self->max_class);
        mm_class_remove_chunk(self, ncls, next);
        chunk->size += next->size + sizeof(jlos_memory_chunk_t);
        chunk->next = next->next;
        if (chunk->next) {
            chunk->next->prev = chunk;
        }
        if (self->tail == next) {
            self->tail = chunk;
        }
    }
    int cls = mm_size_to_class(chunk->size, self->max_class);
    mm_class_add(self, cls, chunk);
    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
    ptr = NULL;
}

void *jlos_kvalloc(size_t size)
{
    if (!jlos_active_memory_manager || !size) {
        return NULL;
    }
    if (size >= JLOS_PAGE_FRAME_SIZE) {
        size_t raw = size + sizeof(jlos_kv_contig_hdr_t);
        size_t npages = JLOS_EXCEPT_CEIL(raw, JLOS_PAGE_FRAME_SIZE);
        if (npages <= 0xFFFFFFFEU) {
            void *vframe = jlos_page_frame_reserve_bulk((uint32_t)npages);
            if (vframe) {
                jlos_kv_contig_hdr_t *hdr = (jlos_kv_contig_hdr_t *)vframe;
                hdr->magic = JLOS_KV_CONTIG_MAGIC;
                hdr->npages = (uint32_t)npages;
                
                for (size_t i = 0; i < npages; i++) {
                    uint32_t p = (uint32_t)VIRT_TO_PHYS(hdr) + i * JLOS_PAGE_FRAME_SIZE;
                    jlos_page_frame_set_owner_type(p, (void *)npages, JLOS_PAGE_FRAME_TYPE_KV_CONTIG);
                    jlos_page_frame_refcount_inc(p);
                }
                return (void *)(hdr + 1);
            }
        }
    }
    return jlos_memory_manager_malloc(jlos_active_memory_manager, size);
}

void jlos_kvfree(void *ptr)
{
    if (!ptr) {
        return;
    }
    uint32_t va = (uint32_t)ptr;
    if (va >= KERNEL_VIRTUAL_BASE && va < KERNEL_VIRTUAL_BASE + KERNEL_DIRECT_MAP_SIZE) {
        uint32_t phys = JLOS_ALIGN_DOWN(VIRT_TO_PHYS(va), JLOS_PAGE_FRAME_SIZE);
        jlos_page_frame_type_t t = jlos_page_frame_get_type(phys);
        void *owner = jlos_page_frame_get_owner(phys);
        if (t == JLOS_PAGE_FRAME_TYPE_SLAB_OBJ && owner) {
            jlos_memory_slab_cache_t *cache = (jlos_memory_slab_cache_t *)owner;
            jlos_memory_slab_page_t *sp = (jlos_memory_slab_page_t *)JLOS_ALIGN_DOWN(va, JLOS_PAGE_FRAME_SIZE);
            if (sp->inuse && sp->inuse <= cache->pg_1_num) {
                jlos_memory_slab_cache_free(cache, ptr);
            }
            return;
        }
        if (t == JLOS_PAGE_FRAME_TYPE_KV_CONTIG && owner) {
            jlos_kv_contig_hdr_t *hdr = (jlos_kv_contig_hdr_t *)ptr - 1;
            uint32_t np_owner = (uint32_t)owner;
            uint32_t phys_hdr = (uint32_t)VIRT_TO_PHYS(hdr);
            if (hdr->magic == JLOS_KV_CONTIG_MAGIC && hdr->npages && hdr->npages <= np_owner
            && !(phys_hdr & (JLOS_PAGE_FRAME_SIZE - 1))) {
                uint32_t np = hdr->npages;
                hdr->magic = 0;
                for (uint32_t i = 0; i < np; i++) {
                    uint32_t p = phys_hdr + i * JLOS_PAGE_FRAME_SIZE;
                    jlos_page_frame_clear_owner_type(p);
                    uint8_t g = 0;
                    while (jlos_page_frame_refcount_get(p) && g++ < JLOS_PAGE_FRAME_REFCOUNT_MAX) {
                        jlos_page_frame_refcount_dec(p);
                    }
                }
            }
            return;
        }
        return;
    }
    if (va >= KERNEL_HEAP_VIRT_BASE && jlos_active_memory_manager) {
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

void *jlos_memcpy(void *dst, const void *src, size_t size)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (size > 0 && ((size_t)d & 3)) {
        *d++ = *s++;
        size--;
    }
    uint32_t *d32 = (uint32_t *)d;
    const uint32_t *s32 = (const uint32_t *)s;
    while (size >= 4) {
        *d32++ = *s32++;
        size -= 4;
    }
    d = (uint8_t *)d32;
    s = (const uint8_t *)s32;
    while (size > 0) {
        *d++ = *s++;
        size--;
    }
    return dst;
}

size_t jlos_strlcpy(char *dst, const char *src, size_t dsize)
{
    const char *osrc = src;
    size_t nleft = dsize;
    if (nleft) while (--nleft) if (!(*dst++ = *src++)) break;
    if (!nleft) {if (dsize) *dst = 0; while (*src++);}
    return (size_t)(src - osrc - 1);
}

void *memcpy(void *dst, const void *src, size_t size) __attribute__((weak, alias("jlos_memcpy")));

void jlos_kvalloc_stats(jlos_memory_manager_t *self)
{
    if (!self) return;
    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);

    uint32_t total = 0, free = 0, alloc = 0;
    uint32_t free_bytes = 0, alloc_bytes = 0;

    for (jlos_memory_chunk_t *c = self->first; c; c = c->next) {
        total++;
        if (c->allocated) {
            alloc++;
            alloc_bytes += c->size;
        } else {
            free++;
            free_bytes += c->size;
        }
    }

    printk("Memory Manager Stats:\n");
    printk("  Total chunks: %u\n", total);
    printk("  Allocated: %u chunks, %u bytes\n", alloc, alloc_bytes);
    printk("  Free: %u chunks, %u bytes\n", free, free_bytes);
    printk("  Free bitmap: 0x%x\n", self->size_bitmap);

    for (int i = 0; i <= self->max_class; i++) {
        uint32_t count = 0;
        for (jlos_memory_chunk_t *c = self->class_head[i]; c; c = c->free_next) {
            count++;
        }
        if (count > 0) {
            uint32_t sz = JLOS_MM_MIN_ALLOC << i;
            printk("  Class %2u (%7uB): %u free\n", i, sz, count);
        }
    }

    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
}

static void slab_list_insert(jlos_memory_slab_page_t **head, jlos_memory_slab_page_t *sp)
{
    sp->next = *head;
    sp->prev = NULL;
    if (*head) {
        (*head)->prev = sp;
    }
    *head = sp;
}

static void slab_list_remove(jlos_memory_slab_page_t **head, jlos_memory_slab_page_t *sp)
{
    if (sp->prev) {
        sp->prev->next = sp->next;
    } else {
        *head = sp->next;
    }
    if (sp->next) {
        sp->next->prev = sp->prev;
    }
    sp->next = sp->prev = NULL;
}

static uint32_t slab_count_list(jlos_memory_slab_page_t *head)
{
    uint32_t n = 0;
    while (head) {
        head = head->next;
        n++;
    }
    return n;
}

static uint32_t slab_page_frame_1_num(size_t obj_size, size_t align)
{
    if (!obj_size) {
        return 0;
    }
    size_t aligned_start = (sizeof(jlos_memory_slab_page_t) + align - 1) & ~(align - 1);
    if (aligned_start >= JLOS_PAGE_FRAME_SIZE) {
        return 0;
    }
    return (JLOS_PAGE_FRAME_SIZE - aligned_start) / obj_size;
}

static jlos_memory_slab_page_t *slab_page_alloc(jlos_memory_slab_cache_t *cache)
{
    void *page = jlos_page_frame_malloc();
    if (!page) {
        return NULL;
    }
    jlos_page_frame_set_owner_type((uint32_t)VIRT_TO_PHYS(page), (void *)cache, JLOS_PAGE_FRAME_TYPE_SLAB_OBJ);
    jlos_memory_slab_page_t *sp = (jlos_memory_slab_page_t *)page;
    sp->cache = cache;
    sp->inuse = 0;

    size_t offset = (size_t)cache->colour_next * cache->cacheline_size;
    cache->colour_next = (cache->colour_next + 1) % (cache->colour + 1);

    uint32_t start = JLOS_ALIGN_UP((uint32_t)page + sizeof(jlos_memory_slab_page_t) + offset, cache->align);
    uint32_t end = (uint32_t)page + JLOS_PAGE_FRAME_SIZE;
    void *fhead = NULL;
    for (uint32_t p = start; p + cache->obj_size <= end; p += cache->obj_size) {
        if (cache->ctor) {
            cache->ctor((void *)p);
        }
        *(void **)p = fhead;
        fhead = (void *)p;
    }
    sp->freelist = fhead;
    cache->total++;
    return sp;
}

static void slab_page_free(jlos_memory_slab_cache_t *cache, jlos_memory_slab_page_t *sp)
{
    if (cache->dtor) {
        size_t offset = 0;
        uint32_t start = JLOS_ALIGN_UP((uint32_t)sp + sizeof(jlos_memory_slab_page_t) + offset, cache->align);
        for (uint32_t i = 0; i < cache->pg_1_num; i++) {
            cache->dtor((void *)(start + i *cache->obj_size));
        }
    }
    jlos_page_frame_clear_owner_type((uint32_t)VIRT_TO_PHYS(sp));
    cache->total--;
    jlos_page_frame_free((void *)sp);
}

jlos_memory_slab_cache_t *jlos_memory_slab_cache_create(const char *name, size_t size, size_t align,
    unsigned long flags, jlos_memory_slab_ctor_t ctor, jlos_memory_slab_dtor_t dtor)
{
    if (!size) {
        return NULL;
    }
    if (align < sizeof(void *)) {
        align = sizeof(void *);
    }
    size_t obj = ((size + align - 1) / align) *align;
    uint32_t num = slab_page_frame_1_num(obj, align);
    if (!num) {
        return NULL;
    }
    uint32_t aligned_start = JLOS_ALIGN_UP(sizeof(jlos_memory_slab_page_t), align);
    size_t leftover = JLOS_PAGE_FRAME_SIZE - aligned_start - (size_t)num * obj;
    uint32_t colour = (uint32_t)(leftover / JLOS_CACHELINE_SIZE);
    jlos_memory_slab_cache_t *c = (jlos_memory_slab_cache_t *)jlos_kvalloc(sizeof(*c));
    if (!c) {
        return NULL;
    }
    (void)jlos_strlcpy(c->name, name ? name : "", sizeof(c->name));
    c->size = size;
    c->obj_size = obj;
    c->align = align;
    c->pg_1_num = num;
    c->colour = colour;
    c->colour_next = 0;
    c->cacheline_size = JLOS_CACHELINE_SIZE;
    c->flags = flags;
    c->ctor = ctor;
    c->dtor = dtor;
    for (uint32_t i = 0; i < JLOS_MAX_CPUS; i++) {
        c->cpu[i].partial = NULL;
        c->cpu[i].partial_count = 0;
    }
    c->partial = c->full = c->empty = NULL;
    c->total = 0;
    c->empty_ratio = 50;
    c->min_partial = 2;
    return c;
}

void *jlos_memory_slab_cache_alloc(jlos_memory_slab_cache_t *cache)
{
    if (!cache) {
        return NULL;
    }
    uint32_t cpu = jlos_hal_get_cpu_id();
    jlos_memory_slab_cpu_t *cs = &cache->cpu[cpu];
    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);
    jlos_memory_slab_page_t *sp = cs->partial;
    if (!sp) {
        if (cache->partial) {
            sp = cache->partial;
            slab_list_remove(&cache->partial, sp);
        } else {
            sp = slab_page_alloc(cache);
            if (!sp) {
                jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
                return NULL;
            }
        }
        slab_list_insert(&cs->partial, sp);
        cs->partial_count++;
    }
    void *obj = sp->freelist;
    sp->freelist = *(void **)obj;
    if (++sp->inuse == cache->pg_1_num) {
        slab_list_remove(&cs->partial, sp);
        cs->partial_count--;
        slab_list_insert(&cache->full, sp);
    }
    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
    return obj;
}

void jlos_memory_slab_cache_free(jlos_memory_slab_cache_t *cache, const void *obj)
{
    if (!cache || !obj) {
        return;
    }
    uint32_t cpu = jlos_hal_get_cpu_id();
    jlos_memory_slab_cpu_t *cs = &cache->cpu[cpu];
    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);
    uint32_t page_base = JLOS_ALIGN_DOWN(obj, JLOS_PAGE_FRAME_SIZE);
    jlos_memory_slab_page_t *sp = (jlos_memory_slab_page_t *)page_base;
    
    bool was_full = (sp->inuse == cache->pg_1_num);
    *(void **)obj = sp->freelist;
    sp->freelist = (void *)obj;
    sp->inuse--;
    if (was_full) {
        slab_list_remove(&cache->full, sp);
        slab_list_insert(&cs->partial, sp);
        cs->partial_count++;
    }
    if (!sp->inuse) {
        slab_list_remove(&cs->partial, sp);
        cs->partial_count--;
        slab_list_insert(&cache->empty, sp);
    }
    while (cache->total > cache->min_partial && cache->empty
    && slab_count_list(cache->empty) * 100UL > cache->empty_ratio * cache->total) {
        jlos_memory_slab_page_t *victim = cache->empty;
        slab_list_remove(&cache->empty, victim);
        slab_page_free(cache, victim);
    }
    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
}

void jlos_memory_slab_cache_destroy(jlos_memory_slab_cache_t *cache)
{
    if (!cache) {
        return;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&s_mm_lock);
    jlos_memory_slab_page_t *p, *next;
    for (uint32_t i = 0; i < JLOS_MAX_CPUS; i++) {
        for (p =cache->cpu[i].partial; p; p = next) {
            next = p->next;
            slab_page_free(cache, p);
        }
    }
    for (p = cache->partial; p; p = next) {
        next = p->next;
        slab_page_free(cache, p);
    }
    for (p = cache->full; p; p = next) {
        next = p->next;
        slab_page_free(cache, p);
    }
    for (p = cache->empty; p; p = next) {
        next = p->next;
        slab_page_free(cache, p);
    }
    jlos_spin_unlock_irqrestore(&s_mm_lock, fl);
    jlos_kvfree(cache);
}

static inline size_t jlos_kalloc_max_small(void)
{
    return JLOS_PAGE_FRAME_SIZE - sizeof(jlos_memory_slab_page_t);
}

static void kalloc_make_name(char *buf, size_t sz)
{
    const char *prefix = "jlos_k";
    int i = 0;
    while (prefix[i]) { buf[i] = prefix[i]; i++; }
    char tmp[11];
    int n = 0;
    if (sz == 0) { tmp[n++] = '0'; }
    else { while (sz > 0) { tmp[n++] = (char)('0' + (sz % 10)); sz /= 10; } }
    while (n > 0) { buf[i++] = tmp[--n]; }
    buf[i] = '\0';
}

static void kalloc_caches_ensure(void)
{
    if (s_kalloc_caches[0]) {
        return;
    }
    for (int i = 0; i < JLOS_MM_CLASS_COUNT; i++) {
        size_t s = (size_t)JLOS_MM_MIN_ALLOC << i;
        if (s > jlos_kalloc_max_small()) {
            s_kalloc_caches[i] = NULL;
            continue;
        }
        char name[32];
        kalloc_make_name(name, s);
        s_kalloc_caches[i] = jlos_memory_slab_cache_create(name, s, sizeof(void *), 0, NULL, NULL);
    }
}

static inline int kalloc_size_to_bucket(size_t n)
{
    if (n <= JLOS_MM_MIN_ALLOC) {
        return 0;
    }
    n = JLOS_EXCEPT_CEIL(n, JLOS_MM_MIN_ALLOC);
    int r = sizeof(unsigned) * 8 - __builtin_clz((unsigned)(n - 1));
    if (r >= JLOS_MM_CLASS_COUNT) {
        r = JLOS_MM_CLASS_COUNT - 1;
    }
    return r;
}

void *jlos_kalloc(size_t size)
{
    if (!size) {
        return NULL;
    }
    int bucket = -1;
    jlos_memory_slab_cache_t *c = NULL;
    void *ret = NULL;
    if (size <= jlos_kalloc_max_small()) {
        kalloc_caches_ensure();
        bucket = kalloc_size_to_bucket(size);
        c = s_kalloc_caches[bucket];
        if (c) {
            void *p = jlos_memory_slab_cache_alloc(c);
            if (p) {
                ret = p;
            }
        }
    }
    if (!ret) {
        ret = jlos_kvalloc(size);
    }
    return ret;
}

void jlos_kfree(const void *obj)
{
    jlos_kvfree((void *)obj);
}
