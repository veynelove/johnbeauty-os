# JohnBeauty OS (JLOS)

> 公主平安开心 🌸

## 🔧 编译与运行

### 环境依赖

```text
宿主机：windows11
虚拟机: vmplayer + ubuntu24.04
依赖：sudo apt install xorriso clangd bear

note: clangd代码分析 + gcc编译
(vscode下载clangd插件，禁用microsoft的c/c++插件，运行 bear --output ./build/compile_commands.json -- make 生成编译文件供clangd分析)
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
[MEMORY] ALL PASSED             # TEST 1~5 全 PASS (boundary/slab/contig/kvheap/roundtrip)
[MULTITASK] ALL PASSED           # TEST 1~4 全 PASS (调度交替/fork-wait/10子并发/ring3 冒烟)
[PF] addr=0x... err=0x.. user=1 ip=0x.. pid=N   # 用户栈 demand paging 一行总览
```
