# JohnBeauty OS 内核优化计划

版本: v2.1 | 日期: 2026-08-09 | 作者: JohnLove

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
| 虚拟内存 | 3:1 高半核分页 + 内核/用户地址空间隔离 | 256MB 映射，ring3 用户进程正常运行 |
| 内存管理 | 页帧分配器 + 低内存堆(0x50000) + 主堆(32MB) + 16字节最小分配 | DMA 缓冲一致性，cache disable 映射 |
| 系统调用 | exit/fork/read/write/printf/get_errno/get_pid/yield/sleep/wait_pid/brk/pipe/fd_close | per-thread errno + wait_pid 退出码 |
| 多任务调度 | MLFQ 四级反馈队列 + 老化升级 + 抢占/协作可切换 | 时间片轮转正常，交互型优先 |
| 同步原语 | 信号量 + 互斥锁(可重入+所有权传递) + 条件变量 | spinlock 关中断保护 |
| IPC | 管道(堆分配环形缓冲) + 消息队列(柔性数组) | FIFO 顺序，close/destroy 分离 |
| 用户进程 | ring3 用户态进程 + 用户栈映射 + TSS 特权级切换 | Hello from ring3 + Wake up 验证通过 |

---

## 优化总览

| 阶段 | 模块 | 优先级 | 状态 |
|------|------|--------|------|
| 一 | 虚拟内存管理 | 高 | 已完成，优化中 |
| 二 | 系统调用接口 | 高 | 已完成核心，扩展中 |
| 三 | 多任务系统增强 | 高 | 已完成核心，扩展中 |
| 四 | 文件系统完善（FAT32） | 中 | 待开始 |
| 五 | 网络驱动优化 | 中 | 部分完成 |
| 六 | 高级特性 | 低 | 待开始 |

---

## 已知问题与优化建议

### 架构级

| 编号 | 问题 | 严重度 | 说明 |
|------|------|--------|------|
| A1 | 用户态代码与内核混编 | 中 | `jlos_user_*` 函数和字符串字面量链接在 `.text/.rodata`（内核空间 0xC0xxxxxx），靠 `PTE_USER` 共享。经典做法是用户态代码单独编译为独立 ELF 加载到用户地址空间，当前方案隔离性弱 |
| A2 | 0~1MB 恒等映射未移除 | 低 | 启动时映射的 0~1MB 恒等映射在内核运行后仍保留，应在 IDT 初始化后移除以增强安全性 |
| A3 | 具体架构代码边界 | 低 | 需持续检查 x86 代码不泄漏到 kernel 层，保持 HAL 抽象层纯净 |

### 网络驱动

| 编号 | 问题 | 严重度 | 说明 |
|------|------|--------|------|
| N1 | ARP 网关解析超时 | 中 | 日志显示 `ARP: resolve TIMEOUT`，网关 MAC 未解析成功。RXON=00 说明接收可能未正常开启 |
| N2 | 发送状态异常 | 中 | `POST-SEND CSR0=0x0043 RXON=00 TXON=00 SENT=00`，发送后 RX/TX 均未开启，SENT=0 疑似发送未完成 |
| N3 | DHCP/DNS 未实现 | 低 | 当前 IP 硬编码，缺少自动获取和域名解析 |

### 系统调用

| 编号 | 问题 | 严重度 | 说明 |
|------|------|--------|------|
| S1 | syscall_write 用 printf 输出 | 中 | 控制台输出用 `printf((const char *)buf)`，若 buf 含 `%` 字符会被误解为格式化符，应改为逐字符输出或 `puts` |
| S2 | syscall_write 每次 malloc/free | 低 | 每次 write 都 `jlos_malloc`/`jlos_free`，可用栈缓冲或预分配缓冲池优化 |
| S3 | fd < 3 的 len+1 逻辑不清晰 | 低 | `if (fd < 3 && len < MAX) { len += 1; }` 为控制台 fd 额外加 1 字节 null terminator，条件不够明确，应改为显式判断 fd 类型 |
| S4 | jlos_task_fd_close pipe 未实现 | 低 | 代码中有 `//todo`，pipe 关闭逻辑未完成 |

### 内存管理

| 编号 | 问题 | 严重度 | 说明 |
|------|------|--------|------|
| M1 | 用户栈固定单页 | 中 | 用户栈仅 4KB（1 页），可能不够深递归使用，应支持多页或可扩展栈 |
| M2 | jlos_process_fork 用户栈复制 | 中 | fork 时子进程用户栈是否正确复制/映射需验证 |
| M3 | loader 启动页目录浪费 | 低 | `boot_page_dir` 占 4KB，切到内核页目录后不再使用但仍占用内存 |
| M4 | jlos_paging_is_user_accessible 性能 | 低 | 每次检查遍历页表，无快速路径（已有 KERNEL_VIRTUAL_BASE 检查，可进一步优化） |

### 多任务调度

| 编号 | 问题 | 严重度 | 说明 |
|------|------|--------|------|
| T1 | 主线程 hlt 循环效率 | 低 | 主线程在 hlt 循环中，每个 PIT tick 都 save/restore main_thread_state，功能正确但略有浪费 |
| T2 | sleep 精度受限 | 低 | sleep 基于 tick 计数，精度为 10ms（100Hz），不支持更精细睡眠 |
| T3 | 非抢占式下任务不交替 | 低 | task_a/task_b 各跑完 10 次才切换，非抢占式下任务不主动让出 |

---

## 阶段一：虚拟内存管理（已完成核心，持续优化）

**已完成**：

