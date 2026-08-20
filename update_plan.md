# JohnBeauty OS 内核架构升级计划

版本: v2.6 | 日期: 2026-08-20 | 作者: JohnLove

---

## 优化原则

1. 核心功能优先;
2. 优化时，选择经典做法优先;
3. **性能优先于改动大小;**
4. **面向未来多核/SMP 设计，不为单核临时妥协;**
5. **各子系统按依赖顺序升级，底层先于上层。**

---

## 里程碑总览

| 模块 | 完成项 | 验证 |
|------|--------|------|
| 网络驱动 | AMD AM79C973 初始化（CSR/BCR 配置、描述符环、IRQ 处理） | ERR=0，收发稳定 |
| ARP | 请求/响应 + 非阻塞查找缓存 | ARP 缓存正常更新 |
| IPv4 | 路由 + 校验和 + 收发封装 | 正常收发 |
| ICMP | Echo Request/Reply（完整 payload 校验） | ping 可正常工作 |
| UDP | 非阻塞 send/recv + Socket 管理 | 收发正常，无 ERR=1 循环 |
| TCP | 完整状态机 + 三次握手/四次挥手 + SYN/FIN 序列号处理 | curl 直连成功 |
| HTTP | HTTP/1.1 响应 + Content-Length + 正确 header | curl -v 原生解析 200 OK |
| 虚拟内存 | 3:1 高半核分页 + 内核/用户地址空间隔离 + per-context lock + COW + context_clone 浅拷贝 | 动态映射，ring3 用户进程正常运行 |
| 内存管理 | Buddy PFA + Bootstrap Allocator + Linux 风格虚拟布局 + 批量 expand_heap + 修复 prev 合并方向 | 256MB QEMU 全流程通过 |
| 系统调用 | exit/fork/read/write/get_errno/get_pid/yield/sleep/wait_pid/brk/pipe/fd_close | per-thread errno + wait_pid 退出码 |
| 多任务调度 | MLFQ 四级反馈队列 + 老化升级 + 抢占/协作可切换 + pid hash table + O(1) zombie 清理 | 时间片轮转正常，交互型优先 |
| 同步原语 | 信号量 + 互斥锁(可重入+所有权传递) + 条件变量 | spinlock 关中断保护 |
| IPC | 管道(堆分配环形缓冲+引用计数) + 消息队列(柔性数组) | FIFO 顺序，close/destroy 分离 |
| 用户进程 | ring3 用户态进程 + 用户栈(64KB多页)映射 + TSS 特权级切换 + fork COW + exit stub 修复 | Hello from ring3 + Wake up 验证通过 |

---

## 架构升级总览（v2.5 核心）

当前系统四个基础子系统（PFA / Paging / MemoryManager / Multitask）存在大量非经典做法和性能瓶颈。本计划按**依赖顺序**从底层向上逐层重构，使内核达到经典操作系统教科书级别的实现质量，同时为 SMP 预留清晰的扩展点。

```
依赖关系：

Phase 0: Buddy PFA ✅  ←──────────────────────────────────┐
Phase 1: Paging 修复 ✅  ←── 依赖 PFA                         │
Phase 2: Memory Manager 升级 ✅ ←── 依赖 PFA + Paging          │
Phase 3: Multitask 升级 ✅ ←── 依赖 PFA + Paging + MM          │
Phase 4: SMP 预留  ←── 依赖全部                              │
                                                             │
                    ─── 先修 BUG，再做优化 ───               │
                    ─── 底层改好，上层才好改 ───              │
```

---

## Phase 0: Buddy Page Frame Allocator ✅ 已完成

### 实施结果

| 项 | 状态 | 说明 |
|----|------|------|
| Buddy 分配器 (MAX_ORDER=10, free_area[0..10]) | ✅ | 空闲链表节点嵌入物理帧前 8 字节 |
| Bootstrap Allocator (boot_alloc) | ✅ | 按字节分配，page_alloc 为薄 wrapper |
| 两阶段初始化 (boot_alloc → paging → PFA init) | ✅ | 经典方案 A |
| 元数据按字节分配 (bitmap/refcount/buddy_order) | ✅ | 不再固定 1 页，按实际大小 |
| mark_occupied 语义统一 | ✅ | bitmap=1 + refcount=1 + buddy_order=INVALID |
| reserve_bulk 从 buddy 分配 | ✅ | 修复 mark_reserve 循环变量 bug (frame+i) |
| Linux 风格虚拟布局 | ✅ | DIRECT_MAP_SIZE=896MB, HEAP_BASE=0xF8000000 |
| 内核堆逐帧映射 | ✅ | init_main + expand_heap 同构 (物理散、虚拟连) |
| spinlock 保护 | ✅ | s_pfa_lock |
| 验证 | ✅ | 256MB QEMU 全流程通过: ring3 + fork COW + 网络 + 多任务 |

