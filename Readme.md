# JohnSunshine OS (JLOS)

> 公主平安开心 🌸

## 🔧 编译与运行

### 环境依赖

```text
宿主机：windows11
虚拟机: vmplayer + ubuntu24.04
依赖：sudo apt install xorriso clangd bear

note: clangd代码分析 + gcc编译
(vscode下载clangd插件，禁用microsoft的c/c++插件，运行 `make compdb` 生成编译文件供clangd分析)
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
这里我们需要先编辑vmplayer虚拟机设置，删除原有硬盘，点击添加设备，选择硬盘，选择IDE类型，选择使用现有虚拟磁盘，绝对路径例如：C:\Users\johnbeauty\Desktop\disk-flat.vmdk。
注意不要选到disk-flat.vmdk了。因为vmplayer不支持加载img格式磁盘，所以在执行make后，编译系统把生成的磁盘img转化成了vmplayer能识别的vmdk虚拟磁盘格式。这样就能加载装有用户程序的磁盘了。
这一步只是为了测试ring3用户程序。
可通过编译参数 BUILD_DISK 控制条件编译。

start "" "F:\vmware\vmplayer.exe" "E:\johnbeauty\johnbeauty.vmx"
这里是命令行启动虚拟机，因虚拟机不同而不同，我用的是vmplayer 17
```

1. 另外，kernel默认开启了com1串口打印，所有的日志都会输出到串口中，可以在虚拟机"编辑虚拟机设置"中，找到"串行端口"选项，在连接中选择“使用输出文件"选中一个在windows本地任意位置创建的文件，比如

2. "C:\Users\johnbeauty\Desktop\log.txt"文件。这样，运行虚拟机后，日志就会输出两份，一份在虚拟机终端显示，一份存在log.txt文件中。方便复制查看日志。

3. 开启 HAL I/O 诊断追踪（查"写 CF8 后网卡中断丢失"类竞态）：

```bash
CFLAGS_EXTRA="-DHAL_CONFIG_TRACE_IO=1" make clean all
# 运行到怀疑点：调用 jlos_hal_trace_dump(128) 打印最近 128 条 in/out 记录
```

### ✅ 启动成功关键字段（串口/framebuffer 输出）

