# JohnBeauty OS 内核优化计划

版本: v2.0 | 日期: 2026-07-31 | 作者: JohnLove

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
| 虚拟内存 | 4KB 分页 + 页目录/页表 + CR3 切换 + 内核/用户隔离 | 256MB 映射，ring3 用户进程正常运行 |
| 内存管理 | 页帧分配器 + 低内存堆(0x50000) + 主堆(32MB) + 16字节最小分配 | DMA 缓冲一致性，cache disable 映射 |
| 系统调用 | exit/fork/read/write/printf/get_errno/get_pid/yield/sleep/wait_pid/debug_tasks | per-thread errno + wait_pid 退出码 |
| 多任务调度 | MLFQ 四级反馈队列 + 老化升级 + 抢占/协作可切换 | 时间片轮转正常，交互型优先 |
| 同步原语 | 信号量 + 互斥锁(可重入+所有权传递) + 条件变量 | spinlock 关中断保护 |
| IPC | 管道(堆分配环形缓冲) + 消息队列(柔性数组) | FIFO 顺序，close/destroy 分离 |

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

## 阶段一：虚拟内存管理（已完成核心，持续优化）

**已完成**：

- [x] x86 4KB 分页机制 + 页表/页目录管理
- [x] CR3 切换 + 虚拟地址空间映射
- [x] 内核/用户空间隔离
- [x] 低内存区域 cache disable 映射（DMA 一致性）

**待优化**：

- [ ] Page Fault 处理完善（区分非法访问 vs 合法缺页）
- [ ] 写时复制（COW）fork 优化
- [ ] 按需分页（demand paging）
- [ ] 用户态 brk 系统调用
- [ ] mmap 内存映射

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

**已完成配套**：

- [x] per-thread errno
- [x] 用户空间指针校验（copy_from_user / copy_to_user）
- [x] 错误码定义（SYSCALL_ENOMEM / EFAULT / ENINVAL 等）

**待扩展**：

- [ ] 文件描述符表（每进程独立 fd 表）
- [ ] pipe 系统调用（管道暴露给用户态）
- [ ] open/close 系统调用（依赖 FAT32）
- [ ] 用户态 malloc/free（依赖 brk）
- [ ] signal/kill 信号机制
- [ ] 用户态同步原语接口（sem/mutex 系统调用）

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

**待扩展**：

- [ ] Zombie 进程回收（wait_pid 后释放 PCB）
- [ ] 线程支持（共享地址空间）
- [ ] 用户态同步原语接口
- [ ] 定时器（timer_create/timer_settime）
- [ ] 死锁检测（可选）

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
| 高 | 1. 分页 + 虚拟内存基础 | 已完成 |
| 高 | 2. 进程管理增强（PCB + 状态机 + MLFQ） | 已完成 |
| 高 | 3. 系统调用核心（exit/fork/read/write/wait_pid） | 已完成 |
| 高 | 4. 同步原语（信号量/互斥锁/条件变量） | 已完成 |
| 高 | 5. IPC（管道/消息队列） | 已完成 |
| 高 | 6. Zombie 进程回收 + 文件描述符表 + Page Fault | 进行中 |
| 中 | 7. COW 写时复制 + 用户态 brk + pipe 系统调用 | 待开始 |
| 中 | 8. 用户态 malloc/free + 按需分页 + 线程支持 | 待开始 |
| 中 | 9. 用户态同步原语 + signal/kill + 定时器 + mmap | 待开始 |
| 中 | 10. FAT32 文件系统 | 待开始 |
| 中 | 11. 网络性能优化 + DHCP/DNS | 待开始 |
| 低 | 12. 磁盘交换机制 | 待开始 |
| 低 | 13. slab/伙伴系统 + 设备驱动框架 | 待开始 |

---

## 开发日志

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