### 已知遗留

| # | 问题 | 说明 | 优先级 |
|---|------|------|--------|
| PFA-R1 | 全局单 spinlock → 多核瓶颈 | 留给 Phase 4 per-CPU cache | 低 |
| PFA-R2 | boot_alloc 用完不释放指针 | s_boot_heap_ptr 后续不再被引用，无实际影响 | 极低 |

---

## Phase 1: Paging 子系统修复与优化 ✅ 已完成

### 已完成修复

| # | 项 | 状态 | 位置 |
|---|----|------|------|
| PAG-D1 | PSE 4MB flag 保留 (change_flags_1 保留 PS 位) | ✅ | paging.c |
| PAG-D2 | TLB 刷新策略 (非 active context 不刷, active 按 threshold 批量) | ✅ | paging.c |
| PAG-D3 | Boot allocator 集成 (paging_init 接收 alloc_fn) | ✅ | paging.c |
| PAG-D4 | Linux 风格虚拟布局 (DIRECT_MAP_SIZE=896MB) | ✅ | paging.h |
| PAG-D5 | 内核段权限分离 (.text RO, .data/.bss RW) | ✅ | paging.c |
| PAG-D6 | access_ok 地址范围 fast check | ✅ | paging.c |
| PAG-1 | map_range 回滚用 unmap_nolock 锁内完成 | ✅ | paging.c |
| PAG-2 | Per-context spinlock 替代全局锁 | ✅ | paging.h + paging.c |
| PAG-3 | access_ok 仅范围检查，删掉页表遍历 | ✅ | paging.c |
| PAG-4 | context_clone 内核 PDE 浅拷贝 (共享) | ✅ | paging.c |
| PAG-5 | context_destroy 用 used_pde_count 优化 | ✅ | paging.c |
| PAG-8 | COW fault handler 一次锁内完成 | ✅ | paging.c |
| PAG-9 | context_clone malloc 失败处理 | ✅ | paging.c |
| PAG-11 | fork COW 只遍历 present PDE | ✅ | paging.c + multitask.c |

### 已知遗留（低优先级）

| # | 问题 | 说明 | 优先级 |
|---|------|------|--------|
| PAG-6 | 4MB PDE unmap 循环 1024 次 free | 可批量 refcount_dec 优化 | 低 |
| PAG-7 | 无 page table slab cache | 预分配 PT 池 | 低 |
| PAG-10 | change_flags 双重 TLB 刷新 | 内部不刷外层统一刷 | 低 |

---

## Phase 2: Memory Manager 升级 ✅ 已完成

### 已完成修复

| # | 项 | 状态 | 位置 |
|---|----|------|------|
| MM-Bug1 | **prev 合并方向错误**（严重 Bug）：header 留在 chunk 地址，size 虚增 → 内存越界 | ✅ | memory_manager.c free() |
| MM-Bug2 | **next 合并后 self->tail 未更新**：tail 指向被吸收的 chunk → 野指针 | ✅ | memory_manager.c free() |
| MM-2 | expand_heap 从逐页 malloc 改为 reserve_bulk + 批量 map | ✅ | memory_manager.c |
| MM-2b | expand_heap 分配后与 tail 空闲 chunk 合并（减少碎片） | ✅ | memory_manager.c |

### 已知遗留（Phase 4 SMP 范畴）

| # | 问题 | 说明 | 优先级 |
|---|------|------|--------|
| MM-1 | 全局 `s_mm_lock` → 多核瓶颈 | Per-CPU freelists (SLUB style) | 低 |
| MM-3 | 无 per-type cache | kmem_cache（task / fd / pipe） | 低 |
| MM-5 | 16B min alloc + 24B header → 小对象开销 | 调整 size classes | 低 |

