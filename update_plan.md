# JohnBeauty OS 内核架构升级计划

版本: v2.4 | 日期: 2026-08-13 | 作者: JohnLove

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
| 虚拟内存 | 3:1 高半核分页 + 内核/用户地址空间隔离 + 页表深拷贝 | 动态映射，ring3 用户进程正常运行 |
| 内存管理 | 动态物理内存探测（GRUB mmap） + 页帧分配器 + 低内存堆(0x50000) + 主堆(动态 4~64MB) + 16字节最小分配 | 支持不同物理内存大小，MMIO 保留区自动标记 |
| 系统调用 | exit/fork/read/write/get_errno/get_pid/yield/sleep/wait_pid/brk/pipe/fd_close | per-thread errno + wait_pid 退出码 |
| 多任务调度 | MLFQ 四级反馈队列 + 老化升级 + 抢占/协作可切换 | 时间片轮转正常，交互型优先 |
| 同步原语 | 信号量 + 互斥锁(可重入+所有权传递) + 条件变量 | spinlock 关中断保护 |
| IPC | 管道(堆分配环形缓冲+引用计数) + 消息队列(柔性数组) | FIFO 顺序，close/destroy 分离 |
| 用户进程 | ring3 用户态进程 + 用户栈(64KB多页)映射 + TSS 特权级切换 + fork 用户栈复制 | Hello from ring3 + Wake up 验证通过 |

---

## 架构升级总览（v2.4 核心）

当前系统四个基础子系统（PFA / Paging / MemoryManager / Multitask）存在大量非经典做法和性能瓶颈。本计划按**依赖顺序**从底层向上逐层重构，使内核达到经典操作系统教科书级别的实现质量，同时为 SMP 预留清晰的扩展点。

```
依赖关系：

Phase 0: Buddy PFA  ←────────────────────────────────────┐
Phase 1: Paging 修复  ←── 依赖 PFA                       │
Phase 2: Memory Manager 升级 ←── 依赖 PFA + Paging        │
Phase 3: Multitask 升级 ←── 依赖 PFA + Paging + MM        │
Phase 4: SMP 预留  ←── 依赖全部                          │
                                                         │
                    ─── 先修 BUG，再做优化 ───             │
                    ─── 底层改好，上层才好改 ───            │
```

---

## Phase 0: Buddy Page Frame Allocator（最高优先级）

### 当前问题

| # | 问题 | 严重度 | 经典做法 |
|---|------|--------|----------|
| PFA-1 | O(N) bitmap 线性扫描，free 后不合并邻居帧 → 碎片化 | 🔴 Critical | Buddy allocator + merge |
| PFA-2 | reserve_bulk O(N×M) 找连续帧 + 与 malloc 扫描逻辑不统一 | 🔴 Critical | Buddy split from high order |
| PFA-3 | `mark_occupied` 只设 bitmap 不设 refcount，语义含糊 | 🟡 | 统一 refcount=1 标记 |
| PFA-4 | `first_free_frame` 只单帧推进，reserve_bulk 不推进 | 🟡 | 由 buddy free_area 完全替代 |
| PFA-5 | 全局单 spinlock → 多核瓶颈 | 🟢 暂不处理 | 留给 Phase 4 per-CPU cache |

### 设计方案

