#include <kernel/page_frame_allocator.h>
#include <kernel/memory_manager.h>

static uint32_t s_total_frames = 0;
static uint32_t s_free_frames = 0;
static uint8_t *s_bitmap = NULL;
static uint32_t s_start_addr = 0;
static uint32_t s_first_free_frame = 0;

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
    jlos_memset(s_bitmap, 0x00, bitmap_size); //ceil
    
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
    for (uint32_t i = s_first_free_frame; i < s_total_frames; i++) {
        if (!(s_bitmap[i / 8] & (1 << (i % 8)))) {
            s_bitmap[i / 8] |= (1 << (i % 8));
            s_free_frames--;
            return (void *)(s_start_addr + i * JLOS_PAGE_FRAME_SIZE);
        }
    }
    return NULL;
}

void jlos_page_frame_free(void *addr)
{
    uint32_t frame = ((uint32_t)addr - s_start_addr) / JLOS_PAGE_FRAME_SIZE;
    if (frame >= s_total_frames) {
        return;
    }
    if (!(s_bitmap[frame / 8] & (1 << (frame % 8)))) {
        return;
    }
    s_bitmap[frame / 8] &= ~(1 << (frame & 8));
    s_free_frames++;
}

uint32_t jlos_page_frame_get_total(void)
{
    return s_total_frames;
}

uint32_t jlos_page_frame_get_free(void)
{
    return s_free_frames;
}