---

## Phase 3: Multitask 调度器升级 ✅ 已完成

### 已完成修复

| # | 项 | 状态 | 位置 |
|---|----|------|------|
| MT-Exit | **exit stub 修复**：asm → C 函数 + cli/sti 原子写 ZOMBIE + 清空 g_current_task_ptr 防野指针 | ✅ | context_switch.c |
| MT-1/5 | **pid hash table**（256 桶，链地址法）：zombie parent 查找 O(n)→O(1)，wait_pid 查找 O(n)→O(1) | ✅ | multitask.h + multitask.c |
| MT-Wait | WAITING 唤醒用 pid hash 替代 O(n) 扫表 | ✅ | multitask.c schedule() |
| MT-7 | exec init_user 失败已正确 return -1（确认无需改动） | ✅ 确认 | multitask.c |
| MT-8 | fork COW 已 skip non-present PDE（确认无需改动） | ✅ 确认 | multitask.c |

### 设计决策

| 决策 | 理由 |
|------|------|
| 用 pid hash 替代 parent 指针 + children 链表 | parent 被 free 后 hash 自动返回 NULL，无悬空指针风险；当前无信号/ptrace，不需要 parent 指针 |
| task struct 仅加 1 个字段 (`next_hash`) | 不加 `parent`/`children_head`/`next_sibling`/`prev_sibling`，将来加信号系统时再补 |
| 保持 swap-with-last O(1) 删除 | 倒序遍历 + swap 不会跳过元素（已确认正确） |

### 已知遗留（低优先级）

| # | 问题 | 说明 | 优先级 |
|---|------|------|--------|
| MT-3 | O(n) 调度选择（4 层 × n 任务） | per-level runqueue 可优化为 O(1) | 低 |
| MT-4 | 静态 256 task 数组 | 256 上限对当前足够 | 低 |
| MT-6 | 单全局 runqueue | per-CPU runqueue 留给 Phase 4 | 低 |

---

## Phase 4: SMP 预留（多 CPU 准备）

本阶段不实现 SMP，而是为 SMP 预留清晰的扩展点。当前单核实现中就把架构搭好，避免未来重写。

| 序号 | 任务 | 涉及文件 | 复杂度 |
|------|------|----------|--------|
| 4.1 | PFA: per-CPU frame cache (order-0 batch refill/drain) | page_frame_allocator.c/h | 中 |
| 4.2 | Paging: TLB shootdown IPI 抽象接口 `jlos_hal_paging_tlb_shootdown(cpu_mask, addr)` | hal/paging.h + 实现占位 | 低 |
| 4.3 | MM: per-CPU freelist + batch drain to global | memory_manager.c | 中 |
| 4.4 | Multitask: per-CPU runqueue + task->cpu + task->cpumask | multitask.h + multitask.c | 高 |
| 4.5 | HAL: 抽象 `jlos_hal_get_cpu_id()` / `jlos_hal_num_cpus()` | hal/cpu.h | 低 |
| 4.6 | Spinlock: ticket lock 替换当前实现（为多核公平性） | hal/spinlock.c | 中 |

---

## 实施优先级

| 优先级 | Phase | 任务 | 依赖 |
|--------|-------|------|------|
| **P0** | 0 | Buddy Page Frame Allocator | ✅ 已完成 |
| **P0** | 1 | Paging 全部修复 (per-context lock + clone 浅拷贝 + COW 锁合并等) | ✅ 已完成 |
| **P0** | 2 | Memory Manager 修复 (prev 合并方向 + tail 更新 + 批量 expand_heap) | ✅ 已完成 |
| **P0** | 3 | Multitask 升级 (exit stub 修复 + pid hash + O(1) zombie/wait_pid) | ✅ 已完成 |
| **P3** | 4.x | SMP 全部预留 | Phase 0-3 ✅ |

---

## 待实现功能（按经典路线图）

