# JohnBeauty OS 内核优化计划

版本: v2.3 | 日期: 2026-08-11 | 作者: JohnLove

---

## 优化原则

1. 核心功能优先;
2. 优化时，选择经典做法优先;

---

## 已完成里程碑

| 模块 | 完成项 | 验证 |
|------|--------|------|
| 网络驱动 | AMD AM79C973 初始化（CSR/BCR 配置、描述符环、IRQ 处理） | ERR=0，收发稳定 |
| ARP | 请求/响应 + 非阻塞查找缓存 | ARP 缓存正常更新 |
| IPv4 | 路由 + 校验和 + 收发封装 | 正常收发 |
| ICMP | Echo Request/Reply（完整 payload 校验） | ping 可正常工作 |
| UDP | 非阻塞 send/recv + Socket 管理 | 收发正常，无 ERR=1 循环 |
| TCP | 完整状态机 + 三次握手/四次挥手 + SYN/FIN 序列号处理 | curl 直连成功 |
| HTTP | HTTP/1.1 响应 + Content-Length + 正确 header | curl -v 原生解析 200 OK |
| 虚拟内存 | 3:1 高半核分页 + 内核/用户地址空间隔离 + 页表深拷贝 | 动态映射，ring3 用户进程正常运行 |
| 内存管理 | 动态物理内存探测（GRUB mmap） + 页帧分配器 + 低内存堆(0x50000) + 主堆(动态 4~64MB) + 16字节最小分配 | 支持不同物理内存大小，MMIO 保留区自动标记 |
| 系统调用 | exit/fork/read/write/get_errno/get_pid/yield/sleep/wait_pid/brk/pipe/fd_close | per-thread errno + wait_pid 退出码 |
| 多任务调度 | MLFQ 四级反馈队列 + 老化升级 + 抢占/协作可切换 | 时间片轮转正常，交互型优先 |
| 同步原语 | 信号量 + 互斥锁(可重入+所有权传递) + 条件变量 | spinlock 关中断保护 |
| IPC | 管道(堆分配环形缓冲+引用计数) + 消息队列(柔性数组) | FIFO 顺序，close/destroy 分离 |
| 用户进程 | ring3 用户态进程 + 用户栈(64KB多页)映射 + TSS 特权级切换 + fork 用户栈复制 | Hello from ring3 + Wake up 验证通过 |

---

## 优化总览

| 阶段 | 模块 | 优先级 | 状态 |
|------|------|--------|------|
| 一 | 虚拟内存管理 | 高 | 已完成，持续优化 |
| 二 | 系统调用接口 | 高 | 已完成核心，持续扩展 |
| 三 | 多任务系统增强 | 高 | 已完成核心，持续扩展 |
| 四 | 文件系统完善（FAT32） | 中 | 待开始 |
| 五 | 网络驱动优化 | 中 | 部分完成 |
| 六 | 高级特性 | 低 | 待开始 |

---

## 已解决问题

| 编号 | 问题 | 解决版本 | 修复方式 |
|------|------|----------|----------|
| S1 | syscall_write printf 格式化漏洞 | v2.2 | 改为 `printk("%s", buf)`，格式化符不再被解释 |
| S3 | fd < 3 魔法数字判断 | v2.2 | 改为 `fd_entry->type == JLOS_TASK_FD_CONSOLE` 显式类型判断 |
| S4 | jlos_task_fd_close pipe 未实现 | v2.2 | 补充引用计数递减 + destroy 完整逻辑 |
| M1 | 用户栈固定单页（4KB） | v2.2 | 扩展为 64KB（`JLOS_TASK_USER_STACK_SIZE = 0x10000`） |
| M2 | fork 用户栈未复制 | v2.2 | 子进程分配新物理帧 + memcpy 用户栈内容 |
| — | 栈切换导致 multiboot 参数丢失 | v2.2 | .boot 段全局变量保存参数，不受切栈和 BSS 清零影响 |
| — | 连续物理帧分配不验证碎片 | v2.2 | reserve_bulk 重写为遍历 bitmap 找真正连续空闲帧 |
| — | 页帧分配器无 OOM 防护 | v2.2 | init_main 逐级回退 heap_size，极端情况 halt |

