#ifndef __JLOS_ARCH_X86_TSS_H
#define __JLOS_ARCH_X86_TSS_H

#include <common/types.h>

typedef struct {
    uint16_t m_back_link;
    uint16_t m_reserved0;
    uint32_t m_esp0;
    uint16_t m_ss0;
    uint16_t m_reserved1;
    uint32_t m_esp1;
    uint16_t m_ss1;
    uint16_t m_reserved2;
    uint32_t m_esp2;
    uint16_t m_ss2;
    uint16_t m_reserved3;
    uint32_t m_cr3;
    uint32_t m_eip;
    uint32_t m_eflags;
    uint32_t m_eax;
    uint32_t m_ecx;
    uint32_t m_edx;
    uint32_t m_ebx;
    uint32_t m_esp;
    uint32_t m_ebp;
    uint32_t m_esi;
    uint32_t m_edi;
    uint16_t m_es;
    uint16_t m_reserved4;
    uint16_t m_cs;
    uint16_t m_reserved5;
    uint16_t m_ss;
    uint16_t m_reserved6;
    uint16_t m_ds;
    uint16_t m_reserved7;
    uint16_t m_fs;
    uint16_t m_reserved8;
    uint16_t m_gs;
    uint16_t m_reserved9;
    uint16_t m_ldt_segment;
    uint16_t m_reserved10;
    uint16_t m_trap_flag;
    uint16_t m_io_map_base;
} __attribute__((packed)) jlos_x86_tss_t;

void jlos_x86_tss_init(jlos_x86_tss_t *self, uint32_t esp0, uint16_t ss0);
void jlos_x86_tss_load(jlos_x86_tss_t *self, uint16_t selector);

#endif
