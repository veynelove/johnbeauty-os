#include <kernel/page_frame_allocator.h>
#include <kernel/memory_manager.h>
#include <hal/spinlock.h>

static uint32_t s_total_frames = 0;
static uint32_t s_free_frames = 0;
static uint8_t *s_bitmap = NULL;
static uint32_t s_start_addr = 0;
static uint32_t s_first_free_frame = 0;

static jlos_spinlock_t s_pfa_lock = JLOS_SPINLOCK_INIT;

void jlos_page_frame_allocator_init(uint32_t start_addr, uint32_t end_addr, uint32_t kernel_end_addr)
{
    s_start_addr = start_addr;
    s_total_frames = (end_addr - start_addr) / JLOS_PAGE_FRAME_SIZE;

    s_bitmap = (uint8_t *)kernel_end_addr;
    uint32_t bitmap_size = (s_total_frames + 7) / 8;
    if ((uint32_t)s_bitmap + bitmap_size > end_addr) {
        bitmap_size = end_addr - (uint32_t)s_bitmap;
        s_total_frames = bitmap_size * 8;
    }
    jlos_memset(s_bitmap, 0x00, bitmap_size);
    
    uint32_t kernel_frames = (kernel_end_addr - start_addr) / JLOS_PAGE_FRAME_SIZE;
    uint32_t bitmap_frames = (bitmap_size + JLOS_PAGE_FRAME_SIZE - 1) / JLOS_PAGE_FRAME_SIZE;

    for (uint32_t i = 0; i < kernel_frames + bitmap_frames; i++) {
        if (i < s_total_frames) {
            s_bitmap[i / 8] |= (1 << (i % 8));
        }
    }
    s_first_free_frame = kernel_frames + bitmap_frames;
    s_free_frames = s_total_frames - kernel_frames - bitmap_frames;
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
            return (void *)(s_start_addr + free_frame * JLOS_PAGE_FRAME_SIZE);
        }
        
        word_idx++;
    }
    
    jlos_spin_unlock_irqrestore(&s_pfa_lock, flags);
    return NULL;
}

void jlos_page_frame_free(void *addr)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_pfa_lock);
    uint32_t frame = ((uint32_t)addr - s_start_addr) / JLOS_PAGE_FRAME_SIZE;
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
