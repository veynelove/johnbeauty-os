#include <arch/x86/cpu_state.h>
#include <hal/cpu_state.h>

bool jlos_cpu_state_is_user_mode(jlos_cpu_state_t *s)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)s;
    return (r->m_cs & 3) != 0;
}

void jlos_cpu_state_init(jlos_cpu_state_t *s)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)s;
    r->m_eax = 0;
    r->m_ebx = 0;
    r->m_ecx = 0;
    r->m_edx = 0;
    r->m_esi = 0;
    r->m_edi = 0;
    r->m_ebp = 0;
    r->m_error = 0;
    r->m_padding = 0;
    r->m_eip = 0;
    r->m_cs = 0;
    r->m_eflags = 0x200;
    r->m_user_esp = 0;
    r->m_user_ss = 0;
}

uint32_t jlos_cpu_state_get_syscall_num(jlos_cpu_state_t *s)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)s;
    return r->m_eax;
}
void jlos_cpu_state_set_retval(jlos_cpu_state_t *s, int32_t val)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)s;
    r->m_eax = val;
}

void jlos_cpu_state_record_user_stack(jlos_cpu_state_t *curr_cpu, jlos_cpu_state_t *cpu)
{
    jlos_x86_regs_t *curr_c = (jlos_x86_regs_t *)curr_cpu;
    jlos_x86_regs_t *c = (jlos_x86_regs_t *)cpu;
    curr_c->m_user_esp = (uint32_t)c + 48;
    curr_c->m_user_ss = 0;
}