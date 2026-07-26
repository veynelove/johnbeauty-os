#include <arch/x86/gdt.h>
#include <hal/mmu.h>

extern uint32_t _text_start, _text_end;
extern uint32_t _data_start, _data_end;
extern uint32_t _bss_start, _bss_end;

static jlos_hal_kernel_segments_t s_kernel_segments = {0};

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

void jlos_gdt_segment_descriptor_init(jlos_gdt_segment_descriptor_t* self, uint32_t m_base, uint32_t limit, uint8_t m_flags)
{
    uint8_t* target = (uint8_t*)self;
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
    
    target[2] = m_base & 0xFF;
    target[3] = (m_base >> 8) & 0xFF;
    target[4] = (m_base >> 16) & 0xFF;
    target[7] = (m_base >> 24) & 0xFF;
    
    target[5] = m_flags;
}

uint32_t jlos_gdt_segment_descriptor_base(jlos_gdt_segment_descriptor_t* self)
{
    uint8_t *target = (uint8_t*)self;
    uint32_t result = target[7];
    result = (result << 8) + target[4];
    result = (result << 8) + target[3];
    result = (result << 8) + target[2];
    return result;
}

uint32_t jlos_gdt_segment_descriptor_limit(jlos_gdt_segment_descriptor_t* self)
{
    uint8_t *target = (uint8_t*)self;
    uint32_t result = target[6] & 0xF;
    result = (result << 8) + target[1];
    result = (result << 8) + target[0];
    
    if ((target[6] & 0xC0) == 0xC0) {
        result = (result << 12) | 0xFFF;
    }
    return result;
}

void jlos_gdt_init(jlos_gdt_t* self)
{
    jlos_gdt_segment_descriptor_init(&self->m_null_segment_selector, 0, 0, 0);
    jlos_gdt_segment_descriptor_init(&self->m_unused_segment_selector, 0, 0, 0);
    jlos_gdt_segment_descriptor_init(&self->m_code_segment_selector, 0, 64*1024*1024, 0x9A);
    jlos_gdt_segment_descriptor_init(&self->m_data_segment_selector, 0, 64*1024*1024, 0x92);
    
    uint32_t i[2];
    i[1] = (uint32_t)self;
    i[0] = sizeof(jlos_gdt_t) << 16;
    
    __asm__ __volatile__("lgdt (%0)" : : "p" (((uint8_t *) i)+2));
}

void jlos_gdt_destroy(jlos_gdt_t* self)
{
}

uint16_t jlos_gdt_data_segment_selector(jlos_gdt_t* self)
{
    return (uint8_t*)&self->m_data_segment_selector - (uint8_t*)self;
}

uint16_t jlos_gdt_code_segment_selector(jlos_gdt_t* self)
{
    return (uint8_t*)&self->m_code_segment_selector - (uint8_t*)self;
}