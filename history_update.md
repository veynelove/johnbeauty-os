# JLOS 开发历史日志（history_update.md）

> 从 update_plan.md 迁出的历史记录：完整开发日志（v1.0 ~ v3.0）+ v2.3 及以前的历史版本档案。
> 当前计划与待办以 update_plan.md 为准，本文件仅用于追溯。

***
## 历史版本（v2.3 及以前，保留参考）

### 已完成里程碑（历史）

| 模块       | 完成项                                                                                               | 验证                                      |
| ---------- | ---------------------------------------------------------------------------------------------------- | ----------------------------------------- |
| 虚拟内存   | 3:1 高半核分页 + 内核/用户地址空间隔离 + 页表深拷贝                                                  | 动态映射，ring3 用户进程正常运行          |
| 内存管理   | 动态物理内存探测（GRUB mmap） + 页帧分配器 + 低内存堆(0x50000) + 主堆(动态 4\~64MB) + 16字节最小分配 | 支持不同物理内存大小，MMIO 保留区自动标记 |
| 系统调用   | exit/fork/read/write/get\_errno/get\_pid/yield/sleep/wait\_pid/brk/pipe/fd\_close                    | per-thread errno + wait\_pid 退出码       |
| 多任务调度 | MLFQ 四级反馈队列 + 老化升级 + 抢占/协作可切换                                                       | 时间片轮转正常，交互型优先                |
| 同步原语   | 信号量 + 互斥锁(可重入+所有权传递) + 条件变量                                                        | spinlock 关中断保护                       |
| IPC        | 管道(堆分配环形缓冲+引用计数) + 消息队列(柔性数组)                                                   | FIFO 顺序，close/destroy 分离             |
| 用户进程   | ring3 用户态进程 + 用户栈(64KB多页)映射 + TSS 特权级切换 + fork 用户栈复制                           | Hello from ring3 + Wake up 验证通过       |

### 已解决问题（历史）

| 编号 | 问题                              | 解决版本 | 修复方式                                                   |
| ---- | --------------------------------- | -------- | ---------------------------------------------------------- |
| S1   | syscall\_write printf 格式化漏洞  | v2.2     | 改为 `printk("%s", buf)`，格式化符不再被解释               |
| S3   | fd < 3 魔法数字判断               | v2.2     | 改为 `fd_entry->type == JLOS_TASK_FD_CONSOLE` 显式类型判断 |
| S4   | jlos\_task\_fd\_close pipe 未实现 | v2.2     | 补充引用计数递减 + destroy 完整逻辑                        |
| M1   | 用户栈固定单页（4KB）             | v2.2     | 扩展为 64KB（`JLOS_TASK_USER_STACK_SIZE = 0x10000`）       |
| M2   | fork 用户栈未复制                 | v2.2     | 子进程分配新物理帧 + memcpy 用户栈内容                     |
| —    | 栈切换导致 multiboot 参数丢失     | v2.2     | .boot 段全局变量保存参数，不受切栈和 BSS 清零影响          |
| —    | 连续物理帧分配不验证碎片          | v2.2     | reserve\_bulk 重写为遍历 bitmap 找真正连续空闲帧           |
| —    | 页帧分配器无 OOM 防护             | v2.2     | init\_main 逐级回退 heap\_size，极端情况 halt              |

### 开发日志

#### 2026-09-21（v3.0）

- **Phase 8: 文件系统 + ELF 加载器 + execve + 用户态程序 + SSE2 优化 全部完成**

- **F4 FAT32 文件系统**：VFS 抽象层（inode/dentry/super\_block/file/fs\_type/mount + ops 向量 + hash 缓存 + 路径解析）+ FAT32 驱动（完整 BPB + 短目录项 + FAT 链遍历 + cluster 读写）+ MBR 分区层（4 主分区解析 + LBA 偏移封装）+ 块设备抽象（hal/block.h/c ops 表 + 不透明 priv[64]）+ dsa 补充（hash\_chain/bitmap/ringbuf）+ rootfs init（JLOS\_INITCALL\_LATE 级自动挂载）。

- **ELF 加载器**：ELF32 header + program header + i386 校验 + PT\_LOAD 按页映射 + VMA 计入 + 文件内容拷贝。用户地址布局：0x08048000 ELF 加载区 + brk 堆 + 0xBFFFF000 用户栈顶 + 0xC0000000 内核空间。

