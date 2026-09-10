#include <kernel/page_frame_allocator.h>
#include <kernel/memory_manager.h>
#include <kernel/device.h>
#include <kernel/paging.h>
#include <kernel/printk.h>
#include <hal/spinlock.h>
#include <hal/hal.h>

#define JLOS_KERNEL_LOG_SUBSYS "pfa"

extern uint32_t             _boot_end_phys;
extern uint32_t             _kernel_end_phys;

static uint32_t             s_boot_heap_ptr = 0;

static uint32_t             s_total_frames = 0;
static uint32_t             s_start_addr = 0;

static jlos_atomic_t        s_free_frames = JLOS_ATOMIC_INIT(0);

static uint32_t             s_free_order_bitmap = 0;

static jlos_page_t          *s_pages = NULL;
static jlos_list_head_t     s_free_area[JLOS_PFA_MAX_ORDER + 1];

static jlos_spinlock_t      s_buddy_lock = JLOS_SPINLOCK_INIT;
static jlos_spinlock_t      s_owner_lock = JLOS_SPINLOCK_INIT;

void jlos_pfa_boot_alloc_init(uint32_t start_phys)
{
    s_boot_heap_ptr = JLOS_PAGE_ALIGN_UP(start_phys);
}

void *jlos_pfa_boot_alloc(uint32_t size)
{
    uint32_t addr = s_boot_heap_ptr;
    s_boot_heap_ptr += JLOS_PAGE_ALIGN_UP(size);
    return (void *)PHYS_TO_VIRT(addr);
}

void *jlos_pfa_boot_alloc_page(void)
{
    return jlos_pfa_boot_alloc(JLOS_PAGE_FRAME_SIZE);
}

uint32_t jlos_pfa_boot_alloc_get_end(void)
{
    return s_boot_heap_ptr;
}

static inline uint32_t order_to_frames(uint32_t order)
{
    return 1U << order;
}

static inline uint32_t frames_to_order(uint32_t n)
{
    if (n <= 1) {
        return 0;
    }
    uint32_t order = 0;
    uint32_t v = n - 1;
    while (v) {
        order++;
        v >>= 1;
    }
    return order;
}

static inline jlos_page_t *pfa_page(uint32_t frame)
{
    return &s_pages[frame];
}

static inline bool buddy_mergeable(uint32_t frame, uint32_t order)
{
    uint32_t block = order_to_frames(order);
    uint32_t buddy = frame ^ block;
    if (buddy >= s_total_frames) {
        return false;
    }
    uint32_t merged_start = (buddy < frame) ? buddy : frame;
    if (merged_start + block * 2 > s_total_frames) {
        return false;
    }
    jlos_page_t *page = pfa_page(buddy);
    return page->order == order && !(page->flags & JLOS_PFA_FLAG_OCCUPIED);
}

static void buddy_insert(uint32_t frame, uint32_t order)
{
    uint32_t block = order_to_frames(order);
    if (frame >= s_total_frames || frame + block > s_total_frames) {
        printk_emerg("[OOB] insert frame=%u order=%u block=%u total=%u (cross phys boundary)\n",
               frame, order, block, s_total_frames);
        goto halt;
    }
    if (frame & (block - 1)) {
        printk_emerg("[ALIGN] insert frame=%u order=%u (frame not aligned to order)\n", frame, order);
        goto halt;
    }
    jlos_page_t *pg = pfa_page(frame);
    if (frame < s_total_frames) {
        bool is_reserved = (pg->flags & JLOS_PFA_FLAG_OCCUPIED);
        uint8_t cur_order = pg->order;
        if (is_reserved) {
            printk_emerg("[ASSERT] insert frame=%u order=%u FAIL: flags=0x%x (reserved)!\n", frame, order, pg->flags);
            goto halt;
        }
        if (cur_order != JLOS_PFA_BUDDY_ORDER_INVALID && cur_order != order) {
            printk_emerg("[ASSERT] insert frame=%u order=%u FAIL: buddy_order=%u (already in free list)!\n",
                   frame, order, cur_order);
            goto halt;
        }
    }
    if (jlos_list_empty(&s_free_area[order])) {
        s_free_order_bitmap |= order_to_frames(order);
    }
    jlos_list_add(&pg->u.free_list, &s_free_area[order]);
    pg->order = (uint8_t)order;
    return;
halt:
    for (;;) {
        jlos_hal_halt();
    }
}

