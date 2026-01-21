#include <kernel/multitask.h>

namespace JLOS {
namespace Kernel {
task::task(Kernel::global_descriptor_table *gdt, void entrypoint())
{
     cpustate = (cpu_state *)(stack + 4096 - sizeof(cpu_state));
     cpustate->m_eax = 0;
     cpustate->m_ebx = 0;
     cpustate->m_ecx = 0;
     cpustate->m_edx = 0;

     cpustate->m_esi = 0;
     cpustate->m_edi = 0;
     cpustate->m_ebp = 0;

     cpustate->m_eip = (uint32_t)entrypoint;
     cpustate->m_cs = gdt->code_segment_selector();
     cpustate->m_eflags = 0x202;
}

task::~task(){}

task_manager::task_manager() : m_num_tasks(0), m_current_task(-1){}

task_manager::~task_manager(){}

bool task_manager::add_task(task *task)
{
     if (m_num_tasks >= 256) {
          return false;
     }
     tasks[m_num_tasks++] = task;
     return true;
}

cpu_state *task_manager::schedule(cpu_state *cpustate)
{
     if (m_num_tasks <= 0) {
          return cpustate;
     }
     if (m_current_task >= 0) {
          tasks[m_current_task]->cpustate = cpustate;
     }
     if (++m_current_task >= m_num_tasks) {
          m_current_task %= m_num_tasks;
     }
     return tasks[m_current_task]->cpustate;
}
}
}