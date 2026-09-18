#include <lib/user_syscall.h>

void jlos_user_printf(const char *fmt, ...)
{
    char buf[256];
    int pos = 0;
    uint32_t *args = (uint32_t *)&fmt + 1;

    for (int i = 0; fmt[i] && pos < (int)sizeof(buf) - 1; i++) {
        if (fmt[i] == '%') {
            i++;
            if (!fmt[i]) break;
            switch (fmt[i]) {
            case 'd': {
                int32_t val = (int32_t)*args++;
                char tmp[12];
                int tpos = 11;
                tmp[tpos] = '\0';
                if (val < 0) {
                    buf[pos++] = '-';
                    val = -val;
                }
                if (val == 0) {
                    tpos--; tmp[tpos] = '0';
                } else {
                    while (val > 0) { tpos--; tmp[tpos] = '0' + (val % 10); val /= 10; }
                }
                for (int j = tpos; tmp[j]; j++) {
                    if (pos >= (int)sizeof(buf) - 1) break;
                    buf[pos++] = tmp[j];
                }
                break;
            }
            case 'u': {
                uint32_t val = *args++;
                char tmp[12];
                int tpos = 11;
                tmp[tpos] = '\0';
                if (val == 0) {
                    tpos--; tmp[tpos] = '0';
                } else {
                    while (val > 0) { tpos--; tmp[tpos] = '0' + (val % 10); val /= 10; }
                }
                for (int j = tpos; tmp[j]; j++) {
                    if (pos >= (int)sizeof(buf) - 1) break;
                    buf[pos++] = tmp[j];
                }
                break;
            }
            case 's': {
                const char *s = (const char *)*args++;
                while (*s && pos < (int)sizeof(buf) - 1) {
                    buf[pos++] = *s++;
                }
                break;
            }
            case 'c': {
                char c = (char)*args++;
                if (pos < (int)sizeof(buf) - 1) buf[pos++] = c;
                break;
            }
            default:
                if (pos < (int)sizeof(buf) - 1) buf[pos++] = fmt[i];
                break;
            }
        } else {
            buf[pos++] = fmt[i];
        }
    }
    jlos_user_write(JLOS_TASK_FD_STD_OUT, buf, pos);
}
