#ifndef _DRIVER_CLOCK_H_
#define _DRIVER_CLOCK_H_

#include <stdint.h>

#define NANOSECONDS_PER_SECOND 1000000000LL

/**
 * @brief Read the battery-backed clock (assumed UTC) once, so the wall clock
 *        starts there and then advances with the timer. Call after timer_init.
 */
void clock_init(void);

/**
 * @brief Count the days from 1970-01-01 to a date of the proleptic Gregorian
 *        calendar (Howard Hinnant's days_from_civil).
 *
 * @param year The year, e.g. 2026 (any, negative too).
 * @param month The month, 1-12.
 * @param day The day of the month, 1-31.
 * @return Days since 1970-01-01: 0 for it, negative before it.
 */
int64_t clock_days_from_civil(int64_t year, int64_t month, int64_t day);

/**
 * @brief The monotonic clock: time since boot, which never jumps. For
 *        durations and timeouts. One tick of resolution (about 10 ms).
 *
 * @return Nanoseconds since boot.
 */
int64_t clock_monotonic(void);

/**
 * @brief The wall clock: the date read at boot plus the monotonic clock.
 *        One tick of resolution (about 10 ms).
 *
 * @return Nanoseconds since 1970-01-01 00:00:00 UTC.
 */
int64_t clock_realtime(void);

/**
 * @brief The wall clock in whole seconds: clock_realtime() / 10^9. File
 *        timestamps use it.
 *
 * @return Seconds since 1970-01-01 00:00:00 UTC.
 */
int64_t clock_seconds(void);

#endif // _DRIVER_CLOCK_H_