- **execve 系统调用（方案 A）**：经典做法——复用内核栈，修改 trapframe（eip/cs/user\_esp/user\_ss/eflags），正常 return 由 iret 到新程序。全局 `g\_hal\_syscall\_trapframe` 在 syscall entry 设置/清除。`jlos\_cpu\_state\_set\_user\_entry` 修改 trapframe。

- **argv/envp 栈布局**：System V ABI x86 32-bit 经典栈布局（环境字符串 → 参数字符串 → NULL → envp\[\] → NULL → argv\[\] → argc）。`syscall\_copy\_strings` 从用户空间拷贝 argv/envp + `jlos\_exec\_setup\_user\_stack` 写入用户栈。

- **用户态子项目 jlcy/**：独立子项目与内核隔离。crt0.S 汇编入口（\_start 从 esp 取 argc/argv → call main → exit syscall）+ user\_syscall.c（int $0x80 stub）+ syscall\_abi.h（syscall 号 #define，C 和汇编通用，\_\_ASSEMBLY\_\_ 保护）+ hello.c（int main(int argc, char \*\*argv) 纯 C，get\_pid + printf + return 7）+ user\_linker.ld（0x08048000 经典基址）。构建集成：根 Makefile → jlcy/user/hello.elf → FAT32 镜像 → VMDK → 内核 rootfs 挂载后 execve。

- **MM-5 SSE2 优化**：weak/strong 链接模式（common/types.c 标 weak，arch/x86/lib/memset.s + memcpy.s strong 覆盖）+ SSE2 32B 宽写（movdqa/movdqu + pshufd 广播 + 32B 循环）+ CR4.OSFXSR 安全检查（启动早期未置位时回退字节循环）+ CR0.TS 安全模式（保存 CR0 → clts → 保存 xmm0 → 操作 → 恢复）+ movdqu 栈保存（32 位栈不保证 16B 对齐）+ 小尺寸阈值（< 64B 字节循环）。

- **调试过程**：SSE2 初始版本两次崩溃——① `/* */` 注释 + 无 `l` 后缀导致 `$` 立即数解析错误（改 `#` 注释 + `l` 后缀）；② vCPU triple fault——CR4.OSFXSR 未检查（启动早期 #UD）+ movdqa 栈保存未对齐（#GP）。修复后 ALL PASSED。

- **A1 遗留解决**：用户态代码与内核混编问题通过 jlcy/ 独立子项目 + ELF 加载器 + execve 彻底解决，用户态 ELF 独立链接 0x08048000。

- **验证**：memory/pfa/paging/multitask ALL PASSED（ring3: exited=1, exit\_code=7, argc=1, argv[0]=/hello.elf）。

- 更新计划至 v3.0

#### 2026-09-18（v2.12）

- **HAL 层架构合规重构完成**：分析 hal/ 33 个文件，识别 18 个问题，分 4 批修改全部完成。

- **分层模式判定**：确立 A（hal 声明 + arch 定义）、B（ops 表转发）、B2（实例注册链表 + rating 选优）三种模式。判定关键是"是否需要运行时多实现共存"。多架构不构成 B/B2 理由（编译期链接选不同 arch 目录），多核只影响 timer/irq。

- **第 1 批局部修复**：dma.c count 端口 bug + 死代码 + 重复 OR；serial.c UART 命名；pci.h 0x80000000u 宏化；block.h/c ATA28 LBA 魔数宏化；device.h/c mmio owner 改 char[24] + jlos_strlcpy；display.c 花括号统一。

- **第 2 批职责归属**：新建 hal/kernel_syscall.c 移出 4 个 syscall 全局变量；hal.c 自持 s_hal_info + jlos_hal_info_get_for_init() 替代 g_hal_info extern；新建 hal/hal_arch.h 拆出 arch 注入函数声明（11 个文件加 include）；user_syscall 移到 user/ 目录 + Makefile 加 user。

