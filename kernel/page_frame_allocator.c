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
static uint32_t             s_free_frames = 0;
static uint32_t             s_start_addr = 0;

static uint8_t              *s_bitmap = NULL;
static uint8_t              *s_refcount = NULL;
static uint8_t              *s_buddy_order = NULL;
static uint32_t             s_free_order_bitmap = 0;

static uint8_t              *s_page_type = NULL;
static void                 **s_page_owner = NULL;

static free_order_node_t    *s_free_area[JLOS_PFA_MAX_ORDER + 1];

static jlos_spinlock_t      s_pfa_lock = JLOS_SPINLOCK_INIT;

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

static inline bool buddy_mergeable(uint32_t frame, uint32_t order)
{
    uint32_t block = order_to_frames(order);
    uint32_t buddy = frame ^ block;
    if (buddy >= s_total_frames) {
        return false;
    }
    /* 合并后块起始 = min(frame, buddy), 大小 = block * 2 */
    uint32_t merged_start = (buddy < frame) ? buddy : frame;
    if (merged_start + block * 2 > s_total_frames) {
        return false;   /* 合并后会跨越物理内存边界 → 拒绝 */
    }
    return (s_buddy_order[buddy] == order) && !JLOS_PFA_BIT_MAP_FRAME_VALUE(buddy);
}

static void buddy_insert(uint32_t frame, uint32_t order)
{
    uint32_t block = order_to_frames(order);
    /* 防 double insert: 确保帧 free 且不在任何 free list, 块不跨越物理边界 */
    if (frame >= s_total_frames || frame + block > s_total_frames) {
        printk_emerg("[buddy-OOB] insert frame=%u order=%u block=%u total=%u (cross phys boundary)\n",
               frame, order, block, s_total_frames);
        for (;;) jlos_hal_halt();
    }
    if (frame & (block - 1)) {
        printk_emerg("[buddy-ALIGN] insert frame=%u order=%u (frame not aligned to order)\n", frame, order);
        for (;;) jlos_hal_halt();
    }
    if (frame < s_total_frames) {
        bool is_reserved = JLOS_PFA_BIT_MAP_FRAME_VALUE(frame);
        uint8_t cur_order = s_buddy_order[frame];
        if (is_reserved) {
            printk_emerg("[buddy-ASSERT] insert frame=%u order=%u FAIL: bitmap=1 (reserved)!\n", frame, order);
            for (;;) jlos_hal_halt();
        }
        if (cur_order != JLOS_PFA_BUDDY_ORDER_INVALID && cur_order != order) {
            printk_emerg("[buddy-ASSERT] insert frame=%u order=%u FAIL: buddy_order=%u (already in free list)!\n",
                   frame, order, cur_order);
            for (;;) jlos_hal_halt();
        }
    }
    free_order_node_t *node = (free_order_node_t *)PHYS_TO_VIRT(s_start_addr + frame * JLOS_PAGE_FRAME_SIZE);
    node->phys_frame = frame;
    node->magic = JLOS_BUDDY_NODE_MAGIC;
    node->next = s_free_area[order];
    node->prev = NULL;
    if (s_free_area[order]) {
        s_free_area[order]->prev = node;
    } else {
        s_free_order_bitmap |= order_to_frames(order);
    }
    s_free_area[order] = node;
    s_buddy_order[frame] = (uint8_t)order;
}

