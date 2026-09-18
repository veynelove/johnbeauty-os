#ifndef _JLOS_HAL_H
#define _JLOS_HAL_H

#include <common/types.h>
#include <hal/io.h>

#ifndef HAL_CONFIG_TRACE_IO
#define HAL_CONFIG_TRACE_IO     0
#endif

#define JLOS_HAL_IO_RANGES_MAX  16
#define JLOS_HAL_IO_OWNER_LEN   24

#define JLOS_HAL_IRQ_MAX        256
#define JLOS_HAL_IRQ_OWNER_LEN  24

typedef enum {
    JLOS_HAL_IRQ_PIC_8259       = 1,
    JLOS_HAL_IRQ_APIC           = 2,
} jlos_hal_irq_mode_t;

typedef enum {
    JLOS_HAL_TIMER_PIT_8253     = 1,
    JLOS_HAL_TIMER_HPET         = 2,
    JLOS_HAL_TIMER_ARM_GEN      = 3,
} jlos_hal_timer_mode_t;

typedef struct {
    void        (*init_io8)(jlos_io8_t *self, uint16_t port);
    void        (*write_io8)(jlos_io8_t *self, uint8_t val);
    uint8_t     (*read_io8)(jlos_io8_t *self);

    void        (*init_io8_slow)(jlos_io8_slow_t *self, uint16_t port);
    void        (*write_io8_slow)(jlos_io8_slow_t *self, uint8_t val);
    uint8_t     (*read_io8_slow)(jlos_io8_slow_t *self);

    void        (*init_io16)(jlos_io16_t *self, uint16_t port);
    void        (*write_io16)(jlos_io16_t *self, uint16_t val);
    uint16_t    (*read_io16)(jlos_io16_t *self);

    void        (*init_io32)(jlos_io32_t *self, uint16_t port);
    void        (*write_io32)(jlos_io32_t *self, uint32_t val);
    uint32_t    (*read_io32)(jlos_io32_t *self);
} jlos_hal_io_ops_t;

typedef struct {
    uint16_t start;
    uint16_t end;                    
    char     owner[JLOS_HAL_IO_OWNER_LEN];
    uint8_t  claimed;
} jlos_hal_io_range_t;

typedef struct {
    char    owner[JLOS_HAL_IRQ_OWNER_LEN];
    uint8_t claimed;
} jlos_hal_irq_info_t;

typedef struct {
    
    uint32_t                cpu_model;
    bool                    cpu_has_cpuid;
    bool                    cpu_has_apic;
    jlos_hal_irq_mode_t     irq_mode;
    uint16_t                irq_base_vector;
    uint32_t                irq_reserved_bitmap_31_0;
    jlos_hal_timer_mode_t   timer_mode;
    uint64_t                timer_input_clock_hz;
    uint32_t                pci_mmconfig_base;
    bool                    pci_ecam_available;
    uint32_t                mmio_reserved_start;
    uint32_t                mmio_reserved_end;
} jlos_hal_info_t;

extern const jlos_hal_io_ops_t *jlos_hal_io_ops;

int jlos_hal_io_sanity_check(uint16_t port, int is_write, const char *owner);

int jlos_hal_register_io_range(uint16_t start, uint16_t end, const char *owner);
int jlos_hal_unregister_io_range(uint16_t start, uint16_t end);
const jlos_hal_io_range_t *jlos_hal_get_io_ranges(int *out_count);

int jlos_hal_irq_claim(uint8_t irq, const char *owner);
int jlos_hal_irq_release(uint8_t irq);
int jlos_hal_irq_is_claimed(uint8_t irq, char *out_owner, int owner_bufsz);
const jlos_hal_irq_info_t *jlos_hal_get_irq_table(int *out_count);

const jlos_hal_info_t *jlos_hal_get_info(void);
jlos_hal_info_t *jlos_hal_info_get_for_init(void);

void jlos_hal_irq_refresh_reserved_bitmap(void);

#endif