static void buddy_remove(uint32_t frame, uint32_t order)
{
    uint32_t block = order_to_frames(order);
    if (frame >= s_total_frames || frame + block > s_total_frames) {
        printk_emerg("[PANIC] frame=%u order=%u block=%u s_total=%u (frame/block OOB)\n",
               frame, order, block, s_total_frames);
        goto halt;
    }
    jlos_page_t *pg = pfa_page(frame);
    uint32_t bad = 0;
    if (pg->order != (uint8_t)order) bad |= 1;
    if (pg->order == JLOS_PFA_BUDDY_ORDER_INVALID) bad |= 2;
    if (jlos_list_empty(&pg->u.free_list)) bad |= 4;
    if (bad) {
        printk_emerg("[PANIC] remove frame=%u order=%u bad=%u next=%p prev=%p\n",
               frame, order, bad, pg->u.free_list.next, pg->u.free_list.prev);
        goto halt;
    }
    jlos_list_del(&pg->u.free_list);
    if (jlos_list_empty(&s_free_area[order])) {
        s_free_order_bitmap &= ~order_to_frames(order);
    }
    pg->order = JLOS_PFA_BUDDY_ORDER_INVALID;
    return;
halt:
    for (;;) {
        jlos_hal_halt();
    }
}

static void jlos_page_frame_order_split(uint32_t frame, uint32_t order, uint32_t target_order)
{
    while (order > target_order) {
        order--;
        uint32_t buddy_frame = frame | order_to_frames(order);
        buddy_insert(buddy_frame, order);
    }
}

static void jlos_page_frame_mark_recycle(uint32_t frame)
{
    if (frame >= s_total_frames) return;
    jlos_page_t *pg = pfa_page(frame);
    pg->flags &= ~JLOS_PFA_FLAG_OCCUPIED;
    jlos_atomic_set(&pg->refcount, 0);
    pg->order = JLOS_PFA_BUDDY_ORDER_INVALID;
}

static void jlos_page_frame_mark_reserve(uint32_t frame)
{
    if (frame >= s_total_frames) return;
    jlos_page_t *pg = pfa_page(frame);
    pg->flags |= JLOS_PFA_FLAG_OCCUPIED;
    jlos_atomic_set(&pg->refcount, 1);
    pg->order = JLOS_PFA_BUDDY_ORDER_INVALID;
    pg->type = JLOS_PAGE_FRAME_TYPE_FREE;
    pg->pt_present_count = 0;
}