- **第 3 批下沉 arch**：timer B2（hal/timer.h 定义 jlos_timer_device + dsa/list 链表注册 + rating 选优，arch/x86/pit.c SUBSYS 级注册）；serial A（hal/serial.h 纯声明，删 hal/serial.c，新建 arch/x86/uart_8250.c）；dma A（hal/dma.h 纯声明，删 hal/dma.c，新建 arch/x86/dma_8237.c）；ext_state A（hal/ext_state.h 自定义 raw[512]+used 结构，arch/x86/fpu.c 改用 raw，删 arch/x86/fpu_state.h）。

- **第 4 批 PCI 分层 + block 去 drivers**：PCI A（hal/pci.h 自定义类型 + 不透明 controller，hal/pci.c 只留 read16/8/write16/8 通用逻辑 + initcall，arch/x86/pci.h 定义 controller 结构，arch/x86/pci.c 实现全部 arch 部分，函数名统一 jlos_hal_pci_ 前缀，drivers/amd_am79c973 参数类型跟随改名）；block C1（hal/block.h 去掉 #include drivers/ata.h，union dev_priv 改 uint8_t priv[64] + _Static_assert，hal/block.c 用 block_ata() helper 访问）。

- **PCI read32/write32 由 arch 定义的理由**：read32 是"发起硬件访问"原语（地址编码 + 端口操作整体 arch 特有），read16/8 是"在结果上做位操作"通用算法（调 read32 再移位，所有 arch 复用，避免 DRY 违反）。

- **验证**：编译通过 + 全测试 PASSED（memory/pfa/paging/multitask + 网络栈 + timer 选优生效）。12 项遗留检查全部通过。

- 更新计划至 v2.12

#### 2026-09-17（v2.11）

- **Initcall 机制完成**：新增 `kernel/initcall.h` + `kernel/initcall.c`，5 级 initcall（CORE/SUBSYS/DEVICE/LATE/POST），两层宏展开（`__JLOS_INITCALL` + `JLOS_INITCALL`）先展开 level 再字符串化。`linker.ld` 中 `.initcall0`~`.initcall4` 段替代 `.init_array`，`ALIGN(4)` 非 `ALIGN(4K)` 避免 BSS LMA 不连续。`john_beauty_main()` 从手动调用各子系统 init 改为 `jlos_do_initcalls()` 一行，`loader.s` 删除 `call_constructors` 调用。10 个子系统按依赖级别自动注册。

- **Framebuffer Console 完成**：`loader.s` 启用 `MULTIBOOT_VIDEO_MODE`（1024×768×32），`common/multiboot.h` 添加 `jlos_vbe_mode_info_t`。`arch/x86/font_8x16.h`/`.c` 256 字符 × 16 字节点阵。`arch/x86/fb_console.c` 完整实现 putc\_at/clear/scroll\_up/erase\_cursor/draw\_cursor/get\_info/invert\_at，软件光标 erase/draw 分离模式（反转字符底部 2 行），鼠标光标反转整个字符块（`visible` 标志防首次 erase）。framebuffer 映射用 write-back 缓存（去掉 CACHE\_DISABLE）。`hal/display.h` 中 `GRAPHIC` → `FRAMEBUFFER`，`set_hw_cursor` → `erase_cursor` + `draw_cursor`。`kernel/console.c` 改为 erase/draw 光标模式 + `\b` 处理 + info 预填默认值 + `display_device_init` initcall（DEVICE 级）。

- **输入子系统迁移**：`tools/samples/debug_console.c` → `drivers/input/input.c`，键盘/鼠标驱动注册 + 事件回调，`JLOS_INITCALL_DEVICE` 自动注册。删除 `KERNEL_CONFIG_DEBUG_CONSOLE` 和 `KERNEL_CONFIG_DEBUG_NETWORK` 宏，删除原 `debug_console.c/.h`。

- **Bug 修复**：① keyboard.c 添加 Backspace scancode 0x0E（原缺失，按键被丢弃）；② interrupts.c 删除冗余 `handles[i] = NULL` 清零（BSS 已为零，清零覆盖已注册 handler）；③ syscall.c initcall 从 SUBSYS 改为 DEVICE（依赖 IRQ manager 先初始化）；④ multitask.c VMA end 从 `stack_base + stack_size` 改为 `JLOS_TASK_USER_STACK_TOP + JLOS_PAGE_FRAME_SIZE`（`[start,end)` 语义不含栈顶）。

