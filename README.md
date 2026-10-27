# JohnSunshine OS

> 公主平安开心 🌸

### 环境依赖

```text

宿主机：windows11
虚拟机: vmplayer + ubuntu24.04
依赖：sudo apt install xorriso clangd bear

note: clangd代码分析 + gcc编译
vscode下载clangd插件，禁用microsoft的c/c++插件，编译数据库按架构分别生成：

```bash
make ARCH=x86 compdb      # -> build/x86_64/compile_commands.json
make ARCH=arm64 compdb    # -> build/arm64/compile_commands.json
```

```

### 主机是intel芯片，需要调试arm64架构代码，环境配置

```bash

mkdir -p ~/toolchains
tar -xf arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-elf.tar.xz -C ~/toolchains
~/toolchains/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-gcc --version
# 预期: aarch64-none-elf-gcc (Arm GNU Toolchain 13.3.Rel1 ...) 13.3.1

sudo apt install libglib2.0-dev libpixman-1-dev

wget -O ~/qemu-9.2.0.tar.gz https://github.com/qemu/qemu/archive/refs/tags/v9.2.0.tar.gz
tar -xf ~/qemu-9.2.0.tar.gz -C ~

python3 -m venv ~/qemu-build
~/qemu-build/bin/pip install meson ninja
export PATH="$HOME/qemu-build/bin:$PATH"

cd ~/qemu-9.2.0
./configure --target-list=aarch64-softmmu --prefix=$HOME/qemu --disable-werror --disable-docs
ninja -C build install
~/qemu/bin/qemu-system-aarch64 --version

export PATH="$HOME/toolchains/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-elf/bin:$PATH"
make ARCH=arm64

make ARCH=arm64 run

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

make            # 默认编译x86_64架构镜像
make ARCH=x86 BITS=32
make ARCH=arm64

make ARCH=arm64 run #编译并在qemu运行arm64镜像,运行日志存放在out/serial.log里。(note: x86镜像请通过auto_build_run bat脚本，在虚拟机启动)

make clean      # 清理编译生成产物，包括jlcy子项目编译产物
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

x86_64

[1970-01-01 08:00:00] [I] [boot] [john_sunshine_main] princess yihan is safe and happy!
[1970-01-01 08:00:00] [I] [dev] [jlos_device_init] boot mem: 11 entries, start = 0x100000, end = 0x10000000, available = 261630 KB
[1970-01-01 08:00:00] [I] [boot] [john_sunshine_main] paging initialized
[1970-01-01 08:00:00] [I] [mm] [jlos_memory_manager_init_main] init_main: first=0xffffffff00000000 size=4194256 heap=[0xffffffff00000000,0xffffffff00400000) current=0xffffffff00400000
[1970-01-01 08:00:00] [I] [lapic] [jlos_arch_lapic_init] lapic id = 0, base = 0xfee00000
[1970-01-01 08:00:00] [D] [acpi] [jlos_acpi_init] xsdt = 0xfeee923 len = 36 oem = 50544c544420 ext = 0xd3
[1970-01-01 08:00:00] [D] [acpi] [jlos_acpi_init] cpus num = 4u, lapic base = 0xfee00000
[1970-01-01 08:00:00] [D] [acpi] [jlos_acpi_init] cpu id = 0, apic_id = 0
[1970-01-01 08:00:00] [D] [acpi] [jlos_acpi_init] cpu id = 1, apic_id = 2
[1970-01-01 08:00:00] [D] [acpi] [jlos_acpi_init] cpu id = 2, apic_id = 4
[1970-01-01 08:00:00] [D] [acpi] [jlos_acpi_init] cpu id = 3, apic_id = 6
[1970-01-01 08:00:00] [I] [ioapic] [jlos_ioapic_init] ioapic id = 1, gsi_base = 0, gsi_count = 24
[1970-01-01 08:00:00] [I] [lapic] [jlos_arch_lapic_timer_calibrate] lapic timer freq = 66179500 hz
[1970-01-01 08:00:00] [I] [tsc] [tsc_register] freq = 2497 MHz
[1970-01-01 08:00:00] [D] [pci] [pci_network_controller_handle] amd am79c973 pci command: 0x7
[1970-01-01 08:00:00] [D] [pci] [pci_network_controller_handle] allocating amd am79c973 driver structure
[1970-01-01 08:00:00] [D] [pci] [pci_network_controller_handle] amd am79c9973 driver allocated at: 0xffff800000c40008
[1970-01-01 08:00:00] [I] [eth] [jlos_amd_am79c973_init] IRQ=b interrupt=2b
[1970-01-01 08:00:00] [D] [eth] [jlos_amd_am79c973_init] MAC port registers: C 0 7A 29 A0 1F
[1970-01-01 08:00:00] [D] [eth] [jlos_amd_am79c973_init] init RLEN=0x50 (32 recv) SLEN=0x30 (8 send)
[1970-01-01 08:00:00] [D] [ata] [jlos_ata_block_dev_init] ata primary device ready, sectors = 32768
[1970-01-01 08:00:00] [I] [clocke] [jlos_clock_event_select] selected = lapic_timer, rating = 400
[1970-01-01 08:00:00] [I] [clocks] [clocksource_select] selected = TSC, rating = 300
[1970-01-01 08:00:00] [D] [rootfs] [jlos_rootfs_init] rootfs mounted on partition 0, type 0xc
[2026-10-27 21:09:54] [I] [rtc] [rtc_init] rtc: 1793106594 seconds since epoch
[2026-10-27 21:09:54] [I] [lapic] [jlos_arch_lapic_init] lapic id = 2, base = 0xfee00000
[2026-10-27 21:09:54] [I] [smp] [jlos_ap_main] AP 1 online
[2026-10-27 21:09:54] [I] [smp] [jlos_hal_smp_boot_aps] AP 1 (apic 2) brought up
[2026-10-27 21:09:54] [I] [clocke] [jlos_clock_event_select] selected = lapic_timer, rating = 400
[2026-10-27 21:09:54] [I] [smp] [jlos_ap_main] ap 1 idle ready
[2026-10-27 21:09:54] [I] [lapic] [jlos_arch_lapic_init] lapic id = 4, base = 0xfee00000
[2026-10-27 21:09:54] [I] [smp] [jlos_ap_main] AP 2 online
[2026-10-27 21:09:54] [I] [smp] [jlos_hal_smp_boot_aps] AP 2 (apic 4) brought up
[2026-10-27 21:09:54] [I] [clocke] [jlos_clock_event_select] selected = lapic_timer, rating = 400
[2026-10-27 21:09:54] [I] [smp] [jlos_ap_main] ap 2 idle ready
[2026-10-27 21:09:54] [I] [lapic] [jlos_arch_lapic_init] lapic id = 6, base = 0xfee00000
[2026-10-27 21:09:54] [I] [smp] [jlos_ap_main] AP 3 online
[2026-10-27 21:09:54] [I] [smp] [jlos_hal_smp_boot_aps] AP 3 (apic 6) brought up
[2026-10-27 21:09:54] [I] [clocke] [jlos_clock_event_select] selected = lapic_timer, rating = 400
[2026-10-27 21:09:54] [D] [eth] [jlos_amd_am79c973_activate] BCR written with 0x102
[2026-10-27 21:09:54] [I] [smp] [jlos_ap_main] ap 3 idle ready
[2026-10-27 21:09:54] [D] [eth] [jlos_amd_am79c973_activate] CSR0 written with 0x04 (STOP)
[2026-10-27 21:09:54] [I] [eth] [jlos_amd_am79c973_activate] STOP acknowledged
[2026-10-27 21:09:54] [D] [eth] [jlos_amd_am79c973_activate] post-start CSR0=0x1f3 STRT=1 INEA=1 INTR=1 RXON=1 TXON=1 RINT=0 TINT=0 IDON=1
[2026-10-27 21:09:54] [I] [eth] [jlos_amd_am79c973_activate] activation complete
[2026-10-27 21:09:54] [I] [net] [network_init] initializing network stack (unconfigured)
[2026-10-27 21:09:54] [I] [arp] [jlos_arp_init] initializing ARP protocol
[2026-10-27 21:09:54] [I] [udp] [jlos_udp_provider_init] initialized
[2026-10-27 21:09:54] [I] [dhcp] [jlos_dhcp_client_init] client initialized, xid = 0
[2026-10-27 21:09:54] [I] [net] [network_init] network stack initialization complete
[2026-10-27 21:09:54] [D] [eth] [jlos_amd_am79c973_send] send: 342 bytes
[2026-10-27 21:09:54] [D] [eth] [jlos_amd_am79c973_send] post-send CSR0=0x3f3 RINT=0 TINT=1 INTR=1 INEA=1 RXON=1 TXON=1 sent=1 sflags=0x3080eaa
[2026-10-27 21:09:54] [I] [dhcp] [jlos_dhcp_client_discover] DISC sent, xid = 5e398cd
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all] === memory ===
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all]   PASS  roundtrip
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all]   PASS  kvheap_fallback
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all]   PASS  large_contig
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all]   PASS  small_slab
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all]   PASS  boundary
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all] === timek ===
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all]   PASS  realtime_ge_monotonic
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all]   PASS  ticks_non_decreasing
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all]   PASS  monotonic_increasing
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all] === hrtimer ===
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all]   PASS  ordering
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all]   PASS  periodic
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all]   PASS  lifecycle
[2026-10-27 21:09:54] [I] [test] [jlos_test_run_all] === multitask ===
[2026-10-27 21:09:57] [I] [test] [jlos_test_run_all]   PASS  sleep_until_cascade
[2026-10-27 21:09:57] [I] [test] [jlos_test_run_all]   PASS  sleep_until_short
u[2026-10-27 21:09:57] [I] [test] [jlos_test_run_all]   PASS  ring3_nanosleep
ummap_test: pid=9
mmap_test: ALL PASSED
[2026-10-27 21:09:57] [I] [test] [jlos_test_run_all]   PASS  ring3_mmap
usignal_test: pid=10
signal_test: caught sig=15
signal_test: ALL PASSED
[2026-10-27 21:09:58] [I] [test] [jlos_test_run_all]   PASS  ring3_signal
ufile_test: pid=13 argc=1
OK: elf magic 127 69 76 70
OK: write/read match
OK: lseek+read
OK: unlink+reopen-fail
OK: mmap/munmap anon
file_test: ALL PASSED
[2026-10-27 21:09:58] [I] [test] [jlos_test_run_all]   PASS  ring3_file
uhello from user ELF. PID = 14, argc = 1, argv[0] = /hello.elf, curr time: 1793106598
[2026-10-27 21:09:58] [I] [syscall] [syscall_get_tasks_info] --- task list ---
[2026-10-27 21:09:58] [I] [syscall] [syscall_get_tasks_info] [0] name = idle, pid = 0, status = running, task_type = kernel
[2026-10-27 21:09:58] [I] [syscall] [syscall_get_tasks_info] [1] name = idle, pid = 3, status = running, task_type = kernel
[2026-10-27 21:09:58] [I] [syscall] [syscall_get_tasks_info] [2] name = idle, pid = 4, status = running, task_type = kernel
[2026-10-27 21:09:58] [I] [syscall] [syscall_get_tasks_info] [3] name = idle, pid = 5, status = running, task_type = kernel
[2026-10-27 21:09:58] [I] [syscall] [syscall_get_tasks_info] [4] name = mt_ring3, pid = 14, status = running, task_type = user
[2026-10-27 21:09:58] [I] [syscall] [syscall_get_tasks_info] ---
user ELF: wakeup -> exit
[2026-10-27 21:09:58] [I] [test] [jlos_test_run_all]   PASS  ring3_smoke
ufork_test: pid=15
fork_test: fork+wait OK
fork_test: 10-child pressure OK
fork_test: ALL PASSED
[2026-10-27 21:09:59] [I] [test] [jlos_test_run_all]   PASS  fork_pressure
[2026-10-27 21:09:59] [I] [test] [jlos_test_run_all]   PASS  schedule_alternation
[2026-10-27 21:09:59] [I] [test] [jlos_test_run_all] === pfa ===
[2026-10-27 21:09:59] [I] [test] [jlos_test_run_all]   PASS  pressure_and_accounting
[2026-10-27 21:09:59] [I] [test] [jlos_test_run_all]   PASS  owner_type
[2026-10-27 21:09:59] [I] [test] [jlos_test_run_all]   PASS  refcount
[2026-10-27 21:09:59] [I] [test] [jlos_test_run_all]   PASS  alloc_n
[2026-10-27 21:09:59] [I] [test] [jlos_test_run_all]   PASS  order_alloc
[2026-10-27 21:09:59] [I] [test] [jlos_test_run_all]   PASS  single_frame_roundtrip
[2026-10-27 21:09:59] [I] [test] [jlos_test_run_all] === sync ===
[2026-10-27 21:10:01] [I] [test] [jlos_test_run_all]   PASS  cond_wait_broadcast
[2026-10-27 21:10:02] [I] [test] [jlos_test_run_all]   PASS  cond_wait_signal
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  semaphore_block_and_wake
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  mutex_recursive_reentry
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  mutex_lock_unlock
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  semaphore_count_down_up
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] === paging ===
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  user_accessible
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  clone
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  change_flags
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  map_range
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  map_unmap
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] === ata ===
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  write_read_roundtrip
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  read_mbr
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  identify
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] === udp ===
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  server_listen
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] === http ===
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  server_listen
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] === timer_wheel ===
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  same_slot_multiple
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  empty_advance
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  del_prevents_expire
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  boundary_deltas
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  cascade_multi_level
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  cascade_vec2_to_vec1
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  init_all_slots
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] === rbtree ===
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  random_insert_remove
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  find_le
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  remove
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  ordered_traversal
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all]   PASS  insert_and_find
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] === summary ===
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] suite             pass  fail total
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] memory               5     0     5
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] timek                3     0     3
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] hrtimer              3     0     3
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] multitask            9     0     9
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] pfa                  6     0     6
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] sync                 6     0     6
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] paging               5     0     5
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] ata                  3     0     3
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] udp                  1     0     1
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] http                 1     0     1
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] timer_wheel          7     0     7
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] rbtree               5     0     5
[2026-10-27 21:10:03] [I] [test] [jlos_test_run_all] total               54     0    54

ARM64

[1970-01-01 08:00:00] [I] [boot] [john_sunshine_main] princess yihan is safe and happy!
[1970-01-01 08:00:00] [I] [dev] [jlos_device_init] boot mem: 1 entries, start = 0x40000000, end = 0x48000000, available = 131072 KB
[1970-01-01 08:00:00] [I] [boot] [john_sunshine_main] paging initialized
[1970-01-01 08:00:00] [I] [mm] [jlos_memory_manager_init_main] init_main: first=0xffff800200000000 size=4194256 heap=[0xffff800200000000,0xffff800200400000) current=0xffff800200400000
[1970-01-01 08:00:00] [I] [gic] [gic_init] dist = 0xffff000008000000, rdist = 0xffff0000080a0000, stride = 0x20000
[1970-01-01 08:00:00] [I] [timer] [jlos_arch_timer_init] freq = 1000000000 hz, intid = 30
[1970-01-01 08:00:00] [I] [virtio] [jlos_virtio_blk_dev_init] blk0 ready, sectors = 32768
[1970-01-01 08:00:00] [I] [virtio] [jlos_virtio_net_dev_init] net0 ready, mac = 0x563412005452, intid = 78
[1970-01-01 08:00:00] [E] [ata] [jlos_ata_block_dev_init] ata primary device init failed
[1970-01-01 08:00:00] [I] [clocke] [jlos_clock_event_select] selected = arch_timer, rating = 400
[1970-01-01 08:00:00] [I] [clocks] [clocksource_select] selected = arch_timer, rating = 350
[1970-01-01 08:00:00] [D] [rootfs] [jlos_rootfs_init] rootfs mounted on partition 0, type 0xc
[2026-10-27 21:08:30] [I] [rtc] [pl031_init] rtc: 1793106510 seconds since epoch
[2026-10-27 21:08:30] [I] [clocke] [jlos_clock_event_select] selected = arch_timer, rating = 400
[2026-10-27 21:08:30] [I] [smp] [jlos_ap_main] AP 1 online
[2026-10-27 21:08:30] [I] [clocke] [jlos_clock_event_select] selected = arch_timer, rating = 400
[2026-10-27 21:08:30] [I] [smp] [jlos_ap_main] ap 1 idle ready
[2026-10-27 21:08:30] [I] [smp] [jlos_ap_main] AP 2 online
[2026-10-27 21:08:30] [I] [clocke] [jlos_clock_event_select] selected = arch_timer, rating = 400
[2026-10-27 21:08:30] [I] [smp] [jlos_ap_main] ap 2 idle ready
[2026-10-27 21:08:30] [I] [smp] [jlos_ap_main] AP 3 online
[2026-10-27 21:08:30] [I] [smp] [jlos_hal_smp_boot_aps] 4 cpus booted
[2026-10-27 21:08:30] [I] [smp] [jlos_ap_main] ap 3 idle ready
[2026-10-27 21:08:30] [I] [net] [network_init] initializing network stack (unconfigured)
[2026-10-27 21:08:30] [I] [arp] [jlos_arp_init] initializing ARP protocol
[2026-10-27 21:08:30] [I] [udp] [jlos_udp_provider_init] initialized
[2026-10-27 21:08:30] [I] [dhcp] [jlos_dhcp_client_init] client initialized, xid = 0
[2026-10-27 21:08:30] [I] [net] [network_init] network stack initialization complete
[2026-10-27 21:08:30] [I] [dhcp] [jlos_dhcp_client_discover] DISC sent, xid = 4b19807
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all] === memory ===
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all]   PASS  roundtrip
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all]   PASS  kvheap_fallback
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all]   PASS  large_contig
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all]   PASS  small_slab
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all]   PASS  boundary
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all] === timek ===
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all]   PASS  realtime_ge_monotonic
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all]   PASS  ticks_non_decreasing
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all]   PASS  monotonic_increasing
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all] === hrtimer ===
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all]   PASS  ordering
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all]   PASS  periodic
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all]   PASS  lifecycle
[2026-10-27 21:08:30] [I] [test] [jlos_test_run_all] === multitask ===
[2026-10-27 21:08:33] [I] [test] [jlos_test_run_all]   PASS  sleep_until_cascade
[2026-10-27 21:08:33] [I] [test] [jlos_test_run_all]   PASS  sleep_until_short
u[2026-10-27 21:08:33] [I] [test] [jlos_test_run_all]   PASS  ring3_nanosleep
ummap_test: pid=9
mmap_test: ALL PASSED
[2026-10-27 21:08:33] [I] [test] [jlos_test_run_all]   PASS  ring3_mmap
usignal_test: pid=10
signal_test: caught sig=15
signal_test: ALL PASSED
[2026-10-27 21:08:33] [I] [test] [jlos_test_run_all]   PASS  ring3_signal
ufile_test: pid=13 argc=1
OK: elf magic 127 69 76 70
OK: write/read match
OK: lseek+read
OK: unlink+reopen-fail
OK: mmap/munmap anon
file_test: ALL PASSED
[2026-10-27 21:08:33] [I] [test] [jlos_test_run_all]   PASS  ring3_file
uhello from user ELF. PID = 14, argc = 1, argv[0] = /hello.elf, curr time: 1793106513
[2026-10-27 21:08:33] [I] [syscall] [syscall_get_tasks_info] --- task list ---
[2026-10-27 21:08:33] [I] [syscall] [syscall_get_tasks_info] [0] name = idle, pid = 0, status = running, task_type = kernel
[2026-10-27 21:08:33] [I] [syscall] [syscall_get_tasks_info] [1] name = idle, pid = 3, status = running, task_type = kernel
[2026-10-27 21:08:33] [I] [syscall] [syscall_get_tasks_info] [2] name = idle, pid = 4, status = running, task_type = kernel
[2026-10-27 21:08:33] [I] [syscall] [syscall_get_tasks_info] [3] name = idle, pid = 5, status = running, task_type = kernel
[2026-10-27 21:08:33] [I] [syscall] [syscall_get_tasks_info] [4] name = mt_ring3, pid = 14, status = running, task_type = user
[2026-10-27 21:08:33] [I] [syscall] [syscall_get_tasks_info] ---
user ELF: wakeup -> exit
[2026-10-27 21:08:34] [I] [test] [jlos_test_run_all]   PASS  ring3_smoke
ufork_test: pid=15
fork_test: fork+wait OK
fork_test: 10-child pressure OK
fork_test: ALL PASSED
[2026-10-27 21:08:34] [I] [test] [jlos_test_run_all]   PASS  fork_pressure
[2026-10-27 21:08:34] [I] [test] [jlos_test_run_all]   PASS  schedule_alternation
[2026-10-27 21:08:34] [I] [test] [jlos_test_run_all] === pfa ===
[2026-10-27 21:08:34] [I] [test] [jlos_test_run_all]   PASS  pressure_and_accounting
[2026-10-27 21:08:34] [I] [test] [jlos_test_run_all]   PASS  owner_type
[2026-10-27 21:08:34] [I] [test] [jlos_test_run_all]   PASS  refcount
[2026-10-27 21:08:34] [I] [test] [jlos_test_run_all]   PASS  alloc_n
[2026-10-27 21:08:34] [I] [test] [jlos_test_run_all]   PASS  order_alloc
[2026-10-27 21:08:34] [I] [test] [jlos_test_run_all]   PASS  single_frame_roundtrip
[2026-10-27 21:08:34] [I] [test] [jlos_test_run_all] === sync ===
[2026-10-27 21:08:36] [I] [test] [jlos_test_run_all]   PASS  cond_wait_broadcast
[2026-10-27 21:08:37] [I] [test] [jlos_test_run_all]   PASS  cond_wait_signal
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  semaphore_block_and_wake
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  mutex_recursive_reentry
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  mutex_lock_unlock
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  semaphore_count_down_up
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] === paging ===
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  user_accessible
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  clone
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  change_flags
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  map_range
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  map_unmap
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] === ata ===
[2026-10-27 21:08:38] [I] [t_ata] [jlos_test_fn_ata_write_read_roundtrip] skip: no ata device
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  write_read_roundtrip
[2026-10-27 21:08:38] [I] [t_ata] [jlos_test_fn_ata_read_mbr] skip: no ata device
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  read_mbr
[2026-10-27 21:08:38] [I] [t_ata] [jlos_test_fn_ata_identify] skip: no ata device
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  identify
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] === udp ===
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  server_listen
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] === http ===
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  server_listen
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] === timer_wheel ===
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  same_slot_multiple
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  empty_advance
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  del_prevents_expire
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  boundary_deltas
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  cascade_multi_level
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  cascade_vec2_to_vec1
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  init_all_slots
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] === rbtree ===
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  random_insert_remove
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  find_le
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  remove
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  ordered_traversal
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all]   PASS  insert_and_find
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] === summary ===
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] suite             pass  fail total
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] memory               5     0     5
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] timek                3     0     3
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] hrtimer              3     0     3
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] multitask            9     0     9
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] pfa                  6     0     6
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] sync                 6     0     6
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] paging               5     0     5
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] ata                  3     0     3
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] udp                  1     0     1
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] http                 1     0     1
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] timer_wheel          7     0     7
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] rbtree               5     0     5
[2026-10-27 21:08:38] [I] [test] [jlos_test_run_all] total               54     0    54

```