---

## 已知问题与优化建议

### 架构级

| 编号 | 问题 | 严重度 | 说明 |
|------|------|--------|------|
| A1 | 用户态代码与内核混编 | 中 | `jlos_user_*` 函数和字符串字面量链接在 `.text/.rodata`（内核空间 0xC0xxxxxx），靠 `PTE_USER` 共享。经典做法是用户态代码单独编译为独立 ELF 加载到用户地址空间，当前方案隔离性弱 |
| A2 | 0~1MB 恒等映射未移除 | 低 | 启动时映射的 0~1MB 恒等映射在内核运行后仍保留。对当前项目影响小（用户页表不复制低 3GB），但经典做法会移除 |
| A3 | 具体架构代码边界 | 低 | 需持续检查 x86 代码不泄漏到 kernel 层，保持 HAL 抽象层纯净 |

### 网络驱动

| 编号 | 问题 | 严重度 | 说明 |
|------|------|--------|------|
| N1 | DHCP/DNS 未实现 | 低 | 当前 IP 硬编码，缺少自动获取和域名解析 |

### 系统调用

| 编号 | 问题 | 严重度 | 说明 |
|------|------|--------|------|
| S2 | syscall_write 每次 malloc/free | 低 | 每次 write 都 `jlos_malloc`/`jlos_free`，可用栈缓冲或预分配缓冲池优化 |

### 内存管理

| 编号 | 问题 | 严重度 | 说明 |
|------|------|--------|------|
| M3 | loader 启动页目录浪费 | 低 | `boot_page_dir` 占 4KB，切到内核页目录后不再使用但仍占用内存 |
| M4 | jlos_paging_is_user_accessible 性能 | 低 | 每次检查遍历页表，无快速路径（已有 KERNEL_VIRTUAL_BASE 检查，可进一步优化） |

### 多任务调度

| 编号 | 问题 | 严重度 | 说明 |
|------|------|--------|------|
| T1 | 主线程 hlt 循环效率 | 低 | 主线程在 hlt 循环中，每个 PIT tick 都 save/restore main_thread_state，功能正确但略有浪费 |
| T2 | sleep 精度受限 | 低 | sleep 基于 tick 计数，精度为 10ms（100Hz），不支持更精细睡眠 |
| T3 | 非抢占式下任务不交替 | 低 | task_a/task_b 各跑完 10 次才切换，非抢占式下任务不主动让出 |

### P0 - Bug / 正确性问题（必须修复）

| 编号 | 问题 | 位置 | 影响 |
|------|------|------|------|
| B1 | **fork 时 brk 重置不完整** | multitask.c `jlos_process_fork` | 子进程 `brk_end = brk_start`，但父进程已扩展的 brk 区域的物理页未复制到子进程，访问会 page fault |
| B2 | **exec 的 user_stack 清理不完整** | multitask.c `jlos_process_exec` | unmap 后物理帧未释放，多次 exec 耗尽内存 |
| B3 | **interruptnumber 全局变量非原子** | interrupts_asm.s | 中断号写入和读取之间可能被另一个中断覆盖，极少数情况下 handler 收到错误中断号 |
| B4 | **reserve_bulk 与 mark_occupied 竞态** | page_frame_allocator.c | reserve_bulk 释放锁后才更新 s_first_free_frame，低概率重复分配同一帧 |
| B5 | **Makefile grub.cfg 重复追加** | Makefile `$(JLOS).iso` | 没有清理旧 grub.cfg，多次 make 导致 menuentry 重复 |

### P1 - 代码质量问题

