#ifndef __JLOS_HAL_DIAG_H
#define __JLOS_HAL_DIAG_H

#include <tools/config.h>
#include <common/types.h>
#include <hal/hal.h>

typedef enum {
    JLOS_HAL_TRACE_OP_WR8  = 0,
    JLOS_HAL_TRACE_OP_RD8  = 1,
    JLOS_HAL_TRACE_OP_WR16 = 2,
    JLOS_HAL_TRACE_OP_RD16 = 3,
    JLOS_HAL_TRACE_OP_WR32 = 4,
    JLOS_HAL_TRACE_OP_RD32 = 5,
    JLOS_HAL_TRACE_OP_MSG  = 6,
} jlos_hal_trace_op_t;

#if HAL_CONFIG_TRACE_IO

void jlos_hal_trace_io(const char *file, uint16_t line,
                       jlos_hal_trace_op_t op,
                       uint16_t port, uint32_t value);
void jlos_hal_trace_msg(const char *file, uint16_t line, const char *msg);
void jlos_hal_trace_dump(int last_n);
void jlos_hal_trace_clear(void);
int  jlos_hal_trace_count(void);

#define HAL_TRACE_IO(op, port, val)   jlos_hal_trace_io(__FILE__, (uint16_t)__LINE__, (op), (uint16_t)(port), (uint32_t)(val))
#define HAL_TRACE_MSG(m)              jlos_hal_trace_msg(__FILE__, (uint16_t)__LINE__, (m))
#define HAL_TRACE_DUMP(n)             jlos_hal_trace_dump(n)
#define HAL_TRACE_CLEAR()             jlos_hal_trace_clear()
#define HAL_TRACE_COUNT()             jlos_hal_trace_count()

#else

#define HAL_TRACE_IO(op, port, val)   do {} while (0)
#define HAL_TRACE_MSG(m)              do {} while (0)
#define HAL_TRACE_DUMP(n)             do {} while (0)
#define HAL_TRACE_CLEAR()             do {} while (0)
#define HAL_TRACE_COUNT()             (0)

static inline void jlos_hal_trace_io(const char *f, uint16_t l, jlos_hal_trace_op_t o, uint16_t p, uint32_t v) { (void)f;(void)l;(void)o;(void)p;(void)v; }
static inline void jlos_hal_trace_msg(const char *f, uint16_t l, const char *m) { (void)f;(void)l;(void)m; }
static inline void jlos_hal_trace_dump(int n) { (void)n; }
static inline void jlos_hal_trace_clear(void) {}
static inline int  jlos_hal_trace_count(void) { return 0; }

#endif

#endif