void jlos_page_frame_allocator_init(void)
{
    uint32_t phys_start = KERNEL_MEMORY_PHYSICAL_START;
    uint32_t phys_end = jlos_device_physical_memory_end;
    const multiboot_info_t *mb = jlos_device_multiboot_info;

    s_start_addr = phys_start;
    s_total_frames = (phys_end - phys_start) / JLOS_PAGE_FRAME_SIZE;

    for (uint32_t i = 0; i <= JLOS_PFA_MAX_ORDER; i++) {
        jlos_list_init(&s_free_area[i]);
    }

    s_pages = (jlos_page_t *)jlos_pfa_boot_alloc(s_total_frames * sizeof(jlos_page_t));
    for (uint32_t f = 0; f < s_total_frames; f++) {
        jlos_page_t *pg = &s_pages[f];
        jlos_list_init(&pg->u.free_list);
        pg->flags = 0;
        pg->type = JLOS_PAGE_FRAME_TYPE_FREE;
        pg->order = JLOS_PFA_BUDDY_ORDER_INVALID;
        jlos_atomic_set(&pg->refcount, 0);
    }

    uint32_t kernel_frames = ((uint32_t)&_kernel_end_phys - phys_start) / JLOS_PAGE_FRAME_SIZE;
    for (uint32_t i = 0; i < kernel_frames && i < s_total_frames; i++) {
        jlos_page_frame_mark_reserve(i);
    }
    
    uint32_t boot_frames = ((uint32_t)&_boot_end_phys - phys_start) / JLOS_PAGE_FRAME_SIZE;
    for (uint32_t i = kernel_frames; i < boot_frames && i < s_total_frames; i++) {
        jlos_page_frame_mark_recycle(i);
    }

    uint32_t boot_alloc_end_phys = jlos_pfa_boot_alloc_get_end();
    uint32_t boot_alloc_end_frame = JLOS_EXCEPT_CEIL(boot_alloc_end_phys - phys_start, JLOS_PAGE_FRAME_SIZE);
    for (uint32_t i = kernel_frames; i < boot_alloc_end_frame && i < s_total_frames; i++) {
        jlos_page_frame_mark_reserve(i);
    }

    if (phys_start > 0) {
        jlos_page_frame_mark_occupied(0, phys_start);
    }
    if (mb && (mb->flags & MULTIBOOT_INFO_MEM_MAP)) {
        multiboot_mmap_entry_t *entries = (multiboot_mmap_entry_t *)PHYS_TO_VIRT(mb->mmap_addr);
        uint32_t count = mb->mmap_length / sizeof(multiboot_mmap_entry_t);
        for (uint32_t i = 0; i < count; i++) {
            multiboot_mmap_entry_t *e = &entries[i];
            if (e->type == MULTIBOOT_MEMORY_AVAILABLE) {
                continue;
            }
            uint64_t start = e->base_addr;
            uint64_t end = start + e->length;
            if (end <= phys_start || start >= phys_end) {
                continue;
            }
            uint32_t s32 = (start < phys_start) ? phys_start : (uint32_t)start;
            uint32_t e32 = (end > phys_end) ? phys_end : (uint32_t)end;
            if (s32 < e32) {
                jlos_page_frame_mark_occupied(s32, e32);
            }
        }
    }

    for (uint32_t frame = 0; frame < s_total_frames; frame++) {
        jlos_page_t *pg = pfa_page(frame);
        if (!(pg->flags & JLOS_PFA_FLAG_OCCUPIED) && pg->order == JLOS_PFA_BUDDY_ORDER_INVALID) {
            buddy_insert(frame, 0);
        }
    }
    /* 2. 逐层向上合并 buddy 对 */
    for (uint32_t order = 0; order < JLOS_PFA_MAX_ORDER; order++) {
        uint32_t block = order_to_frames(order);
        for (uint32_t frame = 0; frame + block * 2 <= s_total_frames; frame += block * 2) {
            if (pfa_page(frame)->order != (uint8_t)order) continue;
            if (!buddy_mergeable(frame, order)) continue;
            uint32_t buddy = frame ^ block;
            buddy_remove(buddy, order);
            buddy_remove(frame, order);
            uint32_t merged_start = (buddy < frame) ? buddy : frame;
            buddy_insert(merged_start, order + 1);
        }
    }
    jlos_atomic_set(&s_free_frames, 0);
    for (int order = 0; order <= JLOS_PFA_MAX_ORDER; order++) {
        jlos_list_head_t *it;
        jlos_list_head_t *head = &s_free_area[order];
        jlos_list_for_each(it, head) {
            jlos_atomic_fetch_add(&s_free_frames, order_to_frames(order));
        }
    }
#if KERNEL_CONFIG_DEBUG_MEMORY
    jlos_page_frame_print_buddy();
#endif
}

void *jlos_page_frame_malloc(void)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_buddy_lock);
    uint32_t target_order = 0;
    uint32_t order = target_order;
    uint32_t masked = s_free_order_bitmap & (~0U << target_order);
    if (!masked) {
        jlos_spin_unlock_irqrestore(&s_buddy_lock, flags);
        return NULL;
    }
    order = __builtin_ctz(masked);

    uint32_t frame = (uint32_t)(container_of(s_free_area[order].next, jlos_page_t, u.free_list) - s_pages);
    buddy_remove(frame, order);
    if (order > target_order) {
        jlos_page_frame_order_split(frame, order, target_order);
    }
    jlos_page_frame_mark_reserve(frame);
    jlos_atomic_dec(&s_free_frames);
    jlos_spin_unlock_irqrestore(&s_buddy_lock, flags);
    return (void *)PHYS_TO_VIRT(s_start_addr + frame * JLOS_PAGE_FRAME_SIZE);
}

static void buddy_free_nolock(uint32_t frame, uint32_t order)
{
    while (order < JLOS_PFA_MAX_ORDER && buddy_mergeable(frame, order)) {
        uint32_t buddy = frame ^ order_to_frames(order);
        buddy_remove(buddy, order);
        if (buddy < frame) frame = buddy;
        order++;
    }
    buddy_insert(frame, order);
    jlos_atomic_fetch_add(&s_free_frames, order_to_frames(order));
}

