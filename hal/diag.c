#include <hal/diag.h>
#include <hal/timer.h>

#if HAL_CONFIG_TRACE_IO

#define JLOS_HAL_TRACE_CAPACITY  512

extern void printf(const char *str);
extern void printf_hex32(uint32_t value);
extern void printf_hex16(uint16_t value);
extern void printf_hex8(uint8_t value);
extern void printf_char(char c);
extern void printf_uint(uint32_t value);

typedef struct {
    const char *file;
    uint16_t    line;
    uint8_t     op;
    uint16_t    port;
    uint32_t    value;
    uint32_t    tick;
} jlos_hal_trace_rec_t;

static jlos_hal_trace_rec_t s_trace_buf[JLOS_HAL_TRACE_CAPACITY];
static volatile uint32_t    s_trace_head;
static volatile uint32_t    s_trace_wrap;
static volatile int         s_trace_inited;

static void trace_ensure_init(void)
{
    if (s_trace_inited) return;
    for (int i = 0; i < JLOS_HAL_TRACE_CAPACITY; i++) {
        s_trace_buf[i].file = 0;
        s_trace_buf[i].line = 0;
        s_trace_buf[i].op   = 0xFF;
    }
    s_trace_head = 0;
    s_trace_wrap = 0;
    s_trace_inited = 1;
}

static uint32_t trace_safe_tick(void)
{
    /* timer 可能还没初始化；返回 0 不影响排序 */
    extern uint32_t jlos_hal_timer_get_ticks(void);
    if (jlos_hal_timer_get_ticks == 0) return 0;
    return jlos_hal_timer_get_ticks();
}

void jlos_hal_trace_io(const char *file, uint16_t line,
                       jlos_hal_trace_op_t op,
                       uint16_t port, uint32_t value)
{
    trace_ensure_init();
    uint32_t i = s_trace_head;
    s_trace_buf[i].file  = file;
    s_trace_buf[i].line  = line;
    s_trace_buf[i].op    = (uint8_t)op;
    s_trace_buf[i].port  = port;
    s_trace_buf[i].value = value;
    s_trace_buf[i].tick  = trace_safe_tick();
    i++;
    if (i >= JLOS_HAL_TRACE_CAPACITY) { i = 0; s_trace_wrap = 1; }
    s_trace_head = i;
}

void jlos_hal_trace_msg(const char *file, uint16_t line, const char *msg)
{
    /* 用 value=0 + port 编码 msg 指针不太好，简化：复用 WR8+msg 文件指针，内容由 dump 时按 OP_MSG 识别 */
    trace_ensure_init();
    uint32_t i = s_trace_head;
    s_trace_buf[i].file  = file;
    s_trace_buf[i].line  = line;
    s_trace_buf[i].op    = (uint8_t)JLOS_HAL_TRACE_OP_MSG;
    s_trace_buf[i].port  = 0;
    s_trace_buf[i].value = (uint32_t)msg;
    s_trace_buf[i].tick  = trace_safe_tick();
    i++;
    if (i >= JLOS_HAL_TRACE_CAPACITY) { i = 0; s_trace_wrap = 1; }
    s_trace_head = i;
}

void jlos_hal_trace_clear(void)
{
    trace_ensure_init();
    s_trace_head = 0;
    s_trace_wrap = 0;
    for (int i = 0; i < JLOS_HAL_TRACE_CAPACITY; i++) s_trace_buf[i].op = 0xFF;
}

int jlos_hal_trace_count(void)
{
    trace_ensure_init();
    if (s_trace_wrap) return JLOS_HAL_TRACE_CAPACITY;
    return (int)s_trace_head;
}

static const char *op_name(uint8_t op)
{
    switch (op) {
    case JLOS_HAL_TRACE_OP_WR8:  return "WR8 ";
    case JLOS_HAL_TRACE_OP_RD8:  return "RD8 ";
    case JLOS_HAL_TRACE_OP_WR16: return "WR16";
    case JLOS_HAL_TRACE_OP_RD16: return "RD16";
    case JLOS_HAL_TRACE_OP_WR32: return "WR32";
    case JLOS_HAL_TRACE_OP_RD32: return "RD32";
    case JLOS_HAL_TRACE_OP_MSG:  return "MSG ";
    default:                     return "?   ";
    }
}

static void print_basename(const char *path)
{
    if (!path) { printf("(null)"); return; }
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    printf(base);
}

static void print_rec(const jlos_hal_trace_rec_t *r, int idx)
{
    printf("["); printf_uint((uint32_t)idx); printf("] t=");
    printf_hex32(r->tick); printf(" ");
    printf(op_name(r->op)); printf(" port=0x");
    printf_hex16(r->port); printf(" val=0x");
    if (r->op == JLOS_HAL_TRACE_OP_WR8 || r->op == JLOS_HAL_TRACE_OP_RD8)
        printf_hex8((uint8_t)r->value);
    else if (r->op == JLOS_HAL_TRACE_OP_WR16 || r->op == JLOS_HAL_TRACE_OP_RD16)
        printf_hex16((uint16_t)r->value);
    else
        printf_hex32(r->value);
    printf("  ");
    print_basename(r->file);
    printf(":"); printf_uint((uint32_t)r->line);
    printf_char('\n');
}

void jlos_hal_trace_dump(int last_n)
{
    trace_ensure_init();
    int total = jlos_hal_trace_count();
    if (total <= 0) { printf("HAL trace: empty\n"); return; }
    int n = last_n;
    if (n <= 0 || n > total) n = total;
    printf("HAL trace: last "); printf_uint((uint32_t)n);
    printf(" of ");           printf_uint((uint32_t)total);
    printf(" (wrap=");        printf_uint(s_trace_wrap ? 1u : 0u);
    printf(")\n");

    uint32_t start_idx;
    if (s_trace_wrap) {
        start_idx = s_trace_head; /* head 是最老的 */
    } else {
        start_idx = 0;
    }
    int skip = total - n;
    for (int k = 0; k < n; k++) {
        uint32_t i = (start_idx + (uint32_t)(skip + k)) % (uint32_t)JLOS_HAL_TRACE_CAPACITY;
        const jlos_hal_trace_rec_t *r = &s_trace_buf[i];
        if (r->op == 0xFF) continue;
        print_rec(r, total - n + k);
    }
}

#endif
