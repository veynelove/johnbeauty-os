# JLOS (JohnBeauty Operating System) — 内核架构总览

> 一个从 0 写起的 32 位 x86 保护模式单体内核：支持多任务抢占、TCP/IP 协议栈、PCI/ATA/键盘/鼠标/AMD PCnet-FAST III 网卡驱动、FAT16/FAT32 文件系统、GUI 窗口小部件，以及完整的 6 层 HAL 硬件抽象层。

---

## 1. 目录结构

```
johnbeauty-os/
├── arch/x86/               # x86 平台启动与底层机制（loader、GDT、IDT、中断汇编、任务切换、PCI、port I/O、syscall）
├── hal/                    # 🧩 HAL 硬件抽象层（6 级扩展完成，隔离 arch 与上层）
├── kernel/                 # 🧠 内核核心：启动流程、内存管理、多任务调度
├── drivers/                # 🚗 设备驱动：PCI/ATA/VGA/键盘/鼠标/AMD am79c973 网卡 + driver 管理器
├── net/                    # 🌐 网络协议栈（7 层：Ether → ARP/IPv4 → ICMP/UDP/TCP → socket/servers）
├── filesystem/             # 📁 文件系统（FAT16/32 + MS-DOS 路径 + 块设备 HAL）
├── gui/                    # 🖼️ GUI（desktop 桌面、window 窗口、widget 控件）
├── common/                 # 📚 共用头：types.h、multiboot.h、graphics.h
├── tools/                  # 🔧 配置 + 测试用例（memory/multitask/hard_driver/http/udp）+ debug_console
├── linker.ld               # 链接脚本（ELF i386 布局）
├── Makefile                # 构建入口（gcc -m32 -nostdlib，生成 GRUB multiboot ISO）
└── HAL_DESIGN.md           # HAL 设计蓝图（6 级升级路径）
```

---

## 2. 整体架构图（Mermaid，可渲染成图）

> 支持 Mermaid 的 Markdown 渲染器（GitHub / Typora / VS Code + 插件 / GitLab）会直接渲染为架构图；不支持时下方有 ASCII 替代视图。