| 编号 | 问题 | 位置 | 说明 |
|------|------|------|------|
| A10 | **JLOS_ARRAY_LIMIT_RANGE 宏优先级 bug** | types.h | `(idx) + 1 % (range)` 应为 `((idx) + 1) % (range)`，% 优先级高于 + |
| A1 | **0~1MB 恒等映射残留** | paging.c `jlos_paging_initialize_kernel_paging` | GRUB 退出后不再需要，占用页表内存 + 安全隐患 |
| A2 | **boot_page_dir 内存未释放** | loader.s | 切到内核页表后 4KB 无法回收 |
| A3 | **fork 物理页 memcpy（非 COW）** | multitask.c | 用户栈所有物理页立即拷贝，内存翻倍开销 |
| A4 | **syscall_write 每次 malloc/free** | syscall.c | 高频 write 时 malloc/free 开销大 |
| A5 | **semaphore/mutex 无 timeout wait** | sync.c | wait 是无限阻塞，用户态无法实现带超时同步 |
| A6 | **硬编码 4MB 内核栈** | loader.s `.bss` | 固定分配浪费物理内存，应改为页帧动态分配 |
| A7 | **网络 IP 硬编码** | network.c | 换网络环境需改代码重编译 |
| A8 | **ATA/FAT 调试 printf 残留** | ata.c, fat.c | 运行时输出污染 |
| A9 | **任务表硬编码 256 上限** | multitask.h `jlos_task_t *tasks[256]` | 超过 256 任务时崩溃 |

### P2 - 性能优化

| 编号 | 问题 | 位置 | 说明 |
|------|------|------|------|
| P1 | **pipe 逐字节读写** | ipc.c | 4KB buffer 写满需要 4096 次 semaphore + mutex，改为批量可提升 10~100x |
| P2 | **access_ok 每次遍历页表** | paging.c | 每个 syscall 都遍历页表，延迟增加 |
| P3 | **网络初始化大量验证** | network.c | 每次启动都检查所有 handler 非空 + EtherType 匹配，可改为 assert |
| P4 | **fork 立即 memcpy 所有页** | multitask.c | 即使子进程 exec 后也不需要，内存瞬时峰值高 |

### P3 - 功能缺失（按经典路线图）

| 编号 | 功能 | 模块 | 依赖 |
|------|------|------|------|
| F1 | 按需分页 (demand paging) | paging.c | 无 |
| F2 | COW 写时复制 | multitask.c + paging.c | 依赖 F1 |
| F3 | 用户态 malloc/free | user | 依赖 brk（已完成） |
| F4 | FAT32 完善 (mount/open/close/read/write/seek/readdir) | filesystem | 依赖块设备完善 |
| F5 | open/close 系统调用 | syscall.c | 依赖 F4 |
| F6 | ELF 用户态程序加载 | user | 依赖 F4 |
| F7 | signal/kill 信号机制 | syscall.c + multitask.c | 无 |
| F8 | 线程支持（共享地址空间） | multitask.c | 依赖 COW (F2) |
| F9 | DHCP 自动获取 IP | net | 无 |
| F10 | DNS 域名解析 | net | 依赖 F9 |

---

## 阶段一：虚拟内存管理（已完成核心，持续优化）

**已完成**：

- [x] x86 4KB 分页机制 + 页表/页目录管理
- [x] CR3 切换 + 虚拟地址空间映射
- [x] 3:1 高半核架构（内核 0xC0000000，用户 0-3GB）
- [x] 低内存区域 cache disable 映射（DMA 一致性）
- [x] 用户栈多页映射（64KB）到用户地址空间
- [x] .text/.rodata PTE_USER 标志（ring3 syscall wrapper 可取指/读常量）
- [x] 页表深拷贝（fork 时分配新页表，非共享）
- [x] 动态物理内存探测（GRUB mmap 解析 + 保留区自动标记）
- [x] 动态主堆大小（phys_end/4，夹在 4MB~64MB，4KB 对齐）
- [x] 连续物理帧分配验证（reserve_bulk 重写，防止 MMIO 区间碎片）
- [x] 页帧分配器 OOM 回退（逐级减半 heap_size 直到成功或 halt）
- [x] access_ok 用户空间指针校验（copy_from_user / copy_to_user）

**待优化**：

- [ ] Page Fault 处理完善（区分非法访问 vs 合法缺页，如 brk 区域）
- [ ] 写时复制（COW）fork 优化（当前 memcpy 物理页）
- [ ] 按需分页（demand paging）
- [ ] mmap 内存映射
- [ ] 移除启动期 0~1MB 恒等映射（A2，对当前项目非必须）
- [ ] 释放 boot_page_dir（M3）

---

## 阶段二：系统调用接口（已完成核心，持续扩展）