| 编号 | 功能 | 模块 | 依赖 |
|------|------|------|------|
| F1 | 按需分页 (demand paging, 已实现 brk + stack) | paging.c | 无 |
| F2 | COW 写时复制 (已实现) | paging.c + multitask.c | — |
| F3 | 用户态 malloc/free | user | 依赖 brk（已完成） |
| F4 | FAT32 完善 (mount/open/close/read/write/seek/readdir) | filesystem | 依赖块设备完善 |
| F5 | open/close 系统调用 | syscall.c | 依赖 F4 |
| F6 | ELF 用户态程序加载 | user | 依赖 F4 |
| F7 | signal/kill 信号机制 | syscall.c + multitask.c | 无 |
| F8 | 线程支持（共享地址空间） | multitask.c | 依赖 COW (F2) |
| F9 | DHCP 自动获取 IP | net | 无 |
| F10 | DNS 域名解析 | net | 依赖 F9 |
| F11 | mmap 内存映射 | paging.c + syscall.c | 依赖 F4 |

---

## 已知遗留（低优先或需更大重构）

| # | 问题 | 说明 |
|---|------|------|
| A1 | 用户态代码与内核混编 | 经典做法用户态独立 ELF + 加载器；当前靠 PTE_USER 共享 .text/.rodata；安全隔离性弱 |
| A2 | 0~1MB 恒等映射残留 | GRUB 退出后不再需要，经典做法移除 |
| A3 | boot_page_dir 内存未释放 | 切到内核页表后 4KB 无法回收 |
| A4 | MLFQ 不 starvation-free | CFS weighted fair queuing 更经典但复杂度高；当前可接受 |
| A5 | 任务表硬编码 256 上限 | Phase 3.4 改为动态数组 |

---

## 历史版本（v2.3 及以前，保留参考）

### 已完成里程碑（历史）

| 模块 | 完成项 | 验证 |
|------|--------|------|
| 虚拟内存 | 3:1 高半核分页 + 内核/用户地址空间隔离 + 页表深拷贝 | 动态映射，ring3 用户进程正常运行 |
| 内存管理 | 动态物理内存探测（GRUB mmap） + 页帧分配器 + 低内存堆(0x50000) + 主堆(动态 4~64MB) + 16字节最小分配 | 支持不同物理内存大小，MMIO 保留区自动标记 |
| 系统调用 | exit/fork/read/write/get_errno/get_pid/yield/sleep/wait_pid/brk/pipe/fd_close | per-thread errno + wait_pid 退出码 |
| 多任务调度 | MLFQ 四级反馈队列 + 老化升级 + 抢占/协作可切换 | 时间片轮转正常，交互型优先 |
| 同步原语 | 信号量 + 互斥锁(可重入+所有权传递) + 条件变量 | spinlock 关中断保护 |
| IPC | 管道(堆分配环形缓冲+引用计数) + 消息队列(柔性数组) | FIFO 顺序，close/destroy 分离 |
| 用户进程 | ring3 用户态进程 + 用户栈(64KB多页)映射 + TSS 特权级切换 + fork 用户栈复制 | Hello from ring3 + Wake up 验证通过 |

### 已解决问题（历史）

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

### 开发日志

#### 2026-08-20（v2.6）

- **Phase 1 Paging 全部完成**：
  - Per-context spinlock（替代全局锁）
  - context_clone 内核 PDE 浅拷贝（共享，不深拷贝）
  - context_clone malloc 失败处理
  - map_range 回滚用 unmap_nolock 锁内完成
  - access_ok 仅范围检查，删掉页表遍历
  - COW fault handler 一次锁内完成
  - fork COW 只遍历 present PDE
  - context_destroy 用 used_pde_count 优化
- **Phase 2 Memory Manager 全部完成**：
  - 修复严重 Bug：free() prev 合并方向错误 → size 虚增 → 内存越界
  - 修复 Bug：free() next 合并后 self->tail 未更新
  - expand_heap 从逐页 malloc 改为 reserve_bulk + 批量 map
  - expand_heap 分配后与 tail 空闲 chunk 合并
- **Phase 3 Multitask 全部完成**：
  - exit stub 修复：asm → C 函数 + cli/sti 原子写 ZOMBIE + 清空 g_current_task_ptr 防野指针
  - pid hash table（256 桶，链地址法）：zombie parent/wait_pid/WAITING 唤醒全部 O(1)
  - 设计决策：用 pid hash 替代 parent 指针 + children 链表（无悬空指针风险，当前无信号/ptrace 不需要 parent 指针）
  - task struct 仅加 1 个字段 `next_hash`（4 字节）
  - 确认 MT-7（exec 失败处理）和 MT-8（fork COW present PDE）已正确，无需改动