```mermaid
flowchart TB
    UserApp(["用户态：多任务（task_A / task_B …）"]) -->|syscall int 0x80| SyscallHAL

    subgraph KernelSpace["🧠 内核空间（32-bit ring 0）"]
        direction TB

        subgraph AppLayer["🌐 应用 / 协议 / FS / GUI"]
            Servers["🖥️ 测试 Servers<br/>TCP 1234 (HTTP)<br/>UDP 5678"]
            NetStack["🌐 网络栈<br/>Ether ⇄ ARP/IPv4 ⇄ ICMP/UDP/TCP ⇄ Sockets"]
            FS["📁 文件系统<br/>FAT16/FAT32 ⇄ MS-DOS 路径 ⇄ 块 HAL"]
            GUI["🖼️ GUI<br/>Desktop ⇄ Window ⇄ Widget"]
        end

        subgraph Core["🧠 核心子系统（kernel/）"]
            MM["内存管理器<br/>(主堆 + 低地址堆，malloc/free)"]
            Sched["任务调度器<br/>(RR 轮转，PIT 100Hz 抢占)"]
            Intr["中断/异常管理<br/>(PIC 8259A 动态屏蔽)"]
        end

        subgraph HAL["🧩 HAL 硬件抽象层（6 级完成）"]
            HALInfo["📋 jlos_hal_info_t<br/>(只读硬件探测结果)"]
            DeviceMgr["🔌 统一设备模型<br/>jlos_device_t + 资源冲突检测"]
            IOOps["⚡ IO ops 运行时多态<br/>(x86 fast/slow 表调度)"]
            SyncPrim["🔒 同步原语<br/>spinlock(可重入irqsave) + barrier(mfence/lfence/sfence)"]
            HWWrap["🔧 硬件包装<br/>PIT/PCI/Serial/ATA Block/8237 DMA"]
            Diag["🔎 诊断 Trace<br/>环形缓冲 I/O trace + dump"]
        end

        subgraph Drivers["🚗 驱动层（drivers/）"]
            DrvMgr["driver_manager 驱动管理器"]
            NetDrv["AMD am79c973<br/>PCI 网卡 (IRQ 0x0B)"]
            ATA["ATA PIO-28<br/>(IDE 硬盘)"]
            Input["PS/2 键盘 + 鼠标"]
            VGA["VGA 字符/图形"]
        end

        subgraph Arch["🏛️ x86 架构（arch/x86/）"]
            Loader["loader.s<br/>(GRUB multiboot→C)"]
            GDT_IDT["GDT(flat 段) + IDT(256门)"]
            IntrAsm["interruptstubs.s<br/>(pusha/popa 4-byte int#)"]
            CtxSw["context_switch.c<br/>(切 task 栈 + cpustate)"]
            PortIO["port.s<br/>(in/out / inl/outl slow jmp)"]
            PciArch["PCI Mechanism #1<br/>(0xCF8/0xCFC)"]
            Syscall["syscall int 0x80 门"]
        end

        SyscallHAL --> Intr
        Servers  --> NetStack
        NetStack -->|收发包 IRQ 0x0B| NetDrv
        FS       -->|扇区读写| ATA
        GUI      -->|输出| VGA
        GUI      -->|输入| Input

        Core     --> HAL
        HAL      --> Drivers
        Drivers  --> Arch
        MM       --> Sched
        Intr     -->|PIT IRQ0 调度点| Sched
        Sched    -->|上下文切换| CtxSw
    end

    subgraph Hardware["💻 硬件（VMware / QEMU / 真机）"]
        direction LR
        CPU["x86 CPU (支持 CPUID)"]
        PIT["8253 PIT (IRQ0, 100Hz)"]
        PIC["双 8259 PIC (IRQ 0-15)"]
        PCIHost["PCI Host 桥"]
        UART["16550 UART COM1"]
        IDE["IDE 控制器"]
        PS2["PS/2 控制器(kbd/mouse)"]
        VGAHW["VGA 兼容图形卡"]
        DMAC["8237 DMA"]
        NIC["AMD PCnet-FAST III (am79c973)"]
    end

    Arch --> Hardware
```

**ASCII 等效架构图（兜底）**：

```
┌───────────────────────────────────────────────────────────────────────────────┐
│  用户态多任务(task_A/task_B…)  │ TCP 1234  HTTP server  │  UDP 5678 echo        │
└──────────────┬────────────────┴──────────────────┬──────┴───────────┬───────────┘
               │ syscall int 0x80                  │  network stack   │  FS/GUI
┌──────────────▼───────────────────────────────────▼──────────────────▼───────────┐
│  🧠  kernel/ (核心)  memory_manager  │  task_manager(RR, PIT 100Hz)  │  intr mgr │
└──────────────┬───────────────────────────────────┬──────────────────┬───────────┘
               │ 所有硬件操作必经 HAL               │ 驱动模型          │ 设备资源注册
┌──────────────▼───────────────────────────────────▼──────────────────▼───────────┐
│  🧩  HAL (6 层完成)  │  jlos_hal_info_t  │  device tree  │  spinlock + barrier  │
│  io ops 运行时多态  │  PIT/PCI/Serial/ATA Block/DMA 包装  │  I/O 环形 diag trace │
└──────────────┬───────────────────────────────────┬──────────────────┬───────────┘
               │  调 driver activate               │ am79c973 NIC     │ PS/2 + ATA
┌──────────────▼───────────────────────────────────▼──────────────────▼───────────┐
│  🚗  drivers/  driver_manager  │  am79c973  │  ATA PIO-28  │  keyb / mouse / VGA │
└──────────────┬───────────────────────────────────┬──────────────────┬───────────┘
               │  in/out + PCI cfg + IRQ            │  port.s 原始     │ IDT stubs
┌──────────────▼───────────────────────────────────▼──────────────────▼───────────┐
│  🏛️  arch/x86  │  loader.s → GDT/IDT  │  interruptstubs.s  │  context_switch  │
│  port I/O (slow jmp $+2)  │  PCI 0xCF8/CFC  │  syscall 0x80 门                 │
└──────────────┬───────────────────────────────────┬──────────────────┬───────────┘
               ▼                                   ▼                  ▼
   8253 PIT / 双8259 PIC / 16550 UART / AMD am79c973 / IDE / PS2 / VGA / 8237 DMA
```

