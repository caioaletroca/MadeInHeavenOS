#include <platform/platform.h>
#include <stdbool.h>
#include <string.h>
#include <x86/io.h>

#define CMOS_INDEX 0x70
#define CMOS_DATA 0x71 // index bit 7 masks NMIs until the next index write: left clear, as Linux

#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS 0x04
#define RTC_DAY 0x07
#define RTC_MONTH 0x08
#define RTC_YEAR 0x09
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B

#define RTC_UPDATING 0x80 // status A: the chip is changing the values right now
#define RTC_BINARY 0x04   // status B: binary values, else BCD
#define RTC_24_HOUR 0x02  // status B: 24-hour mode, else 12-hour
#define RTC_PM 0x80       // hours register in 12-hour mode: afternoon

/**
 * @brief Read a value from the CMOS register.
 *
 * @param reg CMOS register to read.
 * @return The value read from the register.
 */
static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_INDEX, reg);
    return inb(CMOS_DATA);
}

/**
 * @brief Read the raw date and time from the RTC.
 *
 * @param raw Output structure, fully overwritten with the raw RTC values.
 */
static void rtc_read_raw(platform_date_t *raw)
{
    while (cmos_read(RTC_STATUS_A) & RTC_UPDATING)
        ; // wait until the RTC is not updating

    raw->second = cmos_read(RTC_SECONDS);
    raw->minute = cmos_read(RTC_MINUTES);
    raw->hour = cmos_read(RTC_HOURS);
    raw->day = cmos_read(RTC_DAY);
    raw->month = cmos_read(RTC_MONTH);
    raw->year = cmos_read(RTC_YEAR);
}

/**
 * @brief Convert a BCD-encoded value to binary.
 *
 * @param value BCD-encoded value.
 * @return The binary representation of the value.
 */
static int bcd_to_binary(int value)
{
    return (value & 0x0F) + (value >> 4) * 10;
}

void platform_date_read(platform_date_t *date)
{
    // Two equal readings in a row: the clock did not tick in the middle of one read
    platform_date_t again;
    rtc_read_raw(date);
    do
    {
        again = *date;
        rtc_read_raw(date);
    } while (memcmp(&again, date, sizeof(again)) != 0);

    uint8_t format = cmos_read(RTC_STATUS_B);
    bool pm = (date->hour & RTC_PM) != 0;
    date->hour &= ~RTC_PM; // clear the PM bit

    if ((format & RTC_BINARY) == 0)
    {
        date->second = bcd_to_binary(date->second);
        date->minute = bcd_to_binary(date->minute);
        date->hour = bcd_to_binary(date->hour);
        date->day = bcd_to_binary(date->day);
        date->month = bcd_to_binary(date->month);
        date->year = bcd_to_binary(date->year);
    }

    // 12 AM is 0, 12 PM is 12
    if ((format & RTC_24_HOUR) == 0)
        date->hour = date->hour % 12 + (pm ? 12 : 0);

    // Two digits; the century register is not standard, so as Linux: 70-99 → 19xx
    date->year += date->year < 70 ? 2000 : 1900;
}