static void buddy_free_range(uint32_t base_frame, uint32_t len)
{
    while (len) {
        uint32_t order = 0;
        while (order_to_frames(order + 1) <= len && !(base_frame & (order_to_frames(order + 1) - 1))) {
            order++;
        }
        uint32_t block = order_to_frames(order);
        for (uint32_t i = 0; i < block; i++) {
            jlos_page_t *pg = pfa_page(base_frame + i);
            pg->flags &= ~JLOS_PFA_FLAG_OCCUPIED;
            pg->order = JLOS_PFA_BUDDY_ORDER_INVALID;
            pg->type = JLOS_PAGE_FRAME_TYPE_FREE;
            pg->pt_present_count = 0;
        }
        buddy_free_nolock(base_frame, order);
        base_frame += block;
        len -= block;
    }
}

static inline uint32_t pfa_phys_to_frame(uint32_t p)
{
    return (p - s_start_addr) / JLOS_PAGE_FRAME_SIZE;
}

void jlos_page_frame_free(void *addr)
{
    uint32_t phys = (uint32_t)VIRT_TO_PHYS(addr);
    jlos_page_frame_refcount_dec(phys);
}

void jlos_page_frame_free_bulk(uint32_t phys_start, uint32_t num_frames)
{
    uint32_t head_frame = pfa_phys_to_frame(phys_start);
    uint32_t frames[JLOS_PFA_FREE_BULK_MAX];
    uint32_t n = 0;
    for (uint32_t i = 0; i < num_frames; i++) {
        uint32_t phys = phys_start + i * JLOS_PAGE_FRAME_SIZE;
        uint32_t frame = pfa_phys_to_frame(phys);
        if (frame >= s_total_frames) {
            continue;
        }
        jlos_atomic_t *rc = &s_pages[frame].refcount;
        int old = jlos_atomic_read(rc);
        if (old == 0) {
            printk_emerg("frame = %u refcount = 0 (double free)\n", frame);
            goto halt;
        }
        while (old > 0) {
            if (jlos_atomic_cmpxchg(rc, &old, old - 1)) {
                if (old == 1) {
                    if (n == JLOS_PFA_FREE_BULK_MAX) {
                        printk_emerg("free_bulk overflow: %u frames > %u\n", num_frames, JLOS_PFA_FREE_BULK_MAX);
                        goto halt;
                    }
                    frames[n++] = frame;
                }
                break;
            }
        }
    }
    if (!n) {
        return;
    }

    uint32_t flags = jlos_spin_lock_irqsave(&s_buddy_lock);
    if (head_frame < s_total_frames) {
        uint8_t head_order = s_pages[head_frame].order;
        if (head_order != JLOS_PFA_BUDDY_ORDER_INVALID && order_to_frames(head_order) < num_frames) {
            printk_emerg("free_bulk frame=%u order=%u n=%u (over-release!)\n",
                   head_frame, head_order, num_frames);
            goto halt;
        }
    }
    for (uint32_t i = 1; i < n; i++) {
        uint32_t v = frames[i], j = i;
        while (j > 0 && frames[j - 1] > v) {
            frames[j] = frames[j - 1];
            j--;
        }
        frames[j] = v;
    }
    uint32_t start = frames[0], len = 1;
    for (uint32_t i = 1; i < n; i++) {
        if (frames[i] == start + len) {
            len++;
            continue;
        }
        buddy_free_range(start, len);
        start = frames[i];
        len = 1;
    }
    buddy_free_range(start, len);
    jlos_spin_unlock_irqrestore(&s_buddy_lock, flags);
    return;
halt:
    for (;;) {
        jlos_hal_halt();
    }
}

void jlos_page_frame_refcount_inc(uint32_t phys_addr)
{
    uint32_t frame = pfa_phys_to_frame(phys_addr);
    if (frame >= s_total_frames) {
        return;
    }
    jlos_atomic_t *rc = &s_pages[frame].refcount;
    int old = jlos_atomic_read(rc);
    while (old > 0 && old < JLOS_PAGE_FRAME_REFCOUNT_MAX) {
        if (jlos_atomic_cmpxchg(rc, &old, old + 1)) {
            return;
        }
    }
    printk_emerg("refcount overflow/free frame = %u, count = %d\n", frame, old);
    for (;;) {
        jlos_hal_halt();
    }
}

