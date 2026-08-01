# JohnBeautY-OS 系统启动详解（从 GRUB 到中断激活）

> 最后更新：2026-07-04
> 适用代码版本：当前仓库 HEAD
> 阅读对象：内核开发者 / 架构学习者

---

## ⏱️ 启动总览时间轴（压缩版）

```
GRUB BIOS loader → arch/x86/loader.s → call_constructors
     → kernel/kernel.c:john_beauty_main(L44)
         L46  jlos_printk_init()            [能看见东西了]
         L47  jlos_hal_arch_init()          [划地盘：保留 IO/IRQ]
         L51  jlos_mmu_init(&mmu_ctx)       [分页壳子先建好]
         L55  低地址堆 init (0x50000, 320KB) [给老 PCI DMA <4MB 用]
         L59  主堆 init (16MB, 内存尾)      [真正干活的堆]
         L62  jlos_task_manager_init        [调度器空链表准备]
         L65  jlos_irq_manager_init(0x20)   [256 IDT + PIC remap + 动态 mask，不开中断]
         L68  jlos_syscall_init(int 0x80)   [用户态入口注册]
         L72  driver_manager_init           [驱动壳子]
      ╔══ Stage 1 硬件枚举 ══╗
      ║ L82  切 → 低地址堆     ║
      ║ L84  PCI 枚举绑驱动    ║  ← am79c973_probe，INIT block <4MB !!
      ║ L85  切 → 主堆         ║
      ╚═══════════════════════╝
      ╔══ Stage 2 驱动激活 ══╗
      ║ L89  activate_all      ║  ← am79c973: STOP→CSR4→CSR1/2→INIT→STRT(0x42) 严格序列
      ╚═══════════════════════╝
      ╔══ Stage 3 启动节拍 ══╗
      ║ L93  PIT 100Hz IRQ0   ║
      ║ L96  irq activate+sti  ║  ← 🔥 系统活了！中断全开
      ╚═══════════════════════╝
         L97+ 网络栈 6 层 init → 测试 → TCP/UDP 服务启动 → hlt idle
```

> 口诀（OS 开发者内话）：
> **先通打印 → 划地盘 → 建内存双堆 → 建任务/中断/系统调用壳 → 枚举 PCI 切堆 → 激活驱动 → 启 PIT → 最后 sti 开中断。**

---

## 🔵 第 0 阶段：GRUB 把接力棒交给我们

### GRUB 进入 loader 之前发生了什么？

1. 电脑通电 → BIOS POST 自检 → 读硬盘 MBR（前 512 字节）→ GRUB stage1 启动；
2. GRUB stage2 读入 ISO，搜索文件开头的 **Multiboot Header**（12 字节魔数）；
3. 找到合法 Header 后，GRUB 把 CPU 设置成下面的状态 **然后 `jmp loader`**：

| 寄存器 | GRUB 进入时的值 | 说明 |
|---|---|---|
| EAX | `0x2BADB002` | "我是 Multiboot 兼容 bootloader" 的标记 |
| EBX | `multiboot_info_t*` 物理地址 | 内存大小 / 模块列表 / mmap 都在这里 |
| CS | 32-bit 保护模式 flat code | GRUB 已经帮你开了 CR0.PE=1 |
| DS/ES/FS/GS/SS | 未定义 | **不要依赖！** 我们后面自己设 |
| EFLAGS | IF=0（关中断） | 所有中断屏蔽，直到最后一步 sti |
| ESP | 未定义 | GRUB 栈不可靠，我们 loader.s 第一时间自己设 |

### 0.1 Multiboot Header 结构

