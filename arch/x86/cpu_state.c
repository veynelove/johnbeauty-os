#include <arch/x86/cpu_state.h>
#include <hal/cpu_state.h>

bool jlos_cpu_state_is_user_mode(jlos_cpu_state_t *s)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)s;
    return (r->cs & 3) != 0;
}

void jlos_cpu_state_init(jlos_cpu_state_t *s)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)s;
    r->eax = 0; r->ebx = 0; r->ecx = 0; r->edx = 0;
    r->esi = 0; r->edi = 0; r->ebp = 0;
    r->error = 0; r->padding = 0; r->eip = 0; r->cs = 0;
    r->eflags = 0x200; r->user_esp = 0; r->user_ss = 0;
}

uint32_t jlos_cpu_state_get_syscall_num(jlos_cpu_state_t *s)
{
    return ((jlos_x86_regs_t *)s)->eax;
}

void jlos_cpu_state_set_retval(jlos_cpu_state_t *s, int32_t val)
{
    ((jlos_x86_regs_t *)s)->eax = val;
}

void jlos_cpu_state_record_user_stack(jlos_cpu_state_t *curr_cpu, jlos_cpu_state_t *cpu)
{
    jlos_x86_regs_t *curr_c = (jlos_x86_regs_t *)curr_cpu;
    curr_c->user_esp = (uint32_t)cpu + 48;
    curr_c->user_ss  = 0;
}
