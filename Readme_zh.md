# JohnBeauty OS (JLOS)

> 从 0 写起的 C 语言 32-bit x86 保护模式宏内核：**多任务抢占式调度（PIT 100Hz RR）**、**完整 TCP/IP 7 层协议栈**、**PCI/ATA/PS2/VGA/AMD PCnet-FAST III 网卡驱动**、**FAT16/FAT32 文件系统**、**Composite Widget GUI**、**6 级完整 HAL 硬件抽象层**。

---

## ✨ 功能亮点一览

| 类别 | 已实现功能 |
|---|---|
| 🧠 **内核核心** | 32-bit 保护模式 flat 段、分页/堆 malloc/free(双堆切换)、PIT 100Hz 抢占调度、IDT+双8259 PIC 动态屏蔽 |
| 🧩 **HAL 层 (6级完成)** | IO ops 运行时多态 + jlos_hal_info_t 只读硬件信息 + 统一设备模型(资源冲突检测+回滚) + 可重入自旋锁irqsave + mfence/lfence/sfence 内存屏障 + 环形I/O诊断 Trace |
| 🚗 **驱动** | AMD am79c973 PCI 网卡(20 RX + 8 TX desc 环)、ATA PIO-28 硬盘、PS/2 键盘、PS/2 鼠标、VGA 字符/图形模式、16550 UART COM1、8237 DMA |
| 🌐 **网络** | Ethernet II ⇄ ARP(128 cache + timeout) ⇄ IPv4(路由 + checksum) ⇄ ICMP(ping reply) ⇄ UDP/TCP Socket ⇄ HTTP 1234 + UDP 5678 echo server |
| 📁 **文件系统** | 块设备 HAL 抽象、FAT16/FAT32 BPB 解析、8.3 目录项、簇链、MS-DOS 风格路径解析（兼容 Unix `/` 和 DOS `\`） |
| 🖼️ **GUI** | Desktop 根容器 → Window 可拖动窗口 → Button/Label/EditBox Widget 组合模式；hit-test 鼠标事件分发 |
| 🔧 **工具** | memory_test / multitask_test / hard_driver_test / http_server_test / udp_server_test 5 个测试用例 + debug_console |

---

## 📂 项目目录结构

```
johnbeauty-os/
├── arch/x86/               # x86 底层：loader.s、GDT/IDT、interruptstubs.s(4-byte int#)、context_switch、port I/O、PCI cfg、syscall 0x80
├── hal/                    # HAL 硬件抽象层（6级全部完成）：io/irq/mmu/syscall/context/barrier/spinlock/timer/pci/serial/block/dma/device/diag
├── kernel/                 # 内核核心：kernel_main 三阶段启动、memory_manager(双堆)、multitask(RR调度)
├── drivers/                # 驱动：driver_manager + amd_am79c973 + ata_pio28 + ps2_keyboard + ps2_mouse + vga
├── net/                    # 网络栈：etherframe + arp + ipv4 + icmp + udp + tcp
├── filesystem/             # 文件系统：fat(16/32) + msdospath
├── gui/                    # GUI：desktop + window + widget(composite pattern)
├── common/                 # 共用头：types.h(JLOS_NET_MAX_SLOTS)、multiboot.h、graphics.h
├── tools/                  # 配置(config.h) + tests/* + samples/debug_console
├── docs/                   # 📚 架构文档（见下方索引）
├── linker.ld               # ELF i386 链接脚本
├── Makefile                # gcc -m32 -nostdlib 构建入口
├── HAL_DESIGN.md           # HAL 6级升级设计蓝图
└── Readme_zh.md            # 本文档
```

---

## 🏗️ 整体架构图

> GitHub / Typora / VS Code + Mermaid 插件可直接渲染为图；不支持时下面 ASCII 兜底可读。

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
            MM["内存管理器<br/>(主堆 + 低地址堆双切换)"]
            Sched["任务调度器<br/>(RR 轮转，PIT 100Hz 抢占)"]
            Intr["中断/异常管理<br/>(PIC 8259A 动态屏蔽)"]
        end
        subgraph HAL["🧩 HAL 硬件抽象层（6 级完成）"]
            IOOps["⚡ IO ops 运行时多态"]
            HALInfo["📋 jlos_hal_info_t 只读"]
            DeviceMgr["🔌 统一设备模型+冲突检测"]
            SyncPrim["🔒 spinlock + barrier"]
            HWWrap["🔧 PIT/PCI/Serial/ATA/DMA"]
            Diag["🔎 环形 I/O Trace"]
        end
        subgraph Drivers["🚗 驱动层（drivers/）"]
            DrvMgr["driver_manager"]
            NetDrv["AMD am79c973 (IRQ 0x0B)"]
            ATA["ATA PIO-28 (IDE)"]
            Input["PS/2 键盘 + 鼠标"]
            VGA["VGA 字符/图形"]
        end
        subgraph Arch["🏛️ x86 架构（arch/x86/）"]
            Loader["loader.s → GDT → C"]
            GDT_IDT["GDT(flat 4GB) + IDT(256)"]
            IntrAsm["interruptstubs.s pusha/popa"]
            CtxSw["context_switch 切 task 栈"]
            PortIO["port.s in/out(slow jmp)"]
            PciArch["PCI Mechanism #1"]
            Syscall["syscall int 0x80"]
        end
        SyscallHAL --> Intr
        Servers  --> NetStack
        NetStack -->|IRQ 0x0B 收发包| NetDrv
        FS       -->|LBA 扇区读写| ATA
        GUI      -->|framebuffer| VGA
        GUI      -->|按键/坐标| Input
        Core     --> HAL
        HAL      --> Drivers
        Drivers  --> Arch
        MM       --> Sched
        Intr     -->|PIT IRQ0 调度点| Sched
        Sched    -->|切栈| CtxSw
    end

    subgraph Hardware["💻 硬件（VMware/QEMU/真机）"]
        direction LR
        CPU["x86 CPU + CPUID"]
        PIT["8253 PIT (IRQ0,100Hz)"]
        PIC["双 8259 PIC (IRQ0-15)"]
        PCIHost["PCI Host 桥"]
        UART["16550 UART COM1"]
        IDE["IDE Primary/Secondary"]
        PS2["PS/2 控制器"]
        VGAHW["VGA 兼容卡"]
        DMAC["8237 DMA"]
        NIC["AMD PCnet-FAST III"]
    end
    Arch --> Hardware
```