static void buddy_remove(uint32_t frame, uint32_t order)
{
    uint32_t block = order_to_frames(order);
    if (frame >= s_total_frames || frame + block > s_total_frames) {
        printk_emerg("[buddy-PANIC] frame=%u order=%u block=%u s_total=%u (frame/block OOB)\n",
               frame, order, block, s_total_frames);
        for (;;) jlos_hal_halt();
    }
    free_order_node_t *node = (free_order_node_t *)PHYS_TO_VIRT(s_start_addr + frame * JLOS_PAGE_FRAME_SIZE);
    uint32_t bad = 0;
    if (node->magic != JLOS_BUDDY_NODE_MAGIC)   bad |= 1;
    if (node->phys_frame != frame)             bad |= 2;
    if (!(node->next || node->prev || s_free_area[order] == node)) bad |= 4;
    if (bad) {
        printk_emerg("[buddy-PANIC] remove frame=%u order=%u bad=%u node=%p "
               "next=%p prev=%p phys=0x%x magic=0x%x\n",
               frame, order, bad, node,
               node->next, node->prev, node->phys_frame, node->magic);
        for (;;) jlos_hal_halt();
    }
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        s_free_area[order] = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    }
    node->next = NULL;
    node->prev = NULL;
    node->magic = 0;
    if (!s_free_area[order]) {
        s_free_order_bitmap &= ~order_to_frames(order);
    }
    s_buddy_order[frame] = JLOS_PFA_BUDDY_ORDER_INVALID;
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
    if (frame >= s_total_frames) return;   /* 防御: 越界帧忽略 */
    s_bitmap[frame / 8] &= ~(1 << (frame % 8));
    s_refcount[frame] = 0;
    s_buddy_order[frame] = JLOS_PFA_BUDDY_ORDER_INVALID;
}

static void jlos_page_frame_mark_reserve(uint32_t frame)
{
    if (frame >= s_total_frames) return;   /* 防御: 越界帧忽略 */
    s_bitmap[frame / 8] |= (1 << (frame % 8));
    s_refcount[frame] = 1;
    s_buddy_order[frame] = JLOS_PFA_BUDDY_ORDER_INVALID;
}

void jlos_page_frame_allocator_init(void)
{
    uint32_t phys_start = KERNEL_MEMORY_PHYSICAL_START;
    uint32_t phys_end = jlos_device_physical_memory_end;
    const multiboot_info_t *mb = jlos_device_multiboot_info;

    s_start_addr = phys_start;
    s_total_frames = (phys_end - phys_start) / JLOS_PAGE_FRAME_SIZE;

    uint32_t bitmap_size = (s_total_frames + 7) / 8;
    uint32_t refcount_size = s_total_frames;
    uint32_t buddy_order_size = s_total_frames;

    s_bitmap = (uint8_t *)jlos_pfa_boot_alloc(bitmap_size);
    s_refcount = (uint8_t *)jlos_pfa_boot_alloc(refcount_size);
    s_buddy_order = (uint8_t *)jlos_pfa_boot_alloc(buddy_order_size);
    
    s_page_type = (uint8_t *)jlos_pfa_boot_alloc(s_total_frames);
    s_page_owner = (void **)jlos_pfa_boot_alloc(s_total_frames * sizeof(void *));

    jlos_memset(s_bitmap, 0x00, bitmap_size);
    jlos_memset(s_refcount, 0x00, refcount_size);
    jlos_memset(s_buddy_order, JLOS_PFA_BUDDY_ORDER_INVALID, buddy_order_size);
    
    for (uint32_t f = 0; f < s_total_frames; f++) {
        s_page_type[f] = JLOS_PAGE_FRAME_TYPE_FREE;
        s_page_owner[f] = NULL;
    }

    uint32_t kernel_frames = ((uint32_t)&_kernel_end_phys - phys_start) / JLOS_PAGE_FRAME_SIZE;
    for (uint32_t i = 0; i < kernel_frames && i < s_total_frames; i++) {
        jlos_page_frame_mark_reserve(i);
    }
    
    uint32_t boot_frames = ((uint32_t)&_boot_end_phys - phys_start) / JLOS_PAGE_FRAME_SIZE;
    for (uint32_t i = 0; i < boot_frames && i < s_total_frames; i++) {
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

    /* 经典 buddy 构建: 先全插 order-0, 再逐层向上合并.
     * 原倒序扫描导致高 order 块内部 frame 仍为 INVALID, 被低 order
     * 误判为单页 free 双重插入 -> 页表页被覆写 -> kernel heap PF. */
    /* 1. 所有真正空闲帧先插 order-0 */
    for (uint32_t frame = 0; frame < s_total_frames; frame++) {
        if (!JLOS_PFA_BIT_MAP_FRAME_VALUE(frame)
        && s_buddy_order[frame] == JLOS_PFA_BUDDY_ORDER_INVALID) {
            buddy_insert(frame, 0);
        }
    }
    /* 2. 逐层向上合并 buddy 对 */
    for (uint32_t order = 0; order < JLOS_PFA_MAX_ORDER; order++) {
        bool merged = true;
        while (merged) {
            merged = false;
            uint32_t block = order_to_frames(order);
            for (uint32_t frame = 0; frame + block <= s_total_frames; frame += block) {
                if (s_buddy_order[frame] != (uint8_t)order) continue;
                if (!buddy_mergeable(frame, order)) continue;
                uint32_t buddy = frame ^ block;
                buddy_remove(buddy, order);
                buddy_remove(frame, order);
                uint32_t merged_start = (buddy < frame) ? buddy : frame;
                buddy_insert(merged_start, order + 1);
                merged = true;
            }
        }
    }
    s_free_frames = 0;
    for (int order = 0; order <= JLOS_PFA_MAX_ORDER; order++) {
        free_order_node_t *n = s_free_area[order];
        uint32_t block_size = order_to_frames(order);
        while (n) {
            s_free_frames += block_size;
            n = n->next;
        }
    }
#if KERNEL_CONFIG_DEBUG_MEMORY
    jlos_page_frame_print_buddy();
#endif
}

void *jlos_page_frame_malloc(void)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    uint32_t target_order = 0;
    uint32_t order = target_order;
    uint32_t masked = s_free_order_bitmap & (~0U << target_order);
    if (!masked) {
        jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
        return NULL;
    }
    order = __builtin_ctz(masked);

    uint32_t frame = s_free_area[order]->phys_frame;
    buddy_remove(frame, order);
    if (order > target_order) {
        jlos_page_frame_order_split(frame, order, target_order);
    }
    jlos_page_frame_mark_reserve(frame);
    s_free_frames--;
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
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
    s_free_frames += order_to_frames(order);
}