**已完成系统调用**：

| 编号 | 名称 | 功能 | 状态 |
|------|------|------|------|
| 0 | EXIT | 退出进程 + exit_code | 已完成 |
| 1 | FORK | 创建子进程 + 用户栈复制 | 已完成 |
| 2 | READ | 读 fd（pipe） | 已完成 |
| 3 | WRITE | 写 fd / 控制台输出（已修复 printf 漏洞） | 已完成 |
| 4 | PRINTF | 打印字符串 | 已完成 |
| 5 | GET_ERRNO | 获取线程级 errno | 已完成 |
| 6 | GET_PID | 获取当前 PID | 已完成 |
| 7 | YIELD | 主动让出 CPU | 已完成 |
| 8 | SLEEP | 睡眠指定 tick | 已完成 |
| 9 | DEBUG_TASKS | 打印任务列表 | 已完成 |
| 10 | WAIT_PID | 等待子进程 + 退出码 + Zombie 释放 | 已完成 |
| 11 | CREATE_PIPE | 创建管道 | 已完成 |
| 12 | TASK_FD_CLOSE | 关闭文件描述符 + pipe 引用计数 destroy | 已完成 |
| 13 | TASK_BRK | 用户堆动态扩展/收缩 | 已完成 |

**已完成配套**：

- [x] per-thread errno
- [x] 用户空间指针校验（copy_from_user / copy_to_user）
- [x] 错误码定义（SYSCALL_ENOMEM / EFAULT / ENINVAL / ENOSYS 等）
- [x] 标准文件描述符预分配（fd 0=stdin, 1=stdout, 2=stderr）
- [x] jlos_user_puts 栈缓冲拷贝（修复 .rodata 指针被 access_ok 拒绝问题）
- [x] syscall_write printf 格式化漏洞修复（S1）
- [x] fd 类型显式判断替代魔法数字（S3）
- [x] pipe close 引用计数 + destroy 完整实现（S4）

**待扩展**：

- [ ] open/close 系统调用（依赖 FAT32）
- [ ] 用户态 malloc/free（依赖 brk）
- [ ] signal/kill 信号机制
- [ ] 用户态同步原语接口（sem/mutex 系统调用）
- [ ] 用户态代码独立编译为 ELF（A1）

---

## 阶段三：多任务系统增强（已完成核心，持续扩展）

**已完成**：

- [x] MLFQ 四级反馈队列调度器（Level 0-3，时间片 2/4/8/16 ticks）
- [x] 老化升级机制（200 ticks 未运行自动升级）
- [x] 抢占式/协作式宏开关（KERNEL_CONFIG_PREEMPTIVE）
- [x] 任务状态机（READY/RUNNING/SLEEPING/WAITING/ZOMBIE）
- [x] 集中状态转换宏（JLOS_TASK_SET_READY/RUNNING/ZOMBIE 等）
- [x] 信号量 + 互斥锁（可重入 + 所有权传递 + spinlock 保护）
- [x] 条件变量（cond_wait/signal/broadcast）
- [x] 管道（堆分配环形缓冲 + close/destroy 分离 + 引用计数）
- [x] 消息队列（柔性数组 + FIFO 顺序）
- [x] ring3 用户进程（TSS 特权级切换 + 用户栈多页映射）
- [x] sleep/wake 机制（基于 tick 计数）
- [x] Zombie 进程回收（wait_pid 后释放 PCB）
- [x] fork 用户栈复制（分配新物理帧 + memcpy）

**待扩展**：

- [ ] 线程支持（共享地址空间）
- [ ] 用户态同步原语接口
- [ ] 定时器（timer_create/timer_settime）
- [ ] 死锁检测（可选）

---

## 阶段四：FAT32 文件系统完善（待开始）

**当前状态**：仅基础 FAT32 结构定义（fat.c 有简陋读取逻辑）

**目标 API**：

```c
mount/unmount -> open/close -> read/write/seek -> create/delete -> readdir
```

**配套**：文件权限 + 目录遍历 + 缓冲区缓存

---

## 阶段五：网络驱动优化（部分完成）

**已完成协议**：ARP IPv4 ICMP UDP TCP HTTP