**ASCII 架构兜底视图**：
```
┌───────────────────────────────────────────────────────────────────────────────┐
│  用户态多任务(task_A/task_B…)  │ TCP 1234 HTTP server  │  UDP 5678 echo        │
└──────────────┬────────────────┴──────────────────┬──────┴───────────┬───────────┘
               │ syscall int 0x80                  │  network stack   │  FS/GUI
┌──────────────▼───────────────────────────────────▼──────────────────▼───────────┐
│  🧠  kernel/  memory_manager(双堆)  │  task_manager(RR PIT100Hz)  │  intr mgr    │
└──────────────┬───────────────────────────────────┬──────────────────┬───────────┘
               │ 所有硬件操作 HAL 隔离              │ 驱动模型          │ 资源 claim
┌──────────────▼───────────────────────────────────▼──────────────────▼───────────┐
│  🧩  HAL 6级  │  jlos_hal_info_t  │  device 资源树  │  spinlock+barrier  │  I/O trace │
│  io ops 运行时多态  │  PIT/PCI/Serial/ATA Block/DMA 全包装                       │
└──────────────┬───────────────────────────────────┬──────────────────┬───────────┘
               │  driver activate                  │ am79c973 NIC     │ PS/2 + ATA
┌──────────────▼───────────────────────────────────▼──────────────────▼───────────┐
│  🚗  drivers/  driver_manager  │  am79c973  │  ATA PIO-28  │  keyb / mouse / VGA │
└──────────────┬───────────────────────────────────┬──────────────────┬───────────┘
               │  in/out + PCI cfg + IRQ            │  arch/x86 port.s │ IDT stubs
┌──────────────▼───────────────────────────────────▼──────────────────▼───────────┐
│  🏛️  arch/x86  │  loader.s → GDT/IDT  │  interruptstubs.s  │  context_switch    │
│  port I/O jmp$+2  │  PCI 0xCF8/CFC  │  syscall 0x80 门                         │
└──────────────┬───────────────────────────────────┬──────────────────┬───────────┘
               ▼                                   ▼                  ▼
   8253 PIT / 双8259 PIC / 16550 UART / AMD am79c973 / IDE / PS2 / VGA / 8237 DMA
```

---

## 🚀 启动流程（GRUB → 多任务）