void jlos_page_frame_free(void *addr)
{
    jlos_page_frame_refcount_dec((uint32_t)VIRT_TO_PHYS(addr));
}

static inline uint32_t pfa_phys_to_frame(uint32_t p)
{
    return (p - s_start_addr) / JLOS_PAGE_FRAME_SIZE;
}

void jlos_page_frame_free_bulk(uint32_t phys_start, uint32_t num_frames)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    for (uint32_t i = 0; i < num_frames; i++) {
        uint32_t phys = phys_start + i * JLOS_PAGE_FRAME_SIZE;
        uint32_t frame = pfa_phys_to_frame(phys);
        if (frame < s_total_frames && s_refcount[frame] && --s_refcount[frame] == 0) {
            s_bitmap[frame / 8] &= (uint8_t)~(1U << (frame & 7));
            buddy_free_nolock(frame, 0);
        }
    }
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
}

void jlos_page_frame_refcount_inc(uint32_t phys_addr)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    uint32_t frame = pfa_phys_to_frame(phys_addr);
    if (frame < s_total_frames && s_refcount[frame] < JLOS_PAGE_FRAME_REFCOUNT_MAX) {
        s_refcount[frame]++;
    }
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
}

void jlos_page_frame_refcount_dec(uint32_t phys_addr)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    uint32_t frame = pfa_phys_to_frame(phys_addr);
    if (frame >= s_total_frames) {
        jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
        return;
    }
    if (s_refcount[frame] && --s_refcount[frame] == 0) {
        s_bitmap[frame / 8] &= (uint8_t)~(1U << (frame & 7));
        buddy_free_nolock(frame, 0);
    }
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
}

