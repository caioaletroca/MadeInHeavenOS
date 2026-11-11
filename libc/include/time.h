#ifndef _TIME_H_
#define _TIME_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stddef.h>
#include <mihos/time.h>

#define CLOCK_REALTIME MIHOS_CLOCK_REALTIME
#define CLOCK_MONOTONIC MIHOS_CLOCK_MONOTONIC
#define CLOCK_PROCESS_CPUTIME_ID MIHOS_CLOCK_PROCESS_CPUTIME_ID

// clock()'s unit: microseconds (POSIX's XSI requires exactly 1 000 000)
#define CLOCKS_PER_SEC 1000000L

struct timespec
{
    time_t tv_sec;
    long tv_nsec;
};

// A broken-down time: a date and a clock time, each field on its own
struct tm
{
    int tm_sec;   // 0-60 (60: a leap second; never from gmtime)
    int tm_min;   // 0-59
    int tm_hour;  // 0-23
    int tm_mday;  // day of the month, 1-31
    int tm_mon;   // months since January, 0-11
    int tm_year;  // years since 1900 (2026 is 126)
    int tm_wday;  // days since Sunday, 0-6
    int tm_yday;  // days since January 1, 0-365
    int tm_isdst; // daylight saving time in effect: always 0 (UTC)
};

__BEGIN_DECLS

/**
 * @brief Reads a clock.
 * @return 0, or -1 with errno set (EINVAL for an unknown clock, EFAULT).
 */
int clock_gettime(clockid_t clock, struct timespec *time);

/**
 * @brief The wall clock in whole seconds since 1970-01-01 UTC, also stored in *t if not NULL.
 */
time_t time(time_t *t);

/**
 * @brief The CPU time this process has used, in CLOCKS_PER_SEC units. Only differences
 * between two calls mean something (C), though here it counts from the process's fork.
 * @return The CPU time, or (clock_t)-1 if it cannot be read.
 */
clock_t clock(void);

/**
 * @brief Sleeps at least *request, unless a signal handler runs first.
 * @return 0, or -1 with errno set: EINTR (*remaining, if not NULL, holds the time left), EINVAL.
 */
int nanosleep(const struct timespec *request, struct timespec *remaining);

/**
 * @brief Seconds since 1970 to a UTC date, into *result.
 * @return result, or NULL with errno EOVERFLOW when the year does not fit an int.
 */
struct tm *gmtime_r(const time_t *timer, struct tm *result);

/**
 * @brief gmtime_r into one static struct tm, overwritten by the next call (not thread-safe).
 */
struct tm *gmtime(const time_t *timer);

/**
 * @brief As gmtime_r: MiHOS knows no time zone, so local time is UTC.
 */
struct tm *localtime_r(const time_t *timer, struct tm *result);
struct tm *localtime(const time_t *timer);

/**
 * @brief A UTC date to seconds since 1970 (glibc and BSD extension). Fields out of range
 * carry over (month 12 is January of the next year, day 0 the last of the previous month),
 * and *tm is rewritten normalized, with tm_wday and tm_yday filled in.
 * @return The seconds, or -1 with errno EOVERFLOW.
 */
time_t timegm(struct tm *tm);

/**
 * @brief As timegm: local time is UTC (tm_isdst is ignored and set to 0).
 */
time_t mktime(struct tm *tm);

/**
 * @brief Writes *tm as text into buffer, following format's % conversions (C locale, UTC).
 * @return The characters written without the '\0', or 0 if they and the '\0' do not fit
 * in size (the buffer's contents are then unspecified).
 */
size_t strftime(char *buffer, size_t size, const char *format, const struct tm *tm);

__END_DECLS

#endif /* _TIME_H_ */