```mermaid
sequenceDiagram
    participant GRUB
    participant L as arch/x86/loader.s
    participant K as kernel/kernel.c
    participant MM as kernel/memory_manager.c
    participant HAL as hal/hal.c
    participant PIT as hal/timer.c
    participant DRV as drivers/*
    participant NET as net/*
    participant TS as tools/tests
    participant SCH as kernel/multitask.c

    GRUB->>L: multiboot magic 0x2BADB002 + EBX=info
    L->>K: 关中断 → 开 PE → GDT flat → call kernel_main
    K->>HAL: jlos_hal_arch_init() 注册 5 平台设备 + IO/IRQ claim
    K->>MM:  低地址堆初始化（PCI BAR 需要 <4MB）
    K->>DRV: PCI 枚举 → am79c973 绑定(IRQ 0x0B, INIT块32B对齐)
    K->>MM:  切回主堆(0x0EDCF000 ~)
    K->>DRV: 网卡 activate: STOP→CSR4→CSR1/2→INIT→STRT(0x42)
    K->>PIT: jlos_hal_timer_start_periodic(100) (IRQ0 调度节拍)
    K->>DRV: 键盘/鼠标驱动 heap 分配 + 注册 handler
    Note over K: PIC 默认 mask=0xFA (仅 IRQ0/2 开；注册 handler 自动 unmask)
    K->>K: asm("sti")  ⚠️ 必须在 network_init 前(否则 ARP 死锁)
    K->>NET: network_init() Ether→ARP→IPv4→ICMP→UDP→TCP
    NET->>NET: ARP 广播求网关 MAC (timeout 5,000,000 次)
    K->>TS:  memory_test → multitask_test(2task × 10line) → ATA(简化模式)
    loop 每 10ms PIT IRQ0
        PIT->>SCH: jlos_task_manager_schedule → RR 轮转切任务
    end
    K->>TS:  Starting HTTP server on port 1234...
    K->>TS:  UDP server listening on port 5678
```

**关键硬约束（踩坑总结）**：
1. `am79c973 INIT 块` 必须 **32 字节对齐**；`CSR3/CSR5` 禁止手动写，硬件从 INIT 块读
2. 网卡初始化序列：`STOP → CSR4 → CSR1/CSR2 → INIT → STRT(0x42=STRT\|INEA)`；`CSR0.IENA(0x0400)=1`
3. `sti()` 必须在 `network_init()` 之前（ARP resolve 要收中断包）
4. 中断 stub 用 **`movl/pushl` 4 字节 int#**，不能用 `movb/pushb` 1 字节（栈错位 → EIP 垃圾）
5. `jlos_cpu_state_t` 成员顺序 100% 匹配 interruptstubs.s push：`eax ebx ecx edx esi edi ebp error eip cs eflags`
6. `cpustate` **嵌入 jlos_task_t**，不能放 task 栈上（否则中断 pusha 覆盖下次调度现场）
7. VMware 环境：忽略 IRQ13 (int 0x0D) / int 0x2E 伪中断；ATA 测试简化模式防 #GP

---

## 📚 详细架构文档索引

所有子系统深度文档见 [docs/jlos_kernel_readme.md](./docs/jlos_kernel_readme.md) 入口：

| 文档 | 子系统 | 核心内容 |
|---|---|---|
| [docs/hal/jlos_hal_subsystem_readme.md](./docs/hal/jlos_hal_subsystem_readme.md) | HAL 硬件抽象层 | IO ops 多态表 + jlos_hal_info_t + 统一设备模型 + 资源冲突回滚 + 可重入自旋锁 + 环形 I/O Trace 诊断 |
| [docs/kernel/jlos_kernel_subsystem_readme.md](./docs/kernel/jlos_kernel_subsystem_readme.md) | kernel/ 核心 | 三阶段硬件启动 + 双堆内存管理器 + PIT 100Hz RR 调度 + PIC 动态屏蔽 + printf 线程安全 |
| [docs/net/jlos_net_subsystem_readme.md](./docs/net/jlos_net_subsystem_readme.md) | net/ 网络协议栈 | 7层分层 + AMD RX/TX 描述符环 + 收/发包时序图 + ARP cache + Socket 数组 |
| [docs/filesystem/jlos_filesystem_subsystem_readme.md](./docs/filesystem/jlos_filesystem_subsystem_readme.md) | filesystem/ 文件系统 | 块 HAL → BPB → FAT16/32 簇链 → 8.3 目录项 → MS-DOS 路径解析 |
| [docs/hdc/jlos_drivers_subsystem_readme.md](./docs/hdc/jlos_drivers_subsystem_readme.md) | drivers/ 驱动层 | driver_manager 生命周期 + AMD 网卡 7 条硬约束 + ATA 寄存器表 + PCI 枚举绑定流程 |
| [docs/gui_arch/jlos_gui_arch_subsystem_readme.md](./docs/gui_arch/jlos_gui_arch_subsystem_readme.md) | GUI + arch/x86 | GDT/IDT 布局 + 中断栈压入顺序详解 + 上下文切换时序 + Composite Widget 树 |

---

## 🔧 编译与运行

### 环境依赖
```
gcc-multilib  (支持 gcc -m32)
nasm + binutils (i386 as / ld melf_i386)
grub-pc-bin + xorriso  (grub-mkrescue 生成可引导 ISO)
运行：VMware Workstation / QEMU (qemu-system-i386)
```

### 构建命令
```bash
cd johnbeauty-os
make            # 生成 johnkernel.iso（GRUB2 multiboot）
make clean      # 清理 obj / johnkernel.bin / johnkernel.iso
```

