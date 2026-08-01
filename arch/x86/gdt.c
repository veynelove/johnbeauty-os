#include <arch/x86/gdt.h>
#include <hal/mmu.h>

extern uint32_t _text_start, _text_end;
extern uint32_t _data_start, _data_end;
extern uint32_t _bss_start, _bss_end;

static jlos_hal_kernel_segments_t s_kernel_segments = {0};
static jlos_gdt_t s_kernel_gdt;

void jlos_hal_kernel_segments_init(void)
{
    s_kernel_segments.text_start = (uint32_t)&_text_start;
    s_kernel_segments.text_end = (uint32_t)&_text_end;
    s_kernel_segments.data_start = (uint32_t)&_data_start;
    s_kernel_segments.data_end = (uint32_t)&_data_end;
    s_kernel_segments.bss_start = (uint32_t)&_bss_start;
    s_kernel_segments.bss_end = (uint32_t)&_bss_end;
}

const jlos_hal_kernel_segments_t *jlos_hal_get_kernel_segments(void)
{
    return &s_kernel_segments;
}

void jlos_gdt_segment_descriptor_init(jlos_gdt_segment_descriptor_t* self, uint32_t base, uint32_t limit, uint8_t flags)
{
    uint8_t* target = (uint8_t *)self;
    if (limit <= 65536) {
        target[6] = 0x40;
    } 
    else {
        if ((limit & 0xFFF) != 0xFFF) {
            limit = (limit >> 12) - 1;
        }
        else {
            limit = limit >> 12;
        }
        target[6] = 0xC0;
    }
    target[0] = limit & 0xFF;
    target[1] = (limit >> 8) & 0xFF;
    target[6] |= (limit >> 16) & 0xF;
    
    target[2] = base & 0xFF;
    target[3] = (base >> 8) & 0xFF;
    target[4] = (base >> 16) & 0xFF;
    target[7] = (base >> 24) & 0xFF;
    
    target[5] = flags;
}

uint32_t jlos_gdt_segment_descriptor_base(jlos_gdt_segment_descriptor_t* self)
{
    uint8_t *target = (uint8_t *)self;
    uint32_t result = target[7];
    result = (result << 8) + target[4];
    result = (result << 8) + target[3];
    result = (result << 8) + target[2];
    return result;
}

uint32_t jlos_gdt_segment_descriptor_limit(jlos_gdt_segment_descriptor_t* self)
{
    uint8_t *target = (uint8_t *)self;
    uint32_t result = target[6] & 0xF;
    result = (result << 8) + target[1];
    result = (result << 8) + target[0];
    
    if ((target[6] & 0xC0) == 0xC0) {
        result = (result << 12) | 0xFFF;
    }
    return result;
}

void jlos_gdt_init()
{
    jlos_gdt_t *gdt = &s_kernel_gdt;
    jlos_gdt_segment_descriptor_init(&gdt->null_segment_selector, 0, 0, 0);
    jlos_gdt_segment_descriptor_init(&gdt->unused_segment_selector, 0, 0, 0);
    jlos_gdt_segment_descriptor_init(&gdt->code_segment_selector, 0, 64*1024*1024, 0x9A);
    jlos_gdt_segment_descriptor_init(&gdt->data_segment_selector, 0, 64*1024*1024, 0x92);
    
    jlos_gdt_segment_descriptor_init(&gdt->user_code_segment_selector, 0, 64 * 1024 * 1024, 0xFA);
    jlos_gdt_segment_descriptor_init(&gdt->user_data_segment_selector, 0, 64 * 1024 * 1024, 0xF2);
    jlos_gdt_segment_descriptor_init(&gdt->tss_segment_selector, 0, 0, 0x89);
    uint32_t i[2];
    i[1] = (uint32_t)gdt;
    i[0] = sizeof(jlos_gdt_t) << 16;
    
    __asm__ __volatile__("lgdt (%0)" : : "p" (((uint8_t *) i)+2));
}

void jlos_gdt_destroy(jlos_gdt_t* self)
{
}

uint16_t jlos_gdt_data_segment_selector(jlos_gdt_t* self)
{
    return (uint8_t *)&self->data_segment_selector - (uint8_t *)self;
}

uint16_t jlos_gdt_code_segment_selector(jlos_gdt_t* self)
{
    return (uint8_t *)&self->code_segment_selector - (uint8_t *)self;
}

uint16_t jlos_gdt_user_code_segment_selector(jlos_gdt_t *self)
{
    return (uint16_t)((uint8_t *)&self->user_code_segment_selector - (uint8_t *)self) | 3;
}

uint16_t jlos_gdt_user_data_segment_selector(jlos_gdt_t *self)
{
    return (uint16_t)((uint8_t *)&self->user_data_segment_selector - (uint8_t *)self) | 3;
}

uint16_t jlos_gdt_tss_selector(jlos_gdt_t *self)
{
    return (uint16_t)((uint8_t *)&self->tss_segment_selector - (uint8_t *)self);
}

void jlos_gdt_set_tss(jlos_gdt_t *self, uint32_t base, uint32_t limit)
{
    jlos_gdt_segment_descriptor_init(&self->tss_segment_selector, base, limit, 0x89);
    uint32_t i[2];
    i[1] = (uint32_t)self;
    i[0] = sizeof(jlos_gdt_t) << 16;
    __asm__ __volatile__("lgdt (%0)" : : "p" (((uint8_t *) i)+2));
}

jlos_gdt_t *jlos_gdt_get_kernel(void)
{
    return &s_kernel_gdt;
}