- **方向变更**：原计划阶段 A（VGA 文本模式三层重构 + 日志环形缓冲 + 键盘翻页 + 硬件光标 + 串口宏开关）改为直接实现 framebuffer console（原阶段 B 内容）。VBE framebuffer 与 VGA 文本互斥，一旦 loader.s 请求 VIDEO\_MODE，0xB8000 文本显存失效，因此直接跳到 framebuffer 方案。日志环形缓冲 + 键盘翻页 + 串口宏开关仍为后续待办。

- 全流程验证通过：memory/pfa/paging/multitask ALL PASSED + http/udp server 正常启动

- 更新计划至 v2.11

#### 2026-09-13（v2.10）

- **N1 VMA/mmap 阶段1 完成**：brk + stack 两段硬编码区间统一成 VMA 区间，page\_fault/fork/destroy 改为 VMA 驱动；mmap 只留接口（VMA file/offset 字段 + MMAP/MUNMAP syscall 编号 + stub）。brk 字段从 task 冗余副本统一到 mm；栈首页预映射删除改走 demand paging。

- **Manager 全局化**：7 个 manager（low/main memory\_manager、task\_manager、interrupt\_manager、driver\_manager、pci\_controller、syscall\_handler）从 kernel.c 栈上局部变量改为各自 .c 文件 static 全局变量，init 函数去 self 参数，kernel.c 从 133 行精简到 119 行。

- **修复 memory\_manager bug**：`jlos_memory_manager_init_main` 曾调用操作 `s_low_memory_manager` 的 `jlos_memory_manager_init`，导致 `s_main_memory_manager` 从未初始化；提取 `mm_init` static 内部函数解决。

- **2 个编译警告**：`.note.GNU-stack`（每个 .s 末尾加 `.section .note.GNU-stack,"",%progbits`）+ LINKER RWX LOAD segment（linker.ld 加 PHDRS 分离 boot/code RX 与 data RW）。

- **红黑树 DSA 组件**：新增 `dsa/rbtree.h` + `dsa/rbtree.c`（CLRS 风格，哨兵 nil 节点，侵入式 container\_of），5 项测试全 PASSED。

- **P3 fork/COW 遍历优化**：新增 `jlos_paging_cow_range` 接口（paging.c），src/dst 各加一次锁、一遍遍历完成 refcount\_inc + 映射 dst 为 COW + 改 src 为 COW，替代 `jlos_mm_clone_user` 内层逐页 `get_physical_addr`+`map` 的 2N 次加锁；锁 2N+1→2，遍历 3 遍→1 遍。multitask test 3（fork 10 children）/test 4（ring3 smoke）ALL PASSED。

#### 2026-09-03（v2.9）

- **multitask 字段冗余清理 + 3 个调度稳定性 bug 修复**（解决"首次运行卡死、二次通过"的调度顺序敏感问题, 两次运行 ALL PASSED 稳定）

- **冗余字段/死代码清理**（经典结构优先, 删冗余不允许）:

  - 删除 `jlos_task_t.fds_size` 字段: 边界检查全用 `JLOS_TASK_FDS_NUM` 常量, 该字段仅写不读（唯一"读"处 `i < 3 && i < self->fds_size` 是恒真冗余判断）

  - 删除 `jlos_task_set_blocked`/`jlos_task_set_waiting` 中 `t->yield = true` 死赋值: 两函数同时改 status 为 BLOCKED/WAITING, schedule 读 yield 的唯一位置在 `if (prev->status == JLOS_TASK_RUNNING)` 分支内, BLOCKED/WAITING 进不去, 永远不会被读到

  - `yield` 字段本身保留: `syscall_yield` 系统调用 + `jlos_syscall_need_resched` 检查 + schedule 降优先级判断均真实使用

  - 文件: kernel/multitask.h, kernel/multitask.c

