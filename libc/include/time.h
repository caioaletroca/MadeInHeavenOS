#ifndef _TIME_H_
#define _TIME_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <mihos/time.h>

#define CLOCK_REALTIME MIHOS_CLOCK_REALTIME
#define CLOCK_MONOTONIC MIHOS_CLOCK_MONOTONIC

struct timespec
{
    time_t tv_sec;
    long tv_nsec;
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
 * @brief Sleeps at least *request, unless a signal handler runs first.
 * @return 0, or -1 with errno set: EINTR (*remaining, if not NULL, holds the time left), EINVAL.
 */
int nanosleep(const struct timespec *request, struct timespec *remaining);

__END_DECLS

#endif /* _TIME_H_ */