---

## 3. 启动流程（GRUB → 多任务）

```mermaid
sequenceDiagram
    participant GRUB
    participant L as arch/x86/loader.s
    participant G as arch/x86/gdt.c
    participant K as kernel/kernel.c (kernel_main)
    participant MM as kernel/memory_manager.c
    participant HAL as hal/hal.c (jlos_hal_arch_init)
    participant IRQ as hal/irq.c
    participant PIT as hal/timer.c
    participant DRV as drivers/*
    participant NET as net/* (network_init)
    participant TS as tools/tests
    participant SCH as kernel/multitask.c (scheduler)

    GRUB->>L: multiboot 信息 → EAX=0x2BADB002
    L->>G: 关中断 + 开保护模式 + 加载 GDT(flat 段)
    L->>K: 跳转到 kernel_main(multiboot_info)
    K->>HAL: jlos_hal_arch_init()  注册 5 个平台设备 + IO/IRQ claim
    HAL-->>K: 返回 jlos_hal_io_ops = x86 fast
    K->>MM: 低地址内存管理器初始化（PCI BAR 用）
    K->>DRV: PCI 枚举 → am79c973 驱动绑定 (IRQ=0x0B)
    K->>MM: 切回主内存管理器
    K->>IRQ: IDT 初始化 + double fault/未定义 opcode handlers
    K->>DRV: 网卡 activate: STOP → CSR4 → CSR1/2(INIT 块32B对齐) → INIT → STRT(0x42)
    K->>PIT: jlos_hal_timer_start_periodic(100)  (IRQ0 100Hz 调度节拍)
    K->>DRV: Keyboard / Mouse 驱动注册 + heap 分配
    Note over K,DRV: 动态 PIC mask：默认只开 IRQ0(timer)+IRQ2(cascade)，注册 handler 才开
    K->>K: asm volatile("sti")  必须在 network_init 前开中断(否则 ARP 死锁)
    K->>NET: network_init()  Ether→ARP→IPv4→ICMP→UDP→TCP 分层初始化
    NET->>NET:  ARP 广播求网关 MAC (timeout 5,000,000 次防 hlt 死锁)
    K->>TS: memory_test → multitask_test(2 tasks × 10 lines) → ATA(简化模式)
    K->>SCH:  task_manager_add_task(task_A / task_B)
    loop PIT IRQ0 (每 10ms)
        PIT->>SCH: jlos_task_manager_schedule(cpustate)  → RR 轮转切任务
    end
    K->>TS: http_server 1234 + udp_server 5678 启动
```

**关键启动规则（硬约束，见 project_memory.md）**：
1. `am79c973 INIT 块` 必须 **32 字节对齐**
2. 初始化序列：`STOP → CSR4 → CSR1/CSR2 → INIT → STRT(0x42=STRT|INEA)`
3. `CSR3/CSR5` **禁止手动写**，硬件从 INIT 块自动读
4. `sti` 必须在 `network_init()` **之前**（ARP 解析需要收中断包）
5. 中断 stub 用 **4 字节 int#**（movl/pushl），不能用 1 字节 movb/pushb
6. `jlos_cpu_state_t` 严格按 pusha 顺序：eax→ebx→ecx→edx→esi→edi→ebp→error→eip→cs→eflags

