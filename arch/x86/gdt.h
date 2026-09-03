#ifndef _GDT_H
#define _GDT_H

#include <common/types.h>

typedef struct {
    uint16_t limit_lo;
    uint16_t base_lo;
    uint8_t base_hi;
    uint8_t type;
    uint8_t flags_limit_hi;
    uint8_t base_vhi;
} __attribute__((packed)) jlos_gdt_segment_descriptor_t;

typedef struct {
    jlos_gdt_segment_descriptor_t null_segment_selector;
    jlos_gdt_segment_descriptor_t unused_segment_selector;
    jlos_gdt_segment_descriptor_t code_segment_selector;
    jlos_gdt_segment_descriptor_t data_segment_selector;
    jlos_gdt_segment_descriptor_t user_code_segment_selector;
    jlos_gdt_segment_descriptor_t user_data_segment_selector;
    jlos_gdt_segment_descriptor_t tss_segment_selector;
} __attribute__((packed)) jlos_gdt_t;

void jlos_gdt_init();

uint16_t jlos_gdt_code_segment_selector(jlos_gdt_t* self);
uint16_t jlos_gdt_data_segment_selector(jlos_gdt_t* self);

uint16_t jlos_gdt_user_code_segment_selector(jlos_gdt_t *self);
uint16_t jlos_gdt_user_data_segment_selector(jlos_gdt_t *self);
uint16_t jlos_gdt_tss_selector(jlos_gdt_t *self);
void jlos_gdt_set_tss(jlos_gdt_t *self, uint32_t base, uint32_t limit);

void jlos_gdt_segment_descriptor_init(jlos_gdt_segment_descriptor_t* self, uint32_t base, uint32_t limit, uint8_t flags);
uint32_t jlos_gdt_segment_descriptor_base(jlos_gdt_segment_descriptor_t* self);
uint32_t jlos_gdt_segment_descriptor_limit(jlos_gdt_segment_descriptor_t* self);

jlos_gdt_t *jlos_gdt_get_kernel(void);

#endif