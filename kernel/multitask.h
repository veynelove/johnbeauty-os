#ifndef __JLOS__KERNEL_MULTITASK_H
#define __JLOS__KERNEL_MULTITASK_H

#include <common/types.h>
#include <kernel/gdt.h>

namespace JLOS {
namespace Kernel {
struct cpu_state {
     uint32_t m_eax;
     uint32_t m_ebx;
     uint32_t m_ecx;
     uint32_t m_edx;

     uint32_t m_esi;
     uint32_t m_edi;
     uint32_t m_ebp;

     // uint32_t gs;
     // uint32_t fs;
     // uint32_t es;
     // uint32_t ds;

     uint32_t m_error;

     uint32_t m_eip;
     uint32_t m_cs;
     uint32_t m_eflags;
     uint32_t m_esp;
     uint32_t m_ss;
} __attribute__((packed));

class task {
friend class task_manager;
private:
     uint8_t stack[4096]; //4KiB
     cpu_state *cpustate;

public:
     task(Kernel::global_descriptor_table *gdt, void entrypoint());
     ~task();
};
class task_manager {
private:
     task *tasks[256];
     int m_num_tasks;
     int m_current_task;

public:
     task_manager();
     ~task_manager();
     bool add_task(task *task);
     cpu_state *schedule(cpu_state *cpustate);
};
}
}
#endif