void jlos_page_frame_refcount_dec(uint32_t phys_addr)
{
    uint32_t frame = pfa_phys_to_frame(phys_addr);
    if (frame >= s_total_frames) {
        return;
    }
    jlos_atomic_t *rc = &s_pages[frame].refcount;
    int old = jlos_atomic_read(rc);
    while (old > 0) {
        if (jlos_atomic_cmpxchg(rc, &old, old - 1)) {
            if (old == 1) {
                uint32_t fl = jlos_spin_lock_irqsave(&s_buddy_lock);
                s_pages[frame].flags &= ~JLOS_PFA_FLAG_OCCUPIED;
                s_pages[frame].order = JLOS_PFA_BUDDY_ORDER_INVALID;
                s_pages[frame].type = JLOS_PAGE_FRAME_TYPE_FREE;
                s_pages[frame].pt_present_count = 0;
                buddy_free_nolock(frame, 0);
                jlos_spin_unlock_irqrestore(&s_buddy_lock, fl);
            }
            return;
        }
    }
    printk_emerg("underflow frame = %u\n", frame);
    for (;;) {
        jlos_hal_halt();
    }
}

uint8_t jlos_page_frame_refcount_get(uint32_t phys_addr)
{
    uint32_t frame = pfa_phys_to_frame(phys_addr);
    if (frame >= s_total_frames) {
        return 0;
    }
    return (uint8_t)jlos_atomic_read(&s_pages[frame].refcount);
}

jlos_page_frame_type_t jlos_page_frame_get_type(uint32_t phys)
{
    if (phys < s_start_addr) {
        return JLOS_PAGE_FRAME_TYPE_FREE;
    }
    uint32_t f = pfa_phys_to_frame(phys);
    return (f < s_total_frames) ? (jlos_page_frame_type_t)s_pages[f].type : JLOS_PAGE_FRAME_TYPE_FREE;
}

void *jlos_page_frame_get_owner(uint32_t phys)
{
    if (phys < s_start_addr) {
        return NULL;
    }
    uint32_t f = pfa_phys_to_frame(phys);
    return (f < s_total_frames) ? s_pages[f].u.owner : NULL;
}

void jlos_page_frame_set_owner_type(uint32_t phys, void *owner, jlos_page_frame_type_t type)
{
    if (phys < s_start_addr) {
        return;
    }
    uint32_t f = pfa_phys_to_frame(phys);
    if (f >= s_total_frames) {
        return;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&s_owner_lock);
    if (!(s_pages[f].flags & JLOS_PFA_FLAG_OCCUPIED)) {
        jlos_spin_unlock_irqrestore(&s_owner_lock, fl);
        printk_emerg("[pfa] set_owner_type on free frame=%u (union conflict!)\n", f);
        for (;;) {
            jlos_hal_halt();
        }
    }
    s_pages[f].type = (uint8_t)type;
    s_pages[f].u.owner = owner;
    jlos_spin_unlock_irqrestore(&s_owner_lock, fl);
}

void jlos_page_frame_clear_owner_type(uint32_t phys)
{
    jlos_page_frame_set_owner_type(phys, NULL, JLOS_PAGE_FRAME_TYPE_FREE);
}

void *jlos_page_frame_reserve_bulk(uint32_t num_frames)
{
    if (!num_frames || num_frames > s_total_frames) return NULL;
    uint32_t flags = jlos_spin_lock_irqsave(&s_buddy_lock);

    uint32_t target_order = frames_to_order(num_frames);
    uint32_t masked = s_free_order_bitmap & (~0U << target_order);
    if (!masked) {
        jlos_spin_unlock_irqrestore(&s_buddy_lock, flags);
        return NULL;
    }
    uint32_t order = __builtin_ctz(masked);

    uint32_t frame = (uint32_t)(container_of(s_free_area[order].next, jlos_page_t, u.free_list) - s_pages);
    buddy_remove(frame, order);
    if (order > target_order) {
        jlos_page_frame_order_split(frame, order, target_order);
    }

    uint32_t extra = order_to_frames(target_order) - num_frames;
    if (extra) {
        buddy_free_range(frame + num_frames, extra);
    }

    for (uint32_t i = 0; i < num_frames; i++) {
        jlos_page_frame_mark_reserve(frame + i);
    }
    s_pages[frame].order = (uint8_t)target_order;
    jlos_atomic_fetch_sub(&s_free_frames, (int)num_frames);
    jlos_spin_unlock_irqrestore(&s_buddy_lock, flags);
    return (void *)PHYS_TO_VIRT(s_start_addr + frame * JLOS_PAGE_FRAME_SIZE);
}