- **Bug 1: slot\_idx 维护 UAF（卡死根因之一）**:

  - 根因: `jlos_task_free` 用"末尾任务搬到被删位置"的紧凑删除, 但**没更新被搬任务的 slot\_idx** → 该任务 slot\_idx 失效 → 下次 `schedule` 用失效 slot\_idx 索引 → `tasks[idx] != next` → 走 O(n) fallback → 若再 free 该任务, idx 越界写入错误位置 → tasks\[] 出现 stale 指针 → UAF

  - 场景验证: `tasks=[idle(s=0), A(s=1), B(s=2), C(s=3)]`, free(A) 后 C 搬到 \[1] 但 C->slot\_idx 仍=3; free(C) 时 idx=3 越界, tasks\[3]=tasks\[2]=B, tasks\[1] 仍指向已 free 的 C → UAF

  - 修复: 加 `tasks[idx]==task` 校验（防失效）+ 搬移后 `tasks[idx]->slot_idx = idx`（维护被搬任务 slot\_idx）

  - 文件: kernel/multitask.c

- **Bug 2: rq\_dequeue** **`__builtin_ctz(0)`** **UB（卡死根因之二）**:

  - 根因: `int l = __builtin_ctz(self->rq_nonempty); if (!self->rq_nonempty) return NULL;` —— `__builtin_ctz(0)` 是未定义行为, GCC 在 UB 假设下可能优化掉后面的判空检查 → runqueue 真空时 ctz 返回垃圾值 → `self->rq[l]` 越界 → `first=&head`（自指）→ `jlos_list_del(head)` 删 head 自己 → runqueue 链表彻底损坏 → 后续 rq\_dequeue 返回野指针 → schedule `return cpustate` 不切换 → ZOMBIE prev 恢复 halt → 死循环卡死

  - 修复: 调换顺序, 先 `if (!rq_nonempty) return NULL;` 再 `__builtin_ctz`

  - 文件: kernel/multitask.c

- **Bug 3: fork 浅拷贝链表节点（潜在链表损坏）**:

  - 根因: `jlos_process_fork` 中 `*child = *parent` 整 struct 浅拷贝, 复制了 parent 的 `rq_node` 和 `zombie_node`（指向 parent 的节点地址, 非 child 自己）, 但只重置了 `pid_hash_node`, 没重置 `rq_node`/`zombie_node`

  - 后果: child->rq\_node.next = \&parent->rq\_node → `task_on_rq(child)` 在 enqueue 前误判 true; child->zombie\_node.next = \&parent->zombie\_node → `jlos_task_free` 误判 child 在 zombie 链表执行 list\_del（虽然 list\_del 对自指态 no-op, 但语义错误, 且 parent 之后真进 zombie 链表时 child 的 stale 指针会干扰）

  - 修复: `*child = *parent` 后补 `jlos_list_init(&child->rq_node); jlos_list_init(&child->zombie_node);`

  - 文件: kernel/multitask.c

- 验证: 两次运行 multitask ALL PASSED (TEST1-4 全 PASS), 首次运行卡死问题根除, 调度顺序敏感性消除

- 更新计划至 v2.9

#### 2026-09-01（v2.8）

- **多任务测试基线全部打通**：MEMORY ALL PASSED + MULTITASK ALL PASSED (TEST1-4 全 PASS)

- **TEST 3 fork 子进程页错误链修复**（5 问题串联 → 经典 Linux `copy_thread` + `ret_from_fork` 范式）:

  - 根因 1: K 值用 timer IRQ esp snapshot 估算, C 帧层不确定 100\~320B → 改 `movl %esp,%ecx` 在 fork\_stub 首句抓硬件 esp 作锚点

  - 根因 2: ebx 目标指向 outer return → 栈错位/寄存器覆盖 → ebx = `fork_resume_pc` (asm label1)

  - 根因 3: eflags 条件设 IF=0 → -O2 分支误判死锁 → 无条件硬编码 `0x10246` (IF=1, IOPL=0)

  - 根因 4: 5 个 epilogue slot 未初始化 → retpc=0 triple fault → 显式写 20B slot

  - 根因 5: 未 memcpy 全部 56B cpustate → 9 pop 读到父栈残留 → `__builtin_memcpy(onstack, child_cpustate, 56)`

  - 终极方案: `K_DEAD = -24` (6 args×4 + call retaddr×4 − addl$24 清栈 4B = 0 方差硬编码) + 跳板 `mov $0,%eax; jmp *%ebx` (3 字节窗口 μs 级 IRQ 物理上不可插入)

  - 文件: arch/x86/fork\_stub.c, arch/x86/context\_switch.c, kernel/multitask.c

