#ifndef __GDT_H
#define __GDT_H

#include <common/types.h>

typedef struct {
    uint16_t m_limit_lo;
    uint16_t m_base_lo;
    uint8_t m_base_hi;
    uint8_t m_type;
    uint8_t m_flags_limit_hi;
    uint8_t m_base_vhi;
} __attribute__((packed)) jlos_gdt_segment_descriptor_t;

typedef struct {
    jlos_gdt_segment_descriptor_t m_null_segment_selector;
    jlos_gdt_segment_descriptor_t m_unused_segment_selector;
    jlos_gdt_segment_descriptor_t m_code_segment_selector;
    jlos_gdt_segment_descriptor_t m_data_segment_selector;
    jlos_gdt_segment_descriptor_t m_user_code_segment_selector;
    jlos_gdt_segment_descriptor_t m_user_data_segment_selector;
    jlos_gdt_segment_descriptor_t m_tss_segment_selector;
} __attribute__((packed)) jlos_gdt_t;

void jlos_gdt_init();
void jlos_gdt_destroy(jlos_gdt_t* self);
uint16_t jlos_gdt_code_segment_selector(jlos_gdt_t* self);
uint16_t jlos_gdt_data_segment_selector(jlos_gdt_t* self);

uint16_t jlos_gdt_user_code_segment_selector(jlos_gdt_t *self);
uint16_t jlos_gdt_user_data_segment_selector(jlos_gdt_t *self);
uint16_t jlos_gdt_tss_selector(jlos_gdt_t *self);
void jlos_gdt_set_tss(jlos_gdt_t *self, uint32_t base, uint32_t limit);

void jlos_gdt_segment_descriptor_init(jlos_gdt_segment_descriptor_t* self, uint32_t m_base, uint32_t limit, uint8_t m_flags);
uint32_t jlos_gdt_segment_descriptor_base(jlos_gdt_segment_descriptor_t* self);
uint32_t jlos_gdt_segment_descriptor_limit(jlos_gdt_segment_descriptor_t* self);

jlos_gdt_t *jlos_gdt_get_kernel(void);

#endif