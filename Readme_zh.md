# JohnBeauty OS (JLOS)

---

## ✨ 功能亮点一览

| 类别 | 已实现功能 |
| --- | --- |
| 🧠 **内核核心** | 32-bit 保护模式 flat 段、分页/堆 malloc/free(双堆切换)、PIT 100Hz 抢占调度、IDT+双8259 PIC 动态屏蔽 |
| 🧩 **HAL 层 (6级完成)** | IO ops 运行时多态 + jlos_hal_info_t 只读硬件信息 + 统一设备模型(资源冲突检测+回滚) + 可重入自旋锁irqsave + mfence/lfence/sfence 内存屏障 + 环形I/O诊断 Trace |
| 🚗 **驱动** | AMD am79c973 PCI 网卡(20 RX + 8 TX desc 环)、ATA PIO-28 硬盘、PS/2 键盘、PS/2 鼠标、VGA 字符/图形模式、16550 UART COM1、8237 DMA |
| 🌐 **网络** | Ethernet II ⇄ ARP(128 cache + timeout) ⇄ IPv4(路由 + checksum) ⇄ ICMP(ping reply) ⇄ UDP/TCP Socket ⇄ HTTP 1234 + UDP 5678 echo server |
| 📁 **文件系统** | 块设备 HAL 抽象、FAT16/FAT32 BPB 解析、8.3 目录项、簇链、MS-DOS 风格路径解析（兼容 Unix `/` 和 DOS `\`） |
| 🖼️ **GUI** | Desktop 根容器 → Window 可拖动窗口 → Button/Label/EditBox Widget 组合模式；hit-test 鼠标事件分发 |
| 🔧 **工具** | memory_test / multitask_test / hard_driver_test / http_server_test / udp_server_test 5 个测试用例 + debug_console |

---

---

## 关键硬约束（踩坑总结）

1. `am79c973 INIT 块` 必须 **32 字节对齐**；`CSR3/CSR5` 禁止手动写，硬件从 INIT 块读
2. 网卡初始化序列：`STOP → CSR4 → CSR1/CSR2 → INIT → STRT(0x42=STRT\|INEA)`；`CSR0.IENA(0x0400)=1`
3. `sti()` 必须在 `network_init()` 之前（ARP resolve 要收中断包）
4. 中断 stub 用 **`movl/pushl` 4 字节 int#**，不能用 `movb/pushb` 1 字节（栈错位 → EIP 垃圾）
5. `jlos_cpu_state_t` 成员顺序 100% 匹配 interruptstubs.s push：`eax ebx ecx edx esi edi ebp error eip cs eflags`
6. `cpustate` **嵌入 jlos_task_t**，不能放 task 栈上（否则中断 pusha 覆盖下次调度现场）
7. VMware 环境：忽略 IRQ13 (int 0x0D) / int 0x2E 伪中断；ATA 测试简化模式防 #GP

---

## 🔧 编译与运行

### 环境依赖

```text
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

1. 根目录auto_build_run.test文件存放有.bat文件内容，可用于自动编译加载iso，不过在运行之前需要修改文件内容:

```text
(ssh -t veyne@192.168.159.128 "cd ~/johnbeauty-os/ && make clean && make"), 修改虚拟机ssh地址，以及虚拟机中项目地址
(scp veyne@192.168.159.128:~/johnbeauty-os/johnkernel.iso C:\Users\johnbeauty\Desktop\johnkernel.iso), 修改iso复制到windows的路径，这里的路径要与虚拟机配置的iso路径一致，这样就可以覆盖旧的iso
(start "" "F:\vmware\vmplayer.exe" "E:\johnbeauty\johnbeauty.vmx"), 这里是命令行启动虚拟机，因虚拟机不同而不同，我用的是vmplayer 17
```

1. 另外，kernel默认开启了com1串口打印，所有的日志都会输出到串口中，可以在虚拟机"编辑虚拟机设置"中，找到"串行端口"选项，在连接中选择“使用输出文件"选中一个在windows本地任意位置创建的文件，比如
2. "C:\Users\johnbeauty\Desktop\log.txt"文件。这样，运行虚拟机后，日志就会输出两份，一份在虚拟机终端显示，一份存在log.txt文件中。方便复制查看日志。

3. 开启 HAL I/O 诊断追踪（查"写 CF8 后网卡中断丢失"类竞态）：

```bash
CFLAGS_EXTRA="-DHAL_CONFIG_TRACE_IO=1" make clean all
# 运行到怀疑点：调用 jlos_hal_trace_dump(128) 打印最近 128 条 in/out 记录
```

### ✅ 启动成功关键字段（串口/VGA 输出）

```text
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

## 🏗️ 整体架构图

```text
╔══════════════════════════════════════════════════════════════════════════════════╗
║                        🧑‍💻  用户态（多任务 + 测试服务）                           ║
╠════════════════════════════╦═══════════════════════╦═════════════════════════════╣
║  task_A · task_B … (RR)    ║  HTTP @ TCP :1234    ║  UDP  echo @ :5678          ║
║  └─→ syscall  int 0x80 ──┐ ║  └─→ socket()/listen║  └─→ socket()/sendto        ║
╚═══════════════════════════╬╩═══════════════════════╩═════════════════════════════╝
                            ║         ↑ 向上回调            ↑ on_data()
                            ▼         │                  │