**待完成**：

- [ ] DHCP（自动获取 IP）
- [ ] DNS（域名解析）

**性能优化**：

- DMA 减少 CPU 拷贝 + 中断合并 + 零拷贝 + 缓冲池预分配

---

## 阶段六：高级特性（低优先）

- slab / 伙伴系统分配器
- 统一设备驱动框架
- 电源管理
- 安全加固

---

## 实施优先级清单

| 优先级 | 任务 | 状态 |
|--------|------|------|
| 高 | 1. 3:1 高半核分页 + 虚拟内存基础 | 已完成 |
| 高 | 2. 进程管理增强（PCB + 状态机 + MLFQ） | 已完成 |
| 高 | 3. 系统调用核心（exit/fork/read/write/wait_pid） | 已完成 |
| 高 | 4. 同步原语（信号量/互斥锁/条件变量） | 已完成 |
| 高 | 5. IPC（管道/消息队列） | 已完成 |
| 高 | 6. ring3 用户进程 + TSS 特权级切换 | 已完成 |
| 高 | 7. 动态内存探测 + 页表深拷贝 + fork 用户栈 | 已完成 |
| 高 | 8. syscall_write 漏洞修复 + pipe close 完整实现 | 已完成 |
| 中 | 9. COW 写时复制 + 用户态 brk 完善 | 待开始 |
| 中 | 10. 用户态 malloc/free + 按需分页 + 线程支持 | 待开始 |
| 中 | 11. 用户态同步原语 + signal/kill + 定时器 + mmap | 待开始 |
| 中 | 12. FAT32 文件系统 | 待开始 |
| 中 | 13. 网络性能优化 + DHCP/DNS | 待开始 |
| 低 | 14. 用户态代码独立编译 ELF + 移除恒等映射 | 待开始 |
| 低 | 15. 磁盘交换机制 | 待开始 |
| 低 | 16. slab/伙伴系统 + 设备驱动框架 | 待开始 |

---

## 实施路线图（v2.3）

### Phase 1：Bug 修复 & 代码清理（立即执行）

| 序号 | 编号 | 任务 | 涉及文件 | 复杂度 |
|------|------|------|----------|--------|
| 1 | A10 | JLOS_ARRAY_LIMIT_RANGE 宏优先级修复 | types.h | 极低 |
| 2 | B5 | Makefile grub.cfg 重复追加 | Makefile | 极低 |
| 3 | B1 | fork 时 brk 重置不完整 | multitask.c | 中 |
| 4 | B2 | exec 的 user_stack 物理帧泄漏 | multitask.c | 低 |
| 5 | A8 | ATA/FAT 调试 printf 清理 | ata.c, fat.c | 极低 |

### Phase 2：内存与性能优化（短期）

| 序号 | 编号 | 任务 | 涉及文件 | 复杂度 |
|------|------|------|----------|--------|
| 6 | A1 | 移除 0~1MB 恒等映射 | paging.c | 低 |
| 7 | A2 | 释放 boot_page_dir 内存 | loader.s + kernel.c | 低 |
| 8 | A4 | syscall_write 栈缓冲优化 | syscall.c | 低 |
| 9 | P1 | pipe 批量读写 | ipc.c | 中 |
| 10 | P3 | 网络验证改为 assert 或条件编译 | network.c | 低 |

### Phase 3：核心架构升级（中期）

| 序号 | 编号 | 任务 | 涉及文件 | 复杂度 |
|------|------|------|----------|--------|
| 11 | A3+F2 | COW 写时复制 | paging.c + multitask.c | 高 |
| 12 | F1 | 按需分页 (brk 区域) | paging.c | 中 |
| 13 | F3 | 用户态 malloc/free（基于 brk） | 新建 user/heap.c | 中 |
| 14 | A7 | 网络 IP 配置化 | network.c | 低 |
| 15 | A6 | 动态内核栈大小 | loader.s + kernel.c | 中 |

### Phase 4：文件系统完善（中-长期）