开启 HAL I/O 诊断追踪（查"写 CF8 后网卡中断丢失"类竞态）：
```bash
CFLAGS_EXTRA="-DHAL_CONFIG_TRACE_IO=1" make clean all
# 运行到怀疑点：调用 jlos_hal_trace_dump(128) 打印最近 128 条 in/out 记录
```

### QEMU 直接运行
```bash
qemu-system-i386 -cdrom johnkernel.iso -serial stdio -netdev user,id=n0 -device pcnet,netdev=n0
```

### ✅ 启动成功关键字段（串口/VGA 输出）
```
princess yihan is safe and happy!     # kernel_main 第 1 行（公主平安开心）
initializing hardware, stage 1..3 start
switched to low memory manager for PCI driver allocation
AMD am79c973 PCI command: 0x00000007   IRQ=0B  interrupt=2B
POST-START CSR0=0x01F3  STRT=01 INEA=01 INTR=01 RXON=01 TXON=01
.interrupts activated
.NET: [ OK ] EtherFrame → ARP → IPv4(gw 192.168.159.1/24) → ICMP → UDP → TCP
Starting HTTP server on port 1234...
UDP server listening on port 5678
task: A  task: B  ... × 10 轮      # PIT 100Hz 抢占调度正常
```

---

## 🎯 设计哲学

| 原则 | 落地方式 |
|---|---|
| **隔离优先** | 所有硬件操作统一走 HAL；驱动/Kernel/Net/FS 不直接调 arch/x86 port API（便于未来移植 ARM/RISC-V） |
| **安全优先** | PIC 默认仅开 IRQ0/IRQ2；IO/IRQ/MMIO/DMA 资源 claim 冲突立即返回负错误码；设备注册失败自动回滚 |
| **0 开销抽象** | `HAL_CONFIG_TRACE_IO=0` → `#if` 剪掉整段诊断代码，编译后 0 字节；spinlock inline = 1 条 xchg + mfence |
| **可调试优先** | 自旋锁可重入 irqsave、环形 512 条 I/O Trace；KERNEL_CONFIG_DEBUG_NETWORK/LOG 三级日志 |

---

## 📖 背景 / 源项目

1. 原始教程（C++ 版，Viktor Engelmann 德国）：<https://www.youtube.com/playlist?list=PLHh55M_Kq4OApWScZyPl5HhgsTJS9MZ6M>
2. 学习搬运视频（B 站）：<https://www.bilibili.com/video/BV1Ng411x7As>
3. 原作者主页：<http://www.algorithman.de/Autor/index.php>
4. 语言选择：原教程 C++（有优雅命名空间）；本项目重写为纯 C（考虑内核可移植性 + 编译器支持广度）。
5. ⚠️ GUI 提示：建议不要开启 GUI 图形模式（老硬件/某些 VMware 版本对 VGA 寄存器有损坏风险），默认用串口/VGA 文本模式即可。

---

## ❓ 历史 Question（留档）

### 1. GDT i[0]/i[1] 顺序问题
原 `gdt.cpp:10` 代码：
```cpp
i[0] = (uint32_t)this;
i[1] = sizeof(GlobalDescriptorTable) << 16;
// ↓ 原作者可能误写成以下相反
i[1] = (uint32_t)this;
i[0] = sizeof(global_descriptor_table) << 16;
```
> 在 `interrupts.activate()` 后启动虚拟机失败，提示虚拟 CPU 异常。**交换 i[0] 和 i[1] 后**，正常收到硬件中断。（GDTR 低 16 位是 limit，高 32 位是 base）

### 2. Mouse 颜色反转问题
原 `mouse.cpp:60` 点击颜色代码：
```cpp
for (uint8_t i = 0; i < 3; i++)
    if ((buffer[0] & (1<<i)) != (buttons & (1<<i)))
        VideoMemory[80*y+x] = ((VideoMemory[80*y+x] & 0xF000)>>4)
                             | ((VideoMemory[80*y+x] & 0x0F00)<<4)
                             | ((VideoMemory[80*y+x] & 0x00FF));
```
> 加上这段后，光标点击移动时**初始位置颜色不会恢复**（翻转后未复位）。本项目 VGA 驱动改为独立鼠标光标缓冲避免。

---

## 📋 历史 TODO（早期规划，已完成项保留对照）

| 历史 TODO | 当前状态 |
|---|---|
| 网卡仅 AMD am79c973，Intel 芯片机器跑不了 | ✅ 现跑 VMware/QEMU（都模拟 AMD PCnet-FAST III），功能全覆盖；Intel e1000 为未来可选驱动 |
| 文件系统未实现 | ✅ 已实现 FAT16/FAT32 + 块设备 HAL + MS-DOS 路径解析；见 filesystem/ 目录和文档 |

---

## 💡 Tips

> 公主平安开心 🌸