- **TEST 4 buddy 双重分配修复** (经典 Linux/xv6 顺序构造):

  - 根因: buddy 初始化反向扫描 → 同一物理页入多个 free list → 重叠分配

  - 修复: 经典顺序构造 — 先全插 order-0, 再逐层合并向上

  - 文件: kernel/page\_frame\_allocator.c, kernel/memory\_manager.c

- **TEST 3 父进程 busy-wait 死锁修复** (经典 wait/wake 阻塞语义):

  - 根因: 父 busy-wait budget loop 占满 20ms slice, 子进程饿死无法 exit

  - 修复: 父 `jlos_task_set_waiting(child_pid)` 主动阻塞, 调度器子 ZOMBIE 时自动唤醒父

  - 文件: tools/tests/multitask\_te.c

- **PF handler 简化** (按需分页, 非临时方案):

  - 用户栈 demand paging 是设计内行为, 5 类 PF 日志 (\[PF]/walk/dec/brkmapped/ustkmapped) 啰嗦

  - 精简为只留 `[PF] addr/err/user/ip/pid` 一行总览, 删 walk/dec/mapped 全部

  - 文件: kernel/paging.c

- **测试代码格式统一 + 架构合规**:

  - memory\_te.c: 统一 K\&R 大括号风格, 删冗余空行, 简化日志上下文标签

  - multitask\_te.c: 删除裸写汇编的 `JLOS_INLINE_FORK` 宏, 改用 HAL `jlos_arch_fork_invoke` 接口 (父返回 child\*, 子返回 NULL, 调用方据此判 is\_child); 保留 omit-frame-pointer (子栈 memcpy 父 ebp 指向父 kstack, 必须 esp 寻址)

- **HAL 层架构合规重构** (HAL 不含汇编, 汇编全在 arch/):

  - 删除 hal/cpu\_state.h 中 `JLOS_ARCH_READ_CURRENT_STACK_PTR()` 宏 (含 `movl %%esp` x86 asm, 无调用方死代码)

  - 删除 hal/context.h 中 `JLOS_ARCH_FORK_*_REG` 寄存器名宏 + `JLOS_FORK_DECL/CALL` 宏 + `jlos_arch_fork_peek_regs` 声明 (全无调用方死代码)

  - 删除 arch/x86/cpu\_state.c 中 `jlos_arch_fork_peek_regs` 实现 + `jlos_cpu_state_dump` 调试函数 (无调用方)

  - 精简 hal/cpu\_state.h 70 行 Pattern A 历史注释为简短接口说明

- **multitask 宏上提**: `JLOS_KERN_*` 边界校验宏从 multitask.c 函数体中间移到 multitask.h

- **PFA 调试代码清理**:

  - 删除 `[buddy-SELFCHECK]` 写后读回验证块 (纯调试, 防 slab\_page\_t/kv\_hdr 复用)

  - 精简 `[buddy-PANIC]` dump (删 32 字节 dump + 链表头 dump, 保留 1 行 panic)

  - 删除 `[pmalloc]` malloc 入口冗余 sanity check (buddy\_remove 已有 panic 检查)

  - 保留 `[buddy-OOB/ALIGN/ASSERT]` 经典 buddy sanity check + halt (等价 Linux BUG\_ON)

- **调试代码清理**:

  - 删除 context\_switch.c 中 RC10\~RC15/K 演化/new\_eip/new\_cs/new\_fl/test3\_ebp/method/p 死变量和调试历史注释

  - 删除 multitask.c 中 K\_PRIM/Pattern A 历史注释 + \[fk-OV] 重复注释

  - 删除 page\_frame\_allocator.c ★ 标记

  - 保留必要错误诊断: \[af-MEMCPY-OOB] / \[fk-OV] / \[buddy-\*] sanity check + halt

- **清理中间产物**: 4 个 `.bak_*` 备份文件 + `.forkrepro/` 反汇编调试目录

- 更新计划至 v2.8

#### 2026-08-23（v2.7）

