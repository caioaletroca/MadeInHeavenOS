#include <selftest/clock.h>
#include <driver/clock.h>
#include <sched/thread.h>
#include <kprintf.h>
#include <panic.h>

#define SECONDS_PER_DAY 86400
#define EARLIEST_PLAUSIBLE 1704067200 // 2024-01-01 00:00:00 UTC: an RTC before it was not read right
#define SLEEP_TICKS 3
#define TICK_AT_LEAST 9990000 // ns: the PIT's tick is 9 999 312 ns at 100 Hz

typedef struct known_date
{
    int year, month, day;
    int64_t days; // since 1970-01-01, from Python's datetime.date
} known_date_t;

static const known_date_t known_dates[] = {
    {1970, 1, 1, 0},
    {1969, 12, 31, -1},
    {1970, 3, 1, 59},     // after the first February: not a leap year
    {1972, 3, 1, 790},    // after a leap day
    {2000, 2, 29, 11016}, // divisible by 400: a leap year
    {2000, 3, 1, 11017},
    {2024, 2, 29, 19782},
    {2026, 11, 8, 20765},
    {2100, 3, 1, 47541},  // 2100 divisible by 100, not 400: no Feb 29 before this
    {1900, 3, 1, -25508}, // the same rule, before the epoch
};

static void clock_test_days(void)
{
    for (size_t i = 0; i < sizeof(known_dates) / sizeof(known_dates[0]); i++)
    {
        const known_date_t *d = &known_dates[i];
        int64_t days = clock_days_from_civil(d->year, d->month, d->day);
        if (days != d->days)
            panic("Clock self-test: %d-%d-%d is day %d, expected %d\n",
                  d->year, d->month, d->day, (int)days, (int)d->days);
    }
}

static void clock_test_realtime(void)
{
    int64_t seconds = clock_seconds();
    if (seconds < EARLIEST_PLAUSIBLE)
        panic("Clock self-test: the wall clock says %d seconds since 1970\n", (int)seconds);

    int64_t day_seconds = seconds % SECONDS_PER_DAY;
    kprintf("clock: day %u since 1970, %u:%u:%u UTC\n",
            (unsigned int)(seconds / SECONDS_PER_DAY),
            (unsigned int)(day_seconds / 3600),
            (unsigned int)(day_seconds / 60 % 60),
            (unsigned int)(day_seconds % 60));
}

static void clock_test_monotonic(void)
{
    int64_t before = clock_monotonic();
    int64_t wall_before = clock_realtime();
    thread_sleep(SLEEP_TICKS);
    int64_t after = clock_monotonic();
    int64_t wall_after = clock_realtime();

    if (after - before < SLEEP_TICKS * (int64_t)TICK_AT_LEAST)
        panic("Clock self-test: slept %d ticks, the monotonic clock moved %d ns\n",
              SLEEP_TICKS, (int)(after - before));

    // Both count the same ticks from a fixed offset
    if (wall_after - wall_before != after - before)
        panic("Clock self-test: the wall clock and the monotonic clock moved apart\n");
}

void clock_selftest(void)
{
    clock_test_days();
    clock_test_realtime();
    clock_test_monotonic();
}
