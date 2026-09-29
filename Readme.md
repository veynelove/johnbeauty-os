# JohnSunshine OS (JLOS)

> 公主平安开心 🌸

### 环境依赖

```text

宿主机：windows11
虚拟机: vmplayer + ubuntu24.04
依赖：sudo apt install xorriso clangd bear

note: clangd代码分析 + gcc编译
vscode下载clangd插件，禁用microsoft的c/c++插件，运行 `make compdb` 生成编译文件供clangd分析

```

### 子项目JLCY

1. 该项目是独立项目，不参与内核编译构建，有自己的编译系统。
2. 该项目用于ring3用户程序接口和相关测试；
3. 用户程序通过该项目生成elf文件存放在自制fat32磁盘文件中，通过磁盘挂载到内核，执行用户程序。
4. 之所以放在内核根目录下，是为了方便修改。但原则要求JLCY和内核项目除了abi适配，必须完全无关。
5. jlcy里也加入了clangd代码分析。

### 构建命令

```bash
cd johnbeauty-os
make            # 生成 johnkernel.iso（GRUB2 multiboot）
make clean      # 清理 obj / johnkernel.bin / johnkernel.iso
```

1. 根目录auto_build_run.test文件存放有.bat文件内容，可用于自动编译加载iso，不过在运行之前需要修改文件内容:

```text

ssh -t veyne@192.168.159.128 "cd ~/johnbeauty-os/ && make clean && make"
修改虚拟机ssh地址，以及虚拟机中项目地址

scp veyne@192.168.159.128:~/johnbeauty-os/johnkernel.iso C:\Users\johnbeauty\Desktop\johnkernel.iso
修改iso复制到windows的路径，这里的路径要与虚拟机配置的iso路径一致，这样就可以覆盖旧的iso

scp veyne@192.168.159.128:~/johnbeauty-os/disk.vmdk C:\Users\johnbeauty\Desktop\disk.vmdk
scp veyne@192.168.159.128:~/johnbeauty-os/disk-flat.vmdk C:\Users\johnbeauty\Desktop\disk-flat.vmdk
这里我们需要先编辑vmplayer虚拟机设置，删除原有硬盘，点击添加设备，选择硬盘，选择IDE类型，选择使用现有虚拟磁盘，绝对路径例如：C:\Users\johnbeauty\Desktop\disk.vmdk。
注意不要选到disk-flat.vmdk了。因为vmplayer不支持加载img格式磁盘，所以在执行make后，编译系统把生成的磁盘img转化成了vmplayer能识别的vmdk虚拟磁盘格式。这样就能加载装有用户程序的磁盘了。
这一步只是为了测试ring3用户程序。
可通过编译参数 BUILD_DISK 控制条件编译。

start "" "F:\vmware\vmplayer.exe" "E:\johnbeauty\johnbeauty.vmx"
这里是命令行启动虚拟机，因虚拟机不同而不同，我用的是vmplayer 17

```

2. 另外，kernel默认开启了com1串口打印，所有的日志都会输出到串口中，可以在虚拟机"编辑虚拟机设置"中，找到"串行端口"选项，在连接中选择“使用输出文件"选中一个在windows本地任意位置创建的文件，比如\
    "C:\Users\johnbeauty\Desktop\log.txt"文件。这样，运行虚拟机后，日志就会输出两份，一份在虚拟机终端显示，一份存在log.txt文件中。方便复制查看日志。

3. 开启 HAL I/O 诊断追踪（查"写 CF8 后网卡中断丢失"类竞态）：

```bash
CFLAGS_EXTRA="-DHAL_CONFIG_TRACE_IO=1" make clean all
# 运行到怀疑点：调用 jlos_hal_trace_dump(128) 打印最近 128 条 in/out 记录
```

### ✅ 启动成功关键字段（串口/framebuffer 输出）