- **F12 FPU/SSE 上下文切换全部完成（Lazy 模式）**：

  - HAL 抽象层：新增 `hal/ext_state.h`，用 opaque 类型 + 前向声明避免循环依赖，声明 4 个 hook（init/destroy/switch/trap\_body）；ARM/RISC-V 留 `#error` 占位

  - 状态结构：`arch/x86/fpu_state.h` 定义 `struct jlos_arch_ext_state`（512B 16B 对齐 fxsave\_area + used 标志）

  - 4 个 hook 实现（`arch/x86/fpu.c`）：

    - `ext_init`：拷贝干净模板到 task，used=false

    - `ext_destroy`：若 task 是 FPU owner 则清空所有权（锁保护）

    - `ext_switch`：仅置 CR0.TS（lazy 触发，不立即 save/restore）

    - `ext_trap_body`：#NM 处理，开头先 `clts` 防 GCC 生成 SSE 指令递归触发 #NM；owner 不变直接返回，否则 save 旧 owner + restore 新 owner

  - 干净模板 `build_clean_template`：`clts → fninit → ldmxcsr(0x1F80) → fxsave`，幂等（s\_init\_template\_done 守卫）

  - task struct 新增 `ext_state` 字段（16B 对齐），与 parent/children/next\_sibling/next\_hash 独立

  - 调度器接入：init\_1 调 ext\_init、task\_free 调 ext\_destroy、schedule 调 ext\_switch

  - IDT 注册：#NM(0x07) 和 #PF(0x0E) 都在 IRQ 向量转换前分发；均用 IDT\_INTERRUPT\_GATE（IF 自动清零，irqrestore 安全）

  - Boot 初始化：loader.s `CR0=PG|MP|NE(EM=0)`，`CR4=PSE|OSFXSR`

  - 编译选项：Makefile 加 `-mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx` 防编译器生成 SSE

  - spinlock 实现用 xchgl/pause/pushf，无 SSE 指令，CR0.TS=1 下安全

- 全流程验证通过：multitask\_test + ring3 用户进程 + 网络

- 更新计划至 v2.7

#### 2026-08-20（v2.6）

- **Phase 1 Paging 全部完成**：

  - Per-context spinlock（替代全局锁）

  - context\_clone 内核 PDE 浅拷贝（共享，不深拷贝）

  - context\_clone malloc 失败处理

  - map\_range 回滚用 unmap\_nolock 锁内完成

  - access\_ok 仅范围检查，删掉页表遍历

  - COW fault handler 一次锁内完成

  - fork COW 只遍历 present PDE

  - context\_destroy 用 used\_pde\_count 优化

- **Phase 2 Memory Manager 全部完成**：

  - 修复严重 Bug：free() prev 合并方向错误 → size 虚增 → 内存越界

  - 修复 Bug：free() next 合并后 self->tail 未更新

  - expand\_heap 从逐页 malloc 改为 reserve\_bulk + 批量 map

  - expand\_heap 分配后与 tail 空闲 chunk 合并

- **Phase 3 Multitask 全部完成**：

  - exit stub 修复：asm → C 函数 + cli/sti 原子写 ZOMBIE + 清空 g\_current\_task\_ptr 防野指针

  - pid hash table（256 桶，链地址法）：zombie parent/wait\_pid/WAITING 唤醒全部 O(1)

  - 设计决策：用 pid hash 替代 parent 指针 + children 链表（无悬空指针风险，当前无信号/ptrace 不需要 parent 指针）

  - task struct 仅加 1 个字段 `next_hash`（4 字节）

  - 确认 MT-7（exec 失败处理）和 MT-8（fork COW present PDE）已正确，无需改动

- 全流程验证通过：ring3 + fork COW + sleep/wake + zombie 清理 + 网络 + 多任务

- 更新计划至 v2.6

#### 2026-08-18（v2.5）

- Phase 0 Buddy PFA 全部完成：

  - Bootstrap Allocator (boot\_alloc) 按字节分配

  - 两阶段初始化 (boot\_alloc → paging → PFA init)

  - Buddy 分配器 MAX\_ORDER=10，空闲链表嵌入物理帧

  - mark\_occupied/refcount/buddy\_order 语义统一

  - reserve\_bulk mark\_reserve 循环变量 bug 修复 (frame+i)

- Linux 风格虚拟布局：

  - KERNEL\_DIRECT\_MAP\_SIZE=896MB (0x38000000)

  - KERNEL\_HEAP\_VIRT\_BASE=0xF8000000 (vmalloc 区)

  - init\_main 逐帧 malloc + paging\_map (物理散、虚拟连)

