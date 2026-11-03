/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lib/syscall.h>

void printf(const char *fmt, ...)
{
    char buf[256];
    int pos = 0;
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    for (int i = 0; fmt[i] && pos < (int)sizeof(buf) - 1; i++) {
        if (fmt[i] == '%') {
            i++;
            if (!fmt[i]) break;
            switch (fmt[i]) {
            case 'd': {
                int32_t val = __builtin_va_arg(args, int32_t);
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
                uint32_t val = __builtin_va_arg(args, uint32_t);
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
                const char *s = __builtin_va_arg(args, const char *);
                while (*s && pos < (int)sizeof(buf) - 1) {
                    buf[pos++] = *s++;
                }
                break;
            }
            case 'c': {
                char c = (char)__builtin_va_arg(args, int);
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
    __builtin_va_end(args);
    write(TASK_FD_STD_OUT, buf, pos);
}