| 序号 | 编号 | 任务 | 涉及文件 | 复杂度 |
|------|------|------|----------|--------|
| 16 | F4 | FAT32 mount + 路径解析 | filesystem/ | 高 |
| 17 | F5 | open/close/read/write 系统调用 | syscall.c | 中 |
| 18 | F4+ | FAT32 seek/create/delete/readdir | filesystem/ | 高 |

### Phase 5：高级特性（长期）

| 序号 | 编号 | 任务 | 涉及文件 | 复杂度 |
|------|------|------|----------|--------|
| 19 | F6 | ELF 用户态程序加载 | user/ | 高 |
| 20 | F7 | signal/kill 信号机制 | syscall.c + multitask.c | 高 |
| 21 | F8 | 线程支持（共享地址空间） | multitask.c | 高 |
| 22 | F9 | DHCP 自动获取 IP | net/ | 中 |
| 23 | F10 | DNS 域名解析 | net/ | 高 |
| 24 | A9 | 任务表动态扩展 | multitask.c/h | 中 |

---

## 开发日志

### 2026-08-11（v2.3）

- 完整系统扫描：52 个 C 文件 + 2 个汇编 + 50 个头文件全覆盖
- 发现并分类 5 个 P0 Bug、10 个 P1 代码质量问题、4 个 P2 性能问题、10 个 P3 功能缺失
- JLOS_ARRAY_LIMIT_RANGE 宏 bug（% 优先级高于 +）
- fork brk 重置不完整：子进程 brk 区域物理页未复制
- exec 物理帧泄漏：unmap 后未释放
- Makefile grub.cfg 重复追加
- 制定 Phase 1~5 实施路线图
- 更新计划至 v2.3

### 2026-08-11（v2.2）

- 动态物理内存探测完成：GRUB mmap 解析 + MMIO 保留区自动标记 + 动态主堆大小（phys_end/4，夹 4MB~64MB）
- 栈切换 bug 修复：loader.s 用 .boot 段全局变量保存 multiboot 参数，防止切栈后参数丢失
- 临时页表扩展：从 16MB 扩展到 64MB，覆盖 GRUB 可能放置 mmap 的物理位置
- 连续物理帧分配重写：reserve_bulk 改为遍历 bitmap 找真正连续空闲帧，防止 MMIO 区间碎片
- OOM 防护：init_main 逐级回退 heap_size，极端情况打印错误 + halt
- 系统调用三项修复：S1 printf 漏洞、S3 fd 类型判断、S4 pipe close 引用计数
- 用户栈扩展：从 4KB 改为 64KB（JLOS_TASK_USER_STACK_SIZE = 0x10000）
- 页表深拷贝：fork 时分配新页表，不再共享 PTE
- 更新计划至 v2.2

### 2026-08-09

- 3:1 高半核重构完成：内核映射 0xC0000000，用户空间 0-3GB
- ring3 用户进程完整运行：TSS 特权级切换、用户栈映射、syscall wrapper
- 修复启动期 triple fault：16MB 启动页表、CR0.PG 序列化跳转、mbinfo 指针转换
- 修复 syscall handler 未注册、页故障错误码偏移、.rodata PTE_USER
- 修复 jlos_user_puts 字符串指针被 access_ok 拒绝（栈缓冲拷贝）
- 清理全部调试代码（串口标记、DBG 宏、临时函数）
- 更新计划至 v2.1

### 2026-07-31

- 阶段三多任务系统增强完成：MLFQ 调度器、状态机、同步原语、IPC 全部实现
- 全项目结构体成员 m_ 前缀去除重构完成（71 文件）
- 主堆扩展至 32MB，分页映射扩展至 256MB
- per-thread errno + wait_pid 退出码实现
- 更新计划至 v2.0

### 2026-07-01

- 网络栈全部打通：ARP/IPv4/ICMP/UDP/TCP/HTTP 全部验证通过
- TCP 序列号三次握手对齐 Bug 修复完成，curl -v 原生 HTTP/1.1 200 OK

### 2026-06-23

- 初始化优化计划
- 启动阶段一：虚拟内存管理

---

## 参考资料

1. Intel x86 Architecture Manual
2. OSDEV Wiki：<https://wiki.osdev.org/>
3. 《Operating Systems: Three Easy Pieces》
4. 《Modern Operating Systems》 - Andrew Tanenbaum
