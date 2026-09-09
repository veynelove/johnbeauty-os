# JohnBeauty OS 内核架构升级计划

版本: v2.9 | 日期: 2026-09-03 | 作者: JohnLove

***

## 优化原则

1. **核心功能对齐生产级：多架构可移植、多核可扩展、运行稳定、基础路径性能优秀。**
2. **经典结构优先，性能优先。** 
3. **拒绝"最小修改"思维。** 
4. **稳定性 > 性能 > 改动大小。**
5. **面向未来多核/SMP 设计，HAL 层先做抽象，架构相关代码不侵入内核子系统。**
6. **各子系统按依赖顺序升级，底层先于上层；先修正确性与锁粒度，再做数据结构优化。**
7. **命名遵循 JLOS 规范（`jlos_<subsystem>_<action>`），日志只写错误类型 tag，不重复时间戳/子系统/函数名（由框架自动封装）。**
8. **生产级内核, vmplayer测试，目标运行在硬件上**

***

## 里程碑总览

| 模块           | 完成项                                                                                           | 验证                                         |
| -------------- | ------------------------------------------------------------------------------------------------ | -------------------------------------------- |
| 网络驱动       | AMD AM79C973 初始化（CSR/BCR 配置、描述符环、IRQ 处理）                                          | ERR=0，收发稳定                              |
| ARP            | 请求/响应 + 非阻塞查找缓存                                                                       | ARP 缓存正常更新                             |
| IPv4           | 路由 + 校验和 + 收发封装                                                                         | 正常收发                                     |
| ICMP           | Echo Request/Reply（完整 payload 校验）                                                          | ping 可正常工作                              |
| UDP            | 非阻塞 send/recv + Socket 管理                                                                   | 收发正常，无 ERR=1 循环                      |
| TCP            | 完整状态机 + 三次握手/四次挥手 + SYN/FIN 序列号处理                                              | curl 直连成功                                |
| HTTP           | HTTP/1.1 响应 + Content-Length + 正确 header                                                     | curl -v 原生解析 200 OK                      |
| 虚拟内存       | 3:1 高半核分页 + 内核/用户地址空间隔离 + per-context lock + COW + context\_clone 浅拷贝          | 动态映射，ring3 用户进程正常运行             |
| 内存管理       | Buddy PFA + Bootstrap Allocator + Linux 风格虚拟布局 + 批量 expand\_heap + 修复 prev 合并方向    | 256MB                     |
| 系统调用       | exit/fork/read/write/get\_errno/get\_pid/yield/sleep/wait\_pid/brk/pipe/fd\_close                | per-thread errno + wait\_pid 退出码          |
| 多任务调度     | MLFQ 四级反馈队列 + 老化升级 + 抢占/协作可切换 + pid hash table + O(1) zombie 清理               | 时间片轮转正常，交互型优先                   |
| 同步原语       | 信号量 + 互斥锁(可重入+所有权传递) + 条件变量                                                    | spinlock 关中断保护                          |
| IPC            | 管道(堆分配环形缓冲+引用计数) + 消息队列(柔性数组)                                               | FIFO 顺序，close/destroy 分离                |
| 用户进程       | ring3 用户态进程 + 用户栈(64KB多页)映射 + TSS 特权级切换 + fork COW + exit stub 修复             | Hello from ring3 + Wake up 验证通过          |
| FPU/SSE        | Lazy 上下文切换 (CR0.TS + #NM handler) + FXSAVE/FXRSTOR + HAL ext\_state 抽象层 + 干净模板初始化 | multitask\_test + ring3 正常运行             |
| 多任务测试基线 | fork copy\_thread+ret\_from\_fork 经典范式 + buddy 经典顺序构造 + wait/wake 阻塞 + PF 按需分页   | MEMORY/MULTITASK ALL PASSED, TEST1-4 全 PASS |

***

## 架构升级总览（v2.5 核心）

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

***

## Phase 0: Buddy Page Frame Allocator ✅ 已完成

### 实施结果

| 项                                               | 状态 | 说明                                                    |
| ------------------------------------------------ | ---- | ------------------------------------------------------- |
| Buddy 分配器 (MAX\_ORDER=10, free\_area\[0..10]) | ✅    | 空闲链表节点嵌入物理帧前 8 字节                         |
| Bootstrap Allocator (boot\_alloc)                | ✅    | 按字节分配，page\_alloc 为薄 wrapper                    |
| 两阶段初始化 (boot\_alloc → paging → PFA init)   | ✅    | 经典方案 A                                              |
| 元数据按字节分配 (bitmap/refcount/buddy\_order)  | ✅    | 不再固定 1 页，按实际大小                               |
| mark\_occupied 语义统一                          | ✅    | bitmap=1 + refcount=1 + buddy\_order=INVALID            |
| reserve\_bulk 从 buddy 分配                      | ✅    | 修复 mark\_reserve 循环变量 bug (frame+i)               |
| Linux 风格虚拟布局                               | ✅    | DIRECT\_MAP\_SIZE=896MB, HEAP\_BASE=0xF8000000          |
| 内核堆逐帧映射                                   | ✅    | init\_main + expand\_heap 同构 (物理散、虚拟连)         |
| spinlock 保护                                    | ✅    | s\_pfa\_lock                                            |
| 验证                                             | ✅    | 256MB 全流程通过: ring3 + fork COW + 网络 + 多任务 |

### 已知遗留

| #      | 问题                       | 说明                                          | 优先级 |
| ------ | -------------------------- | --------------------------------------------- | ------ |
| PFA-R1 | 全局单 spinlock → 多核瓶颈 | 留给 Phase 4 per-CPU cache                    | 低     |
| PFA-R2 | boot\_alloc 用完不释放指针 | s\_boot\_heap\_ptr 后续不再被引用，无实际影响 | 极低   |

***

## Phase 1: Paging 子系统修复与优化 ✅ 已完成

### 已完成修复

| #      | 项                                                              | 状态 | 位置                   |
| ------ | --------------------------------------------------------------- | ---- | ---------------------- |
| PAG-D1 | PSE 4MB flag 保留 (change\_flags\_1 保留 PS 位)                 | ✅    | paging.c               |
| PAG-D2 | TLB 刷新策略 (非 active context 不刷, active 按 threshold 批量) | ✅    | paging.c               |
| PAG-D3 | Boot allocator 集成 (paging\_init 接收 alloc\_fn)               | ✅    | paging.c               |
| PAG-D4 | Linux 风格虚拟布局 (DIRECT\_MAP\_SIZE=896MB)                    | ✅    | paging.h               |
| PAG-D5 | 内核段权限分离 (.text RO, .data/.bss RW)                        | ✅    | paging.c               |
| PAG-D6 | access\_ok 地址范围 fast check                                  | ✅    | paging.c               |
| PAG-1  | map\_range 回滚用 unmap\_nolock 锁内完成                        | ✅    | paging.c               |
| PAG-2  | Per-context spinlock 替代全局锁                                 | ✅    | paging.h + paging.c    |
| PAG-3  | access\_ok 仅范围检查，删掉页表遍历                             | ✅    | paging.c               |
| PAG-4  | context\_clone 内核 4MB PDE 浅拷贝共享（4KB 内核 PT 仍深拷贝）  | ✅    | paging.c               |
| PAG-5  | context\_destroy 用 num\_page\_tables 短路非 present PDE        | ✅    | paging.c               |
| PAG-8  | COW fault handler 一次锁内完成                                  | ✅    | paging.c               |
| PAG-9  | context\_clone malloc 失败处理                                  | ✅    | paging.c               |
| PAG-11 | fork COW 只遍历 present PDE                                     | ✅    | paging.c + multitask.c |

### 已知遗留（低优先级）

| #     | 问题                     | 说明         | 优先级 |
| ----- | ------------------------ | ------------ | ------ |
| PAG-7 | 无 page table slab cache | 预分配 PT 池 | 低     |

***

## Phase 2: Memory Manager 升级 ✅ 已完成

### 已完成修复

| #       | 项                                                                              | 状态 | 位置                     |
| ------- | ------------------------------------------------------------------------------- | ---- | ------------------------ |
| MM-Bug1 | **prev 合并方向错误**（严重 Bug）：header 留在 chunk 地址，size 虚增 → 内存越界 | ✅    | memory\_manager.c free() |
| MM-Bug2 | **next 合并后 self->tail 未更新**：tail 指向被吸收的 chunk → 野指针             | ✅    | memory\_manager.c free() |
| MM-2    | expand\_heap 从逐页 malloc 改为 reserve\_bulk + 批量 map                        | ✅    | memory\_manager.c        |
| MM-2b   | expand\_heap 分配后与 tail 空闲 chunk 合并（减少碎片）                          | ✅    | memory\_manager.c        |

### 已知遗留（Phase 4 SMP 范畴）

| #    | 问题                                     | 说明                            | 优先级 |
| ---- | ---------------------------------------- | ------------------------------- | ------ |
| MM-3 | 无 per-type cache（task / fd / pipe 走通用 class） | kmem\_cache 专用精确尺寸 cache | 低     |

***

## Phase 3: Multitask 调度器升级 ✅ 已完成

### 已完成修复

| #           | 项                                                                                                                                                                                                                                                                                                              | 状态   | 位置                      |
| ----------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------ | ------------------------- |
| MT-Exit     | **exit stub 修复**：asm → C 函数 + cli/sti 原子写 ZOMBIE + 清空 g\_current\_task\_ptr 防野指针                                                                                                                                                                                                                  | ✅      | context\_switch.c         |
| MT-1/5      | **pid hash table**（256 桶，链地址法）：zombie parent 查找 O(n)→O(1)，wait\_pid 查找 O(n)→O(1)                                                                                                                                                                                                                  | ✅      | multitask.h + multitask.c |
| MT-Wait     | WAITING 唤醒用 pid hash 替代 O(n) 扫表                                                                                                                                                                                                                                                                          | ✅      | multitask.c schedule()    |
| MT-7        | exec init\_user 失败已正确 return -1（确认无需改动）                                                                                                                                                                                                                                                            | ✅ 确认 | multitask.c               |
| MT-8        | fork COW 已 skip non-present PDE（确认无需改动）                                                                                                                                                                                                                                                                | ✅ 确认 | multitask.c               |
| MT-SlotIdx  | **slot\_idx 维护 UAF 修复**：free 紧凑删除（末尾搬到被删位置）未更新被搬任务的 slot\_idx → 该任务 slot\_idx 失效 → schedule fallback 失败 → 越界写入错误位置 → tasks\[] stale 指针 → UAF。加 `tasks[idx]==task` 校验 + 搬移后更新被搬任务 slot\_idx                                                             | ✅      | multitask.c               |
| MT-RqUB     | **rq\_dequeue** **`__builtin_ctz(0)`** **UB 修复**：先 ctz 后判空, GCC 在 UB 假设下可能优化掉判空 → runqueue 真空时 ctz 返回垃圾值 → `rq[l]` 越界 → `first=&head`（自指）→ `list_del(head)` 删 head 自己 → runqueue 彻底损坏 → schedule `return cpustate` 死循环卡死。调换为先判空再 ctz                        | ✅      | multitask.c               |
| MT-ForkList | **fork 浅拷贝链表节点修复**：`*child=*parent` 复制了 `rq_node`/`zombie_node`（指向 parent 节点地址, 非 child 自己）只重置了 `pid_hash_node` → `task_on_rq(child)` enqueue 前误判 true + `jlos_task_free` 误判 child 在 zombie 链表。补 `jlos_list_init` 重置两个链表节点                                        | ✅      | multitask.c               |
| MT-Cleanup  | **冗余字段/死代码清理**：删 `jlos_task_t.fds_size`（边界检查全用 `JLOS_TASK_FDS_NUM` 常量, 该字段仅写不读）+ 删 `set_blocked`/`set_waiting` 中 `yield=true` 死赋值（两函数同时改 status, 进不了 schedule 的 RUNNING 分支读 yield）。`yield` 字段本身保留（`syscall_yield`/`need_resched`/降优先级判断真实使用） | ✅      | multitask.h + multitask.c |

### 设计决策

| 决策                                         | 理由                                                                                        |
| -------------------------------------------- | ------------------------------------------------------------------------------------------- |
| 用 pid hash 替代 parent 指针 + children 链表 | parent 被 free 后 hash 自动返回 NULL，无悬空指针风险；当前无信号/ptrace，不需要 parent 指针 |
| task struct 仅加 1 个字段 (`next_hash`)      | 不加 `parent`/`children_head`/`next_sibling`/`prev_sibling`，将来加信号系统时再补           |
| 保持 swap-with-last O(1) 删除                | 倒序遍历 + swap 不会跳过元素（已确认正确）                                                  |

### 已知遗留（低优先级）

| #    | 问题                           | 说明                             | 优先级 |
| ---- | ------------------------------ | -------------------------------- | ------ |
| MT-3 | O(n) 调度选择（4 层 × n 任务） | per-level runqueue 可优化为 O(1) | 低     |
| MT-4 | 静态 256 task 数组             | 256 上限对当前足够               | 低     |
| MT-6 | 单全局 runqueue                | per-CPU runqueue 留给 Phase 4    | 低     |

***

## Phase 4: SMP 预留（多 CPU 准备）

本阶段不实现 SMP，而是为 SMP 预留清晰的扩展点。当前单核实现中就把架构搭好，避免未来重写。

| 序号 | 任务                                                                               | 涉及文件                   | 复杂度 |
| ---- | ---------------------------------------------------------------------------------- | -------------------------- | ------ |
| 4.1  | PFA: per-CPU frame cache (order-0 batch refill/drain)                              | page\_frame\_allocator.c/h | 中     |
| 4.2  | Paging: TLB shootdown IPI 抽象接口 `jlos_hal_paging_tlb_shootdown(cpu_mask, addr)` | hal/paging.h + 实现占位    | 低     |
| 4.3  | MM: per-CPU freelist + batch drain to global                                       | memory\_manager.c          | 中     |
| 4.4  | Multitask: per-CPU runqueue + task->cpu + task->cpumask                            | multitask.h + multitask.c  | 高     |
| 4.5  | HAL: 抽象 `jlos_hal_get_cpu_id()` / `jlos_hal_num_cpus()`                          | hal/cpu.h                  | 低     |
| 4.6  | Spinlock: ticket lock 替换当前实现（为多核公平性）                                 | hal/spinlock.c             | 中     |

***

## Phase 5: 核心四子系统生产级优化（v3.0）

在 Phase 0-4 基础功能全通 + 经典结构就位后，聚焦**稳定性能 + 多核可扩展 + 多架构可移植**三项目标。按依赖顺序分 4 个子阶段推进，每步独立可回滚。依赖关系：`Phase 5.1（锁粒度+正确性）→ 5.2（性能热点）→ 5.3（经典化/扩展性）→ 5.4（SMP/多架构预留）`。

### 5.1 锁粒度细化 + 正确性收紧（高优，先做，低侵入）

| ID | 任务 | 根因与影响 | 涉及文件 | 复杂度 | 状态 |
| --- | --- | --- | --- | --- | --- |
| ~~PFA-1~~ | ~~拆 `s_pfa_lock` 为 buddy + refcount + owner 三套锁~~ | **已实现**：refcount 用 `jlos_atomic_add_unless`/`cmpxchg` 完全原子化（无锁），owner 独立 `s_owner_lock`，buddy 独立 `s_buddy_lock`，`s_free_frames` 原子计数 | page_frame_allocator.c | — | ✅ 已完成 |
| ~~PFA-6~~ | ~~`set_owner_type` 用独立 `s_owner_lock`~~ | **已实现**：`s_owner_lock` 已保护 owner/type 读写，与 buddy 锁无交叉 | page_frame_allocator.c | — | ✅ 已完成 |
| ~~MM-1a~~ | ~~拆 `s_mm_lock` 为 `s_slab_lock` + `s_heap_lock`~~ | **已实现**：slab 路径用 `s_slab_lock`，heap 路径用 `s_heap_lock`，数据结构无交叉，消除全局串行化 | memory_manager.c | — | ✅ 已完成 |
| MM-2 | `kvfree` DIRECT_MAP 区域非 SLAB_OBJ/KV_CONTIG type 加 assertion 日志 | 分支已独立无交叉，但非预期 type 静默 return 无日志，难排查 | memory_manager.c | 低 | 部分完成（缺 assertion） |
| PAG-7 | `jlos_active_paging_context` HAL 化为 per-CPU 访问器 | 裸全局指针，SMP 下多核 switch 乱序；HAL 抽象方便 ARM/RISC-V | paging.c / hal/paging.h | 中 | 待做 |

### 5.2 性能热点（高频路径 O(1) / 跳空扫 / 批量化）

| ID | 任务 | 根因与影响 | 涉及文件 | 复杂度 | 状态 |
| --- | --- | --- | --- | --- | --- |
| ~~PAG-1~~ | ~~`context_destroy` 4MB PDE 批量释放~~ | **已实现**：destroy 对 4MB PDE 已 `free_order(MAX_ORDER)` 单次释放；unmap 4MB 用户区分支 `free_bulk(1024)` 无创建者，属死路径 | paging.c | — | ✅ 已完成 |
| ~~PAG-2~~ | ~~`pt_used_count[1024]` 替 PT 空扫~~ | **已实现**：判空用帧级 `pt_present_count` O(1)，map/unmap/clone/destroy 全程维护；再加一层计数属负优化 | paging.h / paging.c | — | ✅ 已完成 |
| PAG-4 | 内核 4KB PDE 共享（4MB PDE 已浅拷贝共享；低直接区 + heap 的 4KB PT 每进程仍深拷贝） | Linux 内核半页共享方案：boot 期低直接 4KB 段全局一次置 USER（删 `create_user_mm` 逐进程 `change_flags_range`）→ clone 内核半区浅拷贝 / destroy 跳过内核半区 → 内核 vmalloc/heap 边界 PT boot 期预建 | paging.c + multitask.c | 高 | 待做（可选） |
| ~~PAG-6~~ | ~~`change_flags_range` 批量 TLB 刷新~~ | **已实现**：< 阈值逐页 flush、≥ 阈值循环外统一 `flush_all_tlb`，非 active context 不刷 | paging.c | — | ✅ 已完成 |
| ~~MT-3~~ | ~~fork COW PT 内层连续 non-present 跳过~~ | **已实现短路**：fork 已 skip 非 present PDE + `pt_present_count==0` 整 PT 跳过；残余仅省分支无内存读收益，正解随 F11 mmap/VMA 按映射区遍历 | multitask.c (fork) | — | ✅ 已完成 |
| MT-1 | wait queue + timer 唤醒经典化 | 前置 swtch()（MT-5）已落地；sleep/wait 仍靠 `schedule()` 对全任务 O(n) 扫描唤醒。改为 wait queue（`task.wait_node` 已备）+ 到期 timer node，删 O(n) 扫描 | multitask.h / multitask.c + sync.c | 高 | 待做 |
| MT-2 | exit→zombie 直连父进程唤醒 | `jlos_sched_wake_waiter` 仍全表扫；改用 `parent_pid` + pid hash O(1) 命中唯一合法 waiter（WAITING && waiting_pid==pid），连带删 `schedule()` 的 WAITING+zombie 分支 | multitask.c (exit + wake + schedule) | 低 | 待做 |
| MT-5 | **实现 `swtch()` 汇编上下文切换** | 已落地为 Linux/xv6 式单轨切换：`jlos_hal_context_switch(old_sp,new_sp)` 保存/恢复 esp+callee-saved；`schedule()` 改 void 内部 swtch 不再返回 cpustate；中断返回路径从「`movl %eax,%esp` 重建现场」改为「现场冻结在任务内核栈、swtch 切回后就地 iret」；新任务（kernel/user）与 fork 子进程都用 swtch 帧首次进入；阻塞原语（sleep/wait_pid/sem）改主动 schedule 而非 spin。multitask ALL PASSED | arch/x86/switch.s + context_switch.c + interrupts_asm.s + multitask.c + sync.c + syscall.c | 高 | ✅ 已完成 |
| ~~PFA-2~~ | ~~`free_bulk` 两级合并~~ | **已实现**：free_bulk 先收集 refcount→0 的帧到数组，排序后连续合并，一次 `buddy_free_range` 释放 | page_frame_allocator.c | — | ✅ 已完成 |

### 5.3 经典化 / 内存布局 / 扩展性

| ID | 任务 | 根因与影响 | 涉及文件 | 复杂度 | 状态 |
| --- | --- | --- | --- | --- | --- |
| ~~PFA-4~~ | ~~引入 `struct jlos_page` 数组~~ | **已实现**：`s_pages[]` 已是 `jlos_page_t` 数组，含 flags/refcount/order/type/u(free_list|owner)，物理帧数据 100% 留给用户 | page_frame_allocator.h / .c | — | ✅ 已完成 |
| MM-3 | size class 从纯 2^n 改精细粒度表，平均膨胀 < 15% | 当前 `mm_size_to_class(513)` → 1024B 浪费 50%；高频 80~1500B 浪费严重 | memory_manager.h / .c | 中 | 待做 |
| MT-4 | `tasks[256]` 静态数组动态化 | 256 上限对网络服务很快打顶；pid wrap 无冲突判定 | multitask.h / multitask.c | 中 | 待做 |

### 5.4 SMP / 多架构预留接口

| ID | 任务 | 说明 | 涉及文件 | 复杂度 |
| --- | --- | --- | --- | --- |
| PAG-5 | HAL 层加 `jlos_hal_paging_global_pages(enable)`（x86 CR4.PGE）+ `jlos_hal_paging_asid_alloc/free`（ARM/RISC-V 留占位，x86 返回 0）；内核页 PTE 统一加 `JLOS_PTE_GLOBAL` 标志 | 减少 CR3 切换导致的内核 TLB 全刷；为 ARM 等有 ASID 的架构预留钩子 | hal/paging.h + 各 arch 实现占位 / paging.c | 低 |
| PFA-5 | 对外多帧 API：`jlos_page_frame_alloc_n(npages)`（合并 reserve_bulk + malloc）/ `jlos_page_frame_free_n(ptr, npages)` | 目前 malloc 单帧 + reserve_bulk 多帧两条独立路径，调用方用错会泄漏；统一出口便于批量优化 | page_frame_allocator.h / .c | 低 |
| MM-5 | `memset`/`memcpy` 32B 展开 + SSE2 宽写（`movdqa`/`movdqu`），函数头 `clts` 清 CR0.TS + 尾 `mov %cr0` 恢复 | 现在 32-bit 逐字写 4B/cycle，COW 4KB 拷贝慢 2~3 倍；CR0.TS 置位保证 #NM lazy FPU 语义不变 | memory_manager.c（汇编块或内联 asm） | 中 |

***

## 实施优先级

| 优先级 | Phase | 任务                                                                | 依赖        |
| ------ | ----- | ------------------------------------------------------------------- | ----------- |
| **P0** | 0     | Buddy Page Frame Allocator                                          | ✅ 已完成    |
| **P0** | 1     | Paging 全部修复 (per-context lock + clone 浅拷贝 + COW 锁合并等)    | ✅ 已完成    |
| **P0** | 2     | Memory Manager 修复 (prev 合并方向 + tail 更新 + 批量 expand\_heap) | ✅ 已完成    |
| **P0** | 3     | Multitask 升级 (exit stub 修复 + pid hash + O(1) zombie/wait_pid)  | ✅ 已完成    |
| **P1** | 5.1   | 锁粒度细化 + 正确性收紧（MM-2 收尾、PAG-7 HAL 化）                | Phase 0-3 ✅ |
| **P1** | 5.2   | 性能热点（MT-2 直连唤醒、MT-1 wait queue 经典化；PAG-4 内核 4KB PDE 共享可选） | Phase 5.1 完成 |
| **P2** | 5.3   | 经典化 / 扩展性（MM-3 size class 精化、MT-4 task 表动态化、per-type cache） | Phase 5.2 完成 |
| **P3** | 4.x/5.4 | SMP 预留 + 多架构 HAL 接口（PAG-5、PFA-5、MM-5、4.1~4.6）        | Phase 5.3 完成 |

***

## 待实现功能（按经典路线图）

| 编号 | 功能                                                     | 模块                              | 依赖               |
| ---- | -------------------------------------------------------- | --------------------------------- | ------------------ |
| F1   | 按需分页 (demand paging, 已实现 brk + stack)             | paging.c                          | 无                 |
| F2   | COW 写时复制 (已实现)                                    | paging.c + multitask.c            | —                  |
| F3   | 用户态 malloc/free                                       | user                              | 依赖 brk（已完成） |
| F4   | FAT32 完善 (mount/open/close/read/write/seek/readdir)    | filesystem                        | 依赖块设备完善     |
| F5   | open/close 系统调用                                      | syscall.c                         | 依赖 F4            |
| F6   | ELF 用户态程序加载                                       | user                              | 依赖 F4            |
| F7   | signal/kill 信号机制                                     | syscall.c + multitask.c           | 无                 |
| F8   | 线程支持（共享地址空间）                                 | multitask.c                       | 依赖 COW (F2)      |
| F9   | DHCP 自动获取 IP                                         | net                               | 无                 |
| F10  | DNS 域名解析                                             | net                               | 依赖 F9            |
| F11  | mmap 内存映射                                            | paging.c + syscall.c              | 依赖 F4            |
| F12  | FPU/SSE 上下文切换（CR0.TS + lazy save/restore）✅ 已完成 | arch/x86/fpu.c + hal/ext\_state.h | —                  |
| F13  | O(1) 调度选择（per-level runqueue + bitmap）             | kernel/multitask.c                | 无                 |

***

## 已知遗留（低优先或需更大重构）

| #   | 问题                       | 说明                                                                               |
| --- | -------------------------- | ---------------------------------------------------------------------------------- |
| A1  | 用户态代码与内核混编       | 经典做法用户态独立 ELF + 加载器；当前靠 PTE\_USER 共享 .text/.rodata；安全隔离性弱 |
| A2  | 0\~1MB 恒等映射残留        | GRUB 退出后不再需要，经典做法移除                                                  |
| A3  | boot\_page\_dir 内存未释放 | 切到内核页表后 4KB 无法回收                                                        |
| A4  | MLFQ 不 starvation-free    | CFS weighted fair queuing 更经典但复杂度高；当前可接受                             |

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

***

## 参考资料

1. Intel x86 Architecture Manual
2. OSDEV Wiki：<https://wiki.osdev.org/>
3. 《Operating Systems: Three Easy Pieces》
4. 《Modern Operating Systems》 - Andrew Tanenbaum
5. Linux kernel source (buddy allocator, SLUB, scheduler)
6. FreeBSD VM system design