```
数据结构：
  s_bitmap[frame/8]   → 1=reserved(内核/MMIO), 0=buddy pool
  s_refcount[frame]   → PTE 引用计数 (COW 用)
  s_free_area[0..MAX_ORDER] → 每个 order 一条空闲链表

order: 0=1页, 1=2页, 2=4页, ..., 10=1024页=4MB

空闲链表节点嵌入物理帧本身：
  struct free_node { free_node *next; uint32_t frame; }
  节点放在空闲块的第一个物理帧的前 8 字节里（利用空闲内存）

分配流程 (page_frame_malloc / reserve_bulk):
  1. order = (size == 1) ? 0 : ceil(log2(size))
  2. 从 s_free_area[order] 取头节点 → O(1)
  3. 没有？向高 order 借 → split → 剩余挂回低 order
  4. 标记所有帧: refcount=1, bitmap 保持 0
  5. 返回首帧虚拟地址

释放流程 (refcount_dec → 归 0):
  1. 找到 buddy = frame ^ (1 << order)
  2. 判断 buddy 可合并: bitmap[buddy] == 0 && refcount[buddy] == 0
  3. 合并成大一块 → 递归向上
  4. 最终挂回 s_free_area[final_order]

reserve_bulk 流程 (内核堆预留):
  1. order = ceil(log2(num_frames))
  2. 跟 malloc 一样从 buddy 分配
  3. 标记所有帧 bitmap=1 (reserved, 永不归还)
  4. 返回虚拟地址

初始化:
  1. 同现有 init: 标记 kernel/bitmap/MMIO/boot 帧
  2. 剩余帧按 order 分块，挂入 s_free_area[]
     (从高 order 开始切，把能凑成大块的凑大块)
```

### 接口变更

```c
// 保持所有现有接口签名不变！
void *jlos_page_frame_malloc(void);       // 内部改为从 free_area[0] 取
void jlos_page_frame_free(void *addr);    // 内部改为 refcount_dec → 归 0 时 buddy_free
void *jlos_page_frame_reserve_bulk(uint32_t num_frames);  // 内部改为 order=ceil(log2(n)) + 标记 reserved
void jlos_page_frame_mark_occupied(uint32_t phys_start, uint32_t phys_end);  // 保持不变
void jlos_page_frame_refcount_inc/dec/get;  // 保持不变
```

**关键约束**：所有上层调用者（paging.c / memory_manager.c / multitask.c）不需要改任何代码。

### 实施清单

| 序号 | 任务 | 涉及文件 | 复杂度 |
|------|------|----------|--------|
| 0.1 | 定义 `MAX_ORDER` (建议 10 = 1024 页) + `free_area[]` 数组 + `free_node` 结构 | page_frame_allocator.h | 低 |
| 0.2 | 实现 buddy `split_block` / `merge_block` / `buddy_alloc` / `buddy_free` | page_frame_allocator.c | 中 |
| 0.3 | 重写 `page_frame_allocator_init`：空闲帧按 order 分块挂链表 | page_frame_allocator.c | 中 |
| 0.4 | 重写 `page_frame_malloc`：从 free_area[0] 取，无则 split | page_frame_allocator.c | 低 |
| 0.5 | 重写 `page_frame_reserve_bulk`：order=ceil(log2(n))，分配后 bitmap=1 标记 | page_frame_allocator.c | 低 |
| 0.6 | 修改 `refcount_dec`：归 0 时调 buddy_free 合并 | page_frame_allocator.c | 低 |
| 0.7 | 删除 `first_free_frame` 相关所有代码 | page_frame_allocator.c | 低 |
| 0.8 | 修复 `mark_occupied`：加 `s_refcount[i] = 1` 保持语义一致 | page_frame_allocator.c | 低 |
| 0.9 | 验证：boot、init_main、fork COW、exec、brk page fault 全流程 | 全项目 | 高 |

---

## Phase 1: Paging 子系统修复与优化

### 当前问题