- 完整扫描 paging.c，更新 Phase 1：

  - 确认 6 项已完成 (PSE flag 保留 / TLB 策略 / boot\_alloc 集成 / Linux 布局 / 段权限 / access\_ok)

  - 发现 3 个新问题 (PAG-9 clone 失败静默 / PAG-10 双重 TLB / PAG-11 fork COW 遍历 3GB)

  - 修正 PAG-1 描述 (非 deadlock，是低效回滚)

  - 修正 PAG-4 严重度 (从 🟢 Med → 🔴 Critical，确认是深拷贝而非共享)

- 更新计划至 v2.5

#### 2026-08-13（v2.4）

- 全面审计 PFA / Paging / MemoryManager / Multitask 四个子系统

- 识别出 Phase 0-4 五级架构升级路线

- 确立 Buddy PFA 为最高优先级基础设施重构

- 确立 per-context lock、Zombie O(1) 清理、pid hash 等核心改进项

- 更新计划至 v2.4

#### 2026-08-11（v2.3）

- 完整系统扫描：52 个 C 文件 + 2 个汇编 + 50 个头文件全覆盖

- 发现并分类 5 个 P0 Bug、10 个 P1 代码质量问题、4 个 P2 性能问题、10 个 P3 功能缺失

- JLOS\_ARRAY\_LIMIT\_RANGE 宏 bug（% 优先级高于 +）

- fork brk 重置不完整：子进程 brk 区域物理页未复制

- exec 物理帧泄漏：unmap 后未释放

- Makefile grub.cfg 重复追加

- 制定 Phase 1\~5 实施路线图

- 更新计划至 v2.3

#### 2026-08-11（v2.2）

- 动态物理内存探测完成：GRUB mmap 解析 + MMIO 保留区自动标记 + 动态主堆大小（phys\_end/4，夹 4MB\~64MB）

- 栈切换 bug 修复：loader.s 用 .boot 段全局变量保存 multiboot 参数，防止切栈后参数丢失

- 临时页表扩展：从 16MB 扩展到 64MB，覆盖 GRUB 可能放置 mmap 的物理位置

- 连续物理帧分配重写：reserve\_bulk 改为遍历 bitmap 找真正连续空闲帧，防止 MMIO 区间碎片

- OOM 防护：init\_main 逐级回退 heap\_size，极端情况打印错误 + halt

- 系统调用三项修复：S1 printf 漏洞、S3 fd 类型判断、S4 pipe close 引用计数

- 用户栈扩展：从 4KB 改为 64KB（JLOS\_TASK\_USER\_STACK\_SIZE = 0x10000）

- 页表深拷贝：fork 时分配新页表，不再共享 PTE

- 更新计划至 v2.2

#### 2026-08-09

- 3:1 高半核重构完成：内核映射 0xC0000000，用户空间 0-3GB

- ring3 用户进程完整运行：TSS 特权级切换、用户栈映射、syscall wrapper

- 修复启动期 triple fault：16MB 启动页表、CR0.PG 序列化跳转、mbinfo 指针转换

- 修复 syscall handler 未注册、页故障错误码偏移、.rodata PTE\_USER

- 修复 jlos\_user\_puts 字符串指针被 access\_ok 拒绝（栈缓冲拷贝）

- 清理全部调试代码（串口标记、DBG 宏、临时函数）

- 更新计划至 v2.1

#### 2026-07-31

- 阶段三多任务系统增强完成：MLFQ 调度器、状态机、同步原语、IPC 全部实现

- 全项目结构体成员 m\_ 前缀去除重构完成（71 文件）

- 主堆扩展至 32MB，分页映射扩展至 256MB

- per-thread errno + wait\_pid 退出码实现

- 更新计划至 v2.0

#### 2026-07-01

- 网络栈全部打通：ARP/IPv4/ICMP/UDP/TCP/HTTP 全部验证通过

- TCP 序列号三次握手对齐 Bug 修复完成，curl -v 原生 HTTP/1.1 200 OK

#### 2026-06-23

- 初始化优化计划

- 启动阶段一：虚拟内存管理