void *jlos_page_frame_alloc_order(uint32_t order)
{
    if (order > JLOS_PFA_MAX_ORDER) {
        return NULL;
    }
    return jlos_page_frame_reserve_bulk(order_to_frames(order));
}

void jlos_page_frame_free_order(void *addr, uint32_t order)
{
    if (order > JLOS_PFA_MAX_ORDER) {
        return;
    }
    jlos_page_frame_free_bulk((uint32_t)VIRT_TO_PHYS(addr), order_to_frames(order));
}

void jlos_page_frame_mark_occupied(uint32_t phys_start, uint32_t phys_end)
{
    if (phys_end <= phys_start) {
        return;
    }
    uint32_t start_frame = pfa_phys_to_frame(phys_start);
    uint32_t end_frame   = JLOS_EXCEPT_CEIL(phys_end - s_start_addr, JLOS_PAGE_FRAME_SIZE);
    if (start_frame >= s_total_frames) {
        return;
    }
    if (end_frame > s_total_frames) {
        end_frame = s_total_frames;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&s_buddy_lock);
    for (uint32_t i = start_frame; i < end_frame; i++) {
        jlos_page_t *pg = &s_pages[i];
        if (pg->flags & JLOS_PFA_FLAG_OCCUPIED) {
            continue;
        }
        if (pg->order != JLOS_PFA_BUDDY_ORDER_INVALID) {
            uint8_t ord = pg->order;
            buddy_remove(i, ord);
            jlos_atomic_fetch_sub(&s_free_frames, order_to_frames(ord));
        }
        jlos_page_frame_mark_reserve(i);
    }
    jlos_spin_unlock_irqrestore(&s_buddy_lock, flags);
}

uint32_t jlos_page_frame_get_total(void)
{
    return s_total_frames;
}

uint32_t jlos_page_frame_get_free(void)
{
    return (uint32_t)jlos_atomic_read(&s_free_frames);
}

void jlos_page_frame_print_buddy(void)
{
    for (int o = 0; o <= JLOS_PFA_MAX_ORDER; o++) {
        uint32_t count = 0;
        jlos_list_head_t *it;
        jlos_list_head_t *head = &s_free_area[o];
        jlos_list_for_each(it, head) {
            count++;
        }
        if (count) {
            printk_debug("order: %d, frames: %u, blocks: %u\n", o, order_to_frames(o), count);
        }
    }
}

bool jlos_page_frame_contains_phys(uint32_t phys)
{
    return phys >= s_start_addr && (phys - s_start_addr) / JLOS_PAGE_FRAME_SIZE < s_total_frames;
}

uint32_t jlos_page_frame_start_phys(void)
{
    return s_start_addr;
}

void jlos_page_frame_pt_present_count_set(uint32_t phys, uint8_t count)
{
    uint32_t f = pfa_phys_to_frame(phys);
    if (f >= s_total_frames) {
        return;
    }
    s_pages[f].pt_present_count = count;
}

uint32_t jlos_page_frame_pt_present_count_get(uint32_t phys)
{
    uint32_t f = pfa_phys_to_frame(phys);
    if (f >= s_total_frames) {
        return 0;
    }
    return s_pages[f].pt_present_count;
}

void jlos_page_frame_pt_present_count_inc(uint32_t phys)
{
    uint32_t f = pfa_phys_to_frame(phys);
    if (f >= s_total_frames) {
        return;
    }
    s_pages[f].pt_present_count++;
}

void jlos_page_frame_pt_present_count_dec(uint32_t phys)
{
    uint32_t f = pfa_phys_to_frame(phys);
    if (f < s_total_frames && s_pages[f].pt_present_count > 0) {
        s_pages[f].pt_present_count--;
    } 
}