| # | 问题 | 严重度 | 经典做法 |
|---|------|--------|----------|
| PAG-1 | **`map_range` 回滚死锁**：释放 `s_paging_lock` 后调 `jlos_paging_unmap`，后者又要拿同一把锁 | 🔴 Critical | 内部 `*_nolock` 变体 |
| PAG-2 | **全局 `s_paging_lock`**：所有 context 的 map/unmap 抢同一把锁 | 🟡 High | Per-context lock |
| PAG-3 | `access_ok` 每次调都拿全局锁遍历页表 | 🟡 High | Fast path: 地址范围检查 + present 即放行 |
| PAG-4 | Kernel page table 每 context 都 clone 但实际可共享 | 🟢 Med | 让 `context_clone` 直接共享内核 PTs（当前已是这样，确认即可） |
| PAG-5 | `context_destroy` 遍历 768 个 PDE（用户空间全扫描） | 🟢 Med | 优化：只遍历已映射 PDE（维护一个 used_pde_count） |
| PAG-6 | 4MB PDE unmap 循环 1024 次 `page_frame_free` 每次拿锁 | 🟢 Med | 批量处理：一次锁 + 批量 refcount_dec |
| PAG-7 | 无 page table slab cache → 每次 map 都 page_frame_malloc 单个 PT | 🟢 Med | 预分配 PT slab，批量分配 |
| PAG-8 | COW page fault 的 refcount 查询拿到锁后又释放（`get_physical_addr` 拿锁 → 释放 → `change_flags` 又拿锁） | 🟢 Low | 减少锁次数，或在 handler 里加一次锁统一做 |

### 实施清单

| 序号 | 任务 | 涉及文件 | 复杂度 |
|------|------|----------|--------|
| 1.1 | 修复 `map_range` 回滚死锁：抽取 `unmap_nolock` 内部版本 | paging.c | 低 |
| 1.2 | 把 `s_paging_lock` 移入 `paging_context_t` → per-context lock | paging.h + paging.c | 中 |
| 1.3 | 重写 `access_ok` fast path：地址范围检查 → 若地址在用户空间内则先放行，不拿锁 | paging.c | 低 |
| 1.4 | 4MB PDE unmap 优化：1 次锁 + 批量 refcount_dec（不再循环拿放锁） | paging.c | 低 |
| 1.5 | `context_clone` 内核 PT 共享确认 + 注释 | paging.c | 极低 |
| 1.6 | Page table slab cache：预分配 64 个 PT 的池，batch 分配/释放 | paging.c | 中 |
| 1.7 | COW fault handler 锁合并：一次锁内完成 get_phys + refcount + change_flags/map | paging.c | 低 |
| 1.8 | 验证：fork/exec/brk page fault/kernel map 全流程 | 全项目 | 高 |

---

## Phase 2: Memory Manager 升级

### 当前问题

| # | 问题 | 严重度 | 经典做法 |
|---|------|--------|----------|
| MM-1 | 全局 `s_mm_lock` → 多核瓶颈 | 🟡 High | Per-CPU freelists (SLUB style) |
| MM-2 | `expand_heap` 逐页 `page_frame_malloc` 再 map → 慢 | 🟡 High | 从 buddy reserve_bulk 大块分配 |
| MM-3 | 只有单一 manager，无 per-type cache | 🟢 Med | 多个 `kmem_cache`（task / fd / pipe 各一个） |
| MM-4 | 全局 `jlos_active_memory_manager` → 无法多 manager 并存 | 🟢 Med | 改为显式传 self，保留 active 作为默认 |
| MM-5 | 16B min alloc + 24B header → 小对象 50% 开销 | 🟢 Med | 调整 size classes 到常用尺寸（task≈128B, fd≈16B） |
| MM-6 | 没有 per-CPU partial slab tracking | 🟢 Low | SLUB active/partial/full 概念 |

### 实施清单

| 序号 | 任务 | 涉及文件 | 复杂度 |
|------|------|----------|--------|
| 2.1 | 把 `expand_heap` 从逐页 malloc 改为 `page_frame_reserve_bulk` + 批量 map | memory_manager.c | 低 |
| 2.2 | 把 `jlos_active_memory_manager` 从全局变量改为每-CPU 存储（为 Phase 4 预留） | memory_manager.c/h | 中 |
| 2.3 | 实现 `kmem_cache`：固定-size 对象缓存（task_cache / fd_cache / pipe_cache） | 新建 kmem_cache.c/h | 中 |
| 2.4 | `jlos_malloc` 改为先查 per-CPU freelist，miss 再走全局锁 | memory_manager.c | 中 |
| 2.5 | 验证：所有 `jlos_malloc`/`jlos_free` 调用点兼容 | 全项目 | 中 |

