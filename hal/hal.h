#ifndef __JLOS_HAL_H
#define __JLOS_HAL_H

#include <tools/config.h>
#include <common/types.h>
#include <hal/io.h>

/* HAL 内部 I/O 诊断 trace：1=开启（环形缓存每次 I/O 写入 1 条，可 dump），0=关闭（0 开销）。仅 HAL 内部文件使用。 */
#ifndef HAL_CONFIG_TRACE_IO
#define HAL_CONFIG_TRACE_IO   0
#endif

/* -------------------- IO ops 运行时多态表 -------------------- */

typedef struct {
    void (*init_io8)(jlos_io8_t *self, uint16_t port);
    void (*write_io8)(jlos_io8_t *self, uint8_t val);
    uint8_t (*read_io8)(jlos_io8_t *self);

    void (*init_io8_slow)(jlos_io8_slow_t *self, uint16_t port);
    void (*write_io8_slow)(jlos_io8_slow_t *self, uint8_t val);
    uint8_t (*read_io8_slow)(jlos_io8_slow_t *self);

    void (*init_io16)(jlos_io16_t *self, uint16_t port);
    void (*write_io16)(jlos_io16_t *self, uint16_t val);
    uint16_t (*read_io16)(jlos_io16_t *self);

    void (*init_io32)(jlos_io32_t *self, uint16_t port);
    void (*write_io32)(jlos_io32_t *self, uint32_t val);
    uint32_t (*read_io32)(jlos_io32_t *self);
} jlos_hal_io_ops_t;

/* 全局活跃 IO ops。jlos_hal_arch_init() 根据 CPU 选型设置。 */
extern const jlos_hal_io_ops_t *jlos_hal_io_ops;
extern const jlos_hal_io_ops_t  jlos_hal_x86_fast_io_ops;   /* 直接 in/out，现代 CPU */
extern const jlos_hal_io_ops_t  jlos_hal_x86_slow_io_ops;   /* 带 jmp $+2 延迟，486/老 Cyrix */

/* -------------------- HAL 初始化 -------------------- */

/* HAL 层启动总入口：探测 CPU → 选 ops → 注册系统保留 IO 段 → 注册系统保留 IRQ */
void jlos_hal_arch_init(void);

/* -------------------- IO sanity check -------------------- */

/* 返回 0 = 合法；非 0 = 非法（会打印 warning）。
 * 校验 ① port ∈ [0x0000, 0xFFFF]
 *     ② 写 0xCF8-0xCFF PCI config 专用区间时无 owner claim 会 warn */
int jlos_hal_io_sanity_check(uint16_t port, int is_write, const char *owner);

/* -------------------- IO ranges 注册去重 -------------------- */

#define JLOS_HAL_IO_RANGES_MAX  16
#define JLOS_HAL_IO_OWNER_LEN   24

typedef struct {
    uint16_t start;
    uint16_t end;                    /* inclusive */
    char     owner[JLOS_HAL_IO_OWNER_LEN];
    uint8_t  claimed;
} jlos_hal_io_range_t;

/* 注册一个 IO 区间；与已注册区间有重叠返回 -1，成功返回 0 */
int jlos_hal_register_io_range(uint16_t start, uint16_t end, const char *owner);

/* 取消注册 */
int jlos_hal_unregister_io_range(uint16_t start, uint16_t end);

/* 读全部注册表（调试用） */
const jlos_hal_io_range_t *jlos_hal_get_io_ranges(int *out_count);

/* -------------------- IRQ claim 保留机制 -------------------- */

#define JLOS_HAL_IRQ_MAX  256
#define JLOS_HAL_IRQ_OWNER_LEN  24

typedef struct {
    char owner[JLOS_HAL_IRQ_OWNER_LEN];
    uint8_t claimed;
} jlos_hal_irq_info_t;

/* claim 一个 IRQ；已被 claim 返回 -1，成功返回 0 */
int jlos_hal_irq_claim(uint8_t irq, const char *owner);

/* 释放 IRQ */
int jlos_hal_irq_release(uint8_t irq);

/* 是否已被 claim；是 返回 1，否 返回 0；同时把 owner 名写到 out_owner（非 NULL 时） */
int jlos_hal_irq_is_claimed(uint8_t irq, char *out_owner, int owner_bufsz);

/* 读全部注册表（调试用） */
const jlos_hal_irq_info_t *jlos_hal_get_irq_table(int *out_count);

/* -------------------- 硬件 Probe 结果汇总：jlos_hal_info_t（只读） --------------------
 * 所有上层驱动/内核只能通过 jlos_hal_get_info() 读当前硬件探测结果，
 * 禁止直接假设 "一定是 8259 PIC" / "PIT 频率 1193180Hz"。
 * 未来加 ACPI/CPUID/SMBIOS 只改 jlos_hal_arch_init() 填充这里的值，上层一行不动。 */

typedef enum {
    JLOS_HAL_IRQ_PIC_8259 = 1,   /* 目前默认：双 8259 主从级联 */
    JLOS_HAL_IRQ_APIC     = 2,   /* 未来 I/O APIC + LAPIC */
} jlos_hal_irq_mode_t;

typedef enum {
    JLOS_HAL_TIMER_PIT_8253  = 1, /* 目前默认：x86 8253 PIT */
    JLOS_HAL_TIMER_HPET      = 2, /* 未来 x86 HPET */
    JLOS_HAL_TIMER_ARM_GEN   = 3, /* 未来 ARM Generic Timer */
} jlos_hal_timer_mode_t;

typedef struct {
    /* CPU 信息（未来补 CPUID 探测） */
    uint32_t cpu_model;
    bool     cpu_has_cpuid;
    bool     cpu_has_apic;

    /* 中断控制器 */
    jlos_hal_irq_mode_t irq_mode;
    uint16_t            irq_base_vector;   /* IRQ0 → 向量号。当前双 8259：IRQ0=0x20 */
    uint32_t            irq_reserved_bitmap_31_0; /* bitN=1 表示 IRQ[N] 系统保留（和 irq_claim 同步） */

    /* Timer 信息 */
    jlos_hal_timer_mode_t timer_mode;
    uint64_t              timer_input_clock_hz; /* PIT=1193180，HPET=14.318MHz 等 */

    /* PCI 资源 */
    uint32_t pci_mmconfig_base;    /* ECAM 基地址，无则 0 */
    bool     pci_ecam_available;   /* false = 只能用 Mechanism #1 (0xCF8/0xCFC) */

    /* 内存布局（预留，未来读 E820 / ACPI） */
    uint32_t mmio_reserved_start;
    uint32_t mmio_reserved_end;
} jlos_hal_info_t;

/* 只读查询；HAL 启动后已填充完，上层不要改。 */
const jlos_hal_info_t *jlos_hal_get_info(void);

/* 内部用：每次 irq_claim/irq_release 会同步 irq_reserved_bitmap_31_0，
 * 驱动也可以直接读这个 bitmap 做快速判断（不用翻 256 项 table）。 */
void jlos_hal_irq_refresh_reserved_bitmap(void);

#endif