文件位置：[arch/x86/loader.s#L1-L9](./arch/x86/loader.s#L1-L9)

| 行号 | 内容 | 作用 |
|---|---|---|
| L1 | `.set MAGIC, 0x1badb002` | GRUB 搜索的"我是 Multiboot 内核"魔数 |
| L2 | `.set FLAGS, (1<<0 \| 1<<1)` | 两个启动要求：<br>`1<<0` = 内核需 **4KB 页对齐** 装入<br>`1<<1` = 填好 multiboot_info 内存信息（mem_upper 我们后面要用！）|
| L3 | `CHECKSUM = -(MAGIC+FLAGS)` | 让 MAGIC+FLAGS+CHECKSUM ≡ 0 (mod 2^32)，GRUB 校验不通过 = 拒绝启动 |
| L5-L9 | `.section .multiboot` + 三行 `.long` | 把 12 字节放进独立段。**必须在 kernel.bin 前 8KB 以内**，这是由 [linker.ld#L10-L13](./linker.ld#L10-L13) 保证的：<br>`*(.multiboot)` 放在 `.text` 最开头 + `. = ALIGN(8K)` 卡住边界。 |

---

## 🟠 第 1 阶段：汇编入口 arch/x86/loader.s

文件位置：[arch/x86/loader.s#L11-L36](./arch/x86/loader.s#L11-L36)

```asm
loader:
    cmp $0x2badb002, %eax    # L17
    jne _stop                # L18
```

> **L17-L18：防御性检查**。如果 EAX 不是 Multiboot 魔数（说明我们不是被 GRUB 带进来的，可能是被错误跳转），立刻 `_stop` 停机。正常启动永远走不到这里，但 Linux/Minix 都有这段，OS 开发教条：**永远别相信 bootloader。**

```asm
    mov $kernel_stack, %esp  # L20
```

> **L20：设内核栈——最关键的一行。**  
> 指向 BSS 段尾巴（[loader.s#L33-L36](./arch/x86/loader.s#L33-L36)）：
>
> ```asm
> .section .bss
> .align 16
> .space 4*1024*1024   ; 先留 4MB 空间（从低往高写）
> kernel_stack:        ; 栈顶 = 空间末尾（栈是从高往低 push）
> ```
>
> 为什么 4MB？
>
> - 中断嵌套越深，压栈越多（ISR 里 pusha = 8 个寄存器）；
> - 以后多任务每个 task 还要留独立栈（后面还要加）；
> - 4MB 是 2^22 对齐，SSE/AVX 指令 16 字节对齐不炸。

```asm
    call call_constructors   # L22
```

> **L22：调用 C constructor（.init_array 段）。**  
> 对应 [kernel/kernel.c#L37-L42](./kernel/kernel.c#L37-L42) 的 for 循环：
>
> ```c
> for (constructor* i = &__init_array_start; i != &__init_array_end; i++)
>     (*i)();
> ```
>
> 段边界由 [linker.ld#L19-L22](./linker.ld#L19-L22) 定义：
>
> ```
> __init_array_start = .;
> KEEP(*(.init_array));
> KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)));
> __init_array_end = .;
> ```
>
> 目前我们没有任何 `__attribute__((constructor))`，所以 start == end，循环 0 次立即返回，占位用。

```asm
    push %eax     # L24：参数 2 = multiboot magic（0x2BADB002）
    push %ebx     # L25：参数 1 = multiboot_info_t* 指针
    call john_beauty_main   # L26
```

> **L24-L26：正式进入 C 世界！**  
> cdecl 约定（C 函数调用 ABI）：**第一个参数最后 push**。所以：
>
> - 先 push EAX（第二个参数 `m_magicnumber`）
> - 再 push EBX（第一个参数 `multiboot_structure`）
>
> 刚好对应函数签名：
>
> ```c
> void john_beauty_main(
>     const multiboot_info_t *multiboot_structure,  // ← push %ebx, 参数1
>     uint32_t magicnumber                         // ← push %eax, 参数2
> );
> ```

```asm
_stop:
    cli          # L29：关中断（防止被打断唤醒 hlt）
    hlt          # L30：CPU 进入 C1 Halt 状态（省电）
    jmp _stop    # L31：万一 NMI（不可屏蔽中断）叫醒了，再回去睡死
```

> 只有 `john_beauty_main` 返回（ret 回来）才会执行到这里。但我们最后一行是 `for(;;) hlt`，永不返回，所以实际永远走不到。

---

## 🟢 第 2 阶段：C 入口 john_beauty_main（L44-L97）

文件位置：[kernel/kernel.c#L44-L118](./kernel/kernel.c#L44-L118)

---

### 🚩 L46：`jlos_printk_init()` — 「让你看得见东西」

文件位置：[kernel/printk.c#L7-L11](./kernel/printk.c#L7-L11)

```c
void jlos_printk_init(void) {
    jlos_spinlock_init(&s_printf_lock);   // L9：printf 用的可重入自旋锁
    jlos_hal_serial_default_init();       // L10：COM1 串口 115200-8N1 初始化
}
```

两个子步骤：

| 步骤 | 作用 | 为什么第一个做？ |
|---|---|---|
| **printf 锁 init** | 从静态初始值 `JLOS_SPINLOCK_INIT` 转成真正初始化好的锁（以后加 debug 计数 / 嵌套深度都依赖这步） | |
| **串口 init** | 16550 UART @ 0x3F8（COM1）：<br>设 LCR=0x80 → 写分频器 12（115200Hz）→ 关 FIFO → 关中断 | ⚠️ **OS 开发第一铁律：先打通「能看见报错」的通道！**<br>如果后面 jlos_hal_arch_init 炸了，没串口 init = 黑屏等死，根本不知道哪步挂的。 |

后面所有 `printf()` 都会**同时写两个地方**：

- COM1 串口（VMware/QEMU 看日志的出口）；
- VGA text buffer 0xB8000（虚拟机屏幕上显示的 80×25 字符）。

---

### 🚩 L47：`jlos_hal_arch_init()` — 「给硬件划地盘」

文件位置：[hal/hal.c](./hal/hal.c)

HAL 层的「平台资源注册」函数，内部核心：

1. **注册 5 个平台设备**（x86 PC/AT 生来就焊死在主板上的东西）：
   - 双 8259A PIC（IO 0x20/0xA0）
   - 8253 PIT（IO 0x40-0x43，IRQ0）
   - 16550 UART COM1（IO 0x3F8-0x3FF，IRQ4）
   - 8237 DMA（IO 0x00-0x0F / 0x80-0x8F）
   - PS/2 键盘鼠标控制器（IO 0x60/0x64，IRQ1/IRQ12）
2. **填 HAL 资源位图**：上面的 IO 端口 / IRQ 号全部置为「已 claim」，防止后面驱动注册时冲突（HAL 资源冲突检测）；
3. 返回 `jlos_hal_io_ops = x86_fast_ops`（`in`/`out` 不带 jmp delay 的 fast 版本；slow 版用于老 ISA 卡）。

一句话理解：**告诉 HAL「我们跑在 x86 PC/AT 上，这些地盘 BIOS 时代就占了，新来的驱动别去抢。」**

---

### 🚩 L48：`printf("princess yihan is safe and happy!\n")` — 「系统活了」标记

这是整个内核**第一个正式打印**的消息。如果屏幕上看不到它：

- 99% 可能是 [linker.ld](./linker.ld) 的内存基址（0x100000 = 1MB）错了，GRUB 没找到 C 入口；
- 0.9% 可能是 [loader.s L20](./arch/x86/loader.s#L20) 栈顶地址算炸，call printf 第一条 push 就 #GP 死机；
- 0.1% 可能是串口 init 全挂了（少见）。

---

### 🚩 L50-L51：`jlos_mmu_init(&mmu_ctx)` — 「分页壳子先建出来」

⚠️ **现在还没开分页（CR0.PG=0，物理地址直连）**，那 mmu_ctx 干嘛用？

`jlos_mmu_t` 里装的是：

- 页目录 PD（Page Directory，1024 PDE）
- 页表 PT 数组（Page Table，每个 1024 PTE）；
- 当前启用的页表根。

虽然现在没开 PG，但：

1. **中断管理器、任务管理器都要求传 mmu_ctx 指针当参数**——以后 #PF 缺页处理、切任务换页表、Copy-On-Write 都靠它；
2. 先「壳子挂全局」，后面子系统要存 mmu 信息就有地方挂；
3. 开分页只需要一条 `mov CR3, PD_Phys; mov CR0, PGbit` 的事，到时候直接用。

一句话：**先把"分页的户口本"办好，人口（任务）要落户总得有个地方。**

---

### 🚩 L53-L55：低地址堆 — 「给 PCI 老设备的 4MB 专用小堆」

```c
uint8_t* low_memory_heap = (uint8_t*)(0x50000);           // L53: 320KB 物理地址
jlos_memory_manager_t low_memory_manager_;                // L54
jlos_memory_manager_init(&low_memory_manager_,            // L55
                         low_memory_heap, 0x50000);       //       320KB 大小
```

⚠️ **这是项目踩过最痛的坑之一，不是随便写的地址！**

**坑王：AMD am79c973 (PCnet-FAST III) 网卡的 24-bit DMA 寻址限制**：
> 这个老网卡的 INIT 块、TX/RX descriptor ring 必须放在 **物理地址 < 4MB** 的连续内存里。它的 DMA 控制器只有 24 根地址线（高 8 位直接扔了）。

如果你把驱动结构 malloc 在主堆（地址 `0x0EDEXXXX` = 237MB，远高于 4MB）→ 网卡拿到被截断的低 24 位地址 → 读不到 INIT block → CSR0 直接读 `0xFFFF` → 网卡死锁，什么包都收不到。

所以「低地址小堆」就是专门给 PCI 24-bit DMA 老古董留的 VIP 专区。用完立刻切回去，别占着茅坑。

| 参数 | 选这个值的理由 |
|---|---|
| 起始 `0x50000` (320KB) | 避开 0-320KB 里的 BIOS IVT / BDA / EBDA 区域，刚好在可用 RAM 里 |
| 大小 320KB | 够 am79c973（≈200B 驱动结构 + 80×16B 描述符 + 20 个接收缓冲）+ ATA + PS/2 所有驱动用，留余量 |

---

### 🚩 L57-L59：主内存堆（16MB）— 「真正干活用的堆」

```c
uint8_t* heap_start = (uint8_t*)(
    1024 * (multiboot_structure->mem_upper - 1024*16));    // L57
jlos_memory_manager_init(&memory_manager_, heap_start,
                         1024*1024*16);                    // L59: 16MB 大小
```

地址计算拆解（例子：VMware 给 256MB 内存的情况）：

| 步骤 | 值 | 含义 |
|---|---|---|
| `multiboot_structure->mem_upper` | 262144 | GRUB 告诉我们的扩展内存大小（单位 KB）= 256 MB |
| `mem_upper - 1024*16` | 262144 - 16384 = 245760 | 给堆留 16MB 空间，取空间起始的 KB 地址 |
| `× 1024` | = 251658240 字节 = `0x0F000000` | 转成字节地址（你日志里 heap 是 `0x0EDCF000`，差一点是 GRUB 对齐 + multiboot mem_upper 取整的偏差，正常） |

为什么放「内存尾巴的倒数 16MB」而不是 1MB 紧挨着放？

1. **1MB-4MB 预留**：留给 .bss / 内核栈 / 低地址堆 / DMA bounce buffer 用；
2. **越界容错**：万一有人 malloc 然后越界写，写的是物理内存末尾的「空洞区」，不会踩坏 0x100000 开头的内核正文 .text 段（踩 .text 会飞随机错误，查半年都找不到）；
3. **大小 16MB 够用**：网络栈 sockets[] 65535 × 结构体（~256B）≈16MB，刚好塞下（你项目记忆里的硬约束，65535 硬编码会栈爆炸 → 改堆上分配就是靠这个堆）。

---

### 🚩 L61-L62：`jlos_task_manager_init(&task_manager_)` — 「调度器壳子」

内部核心：

1. 任务双向链表 `head = tail = NULL`（当前没任务）；
2. 全局 `jlos_active_task_manager = &task_manager_`（PIT IRQ0 来了调度点要用）；
3. 调度策略 = **RR（Round-Robin 轮转，时间片 10ms）**；
4. `current_task = NULL`（此时还没任务，后面 multitask_test 才 add task_A / task_B）。

---

### 🚩 L64-L65：`jlos_irq_manager_init(&irq_mgr, 0x20, &mmu_ctx, &task_manager_)` — 「中断准备（但不开中断！）」

#### 4 个参数表解

| 参数 | 值 | 作用 |
|---|---|---|
| `&irq_mgr` | 栈上对象 | 中断管理器状态指针 |
| `0x20` = 32 | **IRQ 基向量号 = 32**<br>⚠️ x86 CPU 规定：<br>0-31 是 CPU 异常（#DE 除零 / #UD 非法指令 / #DF 双错 / #GP 一般保护 / #PF 缺页等）；<br>所以双 8259A PIC 的 16 个 IRQ（硬件中断）必须 **remap 到 32 开始**：<br>主 PIC IRQ0-IRQ7 → 向量 32-39；<br>从 PIC IRQ8-IRQ15 → 向量 40-47。<br>不 remap 后果：PIT IRQ0 向量 0 和 #DE 除零撞号 = 切任务时偶尔收到"除零错误"实际是定时器打进来，查死你。 |
| `&mmu_ctx` | L51 建好的 | #PF 缺页异常（vector 14）要查页目录，给缺页处理用 |
| `&task_manager_` | L62 建好的 | ⚠️ 关键关联：PIT IRQ0（vector 32）ISR 跑完会调用 `jlos_task_manager_schedule(cpustate)` 切任务，所以 irq_mgr 必须拿 task_manager_ 指针才能切。 |

#### 内部关键动作（都是踩坑出来的硬约束）

1. **填 256 个 IDT 门**（0-255）：
   - 异常（0-31）：每个挂自己的 stub。⚠️ **必须区分带/不带错误码的 stub**！（项目记忆硬约束）：
     - **带错误码**（5 个）：#DF(8) / #TS(10) / #NP(11) / #SS(12) / #GP(13) / #PF(14) / #AC(17) → CPU 会自动 push 错误码在栈上，**stub 不能再手动 push 错误码占位**，否则栈错位；
     - **不带错误码**（其他 24 个）：stub 必须手动 push 一个 0 占位 + 中断号；
     - 错一个：iret 时 ESP 偏 4 字节 → EIP 变成栈里垃圾值 `0x00000003` → #UD 连续异常 → 三重重启。
   - 硬件中断（32-47）：挂通用 stub（push 中断号 → jmp common_interrupt）；
   - 其余（48-255）：默认挂「unhandled_interrupt」兜底。⚠️ 其中 **IRQ13(向量 0x0D=浮点协处理器忙) 和 向量 0x2E** 必须识别并静默忽略（VMware 发的伪中断，不忽略就会报 "UNHANDLED INTERRUPT" 刷屏还卡）。
2. **双 8259A PIC 初始化 + remap**（ICW1-ICW4 标准 4 步写）；
3. ⚠️ **PIC 动态 mask = 0xFA（主片 = `11111010`）**（项目记忆硬约束！）：
   - 只开 IRQ0（PIT 定时器）+ IRQ2（从 PIC 级联）；
   - 其他 IRQ1(键盘)/IRQ12(鼠标)/IRQ11(网卡) **全 mask 掉**；
   - 为什么？之前「先开 IRQ1 再注册键盘 handler」→ 键盘敲一下 → 未注册 handler → unhandled_interrupt → PIC 没送 EOI → 键盘永久死机。
   - 正确做法（我们的代码）：**驱动注册 handler 的那一瞬间，HAL 自动 unmask 对应 IRQ 号**（动态打开，符合项目记忆里的 PIC 动态 masking 规则）。
4. **`lidt` 加载 IDTR**：告诉 CPU「256 个门在这个地址」；
5. **⚠️ 不执行 sti！**（这是最后一步才做的事，L96）

---

### 🚩 L67-L68：`jlos_syscall_init(&syscalls, &irq_mgr, 0x80)` — 「用户态系统调用门」

| 参数 | 值 | 作用 |
|---|---|---|
| `&irq_mgr` | L65 建的 | 往哪个中断管理器的 IDT 挂 |
| `0x80` = 128 | Linux 经典约定号 | **用户态 `int $0x80` → 内核入口**（你写的 sysprintf 就是 eax=4） |

内部关键动作（踩坑 ⚠️）：

1. IDT 向量 128 的 DPL 设成 3（用户态 CPL=3 才能触发 `int 0x80`，否则直接 #GP 把用户程序杀了）；
2. 注册 syscall 表：`eax=0`→sys_exit / `eax=4`→sys_write（printf）；
3. ⚠️ **`jlos_syscall_handler_init` 必须在 `jlos_interrupt_handler_init` 之后**手动把 `handle_interrupt` 回调 set 回去。HAL 的初始化顺序小坑：如果 syscall init 不手动设置回调 → HAL 不认识 vector 128 → 用户态 `int 0x80` 直接走 unhandled_interrupt → 黑屏。

---

### 🚩 L70-L72：Stage 1 开始 + driver_manager_init

```c
printf("initializing hardware, stage 1 start\n");  // L70
jlos_driver_manager_t driver_manager_;              // L71
jlos_driver_manager_init(&driver_manager_);         // L72
```

**三阶段划分**（你日志里最显眼的 3 行打印）：

| 阶段 | 打印 | 做什么 | 关键词 |
|---|---|---|---|
| **Stage 1** | `stage 1 start` | **枚举硬件（detect + bind）**<br>扫总线找设备、malloc 驱动对象、绑好 IRQ，但**不通电/不激活** | 只读、不发命令 |
| **Stage 2** | `stage 2 start` | **激活驱动（activate_all）**<br>给每个绑定好的驱动发 init/start 命令，让硬件准备好收发 | 发序列命令 |
| **Stage 3** | `stage 3 start` | **启动节拍 + 开中断**<br>开 PIT 定时器、sti，整个系统开始跑事件驱动 | 开中断、心跳 |

L72 的 driver_manager_init 就是建个空链表，后面 PCI 枚举到设备就往里面 add。

---

### 🚩 L74-L76：Debug Console（编译开关）

`KERNEL_CONFIG_DEBUG_CONSOLE=1` 时编译，注册键盘 Ctrl+Alt+D 开调试壳子（读符号表 / 查堆分配 / dump IDT 等）。默认关，正式跑时去掉。

---

### 🚩 L78-L79：`jlos_hal_pci_init(&pci_controller)` — 「PCI 控制器初始化」

PCI Mechanism #1（x86 PC 标准）初始化：

- 验证配置空间可用：对 bus0/dev0/func0 读 Vendor ID，如果不是 `0xFFFF` 就说明 Mechanism #1 正常；
- 把「地址端口 0xCF8 / 数据端口 0xCFC」的 HAL ops 表填好；
- 为什么走 HAL？以后上 RISC-V（ECAM）/ ARM（GICv3 ITS）直接换 PCI ops 表，kernel.c 不动。

---

### 🚩 L81-L86：Stage 1 核心 — 「切低地址堆 → PCI 枚举绑驱动 → 切回主堆」

⚠️ **这一段是整个启动流程中最精妙的设计（踩 N 次坑才出来的）。**

```c
jlos_memory_manager_t *old_manager = jlos_active_memory_manager;   // L81 保存当前
jlos_active_memory_manager = &low_memory_manager_;                  // L82 切！
printf("switched to low memory manager for PCI driver allocation\n");

jlos_hal_pci_enumerate_and_bind_drivers(&pci, &dm, &irq_mgr);       // L84 扫 PCI 绑驱动

jlos_active_memory_manager = old_manager;                           // L85 切回来
printf("switched back to main memory manager\n");                  // L86
```

#### L81-L82：堆切换原理

`jlos_active_memory_manager` 是一个**全局指针**（定义在 [kernel/memory_manager.h](./kernel/memory_manager.h)），所有 `jlos_malloc()` / `jlos_free()` 内部都**直接用这个指针分配**。把它从主堆改成指向低地址堆，就完成了「整个 malloc 体系无感知地切换分配区」。

类似 Linux 的 `set_current_mm()` 切换当前进程页表的思路：改全局指针，下面所有子函数不用传堆参数。

#### L84：`jlos_hal_pci_enumerate_and_bind_drivers` 核心动作（三层嵌套循环）

```
for bus 0..255:
   for device 0..31:
      for func 0..7:
          VendorID = PCI_read_config_16(bus,dev,func,0x00)
          if VendorID == 0xFFFF: 空 function，跳过
          DeviceID = PCI_read_config_16(bus,dev,func,0x02)
          switch (VendorID:DeviceID)
              case 0x1022:0x2000:  // 命中 AMD am79c973
                  am79c973_probe(bus,dev,func):
                      1) jlos_malloc(sizeof(drv_struct))      ← ⭐ 因为切了低地址堆，malloc 返回地址天然 <4MB
                          (地址一般是 0x00050010，你日志里能看到)
                      2) INIT block 放在驱动结构体内尾部 → 天然 32B 对齐（结构体对齐 + heap 本身对齐 = 双重保险）
                      3) 读 PCI BAR0/1 拿到 IO 端口基址 + IRQ 号 = 0x0B
                      4) driver_manager_add_driver → 绑到 manager
                      5) HAL claim IRQ 0x0B + IO 端口 → 资源标记为已占用
              case 0x8086:....   Intel SATA/USB/...
```

你日志里对得上的打印：

```
Allocating AMD am79c973 driver structure...
AMD am79c973 driver allocated at: 0x00050010      ← 0x50010 = 低地址堆基址 0x50000 + 0x10，完美命中
AMD am79c973 IRQ=0B interrupt=2B
```

为什么 L85 必须立刻切回主堆？

- 低地址堆只有 320KB！后面 network_init() 要 malloc 一个 `network_stack_t`（里面 65535 个 socket 的超大数组，直接超 320KB）；
- 不切回去 → 低地址堆炸 → 描述符被 network_stack_t 踩 → 网卡 MISS 错误（项目记忆里的「MISSED ERROR」大坑）。

---

### 🚩 L88-L89：Stage 2 — `jlos_driver_manager_activate_all(&driver_manager_)`

把 Stage 1 绑好的所有驱动，**按注册顺序**（PCI 扫到的顺序）发 `activate` 命令：

```c
for each drv in driver_manager.drivers:
    if (drv->ops->activate)  drv->ops->activate(drv);
```

#### 最关键的：am79c973_activate 严格序列（错一个就 CSR0=0xFFFF 死锁！）

项目记忆硬约束 ⚠️：

```
STOP → CSR4 → CSR1/CSR2(INIT block addr, 32B 对齐) → INIT → STRT(0x42)
```

拆解 6 步：

| 步骤 | 写 CSR | 值 | 作用 | 坑 |
|---|---|---|---|---|
| 1 | CSR0 | `0x0004` (STOP) | 让芯片停止当前 DMA，准备初始化 | 必须先 STOP，不然 INIT 命令直接忽略 |
| 2 | CSR4 | `0x0002` (DMAPLUS=1) | 设 DMA 突发长度（我们用双字 DWORDS 模式） | |
| 3 | CSR1 | INIT block 的低 16 位 | INIT 块物理地址（<4MB！） | ⚠️ 这就是为什么必须用低地址堆分配驱动结构 |
| 4 | CSR2 | INIT block 的高 16 位 | | |
| 5 | CSR0 | `0x0001` (INIT) | 让芯片读取 INIT 块、初始化 MAC / RX ring / TX ring | ❌ **从这步开始禁止手动写 CSR3/CSR5！** 硬件会自动从 INIT 块 offset 0x14=RAP, 0x18=SAP 读出来覆盖你的值。之前手动写过 → 16-bit/32-bit 模式不匹配 → CSR0 永远 0xFFFF。 |
| 6 | CSR0 | `0x0042` (STRT \| INEA) | ⚠️ 必须**同时**设置：<br>STRT=1 开始收发；<br>INEA=1 让 IRQ 引脚能输出（不设 INEA 芯片收到包只会设 RINT 位不发 IRQ） | 只写 STRT=0x02 不写 INEA → 网卡收包但 CPU 不知道，你会看见 receive descriptor 有数据但 ISR 永远不进（项目踩过的坑）。 |

激活完日志里会看到：

```
POST-START CSR0=0x01F3
  STRT=01 INEA=01 INTR=01 RXON=01 TXON=01
AMD am79c973 activation complete
```

`0x01F3` = 所有必需状态位都 OK，芯片 ready 收发 ✨。

---

### 🚩 L91-L94：Stage 3 — 启 PIT 定时器 100Hz

```c
printf("initializing hardware, stage 3 start\n");
jlos_hal_timer_start_periodic(100);                    // L93
printf("PIT timer initialized (100Hz)\n");
```

内部（[hal/timer.c](./hal/timer.c)）写 8253 通道 0，Mode 2（Rate Generator，周期性中断）：

| 步骤 | 操作 | 作用 |
|---|---|---|
| 1 | outb 0x43 = 0x34 | 控制字：通道 0 + 低字节/高字节 + Mode 2 + 二进制 |
| 2 | outb 0x40 = 0x9C | 分频 1193182 / 100 = **11932**<br>低字节 11932 & 0xFF = 0x9C |
| 3 | outb 0x40 = 0x2E | 高字节 (11932 >> 8) & 0xFF = 0x2E |

完成后 **每 10ms 触发一次 IRQ0** = 调度器的心跳节拍。如果忘了设分频就会默认 18.2Hz（~55ms 一次），调度反应迟钝，网络超时逻辑也会不准。

---

### 🚩 L96：`jlos_irq_manager_activate(&irq_mgr)` — 🔥 最后一步！系统活了

内部：

1. **根据已注册的 handler 调整 PIC mask**（这就是动态 mask）：
   - 键盘驱动注册了 IRQ1 → HAL 写 IMR unmask IRQ1；
   - 鼠标驱动注册了 IRQ12 → 先写 0x64 端口开 PS/2 第二通道 → unmask IRQ12；
   - 网卡驱动 IRQ 0x0B → unmask IRQ11；
   - IRQ0（PIT）+ IRQ2（从片级联）已经开的保持。
2. 送一次初始 EOI 清除 PIC 可能的伪中断残留；
3. **最后一行执行 `asm volatile("sti")`** → IF 置 1 → **中断全面开放！**

⚠️ 项目记忆硬约束：**`sti` 必须在 `network_init()` 之前！** 为什么？
> ARP 协议栈 `network_init` 最后一步是「广播求网关 MAC → 等 ARP reply」。如果没开中断，`hlt` 会睡死没人能收 ARP reply → 死循环超时。开了 sti，网卡 IRQ 打断 hlt → ISR 收 ARP → cache 更新 → 继续。

sti 之后你会看到这行打印，系统从此进入事件驱动模式：

```
interrupts activated
```

---

## 📖 关键硬约束索引（项目记忆）

| 约束 | 说明 | 启动流程对应位置 |
|---|---|---|
| INIT block 32B 对齐 < 4MB | AMD am79c973 硬件要求 | L55 低地址堆 + L84 probe 时分配 |
| CSR3/CSR5 禁止手动写 | INIT 后硬件从 INIT block offset 0x14/0x18 自动读 | L89 activate 序列第 5 步 |
| 序列 STOP→CSR4→CSR1/2→INIT→STRT(0x42) | 顺序错 = CSR0=0xFFFF | L89 activate_all 内部 |
| PIT 100Hz Mode 2 | 给调度器 10ms 心跳 | L93 |
| PIC 默认只开 IRQ0+IRQ2，驱动注册时才 unmask | 先开中断后注册会导致 unhandled | L65 irq_manager_init 内部 |
| 中断 stub 必须 4 字节 pushl 中断号 | 不然栈错位 EIP 垃圾 | 配套 arch/x86/interruptstubs.s |
| sti 必须在 network_init 前 | ARP resolve 能收到中断包 | L96 sti，L104 才 network_init |
| ARP 循环必须 timeout 5,000,000 次 | VMware 网关有时不响应，防 hlt 死锁 | L104 network_init 内部 |

---

## 🏁 之后（L99-L117）简述

- L99-L104：网络栈 6 层 init（EtherFrame → ARP → IPv4 → ICMP → UDP → TCP），最后一步 ARP 广播求网关 MAC（项目记忆 ⚠️：timeout 5,000,000 次，防死循环 hlt 死锁）；
- L106-L113：5 个测试（memory / multitask × task_A,B 各打 10 行 + ATA 简化模式跳过 + TCP 1234 HTTP 服务 + UDP 5678 echo）；
- L115-L117：`for(;;) hlt` 进入 idle 循环，CPU 占用从 100% → ≈0%，等中断。

---

**到这里，JohnBeautY-OS 从 GRUB 跳进来，到系统正常跑服务，完整走完了 ✅。** 公主平安开心 🌸。