---

## Phase 3: Multitask 调度器升级

### 当前问题

| # | 问题 | 严重度 | 经典做法 |
|---|------|--------|----------|
| MT-1 | **Zombie 清理 O(n²)**：每次 schedule 都双循环扫所有任务 | 🔴 Critical | Parent→children 双向链表 + O(1) reap |
| MT-2 | **Zombie 清理 swap 后跳过元素**：倒序遍历 + swap 移除会跳过 | 🔴 Critical | 用链表代替数组，或正序遍历 |
| MT-3 | **O(n) 调度选择**：4 层 × n 任务全扫 | 🟡 High | O(1) 或 O(log n) 选择 |
| MT-4 | 静态 `tasks[JLOS_TASK_MAX_NUM]` 数组 → 256 上限 | 🟡 High | 动态数组 or 链表 |
| MT-5 | O(n) pid lookup（wait_pid 循环扫 tasks） | 🟡 High | pid hash table |
| MT-6 | 单全局 runqueue → 多核无 locality | 🟢 Low | per-CPU runqueue（留给 Phase 4） |
| MT-7 | `exec` init_user 失败后继续赋 pid + 清 exit_code | 🟢 Low | 失败直接 return -1 |
| MT-8 | `fork` 遍历 768 个 PDE（全空间），大部分不存在 | 🟢 Low | 只遍历已 present 的 PDE |

### 实施清单

| 序号 | 任务 | 涉及文件 | 复杂度 |
|------|------|----------|--------|
| 3.1 | Task struct 加 `children_head`, `parent`, `next_child`, `prev_child` → 建立父子链表 | multitask.h | 低 |
| 3.2 | fork 时把 child 挂入 parent->children_head；exit 时从 children 链表摘除 | multitask.c | 低 |
| 3.3 | Zombie 清理：只遍历 parent 的 children 链表，parent 死了就杀所有子 | multitask.c | 中 |
| 3.4 | tasks 改动态数组 (realloc)，或保持数组但换 O(1) 移除逻辑 | multitask.c | 低 |
| 3.5 | pid hash table (256 桶)：pid→task 直接定位，替换所有 tasks 循环扫 | multitask.c | 中 |
| 3.6 | exec init_user 失败 → 直接 return -1，不赋值 pid/pid/exit_code | multitask.c | 低 |
| 3.7 | fork COW: 只遍历 present PDE（加一个 present_bitmap 或在 loop 里提前 continue） | multitask.c | 低 |
| 3.8 | wait_pid: 用 pid hash 找 task，不再循环扫 tasks | multitask.c | 低 |
| 3.9 | 验证：fork/exec/wait_pid/zombie 全流程 | 全项目 | 高 |

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
| **P0** | 0 | Buddy Page Frame Allocator | 无 |
| **P0** | 1.1 | 修复 map_range 回滚死锁 | Phase 0 |
| **P0** | 1.2 | Per-context paging lock | Phase 0 |
| **P1** | 3.1-3.3 | Zombie O(1) 清理 + 父子链表 | 无（可独立） |
| **P1** | 1.3-1.4 | access_ok fast path + 4MB PDE unmap | Phase 0 |
| **P1** | 3.5 | pid hash table | 无 |
| **P2** | 2.1 | expand_heap 大块分配 | Phase 0 |
| **P2** | 1.6 | PT slab cache | Phase 0 |
| **P2** | 2.3-2.4 | kmem_cache + per-CPU freelist | Phase 0 + 1 |
| **P3** | 4.x | SMP 全部预留 | Phase 0-3 |

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
