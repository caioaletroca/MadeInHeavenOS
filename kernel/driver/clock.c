#include <driver/clock.h>
#include <driver/timer.h>
#include <platform/platform.h>

// Realtime minus monotonic: the date the machine booted, in nanoseconds since 1970
static int64_t boot_realtime;

int64_t clock_days_from_civil(int64_t year, int64_t month, int64_t day)
{
    year -= month <= 2;
    int64_t era = (year >= 0 ? year : year - 399) / 400;
    int64_t year_of_era = year - era * 400; // 0-399
    int64_t day_of_year = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    int64_t day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year; // 0-365
    return era * 146097 + day_of_era - 719468;                                                  // 719468: days from 0000-03-01 to 1970-01-01
}

void clock_init(void)
{
    platform_date_t date;
    platform_date_read(&date);

    int64_t days = clock_days_from_civil((int64_t)date.year, (int64_t)date.month, (int64_t)date.day);
    int64_t seconds = days * 86400 + date.hour * 3600 + date.minute * 60 + date.second;
    boot_realtime = seconds * NANOSECONDS_PER_SECOND - (int64_t)timer_nanoseconds();
}

int64_t clock_monotonic(void)
{
    return (int64_t)timer_nanoseconds();
}

int64_t clock_realtime(void)
{
    return boot_realtime + clock_monotonic();
}

int64_t clock_seconds(void)
{
    return clock_realtime() / NANOSECONDS_PER_SECOND;
}