---

## 4. 子系统索引文档

| 文档（相对路径） | 子系统 | 内容 |
|---|---|---|
| [hal/jlos_hal_subsystem_readme.md](./hal/jlos_hal_subsystem_readme.md) | HAL 硬件抽象层 | 6 级设计总览 + ops 多态 + 设备模型 + 同步 + trace 图 |
| [kernel/jlos_kernel_subsystem_readme.md](./kernel/jlos_kernel_subsystem_readme.md) | kernel/ 核心 | 内存管理器 + 多任务调度 + 中断/异常 + 三阶段硬件 init |
| [net/jlos_net_subsystem_readme.md](./net/jlos_net_subsystem_readme.md) | net/ 网络协议栈 | 7 层协议栈分层图 + AMD 网卡 RX/TX 描述符环 + ARP/TCP/UDP Socket |
| [filesystem/jlos_filesystem_subsystem_readme.md](./filesystem/jlos_filesystem_subsystem_readme.md) | filesystem/ | FAT16/FAT32 + MS-DOS 路径 + 块设备 HAL 映射 |
| [hdc/jlos_drivers_subsystem_readme.md](./hdc/jlos_drivers_subsystem_readme.md) | drivers/ 驱动 | driver_manager + am79c973 环 + ATA + PS/2 输入 + VGA |
| [gui_arch/jlos_gui_arch_subsystem_readme.md](./gui_arch/jlos_gui_arch_subsystem_readme.md) | GUI + arch/x86 | Desktop/Window/Widget + GDT/IDT/interrupt stubs/ctx switch |

---

## 5. 编译与运行

### 环境要求
- GCC（支持 `-m32`）、GNU Binutils（`--32` as / `melf_i386` ld）
- `grub-mkrescue` + `xorriso`（生成可引导 ISO）
- 运行：VMware / QEMU（`qemu-system-i386 -cdrom johnkernel.iso`）

### 构建命令
```bash
cd /home/veyne/johnbeauty-os
make          # 生成 johnkernel.iso（multiboot + GRUB2）
make clean    # 清理 obj/johnkernel.bin/johnkernel.iso
```

开启 HAL I/O trace（诊断"某PCI驱动写CF8后网卡中断丢失"）：
```bash
CFLAGS_EXTRA="-DHAL_CONFIG_TRACE_IO=1" make clean all
# 运行到断点处：调用 jlos_hal_trace_dump(128) 打印最近 128 条 I/O
```

### 启动日志关键字段（成功标志）
```
princess yihan is safe and happy!   // kernel_main 第一行
initializing hardware, stage 1..3 start
switched to low memory manager for PCI driver allocation
AMD am79c973 PCI command: 0x00000007   IRQ=0B  interrupt=2B
POST-START CSR0=0x01F3  STRT=01 INEA=01 INTR=01 RXON=01 TXON=01
.interrupts activated
.NET: [ OK ] EtherFrame → ARP → IPv4(gw 192.168.159.1/24) → ICMP → UDP → TCP
Starting HTTP server on port 1234...
UDP server listening on port 5678
task: A  task: B  ...(×10 轮)   // 多任务抢占调度正常
```

---

## 6. 设计哲学

- **隔离优先**：所有硬件操作走 HAL，驱动/Kernel 不直接写 arch/x86 API（便于以后换 ARM/RISC-V）
- **安全优先**：IRQ 动态 mask + 资源冲突检测（IO/IRQ/MMIO 重复 claim 立即报错）
- **0 开销抽象**：`HAL_CONFIG_TRACE_IO=0` / `diag` 关 → 整段 `#if` 包掉，编译后 0 字节
- **可调试优先**：自旋锁 irqsave 可重入 + 环形 I/O trace，能诊断竞态与跨设备冲突