```text

[1970-01-01 00:00:00] [I] [boot] [john_beauty_main] princess yihan is safe and happy!
[1970-01-01 00:00:00] [I] [dev] [jlos_device_init] mmap: 11 entries, max ram end = 0x10000000, total available = 261630 KB
[1970-01-01 00:00:00] [I] [boot] [john_beauty_main] paging initialized
[1970-01-01 00:00:00] [I] [mm] [jlos_memory_manager_init_main] init_main: first=0xf8000000 size=4194272 heap=[0xf8000000,0xf8400000) current=0xf8400000
[1970-01-01 00:00:00] [I] [tsc] [tsc_register] freq = 9245 MHz
[1970-01-01 00:00:00] [D] [pci] [pci_network_controller_handle] amd am79c973 pci command: 0x7
[1970-01-01 00:00:00] [D] [pci] [pci_network_controller_handle] allocating amd am79c973 driver structure
[1970-01-01 00:00:00] [D] [pci] [pci_network_controller_handle] amd am79c9973 driver allocated at: 0xc07a0008
[1970-01-01 00:00:00] [I] [eth] [jlos_amd_am79c973_init] IRQ=b interrupt=2b
[1970-01-01 00:00:00] [D] [eth] [jlos_amd_am79c973_init] MAC port registers: C 0 7A 29 A0 1F
[1970-01-01 00:00:00] [D] [eth] [jlos_amd_am79c973_init] init RLEN=0x50 (32 recv) SLEN=0x30 (8 send)
[1970-01-01 00:00:00] [D] [ata] [jlos_ata_block_dev_init] ata primary device ready, sectors = 32768
[1970-01-01 00:00:00] [I] [net] [network_init] initializing network stack (unconfigured)
[1970-01-01 00:00:00] [I] [arp] [jlos_arp_init] initializing ARP protocol
[1970-01-01 00:00:00] [I] [udp] [jlos_udp_provider_init] initialized
[1970-01-01 00:00:00] [D] [udp] [jlos_udp_socket_init] socket initialized
[1970-01-01 00:00:00] [I] [dhcp] [jlos_dhcp_client_init] client initialized, xid = 0
[1970-01-01 00:00:00] [I] [net] [network_init] network stack initialization complete
[1970-01-01 00:00:00] [I] [clocke] [clock_event_select_and_start] selected = 8253 PIT, rating = 100
[1970-01-01 00:00:00] [I] [clocks] [clocksource_select] selected = TSC, rating = 300
[1970-01-01 00:00:00] [D] [ptfs] [jlos_partition_parse_mbr] partition 0, type = c, start_lba = 2048, sector = 30720
[1970-01-01 00:00:00] [D] [fat32] [fat32_mount] cluster = 30214, root = 2
[1970-01-01 00:00:00] [D] [rootfs] [jlos_rootfs_init] rootfs mounted on partition 0, type 0xc
[2026-09-29 17:38:57] [I] [rtc] [rtc_init] rtc: 1790703537 seconds since epoch
[2026-09-29 17:38:57] [D] [eth] [jlos_amd_am79c973_activate] BCR written with 0x102
[2026-09-29 17:38:57] [D] [eth] [jlos_amd_am79c973_activate] CSR0 written with 0x04 (STOP)
[2026-09-29 17:38:57] [I] [eth] [jlos_amd_am79c973_activate] STOP acknowledged
[2026-09-29 17:38:57] [D] [eth] [jlos_amd_am79c973_activate] post-start CSR0=0x1f3 STRT=1 INEA=1 INTR=1 RXON=1 TXON=1 RINT=0 TINT=0 IDON=1
[2026-09-29 17:38:57] [I] [eth] [jlos_amd_am79c973_activate] activation complete
[2026-09-29 17:38:57] [I] [eth] [jlos_amd_am79c973_handle_interrupt] init done
[2026-09-29 17:38:57] [D] [udp] [jlos_udp_socket_send] socket sending data
[2026-09-29 17:38:57] [D] [eth] [jlos_amd_am79c973_send] send: 342 bytes
[2026-09-29 17:38:57] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] received IPv4 packet, size=300
[2026-09-29 17:38:57] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] protocol=1
[2026-09-29 17:38:57] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] packet not for us
[2026-09-29 17:38:57] [D] [eth] [jlos_amd_am79c973_send] post-send CSR0=0x4f3 RINT=1 TINT=0 INTR=1 INEA=1 RXON=1 TXON=1 sent=1 sflags=0x3080eaa
[2026-09-29 17:38:57] [I] [dhcp] [jlos_dhcp_client_discover] DISC sent, xid = f5cc10
[2026-09-29 17:38:57] [I] [t_mm] [memory_manager_test] === memory test start ===
[2026-09-29 17:38:57] [I] [t_mm] [memory_manager_test] MIN_ALLOC=16B CLASS_COUNT=32 PAGE=4096B
[2026-09-29 17:38:57] [I] [t_mm] [memory_manager_test] PFA total=65280 frames (~261120 KB) max slab small=4068B
[2026-09-29 17:38:57] [I] [t_mm] [test_boundary] [test 1] boundary
[2026-09-29 17:38:57] [I] [t_mm] [test_boundary] kalloc(0) == NULL
[2026-09-29 17:38:57] [I] [t_mm] [test_boundary] boundary (12 cases)
[2026-09-29 17:38:57] [I] [t_mm] [test_small_slab] [test 2] small slab
[2026-09-29 17:38:57] [I] [t_mm] [test_small_slab] small slab (9 sizes)
[2026-09-29 17:38:57] [I] [t_mm] [test_large_contig] [test 3] large contig
[2026-09-29 17:38:57] [I] [t_mm] [test_large_contig] 1 pages
[2026-09-29 17:38:57] [I] [t_mm] [test_large_contig] 4 pages
[2026-09-29 17:38:57] [I] [t_mm] [test_large_contig] 16 pages
[2026-09-29 17:38:57] [I] [t_mm] [test_large_contig] 64 pages
[2026-09-29 17:38:57] [I] [t_mm] [test_kvheap_fallback] [test 4] kvalloc small -> heap fallback
[2026-09-29 17:38:57] [I] [t_mm] [test_kvheap_fallback] kvheap fallback
[2026-09-29 17:38:57] [I] [t_mm] [test_roundtrip] [test 5] slab roundtrip: kalloc(128) x 1000
[2026-09-29 17:38:57] [I] [t_mm] [test_roundtrip] 1000 iters
[2026-09-29 17:38:57] [I] [t_mm] [test_roundtrip] [test 5b] contig roundtrip: 16 pages x 100
[2026-09-29 17:38:57] [I] [t_mm] [test_roundtrip] 100 iters
[2026-09-29 17:38:57] [I] [t_mm] [memory_manager_test] memory: all passed
[2026-09-29 17:38:57] [I] [t_mm] [memory_manager_test] post-test heap stats:
[2026-09-29 17:38:57] [I] [mm] [jlos_kvalloc_stats] memory manager stats:
[2026-09-29 17:38:57] [I] [mm] [jlos_kvalloc_stats] total chunks: 15
[2026-09-29 17:38:57] [I] [mm] [jlos_kvalloc_stats] allocated: 14 chunks, 4036 bytes
[2026-09-29 17:38:57] [I] [mm] [jlos_kvalloc_stats] free: 1 chunks, 4189788 bytes
[2026-09-29 17:38:57] [I] [mm] [jlos_kvalloc_stats] free bitmap: 0x40000
[2026-09-29 17:38:57] [I] [mm] [jlos_kvalloc_stats] class 18 (4194304B): 1 free
[2026-09-29 17:38:57] [I] [t_timek] [timek_test] === timek test start ===
[2026-09-29 17:38:57] [I] [t_timek] [test_monotonic_increasing] [1] monotonic increasing
[2026-09-29 17:38:57] [I] [t_timek] [test_monotonic_increasing] delta=1241 ns
[2026-09-29 17:38:57] [I] [t_timek] [test_ticks_non_decreasing] [2] ticks non-decreasing
[2026-09-29 17:38:57] [I] [t_timek] [test_ticks_non_decreasing] t1=10 t2=10
[2026-09-29 17:38:57] [I] [t_timek] [test_realtime_ge_monotonic] [3] realtime >= monotonic
[2026-09-29 17:38:57] [I] [t_timek] [test_realtime_ge_monotonic] offset=1790703536992245836 ns
[2026-09-29 17:38:57] [I] [t_timek] [timek_test] timek: all passed
[2026-09-29 17:38:57] [I] [t_hrtm] [hrtimer_test] === hrtimer test start ===
[2026-09-29 17:38:57] [I] [t_hrtm] [test_lifecycle] [1] init/start/cancel lifecycle
[2026-09-29 17:38:57] [I] [t_hrtm] [test_lifecycle] lifecycle ok
[2026-09-29 17:38:57] [I] [t_hrtm] [test_periodic] [2] periodic interval
[2026-09-29 17:38:57] [I] [t_hrtm] [test_periodic] periodic interval ok
[2026-09-29 17:38:57] [I] [t_hrtm] [test_ordering] [3] ordering by expires
[2026-09-29 17:38:57] [I] [t_hrtm] [test_ordering] earliest is t2 (100ms)
[2026-09-29 17:38:57] [I] [t_hrtm] [test_ordering] ordering ok
[2026-09-29 17:38:57] [I] [t_hrtm] [hrtimer_test] hrtimer: all passed
[2026-09-29 17:38:57] [I] [t_sched] [multitask_test] === multitask test start ===
[2026-09-29 17:38:57] [I] [t_sched] [multitask_test] MAX_TASKS=256 KSTACK=16384B USTACK=64KB MLFQ=4 lv
[2026-09-29 17:38:57] [I] [t_sched] [multitask_test] [test 1] schedule alternation (2 kernel tasks)
[2026-09-29 17:38:57] [I] [t_sched] [multitask_test] [test 2] fork+wait + 10-child pressure (user ELF)
[2026-09-29 17:38:57] [I] [t_sched] [multitask_test] [test 4] ring3 user task smoke
fork_test: pid=4
fork_test: fork+wait OK
fork_test: 10-child pressure OK
fork_test: ALL PASSED
[2026-09-29 17:38:57] [I] [t_sched] [multitask_test] [test 5] ring3 file syscall test
hello from user ELF. PID = 16, argc = 1, argv[0] = /hello.elf, curr time: 1790703537
[2026-09-29 17:38:57] [I] [syscall] [syscall_get_tasks_info] --- task list ---
[2026-09-29 17:38:57] [I] [syscall] [syscall_get_tasks_info] [0] name = idle, pid = 0, status = running, task_type = kernel
[2026-09-29 17:38:57] [I] [syscall] [syscall_get_tasks_info] [1] name = t1_a, pid = 2, status = zombie, task_type = kernel
[2026-09-29 17:38:57] [I] [syscall] [syscall_get_tasks_info] [2] name = t1_b, pid = 3, status = zombie, task_type = kernel
[2026-09-29 17:38:57] [I] [syscall] [syscall_get_tasks_info] [3] name = t2_fork, pid = 4, status = zombie, task_type = user
[2026-09-29 17:38:57] [I] [syscall] [syscall_get_tasks_info] [4] name = t4_ring3, pid = 16, status = running, task_type = user
[2026-09-29 17:38:57] [I] [syscall] [syscall_get_tasks_info] ---
[2026-09-29 17:38:57] [I] [t_sched] [multitask_test] [test 6] ring3 signal test
file_test: pid=17 argc=1
OK: elf magic 127 69 76 70
OK: write/read match
[2026-09-29 17:38:57] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] received IPv4 packet, size=531
[2026-09-29 17:38:57] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] protocol=11
[2026-09-29 17:38:57] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] packet is for us
[2026-09-29 17:38:57] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] handler found, calling it
[2026-09-29 17:38:57] [D] [udp] [jlos_udp_provider_on_internet_protocol_received] received packet
[2026-09-29 17:38:57] [D] [udp] [jlos_udp_provider_on_internet_protocol_received] destination port=044
[2026-09-29 17:38:57] [D] [udp] [jlos_udp_provider_on_internet_protocol_received] socket matched
[2026-09-29 17:38:57] [D] [udp] [jlos_udp_socket_handle_udp_message] socket received data
[2026-09-29 17:38:57] [I] [dhcp] [jlos_dhcp_handle_offer] OFFER received, yiaddr = 859fa8c0, server_id = fe9fa8c0
[2026-09-29 17:38:57] [D] [udp] [jlos_udp_socket_send] socket sending data
[2026-09-29 17:38:57] [D] [eth] [jlos_amd_am79c973_send] send: 342 bytes
[2026-09-29 17:38:57] [D] [eth] [jlos_amd_am79c973_send] post-send CSR0=0x6f3 RINT=1 TINT=1 INTR=1 INEA=1 RXON=1 TXON=1 sent=1 sflags=0x3080eaa
[2026-09-29 17:38:57] [I] [dhcp] [jlos_dhcp_client_request] REQ sent, offered_ip = 859fa8c0, server_id = fe9fa8c0
[2026-09-29 17:38:57] [D] [eth] [jlos_amd_am79c973_send] send: 353 bytes
[2026-09-29 17:38:58] [D] [eth] [jlos_amd_am79c973_send] post-send CSR0=0x6f3 RINT=1 TINT=1 INTR=1 INEA=1 RXON=1 TXON=1 sent=1 sflags=0x3080e9f
[2026-09-29 17:38:58] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] received IPv4 packet, size=531
[2026-09-29 17:38:58] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] protocol=11
[2026-09-29 17:38:58] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] packet is for us
[2026-09-29 17:38:58] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] handler found, calling it
[2026-09-29 17:38:58] [D] [udp] [jlos_udp_provider_on_internet_protocol_received] received packet
[2026-09-29 17:38:58] [D] [udp] [jlos_udp_provider_on_internet_protocol_received] destination port=044
[2026-09-29 17:38:58] [D] [udp] [jlos_udp_provider_on_internet_protocol_received] socket matched
[2026-09-29 17:38:58] [D] [udp] [jlos_udp_socket_handle_udp_message] socket received data
[2026-09-29 17:38:58] [I] [dhcp] [jlos_dhcp_handle_ack] BOUND: ip = 192.168.159.133, mask = 255.255.255.0, gw = 192.168.159.2, dns = 192.168.159.2
[2026-09-29 17:38:58] [D] [eth] [jlos_amd_am79c973_send] send: 353 bytes
[2026-09-29 17:38:58] [D] [eth] [jlos_amd_am79c973_send] post-send CSR0=0x6f3 RINT=1 TINT=1 INTR=1 INEA=1 RXON=1 TXON=1 sent=1 sflags=0x3080e9f
OK: lseek+read
OK: unlink+reopen-fail
OK: mmap/munmap anon
file_test: ALL PASSED
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] [test 7] ring3 mmap test
signal_test: pid=18
signal_test: caught sig=15
signal_test: ALL PASSED
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] [test 8] ring3 nanosleep test
mmap_test: pid=21
mmap_test: ALL PASSED
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] 8 seed tasks spawned, run schedule budget...
[2026-09-29 17:38:58] [D] [arp] [jlos_arp_on_ether_frame_received] received ARP request
[2026-09-29 17:38:58] [D] [arp] [jlos_arp_on_ether_frame_received] sending ARP reply
[2026-09-29 17:38:58] [D] [eth] [jlos_amd_am79c973_send] send: 64 bytes
[2026-09-29 17:38:58] [D] [eth] [jlos_amd_am79c973_send] post-send CSR0=0x6f3 RINT=1 TINT=1 INTR=1 INEA=1 RXON=1 TXON=1 sent=1 sflags=0x3080fc0
[2026-09-29 17:38:58] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] received IPv4 packet, size=300
[2026-09-29 17:38:58] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] protocol=1
[2026-09-29 17:38:58] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] packet is for us
[2026-09-29 17:38:58] [D] [ipv4] [jlos_internet_protocol_provider_on_ether_frame_received] handler found, calling it
[2026-09-29 17:38:58] [D] [icmp] [jlos_icmp_on_internet_protocol_received] received packet from c0.a8.9f.fe
[2026-09-29 17:38:58] [D] [icmp] [jlos_icmp_on_internet_protocol_received] echo request received, sending reply
[2026-09-29 17:38:58] [D] [eth] [jlos_amd_am79c973_send] send: 62 bytes
[2026-09-29 17:38:58] [D] [eth] [jlos_amd_am79c973_send] post-send CSR0=0x6f3 RINT=1 TINT=1 INTR=1 INEA=1 RXON=1 TXON=1 sent=1 sflags=0x3080fc2
user ELF: wakeup -> exit
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] budget: elapsed=12 ticks (start=88 now=100)
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] subcase results:
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] [1] alternation: A=500 B=500
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] [2] fork+pressure: exited=1
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] [4] ring3 smoke: exited=1
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] [5] file syscall: exited=1
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] [6] signal: exited=1
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] [7] mmap: exited=1
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] [8] nanosleep: exited=1
[2026-09-29 17:38:58] [I] [t_sched] [multitask_test] multitask: all passed
[2026-09-29 17:38:58] [I] [t_pfa] [pfa_test] === pfa test start ===
[2026-09-29 17:38:58] [I] [t_pfa] [pfa_test] total=65280 frames free=64428
[2026-09-29 17:38:58] [I] [t_pfa] [test_single_frame_roundtrip] [test 1] single frame roundtrip x 64
[2026-09-29 17:38:58] [I] [t_pfa] [test_single_frame_roundtrip] 64 frames alloc/write/verify/free
[2026-09-29 17:38:58] [I] [t_pfa] [test_order_alloc] [test 2] order alloc/free
[2026-09-29 17:38:58] [I] [t_pfa] [test_order_alloc] order 0 (1 frames)
[2026-09-29 17:38:58] [I] [t_pfa] [test_order_alloc] order 1 (2 frames)
[2026-09-29 17:38:58] [I] [t_pfa] [test_order_alloc] order 2 (4 frames)
[2026-09-29 17:38:58] [I] [t_pfa] [test_order_alloc] order 3 (8 frames)
[2026-09-29 17:38:58] [I] [t_pfa] [test_alloc_n] [test 3] alloc/free n
[2026-09-29 17:38:58] [I] [t_pfa] [test_alloc_n] n 2 frames
[2026-09-29 17:38:58] [I] [t_pfa] [test_alloc_n] n 8 frames
[2026-09-29 17:38:58] [I] [t_pfa] [test_alloc_n] n 32 frames
[2026-09-29 17:38:58] [I] [t_pfa] [test_refcount] [test 4] refcount semantics
[2026-09-29 17:38:58] [I] [t_pfa] [test_refcount] inc/dec/get consistent
[2026-09-29 17:38:58] [I] [t_pfa] [test_owner_type] [test 5] owner type mark
[2026-09-29 17:38:58] [I] [t_pfa] [test_owner_type] set/get/clear owner
[2026-09-29 17:38:58] [I] [t_pfa] [test_pressure_and_accounting] [test 6] pressure x 512 + free accounting
[2026-09-29 17:38:58] [I] [t_pfa] [test_pressure_and_accounting] free: base=64427 mid=63931 end=64427
[2026-09-29 17:38:58] [I] [t_pfa] [test_pressure_and_accounting] free accounting restored
[2026-09-29 17:38:58] [I] [t_pfa] [pfa_test] pfa: all passed
[2026-09-29 17:38:58] [I] [t_paging] [paging_test] === paging test start ===
[2026-09-29 17:38:58] [I] [t_paging] [test_map_unmap] [test 1] map/unmap roundtrip
[2026-09-29 17:38:58] [I] [t_paging] [test_map_unmap] map -> get -> unmap -> get(0)
[2026-09-29 17:38:58] [I] [t_paging] [test_map_range] [test 2] map_range 3 pages
[2026-09-29 17:38:58] [I] [t_paging] [test_map_range] 3 pages mapped/unmapped
[2026-09-29 17:38:58] [I] [t_paging] [test_change_flags] [test 3] change_flags
[2026-09-29 17:38:58] [I] [t_paging] [test_change_flags] RW -> RO flag transition
[2026-09-29 17:38:58] [I] [t_paging] [test_clone] [test 4] context clone
[2026-09-29 17:38:58] [I] [t_paging] [test_clone] user mapping not cloned
[2026-09-29 17:38:58] [I] [t_paging] [test_user_accessible] [test 5] access_ok boundary
[2026-09-29 17:38:58] [I] [t_paging] [test_user_accessible] user<->kernel boundary enforced
[2026-09-29 17:38:58] [I] [t_paging] [paging_test] paging: all passed
[2026-09-29 17:38:58] [I] [t_ata] [hard_driver_test] === ata test start ===
[2026-09-29 17:38:58] [I] [t_ata] [hard_driver_test] dev: type=1 sectors=32768
[2026-09-29 17:38:58] [I] [t_ata] [test_identify] [1] identify
[2026-09-29 17:38:58] [I] [t_ata] [test_identify] sectors=32768 bps=512
[2026-09-29 17:38:58] [I] [t_ata] [test_read_mbr] [2] read MBR (sector 0)
[2026-09-29 17:38:58] [I] [t_ata] [test_read_mbr] MBR signature 55AA
[2026-09-29 17:38:58] [I] [t_ata] [test_write_read_roundtrip] [3] write+read roundtrip (last sector)
[2026-09-29 17:38:58] [I] [t_ata] [test_write_read_roundtrip] roundtrip data match
[2026-09-29 17:38:58] [I] [t_ata] [hard_driver_test] ata: all passed
[2026-09-29 17:38:58] [D] [udp] [jlos_udp_handler_init] handler initialized
[2026-09-29 17:38:58] [D] [udp] [jlos_udp_socket_init] socket initialized
[2026-09-29 17:38:58] [I] [t_udp] [udp_server_test] udp server listening on port 5678
[2026-09-29 17:38:58] [I] [t_http] [http_server_test] tcp server listening on port 1234
[2026-09-29 17:38:58] [I] [t_rbtree] [rbtree_test] === rbtree test start ===
[2026-09-29 17:38:58] [I] [t_rbtree] [test_insert_and_find] [test 1] insert and find
[2026-09-29 17:38:58] [I] [t_rbtree] [test_insert_and_find] insert and find (16 entries)
[2026-09-29 17:38:58] [I] [t_rbtree] [test_order_traversal] [test 2] ordered traversal
[2026-09-29 17:38:58] [I] [t_rbtree] [test_order_traversal] ordered traversal (32 entries)
[2026-09-29 17:38:58] [I] [t_rbtree] [test_remove] [test 3] remove
[2026-09-29 17:38:58] [I] [t_rbtree] [test_remove] remove (64 entries, removed evens)
[2026-09-29 17:38:58] [I] [t_rbtree] [test_find_le] [test 4] find_le
[2026-09-29 17:38:58] [I] [t_rbtree] [test_find_le] find_le (4 queries)
[2026-09-29 17:38:58] [I] [t_rbtree] [test_random_insert_remove] [test 5] random insert/remove 128 entries
[2026-09-29 17:38:58] [I] [t_rbtree] [test_random_insert_remove] random insert/remove (128 entries)
[2026-09-29 17:38:58] [I] [t_rbtree] [rbtree_test] rbtree: all passed

```
