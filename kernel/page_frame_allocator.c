#include <kernel/page_frame_allocator.h>
#include <kernel/memory_manager.h>
#include <kernel/device.h>
#include <kernel/paging.h>
#include <kernel/printk.h>
#include <hal/spinlock.h>

extern uint32_t _boot_end_phys;

static uint32_t s_total_frames = 0;
static uint32_t s_free_frames = 0;
static uint8_t *s_bitmap = NULL;
static uint32_t s_start_addr = 0;
static uint32_t s_first_free_frame = 0;

static jlos_spinlock_t s_pfa_lock = JLOS_SPINLOCK_INIT;

void jlos_page_frame_allocator_init(uint32_t kernel_end_addr)
{
    uint32_t phys_start = KERNEL_MEMORY_PHYSICAL_START;
    uint32_t phys_end = jlos_device_physical_memory_end;
    const multiboot_info_t *mb = jlos_device_multiboot_info;

    s_start_addr = phys_start;
    s_total_frames = (phys_end - phys_start) / JLOS_PAGE_FRAME_SIZE;

    /* kernel_end_addr 是物理地址，必须转虚拟地址后才能作为指针解引用！
     * 否则切到新页表（去掉了 1MB+ 恒等映射）后访问 bitmap 立刻 PF。 */
    s_bitmap = (uint8_t *)PHYS_TO_VIRT(kernel_end_addr);
    uint32_t bitmap_size = (s_total_frames + 7) / 8;
    if ((uint32_t)s_bitmap + bitmap_size > PHYS_TO_VIRT(phys_end)) {
        bitmap_size = PHYS_TO_VIRT(phys_end) - (uint32_t)s_bitmap;
        s_total_frames = bitmap_size * 8;
    }
    jlos_memset(s_bitmap, 0x00, bitmap_size);
    
    uint32_t kernel_frames = (kernel_end_addr - phys_start) / JLOS_PAGE_FRAME_SIZE;
    uint32_t bitmap_frames = (bitmap_size + JLOS_PAGE_FRAME_SIZE - 1) / JLOS_PAGE_FRAME_SIZE;

    for (uint32_t i = 0; i < kernel_frames + bitmap_frames; i++) {
        if (i < s_total_frames) {
            s_bitmap[i / 8] |= (1 << (i % 8));
        }
    }
    s_first_free_frame = kernel_frames + bitmap_frames;
    s_free_frames = s_total_frames - kernel_frames - bitmap_frames;
    
    uint32_t boot_end = (uint32_t)&_boot_end_phys;
    uint32_t boot_frames = (boot_end - phys_start) / JLOS_PAGE_FRAME_SIZE;
    if (boot_frames > 0) {
        for (uint32_t i = 0; i < boot_frames; i++) {
            s_bitmap[i / 8] &= ~(1 << (i % 8));
        }
        s_free_frames += boot_frames;
        /* 注意：不改动 s_first_free_frame。
         * boot 帧虽然已标记空闲，但此时 boot_page_dir 还在 CR3 里。
         * 等 paging_init 切完内核页目录后，后续当分配器扫描到这些帧时
         * 就能安全复用了。 */
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
                printk("reserved: 0x%x ~ 0x%x\n", s32, e32);
            }
        }
    }
}

void *jlos_page_frame_malloc(void)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    
    uint32_t frame = s_first_free_frame;
    uint32_t end = s_total_frames;
    
    uint32_t word_idx = frame / 32;
    uint32_t end_word = (end + 31) / 32;
    uint32_t *bitmap32 = (uint32_t *)s_bitmap;
    
    while (word_idx < end_word) {
        uint32_t word = bitmap32[word_idx];
        if (word == 0xFFFFFFFF) {
            word_idx++;
            continue;
        }
        
        uint32_t free_bit = __builtin_ctz(~word);
        uint32_t free_frame = word_idx * 32 + free_bit;
        
        if (free_frame >= s_first_free_frame && free_frame < end) {
            bitmap32[word_idx] |= (1U << free_bit);
            s_free_frames--;
            
            if (free_frame == s_first_free_frame) {
                s_first_free_frame = free_frame + 1;
                while (s_first_free_frame < s_total_frames) {
                    uint32_t bi = s_first_free_frame / 8;
                    uint32_t fi = s_first_free_frame % 8;
                    if (!(s_bitmap[bi] & (1 << fi))) break;
                    s_first_free_frame++;
                }
            }
            
            jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
            return (void *)PHYS_TO_VIRT(s_start_addr + free_frame * JLOS_PAGE_FRAME_SIZE);
        }
        
        word_idx++;
    }
    
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
    return NULL;
}

void jlos_page_frame_free(void *addr)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    uint32_t frame = ((uint32_t)(VIRT_TO_PHYS(addr)) - s_start_addr) / JLOS_PAGE_FRAME_SIZE;
    if (frame >= s_total_frames) {
        jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
        return;
    }
    if (!(s_bitmap[frame / 8] & (1 << (frame % 8)))) {
        jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
        return;
    }
    s_bitmap[frame / 8] &= ~(1 << (frame % 8));
    s_free_frames++;
    if (frame < s_first_free_frame) {
        s_first_free_frame = frame;
    }
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
}

void *jlos_page_frame_reserve_bulk(uint32_t num_frames)
{
    if (!num_frames) {
        return NULL;
    }
    if (num_frames > s_total_frames) {
        return NULL;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    uint32_t start = 0;
    uint32_t len = 0;
    for (uint32_t i = 0; i < s_total_frames; i++) {
        bool occupied = (s_bitmap[i / 8] & (1 << (i % 8))) != 0;
        if (occupied) {
            len = 0;
            continue;
        }
        if (len == 0) {
            start = i;
        }
        len++;
        if (len >= num_frames) {
            break;
        }
    }
    if (len < num_frames) {
        jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
        return NULL;
    }
    for (uint32_t i = start; i < start + num_frames; i++) {
        s_bitmap[i / 8] |= (1 << (i % 8));
    }
    
    s_free_frames -= num_frames;
    if (start < s_first_free_frame) {
        s_first_free_frame = start + num_frames;
    }
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
    return (void *)PHYS_TO_VIRT(s_start_addr + start * JLOS_PAGE_FRAME_SIZE);
}

void jlos_page_frame_mark_occupied(uint32_t phys_start, uint32_t phys_end)
{
    if (phys_end <= phys_start) {
        return;
    }
    uint32_t start_frame = (phys_start - s_start_addr) / JLOS_PAGE_FRAME_SIZE;
    uint32_t end_frame = JLOS_EXCEPT_CEIL(phys_end - s_start_addr, JLOS_PAGE_FRAME_SIZE);
    if (start_frame >= s_total_frames) {
        return;
    }
    if (end_frame > s_total_frames) {
        end_frame = s_total_frames;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    for (uint32_t i = start_frame; i < end_frame; i++) {
        if (!(s_bitmap[i / 8] & (1 << (i % 8)))) {
            s_bitmap[i / 8] |= (1 << (i % 8));
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