╔══════════════════════════════════════════════════════════════════════════════════╗
║  🌐  应用协议 / 文件系统 / GUI  （直接调 HAL 拿资源）                             ║
╠══════════════════════════╦═══════════════════╦═══════════════════╦══════════════╣
║  NET 7 层协议栈          ║  文件系统 FAT     ║  GUI 窗口控件     ║  测试        ║
║  Ether→ARP→IPv4→ICMP    ║  BPB + FAT16/32   ║  Desktop→Window  ║  multitask   ║
║  → UDP/TCP → sockets[]  ║  目录项 + 簇链    ║  → Widget(组合)   ║  memory/ATA  ║
╚═════════════╦════════════╩═══════╦════════════╩═══════╦═══════════╩══════════════╝
              ║   AMD IRQ0x0B      ║   LBA 扇区 R/W      ║   键盘/鼠标/VGA
              ▼                    ▼                    ▼
╔══════════════════════════════════════════════════════════════════════════════════╗
║  🧠  内核核心子系统（kernel/）   ring 0                                            ║
╠══════════════════════════╦═════════════════════════════╦══════════════════════════╣
║  内存管理器  双堆切换    ║  调度器  RR 时间片         ║  中断异常  PIC 动态屏蔽  ║
║  低地址堆 ←→ 主堆        ║  PIT 100Hz  IRQ0 触发     ║  256 门 + 未注册 IRQ 屏蔽║
║  jlos_malloc / free      ║  TERMINATED 跳过           ║  VMware IRQ13 / 0x2E 忽略║
╚═════════════╦════════════╩═══════════╦═════════════════╩═══════════╦══════════════╝
              ║   HAL API 统一入口     ║   调度点 IRQ0              ║   handler 注册
              ▼                        ▼                            ▼
╔══════════════════════════════════════════════════════════════════════════════════╗
║  🧩  HAL 硬件抽象层（6 级全部完成）  架构隔离核心                                  ║
╠═══════╦════════╦═════════╦═══════════════════╦═══════════╦═══════════════════════╣
║ IO 函数║ 只读   ║ 同步原语║  统一设备模型      ║ 硬件包装  ║ 诊断 I/O Trace         ║
║ ops表  ║ jlos_  ║ spinlock║  jlos_device_t    ║ PIT/PCI   ║ 环形 512 条           ║
║ 多态   ║ hal_info║ barrier║  资源冲突回滚     ║ UART/ATA  ║ HAL_CONFIG_TRACE_IO   ║
╚═══════╩═════╦══╩═════════╩═══════╦═════════════╩═════╦═════╩═══════════╦═══════════╝
              ║   driver 激活       ║   资源 claim        ║   调 driver 实现   ║
              ▼                     ▼                    ▼                  ▼
╔══════════════════════════════════════════════════════════════════════════════════╗
║  🚗  驱动子系统（drivers/）  堆上分配对象 jlos_malloc                                ║
╠════════════════╦════════════════╦═══════════════╦═════════════════╦═══════════════╣
║  driver_       ║  AMD am79c973  ║  ATA PIO-28   ║  PS/2 键盘/鼠标  ║  VGA 文本/图形 ║
║  manager 生命周期║  20 RX / 8 TX ║  28bit LBA    ║  扫描码 → ASCII  ║  0xB8000 文本  ║
║  add/activate  ║  INIT 32B 对齐 ║  0x1F0~0x1F7  ║  3 字节鼠标包    ║  0xA0000 图形  ║
╚════════════════╩══════╦═════════╩═══════╦═════════╩═══════╦═════════════╩═══════╦═══════╝
                       ║   in/out + IRQ     ║   arch/x86       ║   原始 port I/O    ║
                       ▼                    ▼                 ▼                    ▼
╔══════════════════════════════════════════════════════════════════════════════════╗
║  🏛️  x86 平台层（arch/x86/）  32-bit protected mode                              ║
╠════════════╦══════════════╦═════════════════╦═════════════════╦═══════════════════╣
║  loader.s  ║  GDT flat    ║  interruptstubs ║  context_switch ║  PCI Mechanism#1  ║
║  GRUB → C  ║  0x08 code   ║  pushl 4字节!!  ║  切 ESP → 栈切换║  0xCF8 / 0xCFC    ║
║  0x2BADB002║  0x10 data   ║  error code分支 ║  cpustate 嵌入  ║  syscall int 0x80  ║
╚════════════╩══════════════╩═════════════════╩═════════════════╩═══════════════════╝
                                          │
                                          ▼
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │  💻  硬件（VMware / QEMU / 真机）                                              │
  │  x86 CPU · 双 8259 PIC · 8253 PIT 100Hz · 16550 UART · PCI Host 桥           │
  │  IDE Primary · PS/2 控制器 · VGA 兼容卡 · 8237 DMA · AMD PCnet-FAST III 网卡 │
  └──────────────────────────────────────────────────────────────────────────────┘
```

## 💡 Tips

> 公主平安开心 🌸