uint8_t jlos_page_frame_refcount_get(uint32_t phys_addr)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    uint32_t frame = pfa_phys_to_frame(phys_addr);
    uint8_t refcount = 0;
    if (frame < s_total_frames) {
        refcount = s_refcount[frame];
    }
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
    return refcount;
}

jlos_page_frame_type_t jlos_page_frame_get_type(uint32_t phys)
{
    if (phys < s_start_addr) {
        return JLOS_PAGE_FRAME_TYPE_FREE;
    }
    uint32_t f = pfa_phys_to_frame(phys);
    return (f < s_total_frames) ? (jlos_page_frame_type_t)s_page_type[f] : JLOS_PAGE_FRAME_TYPE_FREE;
}

void *jlos_page_frame_get_owner(uint32_t phys)
{
    if (phys < s_start_addr) {
        return NULL;
    }
    uint32_t f = pfa_phys_to_frame(phys);
    return (f < s_total_frames) ? s_page_owner[f] : NULL;
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
    uint32_t fl = jlos_spin_lock_irqsave(&s_pfa_lock);
    s_page_type[f] = (uint8_t)type;
    s_page_owner[f] = owner;
    jlos_spin_unlock_irqrestore(&s_pfa_lock, fl);
}

void jlos_page_frame_clear_owner_type(uint32_t phys)
{
    jlos_page_frame_set_owner_type(phys, NULL, JLOS_PAGE_FRAME_TYPE_FREE);
}

void *jlos_page_frame_reserve_bulk(uint32_t num_frames)
{
    if (!num_frames || num_frames > s_total_frames) return NULL;
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);

    uint32_t target_order = frames_to_order(num_frames);
    uint32_t masked = s_free_order_bitmap & (~0U << target_order);
    if (!masked) {
        jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
        return NULL;
    }
    uint32_t order = __builtin_ctz(masked);

    uint32_t frame = s_free_area[order]->phys_frame;
    buddy_remove(frame, order);
    if (order > target_order) {
        jlos_page_frame_order_split(frame, order, target_order);
    }

    uint32_t extra = order_to_frames(target_order) - num_frames;
    for (uint32_t i = 0; i < extra; i++) {
        uint32_t free_frame = frame + num_frames + i;
        jlos_page_frame_mark_recycle(free_frame);
        buddy_free_nolock(free_frame, 0);
    }

    for (uint32_t i = 0; i < num_frames; i++) {
        jlos_page_frame_mark_reserve(frame + i);
    }
    s_free_frames -= num_frames;
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
    return (void *)PHYS_TO_VIRT(s_start_addr + frame * JLOS_PAGE_FRAME_SIZE);
}

void jlos_page_frame_mark_occupied(uint32_t phys_start, uint32_t phys_end)
{
    if (phys_end <= phys_start) return;
    uint32_t start_frame = pfa_phys_to_frame(phys_start);
    uint32_t end_frame   = JLOS_EXCEPT_CEIL(phys_end - s_start_addr, JLOS_PAGE_FRAME_SIZE);
    if (start_frame >= s_total_frames) return;
    if (end_frame > s_total_frames) end_frame = s_total_frames;
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    for (uint32_t i = start_frame; i < end_frame; i++) {
        if (!JLOS_PFA_BIT_MAP_FRAME_VALUE(i)) {
            jlos_page_frame_mark_reserve(i);
            s_free_frames--;
        }
    }
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
}

uint32_t jlos_page_frame_get_total(void)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    uint32_t tf = s_total_frames;
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
    return tf;
}

uint32_t jlos_page_frame_get_free(void)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    uint32_t ff = s_free_frames;
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
    return ff;
}

void jlos_page_frame_print_buddy(void)
{
    for (int o = 0; o <= JLOS_PFA_MAX_ORDER; o++) {
        uint32_t count = 0;
        free_order_node_t *n = s_free_area[o];
        while (n) { count++; n = n->next; }
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