- 全流程验证通过：ring3 + fork COW + sleep/wake + zombie 清理 + 网络 + 多任务
- 更新计划至 v2.6

#### 2026-08-18（v2.5）

- Phase 0 Buddy PFA 全部完成：
  - Bootstrap Allocator (boot_alloc) 按字节分配
  - 两阶段初始化 (boot_alloc → paging → PFA init)
  - Buddy 分配器 MAX_ORDER=10，空闲链表嵌入物理帧
  - mark_occupied/refcount/buddy_order 语义统一
  - reserve_bulk mark_reserve 循环变量 bug 修复 (frame+i)
- Linux 风格虚拟布局：
  - KERNEL_DIRECT_MAP_SIZE=896MB (0x38000000)
  - KERNEL_HEAP_VIRT_BASE=0xF8000000 (vmalloc 区)
  - init_main 逐帧 malloc + paging_map (物理散、虚拟连)
- 完整扫描 paging.c，更新 Phase 1：
  - 确认 6 项已完成 (PSE flag 保留 / TLB 策略 / boot_alloc 集成 / Linux 布局 / 段权限 / access_ok)
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
- JLOS_ARRAY_LIMIT_RANGE 宏 bug（% 优先级高于 +）
- fork brk 重置不完整：子进程 brk 区域物理页未复制
- exec 物理帧泄漏：unmap 后未释放
- Makefile grub.cfg 重复追加
- 制定 Phase 1~5 实施路线图
- 更新计划至 v2.3

#### 2026-08-11（v2.2）

- 动态物理内存探测完成：GRUB mmap 解析 + MMIO 保留区自动标记 + 动态主堆大小（phys_end/4，夹 4MB~64MB）
- 栈切换 bug 修复：loader.s 用 .boot 段全局变量保存 multiboot 参数，防止切栈后参数丢失
- 临时页表扩展：从 16MB 扩展到 64MB，覆盖 GRUB 可能放置 mmap 的物理位置
- 连续物理帧分配重写：reserve_bulk 改为遍历 bitmap 找真正连续空闲帧，防止 MMIO 区间碎片
- OOM 防护：init_main 逐级回退 heap_size，极端情况打印错误 + halt
- 系统调用三项修复：S1 printf 漏洞、S3 fd 类型判断、S4 pipe close 引用计数
- 用户栈扩展：从 4KB 改为 64KB（JLOS_TASK_USER_STACK_SIZE = 0x10000）
- 页表深拷贝：fork 时分配新页表，不再共享 PTE
- 更新计划至 v2.2

#### 2026-08-09

- 3:1 高半核重构完成：内核映射 0xC0000000，用户空间 0-3GB
- ring3 用户进程完整运行：TSS 特权级切换、用户栈映射、syscall wrapper
- 修复启动期 triple fault：16MB 启动页表、CR0.PG 序列化跳转、mbinfo 指针转换
- 修复 syscall handler 未注册、页故障错误码偏移、.rodata PTE_USER
- 修复 jlos_user_puts 字符串指针被 access_ok 拒绝（栈缓冲拷贝）
- 清理全部调试代码（串口标记、DBG 宏、临时函数）
- 更新计划至 v2.1

#### 2026-07-31

- 阶段三多任务系统增强完成：MLFQ 调度器、状态机、同步原语、IPC 全部实现
- 全项目结构体成员 m_ 前缀去除重构完成（71 文件）
- 主堆扩展至 32MB，分页映射扩展至 256MB
- per-thread errno + wait_pid 退出码实现
- 更新计划至 v2.0

#### 2026-07-01

- 网络栈全部打通：ARP/IPv4/ICMP/UDP/TCP/HTTP 全部验证通过
- TCP 序列号三次握手对齐 Bug 修复完成，curl -v 原生 HTTP/1.1 200 OK

#### 2026-06-23

- 初始化优化计划
- 启动阶段一：虚拟内存管理

---

## 参考资料

1. Intel x86 Architecture Manual
2. OSDEV Wiki：<https://wiki.osdev.org/>
3. 《Operating Systems: Three Easy Pieces》
4. 《Modern Operating Systems》 - Andrew Tanenbaum
5. Linux kernel source (buddy allocator, SLUB, scheduler)
6. FreeBSD VM system design