```text

[0.000000] [I] [boot] [john_beauty_main] princess yihan is safe and happy!
[0.000000] [I] [dev] [jlos_device_init] mmap: 11 entries, max ram end = 0x10000000, total available = 261630 KB
[0.000000] [I] [boot] [john_beauty_main] paging initialized
[0.000000] [I] [mm] [jlos_memory_manager_init_main] init_main: first=0xf8000000 size=4194272 heap=[0xf8000000,0xf8400000) current=0xf8400000
[0.000000] [D] [pci] [pci_network_controller_handle] amd am79c973 pci command: 0x7
[0.000000] [D] [pci] [pci_network_controller_handle] allocating amd am79c973 driver structure
[0.000000] [D] [pci] [pci_network_controller_handle] amd am79c9973 driver allocated at: 0xc0760008
[0.000000] [I] [eth] [jlos_amd_am79c973_init] IRQ=b interrupt=2b
[0.000000] [D] [eth] [jlos_amd_am79c973_init] MAC port registers: C 0 7A 29 A0 1F
[0.000000] [D] [eth] [jlos_amd_am79c973_init] init RLEN=0x50 (32 recv) SLEN=0x30 (8 send)
[0.000000] [D] [ata] [jlos_ata_block_dev_init] ata primary device ready, sectors = 32768
[0.000000] [I] [net] [network_init] initializing network stack
[0.000000] [I] [net] [network_init] setting IP to 192.168.159.144
[0.000000] [I] [net] [network_init] initializing etherframe provider
[0.000000] [I] [net] [network_init] [OK] etherframe initialized (handlers=0xc0778e1c)
[0.000000] [I] [arp] [jlos_arp_init] initializing ARP protocol
[0.000000] [I] [net] [network_init] [OK] ARP initialized (type=0x0806, cache_size=128)
[0.000000] [I] [net] [network_init] initializing IPv4 protocol (gateway: 192.168.159.1, subnet: 255.255.255.0)
[0.000000] [I] [net] [network_init] [OK] IPv4 initialized (type=0x0800, gw=0x19fa8c0 mask=0xffffff)
[0.000000] [I] [net] [network_init] initializing ICMP protocol
[0.000000] [I] [net] [network_init] [OK] ICMP initialized (proto=0x01)
[0.000000] [I] [udp] [jlos_udp_provider_init] initialized
[0.000000] [I] [net] [network_init] [OK] UDP initialized (proto=0x11, sockets=0xc0778d1c)
[0.000000] [I] [net] [network_init] initializing TCP protocol
[0.000000] [I] [net] [network_init] [OK] TCP initialized (proto=0x06, sockets=0xc0778c1c)
[0.000000] [I] [net] [network_init] sending ARP broadcast to resolve gateway
[0.000000] [D] [arp] [jlos_arp_resolve] sending request for 19fa8c0
[0.000000] [D] [eth] [jlos_amd_am79c973_send] send: 42 bytes
[0.000000] [D] [eth] [jlos_amd_am79c973_send] post-send CSR0=0x44 RINT=0 TINT=0 INTR=0 INEA=1 RXON=0 TXON=0 sent=0 sflags=0x83080fd6
[0.000000] [D] [arp] [jlos_arp_request_mac_address] request sent
[0.000000] [W] [arp] [jlos_arp_resolve] resolve timeout for 19fa8c0
[0.000000] [D] [eth] [jlos_amd_am79c973_send] send: 42 bytes
[0.000000] [D] [eth] [jlos_amd_am79c973_send] post-send CSR0=0x44 RINT=0 TINT=0 INTR=0 INEA=1 RXON=0 TXON=0 sent=0 sflags=0x83080fd6
[0.000000] [D] [arp] [jlos_arp_broadcast_mac_address] broadcast complete
[0.000000] [I] [net] [network_init] network stack initialization complete
[0.000000] [I] [timer] [timer_select_and_start] selected = 8253 PIT, rating = 100
[0.000000] [D] [ptfs] [jlos_partition_parse_mbr] partition 0, type = c, start_lba = 2048, sector = 30720
[0.000000] [D] [fat32] [fat32_mount] cluster = 30214, root = 2
[0.000000] [D] [rootfs] [jlos_rootfs_init] rootfs mounted on partition 0, type 0xc
[0.000000] [D] [eth] [jlos_amd_am79c973_activate] BCR written with 0x102
[0.000000] [D] [eth] [jlos_amd_am79c973_activate] CSR0 written with 0x04 (STOP)
[0.000000] [I] [eth] [jlos_amd_am79c973_activate] STOP acknowledged
[0.000000] [D] [eth] [jlos_amd_am79c973_activate] post-start CSR0=0x1c3 STRT=1 INEA=1 INTR=1 RXON=0 TXON=0 RINT=0 TINT=0 IDON=1
[0.000000] [I] [eth] [jlos_amd_am79c973_activate] activation complete
[0.010000] [I] [eth] [jlos_amd_am79c973_handle_interrupt] init done
[0.010000] [I] [boot] [john_beauty_main] === running tests ===
[0.010000] [I] [test] [memory_manager_test] === memory test start ===
[0.010000] [I] [test] [memory_manager_test] MIN_ALLOC=16B CLASS_COUNT=32 PAGE=4096B
[0.010000] [I] [test] [memory_manager_test] PFA total=65280 frames (~261120 KB) max slab small=4068B
[0.010000] [I] [test] [test_boundary] [test 1] boundary
[0.010000] [I] [test] [test_boundary] OK: kalloc(0) == NULL
[0.010000] [I] [test] [test_boundary] OK: boundary (12 cases)
[0.020000] [I] [test] [test_small_slab] [test 2] small slab
[0.020000] [I] [test] [test_small_slab] OK: small slab (9 sizes)
[0.020000] [I] [test] [test_large_contig] [test 3] large contig
[0.020000] [I] [test] [test_large_contig] OK: 1 pages
[0.030000] [I] [test] [test_large_contig] OK: 4 pages
[0.040000] [I] [test] [test_large_contig] OK: 16 pages
[0.050000] [I] [test] [test_large_contig] OK: 64 pages
[0.060000] [I] [test] [test_kvheap_fallback] [test 4] kvalloc small -> heap fallback
[0.070000] [I] [test] [test_kvheap_fallback] OK: kvheap fallback
[0.080000] [I] [test] [test_roundtrip] [test 5] slab roundtrip: kalloc(128) x 1000
[0.090000] [I] [test] [test_roundtrip] OK: 1000 iters
[0.100000] [I] [test] [test_roundtrip] [test 5b] contig roundtrip: 16 pages x 100
[0.110000] [I] [test] [test_roundtrip] OK: 100 iters
[0.120000] [I] [test] [memory_manager_test] memory: ALL PASSED
[0.130000] [I] [test] [memory_manager_test] post-test heap stats:
[0.140000] [I] [mm] [jlos_kvalloc_stats] memory manager stats:
[0.140000] [I] [mm] [jlos_kvalloc_stats] total chunks: 10
[0.140000] [I] [mm] [jlos_kvalloc_stats] allocated: 9 chunks, 3500 bytes
[0.140000] [I] [mm] [jlos_kvalloc_stats] free: 1 chunks, 4190484 bytes
[0.140000] [I] [mm] [jlos_kvalloc_stats] free bitmap: 0x40000
[0.140000] [I] [mm] [jlos_kvalloc_stats] class 18 (4194304B): 1 free
[0.150000] [I] [test] [pfa_test] === pfa test start ===
[0.160000] [I] [test] [pfa_test] total=65280 frames free=64457
[0.170000] [I] [test] [test_single_frame_roundtrip] [test 1] single frame roundtrip x 64
[0.180000] [I] [test] [test_single_frame_roundtrip] OK: 64 frames alloc/write/verify/free
[0.190000] [I] [test] [test_order_alloc] [test 2] order alloc/free
[0.200000] [I] [test] [test_order_alloc] OK: order 0 (1 frames)
[0.210000] [I] [test] [test_order_alloc] OK: order 1 (2 frames)
[0.220000] [I] [test] [test_order_alloc] OK: order 2 (4 frames)
[0.230000] [I] [test] [test_order_alloc] OK: order 3 (8 frames)
[0.240000] [I] [test] [test_reserve_bulk] [test 3] reserve/free bulk
[0.250000] [I] [test] [test_reserve_bulk] OK: bulk 2 frames
[0.260000] [I] [test] [test_reserve_bulk] OK: bulk 8 frames
[0.270000] [I] [test] [test_reserve_bulk] OK: bulk 32 frames
[0.280000] [I] [test] [test_refcount] [test 4] refcount semantics
[0.290000] [I] [test] [test_refcount] OK: inc/dec/get consistent
[0.300000] [I] [test] [test_owner_type] [test 5] owner type mark
[0.310000] [I] [test] [test_owner_type] OK: set/get/clear owner
[0.320000] [I] [test] [test_pressure_and_accounting] [test 6] pressure x 512 + free accounting
[0.330000] [I] [test] [test_pressure_and_accounting] free: base=64457 mid=63945 end=64457
[0.340000] [I] [test] [test_pressure_and_accounting] OK: free accounting restored
[0.350000] [I] [test] [pfa_test] pfa: ALL PASSED
[0.360000] [I] [test] [paging_test] === paging test start ===
[0.370000] [I] [test] [test_map_unmap] [test 1] map/unmap roundtrip
[0.380000] [I] [test] [test_map_unmap] OK: map -> get -> unmap -> get(0)
[0.390000] [I] [test] [test_map_range] [test 2] map_range 3 pages
[0.400000] [I] [test] [test_map_range] OK: 3 pages mapped/unmapped
[0.410000] [I] [test] [test_change_flags] [test 3] change_flags
[0.420000] [I] [test] [test_change_flags] OK: RW -> RO flag transition
[0.430000] [I] [test] [test_clone] [test 4] context clone
[0.440000] [I] [test] [test_clone] OK: user mapping not cloned
[0.450000] [I] [test] [test_user_accessible] [test 5] access_ok boundary
[0.460000] [I] [test] [test_user_accessible] OK: user<->kernel boundary enforced
[0.470000] [I] [test] [paging_test] paging: ALL PASSED
[0.480000] [I] [test] [multitask_test] === multitask test start ===
[0.490000] [I] [test] [multitask_test] MAX_TASKS=256 KSTACK=16384B USTACK=64KB MLFQ=4 lv
[0.500000] [I] [test] [multitask_test] [test 1] schedule alternation (2 kernel tasks)
[0.510000] [I] [test] [multitask_test] [test 2] fork -> wait -> exit_code (1 child, magic=42)
[0.520000] [I] [test] [multitask_test] [test 3] fork 10 children (exit_code = 100..109)
[0.530000] [I] [test] [multitask_test] [test 4] ring3 user task smoke
[0.540000] [I] [test] [multitask_test] [test 5] ring3 file syscall test
hello from user ELF. PID = 17, argc = 1, argv[0] = /hello.elf
[0.550000] [I] [syscall] [syscall_get_tasks_info] --- task list ---
[0.550000] [I] [syscall] [syscall_get_tasks_info] [0] name = idle, pid = 0, status = running, task_type = kernel
[0.550000] [I] [syscall] [syscall_get_tasks_info] [1] name = t1_a, pid = 2, status = zombie, task_type = kernel
[0.550000] [I] [syscall] [syscall_get_tasks_info] [2] name = t1_b, pid = 3, status = zombie, task_type = kernel
[0.550000] [I] [syscall] [syscall_get_tasks_info] [3] name = t2_parent, pid = 4, status = zombie, task_type = kernel
[0.550000] [I] [syscall] [syscall_get_tasks_info] [4] name = t3_parent, pid = 6, status = zombie, task_type = kernel
[0.550000] [I] [syscall] [syscall_get_tasks_info] [5] name = t4_ring3, pid = 17, status = running, task_type = user
[0.550000] [I] [syscall] [syscall_get_tasks_info] ---
[0.560000] [I] [test] [multitask_test] OK: 6 seed tasks spawned, run schedule budget...
file_test: pid=18 argc=1
OK: elf magic 127 69 76 70
OK: write/read match
OK: lseek+read
OK: unlink+reopen-fail
file_test: ALL PASSED
user ELF: wakeup -> exit
[1.060000] [I] [test] [multitask_test] budget: elapsed=48 ticks (start=58 now=106)
[1.070000] [I] [test] [multitask_test] subcase results:
[1.080000] [I] [test] [multitask_test] [1] alternation: A=500 B=500 -> [1.080000] [I] [test] [multitask_test] PASS
[1.090000] [I] [test] [multitask_test] [2] fork-wait: status=1 pid=5 exit=42 -> [1.090000] [I] [test] [multitask_test] PASS
[1.100000] [I] [test] [multitask_test] [3] 10-child pressure: status=1
[1.110000] [I] [test] [multitask_test] all 10 exit_codes matched, PASS
[1.120000] [I] [test] [multitask_test] [4] ring3 smoke: exited=1 -> [1.120000] [I] [test] [multitask_test] PASS
[1.130000] [I] [test] [multitask_test] [5] file syscall: exited=1 -> [1.130000] [I] [test] [multitask_test] PASS
[1.140000] [I] [test] [multitask_test] multitask: ALL PASSED
[1.150000] [I] [test] [hard_driver_test] ata test skipped (use simplified mode)
[1.160000] [I] [test] [http_server_test] tcp server listening on port 1234
[1.170000] [D] [udp] [jlos_udp_handler_init] handler initialized
[1.180000] [D] [udp] [jlos_udp_socket_init] socket initialized
[1.190000] [I] [test] [udp_server_test] udp server listening on port 5678

```
