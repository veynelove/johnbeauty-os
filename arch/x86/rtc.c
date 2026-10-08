/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/rtc.h>
#include <hal/io.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "rtc"
#include <kernel/printk.h>

#define JLOS_RTC_INDEX_PORT     0x70
#define JLOS_RTC_DATA_PORT      0x71

#define JLOS_RTC_REG_SEC        0x00
#define JLOS_RTC_REG_MIN        0x02
#define JLOS_RTC_REG_HOUR       0x04
#define JLOS_RTC_REG_DAY        0x07
#define JLOS_RTC_REG_MONTH      0x08
#define JLOS_RTC_REG_YEAR       0x09
#define JLOS_RTC_REG_STATUS_A   0x0A
#define JLOS_RTC_REG_STATUS_B   0x0B

#define JLOS_RTC_UIP            0x80
#define JLOS_RTC_DM_BINARY      0x04
#define JLOS_RTC_24HOUR         0x02

static uint8_t rtc_read_reg(uint8_t reg)
{
    jlos_io8_t idx, data;
    jlos_io8_init(&idx, JLOS_RTC_INDEX_PORT);
    jlos_io8_init(&data, JLOS_RTC_DATA_PORT);
    jlos_io8_write(&idx, reg);
    return jlos_io8_read(&data);
}

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static uint32_t rtc_date_to_unix(uint32_t year, uint32_t month, uint32_t day,
                                  uint32_t hour, uint32_t min, uint32_t sec)
{
    static const uint16_t days_before_month[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint32_t days = 0;
    for (uint32_t y = 1970; y < year; y++) {
        if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) {
            days += 366;
        } else {
            days += 365;
        }
    }
    days += days_before_month[month - 1];
    if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        days += 1;
    }
    days += day - 1;

    return days * 86400 + hour * 3600 + min * 60 + sec;
}

uint64_t jlos_hal_rtc_read_ns(void)
{
    while (rtc_read_reg(JLOS_RTC_REG_STATUS_A) & JLOS_RTC_UIP) {
    }

    uint8_t sec = rtc_read_reg(JLOS_RTC_REG_SEC);
    uint8_t min = rtc_read_reg(JLOS_RTC_REG_MIN);
    uint8_t hour = rtc_read_reg(JLOS_RTC_REG_HOUR);
    uint8_t day = rtc_read_reg(JLOS_RTC_REG_DAY);
    uint8_t month = rtc_read_reg(JLOS_RTC_REG_MONTH);
    uint8_t year = rtc_read_reg(JLOS_RTC_REG_YEAR);
    uint8_t reg_b = rtc_read_reg(JLOS_RTC_REG_STATUS_B);

    if (!(reg_b & JLOS_RTC_DM_BINARY)) {
        sec = bcd_to_bin(sec);
        min = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
        day = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year = bcd_to_bin(year);
    }

    if (!(reg_b & JLOS_RTC_24HOUR) && (hour & 0x80)) {
        hour = (hour & 0x7F) + 12;
    }

    uint32_t full_year = 2000 + year;
    if (year >= 70) {
        full_year = 1900 + year;
    }

    uint32_t unix_sec = rtc_date_to_unix(full_year, month, day, hour, min, sec);
    return (uint64_t)unix_sec * 1000000000ULL;
}

static void rtc_init(void)
{
    uint64_t ns = jlos_hal_rtc_read_ns();
    uint32_t secs = (uint32_t)(ns / 1000000000ULL);
    printk_info("rtc: %u seconds since epoch\n", secs);
}

JLOS_INITCALL(JLOS_INITCALL_LATE, rtc_init);