- [x] x86 4KB 分页机制 + 页表/页目录管理
- [x] CR3 切换 + 虚拟地址空间映射
- [x] 3:1 高半核架构（内核 0xC0000000，用户 0-3GB）
- [x] 低内存区域 cache disable 映射（DMA 一致性）
- [x] 用户栈映射到用户地址空间（经典 access_ok 地址范围检查）
- [x] .text/.rodata PTE_USER 标志（ring3 syscall wrapper 可取指/读常量）

**待优化**：

- [ ] Page Fault 处理完善（区分非法访问 vs 合法缺页）
- [ ] 写时复制（COW）fork 优化
- [ ] 按需分页（demand paging）
- [ ] mmap 内存映射
- [ ] 移除启动期 0~1MB 恒等映射（A2）
- [ ] 用户栈多页支持（M1）

---

## 阶段二：系统调用接口（已完成核心，持续扩展）

**已完成系统调用**：

| 编号 | 名称 | 功能 | 状态 |
|------|------|------|------|
| 0 | EXIT | 退出进程 + exit_code | 已完成 |
| 1 | FORK | 创建子进程 | 已完成 |
| 2 | READ | 读 fd | 已完成 |
| 3 | WRITE | 写 fd / 字符串输出 | 已完成 |
| 4 | PRINTF | 打印字符串 | 已完成 |
| 5 | GET_ERRNO | 获取线程级 errno | 已完成 |
| 6 | GET_PID | 获取当前 PID | 已完成 |
| 7 | YIELD | 主动让出 CPU | 已完成 |
| 8 | SLEEP | 睡眠指定 tick | 已完成 |
| 9 | DEBUG_TASKS | 打印任务列表 | 已完成 |
| 10 | WAIT_PID | 等待子进程 + 退出码 | 已完成 |
| 11 | CREATE_PIPE | 创建管道 | 已完成 |
| 12 | TASK_FD_CLOSE | 关闭文件描述符 | 已完成 |
| 13 | TASK_BRK | 用户堆管理 | 已完成 |

**已完成配套**：

- [x] per-thread errno
- [x] 用户空间指针校验（copy_from_user / copy_to_user）
- [x] 错误码定义（SYSCALL_ENOMEM / EFAULT / ENINVAL 等）
- [x] 标准文件描述符预分配（fd 0=stdin, 1=stdout, 2=stderr）
- [x] jlos_user_puts 栈缓冲拷贝（修复 .rodata 指针被 access_ok 拒绝问题）

**待扩展**：

- [ ] syscall_write 修复 printf 格式化漏洞（S1）
- [ ] open/close 系统调用（依赖 FAT32）
- [ ] 用户态 malloc/free（依赖 brk）
- [ ] signal/kill 信号机制
- [ ] 用户态同步原语接口（sem/mutex 系统调用）
- [ ] pipe close 完整实现（S4）
- [ ] 用户态代码独立编译为 ELF（A1）

---

## 阶段三：多任务系统增强（已完成核心，持续扩展）

**已完成**：

- [x] MLFQ 四级反馈队列调度器（Level 0-3，时间片 2/4/8/16 ticks）
- [x] 老化升级机制（200 ticks 未运行自动升级）
- [x] 抢占式/协作式宏开关（KERNEL_CONFIG_PREEMPTIVE）
- [x] 任务状态机（READY/RUNNING/BLOCKED/TERMINATED/WAITING）
- [x] 集中状态转换函数（jlos_task_set_ready/running/blocked/terminated/waiting）
- [x] 信号量 + 互斥锁（可重入 + 所有权传递 + spinlock 保护）
- [x] 条件变量（cond_wait/signal/broadcast）
- [x] 管道（堆分配环形缓冲 + close/destroy 分离）
- [x] 消息队列（柔性数组 + FIFO 顺序）
- [x] ring3 用户进程（TSS 特权级切换 + 用户栈映射）
- [x] sleep/wake 机制（基于 tick 计数）

**待扩展**：

- [ ] Zombie 进程回收（wait_pid 后释放 PCB）
- [ ] 线程支持（共享地址空间）
- [ ] 用户态同步原语接口
- [ ] 定时器（timer_create/timer_settime）
- [ ] 死锁检测（可选）
- [ ] jlos_process_fork 用户栈复制验证（M2）

---

## 阶段四：FAT32 文件系统完善（待开始）

**当前状态**：仅基础 FAT32 结构定义

**目标 API**：

```c
mount/unmount -> open/close -> read/write/seek -> create/delete -> readdir
```

**配套**：文件权限 + 目录遍历 + 缓冲区缓存

---

## 阶段五：网络驱动优化（部分完成）

**已完成协议**：ARP IPv4 ICMP UDP TCP HTTP

**待完成**：

- [ ] ARP 网关解析超时排查（N1）
- [ ] 发送状态异常排查（N2）
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
| 中 | 7. syscall_write printf 漏洞 + ARP 超时排查 | 待开始 |
| 中 | 8. COW 写时复制 + 用户态 brk + pipe 系统调用 | 待开始 |
| 中 | 9. 用户态 malloc/free + 按需分页 + 线程支持 | 待开始 |
| 中 | 10. 用户态同步原语 + signal/kill + 定时器 + mmap | 待开始 |
| 中 | 11. FAT32 文件系统 | 待开始 |
| 中 | 12. 网络性能优化 + DHCP/DNS | 待开始 |
| 低 | 13. 用户态代码独立编译 ELF + 移除恒等映射 | 待开始 |
| 低 | 14. 磁盘交换机制 | 待开始 |
| 低 | 15. slab/伙伴系统 + 设备驱动框架 | 待开始 |

---

## 开发日志

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
